"""正式构建、安装和打包共用的来源与路径校验；导入不操作游戏。"""
import hashlib
import json
import os
import re
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
PWSH = r"C:\Program Files\PowerShell\7\pwsh.exe"
OLD_CORE_HASH = "d7d7055ca7909021eeaca15e63ff5ff253d2eb7e60a3fe3b79dceeab231de339"


def digest(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def read_json(path):
    return json.loads(Path(path).read_text(encoding='utf-8-sig'))


def write_json(path, value):
    Path(path).write_text(json.dumps(value, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')


def contained(path, parent):
    """拒绝越界及路径任一级链接；移动前也必须调用。"""
    path, parent = Path(path).absolute(), Path(parent).resolve(strict=True)
    if path == parent or not path.resolve().is_relative_to(parent):
        raise ValueError('目标路径越界：' + str(path))
    for item in (path, *path.parents):
        if item.is_symlink() or item.is_junction():
            raise ValueError('路径包含链接：' + str(item))
        if item == parent:
            break
    return path


def inventory(directory):
    directory = Path(directory)
    if directory.is_symlink() or directory.is_junction():
        raise ValueError('目录不能是链接：' + str(directory))
    directory = directory.resolve(strict=True)
    result = {}
    # 每个目录项检查一次；不对每个文件反复解析全部父级。
    for current, folders, files in os.walk(directory, followlinks=False):
        for name in folders + files:
            path = Path(current) / name
            if path.is_symlink() or path.is_junction():
                raise ValueError('目录含链接：' + str(path))
        for name in files:
            path = Path(current) / name
            result[path.relative_to(directory).as_posix()] = digest(path)
    return result


def product(root=ROOT):
    value = read_json(root / 'src/product.json')
    value['version'] = (root / 'VERSION').read_text(encoding='utf-8').strip()
    if not re.fullmatch(r'\d+\.\d+\.\d+(?: alpha[1-9]\d*)?', value['version']):
        raise ValueError('VERSION 必须是三位数字版本，可附加内部测试标识 alphaN')
    expected = value['scripts']
    script_dir = root / 'dist/DualSP/Scripts'
    actual = sorted(p.relative_to(script_dir).as_posix() for p in script_dir.rglob('*.lua'))
    if len(set(expected)) != len(expected) or sorted(expected) != actual:
        raise ValueError('Lua 文件与产品清单不一致；新增模块须登记后整套交付')
    return value


def source_snapshot(root=ROOT):
    """冻结全部产品 C++/头文件、第三方树、Lua 和正式工具；检测增删改。"""
    info = product(root)
    paths = {root / 'VERSION', root / 'src/product.json'}
    paths.update((root / 'src').rglob('*.hpp'))
    paths.update((root / 'src').rglob('*.cpp'))
    paths.update(p for p in (root / 'vendor').rglob('*') if p.is_file())
    paths.update(root / 'dist/DualSP/Scripts' / name for name in info['scripts'])
    paths.update(root / 'src' / name for name in (
        'build.py', 'build_mount_assets.py', 'distribution_symbols.py', 'release_tools.py', 'install.py', 'package-release.py', 'release_package.py',
        'build.cmd', 'build-core-portable.cmd', 'build-ui.cmd'))
    paths.update(root / name for name in ('src/research/build_runtime_mount_data.py',
        'src/research/remaining_external_mount_data.py','src/research/prepare_remaining_external_mounts.py',
        'evidence/remaining-external-mounts-20261028.json','evidence/remaining-external-mount-audit-20261028.json',
        'src/research/native_static_batch_data.py',
        'src/research/prepare_native_four_mounts.py','evidence/static-native-three-aircraft.json',
        'evidence/m31b-mount-observation.json',
        'src/research/m31b_offset_data.py','src/research/prepare_m31b_saam_offset.py',
        'evidence/m31b-saam-offset-r1.json',
        'evidence/m31b-saam-offset-r2.json',
        'evidence/m31b-saam-offset-r3.json',
        'evidence/f14a-restart-feedback-r3.json',
        'src/research/qualify_native_mount_collisions.py',
        'evidence/native-seven-collision-preservation.json',
        'evidence/native-seven-collision-feedback.json',
        'src/research/f18e_layout_feedback_data.py','src/research/prepare_f18e_layout_feedback.py',
        'evidence/f18e-layout-feedback-r1.json','evidence/native-batch-user-feedback-20261020.json',
        'src/research/split_static_mount_asset.py',
        'src/research/prepare_f18etg_socket_r2.py','src/research/f18etg_socket_r2_data.py',
        'evidence/f18etg-whole-rack-socket-r2.json','evidence/f18etg-hangar-freeze-20261021.json',
        'src/research/audit_mount_socket_references.py','evidence/mount-socket-reference-audit-20261021.json',
        'evidence/mount-resource-qualification-fix.json',
        'evidence/view-target-implementation.json','evidence/target-selection-research.json',
        'evidence/mod-aircraft-compat-static.json',
        'evidence/mod-electronic-compat-static.json','evidence/mod-infinite-ammo-compat-static.json',
        'src/research/local_four_runtime_data.py','src/research/prepare_local_four_mounts.py',
        'evidence/local-four-mount-runtime.json','evidence/active-five-mount-static-plan.json',
        'src/research/build_animated_runtime_data.py','src/research/review_su47_native_endpoints.py',
        'src/research/summarize_animation_product_access.py','evidence/mount-expansion-animated.json',
        'evidence/su47-native-endpoint-geometry.json','evidence/sp-animation-product-access.json',
        'evidence/mount-expansion-batch1.json','evidence/mount-expansion-batch2.json','evidence/mount-expansion-batch3.json',
        'evidence/static-expansion-2.11.0.json',
        'evidence/static-expansion-2.12.0.json','evidence/static-expansion-2.13.0.json','evidence/static-expansion-2.14.0.json',
        'evidence/static-mount-five-aircraft-review.json',
        'evidence/static-mount-five-aircraft-feedback.json',
        'evidence/f04e-ugb-model-capacity.json',
        'evidence/f04e-ugb-alternating-stock.json',
        'evidence/aircraft-repeat-pause-restart.json',
        'evidence/camera-interaction-v2-source.json','evidence/view-target-l3-marker-update.json',
        'evidence/m35d-inner-4agm-layout.json',
        'evidence/sp-combination-static-split-experiment.json'))
    paths.update(root / name for name in (
        'evidence/special-aircraft-native-fallback-20261022.json',
        'evidence/gun-salvo-xr302-inspection.json','evidence/gun-salvo-adler-hgp-static.json',
        'evidence/gun-salvo-adler-hgp-live.json','evidence/gun-salvo-mod-pod-survey.json',
        'src/research/survey_gun_pods.py',
        'evidence/mod-special-sp-contract-20261022.json',
        'evidence/aircraft-native-pause-handoff-alpha2.json',
        'evidence/2.24.0-alpha1-source-intake.json',
        'evidence/2.24.0-alpha2-source-intake.json',
        'evidence/2.24.1-release-authorization.json'))
    paths.update(p for p in (root / 'docs/user').rglob('*')
                 if p.is_file() and (p.suffix == '.txt' or p.name.startswith('game.')))
    paths.add(root / 'src/user_texts.py')
    paths.add(root / 'docs/user/interface/author-links.json')
    return {p.relative_to(root).as_posix(): digest(contained(p, root)) for p in sorted(paths)}


def tree_fingerprint(directory):
    files = inventory(directory)
    data = json.dumps(files, sort_keys=True, separators=(',', ':')).encode('utf-8')
    return {'sha256': hashlib.sha256(data).hexdigest(), 'file_count': len(files)}


def verify_mount_manifest(assets, expected_variants):
    """按生成白名单验证可变数量资源，不能仅信回执自报数量。"""
    expected_members = {'Nimbus/Content/AC7MultiSP/Research/' + name + suffix
                        for name in expected_variants for suffix in ('.uasset', '.uexp')}
    if (assets.get('file') != 'AC7MultiSP_Mounts_P.pak' or not expected_variants or
            assets.get('variants') != expected_variants or len(set(expected_variants)) != len(expected_variants) or
            assets.get('file_count') != len(expected_members) or
            set(assets.get('members', {})) != expected_members or assets.get('independent_readback') is not True):
        raise ValueError('缺少完整挂架资源构建回执')


def verify_build(receipt, build_dir=None, root=ROOT):
    """旧版不完整回执不能作为当前构建来源证明。"""
    info = product(root)
    if receipt.get('receipt_schema') != 2 or receipt.get('name') != info['name'] or receipt.get('version') != info['version']:
        raise ValueError('构建回执格式、名称或版本不符；请使用正式入口完整构建')
    if receipt.get('source_files') != source_snapshot(root):
        raise ValueError('构建来源已增删或修改，须重新完整构建')
    if receipt.get('build_define') != info['core_define']:
        raise ValueError('核心编译条件不匹配')
    for name, value in receipt['protected_files'].items():
        if digest(root / name) != value:
            raise ValueError('受保护文件改变：' + name)
    if build_dir:
        symbols = receipt.get('distribution_symbols', {})
        if symbols.get('schema') != 1 or set(symbols.get('files', {})) != {'DualSP.dll', 'DualSPUI.dll'}:
            raise ValueError('缺少交付DLL精简与私有诊断件回执')
        for name, record in symbols['files'].items():
            if (record.get('private_path') != 'private-debug/' + name or record.get('loader_contract_preserved') is not True or
                    digest(contained(Path(build_dir) / record['private_path'], build_dir)) != record['private_sha256'] or
                    digest(contained(Path(build_dir) / name, build_dir)) != record['distribution_sha256']):
                raise ValueError('交付DLL或私有诊断件改变')
        assets = receipt.get('mount_assets',{})
        from research.build_runtime_mount_data import build as runtime_mount_data
        expected_variants = runtime_mount_data()['assets']
        verify_mount_manifest(assets, expected_variants)
        if digest(contained(Path(build_dir)/assets['file'],build_dir))!=assets['sha256']:
            raise ValueError('挂架资源包与构建回执不符')
        for name, expected in receipt['objects'].items():
            if digest(contained(Path(build_dir) / name, build_dir)) != expected:
                raise ValueError('构建中间文件改变：' + name)
        for name, field in [('DualSP.dll', 'candidate_sha256'), ('DualSPUI.dll', 'ui_sha256')]:
            if digest(Path(build_dir) / name) != receipt[field]:
                raise ValueError('构建产物摘要不符：' + name)
    return info


def require_stopped():
    result = subprocess.run([PWSH, '-NoProfile', '-NonInteractive', '-Command',
                             '@(Get-Process -Name Ace7Game -ErrorAction SilentlyContinue).Count'],
                            check=True, capture_output=True, text=True, encoding='utf-8')
    if int(result.stdout.strip()):
        raise RuntimeError('游戏仍在运行，未安装；请先正常退出')
