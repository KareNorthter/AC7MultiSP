"""将补给、HUD 或管理候选成套安装，保留正式核心与完整备份。"""
import argparse
import hashlib
import json
import os
import re
import shutil
import struct
import subprocess
import sys
import wave
from datetime import datetime, timedelta, timezone
from pathlib import Path

# 只处理本项目及已核对的游戏安装，不改变正式载荷或启动游戏。
ROOT = Path(__file__).resolve().parents[1]
GAME = Path(r"E:\SteamLibrary\steamapps\common\ACE COMBAT 7")
MOD = GAME / "ue4ss/Mods/DualSP"
CORE = ROOT / "build-core/DualSP.dll"
FORMAL = ROOT / "dist/DualSP/bin/DualSP.dll"
VERSION = "0.1.7-resupply-candidate.1"
# 默认保留原补给安装记录；合并候选通过明确参数使用单独记录。
EVIDENCE = ROOT / "evidence/resupply-candidate-installed.json"
PWSH = Path(r"C:\Program Files\PowerShell\7\pwsh.exe")
EXPECTED_CORE = "D7D7055CA7909021EEACA15E63FF5FF253D2EB7E60A3FE3B79DCEEAB231DE339"
EXPECTED_EXE = "C7DA97F5F8A807D4F1264ADBB074146FCFFE9BDC2FFA98791B822CD28E558F4F"
FILES = ("bin/DualSP.dll", "bin/DualSPUI.dll", "Scripts/main.lua", "Scripts/json.lua",
         "Scripts/weapon_cycle.lua", "Scripts/native_hud.lua", "Scripts/loadout_flow.lua", "Scripts/native_loadout.lua")


def digest(path):
    """分块计算 SHA256，核对副本时不将游戏大文件一次读入内存。"""
    result = hashlib.sha256()
    with path.open("rb") as source:
        while block := source.read(1024 * 1024):
            result.update(block)
    return result.hexdigest().upper()


def check_selection_wave():
    """核对本候选指定的用户音频，避免安装错文件或发布时遗漏本地资源。"""
    path = ROOT / "dist/DualSP/Audio/SP2.wav"  # 不依赖用户临时提取目录进行安装。
    expected = "278B8F8815823522ED5F121711A1A12EDA2AE8E89991F8522D1FFF22832D9640"
    if digest(path) != expected:
        raise RuntimeError("SP2.wav 与本轮用户指定音频不符")
    with wave.open(str(path), "rb") as audio:
        if (audio.getnchannels(), audio.getsampwidth(), audio.getframerate(), audio.getnframes()) != (2, 2, 48000, 28982):
            raise RuntimeError("SP2.wav 的格式或时长不符")


def ensure_stopped():
    """安装前再次核对游戏进程，发现正在运行就明确停止。"""
    result = subprocess.run([str(PWSH), "-NoProfile", "-NonInteractive", "-Command",
        "@(Get-Process -Name Ace7Game -ErrorAction SilentlyContinue).Count"],
        check=True, capture_output=True, text=True, encoding="utf-8")
    if int(result.stdout.strip()) != 0:
        raise RuntimeError("请先退出皇牌空战 7；候选尚未安装")


def snapshot(directory):
    """记录目录内所有文件；拒绝链接，避免备份或移动跨出指定目录。"""
    result = {}
    if directory.is_symlink() or directory.is_junction():
        raise RuntimeError("目录是链接，停止候选安装：" + str(directory))
    resolved = directory.resolve(strict=True)
    for entry in sorted(directory.rglob("*")):
        if entry.is_symlink() or entry.is_junction() or not entry.resolve().is_relative_to(resolved):
            raise RuntimeError("目录包含外部链接，停止候选安装：" + str(entry))
        if entry.is_file():
            result[entry.relative_to(directory).as_posix()] = digest(entry)
    return result


def compile_check_scripts():
    """仅用 Lua 5.4 编译语法，不执行脚本、不模拟游戏或运行测试模块。"""
    sys.path.insert(0, str(ROOT / "tools/test-python"))
    from lupa.lua54 import LuaRuntime
    parser = LuaRuntime(unpack_returned_tuples=True)
    compile_only = parser.eval("function(s,n) local f,e=load(s,n,'t',{}); if not f then error(e) end; return true end")
    for name in FILES:
        if name.endswith(".lua"):
            path = ROOT / "dist/DualSP" / name
            compile_only(path.read_text(encoding="utf-8"), "@" + str(path))


