"""生成外置挂载研究数据；只读离线资源，不连接游戏或写安装目录。"""
import argparse
import itertools
import json
from pathlib import Path

from survey_aircraft_mounts import Survey
from summarize_mount_assets import properties, resolve


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


def groups(survey, code):
    """只处理已确认的静态挂架；活动骨架应另做动画研究。"""
    configs, origins = pawn_config(survey, code)
    document = survey.load('AcePlayerPawn_' + code)
    result = []
    for index, config in enumerate(configs, 1):
        export = next(e for e in document['Exports'] if e['ObjectName'] == f'SWP{index}_GEN_VARIABLE')
        props = properties(export)
        component_type = resolve(document, export['ClassIndex'])
        if component_type != 'StaticMeshComponent' or 'StaticMesh' not in props:
            raise ValueError(f'{code}.SWP{index}不是本工具支持的静态挂架')
        mesh = resolve(document, props['StaticMesh'])
        socket_info = survey.sockets(mesh, {})
        mesh_doc = survey.load(mesh)
        raw_sockets = {}
        for item in mesh_doc['Exports']:
            socket = properties(item)
            if 'SocketName' in socket:
                raw_sockets[socket['SocketName'].lower()] = socket
        for socket in socket_info['sockets']:
            raw = raw_sockets[socket['name'].lower()]
            socket['serialized_rotation'] = raw.get('RelativeRotation')
            socket['serialized_scale'] = raw.get('RelativeScale')
            socket['omitted_transform_fields'] = [key for key in ('RelativeLocation', 'RelativeRotation', 'RelativeScale') if key not in raw]
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
                       'rack_mesh': mesh, 'weapon': weapon, 'weapon_mesh_references': mesh_references(survey, weapon),
                       'resource_socket_count': len(socket_info['sockets']),
                       'socket_role_runtime_verified': False,
                       'sockets': socket_info['sockets'],
                       'runtime_pool_count': None, 'runtime_logical_count': None,
                       'runtime_verified': False})
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
    samples = [{'code':code, 'groups':groups(survey, code)} for code in args.aircraft]
    decisions, reviews = {}, []
    for path in args.decisions:
        review = json.loads(path.read_text(encoding='utf-8'))
        if 'geometry_reviews' in review:
            reviews.extend(review['geometry_reviews'])
        else:
            reviews.append({key:item for key,item in review.items() if key not in ('aircraft', 'supporting_geometry_summary')})
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
                for assignment in combo['assignments']:
                    group = native[assignment['group']]
                    sockets = {socket['name'] for socket in group['sockets']}
                    if not set(assignment['sockets']).issubset(sockets):
                        raise ValueError('候选引用了原生资源中不存在的Socket：' + sample['code'])
                    # 首批人工几何已确认的巢体数量，不将管口数混为外挂数量。
                    role = (sample['code'], assignment.get('weapon'))
                    if role in (('f02a', 'RKT'), ('f104', 'GRKT')):
                        mouths_per_body = 19 if role[0] == 'f02a' else 7
                        count = len(assignment['sockets'])
                        if count % mouths_per_body:
                            raise ValueError('管口数量与本批已确认的巢体不符')
                        assignment.update(position_role='launcher_muzzle', physical_count_unit='muzzle',
                                          muzzle_count=count, launcher_body_count=count // mouths_per_body)
                    else:
                        assignment.update(position_role='ordnance_station_candidate', physical_count_unit='station')
            sample['proposal'] = decision
    # 网格和机体 JSON 的摘要保留在来源表；数据文件不带本机临时绝对路径。
    result = {'schema':1, 'baseline':'v2.4.0', 'steam_build':9855922,
              'status':'offline_layout_candidate' if decisions else 'offline_resource_dossier', 'game_process_access':False,
              'compiled':False, 'tests_run':False, 'installed':False,
              'aircraft':samples, 'sources':survey.used, 'geometry_reviews':reviews,
              'limits':['挂点数量不是已验证的运行模型池或逻辑队列长度',
                        '静态参考坐标不证明动态净空，省略的变换字段保留为未知',
                        '蓝图父类未覆写的原生默认值不猜测',
                        '弹体网格引用不代替持久模型池身份核对',
                        '布局研究不预设TLS或ECM/IEWS与其他SP互斥；持续效果接入按实际类核对']}
    with args.output.open('x', encoding='utf-8') as stream:
        json.dump(result, stream, ensure_ascii=False, indent=2)
        stream.write('\n')
    print(f'已写{len(samples)}架机体资源摘要：{args.output}')


if __name__ == '__main__':
    main()
