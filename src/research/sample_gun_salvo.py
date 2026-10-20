"""记录用户手动操作时的原生机枪/SP状态；只读进程，不调用游戏函数或注入输入。"""
import argparse
import json
import struct
import time
from pathlib import Path

from inspect_camera_hold import CameraReader


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('pid', type=int)
    parser.add_argument('snapshot', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--seconds', type=float, default=90)
    args = parser.parse_args()
    temporary = (Path(__file__).resolve().parents[2] / 'Temp').resolve()
    if args.output.exists() or not args.output.resolve().is_relative_to(temporary):
        raise ValueError('采样只允许写入工作区Temp的新文件')
    if not 0 < args.seconds <= 180:
        raise ValueError('单轮采样须为0至180秒')
    capture = json.loads(args.snapshot.read_text(encoding='utf-8'))
    if capture['pid'] != args.pid:
        raise ValueError('不能跨进程复用快照身份')
    objects = list(capture['objects'].values())
    plane = next(item['identity'] for item in objects
                 if item['identity']['class_name'].startswith('AcePlayerPawn_'))
    activation = next(item['identity'] for item in objects
                      if 'SpecialWeaponConfig1' in item['fields'])
    controller = next(item for item in objects if 'Pawn' in item['fields'])
    pawn_offset = int(controller['fields']['Pawn']['offset'], 16)
    plane_address, activator = int(plane['address'], 16), int(activation['address'], 16)
    identities = [plane, activation, controller['identity']]
    reader = CameraReader(args.pid)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    count = 0
    started = time.perf_counter()
    try:
        with args.output.open('x', encoding='utf-8') as out:
            out.write(json.dumps({'pid': args.pid, 'snapshot': str(args.snapshot.resolve()),
                'plane': plane, 'activator': activation, 'interval_ms': 20,
                'process_write': False, 'game_functions_called': False, 'input_injected': False,
                'limitations': '跨进程异步读取，不构成同帧原子快照；按住和扣弹不是按键事件计数'})+'\n')
            out.flush()
            print('只读采样已开始；等待用户手动操作', flush=True)
            previous = None
            try:
                while time.perf_counter() - started < args.seconds:
                    for identity in identities:
                        address, index = int(identity['address'], 16), identity['index']
                        if reader.ptr(reader.objects + index * 24) != address or reader.unpack(
                                reader.objects + index * 24 + 16, '<i')[0] != identity['serial']:
                            raise ValueError('玩家或激活器换代，已停止采样')
                    if reader.ptr(plane_address + 0x11a8) != activator or reader.ptr(activator + 0x250) != plane_address:
                        raise ValueError('玩家与激活器已解除关联，已停止采样')
                    if reader.ptr(int(controller['identity']['address'], 16) + pawn_offset) != plane_address:
                        raise ValueError('控制器已更换实际受控玩家，已停止采样')
                    raw = reader.read(activator + 0x2a0, 0x50)
                    state = reader.ptr(reader.ptr(activator + 0x260) + 0xf0)
                    row = {'armed': raw[1], 'gun_held': raw[4], 'main_held': raw[5],
                           'active_batch': raw[0x20], 'finishing': raw[0x21],
                           'queued': struct.unpack_from('<i', raw, 0x18)[0],
                           'plane_swp': reader.unpack(plane_address + 0xb4c, '<i')[0],
                           'gun_ammo': reader.unpack(state + 8, '<i')[0],
                           'main_ammo': reader.unpack(state + 0x90, '<i')[0],
                           'sp_ammo': reader.unpack(state + 0x118, '<i')[0],
                           'classes': [hex(reader.ptr(state + offset)) for offset in (0, 0x88, 0x110)],
                           'active_weapon_count': reader.unpack(activator + 0x340, '<i')[0]}
                    if row != previous:
                        out.write(json.dumps({'elapsed': round(time.perf_counter()-started, 6),
                                              'wall_ns': time.time_ns(), **row})+'\n')
                        out.flush()
                        previous = row
                        count += 1
                    time.sleep(.02)
            except KeyboardInterrupt:
                pass
            out.write(json.dumps({'completed': True, 'seconds': round(time.perf_counter()-started, 3),
                                  'changes': count})+'\n')
        print(json.dumps({'output': str(args.output.resolve()), 'changes': count}, ensure_ascii=False))
    finally:
        reader.memory.close()


if __name__ == '__main__':
    main()
