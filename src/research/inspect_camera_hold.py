"""只读研究当前玩家镜头：反射、函数边界和原生状态，不写内存或调用游戏函数。"""
from pathlib import Path
import argparse
import bisect
import hashlib
import json
import struct
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path[:0] = [str(ROOT / 'src'), 'E:/AC7_MultiSP/tools']
from inspect_native import GameMemory
from capstone import Cs, CS_ARCH_X86, CS_MODE_64


class CameraReader:
    def __init__(self, pid):
        self.memory = GameMemory(pid)
        self.base = self.memory.base
        signature = bytes.fromhex('488b8160020000488b88f00000008b8118010000c3')
        if self.read(self.base + 0x882390, len(signature)) != signature:
            raise ValueError('游戏构建指纹不匹配')
        self.objects, maximum, self.count = self.unpack(self.base + 0x3bcfdb0, '<Qii')
        if not 0 < self.count <= maximum < 3000000:
            raise ValueError('原生对象表范围无效')
        self.registry = self.read(self.objects, self.count * 24)
        self.names = self.ptr(self.base + 0x3bc7070)
        self.name_cache = {}
        pe = self.unpack(self.base + 0x3c, '<I')[0]
        if self.read(self.base + pe, 4) != b'PE\0\0':
            raise ValueError('游戏映像头无效')
        self.image_size = self.unpack(self.base + pe + 24 + 56, '<I')[0]
        rva, size = self.unpack(self.base + pe + 24 + 112 + 3 * 8, '<II')
        if not 0 < size < 0x1000000 or size % 12:
            raise ValueError('原生函数边界表范围无效')
        self.ranges = list(struct.iter_unpack('<III', self.read(self.base + rva, size)))
        self.starts = [row[0] for row in self.ranges]
        self.unwind_roots = {}

    def read(self, address, size):
        return self.memory.read(address, size)

    def unpack(self, address, fmt):
        return struct.unpack(fmt, self.read(address, struct.calcsize(fmt)))

    def ptr(self, address):
        return self.unpack(address, '<Q')[0]

    def fname(self, index):
        if index not in self.name_cache:
            if not 0 <= index < 4000000:
                raise ValueError('名称索引无效')
            entry = self.ptr(self.ptr(self.names + index // 0x4000 * 8) + index % 0x4000 * 8)
            raw = self.read(entry, 256)
            if raw[0] & 1:
                end = next(i for i in range(16, 256, 2) if raw[i:i+2] == b'\0\0')
                value = raw[16:end].decode('utf-16le')
            else:
                value = raw[16:].split(b'\0')[0].decode('utf-8')
            self.name_cache[index] = value
        return self.name_cache[index]

    def identity(self, address):
        raw = self.read(address, 0x28)
        index = struct.unpack_from('<i', raw, 0xc)[0]
        if not 0 <= index < self.count or self.ptr(self.objects + index * 24) != address:
            raise ValueError('对象身份失效')
        cls, name, outer = struct.unpack_from('<Q', raw, 0x10)[0], struct.unpack_from('<I', raw, 0x18)[0], struct.unpack_from('<Q', raw, 0x20)[0]
        return {'address': hex(address), 'index': index,
                'serial': self.unpack(self.objects + index * 24 + 16, '<i')[0],
                'name': self.fname(name), 'class': hex(cls),
                'class_name': self.fname(self.unpack(cls + 0x18, '<I')[0]), 'outer': hex(outer)}

    def members(self, cls):
        field = self.ptr(cls + 0x38)
        result, seen = [], set()
        while field:
            if field in seen or len(seen) >= 4096:
                raise ValueError('反射属性链无效')
            seen.add(field)
            item = self.identity(field)
            item['raw'] = self.read(field, 0xc0).hex()
            if item['class_name'] == 'Function':
                item['native_rva'] = hex(self.ptr(field + 0xb0) - self.base)
            else:
                item['offset'] = hex(self.unpack(field + 0x44, '<I')[0])
                item['size'] = self.unpack(field + 0x34, '<I')[0]
            result.append(item)
            field = self.ptr(field + 0x28)
        return result

    def capture(self, rva, output, functions, label):
        if not 0 < rva < self.image_size:
            return
        index = bisect.bisect_right(self.starts, rva) - 1
        # MSVC会把同一函数按保存寄存器范围拆成多个RUNTIME_FUNCTION；沿CHAININFO合并，不能截断正文。
        exact = index >= 0 and self.ranges[index][0] <= rva < self.ranges[index][1]
        if exact:
            root = self.unwind_root(self.ranges[index])
            related = [row for row in self.ranges[max(0,index-48):index+49] if self.unwind_root(row) == root]
            begin, end = min(row[0] for row in related), max(row[1] for row in related)
        else:
            begin, end = rva, rva + 96
        if not 0 < end - begin <= 0x20000:
            raise ValueError('原生函数范围超过研究上限')
        key = hex(begin)
        if key in functions:
            functions[key]['sources'].append(label)
            return
        raw = self.read(self.base + begin, end - begin)
        instructions = list(Cs(CS_ARCH_X86, CS_MODE_64).disasm(raw, begin))
        (output / f'{begin:x}.bin').write_bytes(raw)
        (output / f'{begin:x}.txt').write_text('\n'.join(f'{i.address:#x}: {i.mnemonic} {i.op_str}' for i in instructions) + '\n', encoding='utf-8')
        functions[key] = {'begin': key, 'size': end - begin, 'sources': [label],
                          'boundary': 'unwind_chain' if exact else 'leaf_window_not_exact',
                          'sha256': hashlib.sha256(raw).hexdigest(),
                          'branches': [i.op_str for i in instructions if i.mnemonic in ('call', 'jmp')]}

    def unwind_root(self, row):
        if row not in self.unwind_roots:
            _, _, unwind = row
            header = self.read(self.base + unwind, 4)
            if header[0] >> 3 & 4:
                chain = self.unpack(self.base + unwind + 4 + ((header[2] + 1) & ~1) * 2, '<III')
                self.unwind_roots[row] = self.unwind_root(chain)
            else:
                self.unwind_roots[row] = row
        return self.unwind_roots[row]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('pid', type=int)
    parser.add_argument('output', type=Path)
    parser.add_argument('--rva', type=lambda value: int(value, 16), nargs='*')
    args = parser.parse_args()
    if args.output.exists():
        raise ValueError('不覆盖旧观察')
    reader = CameraReader(args.pid)
    try:
        args.output.mkdir(parents=True)
        functions, classes, instances = {}, {}, {}
        if args.rva:
            for rva in args.rva:
                reader.capture(rva, args.output, functions, '指定已定位接口')
        else:
            for index in range(min(reader.count, 12000)):
                address = struct.unpack_from('<Q', reader.registry, index * 24)[0]
                if not address:
                    continue
                raw = reader.read(address, 0x28)
                name = reader.fname(struct.unpack_from('<I', raw, 0x18)[0])
                if 'Camera' not in name and name not in ('PlayerPlane', 'NimbusPlayerController'):
                    continue
                item = reader.identity(address)
                if item['class_name'] != 'Class':
                    continue
                outer = reader.identity(int(item['outer'], 16))['name']
                if outer not in ('/Script/Nimbus', '/Script/Engine'):
                    continue
                fields = reader.members(address)
                classes[name] = {'identity': item, 'size': reader.unpack(address + 0x40, '<i')[0], 'members': fields}
                for field in fields:
                    if 'native_rva' in field and any(token in field['name'] for token in ('Camera', 'View', 'TargetSelect')):
                        reader.capture(int(field['native_rva'], 16), args.output, functions, name + ':' + field['name'])
            command = Path('E:/SteamLibrary/steamapps/common/ACE COMBAT 7/ue4ss/Mods/DualSP/native-command.txt').read_text().split()
            plane = int(command[1], 16)
            player = reader.identity(plane)
            if not player['class_name'].startswith('AcePlayerPawn_') or reader.ptr(plane + 0x11a8) != int(command[2], 16):
                raise ValueError('命令文件不是当前玩家机体')
            instances['player'] = player
            cls = reader.ptr(plane + 0x10)
            while cls:
                for field in reader.members(cls):
                    if field['class_name'] == 'ObjectProperty' and ('Camera' in field['name'] or field['name'] in ('Controller', 'InputComponent')):
                        address = reader.ptr(plane + int(field['offset'], 16))
                        if address:
                            item = reader.identity(address)
                            item['player_field'] = field['name']
                            item['size'] = reader.unpack(reader.ptr(address + 0x10) + 0x40, '<i')[0]
                            if not 0 < item['size'] <= 0x20000:
                                raise ValueError('镜头实例大小无效')
                            (args.output / (field['name'] + '.bin')).write_bytes(reader.read(address, item['size']))
                            instances[field['name']] = item
                cls = reader.ptr(cls + 0x30)
            for row in list(functions.values()):
                if any('OnInputViewPressed' in source or 'GetCurrentCameraViewType' in source or 'InitializeCameras' in source for source in row['sources']):
                    for branch in row['branches']:
                        if branch.startswith('0x'):
                            reader.capture(int(branch, 16), args.output, functions, '镜头切换反射下级')
        result = {'pid': args.pid, 'base': hex(reader.base), 'process_write': False,
                  'game_functions_called': False, 'input_injected': False,
                  'classes': classes, 'instances': instances, 'functions': functions}
        (args.output / 'capture.json').write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
        print(json.dumps({'output': str(args.output), 'classes': list(classes), 'instances': instances, 'functions': len(functions)}, ensure_ascii=False))
    finally:
        reader.memory.close()


if __name__ == '__main__':
    main()
