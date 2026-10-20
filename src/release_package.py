"""从当前已验收的冻结构建生成手动发布包；不构建、不测试、不访问游戏目录。"""
import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path, PurePosixPath
import shutil
import subprocess
import uuid
import zipfile

from release_tools import ROOT, PWSH, contained, digest, read_json, verify_build, source_snapshot


VERSION = (ROOT / 'VERSION').read_text(encoding='utf-8').strip()
NAME = 'AC7MultiSP'
OLD_NAME = 'AC7MultiSP-2.16.0'
OLD_ARCHIVE_HASH = '5381a990515fcca8662505d1538384fa640ce3ea3108477da745be63dc18183a'
MOD = 'GameFiles/ue4ss/Mods/DualSP/'
PAK = 'Game/Content/Paks/~mods/AC7MultiSP_Mounts_P.pak'
LICENSES = ('UE4SS.txt', 'DearImGui.txt', 'MinHook.txt', 'nlohmann-json.txt')


def data_hash(data):
    return hashlib.sha256(data).hexdigest()


def json_bytes(value):
    return (json.dumps(value, ensure_ascii=False, indent=2) + '\n').encode('utf-8')


def file_record(data):
    return {'size': len(data), 'sha256': data_hash(data)}


def verify_record(data, record, label):
    if len(data) != record['size'] or data_hash(data) != record['sha256'].lower():
        raise ValueError('文件大小或摘要不符：' + label)


def archive_names(archive):
    """拒绝重复成员、目录项、链接及任何可能越界的成员名。"""
    names = []
    for entry in archive.infolist():
        name = entry.filename
        parts = PurePosixPath(name).parts
        if (entry.is_dir() or '\\' in name or ':' in name or name.startswith('/') or
                not parts or any(part in ('.', '..') for part in name.split('/')) or
                '' in name.split('/') or (entry.external_attr >> 16) & 0o170000 == 0o120000):
            raise ValueError('ZIP 含非法成员：' + name)
        names.append(name)
    if len(names) != len(set(names)) or len(names) != len({name.casefold() for name in names}):
        raise ValueError('ZIP 存在重复或大小写冲突成员')
    return set(names)


def microsoft_signature(path):
    """仅查询签名，不执行运行库安装程序。"""
    literal = str(path).replace("'", "''")
    command = ("[Console]::OutputEncoding=[Text.UTF8Encoding]::new($false);"
               f"$s=Get-AuthenticodeSignature -LiteralPath '{literal}';"
               "$s | Select-Object @{n='status';e={$_.Status.ToString()}},"
               "@{n='signer';e={$_.SignerCertificate.Subject}} | ConvertTo-Json -Compress")
    result = subprocess.run([PWSH, '-NoProfile', '-NonInteractive', '-Command', command],
                            check=True, capture_output=True, text=True, encoding='utf-8-sig')
    value = json.loads(result.stdout)
    if value.get('status') != 'Valid' or 'CN=Microsoft Corporation,' not in (value.get('signer') or ''):
        raise ValueError('运行库必须具备有效微软签名：' + path.name)
    return value


