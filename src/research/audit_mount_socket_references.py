"""从实际成套编码资源复核引用与全部LOD；只写Temp和新证据，不编译或操作游戏。"""
import argparse
import base64
from concurrent.futures import ThreadPoolExecutor, as_completed
import hashlib
import json
from pathlib import Path
import shutil
import struct

from inspect_mount_assets import decode_copy
from split_static_mount_asset import load_mesh, parse_mesh, run_converter

ROOT=Path(__file__).resolve().parents[2]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def audit_document(path):
    document=json.loads(path.read_text(encoding='utf-8-sig'))
    export=next(e for e in document['Exports'] if e['ObjectName']==path.stem)
    raw=base64.b64decode(export['Extras'])
    mesh=parse_mesh(raw)
    errors=[]
    # 通用JSON回读不会理解Extras；必须显式读cooked引用并与真正的导出/属性核对。
    try:
        load_mesh(path)
    except ValueError as error:
        errors.append(str(error))
    imports,exports=document['Imports'],document['Exports']
    def index(value,where):
        if type(value) is not int or not -len(imports)<=value<=len(exports):
            errors.append('包对象引用越界：'+where+'='+str(value))
    for i,item in enumerate(imports):
        index(item['OuterIndex'],f'Imports[{i}].OuterIndex')
    for i,item in enumerate(exports):
        for key in ('ClassIndex','OuterIndex','SuperIndex','TemplateIndex'):
            index(item[key],f'Exports[{i}].{key}')
    object_properties=0
    def walk(node,where):
        nonlocal object_properties
        if isinstance(node,dict):
            kind=node.get('$type','').split(',',1)[0].rsplit('.',1)[-1]
            if kind in ('ObjectPropertyData','ClassPropertyData','InterfacePropertyData'):
                object_properties+=1
                index(node['Value'],where+'.Value')
            for key,value in node.items():
                if isinstance(value,(dict,list)):walk(value,where+'.'+key)
        elif isinstance(node,list):
            for i,value in enumerate(node):walk(value,where+f'[{i}]')
    walk(exports,'Exports')
    owner=exports.index(export)+1
    body,nav=struct.unpack_from('<2i',raw,6)
    for ref,expected in ((body,'BodySetup'),(nav,'NavCollision')):
        index(ref,'cooked.'+expected)
        if ref>0 and ref<=len(exports):
            item=exports[ref-1];cls=item['ClassIndex']
            if not (-len(imports)<=cls<0 and imports[-cls-1]['ObjectName']==expected and item['OuterIndex']==owner):
                errors.append('cooked碰撞/导航对象类型或父链不符：'+expected)
    sockets=[]
    for ref in mesh['socket_references']:
        if 0<ref<=len(exports):
            name=next((p['Value'] for p in exports[ref-1].get('Data',[]) if p['Name']=='SocketName'),None)
            sockets.append(name)
        else:sockets.append(None)
    if None in sockets or len(set(sockets))!=len(sockets):
        errors.append('cooked Socket名称缺失或重复')
    declared=next((p['Value'] for p in export['Data'] if p['Name']=='Sockets'),[])
    return dict(passed=not errors,errors=errors,cooked_socket_references=mesh['socket_references'],
                property_socket_references=[p['Value'] for p in declared],socket_names=sockets,
                export_count=len(exports),import_count=len(imports),object_properties_checked=object_properties,
                all_lod_count=len(mesh['lods']),all_lod_vertex_count=sum(len(l['points']) for l in mesh['lods']),
                all_lod_main_triangles=sum(len(l['buffers'][0]['values'])//3 for l in mesh['lods']),
                cooked_body_reference=body,cooked_nav_reference=nav,json_sha256=digest(path))


def main(args):
    if args.output.exists() or args.evidence.exists() or not args.output.resolve().is_relative_to(ROOT/'Temp'):
        raise ValueError('只使用Temp内新目录和新证据，不覆盖旧记录')
    baseline=json.loads(args.baseline_receipt.read_text(encoding='utf-8'))
    header=(ROOT/'src/mount_expansion_data.hpp').read_text(encoding='utf-8')
    current=json.loads(header.split('R"MSP(',1)[1].split(')MSP"',1)[0])
    fixed=json.loads((ROOT/'evidence/f18etg-whole-rack-socket-r2.json').read_text(encoding='utf-8'))
    old,new=fixed['previous_asset'],fixed['asset']
    if (len(baseline['variants'])!=77 or len(current['assets'])!=77 or old not in baseline['variants'] or
            new in baseline['variants'] or old in current['assets'] or new not in current['assets'] or
            set(baseline['variants'])-{old}!=set(current['assets'])-{new}):
        raise ValueError('安装77项与r2候选77项并非精确的一项替换')
    key=(args.tools_root/'assets/AC7Key.bin').read_bytes()
    args.output.mkdir(parents=True)
    names=baseline['variants']+[new]
    def one(name):
        origin=args.candidate_source if name==new else args.installed_source
        expected=fixed['resource_reports'][0]['encoded_hashes'] if name==new else {
            s:baseline['members']['Nimbus/Content/AC7MultiSP/Research/'+name+s] for s in ('.uasset','.uexp')}
        directory=args.output/name;directory.mkdir()
        for suffix in ('.uasset','.uexp'):
            source=origin/(name+suffix)
            if digest(source)!=expected[suffix]:raise ValueError('资源输入与冻结摘要不符：'+name+suffix)
            shutil.copyfile(source,directory/source.name)
        decoded=decode_copy(directory/(name+'.uasset'),key)
        exported=decoded.with_suffix('.json')
        run_converter(args.tools_root,['tojson',decoded,exported,'VER_UE4_18'],exported)
        row=audit_document(exported)
        row.update(asset=name,encoded_hashes=expected,installed=name!=new,candidate=name!=old)
        return row
    results={}
    with ThreadPoolExecutor(max_workers=2) as pool:
        futures={pool.submit(one,name):name for name in names}
        for future in as_completed(futures):
            name=futures[future]
            results[name]=future.result()
            if len(results)%10==0 or len(results)==len(names):
                print(f'已读取完整资源对并核对引用 {len(results)}/{len(names)}',flush=True)
    active_pairs=0
    for plane in current['planes']:
        for pool,group in enumerate(plane['groups']):
            variants={p['variants'][pool] for p in plane['plans'][1:] if p['variants'][pool]>=0}
            for variant in variants:
                name=current['assets'][variant]
                actual=[n.lower() for n in results[name]['socket_names'] if isinstance(n,str)]
                if actual!=group['sockets']:
                    results[name]['passed']=False
                    results[name]['errors'].append('与运行白名单Socket列表不一致：'+plane['code']+'/SWP'+str(pool+1))
                active_pairs+=1
    installed_bad=[n for n in baseline['variants'] if not results[n]['passed']]
    candidate_bad=[n for n in current['assets'] if not results[n]['passed']]
    report=dict(schema=1,status='completed',scope='77项安装资源加1项r2替代资源，共78份完整编码资源对',
                installed_baseline_receipt_sha256=digest(args.baseline_receipt),installed_pak_sha256=baseline['sha256'],
                installed_asset_count=77,installed_failures=installed_bad,candidate_asset_names=current['assets'],
                candidate_asset_count=77,candidate_failures=candidate_bad,unique_resources_audited=78,
                active_group_asset_pairs_checked=active_pairs,rows=[results[n] for n in names],
                same_family_generator_review='当前研究生成脚本中只有TGM完整借架跨源拼接几何；已使用解析器边界。其余分区只改索引，M31B偏移只改Socket属性',
                limits=['离线核对固定资源引用、类型/父链、包引用边界和全部LOD结构，不替代运行时对象生命周期/动画/显隐/离架/性能验证',
                        '其他机体原生资源、第三方机体和第三方PAK不在这78项派生资源核对范围内',
                        '错误旧副本只有加载到相关组合时才进入该风险路径；不能将未加载它的试玩当作此副本已验证'],
                compiled=False,pak_built=False,tests_run=False,installed=False,game_process_access=False,game_operated=False)
    args.evidence.write_bytes((json.dumps(report,ensure_ascii=False,indent=2)+'\n').encode('utf-8'))
    print(json.dumps(dict(installed_failures=installed_bad,candidate_failures=candidate_bad,
                          candidate_passed=77-len(candidate_bad),resources=78),ensure_ascii=False),flush=True)


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--baseline-receipt',type=Path,required=True)
    p.add_argument('--installed-source',type=Path,required=True)
    p.add_argument('--candidate-source',type=Path,required=True)
    p.add_argument('--tools-root',type=Path,required=True)
    p.add_argument('--output',type=Path,required=True)
    p.add_argument('--evidence',type=Path,required=True)
    main(p.parse_args())
