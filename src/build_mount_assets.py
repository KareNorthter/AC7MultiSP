"""只打包白名单中的16份自有游戏派生挂架；原始/生成资源不进入Git。"""
import hashlib
import io
import json
from pathlib import Path
import struct

from release_tools import ROOT, digest

PAK_NAME = 'AC7MultiSP_Mounts_P.pak'


def build(source, output):
    from research.build_runtime_mount_data import build as runtime_data
    names = runtime_data()['assets']
    proof = json.loads((ROOT/'evidence/sp-combination-static-split-experiment.json').read_text(encoding='utf-8'))
    reports = {v['recipe']['name']:v for v in proof['variants']}
    files = {}
    for name in names:
        report = reports[name]
        if not all(report.get(k) is True for k in ('asset_roundtrip','renamed_ac7_roundtrip','all_exports_payload_preserved')):
            raise ValueError('资源副本缺少完整离线依据：'+name)
        for suffix in ('.uasset','.uexp'):
            path = source/(name+suffix)
            if digest(path)!=report['encoded_hashes'][suffix]:
                raise ValueError('本地派生资源与已核对回执不符：'+name+suffix)
            files['Nimbus/Content/AC7MultiSP/Research/'+name+suffix] = path.read_bytes()

    def string(value):
        encoded = value.encode('utf-8')+b'\0'
        return struct.pack('<i',len(encoded))+encoded

    def entry(offset, data):
        return struct.pack('<QQQI20sBI',offset,len(data),len(data),0,hashlib.sha1(data).digest(),0,0)

    # AC7原生PAK版本4；独立文件使用未压缩/未加密的标准记录，内部资源仍为AC7编码。
    archive, rows = io.BytesIO(), []
    for name,data in sorted(files.items()):
        offset = archive.tell()
        archive.write(entry(0,data));archive.write(data)
        rows.append(string(name)+entry(offset,data))
    index = string('../../../')+struct.pack('<i',len(rows))+b''.join(rows)
    offset = archive.tell();archive.write(index)
    archive.write(struct.pack('<BIIQQ20s',0,0x5A6F12E1,4,offset,len(index),hashlib.sha1(index).digest()))
    target = output/PAK_NAME
    with target.open('xb') as stream:
        stream.write(archive.getvalue())
    # 用项目已有的独立读取器逐字节回读全部32个成员，不以写入成功替代核对。
    from inspect_hud_assets import inspect
    extracted = inspect(target,lambda name:True,output/'mount-pak-check')
    if extracted['mount']!='../../../' or {x['name']:x['sha256'] for x in extracted['assets']} != {
            name:hashlib.sha256(data).hexdigest() for name,data in files.items()}:
        raise ValueError('挂架PAK的独立回读不匹配')
    result = {'file':PAK_NAME,'sha256':digest(target),'variants':names,'file_count':len(files),
              'members':{name:hashlib.sha256(data).hexdigest() for name,data in files.items()},
              'independent_readback':True,'game_loaded':False}
    (output/'mount-assets-receipt.json').write_bytes((json.dumps(result,ensure_ascii=False,indent=2)+'\n').encode('utf-8'))
    return result
