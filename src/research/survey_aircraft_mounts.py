"""对已解析的原版资源做挂载抽样实验；不连接进程、不修改资源、不运行游戏测试。"""
import argparse
import base64
import hashlib
import itertools
import json
import math
from pathlib import Path
import struct

from summarize_mount_assets import properties, resolve, value

# 按结构选择代表机型，名称只用于报告，资源身份由代码和真实引用共同确定。
AIRCRAFT = {'f22a': 'F-22A', 'f35c': 'F-35C', 'pkfa': 'Su-57', 'fa44': 'CFA-44',
            'x02s': 'X-02S', 'adf11f': 'ADF-11F', 'mrgn': 'ADFX-01', 'fa27': 'XFA-27'}


def multiply(a, b):
    """四元数乘法，父旋转乘子旋转；输入顺序为 x/y/z/w。"""
    x, y, z, w = a
    u, v, t, s = b
    return (w*u+x*s+y*t-z*v, w*v-x*t+y*s+z*u, w*t+x*v-y*u+z*s, w*s-x*u-y*v-z*t)


def rotate(q, point):
    """单位四元数旋转局部位置，不使用欧拉角近似。"""
    result = multiply(multiply(q, (*point, 0)), (-q[0], -q[1], -q[2], q[3]))
    return result[:3]


def vector(prop):
    """读取嵌套向量，并接受解析器对正负零的字符串表示。"""
    while isinstance(prop, dict) and len(prop) == 1:
        prop = next(iter(prop.values()))
    return tuple(float(prop[axis]) for axis in ('X', 'Y', 'Z'))


def reference_bones(document, export):
    """只解析此批共同确认的 cooked Skeleton 参考骨架头，严查名称、父序和变换。"""
    raw = base64.b64decode(export['Extras'], validate=True)  # 原始字节仅在内存使用。
    count = struct.unpack_from('<i', raw)[0]
    if not 0 < count <= 1000 or count != len(properties(export)['BoneTree']):
        raise ValueError('参考骨架数量与 BoneTree 不符')
    if struct.unpack_from('<i', raw, 4 + count*12)[0] != count:
        raise ValueError('参考姿态数量与骨骼数量不符')
    bones = []  # 每根骨骼保留组件空间姿态，供挂点换算和通道归属使用。
    for index in range(count):
        name_index, number, parent = struct.unpack_from('<iii', raw, 4 + index*12)
        if not (0 <= name_index < len(document['NameMap']) and number >= 0 and -1 <= parent < index):
            raise ValueError('参考骨架名称或父序无效')
        if (index == 0) != (parent == -1):
            raise ValueError('本批要求单根参考骨架')
        name = document['NameMap'][name_index] + (f'_{number-1}' if number else '')
        transform = struct.unpack_from('<10f', raw, 8 + count*12 + index*40)
        quaternion, position, scale = transform[:4], transform[4:7], transform[7:]
        if not all(math.isfinite(v) for v in transform) or abs(sum(v*v for v in quaternion)-1) > .001:
            raise ValueError('参考姿态不是有限单位旋转')
        if any(abs(v-1) > .0001 for v in scale):
            raise ValueError('本批未支持非单位缩放，不继续估算')
        chain = [name]  # 此挂点可受到父链上所有动画通道影响。
        if parent >= 0:
            ancestor = bones[parent]
            position = tuple(a+b for a, b in zip(ancestor['position'], rotate(ancestor['quaternion'], position)))
            quaternion = multiply(ancestor['quaternion'], quaternion)
            chain += ancestor['chain']
        bones.append({'name': name, 'parent': parent, 'position': position, 'quaternion': quaternion, 'chain': chain})
    if len({bone['name'] for bone in bones}) != count:
        raise ValueError('参考骨架名称重复')
    return bones


