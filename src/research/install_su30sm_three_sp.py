"""整套安装三 SP 固定挂载候选；限定已登记的相邻升级，默认仅预检。"""
import argparse
from datetime import datetime, timedelta, timezone
import importlib.util
import json
import os
from pathlib import Path
import shutil


def main():
    """限定来源、验证完整暂存，整目录备份后交换；不启动游戏。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-dir', type=Path, required=True)
    parser.add_argument('--apply', action='store_true')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]  # 独立研究工作区。
    spec = importlib.util.spec_from_file_location('mount_install', Path(__file__).with_name('install_su30sm_candidate.py'))
    helper = importlib.util.module_from_spec(spec)  # 复用只读摘要、目录和进程核对，不调用旧安装入口。
    spec.loader.exec_module(helper)
    digest, inventory = helper.digest, helper.inventory
    build = args.build_dir.resolve(strict=True)  # 只能使用当前工作区刚构建的来源。
    if not build.is_relative_to((root / 'Temp').resolve()):
        raise ValueError('候选必须来自当前工作区 Temp')
    receipt = json.loads((build / 'build-receipt.json').read_text(encoding='utf-8'))
    version = (root / 'VERSION').read_text(encoding='utf-8').strip()  # 只接受已列明的候选升级。
    # 每个修订绑定唯一的前版回执，不能跨过尚未核对的安装状态。
    previous_receipts = {
        '1.0.2-su30sm-three-sp.2': 'su30sm-three-sp-installed.json',
        '1.0.2-su30sm-three-sp.3': 'su30sm-three-sp-candidate.2-installed.json',
        '1.0.2-su30sm-three-sp.4': 'su30sm-three-sp-candidate.3-installed.json',
        '1.0.2-su30sm-three-sp.5': 'su30sm-three-sp-candidate.4-installed.json',
    }
    revision = version in previous_receipts  # 修订保留已迁移的 schema 2。
    if version != '1.0.2-su30sm-three-sp.1' and not revision:
        raise ValueError('安装器未定义此候选的升级来源')
    if receipt['candidate'] != version or (root / 'VERSION').read_text(encoding='utf-8').strip() != version:
        raise ValueError('候选版本不匹配')
    for name, expected in (receipt['lua_sources'] | receipt['cpp_sources'] | receipt['protected_files']).items():
        if digest(root / name) != expected:
            raise ValueError('构建后来源改变：' + name)
    if digest(build / 'DualSP.dll') != receipt['candidate_sha256'] or digest(build / 'DualSPUI.dll') != receipt['ui_sha256']:
        raise ValueError('构建产物哈希不符')
    if not {'dualsp_fixed_mount_update', 'dualsp_three_sp_supported'} <= set(receipt['required_exports_verified']):
        raise ValueError('构建回执缺少固定挂载及三 SP 核心协议')
    if 'dualsp_three_sp_supported' not in receipt['required_ui_exports_verified']:
        raise ValueError('构建回执缺少三 SP 管理界面协议')
    game = Path('E:/SteamLibrary/steamapps/common/ACE COMBAT 7')
    mod = game / 'ue4ss/Mods/DualSP'  # 不操作其他模组和发布包。
    if mod.is_symlink() or mod.is_junction() or not mod.resolve(strict=True).is_relative_to(game.resolve(strict=True)):
        raise ValueError('安装目标路径无效')
    formal = json.loads((root / 'evidence/1.0.2-installed.json').read_text(encoding='utf-8'))['files']
    previous = json.loads((root / 'evidence/su30sm-fixed-mount-candidate.2-installed.json').read_text(encoding='utf-8'))
    expected_old = formal | previous['changed_files']  # 任一旧运行文件被其他任务改变即停止。
    if revision:
        previous = json.loads((root / 'evidence' / previous_receipts[version]).read_text(encoding='utf-8'))
        expected_old = previous['runtime_files']
        if 'dualsp_refresh_weapon_icons' not in receipt['required_exports_verified']:
            raise ValueError('修订核心缺少装填剪影刷新接口')
    for name, expected in expected_old.items():
        if digest(mod / name) != expected.lower():
            raise ValueError('当前安装已改变，停止覆盖：' + name)
    settings = json.loads((mod / 'profiles.json').read_text(encoding='utf-8-sig'))
    if revision:
        if settings.get('schema') != 2 or settings.get('management_revision') != 3:
            raise ValueError('修订必须基于已迁移的三 SP 配置')
    else:
        if settings.get('schema') != 1 or 52 in settings.get('keyboard_keys', []):
            raise ValueError('旧配置不能直接迁移，需要人工核对 schema 或 4 键占用')
        if (mod / 'profiles.before-three-sp.json').exists():
            raise ValueError('发现已有三 SP 迁移备份，需要先核对')
    sources = {name.removeprefix('dist/DualSP/'): root / name for name in receipt['lua_sources']}
    sources.update({'bin/DualSP.dll': build / 'DualSP.dll', 'bin/DualSPUI.dll': build / 'DualSPUI.dll', 'VERSION': root / 'VERSION'})
    hashes = {name: digest(path) for name, path in sources.items()}  # 安装白名单不含个人配置。
    plan = {'candidate': version, 'base': previous['candidate'], 'runtime_files': hashes,
            'configuration_migration': '保留 schema 2，不再迁移' if revision else '下次启动由 Lua 备份 schema 1 并迁移到 schema 2，旧 SP3 为空',
            'game_started_by_agent': False, 'test_modules_run': False, 'gameplay_verified': False}
    if not args.apply:
        print(json.dumps({**plan, 'status': '预检通过，未安装'}, ensure_ascii=False))
        return
    helper.require_stopped()
    before = inventory(mod)  # 进程已关闭，完整日志和配置可以稳定备份。
    saves = Path(os.environ['LOCALAPPDATA']) / 'BANDAI NAMCO Entertainment/ACE COMBAT 7'
    if not saves.is_dir():
        raise ValueError('存档目录不存在')
    protected_paths = [game / name for name in ('Ace7Game.exe', 'dwmapi.dll', 'ue4ss/UE4SS.dll', 'ue4ss/Mods/mods.txt')]
    protected_paths += [path for path in saves.rglob('*') if path.is_file() and (path.suffix.lower() == '.sav' or path.name == 'GraphicsSetting.ini')]
    protected = {path: digest(path) for path in protected_paths}  # 仅校验摘要，公共证据不保存个人内容。
    now = datetime.now(timezone(timedelta(hours=8)))
    batch = now.strftime('%Y%m%d-%H%M%S-%f')
    backup = root / 'backup' / ('su30sm-three-sp-' + batch)
    staging = root / 'Temp' / ('su30sm-three-sp-install-' + batch) / 'DualSP'
    if backup.exists() or staging.exists():
        raise ValueError('备份或暂存目录已存在')
    if not backup.resolve().is_relative_to((root / 'backup').resolve()) or not staging.resolve().is_relative_to((root / 'Temp').resolve()):
        raise ValueError('备份或暂存目录越界')
    shutil.copytree(mod, staging)
    for name, source in sources.items():
        target = staging / name  # 仅复制已核验运行文件，保留已有音频。
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, target)
    install = json.loads((staging / 'install-receipt.json').read_text(encoding='utf-8-sig'))
    install.update(Name='AC7DualSP', Version=version, Candidate=True, InstalledAt=now.isoformat(),
                   CandidateCoreSHA256=receipt['candidate_sha256'], BackupPath=str(backup), GameStartedByAgent=False)
    (staging / 'install-receipt.json').write_text(json.dumps(install, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    expected = inventory(staging)
    allowed = set(sources) | {'install-receipt.json'}  # 只允许新增 loadout_profile.lua 和替换白名单。
    if set(expected) != set(before) | set(sources) or any(expected[name] != value for name, value in before.items() if name not in allowed):
        raise ValueError('暂存包含计划外变化，现有安装未改')
    if any(expected[name] != value for name, value in hashes.items()):
        raise ValueError('暂存运行文件核验失败')
    helper.require_stopped()
    if inventory(mod) != before or any(digest(path) != value for path, value in protected.items()):
        raise ValueError('安装前文件发生并发变化，现有安装未改')
    backup.mkdir(parents=True)
    shutil.copytree(saves, backup / 'SaveData')
    for path, value in protected.items():
        if path.is_relative_to(saves) and digest(backup / 'SaveData' / path.relative_to(saves)) != value:
            raise ValueError('存档备份核验失败，现有安装未改')
    (backup / 'manifest.json').write_text(json.dumps(before, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    mod.rename(backup / 'DualSP')  # 两个绝对目标均已检查，无递归删除和跨壳路径拼接。
    staging.rename(mod)
    if inventory(mod) != expected or inventory(backup / 'DualSP') != before or any(digest(path) != value for path, value in protected.items()):
        raise RuntimeError('安装后校验失败，完整备份位于：' + str(backup))
    result = {**plan, 'installed': True, 'installed_at': now.isoformat(), 'backup': str(backup),
              'full_backup_verified': True, 'profiles_preserved': True, 'save_backup_verified': True,
              'other_mod_files_preserved': True, 'protected_files_verified': len(protected), 'main_worktree_modified': False}
    receipt_name = 'su30sm-three-sp-candidate.' + version.rsplit('.', 1)[1] + '-installed.json' if revision else 'su30sm-three-sp-installed.json'  # 不覆盖历史安装证据。
    with (root / 'evidence' / receipt_name).open('x', encoding='utf-8') as stream:
        json.dump(result, stream, ensure_ascii=False, indent=2)
        stream.write('\n')
    print(json.dumps(result, ensure_ascii=False))


if __name__ == '__main__':
    main()
