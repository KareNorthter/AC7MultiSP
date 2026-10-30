"""Su-47 单架七组合草案；不接入运行或改变现有生成表。"""
import json


def prepare_su47_candidate(root):
    header = (root / 'src/animated_mount_data.hpp').read_text(encoding='utf-8')
    prefix, suffix = 'R"AC7ANIM(', ')AC7ANIM"'
    if header.count(prefix) != 1 or header.count(suffix) != 1:
        raise ValueError('Su-47动画资源描述格式改变')
    data = json.loads(header.split(prefix, 1)[1].split(suffix, 1)[0])
    matches = [plane for plane in data['planes'] if plane['code'] == 'su47']
    if len(matches) != 1:
        raise ValueError('Su-47资源描述不唯一')
    original = matches[0]['groups']
    expected = [
        ('plwp_saam_r0_C', 'su47_SWP1', 'w_saam_r0_SM', ['sp11', 'sp12']),
        ('plwp_ugb_r0_C', 'su47_SWP2', 'w_ugb_r0_SM', ['sp21', 'sp22', 'sp23', 'sp24']),
        ('plwp_qaam_r0_C', 'su47_SWP3', 'w_qaam_r0_SM', ['sp31', 'sp32', 'sp33', 'sp34']),
    ]
    if len(original) != 3:
        raise ValueError('Su-47原生资源组不完整')
    groups = []
    for index, item in enumerate(original):
        if ((item['class'], item['rack'], item['mesh'], item['sockets']) != expected[index]
                or item['componentkind'] != 'skeletal' or item['window'] != 2):
            raise ValueError('Su-47原生池、Socket或装填窗口改变')
        groups.append({'class': item['class'], 'rack': item['rack'], 'mesh': item['mesh'],
                       'sockets': list(item['sockets']), 'window': item['window'],
                       'skeletal': True, 'fixed': False, 'launcher': False,
                       'rack_package': item['packages']['rack'], 'mesh_package': item['packages']['mesh']})
    proof = json.loads((root / 'evidence/su47-one-rack-static-20261029.json').read_text(encoding='utf-8'))
    checked = proof['projectile_triangle_check']
    if (proof['aircraft'] != 'su47' or proof['status'] != 'offline_candidate_not_enabled'
            or checked['frames'] != 101 or len(checked['counts']) != 101 or any(checked['counts'])):
        raise ValueError('Su-47单架弹体几何依据改变')
    combos = {3: ('SAAM+UGB', (0, 1)), 5: ('SAAM+QAAM', (0, 2)),
              6: ('UGB+QAAM', (1, 2)), 7: ('SAAM+UGB+QAAM', (0, 1, 2))}
    names = ['SAAM', 'UGB', 'QAAM']
    plans = [None]
    for mask in range(1, 8):
        selected = [[], [], []]
        borrowed = []
        full_borrowed = 0
        if mask in combos:
            label, kinds = combos[mask]
            assignments = proof['candidate_plans'][label]
            if set(assignments) != {names[kind] for kind in kinds}:
                raise ValueError('Su-47单架组合兵装不符')
            used = set()
            for kind in kinds:
                for socket in assignments[names[kind]]:
                    position = groups[1]['sockets'].index(socket)
                    if position in used:
                        raise ValueError('Su-47单架Socket重复')
                    used.add(position)
                    if kind == 1:
                        selected[kind].append(position)
                    else:
                        borrowed.append({'kind': kind, 'pool': 1, 'socket': position})
                        full_borrowed |= 1 << kind
            racks = 2
        else:
            kind = {1: 0, 2: 1, 4: 2}[mask]
            selected[kind] = list(range(len(groups[kind]['sockets'])))
            racks = mask
        plan = {'selected': selected, 'racks': racks, 'variants': [-1, -1, -1]}
        if borrowed:
            plan['borrowed_positions'] = borrowed
            plan['full_borrowed_kinds'] = full_borrowed
        plans.append(plan)
    return {'code': 'su47', 'local_mechanism': True, 'local_pool': 1,
            'groups': groups, 'plans': plans}
