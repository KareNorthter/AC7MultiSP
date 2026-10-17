"""离线复核DarkStar原架穿面、ADF-01共架及TLS收尾证据，不连接游戏。

入口显式指定--manifest（可重复）、--batch、--animation-report和--output。
可用--tls-interface-evidence/--tls-feedback-evidence复读历史采样；只有摘要时，
传对应--*-sha256，报告会保留来源并明确未复读。输出不包含原始顶点或权重。
需要NumPy以及本目录公共解析器；不依赖Temp辅助模块，不生成游戏资源。
"""

import argparse
import base64
import copy
import hashlib
import json
from pathlib import Path

import numpy as np

from analyze_sp_switch_animations import Assets, animation_tracks, poses
from read_cooked_skeletal_mount import read_skeletal_mesh, reference_component, skin_points
from split_static_mount_asset import parse_mesh
from survey_aircraft_mounts import reference_bones


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def source(path, expected=None, read_json=False):
    """旧证据可只交接已知摘要，不把未复读文件算作本轮观察。"""
    row = {'path': str(path) if path else None, 'sha256': expected, 'data_reread': False}
    if path is None or not path.is_file():
        row['boundary'] = '未提供或当前无文件；只保留调用者给定的来源摘要。'
        return row, None
    actual = digest(path)
    if expected and actual.casefold() != expected.casefold():
        raise ValueError('证据文件摘要改变：' + str(path))
    row.update(sha256=actual, data_reread=read_json)
    return row, json.loads(path.read_text(encoding='utf-8-sig')) if read_json else None


def static_mesh(assets, name):
    document = assets.load(name)
    export = next(e for e in document['Exports'] if e['ObjectName'] == name)
    return parse_mesh(base64.b64decode(export['Extras'], validate=True))


def bounds(points):
    return [[float(points[:, a].min()), float(points[:, a].max())] for a in range(3)]


def static_summary(model):
    return [{'lod': i, 'vertices': len(lod['points']),
             'triangles': len(lod['buffers'][0]['values']) // 3,
             'bounds_xyz': bounds(np.asarray(lod['points']))}
            for i, lod in enumerate(model['lods'])]


def canonical_triangles(lod):
    """以1e-5资源单位量化比对几何，忽略朝向；不替代材质/显隐核对。"""
    points = [tuple(round(v, 5) for v in p) for p in lod['points']]
    indices = lod['buffers'][0]['values']
    return {tuple(sorted(points[k] for k in indices[i:i+3]))
            for i in range(0, len(indices), 3)}


def edge_crossings(first, second):
    """Moller-Trumbore严格边穿面内部；不将共面、共边或端点当穿透。"""
    hits = np.zeros(len(first), dtype=bool)
    witness = None
    for edge in range(3):
        origin = first[:, edge]
        direction = first[:, (edge+1) % 3] - origin
        e1, e2 = second[:, 1] - second[:, 0], second[:, 2] - second[:, 0]
        h = np.cross(direction, e2)
        determinant = np.einsum('ij,ij->i', e1, h)
        valid = np.abs(determinant) > 1e-8
        inverse = np.zeros(len(first))
        inverse[valid] = 1 / determinant[valid]
        offset = origin - second[:, 0]
        u = inverse * np.einsum('ij,ij->i', offset, h)
        q = np.cross(offset, e1)
        v = inverse * np.einsum('ij,ij->i', direction, q)
        t = inverse * np.einsum('ij,ij->i', e2, q)
        crossed = valid & (u > 1e-6) & (v > 1e-6) & (u+v < 1-1e-6) & (t > 1e-6) & (t < 1-1e-6)
        hits |= crossed
        if witness is None and crossed.any():
            i = int(np.flatnonzero(crossed)[0])
            witness = {'pair': i, 'edge': edge,
                       'point': (origin[i] + direction[i]*t[i]).tolist(),
                       'edge_parameter': float(t[i]),
                       'face_barycentric': [float(1-u[i]-v[i]), float(u[i]), float(v[i])]}
    return hits, witness


