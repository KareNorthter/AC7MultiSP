"""仅平移MiG-31B后腹SAAM借用的两个Socket；原生几何、旋转和其他挂点保持。"""
import argparse
import copy
import hashlib
import json
from pathlib import Path

from split_static_mount_asset import load_mesh, run_converter, encode_ac7_pair
from inspect_mount_assets import decode_copy

ROOT = Path(__file__).resolve().parents[2]
NAME = 'MSP_m31b_saam_rear_lower_r1'
DELTA = {'X': -6.0, 'Y': 0.0, 'Z': -2.0}


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main(args):
    manifest = json.loads(args.manifest.read_text(encoding='utf-8-sig'))
    entry = next(e for e in manifest if e['asset'].endswith('/m31b_SWP1_SM.uasset') and
                 e['pak'] == 'pakchunk0-WindowsNoEditor.pak')
    path = Path(entry['json'])
    original_encoded = path.parent.parent/path.with_suffix('.uasset').name
    if (digest(original_encoded) != '371b5cec68a69dd52aa6672bb3a5aae078ce5db83fb8d5bd0d35afb36f4f524b' or
            digest(path.with_suffix('.uasset')) != '3f0482c61ea30e9d1d9079cbe8a1cf25d1d822c24be08c0c79ebfe54011e329a'):
        raise ValueError('原生来源与原研究指纹不同，停止调整')
    output = args.output.resolve()
    if not output.is_relative_to(ROOT/'Temp') or output.exists() or args.evidence.exists():
        raise ValueError('输出须为本工作树Temp内新目录，不能覆盖旧候选或回执')
    document, export, raw, mesh = load_mesh(path)
    candidate = copy.deepcopy(document)
    changes = []
    for item in candidate['Exports']:
        fields = {p['Name']: p for p in item.get('Data', [])}
        socket = fields.get('SocketName', {}).get('Value')
        if socket not in ('sp13', 'sp14'):
            continue
        location = fields['RelativeLocation']['Value'][0]['Value']
        before = {k: float(location[k]) for k in DELTA}
        for key, offset in DELTA.items():
            location[key] = before[key]+offset
        changes.append({'socket': socket, 'before': before, 'after': {k: location[k] for k in DELTA}})
    if sorted(x['socket'] for x in changes) != ['sp13', 'sp14']:
        raise ValueError('后腹两个Socket不完整或重名')
    source_name = export['ObjectName']
    own_paths = [s for s in candidate['NameMap'] if s.endswith('/'+source_name)]
    if len(own_paths) != 1:
        raise ValueError('原生资源包身份不唯一')
    package = '/Game/AC7MultiSP/Research/'+NAME
    candidate['NameMap'] = [NAME if s == source_name else package if s == own_paths[0] else s
                            for s in candidate['NameMap']]
    next(e for e in candidate['Exports'] if e['ObjectName'] == source_name)['ObjectName'] = NAME
    # 逆向撤回允许的改动后，必须逐项等于原文档，防止带入其他变更。
    reverted = copy.deepcopy(candidate)
    reverted['NameMap'] = document['NameMap']
    for before, after in zip(document['Exports'], reverted['Exports']):
        if before['ObjectName'] == source_name:
            after['ObjectName'] = source_name
        props = {p['Name']: p for p in after.get('Data', [])}
        if props.get('SocketName', {}).get('Value') in ('sp13', 'sp14'):
            after['Data'] = copy.deepcopy(before['Data'])
    if reverted != document:
        raise ValueError('候选改变了允许范围之外的属性')
    output.mkdir(parents=True)
    converted = output/(NAME+'.json')
    converted.write_text(json.dumps(candidate, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
    asset = output/(NAME+'.uasset')
    run_converter(args.tools_root, ['fromjson', converted, asset], asset)
    reread = output/'roundtrip'/(NAME+'.json')
    reread.parent.mkdir()
    run_converter(args.tools_root, ['tojson', asset, reread, 'VER_UE4_18'], reread)
    actual, _, actual_raw, actual_mesh = load_mesh(reread)
    if actual['NameMap'] != candidate['NameMap'] or actual['Imports'] != candidate['Imports'] or actual_raw != raw:
        raise ValueError('资源改名/回读改变了导入或原生几何')
    for before, after in zip(candidate['Exports'], actual['Exports'], strict=True):
        for key in ('ObjectName', 'Data', 'Extras', 'OuterIndex', 'ClassIndex', 'SuperIndex', 'TemplateIndex'):
            if before.get(key) != after.get(key):
                raise ValueError('资源回读改变了候选导出：'+key)
    key = (args.tools_root/'assets/AC7Key.bin').read_bytes()
    original_pair = encode_ac7_pair(path.with_suffix('.uasset').read_bytes(), path.with_suffix('.uexp').read_bytes(), key, source_name)
    if original_pair != (original_encoded.read_bytes(), original_encoded.with_suffix('.uexp').read_bytes()):
        raise ValueError('原生编码未能逐字节复原')
    encoded = output/'source'
    encoded.mkdir()
    pair = encode_ac7_pair(asset.read_bytes(), asset.with_suffix('.uexp').read_bytes(), key, NAME)
    target = encoded/(NAME+'.uasset')
    target.write_bytes(pair[0]);target.with_suffix('.uexp').write_bytes(pair[1])
    decoded = decode_copy(target, key)
    if decoded.read_bytes() != asset.read_bytes() or decoded.with_suffix('.uexp').read_bytes() != asset.with_suffix('.uexp').read_bytes():
        raise ValueError('新名编码与独立解码回读不符')
    report = {'recipe': {'name': NAME, 'source_asset': source_name, 'socket_offset': DELTA, 'sockets': ['sp13', 'sp14']},
              'asset_roundtrip': True, 'renamed_ac7_roundtrip': True, 'all_exports_payload_preserved': True,
              'source_ac7_encoding_reproduced': True, 'all_lod_geometry_bytes_unchanged': actual_raw == raw,
              'lod_count': len(actual_mesh['lods']), 'allowed_changes': ['自身资源名/包名', 'sp13/sp14的RelativeLocation'],
              'socket_changes': changes, 'rotations_scales_and_other_sockets_unchanged': True,
              'encoded_hashes': {s: digest(encoded/(NAME+s)) for s in ('.uasset', '.uexp')},
              'source_encrypted_sha256': digest(original_encoded), 'source_decoded_sha256': digest(path.with_suffix('.uasset'))}
    observation = ROOT/'evidence/m31b-mount-observation.json'
    receipt = {'schema': 1, 'status': 'user_requested_offset_observation_not_validated', 'masks': [6, 7],
               'observation_sha256': digest(observation), 'asset': NAME, 'offset': DELTA,
               'resource_reports': [report], 'resource_input': str(encoded),
               'feedback': '用户本次未遇报错，功能符合预期；确认SAAM与前方LAAM及上方机腹穿插，要求稍后移并下移后继续观察',
               'known_limits': ['不据本次无报错推定旧机库报错根因或所有场景通过', '不宣称本次偏移已消除穿模，支撑与净空仍由用户观察'],
               'assets_generated': True, 'compiled': False, 'tests_run': False, 'installed': False, 'game_process_access': False}
    args.evidence.write_text(json.dumps(receipt, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
    print(json.dumps({'resource': NAME, 'changes': changes, 'resource_input': str(encoded)}, ensure_ascii=False))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--manifest', type=Path, required=True)
    parser.add_argument('--tools-root', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--evidence', type=Path, required=True)
    main(parser.parse_args())