def legacy_payload(archive_path):
    """整包和旧清单都可信后，仅收取固定的运行库集合。"""
    receipt = read_json(ROOT / 'evidence/2.16.0-release.json')
    if (receipt.get('name') != NAME or receipt.get('version') != '2.16.0' or
            receipt.get('all_members_verified') is not True or
            receipt.get('sha256', '').lower() != OLD_ARCHIVE_HASH or
            digest(archive_path) != OLD_ARCHIVE_HASH or archive_path.stat().st_size != receipt['size']):
        raise ValueError('旧 2.16.0 发布包或来源回执不符')
    records = receipt['files']
    with zipfile.ZipFile(archive_path) as archive:
        names = archive_names(archive)
        expected = {OLD_NAME + '/' + name for name in records} | {OLD_NAME + '/文件清单.json'}
        if names != expected or len(names) != receipt['member_count']:
            raise ValueError('旧包成员与发布回执不符')
        manifest = json.loads(archive.read(OLD_NAME + '/文件清单.json').decode('utf-8-sig'))
        if (manifest.get('name') != NAME or manifest.get('version') != '2.16.0' or
                manifest.get('files') != records):
            raise ValueError('旧包内外文件清单不一致')
        runtimes = {}
        for name, record in records.items():
            data = archive.read(OLD_NAME + '/' + name)
            verify_record(data, record, name)
            if name.startswith('Runtimes/'):
                parts = PurePosixPath(name).parts
                if name != 'Runtimes/VC_redist.x64.exe' and not (
                        len(parts) == 3 and parts[1] == 'DirectX' and
                        PurePosixPath(name).suffix.lower() in ('.exe', '.dll', '.cab')):
                    raise ValueError('旧包运行库包含非预期文件：' + name)
                runtimes[name] = data
    required = {'runtimes/vc_redist.x64.exe', 'runtimes/directx/dxsetup.exe',
                'runtimes/directx/dsetup.dll', 'runtimes/directx/dsetup32.dll',
                'runtimes/directx/dxupdate.cab', 'runtimes/directx/apr2007_xinput_x64.cab'}
    if len(runtimes) != 158 or not required <= {name.lower() for name in runtimes}:
        raise ValueError('旧包必须包含完整的 158 份运行库文件')
    return runtimes, records


def frozen_sources(build_dir):
    """安装回执、测试回执与完整冻结构建必须属于同一代字节。"""
    build_path = build_dir / 'build-receipt.json'
    test_path = build_dir / 'test-receipt.json'
    build = read_json(build_path)
    validation_path = ROOT / ('evidence/' + VERSION + '-user-validation.json')
    validation = read_json(validation_path)
    current_sources = source_snapshot()
    metadata_transition = None
    verification = build
    if current_sources != build.get('source_files'):
        # 同版本验收转正式只允许candidate由true变false；原冻结回执不改写。
        before = build.get('source_files', {})
        changed = {name for name in set(before) | set(current_sources)
                   if before.get(name) != current_sources.get(name)}
        if changed != {'src/product.json'} or validation.get('release_metadata_only_authorized') is not True:
            raise ValueError('验收后来源变化超出发布状态元数据：' + repr(sorted(changed)))
        commit = validation['validated_commit']
        subprocess.run(['git', 'merge-base', '--is-ancestor', commit, 'HEAD'], cwd=ROOT, check=True)
        original = subprocess.check_output(['git', 'show', commit + ':src/product.json'], cwd=ROOT)
        expected = json.loads(original)
        if data_hash(original) != before['src/product.json'] or expected.get('candidate') is not True:
            raise ValueError('验收提交的产品元数据与冻结来源不符')
        expected['candidate'] = False
        if read_json(ROOT / 'src/product.json') != expected:
            raise ValueError('仅允许候选状态true转false，不能改其他产品字段')
        # 临时校验视图只替换已精确证明的元数据摘要，其余源/对象/资源沿原严格检查。
        verification = dict(build, source_files=dict(before, **{'src/product.json': current_sources['src/product.json']}))
        metadata_transition = {'field': 'candidate', 'before': True, 'after': False,
                               'original_sha256': before['src/product.json'],
                               'release_sha256': current_sources['src/product.json'],
                               'validated_commit': commit, 'original_build_receipt_unchanged': True}
    info = verify_build(verification, build_dir)
    installed = read_json(ROOT / ('evidence/' + VERSION + '-installed.json'))
    tests = read_json(test_path)
    build_hash, test_hash = digest(build_path), digest(test_path)
    if (info['version'] != VERSION or info['name'] != NAME or
            installed.get('name') != NAME or installed.get('candidate') != VERSION or
            installed.get('installed') is not True or installed.get('test_modules_run') is not True or
            installed.get('build_receipt_sha256') != build_hash or
            installed.get('test_receipt_sha256') != test_hash or
            tests.get('name') != NAME or tests.get('version') != VERSION or
            tests.get('passed') is not True or tests.get('build_receipt_sha256') != build_hash):
        raise ValueError('当前构建、通过测试和安装回执未形成完整关联')
    authorization_path = ROOT / ('evidence/' + VERSION + '-user-feedback-followup.json')
    authorization = read_json(authorization_path)
    if (validation.get('version') != VERSION or validation.get('main_acceptance_authorized') is not True or
            validation.get('build_receipt_sha256') != build_hash or
            validation.get('installed_receipt_sha256') != digest(ROOT / ('evidence/' + VERSION + '-installed.json')) or
            authorization.get('version') != VERSION or authorization.get('release_authorized') is not True):
        raise ValueError('缺少当前冻结构建的用户验收或本轮发布授权')
    if not tests.get('test_sources'):
        raise ValueError('测试回执缺少来源摘要')
    for name, expected in tests['test_sources'].items():
        if digest(contained(ROOT / name, ROOT)) != expected.lower():
            raise ValueError('测试来源已改变：' + name)
    sources = {'Scripts/' + name: ROOT / 'dist/DualSP/Scripts' / name for name in info['scripts']}
    sources.update({'bin/DualSP.dll': build_dir / 'DualSP.dll',
                    'bin/DualSPUI.dll': build_dir / 'DualSPUI.dll', 'VERSION': ROOT / 'VERSION'})
    if len(sources) != len(info['scripts']) + 3 or set(sources) != set(installed['runtime_files']):
        raise ValueError('运行文件必须与产品Lua清单及已安装回执逐项一致')
    runtime = {}
    for name, path in sources.items():
        data = contained(path, ROOT).read_bytes()
        if data_hash(data) != installed['runtime_files'][name].lower():
            raise ValueError('运行文件与已装回执不符：' + name)
        runtime[name] = data
    assets = build['mount_assets']
    asset_record = installed.get('mount_assets', {})
    if (asset_record.get('path') != PAK or asset_record.get('sha256') != assets['sha256'] or
            asset_record.get('variant_count') != len(assets['variants']) or
            asset_record.get('file_count') != assets['file_count']):
        raise ValueError('当前挂架包与安装回执不符')
    pak = (build_dir / assets['file']).read_bytes()
    if data_hash(pak) != assets['sha256'].lower():
        raise ValueError('挂架包读取期间发生变化')
    return runtime, pak, dict(installed, release_metadata_transition=metadata_transition), build_hash, test_hash, digest(validation_path), digest(authorization_path)


