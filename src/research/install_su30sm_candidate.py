"""在已核对的正式 1.0.2 上安装固定挂载研究候选；默认仅预检，不启动游戏。"""

import argparse
from datetime import datetime, timedelta, timezone
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess


def digest(path):
    """只计算文件摘要，个人状态内容不写入公共证据。"""
    return hashlib.sha256(path.read_bytes()).hexdigest()


def inventory(directory):
    """拒绝链接，保存相对路径清单以核对整套备份。"""
    root = directory.resolve(strict=True)
    result = {}  # 旧模组的运行文件、个人配置和日志都需要保留。
    for path in directory.rglob("*"):
        if path.is_symlink() or path.is_junction() or not path.resolve().is_relative_to(root):
            raise ValueError("目录含外部链接：" + str(path))
        if path.is_file():
            result[path.relative_to(directory).as_posix()] = digest(path)
    return result


def require_stopped():
    """发现游戏仍运行就停止安装，由明确的外部操作完成退出。"""
    result = subprocess.run([r"C:\Program Files\PowerShell\7\pwsh.exe", "-NoProfile", "-NonInteractive", "-Command",
                             "@(Get-Process -Name Ace7Game -ErrorAction SilentlyContinue).Count"],
                            check=True, capture_output=True, text=True, encoding="utf-8")
    if int(result.stdout.strip()):
        raise RuntimeError("游戏仍在运行，尚未安装候选")


def main():
    """只替换候选核心、main.lua 和安装标识，其余正式文件必须逐字节保留。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--apply", action="store_true")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]  # 当前独立研究工作区。
    build = args.build_dir.resolve(strict=True)
    if not build.is_relative_to((root / "Temp").resolve()):
        raise ValueError("候选必须来自本工作区 Temp")
    receipt = json.loads((build / "build-receipt.json").read_text(encoding="utf-8"))
    version = receipt["candidate"]  # 与 Lua 研究日志中的标识一起核对。
    native = json.loads((root / receipt["native_research_evidence"]).read_text(encoding="utf-8"))
    if receipt["installation_blockers"] or not native["native_checks_complete"] or not native["ready_for_first_controlled_install"]:
        raise ValueError("原生接入研究尚未完成")
    candidate = build / "DualSP.dll"
    if digest(candidate) != receipt["candidate_sha256"]:
        raise ValueError("候选核心与构建回执不符")
    for name, expected in (receipt["lua_sources"] | receipt["cpp_sources"] | receipt["protected_files"]).items():
        if digest(root / name) != expected:
            raise ValueError("构建后来源或保护文件改变：" + name)
    if f'FIXED_MOUNT_CANDIDATE {version}' not in (root / "dist/DualSP/Scripts/main.lua").read_text(encoding="utf-8"):
        raise ValueError("研究版本标识不匹配")
    game = Path(r"E:\SteamLibrary\steamapps\common\ACE COMBAT 7")
    mod = game / "ue4ss/Mods/DualSP"
    if mod.is_symlink() or mod.is_junction() or not mod.resolve(strict=True).is_relative_to(game.resolve(strict=True)):
        raise ValueError("目标模组路径无效")
    formal = json.loads((root / "evidence/1.0.2-installed.json").read_text(encoding="utf-8"))["files"]
    for name, expected in formal.items():
        if digest(mod / name) != expected.lower():
            raise ValueError("当前安装不再是已核对的正式 1.0.2，停止以免覆盖其他工作：" + name)
    plan = {"candidate": version, "core_sha256": receipt["candidate_sha256"],
            "replacement_files": ["bin/DualSP.dll", "Scripts/main.lua", "VERSION", "install-receipt.json"],
            "base": "1.0.2", "game_started_by_agent": False}
    if not args.apply:
        print(json.dumps({**plan, "status": "预检通过，未修改游戏文件"}, ensure_ascii=False))
        return
    require_stopped()
    before = inventory(mod)  # 游戏关闭后日志稳定，才能核对完整备份。
    saves = Path(os.environ["LOCALAPPDATA"]) / "BANDAI NAMCO Entertainment/ACE COMBAT 7"
    if not saves.is_dir():
        raise ValueError("存档目录不存在，停止安装")
    protected_paths = [game / "Ace7Game.exe", game / "dwmapi.dll", game / "ue4ss/UE4SS.dll", game / "ue4ss/Mods/mods.txt"]
    protected_paths += [path for path in saves.rglob("*") if path.is_file() and (path.suffix.lower() == ".sav" or path.name == "GraphicsSetting.ini")]
    protected = {path: digest(path) for path in protected_paths}  # 不读取或改写个人状态内容。
    now = datetime.now(timezone(timedelta(hours=8)))
    batch = now.strftime("%Y%m%d-%H%M%S-%f")
    backup = root / "backup" / ("su30sm-mount-" + batch)
    staging = root / "Temp" / ("su30sm-install-" + batch) / "DualSP"
    if not backup.resolve().is_relative_to((root / "backup").resolve()) or backup.exists():
        raise ValueError("备份路径无效")
    if not staging.resolve().is_relative_to((root / "Temp").resolve()) or staging.exists():
        raise ValueError("暂存路径无效")
    shutil.copytree(mod, staging)
    shutil.copy2(candidate, staging / "bin/DualSP.dll")
    shutil.copy2(root / "dist/DualSP/Scripts/main.lua", staging / "Scripts/main.lua")
    (staging / "VERSION").write_text(version + "\n", encoding="utf-8")
    install = json.loads((staging / "install-receipt.json").read_text(encoding="utf-8-sig"))
    install.update(Name="AC7DualSP", Version=version, Candidate=True, InstalledAt=now.isoformat(),
                   CandidateCoreSHA256=receipt["candidate_sha256"], BackupPath=str(backup), GameStartedByAgent=False)
    (staging / "install-receipt.json").write_text(json.dumps(install, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    expected = inventory(staging)  # 只允许计划中的四个文件不同。
    if set(expected) != set(before) or any(expected[name] != value for name, value in before.items() if name not in plan["replacement_files"]):
        raise ValueError("暂存包含计划外变更，现有安装未改")
    require_stopped()
    if inventory(mod) != before or any(digest(path) != value for path, value in protected.items()):
        raise ValueError("安装前现场文件改变，现有安装未改")
    backup.mkdir(parents=True)
    (backup / "manifest.json").write_text(json.dumps(before, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    mod.rename(backup / "DualSP")  # 旧整套移入已验证的备份，不永久删除文件。
    staging.rename(mod)
    if inventory(mod) != expected or inventory(backup / "DualSP") != before or any(digest(path) != value for path, value in protected.items()):
        raise RuntimeError("安装后核验失败，完整备份位于：" + str(backup))
    result = {**plan, "installed_at": now.isoformat(), "backup": str(backup), "installed": True,
              "changed_files": {name: expected[name] for name in plan["replacement_files"]},
              "other_mod_files_preserved": True, "protected_files_verified": len(protected),
              "profiles_preserved": True, "full_backup_verified": True, "gameplay_verified": False,
              "test_modules_run": False, "main_worktree_modified": False}
    with (root / "evidence/su30sm-fixed-mount-candidate.2-installed.json").open("x", encoding="utf-8") as stream:
        json.dump(result, stream, ensure_ascii=False, indent=2);stream.write("\n")
    print(json.dumps(result, ensure_ascii=False))


if __name__ == "__main__":
    main()
