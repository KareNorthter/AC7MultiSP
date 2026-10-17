"""FB-22公共LAAM架借原生Socket的离线深化，不连接游戏或生成游戏资源。

显式输入--manifest（可重复）、--batch、--animation和--output。
先审查三挂必需的SFFS借站：全101键、完整Body四LOD及完整原架表面，
以原生SFFS两个站位为对照。若全部借站有新增Body穿面，整机七组合暂缓，
不为凑方案忽略Body；输出保留其他双挂的条件表，不将未审查项当通过。
"""
import argparse
import base64
import copy
import hashlib
import json
import math
from pathlib import Path

import numpy as np

from analyze_sp_switch_animations import Assets, animation_tracks, poses
from read_cooked_skeletal_mount import read_skeletal_mesh, reference_component, skin_points
from review_animated_dark_falken import edge_crossings, triangle_bones
from split_static_mount_asset import parse_mesh
from summarize_mount_assets import properties
from survey_aircraft_mounts import reference_bones, rotate, multiply, vector


def digest(value):
    return hashlib.sha256(json.dumps(value, sort_keys=True, separators=(',', ':')).encode()).hexdigest()


def file_source(path):
    return {'path': str(path.resolve()), 'sha256': hashlib.sha256(path.read_bytes()).hexdigest()}


def rotation(q):
    return np.asarray([rotate(q, v) for v in np.eye(3)]).T


def rotator(value):
    while len(value) == 1:
        value = next(iter(value.values()))
    p, y, r = [math.radians(float(value[k]))/2 for k in ('Pitch', 'Yaw', 'Roll')]
    sp, cp, sy, cy, sr, cr = math.sin(p), math.cos(p), math.sin(y), math.cos(y), math.sin(r), math.cos(r)
    return (cr*sp*sy-sr*cp*cy, -cr*sp*cy-sr*cp*sy, cr*cp*sy-sr*sp*cy, cr*cp*cy+sr*sp*sy)


def bounds(points):
    return [[float(points[:, i].min()), float(points[:, i].max())] for i in range(3)] if len(points) else None


class Animated:
    """保留公共解析器的section骨映射/字节权重；仅将LBS计算向量化。"""
    def __init__(self, assets, name, controlled=None):
        self.name = name
        self.model = read_skeletal_mesh(assets.load(name), name)
        document = assets.load(name+'_Skeleton')
        export = next(e for e in document['Exports'] if e['ObjectName'] == name+'_Skeleton')
        self.bones = reference_bones(document, export)
        names = [b['name'] for b in self.bones]
        mesh_names = [b['name'] for b in self.model['bones']]
        if len(set(names)) != len(names) or set(names) != set(mesh_names):
            raise ValueError(name+'网格/骨架唯一骨名集合不一致')
        mesh_by_name = {b['name']: b for b in self.model['bones']}
        for b in self.bones:
            mb = mesh_by_name[b['name']]
            parent = names[b['parent']] if b['parent'] >= 0 else None
            mp = mesh_names[mb['parent']] if mb['parent'] >= 0 else None
            if parent != mp:
                raise ValueError(name+'骨名父链改变：'+b['name'])
        tracks, self.metadata = animation_tracks(assets.load(name+'_Anim'), self.bones)
        if set(tracks) != set(range(len(self.bones))):
            raise ValueError(name+'缺动画轨，不猜原生缺轨策略')
        if controlled is not None:
            for i, b in enumerate(self.bones):
                if b['name'] not in controlled:
                    mb = mesh_by_name[b['name']]
                    tracks[i] = {'translation': [mb['position']], 'rotation': [mb['quaternion']]}
        self.samples = [dict(zip(names, p)) for p in poses(self.bones, tracks, self.metadata['frames'])]
        self.sample_keys = [digest(p) for p in self.samples]
        self.sockets = {}
        for ref in properties(export).get('Sockets', []):
            prop = properties(document['Exports'][ref-1])
            if not prop['SocketName'].startswith('sp'):
                continue
            if 'RelativeLocation' not in prop or 'RelativeRotation' not in prop or prop.get('BoneName') not in names:
                raise ValueError(name+'Socket缺明确局部位置/旋转/骨绑定')
            local, q = vector(prop['RelativeLocation']), rotator(prop['RelativeRotation'])
            values = []
            for sample in self.samples:
                p, bq = sample[prop['BoneName']]
                location = tuple(a+b for a, b in zip(p, rotate(bq, local)))
                values.append((np.asarray(location), rotation(multiply(bq, q))))
            self.sockets[prop['SocketName']] = {'properties': prop, 'samples': values}
        reference = reference_component(self.model['bones'])
        self.lods, self.cache = [], {}
        for lod in self.model['lods']:
            original = np.asarray(lod['points'])
            influences = {}
            for sec in lod['sections']:
                for index in range(sec['start'], sec['start']+sec['vertices']):
                    for local_bone, weight in lod['weights'][index]:
                        if weight:
                            bone = sec['bone_map'][local_bone]
                            influences.setdefault(bone, []).append((index, weight/255))
            prepared = []
            for bone, rows in influences.items():
                ids = np.asarray([i for i, _ in rows])
                weights = np.asarray([w for _, w in rows])[:, None]
                p, q = reference[bone]
                local = (original[ids]-np.asarray(p)) @ rotation(q)
                prepared.append((self.model['bones'][bone]['name'], ids, weights, local))
            self.lods.append({'original': original, 'indices': np.asarray(lod['indices']).reshape(-1, 3), 'prepared': prepared})
        self.lbs_public_comparison_errors = []
        for i, lod in enumerate(self.model['lods']):
            public_points = np.asarray(skin_points(self.model, lod, self.samples[0]))
            error = float(np.abs(public_points-self.points(i, 0)).max())
            if error > 1e-5:
                raise ValueError(name+'向量化LBS与公共skin_points差异超限')
            self.lbs_public_comparison_errors.append(error)

    def points(self, lod, frame):
        key = (lod, self.sample_keys[frame])
        if key not in self.cache:
            data = self.lods[lod]
            result = np.zeros_like(data['original'])
            for name, ids, weights, local in data['prepared']:
                position, q = self.samples[frame][name]
                result[ids] += (local @ rotation(q).T + np.asarray(position))*weights
            self.cache[key] = result
        return self.cache[key]

    def summary(self):
        return {'name': self.name, 'complete_payload': self.model['complete'], 'bytes': self.model['bytes'],
                'frames': len(self.samples), 'unique_pose_count': len(set(self.sample_keys)),
                'frame0_lbs_vs_public_skin_points_max_errors': self.lbs_public_comparison_errors,
                'lods': [{'lod': i, 'vertices': len(l['original']), 'triangles': len(l['indices'])}
                         for i, l in enumerate(self.lods)],
                'sockets': {n: {'properties': s['properties'], 'frame0_position': s['samples'][0][0].tolist(),
                                'last_position': s['samples'][-1][0].tolist()}
                            for n, s in self.sockets.items()}}


