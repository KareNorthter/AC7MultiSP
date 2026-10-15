"""只读保存当前 Su-30SM 快照对应的挂载代码片段；不写进程、不调用游戏函数。"""

import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys


def main():
    """核对进程、玩家身份、代码签名和可执行节，再生成仅供本机分析的原件。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--state', type=Path, required=True)
    parser.add_argument('--project-root', type=Path, required=True)
    parser.add_argument('--output-dir', type=Path, required=True)
    parser.add_argument('--range', action='append', default=[], help='显式 RVA:长度，均为十六进制')
    args = parser.parse_args()
    # 原始代码只在本工作区 Temp 留存，不混入精简证据目录。
    temporary_root = Path(__file__).resolve().parents[2] / 'Temp'
    if not args.output_dir.resolve().is_relative_to(temporary_root.resolve()):
        raise ValueError('原始代码片段必须保存在本工作区 Temp 内')
    sys.path[:0] = [str(args.project_root / 'tools'), str(args.project_root / 'src')]
    from inspect_native import GameMemory
    from capstone import Cs, CS_ARCH_X86, CS_MODE_64

    snapshot_bytes = args.state.read_bytes()  # 原件哈希防止把其他任务快照当作本次输入。
    snapshot = json.loads(snapshot_bytes)
    memory = GameMemory(snapshot['pid'])  # 既有接口只申请 0x410 查询和读取权限。
    try:
        if memory.base != int(snapshot['base'], 16):
            raise ValueError('快照对应的游戏进程已改变')
        signature = bytes.fromhex('488b8160020000488b88f00000008b8118010000c3')
        if memory.read(memory.base + 0x882390, len(signature)) != signature:
            raise ValueError('原生 getter 签名不匹配')
        plane = int(snapshot['plane']['address'], 16)
        # 玩家指针、类和对象表序列号共同验证，拒绝已销毁或地址复用的实例。
        identity = memory.read(plane, 0x28)
        index = struct.unpack_from('<i', identity, 0xc)[0]
        cls = struct.unpack_from('<Q', identity, 0x10)[0]
        objects, maximum, count = struct.unpack('<Qii', memory.read(memory.base + 0x3bcfdb0, 16))
        if not 0 <= index < count <= maximum < 3000000:
            raise ValueError('玩家 UObject 索引或对象表边界无效')
        object_entry = memory.read(objects + index * 24, 24)
        if (index != snapshot['plane']['index'] or cls != int(snapshot['plane']['class'], 16)
                or struct.unpack_from('<Q', object_entry)[0] != plane
                or struct.unpack_from('<i', object_entry, 16)[0] != snapshot['plane']['serial']
                or struct.unpack('<Q', memory.read(plane + 0x11a8, 8))[0] != int(snapshot['activator']['address'], 16)):
            raise ValueError('玩家身份变化，请重新采集暂停快照')

        header = memory.read(memory.base, 0x1000)  # 只从当前内存映像读取 PE 节布局。
        pe_offset = struct.unpack_from('<I', header, 0x3c)[0]
        if header[:2] != b'MZ' or header[pe_offset:pe_offset+4] != b'PE\0\0':
            raise ValueError('运行映像 PE 标识异常')
        section_count = struct.unpack_from('<H', header, pe_offset + 6)[0]
        optional_size = struct.unpack_from('<H', header, pe_offset + 20)[0]
        if not 0 < section_count <= 96:
            raise ValueError('PE 节数量异常')
        sections = []  # 只接受完全落在可执行节内的代码范围。
        for index in range(section_count):
            offset = pe_offset + 24 + optional_size + index * 40
            section = memory.read(memory.base + offset, 40)
            size, rva = struct.unpack_from('<II', section, 8)
            characteristics = struct.unpack_from('<I', section, 36)[0]
            if characteristics & 0x20000000:
                sections.append((rva, rva + size))

        # 连续片段只提供调用链线索；不能把这些范围当作完整函数边界。
        ranges = [(0x898000, 0xc000), (0x87f000, 0x9000)]
        if args.range:
            ranges = [tuple(int(part, 16) for part in value.split(':')) for value in args.range]
        for item in ranges:
            if len(item) != 2:
                raise ValueError('代码范围应为 RVA:长度')
            start, length = item
            if not 0 < length <= 0x20000 or not any(low <= start < start + length <= high for low, high in sections):
                raise ValueError(f'请求不在可执行节内：{item}')
        args.output_dir.mkdir(parents=True, exist_ok=False)
        manifest = {'pid': snapshot['pid'], 'base': snapshot['base'],
                    'snapshot_sha256': hashlib.sha256(snapshot_bytes).hexdigest(),
                    'process_write': False, 'game_functions_called': False, 'ranges': [],
                    'caution': '线性反汇编，未进行控制流恢复；只读证据不能替代接口调用验证。'}
        disassembler = Cs(CS_ARCH_X86, CS_MODE_64)
        for start, length in ranges:
            code = memory.read(memory.base + start, length)
            filename = f'mount-{start:x}-{length:x}.bin'
            (args.output_dir / filename).write_bytes(code)
            manifest['ranges'].append({'rva': hex(start), 'length': length, 'file': filename,
                                       'sha256': hashlib.sha256(code).hexdigest()})
        # 从已经观察到的入口开始，避免依赖连续块首部是否恰好为指令边界。
        entries = {0x89da40: 0xd60, 0x8a0400: 0x1000, 0x882e40: 0xc00, 0x880720: 0x3b0}
        for start, limit in entries.items():
            for region in manifest['ranges']:
                begin = int(region['rva'], 16)
                if begin <= start < begin + region['length']:
                    length = min(limit, begin + region['length'] - start)
                    code = memory.read(memory.base + start, length)
                    instructions = [f'{instruction.address:#x}: {instruction.mnemonic} {instruction.op_str}'
                                    for instruction in disassembler.disasm(code, start)]
                    (args.output_dir / f'entry-{start:x}.txt').write_text('\n'.join(instructions) + '\n', encoding='utf-8')
                    break
        (args.output_dir / 'manifest.json').write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
        print(json.dumps({'output': str(args.output_dir), 'ranges': len(ranges), 'process_write': False}, ensure_ascii=False))
    finally:
        memory.close()


if __name__ == '__main__':
    main()