def animated_poses(assets, model):
    document = assets.load('dark_SWP1_Skeleton')
    export = next(e for e in document['Exports'] if e['ObjectName'] == 'dark_SWP1_Skeleton')
    bones = reference_bones(document, export)
    names = [b['name'] for b in bones]
    if len(names) != len(set(names)) or set(names) != {b['name'] for b in model['bones']}:
        raise ValueError('DarkStar网格和序列骨名不是唯一对应')
    parents = {b['name']: names[b['parent']] if b['parent'] >= 0 else None for b in bones}
    for b in model['bones']:
        parent = model['bones'][b['parent']]['name'] if b['parent'] >= 0 else None
        if parents[b['name']] != parent:
            raise ValueError('DarkStar网格与动画骨父链改变：' + b['name'])
    tracks, metadata = animation_tracks(assets.load('dark_SWP1_Anim'), bones)
    if set(tracks) != set(range(len(bones))):
        raise ValueError('DarkStar序列出现缺轨，不猜引擎缺轨策略')
    return [dict(zip(names, pose)) for pose in poses(bones, tracks, metadata['frames'])], metadata


def triangle_bones(model, lod, triangle):
    vertices = lod['indices'][triangle*3:triangle*3+3]
    names = set()
    for index in vertices:
        section = next(s for s in lod['sections'] if s['start'] <= index < s['start']+s['vertices'])
        names.update(model['bones'][section['bone_map'][b]]['name'] for b, w in lod['weights'][index] if w)
    return sorted(names)