def check_core(management):
    """读取 PE 导出核对候选接口，不加载或调用游戏内武器代码。"""
    sys.path.insert(0, str(ROOT / "tools"))
    import pefile
    with pefile.PE(str(CORE)) as library:
        exports = {entry.name.decode("ascii") for entry in library.DIRECTORY_ENTRY_EXPORT.symbols if entry.name}
        if library.FILE_HEADER.Machine != 0x8664:
            raise RuntimeError("候选核心不是 Windows x64 DLL")
    required = {"lab_native_execute", "dualsp_catalogue", "dualsp_resupply_supported"}
    if management or VERSION.startswith("0.1.7-merged-candidate."):
        required.add("dualsp_hud_readiness")
    if management:
        # 输入隔离与音效收尾是本轮必需协议，不能与候选 5 核心混装。
        required.update({"dualsp_resupply_takeoff", "dualsp_resupply_control", "dualsp_prepare_weapon_rebuild"})
    if VERSION in ("1.0.2", "1.0.1", "1.0.1-candidate.1", "1.0.0", "0.1.9", "0.1.9-performance-candidate.1") or VERSION.startswith("0.1.9-experience-candidate."):
        required.update({"dualsp_preparation_action", "dualsp_selection_audio", "dualsp_menu_confirm", "dualsp_custom_audio_update",
                         "dualsp_loadout_ammo", "dualsp_weapon_icon", "dualsp_finish_tls", "dualsp_take_empty_switch"})
    if VERSION in ("1.0.2", "1.0.1", "1.0.1-candidate.1"):
        required.add("dualsp_runtime_gate")  # 多人禁用不能只依赖 Lua 或旧核心。
    if not required.issubset(exports):
        raise RuntimeError("候选核心缺少补给协议或已有接口")


def check_management_ui():
    """只读 PE 导出，拒绝新版控制器搭配旧的 F8 选兵装界面。"""
    sys.path.insert(0, str(ROOT / "tools"))
    import pefile
    path = ROOT / "dist/DualSP/bin/DualSPUI.dll"
    with pefile.PE(str(path)) as library:
        exports = {entry.name.decode("ascii") for entry in library.DIRECTORY_ENTRY_EXPORT.symbols if entry.name}
        required = {"dualsp_management_supported", "dualsp_preparation_navigation"}
        if VERSION in ("1.0.2", "1.0.1", "1.0.1-candidate.1"):
            required.add("dualsp_runtime_gate")  # 窗口消息、F8 和手柄均需同步禁用。
        if library.FILE_HEADER.Machine != 0x8664 or not required <= exports:
            raise RuntimeError("F8 管理界面尚未构建或版本不符；请先按项目规则明确要求构建新版 UI，再统一安装")


def protected_files():
    """只记录存档和图形设置哈希，不读取或修改它们的内容。"""
    directory = Path(os.environ["LOCALAPPDATA"]) / "BANDAI NAMCO Entertainment/ACE COMBAT 7"
    if not directory.is_dir():
        raise RuntimeError("未找到既有存档目录，停止安装并等待核对")
    return {path: digest(path) for path in directory.rglob("*")
            if path.is_file() and (path.suffix.lower() == ".sav" or path.name == "GraphicsSetting.ini")}


