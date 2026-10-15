"""枚举抽样布局的逻辑轮转和参考原点间距；结果仅是离线候选，不写入模组。"""
import argparse
from collections import Counter
import itertools
import json
import math
from pathlib import Path

from survey_aircraft_mounts import Survey, reference_bones

# 每个样本只保留一个有代表性的候选；索引指资源 Sockets 顺序，尚非运行时槽身份。
CASES = {
    'f22a': [(0, [4, 5], '消耗型'), (1, [0, 1, 2, 3], '消耗型')],
    'f35c': [(0, None, '消耗型'), (2, None, '消耗型')],
    'pkfa': [(0, None, '消耗型'), (2, None, '固定发射器')],
    'fa44': [(0, None, '垂直发射阵列'), (1, None, '固定发射器')],
    'x02s': [(0, None, '消耗型'), (2, None, '固定发射器')],
    'adf11f': [(0, None, '固定发射器'), (1, None, '消耗型')],
    'mrgn': [(0, None, '固定发射器'), (1, None, '消耗型')],
    'fa27': [(0, None, '消耗型'), (1, [2, 3], '消耗型')],
}


def enumerate_queue(group, indices, role, override=None):
    """穷举所有游标的最大装填窗口，量化重复离架；不把资源槽数当运行时已证实数量。"""
    sockets = group['sockets']
    count = len(sockets)
    selected = list(range(count)) if indices is None else indices
    mapping = override if override is not None else [selected[index % len(selected)] for index in range(count)]
    loaded = group['weapon']['defaults'].get('MaxLoadedCount')
    result = {'configuration': group['configuration'], 'weapon': group['weapon']['class'], 'role': role,
              'resource_socket_count': count, 'chosen_sockets': [sockets[index]['name'] for index in selected],
              'logical_to_chosen_socket': [sockets[index]['name'] for index in mapping],
              'loaded_window': loaded['value'] if loaded else None}
    if loaded and role in ('消耗型', '垂直发射阵列'):
        window = loaded['value']
        if not 0 < window <= count:
            raise ValueError('本实验装填窗口超出资源挂点数，不能直接模拟')
        records = []  # 只记录重复位置的统计，避免输出每颗 ADMM 的长列表。
        for cursor in range(count):
            distribution = Counter(mapping[(cursor+offset) % count] for offset in range(window))
            records.append({'cursor': cursor, 'unique_origins': len(distribution), 'max_rounds_per_origin': max(distribution.values())})
        result['queue_experiment'] = {'cursor_count': count, 'minimum_unique_origins': min(r['unique_origins'] for r in records),
                                      'maximum_rounds_per_origin': max(r['max_rounds_per_origin'] for r in records),
                                      'cursors_with_repeated_origins': [r['cursor'] for r in records if r['unique_origins'] < window]}
    else:
        result['queue_experiment'] = None
        result['queue_not_applied_reason'] = '固定发射器不以弹药冷却隐藏炮体' if role == '固定发射器' else '装填窗口继承原生类默认值，本批没有直接读取'
    return result, [sockets[index] for index in selected]


def distribute_xsdb(group, indices):
    """穷举六槽到四处的左右同侧映射，避免简单取余在队列绕回时只剩两处离架。"""
    sockets = group['sockets']
    window = group['weapon']['defaults']['MaxLoadedCount']['value']
    choices = [[target for target in indices if sockets[target]['reference_position'][1] * source['reference_position'][1] > 0]
               for source in sockets]  # 保持每个原生槽的左右侧，不人为跨机身出弹。
    count, best, best_score = 0, None, None
    for mapping in itertools.product(*choices):
        if set(mapping) != set(indices):
            continue
        count += 1
        distinct = [len({mapping[(cursor+offset) % len(mapping)] for offset in range(window)}) for cursor in range(len(mapping))]
        score = (min(distinct), sum(distinct))  # 首先改善最差游标，再减少总重复。
        if best_score is None or score > best_score:
            best_score, best = score, list(mapping)
    if best is None:
        raise ValueError('没有覆盖四处且左右对应的候选')
    return best, {'surjective_same_side_mappings_examined': count, 'best_minimum_unique_origins': best_score[0],
                  'all_four_distinct_for_every_cursor_possible': best_score[0] == window,
                  'boundary': '仍允许重复离架；不证明真实槽顺序、门体或弹体净空'}


