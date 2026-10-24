"""从完整构建生成唯一通用ZIP及同内容的双语EXE输入；不安装、不启动游戏。"""
import argparse
import hashlib
import json
import re
import struct
import zipfile
from datetime import datetime, timezone
from pathlib import Path, PurePosixPath
from release_tools import ROOT, product, read_json, digest, verify_build, source_snapshot
from user_texts import DOCUMENTS as TOPICS, author_links
from author_signing import (Signer, identity, sign_manifest, csharp_trust, TRUST, CONTENT, CONTENT_SIG,
                            valid_path as signed_path, unique)

SOURCE = ROOT / 'src/delivery'
MOD = 'ue4ss/Mods/DualSP/'
PAK = 'Game/Content/Paks/~mods/AC7MultiSP_Mounts_P.pak'


def sha(data):
    return hashlib.sha256(data).hexdigest()


def installer_icon(path):
    """只读取显式指定的本机ICO，冻结原字节；不把游戏图标放入源码仓库。"""
    path = path.resolve(strict=True)
    if not 6 <= path.stat().st_size <= 1048576:
        raise ValueError('安装器图标大小无效')
    raw = path.read_bytes()
    reserved, kind, count = struct.unpack_from('<HHH', raw)
    if reserved or kind != 1 or not 1 <= count <= 64 or len(raw) < 6 + count * 16:
        raise ValueError('安装器图标必须是完整ICO')
    for i in range(count):
        length, offset = struct.unpack_from('<II', raw, 6 + i * 16 + 8)
        if not length or offset < 6 + count * 16 or offset + length > len(raw):
            raise ValueError('安装器ICO图像范围无效')
    return raw, {'source_path': str(path), 'sha256': sha(raw), 'frame_count': count,
                 'frozen_path': 'setup-source/app.ico'}


def encoded(value):
    return (json.dumps(value, ensure_ascii=False, indent=2) + '\n').encode('utf-8')


def valid_path(name):
    signed_path(name)
    parts = name.split('/')
    if ('\\' in name or ':' in name or PurePosixPath(name).is_absolute() or
            any(not p or p in ('.', '..') or p.endswith(('.', ' ')) or
                re.match(r'(?i)^(CON|PRN|AUX|NUL|COM[0-9]|LPT[0-9])(\.|$)', p) for p in parts)):
        raise ValueError('非法归档路径：' + name)
    return name


