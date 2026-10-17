"""Su-47七组合原生左右分区的LOD0、101键只读几何核对。

--manifest可重复，--output必须不存在；不依赖Temp分析结果，不访问游戏。
本工具计算离线资源几何，不编译、不运行测试、不赋予产品运行准入。
"""
import argparse
import hashlib
import itertools
import json
from pathlib import Path

import numpy as np
import review_animated_sukhoi as native
from summarize_mount_assets import properties


PLANS = {
    'SAAM': ['SAAM:SWP1:sp11', 'SAAM:SWP1:sp12'],
    'UGB': ['UGB:SWP2:sp21', 'UGB:SWP2:sp22', 'UGB:SWP2:sp23', 'UGB:SWP2:sp24'],
    'QAAM': ['QAAM:SWP3:sp31', 'QAAM:SWP3:sp32', 'QAAM:SWP3:sp33', 'QAAM:SWP3:sp34'],
    'SAAM+UGB': ['SAAM:SWP1:sp11', 'UGB:SWP2:sp22', 'UGB:SWP2:sp24'],
    'SAAM+QAAM': ['SAAM:SWP1:sp11', 'QAAM:SWP3:sp32', 'QAAM:SWP3:sp34'],
    'UGB+QAAM': ['UGB:SWP2:sp21', 'UGB:SWP2:sp23', 'QAAM:SWP3:sp32', 'QAAM:SWP3:sp34'],
    'SAAM+UGB+QAAM': ['SAAM:SWP1:sp11', 'UGB:SWP2:sp24', 'QAAM:SWP3:sp32'],
}
RACK_PLANS = {
    'SAAM': ['SWP1'], 'UGB': ['SWP2'], 'QAAM': ['SWP3'],
    'SAAM+UGB': ['SWP1', 'SWP2'], 'SAAM+QAAM': ['SWP1', 'SWP3'],
    'UGB+QAAM': ['SWP3'], 'SAAM+UGB+QAAM': ['SWP1', 'SWP3'],
}
SWAPPED_PLAN = ['SAAM:SWP1:sp11', 'UGB:SWP2:sp22', 'QAAM:SWP3:sp34']
DISTANCES = [0, 1, 2, 5, 10, 20, 50, 100, 200, 400]
LAUNCH_FIELDS = {
    'InitialSpeed', 'InitialDropSpeed', 'Acceleration', 'AccelerationDelayFromStart',
    'HomingDelayFromStart', 'FreefallAcceleration', 'SpeedMaximum', 'MaxLoadedCount',
}


def area(triangles):
    return float((np.linalg.norm(np.cross(triangles[:, 1]-triangles[:, 0],
                                          triangles[:, 2]-triangles[:, 0]), axis=1)/2).sum())


def empty():
    return {'pairs': 0, 'first_ids': [], 'second_ids': [],
            'first_witness_face_set_area_source_units2': 0.,
            'second_witness_face_set_area_source_units2': 0.,
            'second_triangle_straddle_first_plane_max_source_units': 0.,
            'second_triangle_straddle_first_plane_p95_source_units': 0.,
            'first_witness_face_envelope_source_units': None,
            'second_witness_face_envelope_source_units': None}