def check_lua_forwarders():
    """离线核对专用加载器的 Lua 转发及代码区目标，不加载或调用 DLL。"""
    sys.path.insert(0, str(ROOT / "tools"))
    import pefile
    # 与候选桥接声明对应的具名导出，限定固定前缀而非扫描整个模块。
    definitions = (
        ("gettop", "?get_stack_size@Lua@LuaMadeSimple@RC@@QEBAHXZ", "488b09", 0xE9),
        ("tointegerx", "?get_integer@Lua@LuaMadeSimple@RC@@QEBA_JH@Z", "48895c24084889742410574883ec20488bf14533c0488b098bfa", 0xE8),
        ("tonumberx", "?get_number@Lua@LuaMadeSimple@RC@@QEBANH@Z", "48895c2408574883ec30488bf90f29742420488b094533c08bda", 0xE8),
        ("pushboolean", "?set_bool@Lua@LuaMadeSimple@RC@@QEBAX_N@Z", "488b090fb6d2", 0xE9),
        ("pushlstring", "?set_string@Lua@LuaMadeSimple@RC@@QEBAXPEBD_K@Z", "488b09", 0xE9),
    )
    result = {}
    with pefile.PE(str(GAME / "ue4ss/UE4SS.dll")) as library:
        exports = {entry.name.decode("ascii"): entry.address for entry in library.DIRECTORY_ENTRY_EXPORT.symbols if entry.name}
        for api, name, encoded, opcode in definitions:
            if name not in exports:
                raise RuntimeError("专用加载器缺少 Lua 转发：" + api)
            prefix = bytes.fromhex(encoded)
            address = exports[name]
            code = library.get_data(address, len(prefix) + 5)
            if code[:len(prefix)] != prefix or code[len(prefix)] != opcode:
                raise RuntimeError("专用加载器 Lua 转发不匹配：" + api)
            target = address + len(prefix) + 5 + struct.unpack_from("<i", code, len(prefix) + 1)[0]
            section = library.get_section_by_rva(target)
            if section is None or not section.Characteristics & 0x20000000:
                raise RuntimeError("Lua 转发目标不是模块代码：" + api)
            result[api] = {"export": name, "wrapper_rva": hex(address), "target_rva": hex(target), "prefix": encoded}
    return result


def check_declared_ui_names():
    """离线检查声明的 UI API 名称；存在性不等同于运行时反射绑定通过。"""
    # 防止再次安装明显引用其他引擎版本 API 的 Lua；实际路径仍由游戏内预检验证。
    image = (GAME / "Ace7Game.exe").read_bytes()
    scripts = [ROOT / "dist/DualSP" / name for name in FILES if name.endswith(".lua")]
    names = sorted({name for path in scripts
                    for name in re.findall(r"/Script/[A-Za-z0-9_.]+:([A-Za-z0-9_]+)", path.read_text(encoding="utf-8"))})
    missing = [name for name in names if (name + "\0").encode("ascii") not in image]
    if missing:
        raise RuntimeError("当前游戏中找不到声明的 UI API 名称：" + ", ".join(missing))
    return names


