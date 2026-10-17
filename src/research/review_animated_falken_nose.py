"""ADF-01完整七组合与独立机鼻域的离线深化，不连接进程或生成游戏资源。

显式输入manifest/batch/animation/output；复用统一蒙皮、动画及静态解析器。
只报告资源可证明的事实。原生TLS门控分支与宿主接口尚未证明时保留整机暂缓，
不能用属性名、101键或候选方案直接宣布已有运行支持。
"""
import argparse
import copy
import hashlib
import itertools
import json
from pathlib import Path

from analyze_sp_switch_animations import Assets, animation_tracks, graph, poses
from read_cooked_skeletal_mount import read_skeletal_mesh, skin_points
from review_animated_dark_falken import falken_shared_rack, static_mesh, static_summary, tls_route
from summarize_mount_assets import properties, resolve, value
from survey_aircraft_mounts import reference_bones


def source(path):
    return {'path': str(path.resolve()), 'sha256': hashlib.sha256(path.read_bytes()).hexdigest()}


def fingerprint(data):
    return hashlib.sha256(json.dumps(data, sort_keys=True, separators=(',', ':')).encode()).hexdigest()


def pose_summary(assets, mesh_name, channels):
    """只移除完全不参与网格的缺轨叶骨，不给缺轨填零或参考占位。"""
    model = read_skeletal_mesh(assets.load(mesh_name), mesh_name)
    document = assets.load(mesh_name+'_Skeleton')
    export = next(e for e in document['Exports'] if e['ObjectName'] == mesh_name+'_Skeleton')
    skeleton = reference_bones(document, export)
    tracks, metadata = animation_tracks(assets.load(mesh_name+'_Anim'), skeleton)
    kept = sorted(tracks)
    missing = sorted(set(range(len(skeleton)))-set(kept))
    mesh_names = {b['name'] for b in model['bones']}
    if any(skeleton[i]['name'] in mesh_names for i in missing):
        raise ValueError(mesh_name+'缺轨影响真实网格，不能忽略')
    if any(b['parent'] in missing for b in skeleton):
        raise ValueError(mesh_name+'缺轨有子链，不能忽略')
    remap = {old: new for new, old in enumerate(kept)}
    bones = [dict(skeleton[i], parent=remap[skeleton[i]['parent']] if skeleton[i]['parent'] >= 0 else -1) for i in kept]
    names = [b['name'] for b in bones]
    if not mesh_names <= set(names):
        raise ValueError(mesh_name+'真实网格骨缺姿态')
    samples = [dict(zip(names, p)) for p in poses(bones, {remap[i]: tracks[i] for i in kept}, metadata['frames'])]
    affected = set()
    for b in model['bones']:
        chain, index = [], model['bones'].index(b)
        while index >= 0:
            chain.append(model['bones'][index]['name'])
            index = model['bones'][index]['parent']
        if any(channels.get(n) in ('swc1', 'spwc') for n in chain):
            affected.add(b['name'])
    rows = []
    for li, lod in enumerate(model['lods']):
        counts = {b['name']: 0 for b in model['bones']}
        affected_indices = set()
        for section in lod['sections']:
            for index in range(section['start'], section['start']+section['vertices']):
                for local_bone, weight in lod['weights'][index]:
                    if weight:
                        name = model['bones'][section['bone_map'][local_bone]]['name']
                        counts[name] += 1
                        if name in affected:
                            affected_indices.add(index)
        selected = sorted(affected_indices)
        sweep_min, sweep_max = [float('inf')]*3, [float('-inf')]*3
        selected_bounds = {}
        if selected:
            for frame, sample in enumerate(samples):
                points = skin_points(model, lod, sample)
                lo = [min(points[i][a] for i in selected) for a in range(3)]
                hi = [max(points[i][a] for i in selected) for a in range(3)]
                sweep_min = [min(a, b) for a, b in zip(sweep_min, lo)]
                sweep_max = [max(a, b) for a, b in zip(sweep_max, hi)]
                if frame in (0, 25, 50, 75, 100):
                    selected_bounds[str(frame)] = [lo, hi]
        rows.append({'lod': li, 'vertices': len(lod['points']), 'triangles': len(lod['indices'])//3,
                     'channel_influenced_vertices': len(selected),
                     'channel_weighted_bones': {k: v for k, v in counts.items() if k in affected},
                     'sweep_bounds': [sweep_min, sweep_max] if selected else None,
                     'selected_frame_bounds': selected_bounds,
                     'geometry_weights_sha256': fingerprint([lod['points'], lod['indices'], lod['weights'], lod['sections']])})
    return {'mesh': mesh_name, 'complete_payload': model['complete'], 'mesh_bones': len(model['bones']),
            'skeleton_bones': len(skeleton), 'frames': len(samples), 'sequence_metadata': metadata,
            'missing_tracks': [skeleton[i]['name'] for i in missing],
            'missing_bones_absent_from_mesh': all(skeleton[i]['name'] not in mesh_names for i in missing),
            'missing_bones_have_no_children': all(not any(b['parent'] == i for b in skeleton) for i in missing),
            'missing_track_policy': '不求值无网格/无子链的缺轨叶骨；其余真实网格全部用明确的原始轨。产品仍用完整原生AnimBP，不改骨架。',
            'lods': rows, 'scope': '101键组件空间资源采样；不是实际TLS触发顺序、实时节点混合或原生时间映射的证明。'}