def write_zip(output, payload):
    with zipfile.ZipFile(output, 'x', zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        for name, data in sorted(payload.items()):
            item = zipfile.ZipInfo(valid_path(name), (2026, 10, 23, 0, 0, 0))
            item.compress_type = zipfile.ZIP_DEFLATED
            archive.writestr(item, data)


def records(payload):
    return {name: {'size': len(data), 'sha256': sha(data)} for name, data in sorted(payload.items())}


def delivery_sources():
    paths = [ROOT / 'src/package_delivery.py', ROOT / 'src/user_texts.py', ROOT / 'src/author_signing.py'] + sorted(SOURCE.rglob('*'))
    paths += sorted((ROOT / 'docs/user').rglob('*'))
    return {p.relative_to(ROOT).as_posix(): digest(p) for p in paths if p.is_file()}


def package(args, signer):
    """私钥操作只通过显式选定的本机密钥句柄完成。"""
    public_source = csharp_trust()
    icon_raw, icon_info = installer_icon(args.icon_source)
    build = args.build_dir.resolve(strict=True)
    output = args.output_dir.resolve()
    if output.exists():
        raise ValueError('输出目录已存在，拒绝覆盖；每次使用独立目录')
    from user_texts import generate, extract_compact_credits
    generate(check=True)
    info = product()
    signed_identity = identity(info['version'], info['candidate'])
    receipt = read_json(build / 'build-receipt.json')
    verify_build(receipt, build)
    policy = read_json(SOURCE / 'settings.json')
    version = info['version']
    release = re.fullmatch(r'(\d+\.\d+\.\d+)(?: alpha([1-9]\d*))?', version)
    if not release:
        raise ValueError('交付版本必须为三段数字，可附加自测标识 alphaN')
    alpha = int(release[2] or 0)
    if alpha and not info['candidate']:
        raise ValueError('alpha自测版必须标记candidate，不能冒充正式发布')
    if any(int(part) > 65534 for part in release[1].split('.')) or alpha > 65534:
        raise ValueError('交付版本超出Windows程序集版本范围')
    assembly_version = release[1] + '.' + str(alpha)
    # 兼容旧清单字段，但唯一版本来源仍为产品版本。
    policy['delivery_version'] = version
    frozen = delivery_sources()
    runtime = {'Scripts/' + name: (ROOT / 'dist/DualSP/Scripts' / name).read_bytes() for name in info['scripts']}
    runtime.update({'bin/DualSP.dll': (build / 'DualSP.dll').read_bytes(),
                    'bin/DualSPUI.dll': (build / 'DualSPUI.dll').read_bytes(),
                    'VERSION': (ROOT / 'VERSION').read_bytes()})
    main_files = {MOD + name: data for name, data in runtime.items()}
    main_files[PAK] = (build / receipt['mount_assets']['file']).read_bytes()
    main_files[MOD + 'enabled.txt'] = b''
    audio = args.audio_source.resolve(strict=True)
    if digest(audio) != '278b8f8815823522ed5f121711a1a12eda2ae8e89991f8522d1fff22832d9640':
        raise ValueError('提示音不符合原验收来源；不得用个人替换音频分发')
    main_files[MOD + 'Audio/SP2.wav'] = audio.read_bytes()
    for p in sorted((ROOT / 'payload/ue4ss/Mods/DualSP/licenses').glob('*.txt')):
        main_files[MOD + 'licenses/' + p.name] = p.read_bytes()
    documents = {}
    readme_documents = {}
    for topic, filenames in TOPICS.items():
        for language, filename in zip(('zh-CN', 'en'), filenames):
            path = ROOT / 'docs/user' / filename
            raw = path.read_bytes()
            raw.decode('utf-8-sig')
            documents[f'installer/docs/{language}/{topic}.txt'] = raw
            if topic == 'credits':
                documents[f'installer/credits-compact/{language}.json'] = encoded(extract_compact_credits(raw.decode('utf-8-sig'), language))
            main_files[MOD + 'docs/' + path.name] = raw
            folder = 'ReadMe_zhCN请读我' if language == 'zh-CN' else 'ReadMe_EN'
            readme_documents[folder + '/' + path.name] = raw
    main_files[MOD + 'vortex-package.json'] = encoded({
        'schema': 1, 'product': info['name'], 'version': version,
        'distribution': 'universal', 'delivery_version': policy['delivery_version'],
        'candidate': info['candidate'], 'shared_loader_included': False})
    main_files[MOD + 'install-receipt.json'] = encoded({
        'Name': info['name'], 'Version': version, 'Candidate': info['candidate'],
        'Distribution': 'universal', 'name': info['name'], 'candidate': version,
        'runtime_files': {n: sha(d) for n, d in sorted(runtime.items())},
        'mount_assets': receipt['mount_assets'], 'build_receipt_sha256': digest(build / 'build-receipt.json'),
        'gameplay_validated': False, 'game_installed': False})
    for name in main_files:
        if re.search(r'/(profiles[^/]*\.json|ui-state\.json|catalogue[^/]*\.json|.*\.log|.*\.sav)$', name, re.I):
            raise ValueError('禁止分发个人文件：' + name)
    content, content_signature = sign_manifest(dict(signed_identity, purpose='content', files=records(main_files)), signer)
    main_files[CONTENT], main_files[CONTENT_SIG] = content, content_signature
    instructions = [{'type': 'setmodtype', 'value': 'dinput'}]
    attributes = {'version': version, 'source': 'nexus', 'modId': str(policy['nexus_mod_id']), 'downloadGame': policy['game_id']}
    instructions.extend({'type': 'attribute', 'source': 'ac7multisp-' + k, 'key': k, 'value': v} for k, v in attributes.items())
    instructions.extend({'type': 'copy', 'source': n.replace('/', '\\'), 'destination': n.replace('/', '\\')} for n in sorted(main_files))
    universal = dict(main_files)
    universal['vortex_override_instructions.json'] = encoded(instructions)
    universal['ac7multisp-package.json'] = encoded({
        'schema': 1, 'product': info['name'], 'version': version, 'candidate': info['candidate'],
        'distribution': 'universal', 'delivery_version': policy['delivery_version'],
        'files': records(main_files), 'build_receipt_sha256': digest(build / 'build-receipt.json')})
    # 两种语言各四份原字节副本；只供阅读，不加入Vortex的游戏部署映射。
    universal.update(readme_documents)
    output.mkdir(parents=True)
    zip_name = f'AC7MultiSP-{version}-Universal.zip'
    archive = output / zip_name
    write_zip(archive, universal)
    with zipfile.ZipFile(archive) as z:
        if len(z.namelist()) != len(universal) or set(z.namelist()) != set(universal):
            raise ValueError('通用ZIP成员不符')
        for name, raw in universal.items():
            if z.read(name) != raw:
                raise ValueError('通用ZIP回读字节不符：' + name)
    loader = {'dwmapi.dll': ROOT / 'payload/dwmapi.dll',
              'ue4ss/UE4SS.dll': ROOT / 'payload/ue4ss/UE4SS.dll',
              'ue4ss/UE4SS-settings.ini': ROOT / 'payload/ue4ss/UE4SS-settings.ini'}
    for name, checksum in policy['ue4ss']['files'].items():
        if digest(loader[name]) != checksum:
            raise ValueError('AC7专用UE4SS与固定来源不符：' + name)
    payload = {n: ('mod', data) for n, data in main_files.items()}
    payload.update({n: ('loader', p.read_bytes()) for n, p in loader.items()})
    payload.update({n: ('document', raw) for n, raw in documents.items()})
    payload['installer/universal.zip'] = ('archive', archive.read_bytes())
    installer_catalogs = {}
    for language in ('zh-CN', 'en'):
        raw = (ROOT / 'docs/user/interface' / ('installer.' + language + '.json')).read_bytes()
        catalog = json.loads(raw.decode('utf-8-sig'), object_pairs_hook=unique)
        if not isinstance(catalog, dict) or not catalog or any(not isinstance(v, str) for v in catalog.values()) or len(raw) > 65536:
            raise ValueError('安装器词条格式或大小无效')
        installer_catalogs[language] = catalog
        payload['installer/ui/' + language + '.json'] = ('interface', raw)
    if set(installer_catalogs['zh-CN']) != set(installer_catalogs['en']):
        raise ValueError('安装器双语词条缺失')
    resource_dir = output / 'exe-input'
    resource_dir.mkdir()
    resources = []
    embedded = {}
    for name, (kind, raw) in sorted(payload.items()):
        identifier = 'payload.' + sha(name.encode('utf-8'))
        file = resource_dir / identifier
        file.write_bytes(raw)
        resources.append((file, identifier))
        embedded[name] = {'kind': kind, 'resource': identifier, 'size': len(raw), 'sha256': sha(raw)}
    meta = {**signed_identity, 'purpose': 'bundle', 'candidate': info['candidate'],
            'delivery_version': policy['delivery_version'], 'zip_name': zip_name,
            'universal_sha256': digest(archive), 'files': embedded, 'policy': policy}
    meta_raw, meta_signature = sign_manifest(meta, signer)
    meta_file = resource_dir / 'bundle.json'
    meta_file.write_bytes(meta_raw)
    resources.append((meta_file, 'bundle.json'))
    signature_file = resource_dir / 'bundle.sig.json'
    signature_file.write_bytes(meta_signature)
    resources.append((signature_file, 'bundle.sig.json'))
    exe = output / 'AC7MultiSP_Installer.exe'
    frozen_dir = output / 'setup-source'
    frozen_dir.mkdir()
    source_hashes = {}
    (frozen_dir / 'app.ico').write_bytes(icon_raw)
    source_hashes['app.ico'] = sha(icon_raw)
    for path in sorted((SOURCE / 'setup').iterdir()):
        if path.suffix not in ('.cs', '.manifest'):
            continue
        raw = path.read_bytes()
        if path.name == 'app.manifest':
            template = raw.decode('utf-8-sig')
            if template.count('@PRODUCT_ASSEMBLY_VERSION@') != 1:
                raise ValueError('安装器manifest缺少唯一产品版本占位符')
            raw = template.replace('@PRODUCT_ASSEMBLY_VERSION@', assembly_version).encode('utf-8')
        (frozen_dir / path.name).write_bytes(raw)
        source_hashes[path.name] = sha(raw)
    # 版本属性随同冻结输入生成，源码不保留另一套安装器版本。
    assembly_info = ('using System.Reflection;\n'
                     f'[assembly: AssemblyVersion("{assembly_version}")]\n'
                     f'[assembly: AssemblyFileVersion("{assembly_version}")]\n'
                     f'[assembly: AssemblyInformationalVersion("{version}")]\n').encode('utf-8')
    (frozen_dir / 'ProductVersion.cs').write_bytes(assembly_info)
    source_hashes['ProductVersion.cs'] = sha(assembly_info)
    (frozen_dir / 'TrustedAuthorKeys.cs').write_bytes(public_source)
    source_hashes['TrustedAuthorKeys.cs'] = sha(public_source)
    # C++与安装器的作者入口同源，网址不从下载清单接受。
    links_source = ('// 自动生成；唯一来源为interface/author-links.json。\n'
                    'namespace AC7MultiSP.Setup { internal static class AuthorLinks {\n' +
                    '\n'.join('internal const string ' + key.title() + ' = ' + json.dumps(url) + ';'
                              for key, url in author_links().items()) + '\n} }\n').encode('utf-8')
    (frozen_dir / 'AuthorLinks.cs').write_bytes(links_source)
    source_hashes['AuthorLinks.cs'] = sha(links_source)
    # 首次验签失败不能依赖未验证的UI资源；从同一词条生成最小双语提示。
    bootstrap = ['// 自动生成；唯一来源为installer双语JSON，不手工维护。',
                 'namespace AC7MultiSP.Setup { internal static class SignatureBootstrap {']
    for suffix, language in (('Zh', 'zh-CN'), ('En', 'en')):
        for field, key in (('Title', 'app.title'), ('Failure', 'error.authorSignature')):
            bootstrap.append('internal const string ' + field + suffix + ' = ' +
                             json.dumps(installer_catalogs[language][key], ensure_ascii=True) + ';')
    bootstrap_raw = ('\n'.join(bootstrap + ['} }', ''])).encode('utf-8')
    (frozen_dir / 'SignatureBootstrap.cs').write_bytes(bootstrap_raw)
    source_hashes['SignatureBootstrap.cs'] = sha(bootstrap_raw)
    rsp = ['/nologo', '/target:winexe', '/platform:x64', '/optimize+', '/codepage:65001',
           '/reference:System.dll', '/reference:System.Core.dll', '/reference:System.Windows.Forms.dll',
           '/reference:System.Drawing.dll', '/reference:System.Web.Extensions.dll',
           '/reference:System.IO.Compression.dll', '/reference:System.IO.Compression.FileSystem.dll',
           f'/out:"{exe}"', f'/win32manifest:"{frozen_dir / "app.manifest"}"',
           f'/win32icon:"{frozen_dir / "app.ico"}"']
    rsp.extend(f'/resource:"{file}",{identifier}' for file, identifier in resources)
    rsp.extend(f'"{file}"' for file in sorted(frozen_dir.glob('*.cs')))
    (output / 'setup-build.rsp').write_text('\n'.join(rsp) + '\n', encoding='utf-8-sig')
    if frozen != delivery_sources() or receipt['source_files'] != source_snapshot():
        raise ValueError('封装期间来源改变，不能交付')
    if digest(args.icon_source) != icon_info['sha256']:
        raise ValueError('封装期间图标来源改变，不能交付')
    signing = dict(signed_identity, key_id=signer.key_id, registry_sha256=digest(TRUST),
                   content_sha256=sha(content), bundle_sha256=sha(meta_raw))
    report = {'schema': 2, 'version': version, 'candidate': info['candidate'], 'author_signing': signing,
              'delivery_version': policy['delivery_version'], 'created_at': datetime.now(timezone.utc).isoformat(),
              'universal_zip': zip_name, 'universal_sha256': digest(archive),
              'universal_size': archive.stat().st_size, 'universal_member_count': len(universal),
              'exe_filename': exe.name, 'installer_icon': icon_info, 'installer_sources_sha256': source_hashes,
              'installer_inputs_sha256': {
                  p.relative_to(output).as_posix(): digest(p)
                  for p in sorted([output / 'setup-build.rsp', frozen_dir / 'app.ico', *resource_dir.iterdir()]) if p.is_file()},
              'build_receipt_sha256': digest(build / 'build-receipt.json'), 'source_files': frozen,
              'runtime_files': {n: sha(d) for n, d in runtime.items()}, 'mount_assets': receipt['mount_assets'],
              'game_files': records(main_files), 'documents': records(documents),
              'game_dlls_recompiled': receipt['core_recompiled'], 'exe_compiled': False,
              'archive_members_read_back': True, 'separate_vortex_extension': False,
              'tests_run': False, 'game_installed': False, 'game_started': False, 'uploaded': False}
    (output / '封装回执.json').write_bytes(encoded(report))
    print(json.dumps({k: report[k] for k in ('version', 'candidate', 'universal_zip', 'universal_sha256', 'universal_member_count', 'exe_filename')}, ensure_ascii=False))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-dir', type=Path, required=True)
    parser.add_argument('--output-dir', type=Path, required=True)
    parser.add_argument('--audio-source', type=Path, default=ROOT / 'dist/DualSP/Audio/SP2.wav')
    parser.add_argument('--icon-source', type=Path, required=True, help='作者显式指定的本机游戏ICO；只冻结到本轮Temp，不入Git')
    parser.add_argument('--signing-key-name', help='可省略以按公钥查找唯一的本机CNG密钥；不接受私钥文件')
    parser.add_argument('--signing-provider', default='Microsoft Software Key Storage Provider')
    args = parser.parse_args()
    with Signer(args.signing_key_name, args.signing_provider) as signer:
        package(args, signer)


if __name__ == '__main__':
    main()