def version_label_sources(build_dir, accepted_dir):
    """仅版本标识重建：逐源对照已验收安装，不伪造新版本安装或实机验收。"""
    validation_path = ROOT / ('evidence/' + VERSION + '-user-validation.json')
    authorization_path = ROOT / ('evidence/' + VERSION + '-user-feedback-followup.json')
    validation, authorization = read_json(validation_path), read_json(authorization_path)
    old_version = validation['validated_version']
    if (validation.get('version') != VERSION or validation.get('main_acceptance_authorized') is not True or
            validation.get('version_label_only_authorized') is not True or old_version == VERSION or
            authorization.get('version') != VERSION or authorization.get('release_authorized') is not True or
            accepted_dir != contained(ROOT / validation['validated_build_dir'], ROOT / 'Temp').resolve(strict=True)):
        raise ValueError('缺少已验收基线或仅版本标识重建的发布授权')
    old_build = read_json(accepted_dir / 'build-receipt.json')
    old_tests = read_json(accepted_dir / 'test-receipt.json')
    installed_path = ROOT / ('evidence/' + old_version + '-installed.json')
    old_installed = read_json(installed_path)
    old_build_hash, old_test_hash = digest(accepted_dir / 'build-receipt.json'), digest(accepted_dir / 'test-receipt.json')
    if (old_build.get('version') != old_version or old_build.get('receipt_schema') != 2 or
            old_tests.get('passed') is not True or old_tests.get('build_receipt_sha256') != old_build_hash or
            old_installed.get('installed') is not True or old_installed.get('candidate') != old_version or
            old_installed.get('build_receipt_sha256') != old_build_hash or
            old_installed.get('test_receipt_sha256') != old_test_hash or
            validation.get('validated_build_receipt_sha256') != old_build_hash or
            validation.get('validated_installed_receipt_sha256') != digest(installed_path)):
        raise ValueError('已验收基线的构建、测试和安装记录不一致')
    for name, expected in old_build['objects'].items():
        if digest(contained(accepted_dir / name, accepted_dir)) != expected:
            raise ValueError('已验收构建中间文件改变：' + name)
    for name, field in (('DualSP.dll', 'candidate_sha256'), ('DualSPUI.dll', 'ui_sha256')):
        if (digest(accepted_dir / name) != old_build[field] or
                old_installed['runtime_files']['bin/' + name] != old_build[field]):
            raise ValueError('已验收产物改变：' + name)
    current = read_json(build_dir / 'build-receipt.json')
    info = verify_build(current, build_dir)
    tests = read_json(build_dir / 'test-receipt.json')
    build_hash, test_hash = digest(build_dir / 'build-receipt.json'), digest(build_dir / 'test-receipt.json')
    if (info['version'] != VERSION or tests.get('name') != NAME or tests.get('version') != VERSION or tests.get('passed') is not True or
            tests.get('build_receipt_sha256') != build_hash or tests.get('test_sources') != old_tests.get('test_sources') or
            current['build_define'] != old_build['build_define'] or current['toolchain'] != old_build['toolchain'] or
            set(current['source_files']) != set(old_build['source_files'])):
        raise ValueError('新构建条件、来源集合或检查来源与验收基线不同')
    changed = {name for name, value in current['source_files'].items() if value != old_build['source_files'][name]}
    # 新卡片界面的版本文字已移到management_panel.hpp；仍只允许一个UI标识文件精确换版。
    ui_labels = changed & {'src/overlay.cpp', 'src/management_panel.hpp'}
    allowed = {'VERSION', 'src/product.json', 'dist/DualSP/Scripts/main.lua'} | ui_labels
    if len(ui_labels) != 1 or changed != allowed:
        raise ValueError('超出版本标识差异：' + repr(sorted(changed)))
    commit = validation['validated_commit']
    subprocess.run(['git', 'merge-base', '--is-ancestor', commit, 'HEAD'], cwd=ROOT, check=True)
    for name in allowed:
        original = subprocess.check_output(['git', 'show', commit + ':' + name], cwd=ROOT)
        if data_hash(original) != old_build['source_files'][name]:
            raise ValueError('原提交与已测来源不符：' + name)
        runtime_name = name.removeprefix('dist/DualSP/')
        if name in ('VERSION', 'dist/DualSP/Scripts/main.lua'):
            if data_hash(original) != old_installed['runtime_files'][runtime_name]:
                raise ValueError('原版本标识与已装记录不符：' + name)
        actual = (ROOT / name).read_bytes()
        if name == 'src/product.json':
            expected = json.loads(original)
            expected.update(candidate=False, upgrade_receipt='evidence/' + old_version + '-installed.json')
            equal = json.loads(actual) == expected
        else:
            equal = actual == original.replace(old_version.encode(), VERSION.encode())
        if not equal:
            raise ValueError('版本标识文件含其他改动：' + name)
    for name, expected in tests['test_sources'].items():
        if digest(ROOT / name) != expected:
            raise ValueError('检查来源改变：' + name)
    assets = current['mount_assets']
    if (assets['sha256'] != old_build['mount_assets']['sha256'] or
            assets['members'] != old_build['mount_assets']['members'] or
            assets['sha256'] != old_installed['mount_assets']['sha256']):
        raise ValueError('挂载资源不同于已验收版本')
    runtime = {'Scripts/' + name: (ROOT / 'dist/DualSP/Scripts' / name).read_bytes() for name in info['scripts']}
    runtime.update({'bin/DualSP.dll': (build_dir / 'DualSP.dll').read_bytes(),
                    'bin/DualSPUI.dll': (build_dir / 'DualSPUI.dll').read_bytes(), 'VERSION': (ROOT / 'VERSION').read_bytes()})
    if len(runtime) != len(info['scripts']) + 3 or set(runtime) != set(old_installed['runtime_files']):
        raise ValueError('运行文件集合不同于已验收版本')
    for name, data in runtime.items():
        if name not in ('bin/DualSP.dll', 'bin/DualSPUI.dll', 'VERSION', 'Scripts/main.lua'):
            if data_hash(data) != old_installed['runtime_files'][name]:
                raise ValueError('非版本运行文件改变：' + name)
    manifest = {'runtime_files': {name: data_hash(data) for name, data in runtime.items()},
                'mount_assets': old_installed['mount_assets'],
                'validated_version': old_version, 'actual_installed_receipt': installed_path.relative_to(ROOT).as_posix()}
    return runtime, (build_dir / assets['file']).read_bytes(), manifest, build_hash, test_hash, digest(validation_path), digest(authorization_path)