class Survey:
    """将多个提取批次按资源名关联，显式保留更新包与基础包来源。"""
    def __init__(self, manifests):
        self.assets = {}  # 每个名称可有基础和更新包两个版本，不断言第三方覆盖情况。
        for manifest in manifests:
            for entry in json.loads(manifest.read_text(encoding='utf-8')):
                name = Path(entry['asset']).stem
                self.assets.setdefault(name, []).append(entry)
        self.used = {}  # 最终报告仅列实际读取资源的摘要。
        self.cache = {}

    def load(self, name):
        """优先分析原版更新包；运行时覆盖仍须现场确认。"""
        if name not in self.cache:
            entries = self.assets[name]
            entry = sorted(entries, key=lambda x: '_0_P.' in x['pak'], reverse=True)[0]
            document = json.loads(Path(entry['json']).read_text(encoding='utf-8-sig'))
            self.cache[name] = document
            self.used[name] = {'pak': entry['pak'], 'asset': entry['asset'], 'sha256': entry['encrypted_sha256'],
                               'json_sha256': hashlib.sha256(Path(entry['json']).read_bytes()).hexdigest()}
        return self.cache[name]

    def weapon(self, name):
        """沿蓝图父类合并明确默认值；原生父类缺省项标为未知，不能猜测。"""
        current = name.removesuffix('_C')
        chain, explicit = [], {}
        fields = ('WeaponID', 'MaxLoadedCount', 'LoadTime', 'bFireAllInOneShot')
        while current in self.assets:
            if current in chain:
                raise ValueError('兵装继承循环')
            chain.append(current)
            document = self.load(current)
            default = next(e for e in document['Exports'] if e['ObjectName'] == 'Default__'+current+'_C')
            for key, item in properties(default).items():
                if key in fields and key not in explicit:
                    explicit[key] = {'value': item, 'source': current}
            cls = next(e for e in document['Exports'] if e['ObjectName'] == current+'_C')
            current = resolve(document, cls['SuperIndex']).removesuffix('_C')
        return {'class': name, 'blueprint_chain': chain, 'unparsed_parent': current, 'defaults': explicit}

    def sockets(self, name, channels):
        """按资源 Sockets 引用数组读取挂点，骨架先转换到组件参考姿态。"""
        document = self.load(name)
        export = next(e for e in document['Exports'] if e['ObjectName'] == name)
        props = properties(export)
        skeleton = name.endswith('_Skeleton')
        bones = reference_bones(document, export) if skeleton else []
        by_name = {bone['name']: bone for bone in bones}
        sockets = []
        for reference in props.get('Sockets', []):
            if reference <= 0:
                raise ValueError('挂点不是本资源导出')
            socket = properties(document['Exports'][reference-1])
            if not socket['SocketName'].lower().startswith(('sp', 'ws')):
                continue
            location = vector(socket['RelativeLocation']) if 'RelativeLocation' in socket else None  # 省略的默认值不当成已解析的零坐标。
            parent = socket.get('BoneName')
            if skeleton:
                bone = by_name[parent]
                if location is not None:
                    location = tuple(a+b for a, b in zip(bone['position'], rotate(bone['quaternion'], location)))
                driven = sorted({channels[n] for n in bone['chain'] if n in channels})
            else:
                driven = []
            sockets.append({'name': socket['SocketName'], 'bone': parent, 'reference_position': [round(v, 5) for v in location] if location is not None else None,
                            'position_status': '已解析并换算' if location is not None else '资源省略 RelativeLocation，未读取引擎默认值',
                            'ancestor_animation_channels': driven})
        return {'asset': name, 'bone_count': len(bones), 'sockets': sockets,
                'animation_channels': sorted({channels[b['name']] for b in bones if b['name'] in channels})}

    def aircraft(self, code, channels, base_configs):
        """形成三个配置的结构、参考坐标和控制通道快照。"""
        document = self.load('AcePlayerPawn_'+code)
        activator = next(e for e in document['Exports'] if e['ObjectName'] == 'PlayerWeaponActivator')
        props = properties(activator)
        configs = dict(base_configs)
        for key, config in props.items():
            if key.startswith('SpecialWeaponConfig'):
                configs[key] = resolve(document, config['WeaponClass'])
        parts = self.load('PartsAnimationConfig_'+code)
        part_props = value(parts['Exports'][0]['Table']['Data'][0])
        groups = []
        for index in range(1, 4):
            export = next(e for e in document['Exports'] if e['ObjectName'] == f'SWP{index}_GEN_VARIABLE')
            component = properties(export)
            mesh_key = 'SkeletalMesh' if 'SkeletalMesh' in component else 'StaticMesh'
            mesh = resolve(document, component[mesh_key])
            socket_asset = mesh+'_Skeleton' if mesh_key == 'SkeletalMesh' else mesh
            socket_info = self.sockets(socket_asset, channels)
            weapon = self.weapon(configs[f'SpecialWeaponConfig{index}'])
            parent_nodes = []  # 原始 SCS 父级引用仍保存，跨组件比较前需人工核对。
            for node in document['Exports']:
                p = properties(node)
                if p.get('ComponentTemplate') and resolve(document, p['ComponentTemplate']) == export['ObjectName']:
                    parent_nodes.append(p.get('ParentComponentOrVariableName'))
            animation = None
            if mesh_key == 'SkeletalMesh':
                anim_document = self.load(mesh+'_AnimBP')
                default = next(e for e in anim_document['Exports'] if e['ObjectName'].startswith('Default__'))
                blend = next(v for k, v in properties(default).items() if k.startswith('AnimGraphNode_FlightEngineBlend'))
                sequence = resolve(anim_document, blend['SequenceConfig']['Sequence'])
                sequence_document = self.load(sequence)
                sequence_props = properties(sequence_document['Exports'][0])
                animation = {'node': 'AnimNode_FlightEngineBlend', 'bone_config': resolve(anim_document, blend['BoneConfig']),
                             'sequence': sequence, 'frames': sequence_props['NumFrames'], 'asset_length': sequence_props['SequenceLength'],
                             'boundary': '资源采样长度不代表舱门实际播放耗时；未读取游戏中的控制输出值'}
            groups.append({'configuration': index-1, 'component_class': resolve(document, export['ClassIndex']),
                           'parent_references': parent_nodes, 'component_transform_overrides': {k:v for k,v in component.items() if k.startswith('Relative')},
                           'mesh': mesh, 'weapon': weapon, **socket_info, 'animation': animation})
        # 仅比较参考姿态挂点原点距离，不以此宣称武器体积无穿插或骨架已可互换。
        pairs = []
        for first, second in itertools.combinations(groups, 2):
            distances = [(math.dist(a['reference_position'], b['reference_position']), a['name'], b['name'])
                         for a in first['sockets'] for b in second['sockets']
                         if a['reference_position'] is not None and b['reference_position'] is not None]
            closest = sorted(distances)[:4]
            pairs.append({'configs': [first['configuration'], second['configuration']],
                          'minimum_distance': round(closest[0][0], 5) if closest else None,
                          'pairs_within_1_resource_unit': sum(d <= 1 for d, _, _ in distances),
                          'closest': [[round(d,5), a, b] for d,a,b in closest]})
        return {'code': code, 'name': AIRCRAFT[code], 'groups': groups, 'reference_origin_comparison': pairs,
                'cover_options': {k:v for k,v in part_props.items() if 'cover' in k.lower()},
                'body_missile_sockets': self.sockets(code+'_Skeleton', channels)['sockets']}


