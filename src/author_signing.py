"""作者离线签名及只读核验；只调用Windows密码API，不生成或导出私钥。"""
import argparse
import base64
import ctypes as ct
from ctypes import wintypes as wt
import hashlib
import json
import os
from pathlib import Path
import re
import struct
import subprocess
import uuid

ROOT = Path(__file__).resolve().parents[1]
TRUST = ROOT / 'src/delivery/trusted-author-keys.json'
ALGORITHM = 'RSA-PSS-SHA256'
DOMAIN = b'AC7MultiSP.signature.v1\0'
LIMIT = 65536
PROVIDER = 'Microsoft Software Key Storage Provider'
CONTENT = 'ue4ss/Mods/DualSP/author-content.json'
CONTENT_SIG = 'ue4ss/Mods/DualSP/author-content.sig.json'
RELEASE = 'AC7MultiSP-release.json'
RELEASE_SIG = 'AC7MultiSP-release.sig.json'


def unique(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError('签名资料包含重复键')
        result[key] = value
    return result


def decode(raw, maximum=LIMIT):
    if not 0 < len(raw) <= maximum:
        raise ValueError('签名资料大小无效')
    return json.loads(raw.decode('utf-8'), object_pairs_hook=unique,
                      parse_constant=lambda _: (_ for _ in ()).throw(ValueError('非法JSON常量')))


def encode(value):
    return (json.dumps(value, ensure_ascii=False, sort_keys=True, indent=2) + '\n').encode('utf-8')


def read_limited(path, maximum=LIMIT):
    with Path(path).open('rb') as stream:
        raw = stream.read(maximum + 1)
    if not 0 < len(raw) <= maximum:
        raise ValueError('签名文件大小无效')
    return raw


def valid_path(name):
    if not isinstance(name, str) or not 0 < len(name) <= 240:
        raise ValueError('签名成员路径无效')
    if any(ord(c) < 32 or ord(c) > 0xffff or c in '\\:*?"<>|\x7f' for c in name):
        raise ValueError('签名成员路径含非法字符')
    for part in name.split('/'):
        if (not part or part in ('.', '..') or part.endswith(('.', ' ')) or
                re.match(r'(?i)^(CON|PRN|AUX|NUL|COM[0-9]|LPT[0-9])(?:\.|$)', part)):
            raise ValueError('签名成员路径不安全')
    return name


def public_blob(raw):
    if len(raw) < 24:
        raise ValueError('公钥格式无效')
    magic, bits, exponent, modulus, prime1, prime2 = struct.unpack_from('<6I', raw)
    if (magic != 0x31415352 or bits != 3072 or exponent != 3 or modulus != 384 or
            prime1 or prime2 or len(raw) != 24 + exponent + modulus or raw[24:27] != b'\x01\x00\x01' or
            not raw[27] & 0x80 or not raw[-1] & 1):
        raise ValueError('只接受RSA3072、65537指数的公钥，不接受私钥BLOB')
    return raw


def trusted_keys():
    registry = decode(read_limited(TRUST))
    if (set(registry) != {'schema', 'algorithm', 'keys'} or type(registry['schema']) is not int or
            registry['schema'] != 1 or registry['algorithm'] != ALGORITHM or
            not isinstance(registry['keys'], list) or not 1 <= len(registry['keys']) <= 8):
        raise ValueError('尚未登记作者公钥，禁止制作无签名新包')
    keys = {}
    for entry in registry['keys']:
        if not isinstance(entry, dict) or set(entry) != {'key_id', 'public_blob'}:
            raise ValueError('公钥登记字段无效')
        blob = public_blob(base64.b64decode(entry['public_blob'], validate=True))
        identity = hashlib.sha256(blob).hexdigest()
        if entry['key_id'] != identity or identity in keys:
            raise ValueError('公钥指纹不匹配或重复')
        keys[identity] = blob
    return keys


class Padding(ct.Structure):
    _fields_ = [('algorithm', wt.LPCWSTR), ('salt_length', wt.ULONG)]


class KeyName(ct.Structure):
    _fields_ = [('name', wt.LPWSTR), ('algorithm', wt.LPWSTR), ('legacy_spec', wt.DWORD), ('flags', wt.DWORD)]


def api():
    if os.name != 'nt':
        raise RuntimeError('签名和核验需要Windows；未自动安装其他密码库')
    b = ct.WinDLL('bcrypt.dll')
    n = ct.WinDLL('ncrypt.dll')
    h, p, u = ct.c_void_p, ct.c_void_p, wt.DWORD
    declarations = [
        (b.BCryptOpenAlgorithmProvider, [ct.POINTER(h), wt.LPCWSTR, wt.LPCWSTR, u]),
        (b.BCryptImportKeyPair, [h, h, wt.LPCWSTR, ct.POINTER(h), p, u, u]),
        (b.BCryptVerifySignature, [h, p, p, u, p, u, u]),
        (b.BCryptDestroyKey, [h]), (b.BCryptCloseAlgorithmProvider, [h, u]),
        (n.NCryptOpenStorageProvider, [ct.POINTER(h), wt.LPCWSTR, u]),
        (n.NCryptOpenKey, [h, ct.POINTER(h), wt.LPCWSTR, u, u]),
        (n.NCryptEnumKeys, [h, wt.LPCWSTR, ct.POINTER(p), ct.POINTER(p), u]),
        (n.NCryptExportKey, [h, h, wt.LPCWSTR, p, p, u, ct.POINTER(u), u]),
        (n.NCryptSignHash, [h, p, p, u, p, u, ct.POINTER(u), u]),
        (n.NCryptFreeObject, [h]), (n.NCryptFreeBuffer, [p])]
    for function, arguments in declarations:
        function.argtypes, function.restype = arguments, ct.c_long
    return b, n


def success(status):
    if status != 0:
        # 不将密钥路径、名称或密码学内部数据拼入错误。
        raise RuntimeError(f'Windows签名API未完成，状态0x{status & 0xffffffff:08X}')


def export_public(n, key):
    length = wt.DWORD()
    success(n.NCryptExportKey(key, None, 'RSAPUBLICBLOB', None, None, 0, ct.byref(length), 0))
    if length.value > 1024:
        raise ValueError('公钥大小异常')
    output = ct.create_string_buffer(length.value)
    success(n.NCryptExportKey(key, None, 'RSAPUBLICBLOB', None, output, len(output), ct.byref(length), 0))
    return public_blob(output.raw[:length.value])


def discover_key(n, provider):
    """只打开本项目名称前缀的当前用户密钥，按已登记公钥选择，绝不自动创建。"""
    state, item = ct.c_void_p(), ct.c_void_p()
    found, keys = [], trusted_keys()
    try:
        while True:
            status = n.NCryptEnumKeys(provider, None, ct.byref(item), ct.byref(state), 0)
            if status & 0xffffffff == 0x8009002a:
                break
            success(status)
            try:
                name = ct.cast(item, ct.POINTER(KeyName)).contents.name
                if not name.startswith('AC7MultiSP-Author-'):
                    continue
                key = ct.c_void_p()
                try:
                    success(n.NCryptOpenKey(provider, ct.byref(key), name, 0, 0))
                    blob = export_public(n, key)
                    if keys.get(hashlib.sha256(blob).hexdigest()) == blob:
                        found.append(name)
                finally:
                    if key.value:
                        n.NCryptFreeObject(key)
            finally:
                if item.value:
                    n.NCryptFreeBuffer(item); item = ct.c_void_p()
    finally:
        if state.value:
            n.NCryptFreeBuffer(state)
    if len(found) != 1:
        raise ValueError('本机匹配的作者密钥不是唯一一份，请显式指定--signing-key-name；不自动创建或重试')
    return found[0]


class Signer:
    def __init__(self, key_name=None, provider=PROVIDER):
        # 公钥未启用时，在打开任何本机密钥前停止。
        keys = trusted_keys()
        self.provider, self.key = ct.c_void_p(), ct.c_void_p()
        self.b, self.n = api()
        try:
            success(self.n.NCryptOpenStorageProvider(ct.byref(self.provider), provider, 0))
            key_name = key_name or discover_key(self.n, self.provider)
            success(self.n.NCryptOpenKey(self.provider, ct.byref(self.key), key_name, 0, 0))
            self.blob = export_public(self.n, self.key)
            self.key_id = hashlib.sha256(self.blob).hexdigest()
            if keys.get(self.key_id) != self.blob:
                raise ValueError('本机签名密钥不对应已登记公钥，停止制作')
        except Exception:
            self.close()
            raise

    def close(self):
        if self.key.value:
            self.n.NCryptFreeObject(self.key); self.key = ct.c_void_p()
        if self.provider.value:
            self.n.NCryptFreeObject(self.provider); self.provider = ct.c_void_p()

    def __enter__(self):
        return self

    def __exit__(self, *_):
        self.close()

    def sign(self, raw):
        digest = ct.create_string_buffer(hashlib.sha256(DOMAIN + raw).digest())
        padding, length = Padding('SHA256', 32), wt.DWORD()
        output = ct.create_string_buffer(384)
        success(self.n.NCryptSignHash(self.key, ct.byref(padding), digest, 32, output, 384, ct.byref(length), 8))
        if length.value != 384:
            raise ValueError('签名长度异常')
        return encode({'schema': 1, 'algorithm': ALGORITHM, 'key_id': self.key_id,
                       'signature': base64.b64encode(output.raw).decode('ascii')})


def validate_manifest(value, purpose, version=None):
    common = {'schema', 'product', 'purpose', 'version', 'channel', 'release_id', 'source_commit', 'files'}
    extra = {'candidate', 'delivery_version', 'zip_name', 'universal_sha256', 'policy'} if purpose == 'bundle' else set()
    if (not isinstance(value, dict) or set(value) != common | extra or type(value['schema']) is not int or
            value['schema'] != 1 or value['product'] != 'AC7MultiSP' or value['purpose'] != purpose or
            purpose not in ('content', 'bundle', 'release') or
            not isinstance(value['version'], str) or not re.fullmatch(r'\d+\.\d+\.\d+(?: alpha[1-9]\d*| beta)?', value['version']) or
            (version is not None and value['version'] != version) or value['channel'] not in ('stable', 'candidate') or
            ((' alpha' in value['version'] or value['version'].endswith(' beta')) and value['channel'] != 'candidate') or
            not isinstance(value['release_id'], str) or not re.fullmatch(r'[0-9a-f]{32}', value['release_id']) or
            not isinstance(value['source_commit'], str) or not re.fullmatch(r'[0-9a-f]{40}', value['source_commit'])):
        raise ValueError('签名清单身份、版本或用途无效')
    files, folded, total = value['files'], set(), 0
    if not isinstance(files, dict) or not 1 <= len(files) <= 256:
        raise ValueError('签名成员数量无效')
    resources = set()
    for name, record in files.items():
        valid_path(name)
        lower = name.upper()
        if lower in folded:
            raise ValueError('签名成员大小写冲突')
        folded.add(lower)
        fields = {'size', 'sha256'} | ({'kind', 'resource'} if purpose == 'bundle' else set())
        if (not isinstance(record, dict) or set(record) != fields or type(record['size']) is not int or
                not 0 <= record['size'] <= 268435456 or not isinstance(record['sha256'], str) or
                not re.fullmatch(r'[0-9a-f]{64}', record['sha256'])):
            raise ValueError('签名成员字段无效')
        total += record['size']
        if purpose == 'bundle':
            if (record['kind'] not in ('mod', 'loader', 'document', 'archive', 'interface') or
                    not re.fullmatch(r'payload\.[0-9a-f]{64}', record['resource']) or record['resource'] in resources):
                raise ValueError('安装器签名资源标识无效')
            resources.add(record['resource'])
    if total > 1073741824:
        raise ValueError('签名成员总大小超限')
    if purpose == 'bundle':
        if (type(value['candidate']) is not bool or value['channel'] != ('candidate' if value['candidate'] else 'stable') or
                value['delivery_version'] != value['version'] or
                value['zip_name'] != f"AC7MultiSP-{value['version']}-Universal.zip" or
                not re.fullmatch(r'[0-9a-f]{64}', value['universal_sha256']) or not isinstance(value['policy'], dict)):
            raise ValueError('安装器签名元数据无效')
    return value


def verify_manifest(raw, signature, purpose, version=None):
    if not 0 < len(raw) <= LIMIT:
        raise ValueError('清单大小超限')
    envelope = decode(signature, 2048)
    if (not isinstance(envelope, dict) or set(envelope) != {'schema', 'algorithm', 'key_id', 'signature'} or
            type(envelope['schema']) is not int or envelope['schema'] != 1 or envelope['algorithm'] != ALGORITHM):
        raise ValueError('签名格式无效')
    keys = trusted_keys()
    blob = keys.get(envelope['key_id'])
    if blob is None:
        raise ValueError('签名公钥未登记')
    signature_bytes = base64.b64decode(envelope['signature'], validate=True)
    if len(signature_bytes) != 384:
        raise ValueError('签名长度无效')
    b, _ = api()
    algorithm, key = ct.c_void_p(), ct.c_void_p()
    try:
        success(b.BCryptOpenAlgorithmProvider(ct.byref(algorithm), 'RSA', None, 0))
        buffer = ct.create_string_buffer(blob)
        success(b.BCryptImportKeyPair(algorithm, None, 'RSAPUBLICBLOB', ct.byref(key), buffer, len(blob), 0))
        padding = Padding('SHA256', 32)
        hashed = ct.create_string_buffer(hashlib.sha256(DOMAIN + raw).digest())
        signed = ct.create_string_buffer(signature_bytes)
        success(b.BCryptVerifySignature(key, ct.byref(padding), hashed, 32, signed, 384, 8))
    finally:
        if key.value:
            b.BCryptDestroyKey(key)
        if algorithm.value:
            b.BCryptCloseAlgorithmProvider(algorithm, 0)
    return validate_manifest(decode(raw), purpose, version)


def identity(version, candidate, release_id=None, source_commit=None):
    return {'schema': 1, 'product': 'AC7MultiSP', 'version': version,
            'channel': 'candidate' if candidate else 'stable', 'release_id': release_id or uuid.uuid4().hex,
            'source_commit': source_commit or subprocess.check_output(
                ['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip()}


def sign_manifest(value, signer):
    validate_manifest(value, value['purpose'])
    raw = encode(value)
    if len(raw) > LIMIT:
        raise ValueError('待签名清单超过大小限制，未执行私钥签名')
    signature = signer.sign(raw)
    verify_manifest(raw, signature, value['purpose'], value['version'])
    return raw, signature


def csharp_trust():
    lines = ['// 自动生成；唯一来源为src/delivery/trusted-author-keys.json。',
             'namespace AC7MultiSP.Setup { internal static class TrustedAuthorKeys {',
             'internal static readonly System.Collections.Generic.Dictionary<string, byte[]> Values =',
             'new System.Collections.Generic.Dictionary<string, byte[]>(System.StringComparer.Ordinal) {']
    for key_id, blob in sorted(trusted_keys().items()):
        lines.append('{"' + key_id + '", System.Convert.FromBase64String("' + base64.b64encode(blob).decode('ascii') + '")},')
    return ('\n'.join(lines + ['};', '} }', ''])).encode('utf-8')


def verify_release(folder):
    folder = Path(folder).resolve(strict=True)
    manifest = verify_manifest(read_limited(folder / RELEASE), read_limited(folder / RELEASE_SIG, 2048), 'release')
    for name, entry in manifest['files'].items():
        if '/' in name:
            raise ValueError('发布清单只允许同目录文件')
        path = folder / name
        if path.is_symlink() or path.is_junction() or path.stat().st_size != entry['size']:
            raise ValueError('发布文件大小或路径不符')
        with path.open('rb') as stream:
            if hashlib.file_digest(stream, 'sha256').hexdigest() != entry['sha256']:
                raise ValueError('发布文件内容不符')
    return manifest


def verify_package_inputs(folder):
    """编译前核对最终冻结资源与签名，回执不是新的信任根。"""
    folder = Path(folder).resolve(strict=True)
    manifest = verify_manifest(read_limited(folder / 'exe-input/bundle.json'),
                               read_limited(folder / 'exe-input/bundle.sig.json', 2048), 'bundle')
    for name, record in manifest['files'].items():
        path = folder / 'exe-input' / record['resource']
        if path.stat().st_size != record['size']:
            raise ValueError('冻结资源大小不同')
        with path.open('rb') as stream:
            if hashlib.file_digest(stream, 'sha256').hexdigest() != record['sha256']:
                raise ValueError('冻结资源摘要不同')
    with (folder / manifest['zip_name']).open('rb') as stream:
        if hashlib.file_digest(stream, 'sha256').hexdigest() != manifest['universal_sha256']:
            raise ValueError('独立ZIP与签名的内嵌ZIP不同')
    return manifest


def seal_release(folder, signer):
    folder = Path(folder).resolve(strict=True)
    manifest = verify_package_inputs(folder)
    if (folder / RELEASE).exists() or (folder / RELEASE_SIG).exists():
        raise ValueError('发布签名原件已存在，拒绝覆盖')
    receipt = decode(read_limited(folder / 'EXE编译回执.json'))
    exe, archive = folder / 'AC7MultiSP_Installer.exe', folder / manifest['zip_name']
    files = {}
    for path in (exe, archive):
        with path.open('rb') as stream:
            checksum = hashlib.file_digest(stream, 'sha256').hexdigest()
        files[path.name] = {'size': path.stat().st_size, 'sha256': checksum}
    if (files[exe.name]['sha256'] != receipt['sha256'] or not receipt.get('exe_compiled') or
            files[archive.name]['sha256'] != manifest['universal_sha256']):
        raise ValueError('最终EXE/ZIP不符，不能签名发布')
    value = {key: manifest[key] for key in ('schema', 'product', 'version', 'channel', 'release_id', 'source_commit')}
    raw, signed = sign_manifest(dict(value, purpose='release', files=files), signer)
    with (folder / RELEASE).open('xb') as stream:
        stream.write(raw)
    with (folder / RELEASE_SIG).open('xb') as stream:
        stream.write(signed)
    verify_release(folder)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('command', choices=['verify-release', 'verify-inputs', 'seal-release'])
    parser.add_argument('--files-dir', type=Path, required=True)
    parser.add_argument('--signing-key-name')
    parser.add_argument('--signing-provider', default=PROVIDER)
    args = parser.parse_args()
    try:
        if args.command == 'seal-release':
            with Signer(args.signing_key_name, args.signing_provider) as signer:
                seal_release(args.files_dir, signer)
            print('已生成最终发布签名；未上传。')
        else:
            manifest = verify_release(args.files_dir) if args.command == 'verify-release' else verify_package_inputs(args.files_dir)
            print(f"签名及文件一致 / Signature and files match: {manifest['version']}")
    except Exception:
        parser.exit(1, '核验未完成，请保留文件并从作者渠道核对。 / Verification failed; keep the files and check the author’s source.\n')


if __name__ == '__main__':
    main()