def static(assets, name):
    doc = assets.load(name)
    ex = next(e for e in doc['Exports'] if e['ObjectName'] == name)
    model = parse_mesh(base64.b64decode(ex['Extras']))
    return [{'points': np.asarray(l['points']), 'indices': np.asarray(l['buffers'][0]['values']).reshape(-1, 3)}
            for l in model['lods']]


def crossings(points, indices, target, target_indices):
    """只在三角面筛选后判严格内部穿面；返回面号集合和交点范围，不输出顶点。"""
    triangles = points[indices]
    target_triangles = target[target_indices]
    low, high = triangles.min(axis=1), triangles.max(axis=1)
    target_min, target_max = target_triangles.min(axis=1), target_triangles.max(axis=1)
    selected = np.flatnonzero(np.all(high >= target.min(axis=0)-1e-7, axis=1)
                              & np.all(low <= target.max(axis=0)+1e-7, axis=1))
    pairs, witnesses = set(), []
    for start in range(0, len(selected), 128):
        ids = selected[start:start+128]
        a, b = np.nonzero(np.all(low[ids, None, :] <= target_max[None, :, :]+1e-7, axis=2)
                          & np.all(high[ids, None, :] >= target_min[None, :, :]-1e-7, axis=2))
        if not len(a):
            continue
        actual = ids[a]
        one, two = triangles[actual], target_triangles[b]
        h1, w1 = edge_crossings(one, two)
        h2, w2 = edge_crossings(two, one)
        hits = np.flatnonzero(h1 | h2)
        pairs.update((int(actual[k]), int(b[k])) for k in hits)
        for w in (w1, w2):
            if w:
                k = w['pair']
                witnesses.append({'surface_face': int(actual[k]), 'weapon_face': int(b[k]),
                                  'intersection_point_weapon_local': w['point'], 'edge_parameter': w['edge_parameter'],
                                  'face_barycentric': w['face_barycentric']})
    return pairs, witnesses


