"""三机静态候选及用户指定MiG-31B观察方案；已知几何冲突不改写为通过。"""
from copy import deepcopy
import json


def append_native_static_batch(planes, assets, root):
    path = root/'evidence/static-native-three-aircraft.json'
    data = json.loads(path.read_text(encoding='utf-8'))
    observation=json.loads((root/'evidence/m31b-mount-observation.json').read_text(encoding='utf-8'))
    if observation['schema'] != 1 or observation.get('user_requested_observation') is not True or \
            observation['status'] != 'known_geometry_conflicts_user_observation' or \
            observation['baseline_assets'] != data['baseline_assets']+data['appended_assets'] or \
            observation['plane'] != data['deferred_designs'][0]['rejected_runtime_plan']:
        raise ValueError('MiG-31B观察方案与用户指定布局或资源基线不符')
    candidates = data['planes']+[observation['plane']]
    codes = ['f14a','f18e','f18etg','m31b']
    if data['schema'] != 1 or data['steam_build'] != 9855922 or assets != data['baseline_assets']:
        raise ValueError('三机候选与原54资源基线不符')
    if [p['code'] for p in candidates] != codes or set(codes) & {p['code'] for p in planes}:
        raise ValueError('三机候选集合、顺序或机型编号不符')
    names = data['appended_assets']
    reports = {r['recipe']['name']:r for r in data['resource_reports']}
    if len(names) != len(set(names)) or set(names) != set(reports) or set(names) & set(assets):
        raise ValueError('新增挂架重复或缺少逐资源来源')
    for name, report in reports.items():
        if not all(report.get(k) is True for k in ('asset_roundtrip','renamed_ac7_roundtrip',
                'all_exports_payload_preserved','non_index_bytes_preserved','sockets_and_materials_preserved')):
            raise ValueError('三机挂架往返依据不完整：'+name)
        if report['recipe'] not in data['recipes']['variants']:
            raise ValueError('资源回执与配方不符：'+name)
    total = len(assets)+len(names)
    for plane in candidates:
        groups, plans = plane['groups'], plane['plans']
        if len(groups) != 3 or len(plans) != 8 or plans[0] is not None:
            raise ValueError('三机必须给出完整七组合')
        for mask, plan in enumerate(plans[1:],1):
            if len(plan['selected']) != 3 or len(plan['variants']) != 3 or not 0 <= plan['racks'] <= 7:
                raise ValueError('组合字段不完整')
            used=set()
            counts=[0]*3
            for pool, ids in enumerate(plan['selected']):
                if len(set(ids)) != len(ids) or any(type(i) is not int or not 0 <= i < len(groups[pool]['sockets']) for i in ids):
                    raise ValueError('原生Socket未知、重复或越界')
                if ids and not mask & (1 << pool):
                    raise ValueError('未选兵装不得分配物理位置')
                counts[pool] += len(ids)
                used.update((pool,i) for i in ids)
                variant=plan['variants'][pool]
                if variant != -1:
                    if not len(assets) <= variant < total or not plan['racks'] & (1 << pool):
                        raise ValueError('副本编号越界或挂架未显示')
                    report=reports[names[variant-len(assets)]]
                    if report['source'] != groups[pool]['rack'] or report['recipe']['aircraft'] != plane['code']:
                        raise ValueError('副本不属于原生SWP')
            borrowed=plan.get('borrowed_positions',[])
            expected=[dict(kind=1,pool=0,socket=i) for i in (2,3)] if plane['code']=='m31b' and mask in (6,7) else []
            if borrowed != expected:
                raise ValueError('只允许MiG-31B指定SAAM后腹两点借位')
            for item in borrowed:
                key=(item['pool'],item['socket'])
                if key in used or not mask & (1 << item['kind']):
                    raise ValueError('借位重复占站或兵装未选中')
                used.add(key);counts[item['kind']]+=1
            if any(bool(counts[k]) != bool(mask & (1 << k)) for k in range(3)):
                raise ValueError('所选兵装缺少完整物理映射')
            if sum(counts)>64:
                raise ValueError('组合超过64位显隐容量')
            if mask in (1,2,4) and (borrowed or plan['variants'] != [-1]*3 or plan['racks'] != mask):
                raise ValueError('单挂必须保持原生')
    assets.extend(names)
    planes.extend(deepcopy(candidates))