def assignment(groups, weapon, sockets):
    owner = {'TLS': 'SWP1', '4AAM': 'SWP2', 'FAEB': 'SWP3'}[weapon]
    meshes = {'TLS': None, '4AAM': 'w_4aam_a0_SM', 'FAEB': 'w_faeb_a0_closed_SM'}
    defaults = groups[owner]['weapon']['defaults']
    return {'weapon': weapon, 'owner_group': owner, 'owner_class': groups[owner]['config']['WeaponClass'],
            'provider_group': owner, 'sockets': sockets, 'native_weapon_mesh': meshes[weapon],
            'native_load_window': defaults.get('MaxLoadedCount', {}).get('value'),
            'resource_role': 'fixed_emitter_no_ordnance_mesh' if weapon == 'TLS' else 'native_ordnance_mesh',
            'native_emitter_mesh': 'zoef_SWP1' if weapon == 'TLS' else None,
            'logical_policy': '不改原生类、窗口、队列、冷却、伤害或坐标；不从物理位置推断运行池。'}


def combinations(sample):
    groups = {g['group']: g for g in sample['groups']}
    old = {tuple(sorted(c['weapons'])): c for c in sample['proposal']['combinations']}
    rows = []
    for size in (1, 2, 3):
        for selected in itertools.combinations(('TLS', '4AAM', 'FAEB'), size):
            if size == 1:
                row = copy.deepcopy(old[tuple(sorted(selected))])
                row.update(integration_mode='native_passthrough', required_animation_groups=[],
                           required_animation_work=[], runtime_prerequisites=[],
                           native_single_untouched=True, integration_allowed=False)
                rows.append(row)
                continue
            both = '4AAM' in selected and 'FAEB' in selected
            positions = {'TLS': ['sp11'], '4AAM': ['sp21', 'sp22'] if both else ['sp21', 'sp22', 'sp23', 'sp24'],
                         'FAEB': ['sp31', 'sp32']}
            render = (['SWP1'] if 'TLS' in selected else []) + (['SWP2'] if '4AAM' in selected else [])
            if 'FAEB' in selected and not both:
                render.append('SWP3')
            required = ['SWP1'] if 'TLS' in selected else []
            rows.append({'weapons': list(selected), 'status': 'deferred', 'disposition': 'deferred',
                         'assignments': [assignment(groups, w, positions[w]) for w in selected],
                         'rendered_rack_groups': render, 'required_animation_groups': required,
                         'rack_plan': {'render_groups': render, 'required_animation_groups': required,
                                       'hidden_self_only': [g for g in groups if g not in render],
                                       'preserve_native_parent_chains': True, 'new_station_required': False,
                                       'new_resource_required': False},
                         'parts_domains': [{'logical_configuration': 0, 'role': 'TLS_nose',
                                           'Body': ['swc1'], 'SWP1': ['spwc'],
                                           'do_not_control': ['misc', 'gun', 'ordinary_flight_channels']}],
                         'tls_switch_policy': 'finish_tls_before_switch', 'internal_ammo_visibility': None,
                         'required_animation_work': ['机鼻专用原生过渡：TLS结束后仍保持旧原生配置，待真实鼻门和炮体收拢再提交切换；分支证据未齐前不得启用。',
                                                     'Body.swc1和SWP1.spwc保留各自原生AnimBP、曲线、进度与当帧求值，不互写同名spwc或重置鼻门。',
                                                     '所需SWP1骨架必须继续原生Tick及骨骼求值，Body仍原生完整更新一次；失败按实例归还。'],
                         'runtime_prerequisites': ['验证bSpecialWeaponCoverTLSCheck真实原生分支、TLS结束后旧配置0的关闭请求和完成条件。',
                                                   '保留finishPlayerTLS身份、活动对象、函数指纹和三个真实收尾入口校验，不扩大TLS持续白名单。',
                                                   '机鼻实际关闭完成由原生状态和当帧骨骼共同见证；sp11恒定不能作为关闭判据，不造计时器。',
                                                   '补给/检查点/世界和组件重建须重绑、取消旧pending并归还；不得缩减原生逻辑队列/窗口。'],
                         'implementation_prerequisites': ['沿用已支持的fixed_emitter_no_ordnance_mesh角色：TLS原炮体属于SWP1，弹体网格保持null，owner/provider不借位。'],
                         'deferred_reason': '机鼻原生TLS状态分支/关闭完成契约与专用Host仍待动画线程证据；布局完整不等于该契约已实现。',
                         'integration_allowed': False, 'runtime_verified': False, 'gameplay_tested': False})
    return rows


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--manifest', type=Path, action='append', required=True)
    parser.add_argument('--batch', type=Path, required=True)
    parser.add_argument('--animation', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--control-evidence', type=Path, action='append', default=[])
    for kind in ('interface', 'feedback'):
        parser.add_argument('--tls-'+kind+'-evidence', type=Path)
        parser.add_argument('--tls-'+kind+'-sha256')
    args = parser.parse_args()
    assets = Assets(args.manifest)
    batch = next(a for a in json.loads(args.batch.read_text(encoding='utf-8'))['aircraft'] if a['code'] == 'zoef')
    animation = next(a for a in json.loads(args.animation.read_text(encoding='utf-8'))['aircraft'] if a['code'] == 'zoef')
    pawn = assets.load('AcePlayerPawn_zoef')
    defaults = properties(next(e for e in pawn['Exports'] if e['ObjectName'] == 'Default__AcePlayerPawn_zoef_C'))
    activator = properties(next(e for e in pawn['Exports'] if e['ObjectName'] == 'PlayerWeaponActivator'))
    parts_name = resolve(pawn, defaults['PartsAnimationParamDataTable'])
    parts = [value(r) for r in assets.load(parts_name)['Exports'][0]['Table']['Data']]
    bone_channels = {r['BoneName']: r['FlightEngineOuputName'] for r in
                     (value(r) for r in assets.load('PlaneBodyConfig')['Exports'][0]['Table']['Data'])}
    sources = {'batch': source(args.batch), 'animation': source(args.animation),
               'manifests': [source(p) for p in args.manifest], 'analyzer': source(Path(__file__)),
               'control_evidence': [source(p) for p in args.control_evidence],
               'dependencies': [source(Path(__file__).with_name(n)) for n in
                                ('read_cooked_skeletal_mount.py', 'analyze_sp_switch_animations.py',
                                 'review_animated_dark_falken.py', 'split_static_mount_asset.py',
                                 'build_animated_mount_candidates.py')]}
    tls = tls_route(args, sources)
    body = pose_summary(assets, 'zoef', bone_channels)
    emitter = pose_summary(assets, 'zoef_SWP1', bone_channels)
    meshes = {n: static_mesh(assets, n) for n in ('zoef_SWP2_SM', 'zoef_SWP3_SM', 'w_4aam_a0_SM',
                                               'w_faeb_a0_closed_SM', 'w_faeb_a0_SM')}
    shared = falken_shared_rack(meshes)
    row = {'code': 'zoef', 'name': 'ADF-01 FALKEN', 'combinations': combinations(batch),
           'geometric_layout_complete_seven': shared['whole_native_reuse'],
           'layout_candidate_complete_seven': False, 'complete_seven_design': False,
           'integration_allowed': False, 'runtime_enabled': False, 'runtime_ready': False,
           'decision': 'deferred_pending_native_nose_domain_contract',
           'generator_compatibility': {'seven_combination_and_provider_fields_present': True,
                                       'current_normalizer_accepts_fixed_emitter': True,
                                       'boundary': '生成器已支持fixed_emitter_no_ordnance_mesh：native_weapon_mesh=null、native_emitter_mesh=原生zoef_SWP1，owner/provider同为SWP1；不拿组件别名或机库模型当弹体。'},
           'pawn_evidence': {'parts_table': parts_name, 'parts_rows': parts,
                             'activator': activator, 'anim_bindings': {'Body': graph(assets, 'zoef_AnimBP'),
                                                                      'SWP1': graph(assets, 'zoef_SWP1_AnimBP')}},
           'native_groups': batch['groups'], 'body_nose': body, 'SWP1_emitter': emitter,
           'native_socket_evidence': animation['components'][0]['sockets'],
           'static_shared_rack': shared, 'static_mesh_evidence': {n: static_summary(m) for n, m in meshes.items()},
           'tls_lifecycle': tls,
           'animation_domains': {'Body_nose': 'swc1', 'SWP1_emitter': 'spwc',
                                 'TLS_socket': 'SWP1.sp11，Bone=Body，101键静止；不能代替鼻门/炮体的姿态见证'},
           'state_route': [{'state': 'non_TLS', 'policy': '4AAM/FAEB外置原位，鼻部应在原生关闭状态；不借翼下兵装请求打开机鼻。'},
                           {'state': 'enter_TLS', 'policy': '沿原生配置0和bSpecialWeaponCoverTLSCheck控制暖机/打开；不按武器选中或库存常开。'},
                           {'state': 'leave_TLS', 'policy': '释放输入、原生finishPlayerTLS结束，再保持旧原生配置等待原生关闭及当帧鼻部姿态完成，之后才切换逻辑兵装。'},
                           {'state': 'nose_pending', 'policy': 'pending不视为mod致命失败；不能先改activator再假定旧swc1会自己更新。身份改变即取消旧状态并归还。'}],
           'unresolved': ['bSpecialWeaponCoverTLSCheck在当前build的实际原生分支、结束后仍在配置0时是否请求收拢及完成条件尚待控制线程。',
                          '当前共舱profile仅公共spwc，不接收zoef；需要机鼻专用原生过渡Host，不通过写公共spwc冒充swc1。'],
           'boundaries': ['无轨SWPCover1_12/13不在实际Body网格73骨内且无子骨，未参与任何LOD；这项资源几何歧义已解决，仍保留原生完整求值。',
                          'Body机鼻可动蒙皮只在LOD0；LOD1/2/3无这些骨权重，不能推定远景与LOD0动画相同或修改原LOD设计。',
                          '鼻部资源和原生单挂完全保留，不把独立域设计当成已经有运行接口，不把历史TLS通过扩大为七组合实测。']}
    report = {'schema': 'animated-falken-nose-review-v1', 'aircraft': [row], 'sources': sources,
              'asset_sources': assets.used, 'scope': {'compiled': False, 'tests_run': False, 'installed': False,
                                                    'game_process_access': False, 'assets_generated': False,
                                                    'raw_vertices_emitted': False}}
    args.output.write_text(json.dumps(report, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
    print(json.dumps({'output': str(args.output), 'geometric_layout_seven': row['geometric_layout_complete_seven'],
                      'layout_candidate_complete_seven': row['layout_candidate_complete_seven'],
                      'decision': row['decision']}, ensure_ascii=False))


if __name__ == '__main__':
    main()
