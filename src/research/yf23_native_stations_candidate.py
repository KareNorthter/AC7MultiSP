"""YF-23 原生站位研究草案；多个活动供位尚未接入运行。"""
import json


def prepare_yf23_candidate(root):
    baseline = json.loads((root / 'evidence/mount-expansion-batch3.json').read_text(encoding='utf-8'))
    matches = [aircraft for aircraft in baseline['aircraft'] if aircraft['code'] == 'yf23']
    if len(matches) != 1:
        raise ValueError('YF-23原生机体记录不唯一')
    aircraft = matches[0]
    expected = [('SWP1', 'plwp_hvaa_a0_C'), ('SWP2', 'plwp_4aam_a0_C'),
                ('SWP3', 'plwp_ugb_a0_C')]
    if [(group['group'], group['weapon']['class']) for group in aircraft['groups']] != expected:
        raise ValueError('YF-23原生兵装资源改变')
    proof = json.loads((root / 'evidence/yf23-native-stations-static-20261029.json').read_text(encoding='utf-8'))
    if proof['aircraft'] != 'yf23' or proof['status'] != 'offline_candidate_not_enabled':
        raise ValueError('YF-23静态候选记录不符')
    single = [
        {'weapons': ['HVAA'], 'assignments': [{'weapon': 'HVAA', 'provider': 'SWP1', 'sockets': ['sp12', 'sp11']}]},
        {'weapons': ['4AAM'], 'assignments': [{'weapon': '4AAM', 'provider': 'SWP2', 'sockets': ['sp21', 'sp22', 'sp23', 'sp24']}]},
        {'weapons': ['UGB'], 'assignments': [{'weapon': 'UGB', 'provider': 'SWP3', 'sockets': ['sp31', 'sp32', 'sp33', 'sp34']}]},
    ]
    multi = proof['cases']
    expected_weapons = [['HVAA', '4AAM'], ['HVAA', 'UGB'], ['4AAM', 'UGB'], ['HVAA', '4AAM', 'UGB']]
    if ([case['weapons'] for case in multi] != expected_weapons or
            any(not case['all_projectile_obbs_separated_101_frames'] for case in multi)):
        raise ValueError('YF-23候选组合或弹体间距依据改变')
    groups = []
    meshes = ['w_saam_a0_SM', 'w_4aam_a0_SM', 'w_ugb_a0_SM']
    for index, item in enumerate(aircraft['groups']):
        rack = f'yf23_SWP{index + 1}'
        sockets = [f'sp{index + 1}{number}' for number in range(1, 3 if index == 0 else 5)]
        window = item['weapon']['defaults']['MaxLoadedCount']['value']
        if window != [2, 4, 2][index]:
            raise ValueError('YF-23原生待发窗口改变')
        groups.append({'class': expected[index][1], 'rack': rack, 'mesh': meshes[index],
                       'sockets': sockets, 'window': window, 'skeletal': True, 'fixed': False,
                       'launcher': False, 'skeleton': rack + '_Skeleton', 'anim': rack + '_AnimBP_C',
                       'rack_package': '/Game/Vehicles/Aircraft/yf23/' + rack,
                       'mesh_package': '/Game/Vehicles/Weapons/' + meshes[index].removesuffix('_SM') + '/' + meshes[index],
                       'bones': [{'name': 'Body', 'parent': -1}, {'name': 'SWPCover_04', 'parent': 0}]})
    cases = single + [{'weapons': case['weapons'], 'assignments': case['assignments']} for case in multi]
    plans = [None] * 8
    names = ['HVAA', '4AAM', 'UGB']
    for case in cases:
        mask = sum(1 << names.index(name) for name in case['weapons'])
        selected = [[], [], []]
        for row in case['assignments']:
            kind = names.index(row['weapon'])
            if row['provider'] != f'SWP{kind + 1}':
                raise ValueError('YF-23必须使用原生供位')
            selected[kind] = [groups[kind]['sockets'].index(socket) for socket in row['sockets']]
        plans[mask] = {'selected': selected, 'racks': mask if len(case['weapons']) == 1 else 2,
                       'variants': [-1, -1, -1]}
    if any(plan is None for plan in plans[1:]):
        raise ValueError('YF-23七组合不完整')
    return {'code': 'yf23', 'local_mechanism': True, 'local_pools': [0, 1, 2],
            'groups': groups, 'plans': plans}
