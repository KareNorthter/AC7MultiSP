"""生成外置挂载研究数据；只读离线资源，不连接游戏或写安装目录。"""
import argparse
import itertools
import json
from pathlib import Path

from survey_aircraft_mounts import Survey, reference_bones, rotate, vector
from summarize_mount_assets import properties, resolve
from build_mount_handoff import aircraft_gate


def pawn_config(survey, code):
    """逐字段合并基类配置，同时保留每个真实武器类的来源。"""
    configs = [{} for _ in range(3)]
    sources = [{} for _ in range(3)]
    for name in ('AcePlayerPawn', 'AcePlayerPawn_' + code):
        document = survey.load(name)
        activator = next(e for e in document['Exports'] if e['ObjectName'] == 'PlayerWeaponActivator')
        props = properties(activator)
        for index in range(3):
            key = f'SpecialWeaponConfig{index+1}'
            for field, item in props.get(key, {}).items():
                configs[index][field] = resolve(document, item) if field == 'WeaponClass' else item
                sources[index][field] = name
    return configs, sources


def mesh_references(survey, weapon):
    """保留各继承层的网格引用，不把弹体引用冒充已确认的运行模型池。"""
    records = []
    for name in weapon['blueprint_chain']:
        document = survey.load(name)
        for export in document['Exports']:
            prop = properties(export)
            if prop.get('StaticMesh'):
                records.append({'blueprint': name, 'object': export['ObjectName'],
                                'mesh': resolve(document, prop['StaticMesh'])})
    return records


def read_sockets(survey, asset, skeletal, channels):
    """保留原始变换；骨骼缺省或未解析时不猜根骨骼。"""
    document = survey.load(asset)
    matching = [e for e in document['Exports'] if e['ObjectName'].casefold() == asset.casefold()]
    if len(matching) != 1:
        raise ValueError('挂点资源导出身份不唯一：' + asset)
    export = matching[0]
    bones, bone_error = [], None
    if skeletal:
        try:
            bones = reference_bones(document, export)
        except (KeyError, ValueError) as error:
            bone_error = str(error)
    by_name = {bone['name']:bone for bone in bones}
    sockets = []
    for reference in properties(export).get('Sockets', []):
        if reference <= 0:
            raise ValueError('Socket不是本资源导出：' + asset)
        raw = properties(document['Exports'][reference-1])
        if not raw['SocketName'].lower().startswith(('sp', 'ws')):
            continue
        location = vector(raw['RelativeLocation']) if 'RelativeLocation' in raw else None
        bone = by_name.get(raw.get('BoneName')) if skeletal else None
        reason = None
        if skeletal:
            if bone is None:
                location = None
                reason = 'BoneName省略、未匹配或参考骨架未解析；不推断根骨骼'
            elif location is not None:
                location = tuple(a+b for a,b in zip(bone['position'],rotate(bone['quaternion'],location)))
        if 'RelativeLocation' not in raw:
            reason = '资源省略RelativeLocation；未读取原生默认值'
        sockets.append({'name':raw['SocketName'], 'bone':raw.get('BoneName'),
                        'reference_position':[round(v,5) for v in location] if location is not None else None,
                        'position_status':reason or '已解析并换算',
                        'serialized_location':raw.get('RelativeLocation'),
                        'serialized_rotation':raw.get('RelativeRotation'), 'serialized_scale':raw.get('RelativeScale'),
                        'omitted_transform_fields':[k for k in ('RelativeLocation','RelativeRotation','RelativeScale') if k not in raw],
                        'bone_chain':bone['chain'] if bone else [],
                        'bone_reference_quaternion':list(bone['quaternion']) if bone else None,
                        'ancestor_animation_channels':sorted({channels[n] for n in bone['chain'] if n in channels}) if bone else []})
    return {'sockets':sockets, 'bone_count':len(bones), 'reference_bone_parse_error':bone_error,
            'export_object_name':export['ObjectName']}