def audit_station(label, provider, socket, weapon, surfaces, baseline=None):
    result = {'label': label, 'provider': provider.name+'.'+socket, 'rows': []}
    by_frame = {}
    for surface_name, surface in surfaces.items():
        for li, lod in enumerate(surface.lods):
            for wi, weapon_lod in enumerate(weapon):
                counts, novel_counts, union, novel_union, witnesses = [], [], set(), set(), []
                cache = {}
                for frame, (position, matrix) in enumerate(provider.sockets[socket]['samples']):
                    key = (surface.sample_keys[frame], digest([position.tolist(), matrix.tolist()]))
                    if key not in cache:
                        local = (surface.points(li, frame)-position) @ matrix
                        cache[key] = crossings(local, lod['indices'], weapon_lod['points'], weapon_lod['indices'])
                    pairs, sample_witnesses = cache[key]
                    native = baseline.get((surface_name, li, wi, frame), set()) if baseline is not None else set()
                    novel = pairs-native
                    counts.append(len(pairs)); novel_counts.append(len(novel))
                    union.update(pairs); novel_union.update(novel)
                    by_frame[(surface_name, li, wi, frame)] = pairs
                    if frame in (0, 25, 50, 75, 100):
                        for w in sample_witnesses[:2]:
                            witnesses.append({'frame': frame, **w,
                                              'surface_weighted_bones': triangle_bones(surface.model, surface.model['lods'][li], w['surface_face']),
                                              'new_vs_native_pair': (w['surface_face'], w['weapon_face']) not in native if baseline is not None else None})
                result['rows'].append({'surface': surface_name, 'surface_lod': li, 'weapon_lod': wi,
                                       'frames': len(counts), 'crossing_pairs_by_frame': counts,
                                       'novel_pairs_against_native_by_frame': novel_counts if baseline is not None else None,
                                       'union_pairs': len(union), 'union_pairs_sha256': digest(sorted(union)),
                                       'novel_union_pairs': len(novel_union) if baseline is not None else None,
                                       'witnesses_without_raw_vertices': witnesses})
    return result, by_frame