def dark_intersections(assets, static):
    model = read_skeletal_mesh(assets.load('dark_SWP1'), 'dark_SWP1')
    samples, metadata = animated_poses(assets, model)
    reference = dict(zip((b['name'] for b in model['bones']), reference_component(model['bones'])))
    report = {'reader_complete': model['complete'], 'bytes': model['bytes'],
              'bone_count': len(model['bones']), 'frames': len(samples), 'missing_tracks': [], 'lod_pairs': []}
    for li, lod in enumerate(model['lods']):
        original = np.asarray(lod['points'])
        reconstructed = np.asarray(skin_points(model, lod, reference))
        error = float(np.abs(original-reconstructed).max())
        if error > .0001:
            raise ValueError('DarkStar参考蒙皮重建误差超限')
        indices = np.asarray(lod['indices']).reshape(-1, 3)
        # 每个骨架LOD均与全部静态LOD配对，不外推LOD0为其他LOD。
        for ri, rack in enumerate(static['lods']):
            rack_points = np.asarray(rack['points'])
            rack_indices = np.asarray(rack['buffers'][0]['values']).reshape(-1, 3)
            rack_triangles = rack_points[rack_indices]
            rack_min, rack_max = rack_triangles.min(axis=1), rack_triangles.max(axis=1)
            counts, selected = [], []
            sweep_min, sweep_max = np.full(3, np.inf), np.full(3, -np.inf)
            for frame, pose in enumerate(samples):
                points = np.asarray(skin_points(model, lod, pose))
                sweep_min, sweep_max = np.minimum(sweep_min, points.min(axis=0)), np.maximum(sweep_max, points.max(axis=0))
                triangles = points[indices]
                low, high = triangles.min(axis=1), triangles.max(axis=1)
                pairs = np.nonzero(np.all(low[:, None, :] <= rack_max[None, :, :]+1e-7, axis=2)
                                   & np.all(high[:, None, :] >= rack_min[None, :, :]-1e-7, axis=2))
                first, second = triangles[pairs[0]], rack_triangles[pairs[1]]
                forward, fw = edge_crossings(first, second)
                reverse, rw = edge_crossings(second, first)
                counts.append(int((forward | reverse).sum()))
                if frame in {0, len(samples)//4, len(samples)//2, 3*len(samples)//4, len(samples)-1}:
                    witness = fw or rw
                    row = {'frame': frame, 'strict_crossing_triangle_pairs': counts[-1], 'bounds_xyz': bounds(points)}
                    if witness:
                        pair = witness['pair']
                        ti, tj = int(pairs[0][pair]), int(pairs[1][pair])
                        row['witness'] = {'swp1_triangle_zero_based': ti, 'swp2_triangle_zero_based': tj,
                                          'swp1_weighted_bones': triangle_bones(model, lod, ti),
                                          'edge_origin': 'SWP1' if fw else 'SWP2',
                                          **{k: v for k, v in witness.items() if k != 'pair'}}
                    selected.append(row)
            report['lod_pairs'].append({'skeletal_lod': li, 'static_lod': ri, 'vertices': len(original),
                                        'triangles': len(indices), 'reference_reconstruction_max_error': error,
                                        'crossing_pairs_by_frame': counts, 'selected_witnesses_without_raw_vertices': selected,
                                        'sweep_bounds_xyz': np.column_stack((sweep_min, sweep_max)).tolist()})
    report['whole_native_racks_intersect'] = any(any(p['crossing_pairs_by_frame']) for p in report['lod_pairs'])
    report['sequence_metadata'] = metadata
    report['boundary'] = '资源共同组件空间、原生变换不借位；严格穿面是正证据，未采样插值及FlightEngineBlend实机输出未核对。'
    return report


def falken_shared_rack(meshes):
    large, small = meshes['zoef_SWP2_SM']['lods'], meshes['zoef_SWP3_SM']['lods']
    if len(large) != len(small):
        raise ValueError('ADF01两原架LOD数量改变，不能按LOD编号直接配对')
    rows = []
    for index, (a, b) in enumerate(zip(large, small)):
        superset, subset = canonical_triangles(a), canonical_triangles(b)
        rows.append({'lod': index, 'superset_triangles': len(superset), 'subset_triangles': len(subset),
                     'missing_triangles': len(subset-superset)})
    return {'all_lod_triangle_containment': rows, 'whole_native_reuse': all(r['missing_triangles'] == 0 for r in rows),
            'comparison_boundary': '仅证明几何包含；保留原材质、Socket组件与父链，SWP3仅自身非递归隐藏。'}


def tls_route(args, sources):
    observed = {}
    for name in ('interface', 'feedback'):
        row, data = source(getattr(args, 'tls_'+name+'_evidence'), getattr(args, 'tls_'+name+'_sha256'), True)
        sources['historical_tls_'+name] = row
        observed[name] = data
    interface = (observed['interface'] or {}).get('readonly_observation', {})
    expected = {'blueprint': 'plwp_tls_x2_C', 'end_rva': '0x85f1c0',
                'release_rva': '0x894850', 'detach_rva': '0x861490'}
    matched = bool(interface) and all(interface.get(k) == v for k, v in expected.items())
    return {'selected_policy': 'finish_tls_before_switch', 'persistent_admission_required_for_multisp': False,
            'native_class': 'plwp_tls_x2_C', 'parent': 'AttachedInstantWeapon',
            'expected_end_contract': expected, 'interface_evidence_reread_and_matches': matched,
            'historical_observation': interface or None,
            'historical_user_feedback': (observed['feedback'] or {}).get('candidate8_user_validation'),
            'historical_log_evidence': (observed['feedback'] or {}).get('log_evidence'),
            'route': '未登记持续保有的zoef沿用dualsp_finish_tls→finishPlayerTLS，核对身份/活动数组及真实vtable后请求0x894af0自然终止，再继续切换。',
            'prerequisites': ['保留当前函数指纹和end/release/detach逐对象守卫，失败停止本次切换。',
                              '鼻门独立域依据暖机、活动和自然结束收拢，不因切SP重置，也不因剩余库存永久打开。',
                              '保持x2原默认值，不复制Morgan/Raven参数；多SP选择不要求扩persistent白名单。'],
            'boundary': '复读状态与来源摘要分别记录；旧x2采样及用户收尾反馈不等于本轮ADF01七组合动画或安装已通过。'}


def plans(batch, animations, meshes, dark, shared, tls):
    rows = []
    for code in ('dark', 'zoef'):
        original = batch[code]
        if len(original['proposal']['combinations']) != 7:
            raise ValueError('输入批次不含完整七组合：' + code)
        row = {'code': code, 'name': original['proposal']['name'], 'native_groups': copy.deepcopy(original['groups']),
               'animation_design': copy.deepcopy(animations[code]['animation_design']),
               'combinations': copy.deepcopy(original['proposal']['combinations']),
               'integration_allowed': False, 'runtime_verified': False, 'ready_for_install': False}
        for combo in row['combinations']:
            weapons = set(combo['weapons'])
            combo.update(integration_allowed=False, runtime_verified=False, dynamic_clearance_verified=False)
            if len(weapons) == 1:
                combo.update(status='native_reuse', disposition='native_single_baseline')
            elif code == 'dark' and {'SASM', 'XSDB'} <= weapons:
                # 无穿面也不能反向证明动态净空；XSDB展开/下落时序仍是独立门槛。
                combo.update(status='requires_rack_asset' if dark['whole_native_racks_intersect'] else 'deferred',
                             disposition='deferred', resource_split_required=True if dark['whole_native_racks_intersect'] else None,
                             deferred_reason='完整原架穿面已确认，需支撑专题；XSDB展开与另一侧SASM留架时序也未核对。' if dark['whole_native_racks_intersect'] else '本次未命中严格穿面不构成全程净空；支撑与XSDB展开时序仍需核对。')
                combo['rack_plan']['whole_mesh_plan_approved'] = False
                combo['blockers'] = [
                    {'kind': 'whole_rack_geometry', 'resolved': False,
                     'detail': '原架严格穿面见skeletal_geometry_evidence；无命中也不等于全部动态净空。'},
                    {'kind': 'xsdb_deployment_timing', 'resolved': False,
                     'detail': 'XSDB展开跨另一侧SASM留架区域，原生下落及展开时点未核实。'}]
            else:
                combo.update(status='native_reuse', disposition='candidate_existing_assets', blockers=[],
                             blocker_kind='runtime_prerequisite', resource_split_required=False)
                combo.pop('deferred_reason', None)
                combo['rack_plan']['whole_mesh_plan_approved'] = True
                combo['review_outcome'] = '原资源条件候选；保留窗口、队列、冷却和父链，尚未获得新增动画运行验证。'
            if code == 'zoef' and {'4AAM', 'FAEB'} <= weapons:
                if not shared['whole_native_reuse']:
                    combo.update(status='deferred', disposition='deferred', deferred_reason='静态架全LOD包含关系改变')
                combo['rack_plan'] = {'visible_whole_native_meshes': (['zoef_SWP1'] if 'TLS' in weapons else [])+['zoef_SWP2_SM'],
                                      'hidden_native_mesh_components': [{'group': 'SWP3', 'mesh': 'zoef_SWP3_SM',
                                                                          'hide': '仅自身可见性，非递归；保留Socket与兵装父链'}],
                                      'whole_mesh_plan_approved': shared['whole_native_reuse'],
                                      'asset_splitting_performed': False, 'new_station_coordinates': None}
                combo['physical_mapping_note'] = '4AAM固定SWP2.sp21/22外侧两处，少于逻辑窗口4；FAEB保留SWP3.sp31/32，窗口未知。左右分配不随选装顺序变化。'
            if code == 'zoef' and 'TLS' in weapons:
                combo['tls_switch_policy'] = tls['selected_policy']
                combo['runtime_prerequisites'] = list(tls['prerequisites']) + [
                    'Body swc1与SWP1 spwc独立驱动，真实离架在动画更新后取Socket世界变换。',
                    'Body的SWPCover1_12/13缺压缩轨，必须保留完整原生求值，不将参考占位写成零。',
                    '补给、检查点、组件重建按身份代次重新绑定；不改变库存、窗口、齐射、队列和冷却。']
        row['complete_seven_design'] = code == 'zoef' and shared['whole_native_reuse']
        if code == 'dark':
            row['skeletal_geometry_evidence'] = dark
            row['plsl_all_lod_min_abs_y'] = [min(abs(p[1]) for p in lod['points'])
                                            for lod in meshes['dark_SWP3_SM']['lods']]
            row['limits'] = ['PLSL完整炮体全LOD与中央分区分离；SASM+XSDB和三挂继续后置。',
                             'XSDB展开半翼幅约8.4跨越原生左右lane间距5，离架/展开时序未核实。']
        else:
            row['static_shared_rack_evidence'], row['tls_lifecycle_route'] = shared, tls
            groups = {g['group']: g for g in original['groups']}
            positions = {g: {s['name']: s['reference_position'] for s in groups[g]['sockets']}
                         for g in ('SWP2', 'SWP3')}
            missile_extent = max(abs(p[1]) for lod in meshes['w_4aam_a0_SM']['lods'] for p in lod['points'])
            bomb_extent = max(abs(p[1]) for lod in meshes['w_faeb_a0_SM']['lods'] for p in lod['points'])
            row['static_shared_rack_evidence']['conditional_lateral_gap_bound'] = (
                abs(positions['SWP2']['sp21'][1]-positions['SWP3']['sp31'][1])-missile_extent-bomb_extent)
            row['static_shared_rack_evidence']['gap_boundary'] = (
                '全部LOD弹体局部Y范围与原生平行轴的条件筛选；未覆写的组件/兵装运行方向不猜为零，不代表全部投放动画净空。')
        row['unknowns'] = ['原生运行池、逻辑槽身份和实际附着变换未现场读取。',
                           '未覆写的MaxLoadedCount保留未知；物理位置减少不缩减逻辑窗口。',
                           '离线资源与历史证据不能表示新增七组合已可安装或游戏通过。']
        rows.append(row)
    return rows


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--manifest', type=Path, action='append', required=True)
    parser.add_argument('--batch', type=Path, required=True)
    parser.add_argument('--animation-report', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    for kind in ('interface', 'feedback'):
        parser.add_argument('--tls-'+kind+'-evidence', type=Path)
        parser.add_argument('--tls-'+kind+'-sha256')
    parser.add_argument('--weapon-bridge', type=Path)
    parser.add_argument('--continuous-core', type=Path)
    args = parser.parse_args()
    assets = Assets(args.manifest)
    batch = {a['code']: a for a in json.loads(args.batch.read_text(encoding='utf-8-sig'))['aircraft']}
    animations = {a['code']: a for a in json.loads(args.animation_report.read_text(encoding='utf-8-sig'))['aircraft']}
    sources = {'batch': source(args.batch)[0], 'animation_report': source(args.animation_report)[0],
               'manifests': [source(p)[0] for p in args.manifest]}
    for key in ('weapon_bridge', 'continuous_core'):
        sources[key] = source(getattr(args, key))[0]
    names = ['dark_SWP2_SM', 'dark_SWP3_SM', 'zoef_SWP2_SM', 'zoef_SWP3_SM', 'w_sasm_a0_SM',
             'w_xsdb_a0_closed_SM', 'w_xsdb_a0_SM', 'w_4aam_a0_SM', 'w_faeb_a0_closed_SM', 'w_faeb_a0_SM']
    meshes = {name: static_mesh(assets, name) for name in names}
    dark = dark_intersections(assets, meshes['dark_SWP2_SM'])
    shared = falken_shared_rack(meshes)
    tls = tls_route(args, sources)
    result = {'schema': 'animated-dark-falken-offline-review-v2', 'sources': sources,
              'analyzer': {'path': str(Path(__file__).resolve()), 'sha256': digest(__file__),
                           'dependencies': {n: digest(Path(__file__).with_name(n)) for n in
                                            ('analyze_sp_switch_animations.py', 'read_cooked_skeletal_mount.py',
                                             'split_static_mount_asset.py', 'survey_aircraft_mounts.py')}},
              'scope': {'compiled': False, 'tests_run': False, 'installed': False, 'game_process_access': False,
                        'resource_generated': False, 'boundary': '仅离线资源和旧证据审查，不是新增运行验证或准入表。'},
              'method': {'triangle_containment_quantization': 1e-5, 'edge_determinant_epsilon': 1e-8,
                         'edge_and_barycentric_margin': 1e-6, 'raw_vertices_emitted': False,
                         'component_transform_boundary': '保留原生共同组件空间与未借位的原生相对变换。'},
              'static_mesh_evidence': {n: static_summary(m) for n, m in meshes.items()},
              'aircraft': plans(batch, animations, meshes, dark, shared, tls), 'asset_sources': assets.used}
    args.output.write_text(json.dumps(result, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
    print(json.dumps({'output': str(args.output), 'dark_whole_racks_intersect': dark['whole_native_racks_intersect'],
                      'falken_shared_rack': shared['whole_native_reuse']}, ensure_ascii=False))


if __name__ == '__main__':
    main()
