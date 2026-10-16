"""核对并整理三 SP 增量候选目录；不安装、不启动游戏、不运行测试。"""
import importlib.util
import json
import shutil
import subprocess
from datetime import datetime, timedelta, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]  # 本仓库是唯一载荷来源。
VERSION = "1.0.2-three-sp-candidate.1"  # 研究版不能冒充 1.0.2 正式包。
OUTPUT = ROOT / "Release" / ("AC7DualSP-" + VERSION)  # 本地增量目录不入 Git。
CORE = ROOT / "dist/DualSP/cores" / VERSION / "DualSP.dll"  # 独立核心快照。


def main():
    """复用受控安装器的离线核验，只输出可复制目录和精简回执。"""
    spec = importlib.util.spec_from_file_location("candidate_install", ROOT / "src/install-resupply-candidate.py")
    installer = importlib.util.module_from_spec(spec)  # 导入不会调用安装入口。
    spec.loader.exec_module(installer)
    if (ROOT / "VERSION").read_text(encoding="utf-8").strip() != VERSION:
        raise RuntimeError("仓库版本与三 SP 候选不一致")
    if OUTPUT.exists():
        raise RuntimeError("候选目录已存在，请先归档后重新准备：" + str(OUTPUT))
    source_core = ROOT / "build-core/DualSP.dll"  # 构建需由操作者显式执行。
    if CORE.exists() and installer.digest(CORE) != installer.digest(source_core):
        raise RuntimeError("已有核心快照不同，请归档旧候选后明确更新版本")
    CORE.parent.mkdir(parents=True, exist_ok=True)
    if not CORE.exists():
        shutil.copy2(source_core, CORE)
    installer.VERSION = VERSION
    installer.CORE = CORE
    installer.FILES = installer.FILES + ("Scripts/loadout_profile.lua", "Scripts/weapon_names.lua",
        "Scripts/weapon_presentation.lua", "Scripts/runtime_context.lua", "Audio/SP2.wav")
    installer.check_core(True)
    installer.check_management_ui()
    installer.compile_check_scripts()  # 只编译 Lua 语法，不执行任何运行时模块。
    installer.check_selection_wave()
    forwarders = installer.check_lua_forwarders()  # 只读当前加载器，不修改游戏。
    ui_names = installer.check_declared_ui_names()
    if installer.digest(installer.FORMAL) != installer.EXPECTED_CORE:
        raise RuntimeError("受保护旧核心发生变化")
    if installer.digest(installer.GAME / "Ace7Game.exe") != installer.EXPECTED_EXE:
        raise RuntimeError("游戏 build 不匹配")

    sources = {name: CORE if name == "bin/DualSP.dll" else ROOT / "dist/DualSP" / name
               for name in installer.FILES}  # 显式白名单，排除配置、日志和存档。
    sources["VERSION"] = ROOT / "VERSION"
    hashes = {name: installer.digest(path) for name, path in sources.items()}  # 安装器也使用此清单拒绝混装。
    destination = OUTPUT / "GameFiles/ue4ss/Mods/DualSP"
    for name, source in sources.items():
        target = destination / name  # 只复制白名单内的相对路径。
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, target)
    shutil.copy2(ROOT / "docs/research/three-sp-candidate.txt", OUTPUT / "安装与复测说明.txt")
    for name, expected in hashes.items():
        if installer.digest(destination / name) != expected:
            raise RuntimeError("候选副本核验失败：" + name)

    branch = subprocess.run(["git", "branch", "--show-current"], cwd=ROOT, check=True,
                            capture_output=True, text=True, encoding="utf-8").stdout.strip()
    receipt = {"version": VERSION, "baseline_commit": "5628a62", "branch": branch,
        "prepared_at": datetime.now(timezone(timedelta(hours=8))).isoformat(),
        "output": str(OUTPUT), "runtime_files": hashes, "files_verified": True,
        "core_and_ui_exports_verified": True, "lua_syntax_compiled_without_execution": True,
        "loader_forwarders": forwarders, "declared_ui_api_names": ui_names,
        "configuration_schema": 2, "management_protocol": 3, "native_command_protocol": 3,
        "ammo_rule": "部件后按实际槽数 1/2/3 整数均分；正载量至少一发，零载量仍为零",
        "test_modules_run": False, "installed": False, "game_started_by_agent": False,
        "gameplay_verified": False, "ui_layout_verified_in_game": False,
        "protected_old_core_sha256": installer.digest(installer.FORMAL)}
    (ROOT / "evidence/three-sp-candidate-build.json").write_text(
        json.dumps(receipt, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    (OUTPUT / "文件清单.json").write_text(json.dumps(hashes, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"version": VERSION, "output": str(OUTPUT), "file_count": len(sources),
                      "installed": False, "gameplay_verified": False}, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