def detail(first, second, include_pair_boxes=False):
    """17轴三角面SAT；可输出逐见证面对的保守AABB交集盒。"""
    ia, ib = np.arange(len(first)), np.arange(len(second))
    keep = np.linalg.norm(np.cross(first[:, 1]-first[:, 0], first[:, 2]-first[:, 0]), axis=1)>1e-10
    first, ia = first[keep], ia[keep]
    keep = np.linalg.norm(np.cross(second[:, 1]-second[:, 0], second[:, 2]-second[:, 0]), axis=1)>1e-10
    second, ib = second[keep], ib[keep]
    if not len(first) or not len(second):
        return empty()
    lo, hi = second.min((0, 1)), second.max((0, 1))
    keep = np.all((first.min(1)<=hi+1e-6)&(first.max(1)>=lo-1e-6), axis=1)
    first, ia = first[keep], ia[keep]
    if not len(first):
        return empty()
    lo, hi = first.min((0, 1)), first.max((0, 1))
    keep = np.all((second.min(1)<=hi+1e-6)&(second.max(1)>=lo-1e-6), axis=1)
    second, ib = second[keep], ib[keep]
    if not len(second):
        return empty()
    broad = np.all((first.min(1)[:, None, :]<=second.max(1)[None, :, :]+1e-6)&
                   (first.max(1)[:, None, :]>=second.min(1)[None, :, :]-1e-6), axis=2)
    ai, bi = np.nonzero(broad)
    if not len(ai):
        return empty()
    a, b = first[ai], second[bi]
    ae = np.stack([a[:, 1]-a[:, 0], a[:, 2]-a[:, 1], a[:, 0]-a[:, 2]], 1)
    be = np.stack([b[:, 1]-b[:, 0], b[:, 2]-b[:, 1], b[:, 0]-b[:, 2]], 1)
    an, bn = np.cross(ae[:, 0], ae[:, 1]), np.cross(be[:, 0], be[:, 1])
    axes = [an, bn]+[np.cross(ae[:, i], be[:, j]) for i in range(3) for j in range(3)]
    axes += [np.cross(ae[:, i], an) for i in range(3)]+[np.cross(be[:, i], bn) for i in range(3)]
    live = np.ones(len(a), bool)
    for axis in axes:
        norm = np.linalg.norm(axis, axis=1)
        axis = axis/np.maximum(norm[:, None], 1e-30)
        pa, pb = np.einsum('nvi,ni->nv', a, axis), np.einsum('nvi,ni->nv', b, axis)
        separated = (pa.max(1)<pb.min(1)-1e-6)|(pb.max(1)<pa.min(1)-1e-6)
        live &= ~(separated&(norm>1e-10))
    if not np.any(live):
        return empty()
    aid, bid = np.unique(ia[ai[live]]), np.unique(ib[bi[live]])
    a, b = a[live], b[live]
    first_set, second_set = first[np.isin(ia, aid)], second[np.isin(ib, bid)]
    normal = an[live]/np.linalg.norm(an[live], axis=1)[:, None]
    signed = np.einsum('nvi,ni->nv', b-a[:, 0, None, :], normal)
    straddle = np.maximum(0, np.minimum(signed.max(1), -signed.min(1)))
    out = {'pairs': int(live.sum()), 'first_ids': aid.tolist(), 'second_ids': bid.tolist(),
           'first_witness_face_set_area_source_units2': area(first_set),
           'second_witness_face_set_area_source_units2': area(second_set),
           'second_triangle_straddle_first_plane_max_source_units': float(straddle.max()),
           'second_triangle_straddle_first_plane_p95_source_units': float(np.quantile(straddle, .95)),
           'first_witness_face_envelope_source_units': native.box(first_set.reshape(-1, 3)),
           'second_witness_face_envelope_source_units': native.box(second_set.reshape(-1, 3))}
    if include_pair_boxes:
        low, high = np.maximum(a.min(1), b.min(1)), np.minimum(a.max(1), b.max(1))
        out['pair_intersection_boxes_source_units'] = [
            {'min': lo.tolist(), 'max': hi.tolist()} for lo, hi in zip(low, high)]
    return out


def compact(row):
    return {key: value for key, value in row.items()
            if not key.endswith('_ids') and key!='pair_intersection_boxes_source_units'}


def summarize(rows, first_totals, second_totals, face_bones=None):
    counts = [row['pairs'] for row in rows]
    maximum = int(np.argmax(counts))
    out = {'pairs_by_key': counts, 'hit_key_ranges': native.frame_ranges(i for i, n in enumerate(counts) if n),
           'max_pairs': max(counts), 'max_pairs_key': maximum,
           'max_first_witness_face_set_area_source_units2': max(x['first_witness_face_set_area_source_units2'] for x in rows),
           'max_second_witness_face_set_area_source_units2': max(x['second_witness_face_set_area_source_units2'] for x in rows),
           'max_first_witness_face_set_area_fraction': max(x['first_witness_face_set_area_source_units2']/total for x, total in zip(rows, first_totals)),
           'max_second_witness_face_set_area_fraction': max(x['second_witness_face_set_area_source_units2']/total for x, total in zip(rows, second_totals)),
           'max_second_triangle_straddle_first_plane_source_units': max(x['second_triangle_straddle_first_plane_max_source_units'] for x in rows),
           'key0': compact(rows[0]), 'key100': compact(rows[-1]), 'max_pair_key_witness': compact(rows[maximum])}
    if face_bones is not None:
        out['body_bone_witness_face_counts_by_key'] = [
            {label: sum(face_bones[i]==label for i in row['first_ids'])
             for label in sorted({face_bones[i] for i in row['first_ids']})} for row in rows]
    return out