def main():
    """校验、暂存、备份、安装和回读；不调用正式安装载荷的同步入口。"""
    global VERSION, FILES, EVIDENCE, CORE
    # 明确选择合并方案，不能把新 Lua 与不含 HUD 接口的旧核心混装。
    arguments = argparse.ArgumentParser(description=__doc__)
    arguments.add_argument("--merged-hud", action="store_true", help="成套安装补给重选与两个 HUD 新功能")
    arguments.add_argument("--management", action="store_true", help="成套安装原生兵装入口、直接选择和中英文 F8 管理界面")
    arguments.add_argument("--experience", action="store_true", help="按 VERSION 安装独立版本核心及配套 Lua，不覆盖旧版核心")
    options = arguments.parse_args()
    # 管理源码只能通过管理模式安装；旧候选的记录绝不覆盖。
    controller = (ROOT / "dist/DualSP/Scripts/main.lua").read_text(encoding="utf-8")  # 版本协议来自即将安装的调度器。
    if 'loadExport("dualsp_fixed_mount_update")' in controller:
        raise RuntimeError("当前是固定挂载研究 Lua，必须使用专用 install_su30sm_candidate.py；不能搭配正式核心")
    management_source = "dualsp_management_supported" in controller
    experience_source = "dualsp_preparation_action" in controller and "dualsp_selection_audio" in controller
    if options.experience:
        if not management_source or not experience_source:
            raise RuntimeError("当前源码不是体验候选，停止安装")
        options.management = True
        options.merged_hud = True
        VERSION = (ROOT / "VERSION").read_text(encoding="utf-8").strip()
        if VERSION not in ("1.0.2", "1.0.1", "1.0.1-candidate.1", "1.0.0", "0.1.9", "0.1.9-performance-candidate.1") or f'DUALSP_LOADED {VERSION} MANAGEMENT_UI' not in controller:
            raise RuntimeError("版本文件与已核对的控制器不匹配")
        # 版本快照从 Git 保留核心安装，不依赖可能被后续研究覆盖的构建目录。
        core_version = VERSION if VERSION in ("1.0.1", "1.0.1-candidate.1") else "0.1.9"  # 正式快照与候选分别保留。
        if VERSION == "1.0.2":
            core_version = "1.0.1"  # 兼容修订只改 Lua/UI，明确复用已实测核心，不另复制同一 DLL。
        CORE = ROOT / "dist/DualSP/cores" / core_version / "DualSP.dll"
        core_hash = ("EA3C9C17456EAE89289621E40225F7D4CC2F192AE051E97725FEBFDDB9BA27AF"
                     if VERSION in ("1.0.2", "1.0.1", "1.0.1-candidate.1") else "46731AE6AD519E0BD145E0EA03C32A2662947C8B03B56C524D7CDF46CA8096E1")
        if digest(CORE) != core_hash:
            raise RuntimeError("所选核心与版本快照不一致")
        FILES += ("Scripts/weapon_names.lua", "Scripts/weapon_presentation.lua", "Audio/SP2.wav")
        if VERSION in ("1.0.2", "1.0.1", "1.0.1-candidate.1"):
            FILES += ("Scripts/runtime_context.lua",)
        EVIDENCE = ROOT / f"evidence/{VERSION}-installed.json"
        # 用户提供的提取音频只在本机保存；缺失或不符时停止，不改用旧音色。
        check_selection_wave()
        check_management_ui()
    elif options.management:
        if experience_source:
            raise RuntimeError("当前 Lua 需要体验候选核心；请明确使用 --experience，不得与 0.1.8 核心混装")
        if not management_source:
            raise RuntimeError("当前源码不是管理版，停止安装")
        options.merged_hud = True
        VERSION = (ROOT / "VERSION").read_text(encoding="utf-8").strip()
        # 正式核心原值受保护；当前实玩版本使用纳入 Git 的独立核心，不依赖临时构建目录。
        if VERSION != "0.1.8":
            raise RuntimeError("当前管理安装器仅支持已整理的 0.1.8；请先核对新版本核心")
        CORE = ROOT / "dist/DualSP/cores/0.1.8/DualSP.dll"
        if digest(CORE) != "3FFF19496467F1616DD2DB7F5434D2008D84C4E1375C16C51D7F49AE93262A49":
            raise RuntimeError("0.1.8 核心与用户初步验证的核心不一致")
        FILES += ("Scripts/weapon_names.lua", "Scripts/weapon_presentation.lua")
        EVIDENCE = ROOT / "evidence/0.1.8-installed.json"
        check_management_ui()
    elif management_source:
        raise RuntimeError("当前为管理版源码，请先构建新版 UI，并使用 --management 成套安装；不与旧 F8 混装")
    elif options.merged_hud:
        VERSION = "0.1.7-merged-candidate.3"
        FILES += ("Scripts/weapon_names.lua", "Scripts/weapon_presentation.lua")
        EVIDENCE = ROOT / "evidence/merged-candidate.3-installed.json"
    elif "dualsp_hud_readiness" in (ROOT / "dist/DualSP/Scripts/main.lua").read_text(encoding="utf-8"):
        raise RuntimeError("当前源码已包含 HUD 合并功能，请使用 --merged-hud 成套安装")
    ensure_stopped()
    if digest(FORMAL) != EXPECTED_CORE or digest(GAME / "Ace7Game.exe") != EXPECTED_EXE:
        raise RuntimeError("正式核心或游戏版本不匹配，停止安装")
    if not MOD.resolve(strict=True).is_relative_to(GAME.resolve(strict=True)):
        raise RuntimeError("目标模组目录超出游戏安装范围")
    check_core(options.management)
    compile_check_scripts()
    forwarders = check_lua_forwarders() if options.merged_hud else None
    declared_names = check_declared_ui_names() if options.merged_hud else None

    # 已有加载器只核对，不复制或替换；正式恢复载荷必须仍是原 0.1.7。
    baseline = json.loads((ROOT / "Release/baseline-0.1.7.json").read_text(encoding="utf-8"))
    for pair in ((GAME / "dwmapi.dll", ROOT / "payload/dwmapi.dll"),
                 (GAME / "ue4ss/UE4SS.dll", ROOT / "payload/ue4ss/UE4SS.dll")):
        if digest(pair[0]) != digest(pair[1]):
            raise RuntimeError("现有加载器与项目专用加载器不匹配")
    for entry in baseline["runtime_files"]:
        relative = entry["path"].removeprefix("dist/DualSP/")
        if digest(ROOT / "payload/ue4ss/Mods/DualSP" / relative) != entry["sha256"]:
            raise RuntimeError("正式恢复载荷发生变化：" + relative)

    # 安装核验保护这些既有文件；详细旧模组清单只放入被 Git 排除的备份。
    before = snapshot(MOD)
    saves = protected_files()
    profiles = MOD / "profiles.json"
    profile_hash = digest(profiles)
    mod_list = GAME / "ue4ss/Mods/mods.txt"
    list_hash = digest(mod_list)
    if not re.search(r"^\s*DualSP\s*:\s*1\s*$", mod_list.read_text(encoding="utf-8-sig"), re.MULTILINE):
        raise RuntimeError("已有 DualSP 未启用，请先核对模组列表")
    receipt = json.loads((MOD / "install-receipt.json").read_text(encoding="utf-8-sig"))

    # 使用上海时间标识候选批次；所有暂存和备份位置都在同一 E 盘。
    now = datetime.now(timezone(timedelta(hours=8)))
    batch = now.strftime("%Y%m%d-%H%M%S-%f")
    backup = ROOT / "backup" / ("merged-candidate-" if options.merged_hud else "resupply-candidate-") / batch
    staging = ROOT / "Temp" / ("candidate-install-" + batch) / "DualSP"
    # 对外名称改变，内部目录不变；版本文件随运行文件一起核验。
    FILES += ("VERSION",)
    sources = {name: ROOT / "VERSION" if name == "VERSION" else
               CORE if name == "bin/DualSP.dll" else ROOT / "dist/DualSP" / name for name in FILES}
    expected = {name: digest(path) for name, path in sources.items()}
    shutil.copytree(MOD, staging)
    for name, path in sources.items():
        (staging / name).parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(path, staging / name)
    receipt.update(Name="AC7DualSP", Version=VERSION, InstalledAt=now.isoformat(), Candidate="candidate" in VERSION,
                   CandidateCoreSHA256=expected["bin/DualSP.dll"], BackupPath=str(backup), GameStartedByAgent=False)
    (staging / "install-receipt.json").write_text(json.dumps(receipt, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    if any(digest(staging / name) != value for name, value in expected.items()):
        raise RuntimeError("候选暂存文件核验失败，现有安装未改")
    if digest(staging / "profiles.json") != profile_hash:
        raise RuntimeError("候选暂存时配置变化，现有安装未改")

    # 只移动已校验的整套目录，不永久删除旧文件，也不留下新旧核心混装。
    ensure_stopped()
    if any(digest(path) != expected[name] for name, path in sources.items()):
        raise RuntimeError("其他聊天修改了候选源码或核心，停止安装并等待合并")
    if snapshot(MOD) != before or protected_files() != saves or digest(mod_list) != list_hash:
        raise RuntimeError("准备安装期间游戏文件变化，现有安装未改")
    if not backup.resolve().is_relative_to((ROOT / "backup").resolve()) or backup.exists():
        raise RuntimeError("备份目标无效")
    if not staging.resolve(strict=True).is_relative_to((ROOT / "Temp").resolve()):
        raise RuntimeError("暂存目标无效")
    backup.mkdir(parents=True)
    shutil.copy2(mod_list, backup / "mods.txt")
    (backup / "manifest.json").write_text(json.dumps(before, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    MOD.rename(backup / "DualSP")
    staging.rename(MOD)

    # 安装后逐文件回读，并检查备份、配置、存档、游戏和正式核心均保持一致。
    checks = {name: digest(MOD / name) for name in FILES}
    if checks != expected or snapshot(backup / "DualSP") != before:
        raise RuntimeError("候选安装或旧模组备份核验失败；备份位于 " + str(backup))
    if digest(MOD / "profiles.json") != profile_hash or protected_files() != saves:
        raise RuntimeError("安装后配置或存档核验失败；备份位于 " + str(backup))
    if digest(mod_list) != list_hash or digest(FORMAL) != EXPECTED_CORE or digest(GAME / "Ace7Game.exe") != EXPECTED_EXE:
        raise RuntimeError("安装后保护文件核验失败；备份位于 " + str(backup))
    result = {"version": VERSION, "installed_at": now.isoformat(), "game_path": str(GAME), "backup": str(backup),
        "files": checks, "candidate_core_exports_verified": True, "lua54_syntax_compiled_without_execution": True,
        "old_mod_backup_verified": True, "profiles_preserved": True, "saves_and_graphics_preserved": True,
        "save_and_graphics_file_count": len(saves), "formal_core_preserved": True, "game_exe_preserved": True,
        "mod_list_preserved": True, "baseline_restore_payload_verified": True,
        "test_modules_run": False, "game_started_by_agent": False, "gameplay_verified": False}
    result["features"] = ["原生中途补给双 SP 重选"]
    if options.merged_hud:
        result["features"] += ["右下角名称按就绪/耗尽变淡"]
        # 体验候选的中央透明度跟随就绪状态，旧候选仍为固定亮度。
        result["features"] += ["中央简称按就绪状态常驻" if options.experience else "中央当前武器简称正常显示常驻"]
        result["hud_lua_forwarders"] = forwarders
        result["periodic_hud_file_transactions"] = False
        result["declared_ui_api_names_in_game_image"] = declared_names
        result["runtime_ui_binding_verified"] = False
    if options.management:
        result["features"] += ["键盘与固定手柄直接选择武器", "F8 纯管理界面", "中英文界面", "中央常驻提示独立开关", "按需显示日志"]
        result["features"] += ["补给双栏性能/类型/弹药", "起飞 YES/NO 与返回重选"]
        result["features"] += ["准备双栏性能/类型及右下弹药汇总", "原生页面重新激活接入"]
        # 体验候选取消额外确认页，回执不再宣称保留旧版两次交接流程。
        if options.experience:
            if VERSION in ("1.0.2", "1.0.1", "1.0.1-candidate.1", "1.0.0", "0.1.9-performance-candidate.1"):
                result["features"] += ["中央简称字号放大约 11% 与独立绿色样式", "菜单对象发现缓存与激活对象直连",
                                       "静态性能栏与弹药颜色按变化更新", "补给接管不等待原生控制开放", "15 秒调度耗时汇总"]
            if VERSION in ("1.0.2", "1.0.1", "1.0.1-candidate.1"):
                result["features"] += ["多人网络/大厅/菜单禁用与核心/UI 双重许可", "原生操作提示跟随游戏文本语言",
                                       "F8 支持跟随游戏及保留手动语言", "英文说明换行保留单词间空格", "补给起飞 HUD 预建与同回调提交"]
                result["multiplayer_gameplay_verified"] = False
                result["english_gameplay_verified"] = False
                result["post_resupply_hud_verified"] = False
                result["management_protocol"] = 2
                result["management_settings_revision"] = 2
                result["profile_migration_on_first_start"] = "语言设为 auto，移除退役开关，全局比例规范为 50% 至 100% 的 5% 档；机体方案保留"
                result["features"] += ["Game Language 默认与固定双语标签", "取消未购买兵装试玩", "右下 HUD 与直接选择固定启用", "配弹 50% 至 100% 共 11 档"]
            result["features"] += ["机库一步确认和返回", "STANDBY 双 SP 信息", "SP1 预览第二栏 EMPTY",
                                   "开场 HUD 提前接入", "补给打开和激活时即时接入", "SP2 用户 WAV 与音量暂停联动",
                                   "原生兵装模型特写跟随光标", "STANDBY 退出动画保留双栏",
                                   "准备页原生金色选中素材", "准备页与 STANDBY 英文兵装信息",
                                   "动态武器槽池范围校验", "机库与补给内部步骤原生确认音", "未就绪名称亮度系数 35%",
                                   "准备页原生彩色分类图标", "准备页弹药仅右下汇总", "补给英文标签与原生绿色泛光",
                                   "补给完整原生武器介绍", "四页预览计入部件且保留配弹比例",
                                   "SP2 音量乘以 0.175", "准备图标与三行全名留白", "补给介绍对齐和重排",
                                   "PART 双 SP 信息", "MSL 保留上一 SP 挂载", "切换离开 TLS 时原生收尾"]
        else:
            result["features"] += ["确认页修改 SP1/SP2", "取消草稿和原生继续分别交接"]
        result["features"] += ["原生补给 YES 完整关闭链", "飞机完整切换同步锁定参数与部件效果"]
        result["features"] += ["补给页原生输入独占", "兵装重建前逐槽释放锁定音引用"]
        result["features"] += ["补给阶段实例与任务世界分别校验"]
        result["management_ui_export_verified"] = True
        result["preparation_navigation_export_verified"] = True
        result["f9_cycle_removed"] = True
        result["f8_loadout_editor_removed"] = True
    EVIDENCE.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(result, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