def animation_context(survey, code, mesh=None):
    """只记录资源控制节点和序列长度，不把采样长度当实际开舱时长。"""
    name = (mesh or code) + '_AnimBP'
    nodes = []
    if name in survey.assets:
        document = survey.load(name)
        for export in document['Exports']:
            if not export['ObjectName'].startswith('Default__'):
                continue
            for key, item in properties(export).items():
                if not key.startswith('AnimGraphNode_FlightEngineBlend'):
                    continue
                sequence = item.get('SequenceConfig', {}).get('Sequence')
                sequence = resolve(document, sequence) if sequence else None
                bone_config = item.get('BoneConfig')
                bone_config = resolve(document, bone_config) if bone_config else None
                row = {'node':key, 'sequence':sequence, 'bone_config':bone_config,
                       'serialized_sequence_options':{k:v for k,v in item.get('SequenceConfig',{}).items() if k != 'Sequence'}}
                if sequence in survey.assets:
                    seq_doc = survey.load(sequence)
                    seq = next(e for e in seq_doc['Exports'] if e['ObjectName']==sequence)
                    opts = properties(seq)
                    row.update(asset_length=opts.get('SequenceLength'), frames=opts.get('NumFrames'))
                nodes.append(row)
    return {'anim_blueprint':name, 'available':name in survey.assets, 'driver_nodes':nodes,
            'runtime_channels_verified':False,
            'boundary':'参考姿态和序列采样长度不证明动态净空或实际开关舱时长'}


def groups(survey, code):
    """摘取静态/骨架原生规格；可接入性由独立布局与动画评审决定。"""
    configs, origins = pawn_config(survey, code)
    document = survey.load('AcePlayerPawn_' + code)
    channels = {}
    if 'PlaneBodyConfig' in survey.assets:
        table = survey.load('PlaneBodyConfig')
        from summarize_mount_assets import value
        for raw in table['Exports'][0]['Table']['Data']:
            row = value(raw)
            if 'BoneName' in row and 'FlightEngineOuputName' in row:
                channels[row['BoneName']] = row['FlightEngineOuputName']
    result = []
    for index, config in enumerate(configs, 1):
        export = next(e for e in document['Exports'] if e['ObjectName'] == f'SWP{index}_GEN_VARIABLE')
        props = properties(export)
        component_type = resolve(document, export['ClassIndex'])
        if component_type not in ('StaticMeshComponent','SkeletalMeshComponent'):
            raise ValueError(f'{code}.SWP{index}的组件类型未支持')
        skeletal = component_type == 'SkeletalMeshComponent'
        mesh = resolve(document, props['SkeletalMesh' if skeletal else 'StaticMesh'])
        socket_asset = mesh + '_Skeleton' if skeletal else mesh
        socket_info = read_sockets(survey, socket_asset, skeletal, channels)
        parents = []
        for item in document['Exports']:
            node = properties(item)
            if node.get('ComponentTemplate') and resolve(document, node['ComponentTemplate']) == export['ObjectName']:
                parents.append(node.get('ParentComponentOrVariableName'))
        weapon = survey.weapon(config['WeaponClass'])
        result.append({'configuration': index-1, 'group': f'SWP{index}',
                       'config': config, 'config_field_sources': origins[index-1],
                       'component_class': component_type, 'parent_references': parents,
                       'component_transform_overrides': {key:item for key,item in props.items() if key.startswith('Relative')},
                       'rack_mesh': mesh, 'socket_asset':socket_asset, 'socket_export_object_name':socket_info['export_object_name'],
                       'animation':animation_context(survey,code,mesh) if skeletal else None,
                       'weapon': weapon, 'weapon_mesh_references': mesh_references(survey, weapon),
                       'resource_socket_count': len(socket_info['sockets']),
                       'socket_role_runtime_verified': False,
                       'sockets': socket_info['sockets'], 'bone_count':socket_info['bone_count'],
                       'reference_bone_parse_error':socket_info['reference_bone_parse_error'],
                       'runtime_pool_count': None, 'runtime_logical_count': None,
                       'runtime_verified': False})
    return result


def aircraft_context(survey, code):
    """摘取本体舱门与控制配置，和各SWP骨架分开记录。"""
    from summarize_mount_assets import value
    channels = {}
    if 'PlaneBodyConfig' in survey.assets:
        table = survey.load('PlaneBodyConfig')
        for raw in table['Exports'][0]['Table']['Data']:
            row = value(raw)
            if 'BoneName' in row and 'FlightEngineOuputName' in row:
                channels[row['BoneName']] = row['FlightEngineOuputName']
    result = {'body_animation':animation_context(survey,code), 'runtime_verified':False}
    wanted = code + '_Skeleton'
    matching = [name for name in survey.assets if name.casefold()==wanted.casefold()]
    if len(matching)==1:
        name = matching[0]
        result['body_socket_asset'] = name
        result['body_sockets'] = read_sockets(survey,name,True,channels)
    else:
        result['body_socket_asset'] = None
        result['body_socket_status'] = '本批未取得唯一的本体骨架资源'
    parts = 'PartsAnimationConfig_' + code
    result['parts_asset'] = parts if parts in survey.assets else None
    if parts in survey.assets:
        document = survey.load(parts)
        rows = []
        for export in document['Exports']:
            if 'Table' in export:
                rows.extend(value(row) for row in export['Table']['Data'])
        result['parts_serialized_rows'] = rows
        result['cover_options'] = [{k:v for k,v in row.items() if 'cover' in k.casefold()} for row in rows]
    return result


