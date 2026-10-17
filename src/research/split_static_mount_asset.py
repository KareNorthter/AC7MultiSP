"""离线分离 UE4.18 静态挂架的可见几何；只生成 Temp 中的研究副本。"""

import argparse
import base64
import copy
import hashlib
import json
import math
import os
from pathlib import Path
import re
import struct
import subprocess

ROOT = Path(__file__).resolve().parents[2]


def digest(data):
    return hashlib.sha256(data).hexdigest()


class Reader:
    """按序读取已确认的 cooked 布局，不搜索看起来像缓冲区的字节。"""
    def __init__(self, raw):
        self.raw, self.pos = raw, 0

    def take(self, size):
        if size < 0 or self.pos + size > len(self.raw):
            raise ValueError("资源数据越界")
        start = self.pos
        self.pos += size
        return self.raw[start:self.pos]

    def unpack(self, fmt):
        return struct.unpack('<' + fmt, self.take(struct.calcsize('<' + fmt)))

    def integer(self, low=0, high=10_000_000):
        value, = self.unpack('i')
        if not low <= value <= high:
            raise ValueError(f"整数不在支持范围：{value}，位置 {self.pos-4}")
        return value

    def bulk(self, expected_size=None, expected_count=None):
        size, count = self.unpack('ii')
        if size <= 0 or count < 0 or size * count > len(self.raw):
            raise ValueError("批量数组头无效")
        if expected_size is not None and size != expected_size:
            raise ValueError("批量元素宽度不符")
        if expected_count is not None and count != expected_count:
            raise ValueError("批量元素数量不符")
        offset = self.pos
        self.take(size * count)
        return offset, size, count


def load_mesh(path):
    document = json.loads(path.read_text(encoding='utf-8-sig'))
    if document['ObjectVersion'] != 'VER_UE4_ADDED_SOFT_OBJECT_PATH':
        raise ValueError("仅支持当前 AC7 UE4.18 资源版本")
    export = next(e for e in document['Exports'] if e['ObjectName'] == path.stem)
    cls = export['ClassIndex']
    if cls >= 0 or document['Imports'][-cls-1]['ObjectName'] != 'StaticMesh':
        raise ValueError("只支持 StaticMesh，不处理活动骨架")
    raw = base64.b64decode(export['Extras'], validate=True)
    return document, export, raw, parse_mesh(raw)


