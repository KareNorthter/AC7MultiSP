"""只读记录玩家手动转动镜头前后状态；不解除暂停、不注入输入、不调用游戏函数。"""
from pathlib import Path
import argparse
import ctypes
import json
import struct
import time
from inspect_camera_hold import CameraReader


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('pid', type=int)
    parser.add_argument('source', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--seconds', type=float, default=20)
    args = parser.parse_args()
    if args.output.exists() or not 0 < args.seconds <= 60:
        raise ValueError('不覆盖旧记录，单轮最多60秒')
    source = json.loads(args.source.read_text(encoding='utf-8'))
    if source['pid'] != args.pid:
        raise ValueError('来源进程不一致')
    reader = CameraReader(args.pid)
    try:
        identities = source['instances']
        for item in identities.values():
            if reader.identity(int(item['address'],16))['serial'] != item['serial']:
                raise ValueError('玩家或镜头组件已换代')
        plane = int(identities['player']['address'],16)
        component = int(identities['InputComponent']['address'],16)
        bindings, count, capacity = reader.unpack(component+0x110,'<Qii')
        if not 0 < count <= capacity <= 128:
            raise ValueError('原生输入轴范围无效')
        axes = []
        for index in range(count):
            name = reader.fname(reader.unpack(bindings+index*0x70+8,'<I')[0])
            if name.startswith('INP_Camera'):
                axes.append({'index':index,'name':name})
        rotations = {}
        for name in ('FirstPersonCamera','ThirdPersonCamera','CockpitCamera'):
            cls = int(identities[name]['class'],16)
            while cls:
                for field in reader.members(cls):
                    if field['name'] == 'RelativeRotation' and field['size'] == 12:
                        rotations[name] = int(field['offset'],16)
                cls = reader.ptr(cls+0x30)
        pad = ctypes.WinDLL('xinput1_3').XInputGetState
        pad.argtypes = [ctypes.c_uint32,ctypes.c_void_p]
        pad.restype = ctypes.c_uint32
        samples = []
        start = time.perf_counter()
        while time.perf_counter()-start < args.seconds:
            if reader.ptr(plane+0x118) != component or reader.unpack(component+0x110,'<Qii') != (bindings,count,capacity):
                raise ValueError('采样期间玩家输入已换代')
            row = {'elapsed':round(time.perf_counter()-start,4),
                   'axis_values':[reader.unpack(bindings+axis['index']*0x70+0x60,'<f')[0] for axis in axes],
                   'view_type':reader.unpack(int(identities['CameraView']['address'],16)+0x350,'<B')[0],
                   'camera_input_enabled':reader.unpack(plane+0xe12,'<B')[0],
                   'input_states':{},'relative_rotations':{}}
            for name, offset in (('ThirdPersonCameraInput',0x3d4),('FirstPersonCameraInput',0x3d4),('CockpitCameraInput',0x38c)):
                address = int(identities[name]['address'],16)
                row['input_states'][name] = list(reader.unpack(address+offset,'<fff'))
            for name, offset in rotations.items():
                row['relative_rotations'][name] = list(reader.unpack(int(identities[name]['address'],16)+offset,'<fff'))
            state = ctypes.create_string_buffer(16)
            result = pad(0,state)
            if result == 0:
                _, buttons, _, _, _, _, rx, ry = struct.unpack('<IHBBhhhh',state.raw)
                row['controller'] = {'buttons':buttons,'right_x':rx,'right_y':ry}
            else:
                row['controller'] = {'error':result}
            samples.append(row)
            time.sleep(.025)
        args.output.parent.mkdir(parents=True,exist_ok=True)
        args.output.write_text(json.dumps({'pid':args.pid,'process_write':False,'game_functions_called':False,
            'input_injected':False,'axes':axes,'rotations':rotations,'samples':samples},ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
        print(json.dumps({'output':str(args.output),'samples':len(samples),'view_types':sorted({row['view_type'] for row in samples}),
            'active_input_samples':sum(any(abs(value)>.1 for value in row['axis_values']) for row in samples)},ensure_ascii=False))
    finally:
        reader.memory.close()


if __name__ == '__main__':
    main()