def verify_package(path, prefix, payload):
    """完整解压回读每个白名单成员，包括文件清单自身。"""
    with zipfile.ZipFile(path) as archive:
        if archive_names(archive) != {prefix + '/' + name for name in payload}:
            raise ValueError('新包成员不符合发布白名单')
        for name, data in payload.items():
            actual = archive.read(prefix + '/' + name)
            if len(actual) != len(data) or data_hash(actual) != data_hash(data):
                raise ValueError('新包完整回读失败：' + name)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-dir', type=Path, required=True)
    parser.add_argument('--validated-build-dir', type=Path, help='仅版本标识重建时，显式给出已获用户验收的旧冻结构建')
    parser.add_argument('--runtime-archive', type=Path, default=ROOT / 'Release/AC7MultiSP-2.16.0.zip')
    parser.add_argument('--output', type=Path, default=ROOT / ('Release/' + NAME + '-' + VERSION + '.zip'))
    args = parser.parse_args()
    build_dir = contained(args.build_dir, ROOT / 'Temp').resolve(strict=True)
    archive_path = contained(args.runtime_archive, ROOT).resolve(strict=True)
    output = contained(args.output, ROOT / 'Release')
    receipt_path = ROOT / ('evidence/' + VERSION + '-release.json')
    if output.exists() or receipt_path.exists() or output.suffix.lower() != '.zip':
        raise ValueError('发布包或发布回执已存在，或输出不是 ZIP；拒绝覆盖')
    accepted_dir = contained(args.validated_build_dir, ROOT / 'Temp').resolve(strict=True) if args.validated_build_dir else None
    def sources():
        return version_label_sources(build_dir, accepted_dir) if accepted_dir else frozen_sources(build_dir)
    runtime, pak, installed, build_hash, test_hash, validation_hash, authorization_hash = sources()
    actual_installed = installed.get('actual_installed_receipt', 'evidence/' + VERSION + '-installed.json')
    validated_version = installed.get('validated_version', VERSION)
    runtimes, legacy_records = legacy_payload(archive_path)
    payload = {MOD + name: data for name, data in runtime.items()}
    legacy_sources = {
        'GameFiles/dwmapi.dll': ROOT / 'payload/dwmapi.dll',
        'GameFiles/ue4ss/UE4SS.dll': ROOT / 'payload/ue4ss/UE4SS.dll',
        'GameFiles/ue4ss/UE4SS-settings.ini': ROOT / 'payload/ue4ss/UE4SS-settings.ini',
        MOD + 'Audio/SP2.wav': ROOT / 'dist/DualSP/Audio/SP2.wav',
    }
    legacy_sources.update({MOD + 'licenses/' + name:
                          ROOT / 'payload/ue4ss/Mods/DualSP/licenses' / name for name in LICENSES})
    for name, path in legacy_sources.items():
        data = contained(path, ROOT).read_bytes()
        verify_record(data, legacy_records[name], name)
        payload[name] = data
    payload['GameFiles/ue4ss/Mods/mods.txt'] = b'DualSP : 1\r\n'
    payload['GameFiles/' + PAK] = pak
    if len(payload) != len(runtime) + len(legacy_sources) + 2:
        raise ValueError('基础GameFiles必须与运行清单、固定依赖、启用列表及PAK逐项一致')
    # 保留小写 candidate 版本字段，供现有 install.py 的 --base-receipt 读取。
    install = {'Name': NAME, 'Version': VERSION, 'Candidate': False, 'Distribution': 'manual',
               'name': NAME, 'candidate': VERSION, 'runtime_files': installed['runtime_files'],
               'mount_assets': installed['mount_assets'], 'build_receipt_sha256': build_hash,
               'test_receipt_sha256': test_hash, 'user_validation_sha256': validation_hash,
               'release_authorization_sha256': authorization_hash,
               'gameplay_validated_version': validated_version, 'version_label_rebuild': bool(accepted_dir),
               'release_metadata_transition': installed.get('release_metadata_transition')}
    payload[MOD + 'install-receipt.json'] = json_bytes(install)
    for name in ('README_zh-CN.txt', 'README_en.txt'):
        data = contained(ROOT / 'docs' / name, ROOT).read_bytes()
        if not data.decode('utf-8-sig').strip():
            raise ValueError('发布说明为空：' + name)
        payload[name] = data
    payload.update(runtimes)
    staging = ROOT / 'Temp' / ('release-package-' + uuid.uuid4().hex)
    staging.mkdir()
    signatures = {}
    for name in ('Runtimes/VC_redist.x64.exe', 'Runtimes/DirectX/DXSETUP.exe'):
        path = staging / PurePosixPath(name).name
        path.write_bytes(runtimes[name])
        signatures[PurePosixPath(name).name] = microsoft_signature(path)
        if data_hash(path.read_bytes()) != data_hash(runtimes[name]):
            raise ValueError('签名查询前后运行库字节改变')
    records = {name: file_record(data) for name, data in sorted(payload.items())}
    manifest = {'name': NAME, 'version': VERSION, 'Candidate': False,
                'Distribution': 'manual', 'files': records}
    payload['文件清单.json'] = json_bytes(manifest)
    prefix = NAME + '-' + VERSION
    temporary = staging / output.name
    with zipfile.ZipFile(temporary, 'x', compression=zipfile.ZIP_DEFLATED, compresslevel=6) as archive:
        for name, data in sorted(payload.items()):
            archive.writestr(prefix + '/' + name, data)
    verify_package(temporary, prefix, payload)
    # 不重建，只再次复核来源仍冻结；写目标时用独占创建，绝不覆盖旧包。
    sources()
    output.parent.mkdir(parents=True, exist_ok=True)
    with temporary.open('rb') as source, output.open('xb') as target:
        shutil.copyfileobj(source, target)
    verify_package(output, prefix, payload)
    result = {'name': NAME, 'version': VERSION, 'Candidate': False, 'Distribution': 'manual',
              'created_at': datetime.now(timezone.utc).isoformat(),
              'archive': output.relative_to(ROOT).as_posix(), 'size': output.stat().st_size,
              'sha256': digest(output), 'member_count': len(payload),
              'game_file_count': sum(name.startswith('GameFiles/') for name in payload),
              'runtime_file_count': len(runtimes), 'all_members_verified': True,
              'test_modules_run': False, 'recompiled': False, 'game_started_by_agent': False,
              'build_receipt_sha256': build_hash, 'test_receipt_sha256': test_hash,
              'installed_receipt_sha256': digest(ROOT / actual_installed),
              'actual_installed_receipt': actual_installed, 'gameplay_validated_version': validated_version,
              'version_label_rebuild': bool(accepted_dir), 'new_version_installed': not bool(accepted_dir),
              'release_metadata_transition': installed.get('release_metadata_transition'),
              'packager_sha256': digest(Path(__file__)),
              'user_validation_sha256': validation_hash, 'release_authorization_sha256': authorization_hash,
              'runtime_source': '完整核验 2.16.0 发布包及清单后复用的原始微软运行库，历史来源见2.16发布回执',
              'runtime_archive_sha256': OLD_ARCHIVE_HASH,
              'microsoft_signatures': signatures, 'files': records}
    with receipt_path.open('x', encoding='utf-8') as stream:
        stream.write(json_bytes(result).decode('utf-8'))
    print(json.dumps({key: value for key, value in result.items() if key != 'files'}, ensure_ascii=False, indent=2))


if __name__ == '__main__':
    main()
