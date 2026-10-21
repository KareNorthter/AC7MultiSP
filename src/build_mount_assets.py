"""只打包生成白名单中的自有游戏派生挂架；原始/生成资源不进入Git。"""
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
    for evidence in ('static-expansion-2.11.0.json', 'static-expansion-2.12.0.json', 'static-expansion-2.13.0.json',
                     'static-mount-five-aircraft-review.json', 'static-native-three-aircraft.json',
                     'local-four-mount-runtime.json', 'm31b-saam-offset-r1.json', 'm31b-saam-offset-r2.json',
                     'm31b-saam-offset-r3.json', 'f18e-layout-feedback-r1.json', 'f18etg-whole-rack-socket-r2.json'):
        extra = json.loads((ROOT/'evidence'/evidence).read_text(encoding='utf-8'))
        for report in extra['resource_reports']:
            name = report['recipe']['name']
            if name in reports:
                previous = reports[name]
                # 曾离线研究但未安装的同名副本，只有配方关键来源和实际编码字节全相同才复用。
                if (previous.get('encoded_hashes') != report.get('encoded_hashes') or any(
                        previous['recipe'].get(key) != report['recipe'].get(key)
                        for key in ('source_asset', 'source_extras_sha256', 'keep_boxes'))):
                    raise ValueError('同名历史副本与本轮资源来源或字节不符：' + name)
            reports[name] = report
    files = {}
    collision=json.loads((ROOT/'evidence/native-seven-collision-preservation.json').read_text(encoding='utf-8'))
    for entry in collision['entries']:
        if reports[entry['asset']]['encoded_hashes']!=entry['encoded_hashes']:
            raise ValueError('碰撞保持依据与实际打包资源字节不符：'+entry['asset'])
    feedback=json.loads((ROOT/'evidence/f18e-layout-feedback-r1.json').read_text(encoding='utf-8'))
    for entry in feedback['collision_preservation']:
        if reports[entry['asset']]['encoded_hashes']!=entry['encoded_hashes']:
            raise ValueError('两机反馈碰撞依据与实际资源字节不符：'+entry['asset'])
    fixed=json.loads((ROOT/'evidence/f18etg-whole-rack-socket-r2.json').read_text(encoding='utf-8'))
    report,=fixed['resource_reports']
    entry,=fixed['collision_preservation']
    if (fixed['previous_asset'] in names or fixed['asset'] not in names or
            not report['cooked_socket_references_verified'] or report['cooked_socket_references']!=[4,5] or
            entry['encoded_hashes']!=report['encoded_hashes']):
        raise ValueError('TGM错误旧资源仍在打包白名单或新引用资格不完整')
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
    # 用项目已有的独立读取器逐字节回读全部成员，不以写入成功替代核对。
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
