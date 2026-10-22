"""离线筛查已登记Mod PAK的炮类SP；只解析副本，不运行Mod或连接游戏。"""
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor, as_completed
import argparse
import hashlib
import json
import os
import subprocess
import sys
import time
import zlib


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--index',type=Path,required=True)
    parser.add_argument('--tools-root',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    if args.output.exists():raise ValueError('输出目录已存在，不覆盖旧研究')
    args.output.mkdir(parents=True)
    project=Path(__file__).resolve().parents[2]
    sys.path[:0]=[str(args.tools_root/'test-python'),str(project/'src')]
    from inspect_hud_assets import decrypt,entry
    from inspect_mount_assets import decode_copy
    paks=json.loads(args.index.read_text(encoding='utf-8-sig'))
    jobs={};sources=[];excluded=[]
    for i,pak in enumerate(paks):
        source=Path(pak['path'])
        if not source.is_file()or source.stat().st_size!=pak['bytes']:
            raise ValueError('PAK来源不存在或大小变化：'+str(source))
        sources.append(dict(archive=pak['archive'],path=pak['path'],sha256_from_prior_index=pak['sha256'],bytes=pak['bytes']))
        for name,info in pak.get('members',{}).items():
            if not name.endswith('.uasset'):continue
            kind='weapon'if '/Blueprint/Weapons/'in name else 'pawn'if '/Blueprint/Player/Pawn/'in name else None
            if not kind:continue
            export=name[:-7]+'.uexp'
            if export not in pak['members']:
                excluded.append(dict(pak=i,name=name,reason='缺少独立uexp，未猜测格式'));continue
            ei=pak['members'][export]
            key=hashlib.sha256((name+'|'+info['digest']+'|'+ei['digest']).encode()).hexdigest()
            occurrence=dict(pak=i,archive=pak['archive'],path=pak['path'],name=name)
            if key not in jobs:jobs[key]=dict(key=key,kind=kind,name=name,pak=i,info=info,export_info=ei,occurrences=[])
            jobs[key]['occurrences'].append(occurrence)

    def member(path,info):
        with path.open('rb')as stream:
            stream.seek(info['offset']);header=entry(stream)
            if header['size']!=info['size']:raise ValueError('条目大小与索引不符')
            if info['compression']:
                if info['compression']!=1:raise ValueError('不支持的压缩方式')
                blocks=[];compressed=[]
                for start,end in info['blocks']:
                    stream.seek(start);raw=stream.read((end-start+15)//16*16 if info['encrypted']else end-start)
                    if info['encrypted']:raw=decrypt(raw)[:end-start]
                    compressed.append(raw)
                    blocks.append(zlib.decompress(raw))
                data=b''.join(blocks)
                digest_payload=b''.join(compressed)
            else:
                data=stream.read((info['size']+15)//16*16 if info['encrypted']else info['size'])
                if info['encrypted']:data=decrypt(data)
                data=data[:info['size']]
                digest_payload=data
        if len(data)!=info['original']:raise ValueError('资源解压后长度不符')
        # PAK压缩条目的SHA1覆盖压缩载荷；解压文件另记SHA256，不能混用。
        if hashlib.sha1(digest_payload).hexdigest()!=info['digest']:raise ValueError('PAK载荷SHA1与原索引不符')
        return data

    key_bytes=(args.tools_root/'assets/AC7Key.bin').read_bytes()
    converter=args.tools_root/'assets/UAssetGUI.exe'
    environment=dict(os.environ,DOTNET_ROOT_X64=str(args.tools_root/'dotnet8'))
    if not converter.is_file()or len(key_bytes)!=217*1024:raise ValueError('本机资源解析依赖缺失')

    def parse_one(job):
        result={k:v for k,v in job.items()if k not in ('info','export_info')}
        folder=args.output/'assets'/job['key'][:20];folder.mkdir(parents=True)
        target=folder/Path(job['name']).name
        try:
            source=Path(paks[job['pak']]['path'])
            raw=member(source,job['info']);export=member(source,job['export_info'])
            target.write_bytes(raw);target.with_suffix('.uexp').write_bytes(export)
            result['asset_sha256']=hashlib.sha256(raw).hexdigest();result['export_sha256']=hashlib.sha256(export).hexdigest()
            if raw[:4]==b'ACE7':target=decode_copy(target,key_bytes)
            elif raw[:4]!=bytes.fromhex('c1832a9e'):raise ValueError('资源头部不支持')
            header=target.read_bytes()
            # 没有类导出的辅助资源、没有SP覆盖的皮肤不调用较慢的JSON转换器。
            if b'BlueprintGeneratedClass'not in header:
                result['status']='非蓝图类资源';return result
            if job['kind']=='pawn'and '/Skin/'in job['name']and not any(token in header for token in
                (b'SpecialWeaponConfig',b'WeaponClass',b'plwp_',b'MainWeapon',b'GunWeapon',b'DropTankWeapon')):
                result['status']='皮肤头部未见兵装引用或覆盖标记';return result
            destination=target.with_suffix('.json')
            run=subprocess.run([str(converter),'tojson',str(target),str(destination),'VER_UE4_18'],
                env=environment,capture_output=True,text=True,encoding='utf-8',errors='replace',timeout=60,
                creationflags=subprocess.CREATE_NO_WINDOW)
            if run.returncode or not destination.is_file():raise ValueError('JSON解析失败：'+run.stdout[-500:]+run.stderr[-500:])
            doc=json.loads(destination.read_text(encoding='utf-8-sig'))
            result['json']=str(destination);result['json_sha256']=hashlib.sha256(destination.read_bytes()).hexdigest()
            def ref(n):
                if not n:return None
                obj=doc['Imports'][-n-1]if n<0 else doc['Exports'][n-1]
                outer=obj.get('OuterIndex',0)
                return dict(name=obj['ObjectName'],package=ref(outer)if outer<0 else None)
            def simple(prop):
                value=prop.get('Value')
                if 'ObjectPropertyData'in prop.get('$type','')and isinstance(value,int):return ref(value)
                if isinstance(value,list)and all(isinstance(v,dict)and 'Name'in v for v in value):
                    return {v['Name']+'['+str(v.get('ArrayIndex',0))+']':simple(v)for v in value}
                return value
            result['classes']=[]
            for e in doc['Exports']:
                if e.get('$type','').split(',')[0].endswith('.ClassExport'):
                    cdo=doc['Exports'][e['ClassDefaultObject']-1]
                    result['classes'].append(dict(name=e['ObjectName'],parent=ref(e.get('SuperStruct')),
                        default_name=cdo['ObjectName'],defaults={p['Name']:simple(p)for p in cdo.get('Data',[])}))
            result['components']=[];result['functions']=[]
            def calls(node):
                found=[]
                if isinstance(node,dict):
                    if 'StackNode'in node:found.append(ref(node['StackNode']))
                    if 'VirtualFunctionName'in node:found.append(dict(name=node['VirtualFunctionName']))
                    for value in node.values():found.extend(calls(value))
                elif isinstance(node,list):
                    for value in node:found.extend(calls(value))
                return found
            for e in doc['Exports']:
                props=[p for p in e.get('Data',[])if any(t in p['Name']for t in ('WeaponConfig','WeaponClass','WeaponActivator'))]
                if props:result['components'].append(dict(name=e['ObjectName'],properties={p['Name']:simple(p)for p in props}))
                if 'FunctionExport'in e.get('$type',''):
                    result['functions'].append(dict(name=e['ObjectName'],size=e.get('ScriptBytecodeSize'),calls=calls(e.get('ScriptBytecode'))))
            result['imports']=[dict(name=i['ObjectName'],class_name=i['ClassName'],class_package=i['ClassPackage'])for i in doc['Imports']]
            result['status']='已解析';return result
        except Exception as error:
            result['status']='未解析';result['error']=str(error);return result

    results=[];started=time.monotonic();last=started
    print(json.dumps(dict(paks=len(paks),jobs=len(jobs),weapons=sum(j['kind']=='weapon'for j in jobs.values()),
        pawns=sum(j['kind']=='pawn'for j in jobs.values()),missing_export=len(excluded)),ensure_ascii=False),flush=True)
    with ThreadPoolExecutor(max_workers=4)as pool:
        futures=[pool.submit(parse_one,job)for job in jobs.values()]
        for future in as_completed(futures):
            results.append(future.result())
            if len(results)%100==0 or time.monotonic()-last>=25:
                print(json.dumps(dict(done=len(results),total=len(jobs),parsed=sum(r['status']=='已解析'for r in results),
                    errors=sum(r['status']=='未解析'for r in results),seconds=round(time.monotonic()-started,1)),ensure_ascii=False),flush=True)
                last=time.monotonic()
    result=dict(index=str(args.index),index_sha256=hashlib.sha256(args.index.read_bytes()).hexdigest(),sources=sources,
        excluded=excluded,assets=sorted(results,key=lambda r:(r['pak'],r['name'])))
    (args.output/'survey.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf-8',newline='\n')
    errors=[dict(name=r['name'],error=r['error'])for r in results if r['status']=='未解析']
    print(json.dumps(dict(output=str(args.output/'survey.json'),done=len(results),error_count=len(errors),errors=errors[:10]),ensure_ascii=False),flush=True)


if __name__=='__main__':main()
