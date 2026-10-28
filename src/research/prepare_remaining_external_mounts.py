"""制作剩余外挂机体的独立挂架与来源记录；只处理离线副本。"""
import argparse
import base64
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys

from split_static_mount_asset import load_mesh, run_converter
from audit_mount_socket_references import audit_document

ROOT = Path(__file__).resolve().parents[2]
CODES = ('a10a', 'f02x', 'f104', 'm21b')


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def boxes(*intervals):
    return [[-1000, lo, -1000, 1000, hi, 1000] for lo, hi in intervals]


def main(args):
    output = args.output_root.resolve()
    if not output.is_relative_to(ROOT/'Temp/tasks') or output.exists() or args.receipt.exists() or args.audit.exists():
        raise ValueError('只允许任务Temp中的新资源目录及新回执，不覆盖历史文件')
    source = {Path(x['json']).stem: x for x in json.loads(args.source_manifest.read_text(encoding='utf-8'))}
    native = {}
    receipts = {}
    for batch in range(1, 4):
        path = ROOT/f'evidence/mount-expansion-batch{batch}.json'
        data = json.loads(path.read_text(encoding='utf-8'))
        receipts[path.name] = sha(path)
        for plane in data['aircraft']:
            if plane['code'] in CODES:
                native[plane['code']] = plane
                for group in plane['groups']:
                    name = group['rack_mesh']
                    item = source[name]
                    expected = data['sources'][name]
                    if item['encrypted_sha256'] != expected['sha256'] or sha(Path(item['json'])) != expected['json_sha256']:
                        raise ValueError('原挂架与既有来源摘要不符：'+name)
    # 分界位于全部LOD的几何空隙；实际分区仍逐三角面/深度/邻接块拒绝跨界。
    definitions = [
        ('a10a_ugb_inner', 'a10a_SWP1_SM', boxes((-25, 25))),
        ('a10a_rkt_outer_mid', 'a10a_SWP2_SM', boxes((-1000, -25), (25, 1000))),
        ('a10a_rkt_outer', 'a10a_SWP2_SM', boxes((-1000, -43), (43, 1000))),
        ('a10a_agm_mid', 'a10a_SWP3_SM', boxes((-1000, -25), (25, 1000))),
        ('f02x_lasm_inner', 'f02x_SWP1_SM', boxes((-35, 35))),
        ('f02x_aam_outer_inner', 'f02x_SWP2_SM', boxes((-1000, -35), (-24, 24), (35, 1000))),
        ('f02x_ecm_center', 'f02x_SWP3_SM', boxes((-10, 10))),
        ('f104_saam_left', 'f104_SWP1_SM', boxes((-1000, 0))),
        ('f104_grkt_right', 'f104_SWP2_SM', boxes((0, 1000))),
        ('f104_grkt_left', 'f104_SWP2_SM', boxes((-1000, 0))),
        ('f104_hpaa_right', 'f104_SWP3_SM', boxes((0, 1000))),
        ('m21b_rkt_left', 'm21b_SWP2_SM', boxes((-1000, 0))),
        ('m21b_saam_right', 'm21b_SWP3_SM', boxes((0, 1000))),
    ]
    recipes = []
    for suffix, name, keep in definitions:
        _, _, raw, _ = load_mesh(Path(source[name]['json']))
        recipes.append(dict(name='MSP_'+suffix+'_r1', aircraft=suffix.split('_')[0],
                            source_asset=name, source_extras_sha256=hashlib.sha256(raw).hexdigest(), keep_boxes=keep))
    output.mkdir(parents=True)
    recipe_path = output/'recipes.json'
    recipe_path.write_text(json.dumps({'schema': 1, 'variants': recipes}, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
    reports, collision = [], []
    for recipe in recipes:
        name = recipe['name']
        original = Path(source[recipe['source_asset']]['json'])
        subprocess.run([sys.executable, '-X', 'utf8', str(ROOT/'src/research/split_static_mount_asset.py'),
                        '--source-json', str(original), '--recipe', str(recipe_path), '--variant', name,
                        '--output', str(output/name), '--tools-root', str(args.tools_root.resolve())], check=True)
        report = json.loads((output/name/'report.json').read_text(encoding='utf-8'))
        reports.append(report)
        before = json.loads(original.read_text(encoding='utf-8-sig'))
        after = json.loads((output/name/'roundtrip'/(name+'.json')).read_text(encoding='utf-8-sig'))
        ae = next(e for e in before['Exports'] if e['ObjectName'] == original.stem)
        be = next(e for e in after['Exports'] if e['ObjectName'] == name)
        raw, changed = base64.b64decode(ae['Extras']), base64.b64decode(be['Extras'])
        if raw[6:14] != changed[6:14] or before['Imports'] != after['Imports']:
            raise ValueError('碰撞引用改变：'+name)
        body_index = int.from_bytes(raw[6:10], 'little', signed=True)
        nav_index = int.from_bytes(raw[10:14], 'little', signed=True)
        payload_keys = ('ObjectName', 'Data', 'Extras', 'ClassIndex', 'OuterIndex', 'TemplateIndex', 'SuperIndex')
        for index in (body_index, nav_index):
            # 包名长度变化会平移序列化偏移；核对完整载荷与引用，不要求旧文件偏移。
            if index <= 0 or any(before['Exports'][index-1].get(k) != after['Exports'][index-1].get(k) for k in payload_keys):
                raise ValueError('完整碰撞/导航子对象未保留：'+name)
        body = before['Exports'][body_index-1]
        match = re.fullmatch(r'(.+)_([0-9]+)', body['ObjectName'])
        if not match or before['Imports'][-body['ClassIndex']-1]['ObjectName'] != 'BodySetup':
            raise ValueError('碰撞身份未知：'+name)
        collision.append(dict(asset=name, source=original.stem,
            source_package='/Game/Vehicles/Aircraft/'+recipe['aircraft']+'/'+original.stem,
            body_name=match[1], body_number=int(match[2])+1,
            body_guid=list(base64.b64decode(body['Extras'])[:16])))
    # 旧表只作为精确原生身份来源，不回改旧研究的准入或执行状态。
    from build_runtime_mount_data import build
    baseline = build(include_remaining=False)
    assets = baseline['assets']+[r['name'] for r in recipes]
    def variant(suffix):
        return assets.index('MSP_'+suffix+'_r1')
    planes = []
    for code in CODES:
        sample = native[code]
        groups = []
        for k, g in enumerate(sample['groups']):
            assignment = next(c for c in sample['proposal']['combinations'] if len(c['weapons']) == 1 and c['assignments'][0]['group'] == g['group'])['assignments'][0]
            fixed = assignment['position_role'] not in ('ordnance_station_candidate', 'launcher_muzzle')
            mesh = assignment['mesh'] if not fixed else None
            groups.append(dict(**{'class': g['config']['WeaponClass']}, rack=g['rack_mesh'], mesh=mesh,
                sockets=[s['name'].lower() for s in g['sockets']],
                window=g['weapon']['defaults'].get('MaxLoadedCount', {}).get('value', 0),
                fixed=fixed, launcher=assignment['position_role'] == 'launcher_muzzle'))
        plans = [None]+[dict(selected=[list(range(len(g['sockets']))) if mask & (1 << k) else [] for k, g in enumerate(groups)],
                                 racks=mask, variants=[-1]*3) for mask in range(1, 8)]
        def plan(mask, chosen, racks, replacements):
            selected = [[g['sockets'].index(s) for s in chosen[k]] for k, g in enumerate(groups)]
            plans[mask] = dict(selected=selected, racks=racks, variants=[variant(s) if s else -1 for s in replacements])
        if code == 'a10a':
            for mask in (3, 5, 6, 7):
                combo = next(c for c in sample['proposal']['combinations'] if sum(1 << (int(a['group'][-1])-1) for a in c['assignments']) == mask)
                chosen = [next((a['sockets'] for a in combo['assignments'] if a['group'] == 'SWP'+str(k+1)), []) for k in range(3)]
                replacements = {3: ['a10a_ugb_inner', 'a10a_rkt_outer_mid', None],
                                5: [None, None, None], 6: [None, 'a10a_rkt_outer', None],
                                7: ['a10a_ugb_inner', 'a10a_rkt_outer', 'a10a_agm_mid']}[mask]
                plan(mask, chosen, 4 if mask == 5 else mask, replacements)
        elif code == 'f02x':
            # 大型LASM用内翼，6AAM用外翼及最内侧独立站；中央ECM不借作弹体位置。
            for mask in (3, 7):
                plan(mask, [['sp13', 'sp14'], ['sp21', 'sp22', 'sp25', 'sp26'], ['sp31'] if mask == 7 else []], mask,
                     ['f02x_lasm_inner', 'f02x_aam_outer_inner', 'f02x_ecm_center' if mask == 7 else None])
            plan(5, [groups[0]['sockets'], [], ['sp31']], 4, [None]*3)
            plan(6, [[], groups[1]['sockets'], ['sp31']], 6, [None, None, 'f02x_ecm_center'])
        elif code == 'f104':
            left = lambda k: [s['name'].lower() for s in sample['groups'][k]['sockets'] if s['reference_position'][1] < 0]
            right = lambda k: [s['name'].lower() for s in sample['groups'][k]['sockets'] if s['reference_position'][1] > 0]
            plan(3, [left(0), right(1), []], 3, ['f104_saam_left', 'f104_grkt_right', None])
            plan(5, [left(0), [], right(2)], 5, ['f104_saam_left', None, 'f104_hpaa_right'])
            plan(6, [[], left(1), right(2)], 6, [None, 'f104_grkt_left', 'f104_hpaa_right'])
        else:
            left = [s['name'].lower() for s in sample['groups'][1]['sockets'] if s['reference_position'][1] < 0]
            plan(6, [[], left, ['sp32']], 6, [None, 'm21b_rkt_left', 'm21b_saam_right'])
            for mask in (3, 5):
                plans[mask] = dict(native_only=True, selected=[[], [], []], racks=0, variants=[-1]*3)
        if code in ('f104', 'm21b'):
            plans[7] = dict(native_only=True, selected=[[], [], []], racks=0, variants=[-1]*3)
        planes.append(dict(code=code, groups=groups, plans=plans, requires_no_collision=True))
    receipt = dict(schema=1, status='offline_resources_runtime_candidate', baseline='main/37b36a8 / 2.26.1',
        steam_build=9855922, source_receipts=receipts, baseline_assets=baseline['assets'],
        appended_assets=assets[len(baseline['assets']):], planes=planes, resource_reports=reports,
        collision_preservation=collision, source_manifest=str(args.source_manifest.resolve()),
        source_manifest_sha256=sha(args.source_manifest), resources=str(output),
        mgp=dict(status='deferred_pending_native_fire_path', native_damage_per_bullet=5,
            native_max_loaded_count=2, fire_all_in_one_shot=True,
            requirement='单吊舱须保原生两发和总伤害；未证明Bullet两路均走现有取位钩子，含MGP双挂保持原生'),
        compiled=False, tests_run=False, installed=False, game_verified=False)
    args.receipt.write_text(json.dumps(receipt, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
    audit_root = output/'cooked-audit'
    audit_root.mkdir()
    rows = []
    for report in reports:
        name = report['recipe']['name']
        decoded = output/name/'encoded/decoded'/(name+'.uasset')
        target = audit_root/(name+'.json')
        run_converter(args.tools_root.resolve(), ['tojson', decoded, target, 'VER_UE4_18'], target)
        row = audit_document(target)
        row.update(asset=name, encoded_hashes=report['encoded_hashes'])
        if not row['passed']:
            raise ValueError('实际编码副本的cooked引用不符：'+name+str(row['errors']))
        rows.append(row)
    args.audit.write_text(json.dumps(dict(schema=1, receipt_sha256=sha(args.receipt), rows=rows,
        compiled=False, tests_run=False, installed=False), ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
    print('已制作四机候选，资源', len(recipes), '份；MGP组合与两机三挂原生退让')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-manifest', type=Path, required=True)
    parser.add_argument('--tools-root', type=Path, required=True)
    parser.add_argument('--output-root', type=Path, required=True)
    parser.add_argument('--receipt', type=Path, required=True)
    parser.add_argument('--audit', type=Path, required=True)
    main(parser.parse_args())