def main():
    """输出可审阅的精简结构和离线实验结果，拒绝覆盖既有报告。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--manifest', type=Path, action='append', required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    survey = Survey(args.manifest)
    table = survey.load('PlaneBodyConfig')
    rows = [value(p) for p in table['Exports'][0]['Table']['Data']]
    channels = {row['BoneName']: row['FlightEngineOuputName'] for row in rows}
    base = survey.load('AcePlayerPawn')
    activator = next(e for e in base['Exports'] if e['ObjectName'] == 'PlayerWeaponActivator')
    configs = {key: resolve(base, item['WeaponClass']) for key,item in properties(activator).items() if key.startswith('SpecialWeaponConfig')}
    samples = [survey.aircraft(code, channels, configs) for code in AIRCRAFT]
    result = {'steam_build':9855922, 'method':'原版资源解析、参考骨架复合变换、通道关联和挂点原点距离枚举',
              'game_started':False, 'gameplay_tested':False, 'runtime_files_modified':False,
              'animation_requirement':'同一弹仓内切换 SP 保留舱门姿态/进度，不重复触发开关；发射、检查点和补给另行处理',
              'samples':samples,'sources':survey.used,
              'limits':['不含第三方 PAK 覆盖','参考姿态不是实时开舱状态','原点距离不是网格碰撞','资源挂点顺序不代替运行时槽映射','未修改安装候选']}
    with args.output.open('x',encoding='utf-8') as stream:
        json.dump(result,stream,ensure_ascii=False,indent=2)
        stream.write('\n')
    for sample in samples:
        print(sample['name'],[(g['weapon']['class'],len(g['sockets']),g['animation_channels']) for g in sample['groups']])
        print('近距离原点',sample['reference_origin_comparison'])


if __name__ == '__main__':
    main()
