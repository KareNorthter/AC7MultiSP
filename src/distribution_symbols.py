"""为新构建保留私有诊断DLL，精简交付副本；不触及历史核心。"""
from pathlib import Path
import shutil
from release_tools import digest


def section_name(pe, section):
    name = section.Name.rstrip(b'\0')
    if name.startswith(b'/'):
        offset = pe.FILE_HEADER.PointerToSymbolTable + pe.FILE_HEADER.NumberOfSymbols * 18 + int(name[1:])
        data = pe.__data__
        end = data.find(b'\0', offset)
        if offset >= len(data) or end < offset:
            raise ValueError('COFF节名字符串表异常')
        name = data[offset:end]
    return name


def loader_contract(path):
    """比较加载所需信息；只忽略调试目录及其指向的数据。"""
    import pefile
    with pefile.PE(str(path)) as pe:
        pe.parse_data_directories()
        ignored = []
        debug = pe.OPTIONAL_HEADER.DATA_DIRECTORY[6]
        if debug.VirtualAddress and debug.Size:
            ignored.append((debug.VirtualAddress, debug.Size))
        for entry in getattr(pe, 'DIRECTORY_ENTRY_DEBUG', []):
            if entry.struct.AddressOfRawData:
                ignored.append((entry.struct.AddressOfRawData, entry.struct.SizeOfData))
        sections = {}
        for section in pe.sections:
            name = section_name(pe, section)
            # 调试节及构建标识不承担游戏逻辑；所有其他已映射节逐字节保留。
            if name.startswith(b'.debug') or name == b'.buildid' or not section.Characteristics & 0xe0000000:
                continue
            address, length = section.VirtualAddress, section.Misc_VirtualSize
            raw = bytearray(pe.get_data(address, length))
            for first, count in ignored:
                start, end = max(first, address), min(first + count, address + len(raw))
                if start < end:
                    raw[start-address:end-address] = bytes(end-start)
            sections[name] = (address, length, section.Characteristics, bytes(raw))
        exports = sorted((item.name or b'', item.ordinal, item.address, item.forwarder or b'')
                         for item in pe.DIRECTORY_ENTRY_EXPORT.symbols)
        directories = [(i, d.VirtualAddress, d.Size) for i, d in enumerate(pe.OPTIONAL_HEADER.DATA_DIRECTORY)
                       if i not in (4, 6)]
        return {'machine': pe.FILE_HEADER.Machine, 'image_base': pe.OPTIONAL_HEADER.ImageBase,
                'entry': pe.OPTIONAL_HEADER.AddressOfEntryPoint, 'subsystem': pe.OPTIONAL_HEADER.Subsystem,
                'dll_characteristics': pe.OPTIONAL_HEADER.DllCharacteristics,
                'sections': sections, 'exports': exports, 'directories': directories}


def compact(output, compiler, run, environment):
    import pefile
    output, compiler = Path(output), Path(compiler)
    tool = compiler / 'llvm-strip.exe'
    if not tool.is_file():
        raise FileNotFoundError('缺少工具链自带llvm-strip，未自动安装')
    private = output / 'private-debug'
    private.mkdir(exist_ok=False)
    records = {}
    for name in ('DualSP.dll', 'DualSPUI.dll'):
        dll, original = output / name, private / name
        before = loader_contract(dll)
        shutil.copy2(dll, original)
        if digest(original) != digest(dll):
            raise ValueError('私有诊断副本摘要不同，停止精简')
        run([str(tool), '--strip-all', '--keep-section=.reloc', '--keep-section=.pdata',
             '--keep-section=.xdata', '--keep-section=.eh_frame', str(dll)], cwd=output, env=environment, check=True)
        if loader_contract(dll) != before:
            raise ValueError('精简改变加载契约，不能交付；原件仍在private-debug')
        with pefile.PE(str(dll)) as pe:
            if pe.FILE_HEADER.NumberOfSymbols or any(section_name(pe, s).startswith(b'.debug') for s in pe.sections):
                raise ValueError('交付DLL仍含符号表或调试节，不能冒称精简完成')
        records[name] = {'private_path': original.relative_to(output).as_posix(), 'private_sha256': digest(original),
                         'distribution_sha256': digest(dll), 'loader_contract_preserved': True}
    return {'schema': 1, 'tool_sha256': digest(tool), 'mode': 'strip-all-with-loader-contract', 'files': records}
