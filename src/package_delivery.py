"""从已核验的正式发布 ZIP 派生通用包、Vortex 扩展及单 EXE 的原始资源输入。"""
import argparse
import hashlib
import json
import re
import zipfile
from pathlib import Path, PurePosixPath

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'src/delivery'


def sha(data):
    return hashlib.sha256(data).hexdigest()


def encoded(value):
    return (json.dumps(value, ensure_ascii=False, indent=2) + '\n').encode('utf-8')


def valid_path(name):
    parts = name.split('/')
    if ('\\' in name or ':' in name or PurePosixPath(name).is_absolute() or
            any(not p or p in ('.', '..') or p.endswith(('.', ' ')) or
                re.match(r'(?i)^(CON|PRN|AUX|NUL|COM[0-9]|LPT[0-9])(\.|$)', p) for p in parts)):
        raise ValueError('非法归档路径：' + name)
    return name


def write_zip(output, payload):
    with zipfile.ZipFile(output, 'x', zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        for name, data in sorted(payload.items()):
            item = zipfile.ZipInfo(valid_path(name), (2026, 10, 22, 0, 0, 0))
            item.compress_type = zipfile.ZIP_DEFLATED
            archive.writestr(item, data)
    with zipfile.ZipFile(output) as archive:
        if set(archive.namelist()) != set(payload):
            raise ValueError('成品成员集合不符')
        for name, data in payload.items():
            if archive.read(name) != data:
                raise ValueError('成品回读不符：' + name)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--release-zip', type=Path, required=True)
    parser.add_argument('--release-receipt', type=Path, required=True)
    parser.add_argument('--output-dir', type=Path, required=True)
    args = parser.parse_args()
    policy = json.loads((SOURCE / 'settings.json').read_text(encoding='utf-8'))
    receipt = json.loads(args.release_receipt.read_text(encoding='utf-8-sig'))
    raw = args.release_zip.read_bytes()
    if (receipt.get('name') != policy['product'] or receipt.get('Candidate') is not False or
            receipt.get('all_members_verified') is not True or len(raw) != receipt['size'] or sha(raw) != receipt['sha256']):
        raise ValueError('输入不是与正式发布回执完全一致的发布包')
    version = receipt['version']
    if not re.fullmatch(r'\d+\.\d+\.\d+', version):
        raise ValueError('交付只接受三段正式功能版本')
    prefix = f'AC7MultiSP-{version}/'
    original = {}
    with zipfile.ZipFile(args.release_zip) as archive:
        names = archive.namelist()
        expected = {prefix + n for n in receipt['files']} | {prefix + '文件清单.json'}
        if len(names) != receipt['member_count'] or len(names) != len(set(n.casefold() for n in names)) or set(names) != expected:
            raise ValueError('原包成员集合不符')
        for name in names:
            rel = valid_path(name[len(prefix):])
            data = archive.read(name)
            if rel != '文件清单.json':
                record = receipt['files'][rel]
                if len(data) != record['size'] or sha(data) != record['sha256']:
                    raise ValueError('原包损坏：' + rel)
            original[rel] = data
    manifest = json.loads(original['文件清单.json'].decode('utf-8-sig'))
    if manifest['files'] != receipt['files'] or manifest['version'] != version:
        raise ValueError('包内清单与发布回执不一致')
    mod = policy['mod_prefix']
    game = {name.removeprefix('GameFiles/'): data for name, data in original.items() if name.startswith('GameFiles/')}
    shared_names = set(policy['ue4ss']['files']) | {'ue4ss/UE4SS-settings.ini', 'ue4ss/Mods/mods.txt'}
    if set(game) - {n for n in game if n.startswith(mod) or n == policy['pak']} != shared_names:
        raise ValueError('共享文件集合发生变化，须先人工核对')
    for name, checksum in policy['ue4ss']['files'].items():
        if sha(game[name]) != checksum:
            raise ValueError('发布包前置与核验策略不符：' + name)
    main_files = {n: data for n, data in game.items() if n not in shared_names}
    provenance = json.loads(main_files[mod + 'install-receipt.json'].decode('utf-8-sig'))
    if provenance['Version'] != version or provenance['Candidate'] is not False:
        raise ValueError('运行回执版本不符')
    for name, checksum in provenance['runtime_files'].items():
        if sha(main_files[mod + name]) != checksum:
            raise ValueError('运行回执摘要不符：' + name)
    if sha(main_files[policy['pak']]) != provenance['mount_assets']['sha256']:
        raise ValueError('挂架 PAK 与发布回执不符')
    for name in main_files:
        if re.search(r'/(profiles[^/]*\.json|ui-state\.json|catalogue[^/]*\.json)$', name, re.I):
            raise ValueError('禁止分发个人数据')
    main_files[mod + 'enabled.txt'] = b''
    for name in ('README_zh-CN.txt', 'README_en.txt', '版本说明.txt'):
        main_files[mod + 'docs/' + name] = original[name]
    main_files[mod + 'vortex-package.json'] = encoded({
        'schema': 1, 'product': policy['product'], 'version': version,
        'distribution': 'universal', 'delivery_version': policy['delivery_version'],
        'source_zip_sha256': receipt['sha256'], 'shared_loader_included': False})
    records = {n: {'size': len(data), 'sha256': sha(data)} for n, data in sorted(main_files.items())}
    package = {'schema': 1, 'product': policy['product'], 'version': version,
               'delivery_version': policy['delivery_version'], 'distribution': 'universal',
               'source_zip_sha256': receipt['sha256'], 'files': records}
    instructions = [{'type': 'setmodtype', 'value': 'dinput'}]
    attributes = {'ac7multispRole': 'main', 'version': version, 'source': 'nexus',
                  'modId': str(policy['nexus_mod_id']), 'downloadGame': policy['game_id']}
    instructions.extend({'type': 'attribute', 'source': 'ac7multisp-' + key, 'key': key, 'value': value}
                        for key, value in attributes.items())
    instructions.extend({'type': 'copy', 'source': n.replace('/', '\\'), 'destination': n.replace('/', '\\')} for n in sorted(main_files))
    readme = f'''AC7MultiSP {version} 通用主包（交付预览 {policy['delivery_version']}）

推荐使用单 EXE 安装，或由 Vortex 管理。
Vortex：先安装 AC7 游戏支持扩展和 AC7MultiSP 安装支持扩展；本 ZIP 安装、启用后，按提示补齐 AC7 专用 UE4SS，再退出游戏部署。
前置来源：{policy['ue4ss']['page']} （已核验 3.0.1.7，文件 8453）。网页 Requirements 提示不代替前置实际安装。

手动安装：完整退出游戏，确认已有正确 UE4SS，把 ZIP 中 ue4ss 和 Game 两个文件夹合并复制到含 Ace7Game.exe 的游戏根目录。
不要复制 ZIP 的外层文件夹；根目录两个 JSON 是管理说明，无需复制。复制前备份原本 Mod；不要覆盖 profiles.json、个人方案或其他 Mod。
enabled.txt 用于独立启用，不需要覆盖 mods.txt；停用时必须同时移走启用标记、本 Mod 程序及同版挂架 PAK。
升级须按新旧清单处理废弃脚本；已有 Vortex 安装请继续使用 Vortex，不直接手动覆盖。配置迁移由游戏中的 Mod 完成。

本包不含共享 UE4SS、微软运行库、其他 Mod 或个人配置。主程序、界面、Lua 和 PAK 沿用正式发布字节，不重新编译。
功能、按键、版本日志和许可见 ue4ss/Mods/DualSP/docs 及 licenses。发布来源的安装说明属于原包，实际交付步骤以本文为准。
这是尚未进行新交付流程实机验收的预览封装，不代表新的功能版本。
'''
    universal = dict(main_files, **{'ac7multisp-package.json': encoded(package),
                                  'vortex_override_instructions.json': encoded(instructions),
                                  '安装说明.txt': readme.encode('utf-8')})
    output = args.output_dir.resolve()
    if output.exists():
        raise ValueError('输出目录已存在，拒绝覆盖')
    output.mkdir(parents=True)
    zip_name = f'AC7MultiSP-{version}-Universal-preview-{policy["delivery_version"]}.zip'
    write_zip(output / zip_name, universal)
    extension = {'index.js': (SOURCE / 'vortex/index.js').read_bytes(),
                 'info.json': (SOURCE / 'vortex/info.json').read_bytes(),
                 'settings.json': encoded(policy),
                 'ue4ss-files.json': (SOURCE / 'vortex/ue4ss-files.json').read_bytes(),
                 'first-install-settings.ini': (ROOT / 'payload/ue4ss/UE4SS-settings.ini').read_bytes(),
                 '使用说明.txt': (ROOT / 'docs/交付与安装设计.md').read_bytes()}
    ext_name = f'AC7MultiSP-Vortex-Support-{policy["delivery_version"]}-preview.zip'
    write_zip(output / ext_name, extension)
    # EXE 使用每个原始文件作为独立资源；只在用户点击导出时组装通用 ZIP。
    resource_dir = output / 'exe-input'
    resource_dir.mkdir()
    resources, embedded = [], {}
    payload = {n: ('mod', data) for n, data in universal.items()}
    payload.update({n: ('loader', game[n]) for n in shared_names if n != 'ue4ss/Mods/mods.txt'})
    for name, (kind, data) in sorted(payload.items()):
        identifier = 'payload.' + sha(name.encode('utf-8'))
        file = resource_dir / identifier
        file.write_bytes(data)
        resources.append((file, identifier))
        embedded[name] = {'kind': kind, 'resource': identifier, 'size': len(data), 'sha256': sha(data)}
    meta = {'schema': 1, 'product': policy['product'], 'version': version,
            'delivery_version': policy['delivery_version'], 'source_zip_sha256': receipt['sha256'],
            'zip_name': zip_name, 'files': embedded, 'policy': policy}
    meta_file = resource_dir / 'bundle.json'
    meta_file.write_bytes(encoded(meta))
    resources.append((meta_file, 'bundle.json'))
    exe = output / f'AC7MultiSP-{version}-Setup-preview-{policy["delivery_version"]}.exe'
    frozen = output / 'setup-source'
    frozen.mkdir()
    source_hashes = {}
    for source in sorted((SOURCE / 'setup').iterdir()):
        if source.suffix not in ('.cs', '.manifest'):
            continue
        data = source.read_bytes()
        (frozen / source.name).write_bytes(data)
        source_hashes[source.name] = sha(data)
    rsp = ['/nologo', '/target:winexe', '/platform:x64', '/optimize+', '/codepage:65001',
           '/reference:System.dll', '/reference:System.Core.dll', '/reference:System.Windows.Forms.dll',
           '/reference:System.Drawing.dll', '/reference:System.Web.Extensions.dll',
           '/reference:System.IO.Compression.dll', '/reference:System.IO.Compression.FileSystem.dll',
           f'/out:"{exe}"', f'/win32manifest:"{frozen / "app.manifest"}"']
    rsp.extend(f'/resource:"{file}",{identifier}' for file, identifier in resources)
    rsp.extend(f'"{file}"' for file in sorted(frozen.glob('*.cs')))
    (output / 'setup-build.rsp').write_text('\n'.join(rsp) + '\n', encoding='utf-8-sig')
    report = {'schema': 1, 'version': version, 'delivery_version': policy['delivery_version'],
              'source_zip_sha256': receipt['sha256'], 'source_members_verified': len(original),
              'source_runtime_files_verified': len(provenance['runtime_files']),
              'universal_zip': zip_name, 'universal_sha256': sha((output / zip_name).read_bytes()),
              'extension_zip': ext_name, 'extension_sha256': sha((output / ext_name).read_bytes()),
              'exe_filename': exe.name, 'installer_sources_sha256': source_hashes,
              'mod_recompiled': False, 'exe_compiled': False, 'test_modules_run': False,
              'game_installed': False, 'vortex_extension_installed': False, 'uploaded': False,
              'archive_members_read_back': True, 'mod_files': records}
    (output / '封装回执.json').write_bytes(encoded(report))
    print(json.dumps({k: v for k, v in report.items() if k != 'mod_files'}, ensure_ascii=False, indent=2))


if __name__ == '__main__':
    main()