def main():
    """按显式清单生成可交接摘要；既有输出拒绝覆盖。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--manifest', type=Path, action='append', required=True)
    parser.add_argument('--aircraft', action='append', required=True)
    parser.add_argument('--decisions', type=Path, action='append', default=[])
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    if len(set(args.aircraft)) != len(args.aircraft):
        raise ValueError('机体代码重复')
    survey = Survey(args.manifest)
    samples = [{'code':code, 'groups':groups(survey, code), 'aircraft_context':aircraft_context(survey,code)} for code in args.aircraft]
    decisions, reviews = {}, []
    for path in args.decisions:
        review = json.loads(path.read_text(encoding='utf-8'))
        if 'geometry_reviews' in review:
            reviews.extend(review['geometry_reviews'])
            if review.get('sources'):
                reviews.append({'review_sources':review['sources']})
        else:
            summary = {key:item for key,item in review.items() if key not in ('aircraft', 'supporting_geometry_summary')}
            channel_source = summary.get('animation_channel_binding_source')
            if channel_source and 'rows' in channel_source:
                # 只交接SP与普通导弹舱所需映射，不复制整张飞控资源表。
                channel_source['rows'] = [row for row in channel_source['rows']
                                          if row.get('FlightEngineOuputName') in ('spwc','swcc','swc1','swc2','swc3','misc')]
                channel_source['selection_note'] = '仅保留SP与普通导弹舱通道的派生摘要；不是完整原生表'
            reviews.append(summary)
        for record in review['aircraft']:
            # 已交付数据自身保留人工布局输入，可用于重新采集后再生成。
            item = record.get('proposal', record)
            code = item['code']
            if code in decisions or code not in args.aircraft:
                raise ValueError('布局评审机体重复或超出本批：' + code)
            combos = item['combinations']
            if len(combos) != 7 or len({tuple(sorted(c['weapons'])) for c in combos}) != 7:
                raise ValueError('布局评审必须包含七种不同组合：' + code)
            labels = {weapon for combo in combos for weapon in combo['weapons']}
            expected = {tuple(sorted(parts)) for size in range(1, 4) for parts in itertools.combinations(labels, size)}
            if len(labels) != 3 or {tuple(sorted(c['weapons'])) for c in combos} != expected:
                raise ValueError('布局组合未覆盖三种SP的全部非空集合：' + code)
            decisions[code] = item
    if decisions and set(decisions) != set(args.aircraft):
        raise ValueError('本批有机体缺少布局评审')
    for sample in samples:
        if sample['code'] in decisions:
            decision = decisions[sample['code']]
            native = {group['group']: group for group in sample['groups']}
            for combo in decision['combinations']:
                status = combo['status']
                if status not in ('native_reuse', 'requires_rack_asset', 'requires_new_station', 'deferred'):
                    raise ValueError('未知的布局研究状态：' + status)
                # 用户已要求新资源工作后置；保留技术依赖，另记是否允许继续接入。
                if status != 'native_reuse':
                    combo['disposition'] = 'deferred'
                    combo.setdefault('blocker_kind', {'requires_rack_asset':'resource_split',
                                                     'requires_new_station':'new_station'}.get(status,'unknown_support'))
                    blockers = [x if isinstance(x,str) else x.get('detail',str(x)) for x in combo.get('blockers',[])]
                    combo.setdefault('deferred_reason', '；'.join(blockers) or combo.get('reason') or '依赖尚未核实，暂缓接入')
                else:
                    combo['disposition'] = 'native_single' if len(combo['weapons']) == 1 else 'candidate_existing_assets'
                    if {part['weapon'] for part in combo['assignments']} != set(combo['weapons']):
                        raise ValueError('现成资源候选未给每种SP分配位置：' + sample['code'])
                for assignment in combo['assignments']:
                    group = native[assignment['group']]
                    weapon_id = group['weapon']['defaults'].get('WeaponID', {}).get('value')
                    if weapon_id and weapon_id.startswith('EWeaponID::WID_'):
                        expected_label = weapon_id.removeprefix('EWeaponID::WID_').casefold()
                        label = assignment['weapon'].casefold()
                        label = {'iews':'ecm','esm':'ecm','ecmp':'ecm'}.get(label,label)
                        if label != expected_label:
                            raise ValueError('候选弹种与真实兵装配置不一致：' + sample['code'])
                    assignment['native_weapon_id'] = weapon_id
                    sockets = {socket['name'] for socket in group['sockets']}
                    if not set(assignment['sockets']).issubset(sockets):
                        raise ValueError('候选引用了原生资源中不存在的Socket：' + sample['code'])
                    if len(set(assignment['sockets'])) != len(assignment['sockets']):
                        raise ValueError('候选位置重复；轮转映射应另行记录')
                    window = group['weapon']['defaults'].get('MaxLoadedCount', {}).get('value')
                    if assignment.get('loaded_window') is not None and assignment['loaded_window'] != window:
                        raise ValueError('候选装填窗口与兵装继承资料不一致：' + sample['code'])
                    assignment['loaded_window'] = window
                    assignment['below_window'] = len(assignment['sockets']) < window if window is not None else None
                    # 所有火箭均按管口记数；只有明确确认过的机型才填写巢体数量。
                    role = (sample['code'], assignment.get('weapon'))
                    native_kind = weapon_id.removeprefix('EWeaponID::WID_').casefold() if weapon_id else assignment['weapon'].casefold()
                    if native_kind in ('rkt','grkt','admm'):
                        count = len(assignment['sockets'])
                        assignment.update(position_role='launcher_muzzle', physical_count_unit='muzzle',
                                          muzzle_count=count)
                        mouths_per_body = {('f02a', 'RKT'):19, ('f104', 'GRKT'):7}.get(role)
                        if mouths_per_body:
                            if count % mouths_per_body:
                                raise ValueError('管口数量与本批已确认的巢体不符')
                            assignment['launcher_body_count'] = count // mouths_per_body
                        else:
                            assignment.setdefault('launcher_body_count', None)
                        if role == ('a10a', 'RKT'):
                            assignment['launcher_body_count'] = None
                            assignment['launcher_body_count_status'] = '42管口呈三对横向簇；实体巢数未按完整几何逐座确认'
                    else:
                        assignment.update(position_role='ordnance_station_candidate', physical_count_unit='station')
                    if native_kind in ('tls','plsl','eml','mgp'):
                        assignment.update(position_role='fixed_emitter', physical_count_unit='emitter_origin',
                                          emitter_visibility_policy='装备期间保留炮体，不按单发或冷却隐藏')
                    elif native_kind == 'ecm':
                        assignment.update(position_role='effect_source', physical_count_unit='effect_reference',
                                          emitter_visibility_policy='保留原生装置，效果归属与显隐分开管理')
                    elif native_kind == 'uav':
                        assignment.update(position_role='uav_deployment_reference', physical_count_unit='deployment_reference',
                                          emitter_visibility_policy='任务停挂、展开、出击与返回另核对，不用展示架替代')
            sample['proposal'] = decision
            sample['integration_gate'] = aircraft_gate(decision['combinations'])
            for combo in decision['combinations']:
                combo['integration_allowed'] = sample['integration_gate']['eligible']
    # 网格和机体 JSON 的摘要保留在来源表；数据文件不带本机临时绝对路径。
    result = {'schema':1, 'baseline':'v2.4.0', 'steam_build':9855922,
              'status':'offline_layout_candidate' if decisions else 'offline_resource_dossier', 'game_process_access':False,
              'compiled':False, 'tests_run':False, 'installed':False, 'assets_generated':False,
              'research_policy':{'existing_whole_assets_only':True, 'defer_resource_splitting':True,
                                 'defer_new_stations':True, 'require_all_seven_combinations':True,
                                 'runtime_integration_performed':False},
              'aircraft':samples, 'sources':survey.used, 'geometry_reviews':reviews,
              'limits':['挂点数量不是已验证的运行模型池或逻辑队列长度',
                        '静态参考坐标不证明动态净空，省略的变换字段保留为未知',
                        '蓝图父类未覆写的原生默认值不猜测',
                        '弹体网格引用不代替持久模型池身份核对',
                        '布局研究不预设TLS或ECM/IEWS与其他SP互斥；持续效果接入按实际类核对']}
    with args.output.open('x', encoding='utf-8', newline='\n') as stream:
        json.dump(result, stream, ensure_ascii=False, indent=2)
        stream.write('\n')
    print(f'已写{len(samples)}架机体资源摘要：{args.output}')


if __name__ == '__main__':
    main()
