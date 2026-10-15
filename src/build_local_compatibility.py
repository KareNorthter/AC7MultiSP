"""从本机作者兼容表生成指定机体清单；不修改、提交或分发原始资源。"""
import argparse
import hashlib
import json
import os
import struct
import subprocess
from pathlib import Path


def table_rows(asset):
    """数据表必须只有一个可识别的行集合，结构不符时停止。"""
    tables = [export['Table'] for export in asset['Exports'] if 'Table' in export]
    if len(tables) != 1:
        raise ValueError('输入必须包含一个数据表')
    return tables[0]


def row_identity(row):
    """读取机体编号与代码，用于核对输入及写回结果。"""
    values = {prop['Name']: prop.get('Value') for prop in row['Value']}
    return values['PlaneID'], values['PlaneStringID']


def pak_string(value):
    """PAK 使用带终止符的 UTF-8 字符串。"""
    data = value.encode('utf-8') + b'\0'
    return struct.pack('<i', len(data)) + data


def pak_entry(offset, data):
    """生成 UE4 PAK v3 未压缩文件条目。"""
    return (struct.pack('<QQQI', offset, len(data), len(data), 0)
            + hashlib.sha1(data).digest() + struct.pack('<BI', 0, 0))


def write_pak(path, assets):
    """仅打包本次生成的机体表；外部 PAK 保持原样。"""
    index = bytearray(pak_string('../../../Nimbus/Content/'))
    index.extend(struct.pack('<i', len(assets)))
    with path.open('xb') as output:
        for name, data in assets.items():
            offset = output.tell()
            output.write(pak_entry(0, data))
            output.write(data)
            index.extend(pak_string(name))
            index.extend(pak_entry(offset, data))
        offset = output.tell()
        output.write(index)
        output.write(struct.pack('<IIQQ', 0x5A6F12E1, 3, offset, len(index)))
        output.write(hashlib.sha1(index).digest())


def main():
    """根据明确清单构建本地资源；不启动游戏或自动部署。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--manifest', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--uasset-gui', type=Path, required=True)
    parser.add_argument('--dotnet', type=Path, required=True)
    args = parser.parse_args()
    # 清单包含原版与本次明确安装的机体，不通过捕获异常猜测安装内容。
    manifest = json.loads(args.manifest.read_text(encoding='utf-8'))
    wanted = {int(number): code for number, code in manifest['planes'].items()}
    asset = json.loads(args.source.read_text(encoding='utf-8'))
    table = table_rows(asset)
    selected = [row for row in table['Data'] if row_identity(row)[0] in wanted]
    actual = dict(row_identity(row) for row in selected)
    if actual != wanted or len(selected) != len(wanted):
        raise ValueError('作者表与安装清单的机体身份不一致')
    table['Data'] = selected
    # 临时输出目录必须全新，防止误用上一轮文件。
    args.output.mkdir(parents=True, exist_ok=False)
    source = args.output / 'PlayerPlaneDataTable.json'
    target = args.output / 'PlayerPlaneDataTable.uasset'
    source.write_text(json.dumps(asset, ensure_ascii=False, indent=2), encoding='utf-8')
    environment = dict(os.environ, DOTNET_ROOT_X64=str(args.dotnet.resolve()))
    subprocess.run([str(args.uasset_gui.resolve()), 'fromjson', str(source.resolve()), str(target.resolve())],
                   env=environment, check=True, timeout=30)
    # 写回后重新解析，核对完整行值；这是资源构建核验，不运行游戏或测试模块。
    checked = args.output / 'checked.json'
    subprocess.run([str(args.uasset_gui.resolve()), 'tojson', str(target.resolve()), str(checked.resolve()), 'VER_UE4_18'],
                   env=environment, check=True, timeout=30)
    if table_rows(json.loads(checked.read_text(encoding='utf-8')))['Data'] != selected:
        raise ValueError('写回后的数据表行发生变化')
    header = target.read_bytes()
    exports = target.with_suffix('.uexp').read_bytes()
    magic = struct.pack('<I', 0x9E2A83C1)
    if header[:4] != magic or exports[-4:] != magic:
        raise ValueError('未得到标准 UE4 资源')
    # 作者兼容包本身使用标准 UE4 明文资源，这里保留相同格式。
    name = '~~~~~~~~~~~~~~~~AC7DualSP_LocalCatalogue_P.pak'
    pak = args.output / name
    write_pak(pak, {'Blueprint/Information/PlayerPlaneDataTable.uasset': header,
                    'Blueprint/Information/PlayerPlaneDataTable.uexp': exports})
    receipt = dict(planes=actual, count=len(actual), source_sha256=hashlib.sha256(args.source.read_bytes()).hexdigest(),
                   pak=str(pak.resolve()), sha256=hashlib.sha256(pak.read_bytes()).hexdigest(), roundtrip_equal=True)
    (args.output / 'build-receipt.json').write_text(json.dumps(receipt, ensure_ascii=False, indent=2), encoding='utf-8')
    print(json.dumps(receipt, ensure_ascii=False))


if __name__ == '__main__':
    main()