def face_bone_labels(parsed):
    lod, vertex = parsed['lods'][0], {}
    for section in lod['sections']:
        for vi in range(section['start'], section['start']+section['vertices']):
            vertex[vi] = {parsed['bones'][section['bone_map'][bi]]['name']
                          for bi, weight in lod['weights'][vi] if weight}
    return ['+'.join(sorted(set.union(*(vertex[int(v)] for v in tri))))
            for tri in np.array(lod['indices']).reshape(-1, 3)]


def launch_sources(group):
    result = {'blueprint_chain': group['weapon']['blueprint_chain'], 'effective_fields': {}, 'serialized_fields_by_source': []}
    for name in group['weapon']['blueprint_chain']:
        doc = native.assets.load(name)
        export = next(e for e in doc['Exports'] if e['ObjectName']=='Default__'+name+'_C')
        fields = {key: value for key, value in properties(export).items() if key in LAUNCH_FIELDS}
        result['serialized_fields_by_source'].append({'blueprint': name, 'fields': fields})
        for key, value in fields.items():
            result['effective_fields'].setdefault(key, {'value': value, 'source': name})
    return result


def lip_regions(row):
    """将实算保守交集盒按X两段及Y四条窄带分组，不硬编码位置或面数。"""
    boxes = row.get('pair_intersection_boxes_source_units', [])
    if not boxes:
        return []
    low, high = np.array([b['min'] for b in boxes]), np.array([b['max'] for b in boxes])
    center = (low+high)/2
    def cuts(values, count):
        ordered = np.sort(values)
        gaps = np.diff(ordered)
        selected = sorted(np.argsort(gaps)[-(count-1):].tolist())
        return [(ordered[i]+ordered[i+1])/2 for i in selected]
    xcuts, ycuts = cuts(center[:, 0], 2), cuts(center[:, 1], 4)
    labels = np.stack([np.digitize(center[:, 0], xcuts), np.digitize(center[:, 1], ycuts)], axis=1)
    result = []
    for x, y in sorted(set(map(tuple, labels.tolist()))):
        keep = np.all(labels==[x, y], axis=1)
        result.append({'x_band': x, 'y_band': y, 'pairs': int(keep.sum()),
                       'conservative_intersection_envelope_source_units':
                       {'min': low[keep].min(0).round(9).tolist(), 'max': high[keep].max(0).round(9).tolist()}})
    return result


