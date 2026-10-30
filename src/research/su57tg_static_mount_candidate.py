"""Su-57 TGM 静态供位布局草案；由接入方显式调用，当前不注册运行。"""
import json


def prepare_su57tg_candidate(root):
    """从既有资源描述整理原生池及七组合，不改变原有生成表。"""
    header = (root / 'src/animated_mount_data.hpp').read_text(encoding='utf-8')
    prefix = 'R"AC7ANIM('
    suffix = ')AC7ANIM"'
    if header.count(prefix) != 1 or header.count(suffix) != 1:
        raise ValueError('TGM动画资源描述格式改变')
    data = json.loads(header.split(prefix, 1)[1].split(suffix, 1)[0])
    matches = [plane for plane in data['planes'] if plane['code'] == 'su57tg']
    if len(matches) != 1:
        raise ValueError('TGM原生资源描述不唯一')
    source = matches[0]
    expected = [
        ('plwp_4aam_r1_C', 'su57tg_SWP1', 'w_4aam_r1_SM', 4, True),
        ('plwp_hcaa_r1_C', 'su57tg_SWP2', 'w_4aam_r1_SM', 2, True),
        ('plwp_lacm_r0_C', 'su57tg_SWP3_SM', 'w_lacm_r0_closed_SM', 2, False),
    ]
    expected_sockets = [
        ['sp13', 'sp14', 'sp11', 'sp12'],
        ['sp23', 'sp24', 'sp21', 'sp22'],
        ['sp31', 'sp32', 'sp33', 'sp34'],
    ]
    groups = []
    for index, group in enumerate(source['groups']):
        skeletal = group['componentkind'] == 'skeletal'
        actual = (group['class'], group['rack'], group['mesh'], group['window'], skeletal)
        if actual != expected[index] or group['sockets'] != expected_sockets[index]:
            raise ValueError('TGM原生池、武器资源或Socket改变')
        groups.append({'class': group['class'], 'rack': group['rack'], 'mesh': group['mesh'],
                       'sockets': list(group['sockets']), 'window': group['window'],
                       'skeletal': skeletal, 'fixed': False, 'launcher': False,
                       'rack_package': group['packages']['rack'], 'mesh_package': group['packages']['mesh']})
    if len(groups) != 3 or len(source['plans']) != 8:
        raise ValueError('TGM原生资源组或七组合不完整')
    plans = [None]
    for mask in range(1, 8):
        prior = source['plans'][mask]
        # 三SP按作者要求保留后部两枚LACM，前部4AAM和HCAA各一枚。
        if mask == 7:
            if prior['selected'] != [[0, 1], [2], [3]] or prior['providers'] != [2, 2, 2]:
                raise ValueError('TGM三SP原始候选已改变')
            prior = dict(prior, selected=[[0], [1], [2, 3]])
        selected = [[], [], []]
        borrowed = []
        full_borrowed = 0
        for kind in range(3):
            positions = prior['selected'][kind]
            provider = prior['providers'][kind]
            if not positions:
                continue
            if not mask & (1 << kind) or provider not in (kind, 2):
                raise ValueError('TGM旧方案供位组不符')
            if provider == kind:
                selected[kind] = list(positions)
            else:
                if prior['native'] or provider != 2:
                    raise ValueError('TGM借位来源不符')
                borrowed.extend({'kind': kind, 'pool': provider, 'socket': pos} for pos in positions)
                full_borrowed |= 1 << kind
        plan = {'selected': selected, 'racks': prior['racks'], 'variants': [-1, -1, -1]}
        if borrowed:
            plan['borrowed_positions'] = borrowed
            plan['full_borrowed_kinds'] = full_borrowed
        plans.append(plan)
    return {'code': 'su57tg', 'groups': groups, 'plans': plans}
