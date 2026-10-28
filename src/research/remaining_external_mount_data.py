"""追加本体/DLC外挂机体候选；局部准入只接受逐组合明确登记。"""
from copy import deepcopy
import hashlib
import json


RECEIPT = 'remaining-external-mounts-20261028.json'
AUDIT = 'remaining-external-mount-audit-20261028.json'


def append_remaining_external(planes, assets, preserved, root):
    record = json.loads((root/'evidence'/RECEIPT).read_text(encoding='utf-8'))
    audit = json.loads((root/'evidence'/AUDIT).read_text(encoding='utf-8'))
    additions = record['appended_assets']
    if (record['schema'] != 1 or record['status'] != 'offline_resources_runtime_candidate' or
            record['steam_build'] != 9855922 or assets != record['baseline_assets'] or
            len(additions) != 13 or len(set(assets+additions)) != len(assets)+13 or
            audit['receipt_sha256'] != hashlib.sha256((root/'evidence'/RECEIPT).read_bytes()).hexdigest()):
        raise ValueError('剩余外挂机体的资源基线或引用来源不符')
    for name, digest in record['source_receipts'].items():
        if hashlib.sha256((root/'evidence'/name).read_bytes()).hexdigest() != digest:
            raise ValueError('原生身份来源改变：'+name)
    reports = {r['recipe']['name']: r for r in record['resource_reports']}
    rows = {r['asset']: r for r in audit['rows']}
    if set(reports) != set(additions) or set(rows) != set(additions):
        raise ValueError('新增挂架缺少逐项资源或cooked引用依据')
    for name in additions:
        report, row = reports[name], rows[name]
        if (not all(report.get(k) is True for k in ('asset_roundtrip', 'renamed_ac7_roundtrip',
                'all_exports_payload_preserved', 'non_index_bytes_preserved', 'sockets_and_materials_preserved')) or
                not row['passed'] or row['errors'] or row['encoded_hashes'] != report['encoded_hashes']):
            raise ValueError('新增挂架的完整离线资格不成立：'+name)
    candidates = record['planes']
    if [p['code'] for p in candidates] != ['a10a', 'f02x', 'f104', 'm21b'] or {p['code'] for p in planes} & {p['code'] for p in candidates}:
        raise ValueError('剩余外挂机体顺序或集合错误')
    wanted_native = {'a10a': set(), 'f02x': set(), 'f104': {7}, 'm21b': {3, 5, 7}}
    for plane in candidates:
        if len(plane['groups']) != 3 or len(plane['plans']) != 8:
            raise ValueError('三兵装身份及全部组合决策不完整')
        actual_native = set()
        for mask, plan in enumerate(plane['plans'][1:], 1):
            if plan.get('native_only', False):
                actual_native.add(mask)
                if plan != dict(native_only=True, selected=[[], [], []], racks=0, variants=[-1]*3):
                    raise ValueError('原生退让不得持有位置或替换挂架')
                continue
            if sum(map(len, plan['selected'])) > 64:
                raise ValueError('新布局超过64位物理映射容量')
            for k, selected in enumerate(plan['selected']):
                group = plane['groups'][k]
                if bool(selected) != bool(mask & (1 << k)) or len(set(selected)) != len(selected) or any(not 0 <= s < len(group['sockets']) for s in selected):
                    raise ValueError('组合缺位、重复或越界')
                variant = plan['variants'][k]
                if variant >= 0:
                    if not len(assets) <= variant < len(assets)+13 or reports[(assets+additions)[variant]]['source'] != group['rack']:
                        raise ValueError('挂架副本不是本兵装原始资源的分区')
            if mask in (1, 2, 4) and (plan['variants'] != [-1]*3 or plan['racks'] != mask):
                raise ValueError('单SP必须原生')
        if actual_native != wanted_native[plane['code']]:
            raise ValueError('超出已登记双SP/MGP退让边界')
    collision = record['collision_preservation']
    if len(collision) != 13 or {p['asset'] for p in collision} != set(additions):
        raise ValueError('完整碰撞保持依据缺项')
    planes.extend(deepcopy(candidates))
    assets.extend(additions)
    preserved.extend(deepcopy(collision))
    append_single_mgp(planes, assets, preserved, root)


def append_single_mgp(planes, assets, preserved, root):
    """单吊舱只合并真实发射位置，不改变原生双发及Gunplay数值。"""
    record = json.loads((root/'evidence/mgp-single-pod-20261028.json').read_text(encoding='utf-8'))
    plane = next(p for p in planes if p['code'] == 'm21b')
    if (record['schema'] != 1 or record['status'] != 'single_mgp_native_pair_source_candidate' or
            assets != record['baseline_assets'] or plane != record['expected_plane'] or
            record['prior_receipt_sha256'] != hashlib.sha256((root/'evidence'/RECEIPT).read_bytes()).hexdigest() or
            record['appended_assets'] != ['MSP_m21b_mgp_left_r1', 'MSP_m21b_rkt_right_r1']):
        raise ValueError('MGP单吊舱的原机体、资源基线或来源不符')
    reports = record['resource_reports']
    rows = record['resource_audits']
    if len(reports) != 2 or len(rows) != 2 or len(record['collision_preservation']) != 2:
        raise ValueError('MGP单吊舱资源依据缺项')
    for name, report, row, proof in zip(record['appended_assets'], reports, rows, record['collision_preservation']):
        if (report['recipe']['name'] != name or row['asset'] != name or proof['asset'] != name or
                report['source'] != proof['source'] or row['encoded_hashes'] != report['encoded_hashes'] or
                not row['passed'] or row['errors'] or not all(report.get(k) is True for k in (
                    'asset_roundtrip', 'renamed_ac7_roundtrip', 'all_exports_payload_preserved',
                    'non_index_bytes_preserved', 'sockets_and_materials_preserved'))):
            raise ValueError('MGP单吊舱资源、碰撞或cooked引用资格不成立')
    expected = deepcopy(plane)
    expected['groups'][0]['mapped_fixed'] = True
    right = [i for i, socket in enumerate(plane['groups'][1]['sockets']) if int(socket[2:]) % 2 == 0]
    expected['plans'][3] = dict(selected=[[0], right, []], racks=3, variants=[len(assets), len(assets)+1, -1])
    expected['plans'][5] = dict(selected=[[0], [], [1]], racks=5,
        variants=[len(assets), -1, assets.index('MSP_m21b_saam_right_r1')])
    if record['replacement_plane'] != expected or len(right) != 16:
        raise ValueError('单MGP仅准入左sp11及右RKT/SAAM，不得改写三SP或原生双发身份')
    plane.clear()
    plane.update(deepcopy(expected))
    assets.extend(record['appended_assets'])
    preserved.extend(deepcopy(record['collision_preservation']))