def half_recipes(groups, racks, weapons):
    """仅输出原面选择配方；不重编码骨架资源，不删除骨或Socket。"""
    result = {'index_base': 0, 'triangle_id_definition': '原LOD索引缓冲每连续三个索引的面序号',
              'recipes': {}, 'specified_release_samples': {}, 'native_support_checks': {},
              'assets_generated': False, 'loadability_verified': False}
    half = {}
    for name, (frames, indices, info, model) in racks.items():
        if len(model['lods'])!=1:
            raise ValueError('须重核实际LOD，不能把原生单LOD配方套给其他结构')
        y = frames[:, indices, 1]
        left, right = np.all(y<-1e-6, axis=(0, 2)), np.all(y>1e-6, axis=(0, 2))
        keep = np.flatnonzero(left if name=='SWP1' else right)
        drop = np.setdiff1d(np.arange(len(indices)), keep)
        used = np.unique(indices[keep]); half[name] = indices[keep]
        initial_left, initial_right = np.all(y[0]<-1e-6, axis=1), np.all(y[0]>1e-6, axis=1)
        result['recipes'][name] = {'id': 'su47_'+name+('_left' if name=='SWP1' else '_right'),
            'source_mesh': groups[name]['rack_mesh'], 'actual_lods': 1,
            'side': 'negative_y_left' if name=='SWP1' else 'positive_y_right',
            'lod0': {'keep_triangles': keep.tolist(), 'drop_triangles': drop.tolist(),
                'used_original_vertices': used.tolist(), 'kept_vertex_count': len(used),
                'kept_triangle_count': len(keep), 'drop_triangle_count': len(drop),
                'cross_midline_triangles_by_key': [int(np.sum(~(np.all(v<-1e-6, axis=1)|np.all(v>1e-6, axis=1)))) for v in y],
                'left_sign_changed': int(np.sum(initial_left&~left)), 'right_sign_changed': int(np.sum(initial_right&~right)),
                'kept_y_range_101': [float(frames[:, used, 1].min()), float(frames[:, used, 1].max())]},
            'preserve': {'skeleton': groups[name]['socket_asset'], 'animclass': groups[name]['animation']['anim_blueprint']+'_C',
                'component_parent_chain': groups[name]['parent_references'], 'all_native_sockets': groups[name]['sockets'],
                'bones_weights_and_bind_pose': '保留全部原生骨/父链/参考姿态/权重/section局部bone_map，不藏骨',
                'encoding_boundary': 'main/depth/adjacency索引、section范围和其他cooked流须由后续骨架编码器一致处理，当前仅面集'} }
    def contact(rack, key, distance, filtered):
        points = weapons[key]['frames'][100]+np.array([distance, 0, 0])
        indices = half[rack] if filtered else racks[rack][1]
        return compact(detail(racks[rack][0][100][indices], points[weapons[key]['indices']]))
    for rack, key, distances in [('SWP3', 'SAAM:SWP1:sp11', [2, 5, 10]), ('SWP1', 'QAAM:SWP3:sp32', [5, 10])]:
        for distance in distances:
            result['specified_release_samples'][key+'|'+rack+'|plusX'+str(distance)] = {
                'original': contact(rack, key, distance, False), 'half_recipe': contact(rack, key, distance, True)}
    for rack, key in [('SWP1', 'SAAM:SWP1:sp11'), ('SWP2', 'UGB:SWP2:sp24'),
                      ('SWP3', 'QAAM:SWP3:sp32'), ('SWP3', 'UGB:SWP2:sp24')]:
        result['native_support_checks'][key+'|'+rack] = {
            'original': contact(rack, key, 0, False), 'half_recipe': contact(rack, key, 0, True)}
    result['boundary'] = '三份派生Mesh分别绑定原Skeleton/AnimClass/组件父链；SWP2/3同形不代表可互换绑定。离散样本清空不等于真实离架或可加载资产通过。'
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--manifest', type=Path, action='append', required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    if args.output.exists():
        parser.error('--output必须是尚不存在的新路径')
    if not args.output.parent.is_dir():
        parser.error('--output父目录必须已存在')
    native.assets = native.anim.Assets(args.manifest)
    native.survey = native.CheckedSurvey(native.assets)
    gs = {g['group']: g for g in native.groups(native.survey, 'su47')}
    body, body_indices, body_info, body_parsed = native.skin('su47')
    if len(body)!=101:
        raise ValueError('本研究范围要求源101键')
    body_triangles = body[:, body_indices]
    body_areas = [area(t) for t in body_triangles]
    racks = {g: native.skin(gs[g]['rack_mesh']) for g in gs}
    rack_areas = {g: [area(t) for t in value[0][:, value[1]]] for g, value in racks.items()}
    weapons, coordinates = {}, {}
    for weapon, group_name in zip(['SAAM', 'UGB', 'QAAM'], ['SWP1', 'SWP2', 'SWP3']):
        group = gs[group_name]
        for socket in group['sockets']:
            name = socket['name']
            key = f'{weapon}:{group_name}:{name}'
            frames, indices = native.projectile(group, name, native.NATIVE_MESH[weapon], True)
            samples, sample_info = native.socket_samples(group, name, True)
            weapons[key] = {'weapon': weapon, 'group': group_name, 'frames': frames, 'indices': indices,
                            'obb': native.weapon_at(group, name, native.NATIVE_MESH[weapon], True)[0],
                            'areas': [area(t) for t in frames[:, indices]]}
            coordinates[key] = {'socket': sample_info,
                'key0_position': samples[0][0].tolist(), 'key100_position': samples[-1][0].tolist(),
                'key100_forward_X': native.rotate(samples[-1][1], [1, 0, 0]),
                'native_load_window': group['weapon']['defaults']['MaxLoadedCount'],
                'source_socket_count': len(group['sockets']),
                'logical_pool_length': '离线Socket数不证明运行池长度；未缩短队列'}
    out = {'schema': 1, 'aircraft': 'su47', 'plans': PLANS, 'rack_plans': RACK_PLANS,
           'swapped_triple_comparison_plan': SWAPPED_PLAN, 'groups': list(gs.values()),
           'actual_component_bindings': native.actual_component_bindings('su47', list(gs.values())),
           'coordinates_and_windows': coordinates,
           'native_launch_parameters': {name: launch_sources(gs[group]) for name, group in zip(['SAAM', 'UGB', 'QAAM'], ['SWP1', 'SWP2', 'SWP3'])},
           'body_source': body_info, 'rack_sources': {g: value[2] for g, value in racks.items()},
           'body_contacts': {}, 'projectile_pairs': {}, 'rack_weapon_contacts': {},
           'release_translation_samples': {}, 'swapped_triple_release_translation_samples': {},
           'compiled': False, 'tests_run': False, 'installed': False, 'game_process_access': False,
           'boundaries': [
               '仅LOD0与源101键，Body和provider归一化同步；不覆盖独立通道全相位、所有飞行姿态或真实发射轨迹。',
               '坐标及距离保留资源单位；面积为平方资源单位；未核定组件/父链世界缩放，不擅自换算厘米。',
               '面积是去重见证三角面集合面积，不是实际接触区域面积；比例以对应完整LOD0表面面积为分母。',
               'straddle仅为第二网格见证三角顶点跨越第一网格三角平面的较小两侧距离，不是封闭实体穿透深度。',
               '+X与-Z仅为全开端点分量空间离散平移假设，无时间/速度映射，-Z不保证为世界重力方向；采样间可能漏过碰面。',
               '三角面接触含触边；零表面相交不排除完全包含。交集盒为三角面AABB的保守交集，不是真实交线包围盒。',
               '不以38Body全骨精确端点门槛作为左右分区条件；本报告也不赋予产品运行准入。']}
    out['boundaries'].append('plans列出可用原生站位的几何容量，单挂仍走原生；窗口/库存/已装填数决定实际显弹，Socket数不等于逻辑池长度，不缩队列。')
    labels = face_bone_labels(body_parsed)
    for key, weapon in weapons.items():
        rows = [detail(body_triangles[f], weapon['frames'][f][weapon['indices']]) for f in range(101)]
        out['body_contacts'][key] = summarize(rows, body_areas, weapon['areas'], labels)
        print('Body', key, out['body_contacts'][key]['max_pairs'], flush=True)
    all_plans = {**PLANS, '交换右侧对照': SWAPPED_PLAN}
    all_rack_plans = {**RACK_PLANS, '交换右侧对照': ['SWP1', 'SWP3']}
    pairs = sorted({tuple(sorted(pair)) for plan in all_plans.values() for pair in itertools.combinations(plan, 2)})
    for first, second in pairs:
        a, b = weapons[first], weapons[second]
        obb = native.pair_separation(a['obb'], b['obb'])
        rows = [detail(a['frames'][f][a['indices']], b['frames'][f][b['indices']])
                if f in obb['overlap_frames'] else empty() for f in range(101)]
        out['projectile_pairs'][first+'|'+second] = {'obb': obb, **summarize(rows, a['areas'], b['areas'])}
    rackpairs = sorted({(rack, key) for name, plan in all_plans.items() for rack in all_rack_plans[name] for key in plan})
    for rack, key in rackpairs:
        rf, ri = racks[rack][:2]
        weapon = weapons[key]
        rows = [detail(rf[f][ri], weapon['frames'][f][weapon['indices']]) for f in range(101)]
        out['rack_weapon_contacts'][rack+'|'+key] = summarize(rows, rack_areas[rack], weapon['areas'])
    first, second = racks['SWP1'], racks['SWP2']
    rows = [detail(first[0][f][first[1]], second[0][f][second[1]], include_pair_boxes=(f in [0, 100])) for f in range(101)]
    out['native_rack_comparison_SWP1_SWP2'] = summarize(rows, rack_areas['SWP1'], rack_areas['SWP2'])
    out['native_rack_comparison_SWP1_SWP2']['key0_lip_regions'] = lip_regions(rows[0])
    out['native_rack_comparison_SWP1_SWP2']['key100_lip_regions'] = lip_regions(rows[-1])
    out['SWP2_SWP3_identity'] = {
        'all_101_vertex_positions_exactly_equal': bool(np.array_equal(racks['SWP2'][0], racks['SWP3'][0])),
        'all_lod0_triangle_indices_exactly_equal': bool(np.array_equal(racks['SWP2'][1], racks['SWP3'][1])),
        'maximum_vertex_distance': float(np.linalg.norm(racks['SWP2'][0]-racks['SWP3'][0], axis=2).max()),
        'vertex_sweep_sha256': {g: native.hash_array(racks[g][0]) for g in ['SWP2', 'SWP3']},
        'indices_sha256': {g: hashlib.sha256(racks[g][1].astype('<u4').tobytes()).hexdigest() for g in ['SWP2', 'SWP3']}}
    def release_samples(plan_set, rack_set):
        result = {}
        selected = sorted(set(itertools.chain.from_iterable(plan for name, plan in plan_set.items() if '+' in name or name=='交换右侧对照')))
        for key in selected:
            weapon = weapons[key]
            others = sorted({k for plan in plan_set.values() if key in plan for k in plan if k!=key})
            visible = sorted({rack for name, plan in plan_set.items() if key in plan for rack in rack_set[name]})
            result[key] = {}
            for direction, vector in {'plus_X': [1, 0, 0], 'minus_Z': [0, 0, -1]}.items():
                samples = []
                for distance in DISTANCES:
                    moved = (weapon['frames'][100]+np.array(vector)*distance)[weapon['indices']]
                    samples.append({'distance_source_units': distance,
                        'body': compact(detail(body_triangles[100], moved)),
                        'visible_racks': {g: compact(detail(racks[g][0][100][racks[g][1]], moved)) for g in visible},
                        'other_mounted_projectiles': {k: compact(detail(weapons[k]['frames'][100][weapons[k]['indices']], moved)) for k in others}})
                result[key][direction] = samples
        return result
    out['release_translation_samples'] = release_samples(PLANS, RACK_PLANS)
    out['swapped_triple_release_translation_samples'] = release_samples({'交换右侧对照': SWAPPED_PLAN}, {'交换右侧对照': ['SWP1', 'SWP3']})
    out['future_skeletal_half_resources'] = half_recipes(gs, racks, weapons)
    out['sources'] = native.assets.used
    out['manifests'] = [{'basename': p.name, 'sha256': hashlib.sha256(p.read_bytes()).hexdigest()} for p in args.manifest]
    dependency_files = ['review_animated_sukhoi.py', 'analyze_sp_switch_animations.py', 'read_cooked_skeletal_mount.py',
                        'split_static_mount_asset.py', 'survey_aircraft_mounts.py', 'summarize_mount_assets.py', 'build_external_mount_plan.py']
    out['source_sha256'] = {name: hashlib.sha256(Path(__file__).with_name(name).read_bytes()).hexdigest()
                            for name in [Path(__file__).name]+dependency_files}
    with args.output.open('x', encoding='utf-8', newline='\n') as handle:
        json.dump(out, handle, ensure_ascii=False, indent=2)
        handle.write('\n')
    print(json.dumps({'output': str(args.output.resolve()), 'script_sha256': out['source_sha256'][Path(__file__).name],
                      'rack_pairs_min_max': [min(out['native_rack_comparison_SWP1_SWP2']['pairs_by_key']), max(out['native_rack_comparison_SWP1_SWP2']['pairs_by_key'])],
                      'lip_regions': len(out['native_rack_comparison_SWP1_SWP2']['key100_lip_regions'])}, ensure_ascii=False))


if __name__=='__main__':
    main()