def review(args):
    assets = Assets(args.manifest)
    batch = next(a for a in json.loads(args.batch.read_text(encoding='utf-8-sig'))['aircraft'] if a['code'] == 'fb22')
    animation = next(a for a in json.loads(args.animation.read_text(encoding='utf-8-sig'))['aircraft'] if a['code'] == 'fb22')
    controlled = {b['bone'] for b in animation['body']['controlled_bones'] if b['channel'] in ('spwc', 'swcc')}
    body = Animated(assets, 'fb22', controlled)
    racks = {g: Animated(assets, 'fb22_'+g) for g in ('SWP1', 'SWP2', 'SWP3')}
    if any(len(x.samples) != 101 for x in (body, *racks.values())):
        raise ValueError('本专题只接受已核对的101键资源')
    weapons = {n: static(assets, n) for n in ('w_sffs_a0_closed_SM', 'w_sffs_a0_SM',
                                            'w_xsdb_a0_closed_SM', 'w_xsdb_a0_SM', 'w_laam_x0_SM')}
    # 原生SWP2支撑本来就与同一Body/曲线共存；比较资源身份，不以OBB遮盖Body。
    rack_equal = racks['SWP1'].model['lods'] == racks['SWP2'].model['lods']
    native_results, native_body = [], {}
    for socket in ('sp31', 'sp32'):
        row, pairs = audit_station('native_SFFS_'+socket, racks['SWP3'], socket, weapons['w_sffs_a0_closed_SM'], {'Body': body})
        native_results.append(row)
        for k, v in pairs.items():
            native_body.setdefault(k, set()).update(v)
        print('原生SFFS Body基线完成 '+socket, flush=True)
    candidates = []
    for socket in sorted(racks['SWP2'].sockets):
        row, _ = audit_station('SFFS_on_LAAM_'+socket, racks['SWP2'], socket, weapons['w_sffs_a0_closed_SM'],
                               {'Body': body}, native_body)
        # 架面号不同，原生SFFS架对照单列，不把面号硬对应成同一个挂耳。
        support, _ = audit_station('SFFS_support_'+socket, racks['SWP2'], socket,
                                   weapons['w_sffs_a0_closed_SM'], {'SWP2': racks['SWP2']})
        row['support_surface_audit'] = support
        row['novel_body_intersection'] = any(any(r['novel_pairs_against_native_by_frame']) for r in row['rows'])
        candidates.append(row)
        print('SFFS借站全101键/全部Body LOD完成 '+socket, flush=True)
    original_support = []
    for socket in ('sp31', 'sp32'):
        row, _ = audit_station('native_SFFS_support_'+socket, racks['SWP3'], socket,
                               weapons['w_sffs_a0_closed_SM'], {'SWP3': racks['SWP3']})
        original_support.append(row)
    all_blocked = all(c['novel_body_intersection'] for c in candidates)
    combinations = copy.deepcopy(batch['proposal']['combinations'])
    for combo in combinations:
        selection = set(combo['weapons'])
        combo.update(integration_allowed=False, runtime_verified=False)
        if len(selection) == 1:
            combo.update(status='native_reuse', disposition='native_single_baseline',
                         assignments_policy='完整原组、原生Socket和原网格不变')
        else:
            combo.update(status='deferred', disposition='deferred')
            combo['required_animation_groups'] = ['Body.spwc', 'Body.swcc', 'SWP2.spwc']
            combo['whole_rack_render'] = {'visible': ['fb22_SWP2'], 'hidden_self_only': ['fb22_SWP1', 'fb22_SWP3']}
            combo['provider_route'] = '各逻辑owner保留原类/窗口/原兵装网格，provider统一原生SWP2；不得创建坐标或缩放。'
            combo['reason'] = '含SFFS的所有SWP2候选都有新增Body穿面，整机七组合暂缓。' if all_blocked and 'SFFS' in selection else '七组合整机门槛未满足；本记录不把未完成的具体站位净空核对当通过。'
            combo['assignments'] = []
    return {'schema': 'animated-fb22-review-v1', 'aircraft': 'fb22', 'native_groups': batch['groups'],
            'animation_design': animation['animation_design'], 'body_controlled_bones': sorted(controlled),
            'meshes': {'Body': body.summary(), **{g: m.summary() for g, m in racks.items()}},
            'whole_native_rack_comparison': {'SWP1_SWP2_all_lod_geometry_weights_sections_equal': rack_equal,
                                             'SWP1_SWP2_all_101_named_bone_poses_equal': racks['SWP1'].samples == racks['SWP2'].samples,
                                             'rack_body_identity_boundary': '混挂仅显示完整原SWP2，Body与SWP2仍走原生通道/曲线；架与Body的关系就是原LAAM基线，不叠SWP1/3。兵装借站另查真实表面，不能用此身份论证省略弹体Body核对。'},
            'native_SFFS_body_baseline': native_results, 'native_SFFS_support_baseline': original_support,
            'SFFS_SWP2_candidates': candidates,
            'combinations': combinations, 'complete_seven_design': False, 'integration_allowed': False,
            'decision': 'deferred_all_six_sffs_providers_have_new_body_crossings' if all_blocked else 'deferred_further_weapon_support_and_body_contact_review_required',
            'method': {'frames': 101, 'body_lods': len(body.lods), 'obb_used_as_clearance_proof': False,
                       'intersection': '全部真实三角面经AABB粗筛、双向严格边穿面；原生SFFS两点每帧面号对并集为保守对照，仍报告实际总穿面，不自动把原生重叠称为挂耳。',
                       'body_channels': 'spwc及swcc使用原序列同步101键，其他通道留原参考姿态；不是全部FlightEngineBlend实际输出。',
                       'contact_boundary': '候选新增面号对与原生接触分别记录；若存在新增Body穿面，不以弹体OBB分离或挂耳称呼覆盖。无命中也不是体包含/全插值/离架展开的无碰撞证明。'},
            'pending': ['需要原生SFFS支撑/原位与公共架共存的新路线；本轮不拆架、不借位、不改Socket。',
                        '在Body门槛解决前不宣布三挂1+1+1，更不扩满；XSDB与SFFS展开/下落时序及释放后轨迹未核对。',
                        '原生运行槽、池长度、实际附着变换与缺省scale未知；物理点不足不得缩减逻辑窗口。'],
            'native_contact_distinction': '原生SFFS架接触见证在弹体顶部局部Z约+2.058；六处借站新增Body交点见证在下半部约-1.8，不能用顶部挂耳接触解释。原生Body本就有穿面也如实保留，不将其反推为新组合可忽略全部Body。',
            'reference_vs_sequence_boundary': '原SFFS参考Socket Z约-7.923不等于动画第0帧约-6.423；SWP2中排第0帧约-6.773。所有表面核对使用实际101键，不凭参考姿态高低代替动画。',
            'scope': {'compiled': False, 'tests_run': False, 'installed': False, 'game_process_access': False,
                      'assets_generated': False, 'raw_vertices_emitted': False},
            'sources': {'manifests': [file_source(p) for p in args.manifest], 'batch': file_source(args.batch),
                        'animation': file_source(args.animation), 'analyzer': file_source(Path(__file__)),
                        'dependencies': [file_source(Path(__file__).with_name(n)) for n in
                                         ('read_cooked_skeletal_mount.py', 'analyze_sp_switch_animations.py',
                                          'review_animated_dark_falken.py', 'split_static_mount_asset.py')]},
            'asset_sources': assets.used}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--manifest', action='append', type=Path, required=True)
    parser.add_argument('--batch', type=Path, required=True)
    parser.add_argument('--animation', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    result = review(args)
    args.output.write_text(json.dumps(result, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
    print(json.dumps({'output': str(args.output), 'decision': result['decision']}, ensure_ascii=False))


if __name__ == '__main__':
    main()
