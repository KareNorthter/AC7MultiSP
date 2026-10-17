"""安装 AC7MultiSP 完整构建；显式基线核对、整目录备份，默认仅预检。"""
import argparse
from datetime import datetime, timedelta, timezone
import json
import os
from pathlib import Path
import shutil
from release_tools import ROOT, digest, inventory, product, verify_build, require_stopped, contained, read_json


def main():
    """限定来源、验证完整暂存，整目录备份后交换；不启动游戏。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-dir', type=Path, required=True)
    parser.add_argument('--apply', action='store_true')
    parser.add_argument('--base-receipt', type=Path, help='已安装版本的完整 runtime_files 回执，默认读取产品清单')
    args = parser.parse_args()
    root = ROOT
    info = product()
    build = contained(args.build_dir, root / 'Temp').resolve(strict=True)
    receipt = read_json(build / 'build-receipt.json')
    verify_build(receipt, build)
    version = info['version']
    tests = read_json(build / 'test-receipt.json')
    if tests.get('passed') is not True or tests.get('build_receipt_sha256') != digest(build / 'build-receipt.json'):
        raise ValueError('缺少对应构建的通过测试回执')
    for name, expected_hash in tests['test_sources'].items():
        if digest(root / name) != expected_hash:
            raise ValueError('测试来源改变，需要重跑：' + name)
    base_path = contained(args.base_receipt or root / info['upgrade_receipt'], root / 'evidence')
    previous = read_json(base_path)
    expected_old = previous['runtime_files']
    base_scripts={name for name in expected_old if name.startswith('Scripts/') and name.endswith('.lua')}
    required={'bin/DualSP.dll', 'bin/DualSPUI.dll', 'VERSION'} | base_scripts
    if set(expected_old)!=required or 'Scripts/main.lua' not in base_scripts:
        raise ValueError('升级基线必须包含完整运行文件，不能使用局部回执')
    if not {'dualsp_fixed_mount_update', 'dualsp_mount_preview', 'dualsp_three_sp_supported', 'dualsp_refresh_weapon_icons'} <= set(receipt['required_exports_verified']):
        raise ValueError('核心缺少当前必需接口')
    result_path = root / 'evidence' / (version + '-installed.json')
    if args.apply and result_path.exists():
        raise ValueError('此版本已有安装回执，请先核对，不能覆盖历史')
    game = Path('E:/SteamLibrary/steamapps/common/ACE COMBAT 7')
    asset_relative = 'Game/Content/Paks/~mods/AC7MultiSP_Mounts_P.pak'
    asset_target = contained(game / asset_relative, game)
    asset_source = build / receipt['mount_assets']['file']
    asset_hash = receipt['mount_assets']['sha256']
    old_asset = previous.get('mount_assets')
    if old_asset:
        if old_asset.get('path')!=asset_relative or digest(asset_target)!=old_asset['sha256']:
            raise ValueError('已装挂架资源与基线不符')
    elif asset_target.exists():
        raise ValueError('挂架包目标已被未登记文件占用，停止覆盖')
    mod = game / 'ue4ss/Mods/DualSP'  # 不操作其他模组和发布包。
    if mod.is_symlink() or mod.is_junction() or not mod.resolve(strict=True).is_relative_to(game.resolve(strict=True)):
        raise ValueError('安装目标路径无效')
    actual_scripts={'Scripts/' + path.relative_to(mod / 'Scripts').as_posix() for path in (mod / 'Scripts').rglob('*.lua')}
    if actual_scripts!=base_scripts:
        raise ValueError('现有 Lua 集合与基线不一致，不能遗漏未登记文件')
    if not base_scripts <= {'Scripts/' + name for name in info['scripts']}:
        raise ValueError('移除旧 Lua 需要明确迁移安排，不能静默遗留或删除')
    for name, expected in expected_old.items():
        if digest(contained(mod / name, mod)) != expected.lower():
            raise ValueError('当前安装与指定基线不符，停止覆盖：' + name)
    settings = read_json(mod / 'profiles.json')
    if settings.get('schema') != 2 or settings.get('management_revision') not in (5, 6, 7):
        raise ValueError('本机升级要求已有 schema 2 / management_revision 5、6或7，请先核对上一版启动后的配置')
    sources = {'Scripts/' + name: root / 'dist/DualSP/Scripts' / name for name in info['scripts']}
    sources.update({'bin/DualSP.dll': build / 'DualSP.dll', 'bin/DualSPUI.dll': build / 'DualSPUI.dll', 'VERSION': root / 'VERSION'})
    hashes = {name: digest(path) for name, path in sources.items()}  # 安装白名单不含个人配置。
    plan = {'candidate': version, 'base': previous['candidate'], 'runtime_files': hashes,
            'mount_assets': {'path':asset_relative,'sha256':asset_hash,
                             'variant_count':len(receipt['mount_assets']['variants']),
                             'file_count':receipt['mount_assets']['file_count']},
            'name': info['name'], 'configuration_migration': '安装保留个人配置原字节，首次启动先备份再迁移至管理参数7；新增注视默认关闭且键盘未绑定',
            'base_receipt': base_path.relative_to(root).as_posix(), 'base_receipt_sha256': digest(base_path),
            'build_receipt_sha256': digest(build / 'build-receipt.json'), 'test_receipt_sha256': digest(build / 'test-receipt.json'),
            'game_started_by_agent': False, 'test_modules_run': True, 'gameplay_verified': False}
    if not args.apply:
        print(json.dumps({**plan, 'status': '预检通过，未安装'}, ensure_ascii=False))
        return
    require_stopped()
    before = inventory(mod)  # 进程已关闭，完整日志和配置可以稳定备份。
    saves = Path(os.environ['LOCALAPPDATA']) / 'BANDAI NAMCO Entertainment/ACE COMBAT 7'
    if not saves.is_dir():
        raise ValueError('存档目录不存在')
    protected_paths = [game / name for name in ('Ace7Game.exe', 'dwmapi.dll', 'ue4ss/UE4SS.dll', 'ue4ss/Mods/mods.txt')]
    protected_paths += [path for path in saves.rglob('*') if path.is_file() and (path.suffix.lower() == '.sav' or path.name == 'GraphicsSetting.ini')]
    for folder in (game / 'ue4ss/Mods').iterdir():
        if folder.is_dir() and folder != mod:
            for name in inventory(folder):
                protected_paths.append(folder / name)
    for folder in (game / 'Game/Content/Paks').glob('*'):
        if folder.is_dir():
            for name in inventory(folder):
                if folder / name != asset_target:
                    protected_paths.append(folder / name)
    protected = {path: digest(path) for path in protected_paths}  # 仅校验摘要，公共证据不保存个人内容。
    now = datetime.now(timezone(timedelta(hours=8)))
    batch = now.strftime('%Y%m%d-%H%M%S-%f')
    backup = root / 'backup' / ('AC7MultiSP-'  + batch)
    staging = root / 'Temp' / ('AC7MultiSP-install-' + batch) / 'DualSP'
    if backup.exists() or staging.exists():
        raise ValueError('备份或暂存目录已存在')
    if not backup.resolve().is_relative_to((root / 'backup').resolve()) or not staging.resolve().is_relative_to((root / 'Temp').resolve()):
        raise ValueError('备份或暂存目录越界')
    contained(mod, game)
    contained(backup, root / "backup")
    contained(staging, root / "Temp")
    shutil.copytree(mod, staging)
    asset_staging = staging.parent / asset_source.name
    shutil.copy2(asset_source, asset_staging)
    if digest(asset_staging)!=asset_hash:
        raise ValueError('挂架包暂存核对失败')
    for name, source in sources.items():
        target = staging / name  # 仅复制已核验运行文件，保留已有音频。
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, target)
    install = json.loads((staging / 'install-receipt.json').read_text(encoding='utf-8-sig'))
    install.update(Name=info['name'], Version=version, Candidate=info.get('candidate', False), InstalledAt=now.isoformat(),
                   CandidateCoreSHA256=receipt['candidate_sha256'], BackupPath=str(backup), GameStartedByAgent=False)
    (staging / 'install-receipt.json').write_text(json.dumps(install, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    expected = inventory(staging)
    allowed = set(sources) | {'install-receipt.json'}  # 仅允许明确的运行文件与安装回执变化。
    if set(expected) != set(before) | set(sources) or any(expected[name] != value for name, value in before.items() if name not in allowed):
        raise ValueError('暂存包含计划外变化，现有安装未改')
    if any(expected[name] != value for name, value in hashes.items()):
        raise ValueError('暂存运行文件核验失败')
    require_stopped()
    if inventory(mod) != before or any(digest(path) != value for path, value in protected.items()):
        raise ValueError('安装前文件发生并发变化，现有安装未改')
    backup.mkdir(parents=True)
    shutil.copytree(saves, backup / 'SaveData')
    for path, value in protected.items():
        if path.is_relative_to(saves) and digest(backup / 'SaveData' / path.relative_to(saves)) != value:
            raise ValueError('存档备份核验失败，现有安装未改')
    (backup / 'manifest.json').write_text(json.dumps(before, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    require_stopped()
    verify_build(receipt, build)
    if inventory(mod) != before:
        raise ValueError("交换目录前安装已改变")
    contained(mod, game)
    contained(backup / 'DualSP', root / 'backup')
    contained(staging, root / 'Temp')
    if (old_asset and digest(asset_target)!=old_asset['sha256']) or (not old_asset and asset_target.exists()):
        raise ValueError('交换前挂架包发生并发变化')
    asset_backup = backup / 'MountAssets' / asset_target.name
    asset_backup.parent.mkdir()
    if old_asset:
        asset_target.rename(asset_backup)
    try:
        asset_staging.rename(asset_target)
        mod.rename(backup / 'DualSP')  # 两个绝对目标均已检查，无递归删除和跨壳路径拼接。
        staging.rename(mod)
    except OSError:
        # 交换第二步失败时归还原目录；不删除任何文件。
        if not mod.exists() and (backup / 'DualSP').exists():
            (backup / 'DualSP').rename(mod)
        if asset_target.exists():
            asset_target.rename(asset_staging)
        if old_asset:
            asset_backup.rename(asset_target)
        raise
    if digest(asset_target)!=asset_hash or inventory(mod) != expected or inventory(backup / 'DualSP') != before or any(digest(path) != value for path, value in protected.items()):
        raise RuntimeError('安装后校验失败，完整备份位于：' + str(backup))
    result = {**plan, 'installed': True, 'installed_at': now.isoformat(), 'backup': str(backup),
              'full_backup_verified': True, 'profiles_preserved': True, 'save_backup_verified': True,
              'other_mod_files_preserved': True, 'protected_files_verified': len(protected), 'gameplay_verified': False}
    with result_path.open('x', encoding='utf-8') as stream:
        json.dump(result, stream, ensure_ascii=False, indent=2)
        stream.write('\n')
    print(json.dumps(result, ensure_ascii=False))


if __name__ == '__main__':
    main()
