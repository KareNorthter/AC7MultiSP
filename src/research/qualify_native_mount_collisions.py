"""复核七机19份分区副本的完整碰撞子对象；不编译、测试或操作游戏。"""
import argparse
import base64
import hashlib
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main(args):
    sources = {}
    for manifest in args.manifest:
        for item in json.loads(manifest.read_text(encoding='utf-8-sig')):
            path = Path(item.get('json') or '')
            if path.is_file():
                sources.setdefault(path.stem, []).append((path, item))
    entries = []
    for evidence, candidate_root in (
            ('static-native-three-aircraft.json', args.three_root),
            ('local-four-mount-runtime.json', args.four_root)):
        receipt = json.loads((ROOT/'evidence'/evidence).read_text(encoding='utf-8'))
        for report in receipt['resource_reports']:
            name = report['recipe']['name']
            original, intake = next((p, x) for p, x in sources[report['source']]
                                    if digest(p) == report['json_sha256'])
            candidate = candidate_root/name/'roundtrip'/(name+'.json')
            a = json.loads(original.read_text(encoding='utf-8-sig'))
            b = json.loads(candidate.read_text(encoding='utf-8-sig'))
            ae = next(x for x in a['Exports'] if x['ObjectName'] == original.stem)
            be = next(x for x in b['Exports'] if x['ObjectName'] == name)
            ar, br = base64.b64decode(ae['Extras']), base64.b64decode(be['Extras'])
            if ar[6:14] != br[6:14] or a['Imports'] != b['Imports']:
                raise ValueError('碰撞引用或导入变化：'+name)
            body_index = int.from_bytes(ar[6:10], 'little', signed=True)
            nav_index = int.from_bytes(ar[10:14], 'little', signed=True)
            if body_index <= 0 or nav_index <= 0:
                raise ValueError('仅接收完整本地碰撞子对象：'+name)
            payload_keys = ('ObjectName','Data','Extras','ClassIndex','OuterIndex','TemplateIndex','SuperIndex')
            for index in (body_index, nav_index):
                if any(a['Exports'][index-1].get(k) != b['Exports'][index-1].get(k) for k in payload_keys):
                    raise ValueError('碰撞或导航子对象并非逐项保持：'+name)
            body = a['Exports'][body_index-1]
            if a['Imports'][-body['ClassIndex']-1]['ObjectName'] != 'BodySetup':
                raise ValueError('碰撞子对象并非原生BodySetup：'+name)
            guid = base64.b64decode(body['Extras'])[:16]
            match = re.fullmatch(r'(.+)_([0-9]+)', body['ObjectName'])
            if not match or not any(guid):
                raise ValueError('碰撞身份缺失：'+name)
            code = name.split('_')[1]
            entries.append({'asset': name, 'aircraft': code, 'source': original.stem,
                'source_package': '/Game/Vehicles/Aircraft/'+code+'/'+original.stem,
                'body_name': match[1], 'body_number': int(match[2])+1, 'body_guid': list(guid),
                'body_export_sha256': hashlib.sha256(json.dumps(body, sort_keys=True).encode()).hexdigest(),
                'nav_export_sha256': hashlib.sha256(json.dumps(a['Exports'][nav_index-1], sort_keys=True).encode()).hexdigest(),
                'source_json_sha256': digest(original), 'candidate_json_sha256': digest(candidate),
                'source_receipt': evidence, 'source_receipt_sha256': digest(ROOT/'evidence'/evidence),
                'collision_references_unchanged': True, 'complete_body_and_nav_exports_unchanged': True,
                'encoded_hashes': report['encoded_hashes']})
    if len(entries) != 19 or len({x['asset'] for x in entries}) != 19:
        raise ValueError('七机分区副本集合不完整')
    live = json.loads(args.live_identity.read_text(encoding='utf-8'))
    sample = next(e for e in entries if e['asset']=='MSP_f14a_sffs_belly')
    if (live['process_write'] or live['game_functions_called'] or live['body']['class_name']!='BodySetup'
            or live['guid_offsets']!=['0x2e0'] or live['body_class_properties_size']!=0x3a0
            or bytes(sample['body_guid']).hex()!=live['body_guid'] or live['body_number']!=sample['body_number']
            or live['component_fields']['bUseDefaultCollision']['value'] is not False):
        raise ValueError('运行碰撞身份与原生资源依据不符')
    result = {'schema': 1, 'status': 'offline_collision_payload_preserved_runtime_candidate',
        'scope': ['f14a','f18e','f18etg','f14d','f18f','su33','f18x'], 'entries': entries,
        'body_layout': {'size':0x3a0,'guid_offset':0x2e0,'live_identity_sha256':digest(args.live_identity)},
        'limits': ['完整碰撞和导航序列化数据保持，不把模型模式3改为0',
                   '运行仍须核对原生包、BodySetup弱身份和GUID、组件原生配置及换网格前后配置不变',
                   '未证明游戏内碰撞、发射、动画或所有Mod兼容'],
        'compiled': False, 'tests_run': False, 'installed': False}
    if args.output.exists():
        raise ValueError('不覆盖旧碰撞依据')
    args.output.write_bytes((json.dumps(result, ensure_ascii=False, indent=2)+'\n').encode('utf-8'))
    print('已复核七机19份副本的完整BodySetup/NavCollision子对象')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--manifest', type=Path, action='append', required=True)
    parser.add_argument('--three-root', type=Path, required=True)
    parser.add_argument('--four-root', type=Path, required=True)
    parser.add_argument('--live-identity', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    main(parser.parse_args())
