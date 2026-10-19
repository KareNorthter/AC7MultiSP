"""只读采集机炮齐射研究所需的玩家、兵装默认对象及原生动作；不调用游戏函数。"""
import argparse
import hashlib
import json
import struct
import time
from datetime import datetime
from pathlib import Path

from inspect_camera_hold import CameraReader

ROOT = Path(__file__).resolve().parents[2]
MOD = Path('E:/SteamLibrary/steamapps/common/ACE COMBAT 7/ue4ss/Mods/DualSP')


def main():
    started = time.perf_counter()
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('pid', type=int)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    output = args.output.resolve()
    if output.exists() or not output.is_relative_to((ROOT / 'Temp').resolve()):
        raise ValueError('必须使用本研究工作区Temp下的新目录，不覆盖原记录')
    ui = json.loads((MOD / 'ui-state.json').read_text(encoding='utf-8'))
    if not ui.get('configured') or not ui.get('runtime_mod_enabled') or ui.get('mission_fixed_sp'):
        raise ValueError('需要普通单人任务内已配置的玩家；菜单和指定SP任务不采集')
    command = (MOD / 'native-command.txt').read_text(encoding='utf-8').split()
    plane, activator = (int(command[i], 16) for i in (1, 2))
    reader = CameraReader(args.pid)  # 共用已核对的只读权限、进程路径与构建签名。
    try:
        output.mkdir(parents=True)
        report = {'date': datetime.now().isoformat(), 'pid': args.pid, 'base': hex(reader.base),
                  'process_write': False, 'game_functions_called': False, 'input_injected': False,
                  'observation': '跨进程读取不构成同帧原子快照；暂停状态以用户现场说明为准',
                  'ui': {k: ui.get(k) for k in ('sequence', 'scene', 'plane_id', 'plane_name',
                         'configured', 'mission_fixed_sp', 'management_pause')},
                  'classes': {}, 'objects': {}, 'functions': {}, 'sp_configs': []}
        class_cache = {}

        def live(identity):
            address, index = int(identity['address'], 16), identity['index']
            if reader.ptr(reader.objects + index * 24) != address or reader.unpack(
                    reader.objects + index * 24 + 16, '<i')[0] != identity['serial']:
                raise ValueError('采集期间对象已换代，停止并保留已采资料')

        def fields(cls):
            result, seen = {}, set()
            while cls:
                if cls in seen or len(seen) >= 64:
                    raise ValueError('类继承链异常')
                seen.add(cls)
                if cls not in class_cache:
                    identity = reader.identity(cls)
                    members = reader.members(cls)
                    class_cache[cls] = members
                    report['classes'][hex(cls)] = {'identity': identity,
                        'size': reader.unpack(cls + 0x40, '<i')[0], 'members': members}
                for member in class_cache[cls]:
                    result.setdefault(member['name'], member)
                cls = reader.ptr(cls + 0x30)
            return result

        def obj(address, selected=None):
            identity = reader.identity(address)
            live(identity)
            result = {'identity': identity, 'fields': {}}
            for name, field in fields(int(identity['class'], 16)).items():
                if selected is not None and name not in selected:
                    continue
                if 'offset' not in field:
                    continue
                offset, size = int(field['offset'], 16), field['size']
                if not 0 <= offset < 0x20000 or not 0 < size <= 0x4000:
                    raise ValueError('反射字段边界异常')
                kind = field['class_name']
                value = {'kind': kind, 'offset': field['offset'], 'size': size}
                if kind in ('ObjectProperty', 'ClassProperty'):
                    pointer = reader.ptr(address + offset)
                    value['object'] = reader.identity(pointer) if pointer else None
                elif size <= 256:
                    value['raw'] = reader.read(address + offset, size).hex()
                    if kind == 'BoolProperty':
                        value['mask_layout'] = reader.read(int(field['address'], 16) + 0x70, 8).hex()
                    elif kind == 'StructProperty':
                        value['structure'] = reader.identity(reader.ptr(int(field['address'], 16) + 0x70))
                result['fields'][name] = value
            live(identity)
            report['objects'][hex(address)] = result
            return result

        player = obj(plane, {'PlaneTypeID', 'PlaneSWP', 'Controller', 'InputComponent',
                            'PlayerWeaponActivator', 'bEnableWeaponInput'})
        if not player['identity']['class_name'].startswith('AcePlayerPawn_'):
            raise ValueError('命令文件没有指向当前玩家机体')
        if reader.ptr(plane + 0x11a8) != activator or reader.ptr(activator + 0x250) != plane:
            raise ValueError('玩家与激活器归属不一致')
        controller = player['fields'].get('Controller', {}).get('object')
        if not controller:
            raise ValueError('当前玩家没有控制器')
        controlled = obj(int(controller['address'], 16), {'Pawn', 'AcknowledgedPawn'})
        pawn = controlled['fields'].get('Pawn', {}).get('object')
        if not pawn or int(pawn['address'], 16) != plane:
            raise ValueError('命令文件不是控制器当前实际受控玩家')
        activation = obj(activator, {'MachineGunClass', 'MainWeaponClass', 'SpecialWeaponClass',
            'SpecialWeaponConfig1', 'SpecialWeaponConfig2', 'SpecialWeaponConfig3',
            'MaxGunCount', 'MaxMainWeaponCount', 'MaxSpecialWeaponCount'})
        weapon_classes = set()
        for name, field in activation['fields'].items():
            if field['kind'] == 'ClassProperty' and field.get('object'):
                weapon_classes.add(int(field['object']['address'], 16))
            if name.startswith('SpecialWeaponConfig'):
                structure = field.get('structure')
                if not structure:
                    raise ValueError('未取得兵装配置结构反射')
                members = fields(int(structure['address'], 16))
                weapon = members.get('WeaponClass')
                if not weapon or weapon['class_name'] != 'ClassProperty':
                    raise ValueError('兵装配置缺少WeaponClass')
                relative, size = int(weapon['offset'], 16), weapon['size']
                if size != 8 or relative + size > field['size']:
                    raise ValueError('兵装类字段超出配置结构')
                address = reader.ptr(activator + int(field['offset'], 16) + relative)
                identity = reader.identity(address)
                weapon_classes.add(address)
                report['sp_configs'].append({'field': name, 'class': identity,
                    'relative_weapon_offset': weapon['offset']})
        # 按实际Class和CDO标志查找默认对象，不猜测UClass内部CDO指针偏移。
        defaults = {}
        print('已核对当前玩家归属，正在只读查找实际兵装默认对象', flush=True)
        for offset in range(0, len(reader.registry), 24):
            address = struct.unpack_from('<Q', reader.registry, offset)[0]
            if not address:
                continue
            raw = reader.read(address, 0x28)
            flags, cls = struct.unpack_from('<I', raw, 8)[0], struct.unpack_from('<Q', raw, 0x10)[0]
            if cls in weapon_classes and flags & 0x10:
                identity = reader.identity(address)
                if not identity['name'].startswith('Default__') or cls in defaults:
                    raise ValueError('兵装默认对象不唯一或标志不符')
                defaults[cls] = obj(address, {'WeaponID', 'bAutoFireOnHold', 'bGunReaction',
                    'MaxLoadedCount', 'bFireAllInOneShot', 'LoadTime', 'VolleyRestInterval',
                    'bShouldTreatAsOneWeapon', 'bCanUseSameSlot', 'bReloadOnFire', 'bReloadAfterAllFired'})
        if set(defaults) != weapon_classes:
            raise ValueError('没有找到全部实际兵装类的默认对象')
        report['defaults'] = {hex(cls): value for cls, value in defaults.items()}
        input_info = player['fields'].get('InputComponent', {}).get('object')
        if not input_info:
            raise ValueError('玩家输入组件尚未就绪')
        input_address = int(input_info['address'], 16)
        if reader.ptr(input_address + 0x20) != plane:
            raise ValueError('输入组件不属于实际受控玩家')
        input_size = reader.unpack(int(input_info['class'], 16) + 0x40, '<i')[0]
        if not 0x120 <= input_size <= 0x4000:
            raise ValueError('玩家输入组件大小异常')
        (output / 'input-component.bin').write_bytes(reader.read(input_address, input_size))
        report['input'] = {'identity': input_info, 'size': input_size,
                          'owner': reader.identity(reader.ptr(input_address + 0x20))}
        for owner in (plane, activator):
            for name, member in fields(reader.ptr(owner + 0x10)).items():
                if 'native_rva' in member and any(token in name for token in
                        ('WeaponFire', 'GunPressed', 'GunReleased', 'WeaponPressed',
                         'WeaponReleased', 'ReleaseWeaponInput', 'SetupPlayerInput')):
                    reader.capture(int(member['native_rva'], 16), output, report['functions'], name)
        for rva in (0x883e10, 0x883f80, 0x8840b0, 0x8844c0, 0x889c90):
            reader.capture(rva, output, report['functions'], '既有静态研究入口')
        live(player['identity'])
        live(activation['identity'])
        report['files'] = [{'name': p.name, 'bytes': p.stat().st_size,
                            'sha256': hashlib.sha256(p.read_bytes()).hexdigest()}
                           for p in sorted(output.iterdir()) if p.is_file()]
        (output / 'capture.json').write_text(json.dumps(report, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
        print(json.dumps({'output': str(output), 'plane': player['identity']['class_name'],
            'sp_classes': [entry['class']['name'] for entry in report['sp_configs']],
            'defaults': len(defaults), 'functions': len(report['functions']),
            'seconds': round(time.perf_counter() - started, 2)}, ensure_ascii=False))
    finally:
        reader.memory.close()


if __name__ == '__main__':
    main()
