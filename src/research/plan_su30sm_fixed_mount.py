"""从既有 Su-30SM 资源摘要生成四翼下加两机腹的挂点研究方案；不修改游戏。"""

import argparse
import hashlib
import json
import math
import struct
from pathlib import Path


def read_vector(socket, property_name, axes):
    """读取摘要保留的嵌套向量，未知格式明确报错。"""
    value = socket[property_name][property_name]  # UAssetAPI 摘要中的结构体包装。
    return [float(value[axis]) for axis in axes]


def mesh_sockets(assets, filename):
    """只接受唯一的指定网格，不猜测补丁覆盖顺序。"""
    matches = [asset for asset in assets if asset['asset'].endswith('/' + filename + '.uasset')]
    if len(matches) != 1:
        raise ValueError(f'资源不是唯一条目：{filename}')
    return {socket['SocketName']: socket for socket in matches[0]['sockets']}


def build_plan(asset_path, snapshot_path):
    """保留六个逻辑槽，输出显式的多对一挂点映射及其证据边界。"""
    raw_assets = asset_path.read_bytes()  # 来源哈希用于追溯已有资源摘要。
    assets = json.loads(raw_assets)['assets']
    six_sockets = mesh_sockets(assets, 'su30sm_SWP1_SM')
    lagm_sockets = mesh_sockets(assets, 'su30sm_SWP3_SM')
    # 使用原有四个翼下位置与 LAGM 自己的低位一对，不擅自新增任意坐标。
    mappings = {
        '6AAM': {'sp11': 'sp11', 'sp12': 'sp12', 'sp13': 'sp13',
                 'sp14': 'sp14', 'sp15': 'sp13', 'sp16': 'sp14'},
        'LAGM': {'sp31': 'sp35', 'sp32': 'sp36', 'sp33': 'sp35',
                 'sp34': 'sp36', 'sp35': 'sp35', 'sp36': 'sp36'},
    }
    result = {'steam_build': 9855922, 'aircraft': 'Su-30SM', 'status': '离线方案，尚未接入运行时',
              'source_sha256': hashlib.sha256(raw_assets).hexdigest(),
              'user_accepts_repeated_launch_origin': True, 'weapons': {},
              'runtime_modified': False, 'native_launch_origin_verified': False}
    for weapon, mapping in mappings.items():
        # LAGM 装填上限仍为二；不能因压缩到两个位置就删掉原生轮转槽。
        sockets = six_sockets if weapon == '6AAM' else lagm_sockets
        window = 6 if weapon == '6AAM' else 2
        destinations = sorted(set(mapping.values()))
        positions = {name: {'location': read_vector(sockets[name], 'RelativeLocation', ('X', 'Y', 'Z')),
                            'rotation': read_vector(sockets[name], 'RelativeRotation', ('Pitch', 'Yaw', 'Roll'))}
                     for name in destinations}
        # 每个光标下的装填窗口仅用于描述方案，不假装是游戏发射实验。
        order = sorted(mapping)
        windows = [{'cursor': cursor, 'logical_slots': [order[(cursor + i) % 6] for i in range(window)],
                    'physical_positions': [mapping[order[(cursor + i) % 6]] for i in range(window)]}
                   for cursor in range(6)]
        result['weapons'][weapon] = {'native_configuration': 0 if weapon == '6AAM' else 2,
                                    'native_slot_count_expected': 6, 'loaded_window_expected': window,
                                    'mapping': mapping, 'physical_positions': positions,
                                    'queue_windows': windows}
    # 对旧现场快照做结构分析：缓存平移是否接近对应 Socket；这不能证明发射会读取它。
    raw_snapshot = snapshot_path.read_bytes()
    snapshot = json.loads(raw_snapshot)
    group = snapshot['groups']['SP']
    if group['class']['name'] != 'plwp_6aam_r0_C' or group['count'] != 6:
        raise ValueError('历史输入不是六槽 6AAM 快照')
    comparisons = []  # 只记录可解释的变换候选，未知尾部数据不复制到方案。
    for slot in group['slots']:
        slot_bytes = bytes.fromhex(slot['raw'])
        socket_name = slot['socket_name_candidate']
        xyz = list(struct.unpack_from('<3f', slot_bytes, 0x20))
        reference = read_vector(six_sockets[socket_name], 'RelativeLocation', ('X', 'Y', 'Z'))
        quaternion = list(struct.unpack_from('<4f', slot_bytes, 0x10))
        comparisons.append({'slot': slot['index'], 'socket': socket_name,
                            'translation_candidate_at_0x20': xyz,
                            'asset_translation': reference,
                            'distance_in_resource_units': math.dist(xyz, reference),
                            'quaternion_candidate_at_0x10': quaternion,
                            'quaternion_norm': math.sqrt(sum(value * value for value in quaternion)),
                            'scale_candidate_at_0x30': list(struct.unpack_from('<3f', slot_bytes, 0x30)),
                            'unknown_indices_at_0x40_0x44': list(struct.unpack_from('<2i', slot_bytes, 0x40))})
    result['historical_slot_analysis'] = {
        'source_file': snapshot_path.name, 'sha256': hashlib.sha256(raw_snapshot).hexdigest(),
        'version': '0.1.9', 'samples': comparisons,
        'boundary': '相近坐标和单位四元数支持缓存变换解释；尚未确认读取时机及两个索引用途。'}
    result['required_research'] = ['实际离架使用缓存变换还是实时 Socket', '发射和装填分别如何选择显示组件',
                                   '重建队列旋转与持久挂点序号的对应', 'LAGM 六槽两装填窗口的现场核对',
                                   '挂架网格整体重叠与未用挂架的可接受性']
    return result


def main():
    """将方案写入新文件，禁止覆盖历史证据。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--assets', type=Path, required=True)
    parser.add_argument('--historical-6aam', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    plan = build_plan(args.assets, args.historical_6aam)
    with args.output.open('x', encoding='utf-8') as stream:
        json.dump(plan, stream, ensure_ascii=False, indent=2)
        stream.write('\n')
    print(json.dumps({'output': str(args.output), 'runtime_modified': False,
                      'display_positions': {name: len(data['physical_positions'])
                                            for name, data in plan['weapons'].items()}}, ensure_ascii=False))


if __name__ == '__main__':
    main()