def parse_mesh(raw):
    """覆盖全部 LOD、正反索引、深度和 PN-AEN 邻接索引。"""
    r = Reader(raw)
    if r.take(2) != b'\x01\x00' or r.integer(0, 1) != 1:
        raise ValueError("要求已去编辑器数据、未去服务端数据的 cooked 网格")
    r.take(8 + 16)  # BodySetup、NavCollision 引用与 LightingGuid 原样保留。
    socket_count = r.integer(0, 1000)
    r.take(socket_count * 4)
    lod_count = r.integer(1, 8)
    lods = []
    for index in range(lod_count):
        general, special = r.unpack('BB')
        if general != 1 or special & ~5:
            raise ValueError("未支持的 LOD 剥离标志")
        sections = []
        for _ in range(r.integer(1, 64)):
            fields = r.unpack('7i')
            if min(fields[:5]) < 0 or any(x not in (0, 1) for x in fields[5:]):
                raise ValueError("section 格式不符")
            sections.append(dict(zip(('material', 'first', 'triangles', 'min', 'max', 'collision', 'shadow'), fields)))
        deviation, = r.unpack('f')
        if not math.isfinite(deviation):
            raise ValueError("LOD 误差无效")
        if r.integer() != 12:
            raise ValueError("要求 float3 位置顶点")
        vertices = r.integer(1, 1_000_000)
        offset, _, _ = r.bulk(12, vertices)
        points = list(struct.iter_unpack('<3f', raw[offset:offset + vertices*12]))
        if not all(math.isfinite(x) for p in points for x in p):
            raise ValueError("位置顶点不是有限数")
        if r.take(2) != b'\x01\x00':
            raise ValueError("纹理顶点标志不符")
        texcoords, stride, uv_vertices, full_uv, high_tangent = r.unpack('5i')
        expected = (16 if high_tangent else 8) + texcoords * (8 if full_uv else 4)
        if not 1 <= texcoords <= 8 or (full_uv not in (0, 1)) or (high_tangent not in (0, 1)) or stride != expected or uv_vertices != vertices:
            raise ValueError("纹理顶点布局不符")
        r.bulk(stride, vertices)
        if r.take(2) != b'\x01\x00':
            raise ValueError("颜色顶点标志不符")
        color_stride, color_count = r.unpack('ii')
        if (color_stride, color_count) != (0, 0):
            if color_stride != 4 or color_count != vertices:
                raise ValueError("颜色顶点布局不符")
            r.bulk(4, vertices)
        names = ['main']
        if not special & 4:
            names += ['reversed', 'depth', 'reversed_depth']
        else:
            names += ['depth']
        if not special & 1:
            names += ['adjacency']
        buffers = []
        for name in names:
            wide = r.integer(0, 1)
            offset, _, byte_count = r.bulk(1)
            width = 4 if wide else 2
            group = 12 if name == 'adjacency' else 3
            if byte_count % (width * group):
                raise ValueError("索引数量不是完整三角面或邻接块")
            values = list(struct.unpack_from('<' + ('I' if wide else 'H') * (byte_count//width), raw, offset))
            if values and max(values) >= vertices:
                raise ValueError("索引引用越界")
            buffers.append({'name': name, 'offset': offset, 'width': width, 'group': group, 'values': values})
        main = buffers[0]['values']
        if sum(s['triangles'] for s in sections)*3 != len(main):
            raise ValueError("section 三角面总数与主缓冲不符")
        cursor = 0
        for s in sections:
            if s['first'] != cursor:
                raise ValueError("section 索引范围有空洞或重叠")
            end = cursor+s['triangles']*3
            used = main[cursor:end]
            if not used or min(used) < s['min'] or max(used) > s['max']:
                raise ValueError("section 顶点范围不符")
            cursor = end
        # 本批资源没有面积采样表；非空表需要重建概率，不做猜测。
        for _ in range(len(sections)+1):
            if r.integer() != 0 or r.integer() != 0 or r.unpack('f')[0] != 0:
                raise ValueError("存在面积采样表，需专门重算")
        lods.append({'index': index, 'points': points, 'sections': sections, 'buffers': buffers})
    tail_start = r.pos
    if r.take(2) != b'\x01\x00':
        raise ValueError("距离场标志不符")
    if any(r.integer(0, 1) for _ in range(lod_count)):
        raise ValueError("存在距离场，需重新生成后才能分离")
    bounds = r.unpack('7f')
    if not all(math.isfinite(x) for x in bounds) or min(bounds[3:]) < 0:
        raise ValueError("渲染包围范围无效")
    # 简化 LOD 可能略超出原 Bounds；记录原生偏差，不借此改大或缩小包围范围。
    overflow = [max(0, max(abs(p[a]-bounds[a])-bounds[3+a] for p in lod['points'] for a in range(3))) for lod in lods]
    if overflow[0] > 0.005:
        raise ValueError(f"LOD0 超出原包围范围：{overflow[0]}")
    return {'socket_count': socket_count, 'lods': lods, 'tail_start': tail_start,
            'bounds': bounds, 'original_lod_bounds_overflow': overflow}


def summary(mesh):
    result = {'socket_count': mesh['socket_count'], 'original_lod_bounds_overflow': mesh['original_lod_bounds_overflow'], 'lods': []}
    for lod in mesh['lods']:
        points = lod['points']
        result['lods'].append({'lod': lod['index'], 'vertices': len(points), 'sections': lod['sections'],
                               'bounds': [[min(p[a] for p in points), max(p[a] for p in points)] for a in range(3)],
                               'buffers': {b['name']: len(b['values'])//b['group'] for b in lod['buffers']}})
    return result


def split_geometry(raw, mesh, boxes):
    """只保留完整分区；跨界三角面或邻接块立即拒绝，不切断几何。"""
    if not boxes or any(len(b) != 6 or not all(math.isfinite(x) for x in b) or any(b[a] > b[a+3] for a in range(3)) for b in boxes):
        raise ValueError("保留区应为若干完整六数包围盒")
    changed = bytearray(raw)
    reports = []
    for lod in mesh['lods']:
        keep = [any(all(b[a] <= p[a] <= b[a+3] for a in range(3)) for b in boxes) for p in lod['points']]
        counts = {}
        for buffer in lod['buffers']:
            values, group, width = buffer['values'], buffer['group'], buffer['width']
            kept = removed = 0
            for start in range(0, len(values), group):
                block = values[start:start+group]
                flags = {keep[i] for i in block}
                if len(flags) != 1:
                    raise ValueError(f"LOD{lod['index']} {buffer['name']} 第{start//group}块跨越分区边界；需修改分区或人工重建")
                if True in flags:
                    kept += 1
                else:
                    removed += 1
                    fmt = '<' + ('I' if width == 4 else 'H') * group
                    struct.pack_into(fmt, changed, buffer['offset']+start*width, *([block[0]]*group))
            counts[buffer['name']] = {'kept': kept, 'degenerated': removed}
        if not counts['main']['kept'] or not counts['main']['degenerated']:
            raise ValueError("每级 LOD 必须同时保留和排除几何")
        reports.append({'lod': lod['index'], 'buffers': counts})
    # 长度、元数据和非索引字节全部不变；重新顺序解析生成副本。
    candidate = bytes(changed)
    parsed = parse_mesh(candidate)
    allowed = set()
    for lod in mesh['lods']:
        for b in lod['buffers']:
            allowed.update(range(b['offset'], b['offset']+len(b['values'])*b['width']))
    if any(i not in allowed for i, (a, b) in enumerate(zip(raw, candidate)) if a != b):
        raise ValueError("修改越过索引缓冲边界")
    if len(candidate) != len(raw) or len(parsed['lods']) != len(mesh['lods']):
        raise ValueError("生成副本改变了序列化布局")
    return candidate, reports


def run_converter(tools, arguments, expected):
    env = dict(os.environ, DOTNET_ROOT_X64=str(tools/'dotnet8'))
    result = subprocess.run([str(tools/'assets/UAssetGUI.exe'), *map(str, arguments)], env=env,
                            capture_output=True, encoding='utf-8', errors='replace', timeout=45,
                            creationflags=subprocess.CREATE_NO_WINDOW)
    if result.returncode or not expected.is_file():
        raise RuntimeError(f"资源往返转换失败：{result.stdout}\n{result.stderr}")


def encode_ac7_pair(asset, exported, key, name):
    """按目标文件名重新编码完整资源对；密钥仅从调用方本地工具读取。"""
    from inspect_mount_assets import signed32
    if len(key) != 217*1024 or asset[:4] != struct.pack('<I', 0x9E2A83C1):
        raise ValueError('缺少有效本地密钥或输入不是解码资源')
    state = 0
    for c in name.upper():
        state = signed32(state ^ ord(c))
        bit = (signed32(~signed32(signed32(state*8) ^ state)) >> 7) & 1
        state = signed32(signed32(state*2) | bit)
    first, second = (((state*7) % 2**32)+4) % 217, (((state*11) % 2**32)+4) % 1024

    def transform(data):
        nonlocal first, second
        result = bytearray(len(data))
        for i, byte in enumerate(data):
            result[i] = byte ^ key[first*1024+second] ^ 0x77
            first, second = (first+1) % 217, (second+1) % 1024
        return bytes(result)

    return b'ACE7'+transform(asset[4:]), transform(exported)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-json', type=Path, required=True)
    parser.add_argument('--recipe', type=Path, help='资产名与 keep_boxes 的研究配方')
    parser.add_argument('--variant', help='从配方集合中选择指定 name')
    parser.add_argument('--output', type=Path, required=True, help='必须为本工作区 Temp 内的新目录')
    parser.add_argument('--tools-root', type=Path, help='提供后执行资源写出和重新读取，不编译或打包')
    args = parser.parse_args()
    output = args.output.resolve()
    if not output.is_relative_to((ROOT/'Temp').resolve()):
        raise ValueError("研究副本只能写入本工作区 Temp")
    document, export, raw, mesh = load_mesh(args.source_json)
    report = {'schema': 1, 'status': 'offline_geometry_only', 'source': args.source_json.stem,
              'json_sha256': digest(args.source_json.read_bytes()), 'extras_sha256': digest(raw),
              'structure': summary(mesh), 'compiled': False, 'installed': False, 'game_verified': False}
    output.mkdir(parents=True, exist_ok=False)
    if args.recipe:
        recipe = json.loads(args.recipe.read_text(encoding='utf-8'))
        if args.variant:
            recipe = next(x for x in recipe['variants'] if x['name'] == args.variant)
        if recipe.get('source_asset') != export['ObjectName'] or recipe.get('source_extras_sha256') != digest(raw):
            raise ValueError("配方与原始资源身份或摘要不符")
        name = recipe['name']
        if not re.fullmatch(r'MSP_[A-Za-z0-9_]+', name) or name in document['NameMap']:
            raise ValueError("副本必须使用全新 MSP_ 资源名")
        candidate, counts = split_geometry(raw, mesh, recipe['keep_boxes'])
        clone = copy.deepcopy(document)
        target = next(e for e in clone['Exports'] if e['ObjectName'] == export['ObjectName'])
        target['Extras'] = base64.b64encode(candidate).decode('ascii')
        target['ObjectName'] = name
        source_name = export['ObjectName']
        # 自身包名与导出名改为独立路径；材质、其他导入和子对象引用保持原样。
        old_paths = [n for n in clone['NameMap'] if n.endswith('/'+source_name)]
        if len(old_paths) != 1:
            raise ValueError("无法唯一确认源资产自身包路径")
        package = '/Game/AC7MultiSP/Research/'+name
        clone['NameMap'] = [name if n == source_name else package if n == old_paths[0] else n for n in clone['NameMap']]
        json_path = output/(name+'.json')
        json_path.write_text(json.dumps(clone, ensure_ascii=False, indent=2), encoding='utf-8', newline='\n')
        report.update(recipe=recipe, package=package, candidate_extras_sha256=digest(candidate), per_lod=counts,
                      changed_bytes=sum(a != b for a, b in zip(raw, candidate)), byte_length_preserved=True,
                      non_index_bytes_preserved=True, sockets_and_materials_preserved=True, asset_roundtrip=False)
        if args.tools_root:
            tools = args.tools_root.resolve(strict=True)
            asset = output/(name+'.uasset')
            roundtrip = output/'roundtrip'
            roundtrip.mkdir()
            loaded_json = roundtrip/(name+'.json')
            run_converter(tools, ['fromjson', json_path, asset], asset)
            run_converter(tools, ['tojson', asset, loaded_json, 'VER_UE4_18'], loaded_json)
            reread, rexport, rraw, _ = load_mesh(loaded_json)
            if reread['NameMap'] != clone['NameMap'] or rraw != candidate or reread['Imports'] != clone['Imports']:
                raise ValueError("重新读取后的资源名、导入或渲染数据不符")
            for key in ('Data', 'ClassIndex', 'OuterIndex'):
                if rexport[key] != target[key]:
                    raise ValueError("重新读取后的挂点材质属性或导出身份不符")
            if len(reread['Exports']) != len(clone['Exports']):
                raise ValueError("子对象导出数量改变")
            for before, after in zip(clone['Exports'], reread['Exports']):
                for key in ('ObjectName', 'Data', 'Extras', 'OuterIndex', 'ClassIndex', 'SuperIndex', 'TemplateIndex'):
                    if before.get(key) != after.get(key):
                        raise ValueError(f"重新读取后的子对象 {key} 改变")
            report.update(asset_roundtrip=True, all_exports_payload_preserved=True,
                          output_hashes={p.suffix: digest(p.read_bytes()) for p in (asset, asset.with_suffix('.uexp'))})
            key = (tools/'assets/AC7Key.bin').read_bytes()
            original_asset = args.source_json.with_suffix('.uasset')
            original_exp = args.source_json.with_suffix('.uexp')
            original_encoded = encode_ac7_pair(original_asset.read_bytes(), original_exp.read_bytes(), key, source_name)
            encrypted_source = original_asset.parent.parent/original_asset.name
            if original_encoded != (encrypted_source.read_bytes(), encrypted_source.with_suffix('.uexp').read_bytes()):
                raise ValueError('未能逐字节复原原生编码，停止生成 AC7 副本')
            encoded = output/'encoded'
            encoded.mkdir()
            payload = encode_ac7_pair(asset.read_bytes(), asset.with_suffix('.uexp').read_bytes(), key, name)
            encoded_asset = encoded/asset.name
            encoded_asset.write_bytes(payload[0])
            encoded_asset.with_suffix('.uexp').write_bytes(payload[1])
            # 独立使用项目原有解码器回读，避免仅靠本函数自我往返。
            from inspect_mount_assets import decode_copy
            decoded = decode_copy(encoded_asset, key)
            if decoded.read_bytes() != asset.read_bytes() or decoded.with_suffix('.uexp').read_bytes() != asset.with_suffix('.uexp').read_bytes():
                raise ValueError('AC7 新名编码与原有解码器回读不一致')
            report.update(source_ac7_encoding_reproduced=True, renamed_ac7_roundtrip=True,
                          encoded_hashes={'.uasset': digest(payload[0]), '.uexp': digest(payload[1])})
    (output/'report.json').write_text(json.dumps(report, ensure_ascii=False, indent=2)+'\n', encoding='utf-8', newline='\n')
    print(json.dumps({'output': str(output), 'source': report['source'], 'lods': len(mesh['lods']),
                      'asset_roundtrip': report.get('asset_roundtrip')}, ensure_ascii=False))


if __name__ == '__main__':
    main()
