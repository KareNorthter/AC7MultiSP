"""普通Su-57内舱七组合候选；复用原资源，不新增PAK或改变旧资源编号。"""
import hashlib
import json


def digest(value):
    return hashlib.sha256(json.dumps(value, ensure_ascii=False, separators=(',', ':'),
                                     sort_keys=True).encode('utf-8')).hexdigest()


def append_pkfa_internal(data, root):
    source = root/'evidence/pkfa-internal-mount-source-20261028.json'
    record = json.loads(source.read_text(encoding='utf-8'))
    if (record['schema'] != 1 or record['status'] != 'runtime_source_candidate_not_validated'
            or record['steam_build'] != 9855922 or record['added_assets']
            or len(data['assets']) != record['baseline_asset_count']
            or digest(data) != record['baseline_data_sha256']):
        raise ValueError('Su-57候选的28机/77资源基线已变化，须先复核')
    for key in ('static_geometry_source', 'runtime_identity_source'):
        proof = record[key]
        if hashlib.sha256((root/proof['path']).read_bytes()).hexdigest() != proof['sha256']:
            raise ValueError('Su-57来源记录已变化，不能静默沿用候选')
    plane = record['plane']
    if (plane['code'] != 'pkfa' or not plane['local_mechanism'] or plane['local_pool'] != 0
            or [p['code'] for p in data['planes']] != record['baseline_plane_codes']
            or len(data['planes']) != 28 or len(plane['groups']) != 3 or len(plane['plans']) != 8):
        raise ValueError('Su-57候选机型、局部供位架或七组合不完整')
    groups = plane['groups']
    if ([g['sockets'] for g in groups] != [['sp11', 'sp12', 'sp13', 'sp14'],
                                          ['sp21', 'sp22'], ['sp31', 'sp32']]
            or [g['skeletal'] for g in groups] != [True, False, False]
            or [g['fixed'] for g in groups] != [False, False, True]
            or [g['window'] for g in groups] != [4, 2, 2]
            or len(groups[0]['bones']) != 11):
        raise ValueError('Su-57原资源、窗口或骨架范围已变化')
    for mask, plan in enumerate(plane['plans'][1:], 1):
        if plan['variants'] != [-1, -1, -1]:
            raise ValueError('Su-57候选只能使用原资源')
        if mask in (3, 7):
            expected = {'selected': [[], [1], [] if mask == 3 else [0, 1]],
                        'racks': 2 if mask == 3 else 6, 'variants': [-1, -1, -1],
                        'borrowed_positions': [{'kind': 0, 'pool': 1, 'socket': 0}],
                        'full_borrowed_kinds': 1}
        else:
            expected = {'selected': [list(range(len(g['sockets']))) if mask & (1 << i) else []
                                     for i, g in enumerate(groups)],
                        'racks': mask, 'variants': [-1, -1, -1]}
        if plan != expected:
            raise ValueError('Su-57候选必须保持静态双站分区及原生单挂/PLSL路线')
    data['planes'].append(plane)
    return data


def apply_pkfa_dual_4aam(data, root):
    """在原候选校验之后应用作者现场反馈，保留原始记录。"""
    record = json.loads((root/'evidence/pkfa-dual-4aam-feedback-20261028.json').read_text(encoding='utf-8'))
    source = root/record['source']
    if (record['schema'] != 1 or record['aircraft'] != 'pkfa'
            or hashlib.sha256(source.read_bytes()).hexdigest() != record['source_sha256']):
        raise ValueError('Su-57双弹反馈的原候选来源已变化')
    plane = next(p for p in data['planes'] if p['code'] == 'pkfa')
    for mask in (3, 7):
        old = {'selected': [[], [1], [] if mask == 3 else [0, 1]],
               'racks': 2 if mask == 3 else 6, 'variants': [-1, -1, -1],
               'borrowed_positions': [{'kind': 0, 'pool': 1, 'socket': 0}],
               'full_borrowed_kinds': 1}
        expected = {'selected': [[0, 1], [1], [] if mask == 3 else [0, 1]],
                    'racks': 1 if mask == 3 else 5, 'variants': [-1, -1, -1]}
        if plane['plans'][mask] != old or record['plans'][str(mask)] != expected:
            raise ValueError('Su-57双弹反馈与已核对的原供位不符')
        plane['plans'][mask] = expected
    return data
