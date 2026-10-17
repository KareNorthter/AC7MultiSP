"""只读采集 ADF-11F UAV 本体、子弹、控制字段与原生函数；原件仅写入 Temp。"""

import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys
from datetime import datetime


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('pid', type=int)
    parser.add_argument('--project-root', type=Path, required=True)
    parser.add_argument('--output-dir', type=Path, required=True)
    parser.add_argument('--native', action='store_true', help='保存已核对可执行节中的原生函数')
    parser.add_argument('--range', action='append', default=[], help='原生连续片段 RVA:长度，十六进制；需 --native')
    args = parser.parse_args()
    temporary = Path(__file__).resolve().parents[2] / 'Temp'
    if not args.output_dir.resolve().is_relative_to(temporary.resolve()):
        raise ValueError('原始采样必须位于本研究工作区 Temp')
    if args.output_dir.exists():
        raise ValueError('输出目录已存在，请使用新名称')
    if args.range and not args.native:
        raise ValueError('--range 必须与 --native 一起使用')
    sys.path[:0] = [str(args.project_root / 'src'), str(args.project_root / 'tools')]
    from inspect_native import GameMemory
    from capstone import Cs, CS_ARCH_X86, CS_MODE_64
    memory = GameMemory(args.pid)  # 仅申请查询和读取；核对 Ace7Game.exe 的完整路径。
    try:
        def unpack(address, fmt):
            return struct.unpack(fmt, memory.read(address, struct.calcsize(fmt)))

        def pointer(address):
            return unpack(address, '<Q')[0]

        signature = bytes.fromhex('488b8160020000488b88f00000008b8118010000c3')
        if memory.read(memory.base + 0x882390, len(signature)) != signature:
            raise ValueError('游戏构建签名不匹配')
        names = pointer(memory.base + 0x3bc7070)
        table, maximum, count = unpack(memory.base + 0x3bcfdb0, '<Qii')
        if not 0 < count <= maximum < 3000000:
            raise ValueError('对象表范围无效')
        raw = memory.read(table, count * 24)
        known = {struct.unpack_from('<Q', raw, i * 24)[0]: i for i in range(count)}
        known.pop(0, None)
        name_cache = {}
        identity_cache = {}

        def fname(index):
            if not 0 <= index < 4000000:
                raise ValueError('名称索引无效')
            if index not in name_cache:
                entry = pointer(pointer(names + index // 0x4000 * 8) + index % 0x4000 * 8)
                value = memory.read(entry, 256)
                if value[0] & 1:
                    end = next(i for i in range(16, len(value), 2) if value[i:i+2] == bytes(2))
                    name_cache[index] = value[16:end].decode('utf-16le')
                else:
                    name_cache[index] = value[16:].split(b'\0')[0].decode('utf-8')
            return name_cache[index]

        def describe(address):
            if address not in known:
                return {'address': hex(address), 'uobject': False}
            if address not in identity_cache:
                vtable, flags, index, cls, name, number, outer = unpack(address, '<QIiQIIQ')
                if index != known[address] or cls not in known:
                    raise ValueError('对象身份已变化，请在暂停状态重新采集')
                identity_cache[address] = dict(address=hex(address), uobject=True, index=index,
                    serial=struct.unpack_from('<i', raw, index * 24 + 16)[0], flags=flags,
                    name=fname(name), number=number, class_address=hex(cls),
                    class_name=fname(unpack(cls + 0x18, '<I')[0]), outer=hex(outer),
                    vtable_rva=hex(vtable - memory.base))
            return identity_cache[address]

        def reflect(address):
            fields = {}
            methods = {}
            chain = []
            cls = pointer(address + 0x10)
            seen = set()
            while cls:
                if cls in seen or len(chain) >= 64:
                    raise ValueError('反射继承链异常')
                seen.add(cls)
                chain.append(describe(cls)['name'])
                child = pointer(cls + 0x38)
                while child:
                    if child in seen or len(seen) > 8192:
                        raise ValueError('反射字段链异常')
                    seen.add(child)
                    item = describe(child)
                    name, kind = item['name'], item['class_name']
                    if kind.endswith('Property') and name not in fields:
                        offset, size = unpack(child + 0x44, '<I')[0], unpack(child + 0x34, '<I')[0]
                        if offset > 0x20000 or not 0 < size <= 0x4000:
                            raise ValueError('反射字段边界异常')
                        value = {'kind': kind, 'offset': hex(offset), 'size': size}
                        if kind in ('ObjectProperty', 'ClassProperty'):
                            value['object'] = describe(pointer(address + offset))
                        elif kind in ('FloatProperty', 'IntProperty', 'ByteProperty'):
                            value['value'] = unpack(address + offset,
                                {'FloatProperty': '<f', 'IntProperty': '<i', 'ByteProperty': '<B'}[kind])[0]
                        elif kind == 'BoolProperty':
                            value['raw'] = memory.read(address + offset, size).hex()
                            value['property_tail'] = memory.read(child + 0x70, 8).hex()
                        elif kind == 'ArrayProperty':
                            data, length, capacity = unpack(address + offset, '<Qii')
                            if not 0 <= length <= capacity <= 100000:
                                raise ValueError('反射数组边界异常')
                            inner = pointer(child + 0x70)
                            inner_kind = describe(inner).get('class_name')
                            value.update(data=hex(data), count=length, capacity=capacity, inner_kind=inner_kind)
                            if inner_kind in ('ObjectProperty', 'ClassProperty') and length <= 1024:
                                value['objects'] = [describe(pointer(data + i * 8)) for i in range(length)]
                        elif kind == 'NameProperty' and size == 8:
                            index, number = unpack(address + offset, '<II')
                            value.update(name=fname(index), number=number,
                                         raw=memory.read(address + offset, size).hex())
                        elif kind in ('EnumProperty', 'StructProperty') and size <= 256:
                            value['raw'] = memory.read(address + offset, size).hex()
                            if kind == 'StructProperty':
                                structure = pointer(child + 0x70)
                                value['structure'] = describe(structure)
                                # UAVWeaponData 的字段为类、标量与曲线引用，按反射偏移展开。
                                if value['structure'].get('name') == 'UAVWeaponData':
                                    member = pointer(structure + 0x38)
                                    members = {}
                                    while member:
                                        info = describe(member)
                                        if info['name'] in members or len(members) >= 32:
                                            raise ValueError('UAVWeaponData 字段链异常')
                                        relative, element = unpack(member + 0x44, '<I')[0], unpack(member + 0x34, '<I')[0]
                                        if not 0 <= relative < size or relative + element > size:
                                            raise ValueError('UAVWeaponData 字段越界')
                                        item = {'kind': info['class_name'], 'offset': hex(relative)}
                                        if info['class_name'] in ('ClassProperty', 'ObjectProperty'):
                                            item['object'] = describe(pointer(address + offset + relative))
                                        elif info['class_name'] == 'FloatProperty':
                                            item['value'] = unpack(address + offset + relative, '<f')[0]
                                        members[info['name']] = item
                                        member = pointer(member + 0x28)
                                    value['members'] = members
                        fields[name] = value
                    elif kind == 'Function' and name not in methods:
                        methods[name] = hex(pointer(child + 0xb0) - memory.base)
                    child = pointer(child + 0x28)
                cls = pointer(cls + 0x30)
            return {'chain': chain, 'fields': fields, 'methods': methods}

        selected = set()
        for address in known:
            item = describe(address)
            label = item['name'].lower()
            if (label in ('default__weaponuav', 'default__missile', 'default__bullet',
                          'default__weaponbase', 'default__weaponmovementcomponent')
                    or label.startswith(('plwp_weaponuav', 'plwp_plsl_uav', 'uav_returnmovespline',
                                         'default__plwp_weaponuav', 'default__plwp_plsl_uav', 'default__uav_returnmovespline'))
                    and item['class_name'] not in ('BlueprintGeneratedClass', 'Package')
                    and not item['class_name'].endswith('Property')):
                selected.add(address)

        report = {'captured_local': datetime.now().astimezone().isoformat(), 'pid': args.pid,
            'base': hex(memory.base), 'process_write': False, 'game_functions_called': False,
            'warning': '外部只读采样并非原子快照；活动效果需要暂停前后对照，历史地址不可复用。',
            'objects': {}, 'native': []}
        command_path = Path('E:/SteamLibrary/steamapps/common/ACE COMBAT 7/ue4ss/Mods/DualSP/native-command.txt')
        if command_path.exists():
            command = command_path.read_text(encoding='utf-8').split()
            plane, activator = int(command[1], 16), int(command[2], 16)
            if plane in known and activator in known and pointer(plane + 0x11a8) == activator:
                selected.update((plane, activator))
                report['player_hint'] = {'plane': describe(plane), 'activator': describe(activator),
                    'note': '模组命令线索已核对对象表及组件归属，未由控制器再次确认'}
                owner = pointer(activator + 0x260)
                report['weapon_owner'] = describe(owner)
                pool, pool_count, pool_capacity = unpack(owner + 0x158, '<Qii')
                if not 0 <= pool_count <= pool_capacity <= 4096:
                    raise ValueError('原生武器池范围异常')
                report['owner_pool'] = []  # 原生 0x89de1a/0x89df02：外层元素为 16 字节数组。
                for i in range(pool_count):
                    data, length, capacity = unpack(pool + i * 16, '<Qii')
                    if not 0 <= length <= capacity <= 4096:
                        raise ValueError('武器池子数组范围异常')
                    objects = [describe(pointer(data + j * 8)) for j in range(length)]
                    for item in objects:
                        if item.get('uobject'):
                            item = item.copy()
                            component = int(item['address'], 16)
                            report.setdefault('component_cleanup_entries', {})[item['address']] = hex(
                                pointer(pointer(component) + 0x360) - memory.base)
                    report['owner_pool'].append({'group': i, 'objects': objects})
                state = pointer(owner + 0xf0)
                report['group_raw'] = memory.read(state + 0x110, 0x88).hex()
                parameters = pointer(state + 0x158)  # 当前 SP 组 +0x48，现场确认为 WeaponParameters。
                report['sp_parameters'] = {'identity': describe(parameters),
                    'raw': memory.read(parameters, 0xb0).hex() if parameters else ''}
                slots, length, capacity = unpack(state + 0x168, '<Qii')
                if not 0 <= length <= capacity <= 4096:
                    raise ValueError('SP 槽范围异常')
                report['slots_raw'] = [memory.read(slots + i * 0x60, 0x60).hex() for i in range(length)]
            else:
                report['player_hint'] = {'valid': False, 'note': '旧命令未指向当前有效玩家，未解引用'}
        # 展开无人机的直接对象/对象数组引用，记录移动组件、目标与子武器，最多两层。
        # 过滤关卡/资源等大对象，避免无边界追踪整个世界。
        for _ in range(2):
            related = set()
            for address in tuple(selected):
                item = describe(address)
                if not ('uav' in item['name'].lower() or 'uav' in item['class_name'].lower()):
                    continue
                for field in reflect(address)['fields'].values():
                    links = field.get('objects', []) + ([field['object']] if 'object' in field else [])
                    for link in links:
                        if not link.get('uobject'):
                            continue
                        if any(word in link['class_name'].lower() for word in ('component', 'weapon', 'missile', 'bullet')):
                            related.add(int(link['address'], 16))
            selected.update(related)
            if len(selected) > 512:
                raise ValueError('UAV 关联对象超出本轮采样范围')
        for address in sorted(selected):
            data = reflect(address)
            data['identity'] = describe(address)
            # Actor 与组件/属性描述符的虚表长度不同，不能把相邻名称数据当函数。
            table_limit = 0x690 if 'WeaponBase' in data['chain'] else (0x620 if 'Actor' in data['chain'] else 0x310)
            data['vtable'] = {hex(i): hex(pointer(pointer(address) + i) - memory.base)
                              for i in range(0, table_limit, 8)}
            if 'WeaponUAV' in data['chain']:
                # 两个扩展槽来自已观察到的本体/导弹父类调用，不将邻近数据一概视为虚表。
                for slot in (0x6d8, 0x7d8):
                    data['vtable'][hex(slot)] = hex(pointer(pointer(address) + slot) - memory.base)
            if 'PlayerWeaponActivator' in data['chain']:
                data['activator_native_raw'] = memory.read(address + 0x250, 0x260).hex()
            if 'WeaponBase' in data['chain']:
                data['weapon_native_raw'] = memory.read(address + 0x360, 0x2d8).hex()
                if 'WeaponUAV' in data['chain']:
                    class_size = unpack(pointer(address + 0x10) + 0x40, '<I')[0]
                    if not 0x9d0 <= class_size <= 0x2000:
                        raise ValueError('UAV 类实例大小不在已观察范围')
                    data['class_size'] = class_size
                    data['uav_raw'] = memory.read(address + 0x638, class_size - 0x638).hex()
                root = data['fields'].get('RootComponent', {}).get('object', {})
                if root.get('uobject'):
                    data['root_world_position'] = unpack(int(root['address'], 16) + 0x190, '<fff')
            if 'GameObject' in data['chain']:
                root = data['fields'].get('RootComponent', {}).get('object', {})
                if root.get('uobject'):
                    data['root_world_position'] = unpack(int(root['address'], 16) + 0x190, '<fff')
            report['objects'][hex(address)] = data

        # 只展开当前活动效果的附着父链；不把模型组件与武器 Actor 的虚表混为一谈。
        report['attachments'] = {}
        for obj in report['objects'].values():
            weapon_raw = obj.get('weapon_native_raw')
            if not weapon_raw or bytes.fromhex(weapon_raw)[8] == 0:
                continue
            for label in ('RootComponent', 'AttachedComponent'):
                link = obj['fields'].get(label, {}).get('object', {})
                chain = []
                visited = set()
                while link.get('uobject'):
                    address = int(link['address'], 16)
                    if address in visited or len(visited) >= 32:
                        raise ValueError('活动武器附着父链异常')
                    visited.add(address)
                    selected.add(address)
                    component = reflect(address)
                    chain.append({'identity': link, 'class_chain': component['chain'],
                        'cleanup_entry': hex(pointer(pointer(address) + 0x360) - memory.base),
                        'world_position': unpack(address + 0x190, '<fff'),
                        'fields': {k: v for k, v in component['fields'].items() if k in (
                            'AttachParent', 'AttachSocketName', 'RelativeLocation', 'RelativeRotation',
                            'RelativeScale3D', 'bVisible', 'bHiddenInGame', 'PrimaryComponentTick')}})
                    link = component['fields'].get('AttachParent', {}).get('object', {})
                report['attachments'][obj['identity']['address'] + ':' + label] = chain

        # 保存 UAV/子弹不同于 WeaponBase 的函数与共享收尾；不把磁盘代码当运行代码。
        if args.native:
            header = memory.read(memory.base, 0x1000)
            pe = struct.unpack_from('<I', header, 0x3c)[0]
            if header[:2] != b'MZ' or header[pe:pe+4] != b'PE\0\0':
                raise ValueError('运行映像 PE 标识异常')
            optional = pe + 24
            pdata, pdata_size = struct.unpack_from('<II', header, optional + 112 + 3 * 8)
            if not 0 < pdata_size < 0x1000000 or pdata_size % 12:
                raise ValueError('异常展开表范围异常')
            functions = list(struct.iter_unpack('<III', memory.read(memory.base + pdata, pdata_size)))
            section_count = struct.unpack_from('<H', header, pe + 6)[0]
            optional_size = struct.unpack_from('<H', header, pe + 20)[0]
            sections = []
            for i in range(section_count):
                section = memory.read(memory.base + optional + optional_size + i * 40, 40)
                size, rva = struct.unpack_from('<II', section, 8)
                if struct.unpack_from('<I', section, 36)[0] & 0x20000000:
                    sections.append((rva, rva + size))
            base_obj = next(x for x in report['objects'].values()
                            if x['identity']['name'] == 'Default__WeaponBase')
            entries = {0x3d2290, 0x884f10, 0x885240, 0x885510, 0x894af0, 0x897010, 0x89e3c0, 0x89da40}
            for obj in report['objects'].values():
                if not any(kind in obj['chain'] for kind in ('WeaponUAV', 'WeaponBase', 'MissileMovementComponent')):
                    continue
                entries.update(int(target, 16) for target in obj['methods'].values())
                for slot, target in obj['vtable'].items():
                    if target != base_obj['vtable'].get(slot) or int(slot, 16) >= 0x618:
                        entries.add(int(target, 16))
            args.output_dir.mkdir(parents=True, exist_ok=False)
            decoder = Cs(CS_ARCH_X86, CS_MODE_64)
            for entry in sorted(entries):
                if not any(low <= entry < high for low, high in sections):
                    continue
                boundary = next(((begin, end) for begin, end, _ in functions if begin == entry), None)
                if boundary is None:
                    continue  # 叶函数或跳板没有确切边界时不猜测长度。
                begin, end = boundary
                if not 0 < end - begin <= 0x10000 or not any(low <= begin < end <= high for low, high in sections):
                    raise ValueError('函数范围异常')
                code = memory.read(memory.base + begin, end - begin)
                filename = f'native-{begin:x}.bin'
                (args.output_dir / filename).write_bytes(code)
                instructions = [f'{i.address:#x}: {i.mnemonic} {i.op_str}' for i in decoder.disasm(code, begin)]
                (args.output_dir / f'native-{begin:x}.txt').write_text('\n'.join(instructions), encoding='utf-8')
                report['native'].append({'rva': hex(begin), 'end': hex(end), 'file': filename,
                    'boundary': 'PE 展开记录片段，可能只是函数的一部分，外跳须继续采集',
                    'sha256': hashlib.sha256(code).hexdigest()})
            for value in args.range:
                begin, size = (int(x, 16) for x in value.split(':'))
                if not 0 < size <= 0x20000 or not any(low <= begin < begin + size <= high for low, high in sections):
                    raise ValueError('显式代码片段超出可执行节')
                code = memory.read(memory.base + begin, size)
                filename = f'range-{begin:x}-{size:x}.bin'
                (args.output_dir / filename).write_bytes(code)
                instructions = [f'{i.address:#x}: {i.mnemonic} {i.op_str}' for i in decoder.disasm(code, begin)]
                (args.output_dir / f'range-{begin:x}-{size:x}.txt').write_text('\n'.join(instructions), encoding='utf-8')
                report['native'].append({'rva': hex(begin), 'length': size, 'file': filename,
                    'boundary': '显式连续代码片段，不代表函数边界',
                    'sha256': hashlib.sha256(code).hexdigest()})
        else:
            args.output_dir.mkdir(parents=True, exist_ok=False)
        for address in selected:
            item = describe(address)
            current = memory.read(table + item['index'] * 24, 24)
            if (struct.unpack_from('<Q', current)[0] != address
                    or struct.unpack_from('<i', current, 16)[0] != item['serial']):
                raise ValueError('采样期间目标身份变化，结果不可作为完整快照')
        report['completed_local'] = datetime.now().astimezone().isoformat()
        (args.output_dir / 'snapshot.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
        print(json.dumps({'output': str(args.output_dir), 'validated_objects': len(selected),
                          'native_functions': len(report['native'])}, ensure_ascii=False))
    finally:
        memory.close()


if __name__ == '__main__':
    main()
