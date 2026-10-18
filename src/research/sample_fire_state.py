"""只读采样玩家发射与锁定列表；不写进程、不注入输入、不调用游戏函数。"""
import argparse
import ctypes
import json
import struct
import time
from pathlib import Path
from inspect_camera_hold import CameraReader


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('pid', type=int)
    parser.add_argument('output', type=Path)
    parser.add_argument('--seconds', type=float, default=120)
    args = parser.parse_args()
    if args.output.exists() or not 0 < args.seconds <= 600:
        raise ValueError('不覆盖旧记录，单轮最多600秒')
    if not args.output.resolve().is_relative_to((Path(__file__).resolve().parents[2] / 'Temp').resolve()):
        raise ValueError('原始内存采样只能保存到工作区Temp')
    root = Path('E:/SteamLibrary/steamapps/common/ACE COMBAT 7/ue4ss/Mods/DualSP')
    command = (root / 'native-command.txt').read_text(encoding='utf-8').split()
    plane, activator = int(command[1], 16), int(command[2], 16)
    reader = CameraReader(args.pid)
    try:
        seeker = reader.ptr(plane + 0x1310)
        identities = {name: reader.identity(address) for name, address in
                      [('plane', plane), ('activator', activator), ('seeker', seeker)]}
        assert reader.ptr(plane + 0x11a8) == activator and reader.ptr(activator + 0x268) == seeker
        pad = ctypes.WinDLL('xinput1_3').XInputGetState
        pad.argtypes = [ctypes.c_uint32, ctypes.c_void_p]
        pad.restype = ctypes.c_uint32
        args.output.parent.mkdir(parents=True, exist_ok=True)
        with args.output.open('x', encoding='utf-8') as out:
            out.write(json.dumps({'pid': args.pid, 'identities': identities, 'process_write': False,
                                  'input_injected': False, 'interval_ms': 10}) + '\n')
            print('只读采样已开始', flush=True)
            started = time.perf_counter()
            previous = None
            count = 0
            while time.perf_counter() - started < args.seconds:
                for identity in identities.values():
                    address = int(identity['address'], 16)
                    if reader.ptr(reader.objects + identity['index'] * 24) != address or reader.unpack(
                            reader.objects + identity['index'] * 24 + 16, '<i')[0] != identity['serial']:
                        raise ValueError('玩家或武器组件已换代，停止采样')
                raw = reader.read(activator + 0x2a0, 0x50)
                state = reader.ptr(reader.ptr(activator + 0x260) + 0xf0)
                row = {'armed': raw[1], 'held': raw[5], 'active': raw[0x20], 'finishing': raw[0x21],
                       'queued': struct.unpack_from('<i', raw, 0x18)[0],
                       'batch_targets': struct.unpack_from('<i', raw, 0x30)[0],
                       'target_cursor': struct.unpack_from('<i', raw, 0x38)[0],
                       'fired': struct.unpack_from('<i', raw, 0x3c)[0],
                       'total': struct.unpack_from('<i', raw, 0x40)[0],
                       'ammo': [reader.unpack(state + off + 8, '<i')[0] for off in (0x88, 0x110)],
                       'weapons': [hex(reader.ptr(state + off)) for off in (0x88, 0x110)]}
                for label, offset in [('locked', 0x398), ('slots', 0x468)]:
                    data, size, capacity = reader.unpack(seeker + offset, '<Qii')
                    if not 0 <= size <= capacity <= 2048:
                        raise ValueError('锁定数组边界无效')
                    pointers = [p[0] for p in struct.iter_unpack('<Q', reader.read(data, size * 8))] if size else []
                    row[label] = [hex(p) for p in pointers]
                    if label == 'slots':
                        row['slot_states'] = [list(reader.unpack(p + 0x38, '<QB')) for p in pointers]
                controller = ctypes.create_string_buffer(16)
                if pad(0, controller) == 0:
                    row['controller'] = list(struct.unpack('<IHBBhhhh', controller.raw)[1:4])
                if row != previous:
                    out.write(json.dumps({'elapsed': time.perf_counter() - started,
                                          'wall_ns': time.time_ns(), **row}) + '\n')
                    out.flush()
                    previous = row
                    count += 1
                time.sleep(.01)
            print(json.dumps({'output': str(args.output), 'changes': count}), flush=True)
    finally:
        reader.memory.close()


if __name__ == '__main__':
    main()