def main():
    """保存各类型的独立候选结果和 F-22 共享骨架比较，不修改运行文件。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--survey', type=Path, required=True)
    parser.add_argument('--manifest', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    data = json.loads(args.survey.read_text(encoding='utf-8'))
    experiments = []
    for sample in data['samples']:
        choices, points = [], []
        for config, indices, role in CASES[sample['code']]:
            group = sample['groups'][config]
            choice, selected = enumerate_queue(group, indices, role)
            if sample['code'] == 'f22a' and config == 1:
                original = choice['queue_experiment']  # 保留简单映射的反例，不将失败候选抹去。
                mapping, search = distribute_xsdb(group, indices)
                choice, selected = enumerate_queue(group, indices, role, mapping)
                choice['naive_round_robin_result'] = original
                choice['mapping_search'] = search
            choices.append(choice)
            points.append(selected)
        distances = [math.dist(a['reference_position'], b['reference_position']) for a in points[0] for b in points[1]
                     if a['reference_position'] is not None and b['reference_position'] is not None]
        experiments.append({'aircraft': sample['name'], 'code': sample['code'], 'choices': choices,
                            'minimum_reference_origin_distance': round(min(distances), 5) if distances else None,
                            'geometry_clearance_verified': False, 'native_launch_order_verified': False,
                            'animation_replay_suppressed_in_game': False})
    survey = Survey([args.manifest])
    bones = []  # 不比较资源 GUID，而比较实际解出的骨名、父序和参考姿态。
    for number in (1, 2):
        name = f'f22a_SWP{number}_Skeleton'
        document = survey.load(name)
        bones.append(reference_bones(document, next(e for e in document['Exports'] if e['ObjectName'] == name)))
    common = {'aircraft': 'F-22A', 'weapons': ['QAAM', 'XSDB'], 'bone_count': len(bones[0]),
              'reference_bones_exactly_equal': bones[0] == bones[1], 'shared_channel': 'spwc',
              'does_not_prove': ['动画采样曲线相同', '运行时不重置舱门', '两套舱门网格可直接叠加'],
              'candidate': '同一弹仓保留一个舱门呈现对象及控制状态，兵装切换不重新发出开关请求'}
    report = {'kind': '离线布局和轮转枚举实验', 'gameplay_tested': False, 'candidate_installed': False,
              'samples': experiments, 'shared_bay_comparison': common,
              'bay_acceptance_cases': ['关闭时来回切换仍关闭', '开启时来回切换仍开启',
                  '开启过程中切换不归零或重放，沿原进度继续', '关闭过程中切换不反向重启',
                  'MSL/SP 切换不误改另一弹仓的 misc 通道', '检查点与补给按自身生命周期恢复，不沿用旧世界对象'],
              'limitations': ['全部数量基于原版资源；未完成各机型运行时队列对照',
                  '重复离架仅为研究候选，本轮未扩展到已安装模组', '参考原点有距离不代表弹体或舱门无穿插']}
    with args.output.open('x', encoding='utf-8') as stream:
        json.dump(report, stream, ensure_ascii=False, indent=2)
        stream.write('\n')
    for experiment in experiments:
        print(experiment['aircraft'], '参考最小间距', experiment['minimum_reference_origin_distance'],
              [choice['queue_experiment'] for choice in experiment['choices']])
    print('F-22 共享参考骨架', common['reference_bones_exactly_equal'])


if __name__ == '__main__':
    main()
