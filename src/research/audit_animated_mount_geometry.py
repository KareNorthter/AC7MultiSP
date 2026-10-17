"""汇总真实骨架挂架/机体的完整LOD解析边界；不编译或连接游戏。"""
import argparse
import hashlib
import json
from pathlib import Path

from analyze_sp_switch_animations import Assets
from read_cooked_skeletal_mount import FORMAT_SOURCE, read_skeletal_mesh, reference_component, skin_points


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def summarize(model):
    reference = dict(zip((b['name'] for b in model['bones']),reference_component(model['bones'])))
    lods = []
    for index,lod in enumerate(model['lods']):
        reconstructed = skin_points(model,lod,reference)
        error = max(abs(a-b) for p,q in zip(lod['points'],reconstructed) for a,b in zip(p,q))
        if error > .0001:
            raise ValueError('原生参考姿态蒙皮重建误差超限')
        normalized = [[tuple(p) for p in lod['points']],lod['indices'],lod['weights'],lod['sections']]
        lods.append({'lod':index,'vertices':len(lod['points']),'triangles':len(lod['indices'])//3,
                     'sections':[{k:s[k] for k in ('material','first','triangles','start','vertices','influences','bone_map')} for s in lod['sections']],
                     'adjacency_indices':lod['adjacency_count'], 'reference_reconstruction_max_error':error,
                     'geometry_and_skin_sha256':hashlib.sha256(json.dumps(normalized,separators=(',',':')).encode()).hexdigest(),
                     'reference_bounds':[[min(p[a] for p in lod['points']),max(p[a] for p in lod['points'])] for a in range(3)],
                     'single_bone_vertices':sum(sum(w>0 for _,w in ws)==1 for ws in lod['weights'])})
    return {'complete_payload':model['complete'],'payload_bytes':model['bytes'],'bone_count':len(model['bones']),
            'bone_names_and_parents':[{'name':b['name'],'parent_name':model['bones'][b['parent']]['name'] if b['parent']>=0 else None} for b in model['bones']],
            'materials':model['materials'],'lods':lods}


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--manifest',type=Path,action='append',required=True)
    parser.add_argument('--batch',type=Path,required=True)
    parser.add_argument('--aircraft',action='append',required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    if args.output.exists():
        raise ValueError('输出已存在，使用新文件保存本轮真实数据分析')
    assets=Assets(args.manifest)
    samples={a['code']:a for a in json.loads(args.batch.read_text(encoding='utf-8'))['aircraft']}
    names=[]
    for code in args.aircraft:
        names.append(code)
        names.extend(g['rack_mesh'] for g in samples[code]['groups'] if g['component_class']=='SkeletalMeshComponent')
    rows={name:summarize(read_skeletal_mesh(assets.load(name),name)) for name in dict.fromkeys(names)}
    result={'schema':1,'purpose':'原版活动架/Body完整几何、权重和参考重建的真实数据审查，不是运行测试或准入表',
            'compiled':False,'tests_run':False,'installed':False,'game_process_access':False,'assets_generated':False,
            'format_reference':FORMAT_SOURCE,'parser_sha256':digest(Path(__file__).with_name('read_cooked_skeletal_mount.py')),
            'analyzer_sha256':digest(Path(__file__)),'aircraft':args.aircraft,'meshes':rows,'sources':assets.used,
            'limits':['无纹理/法线语义重建；不修改源数据','骨骼顺序不能跨网格/序列套用，必须按唯一骨名及父骨名映射',
                      '参考姿态重建仅验证该文件的蒙皮解释，不代替动画扫掠或运行世界变换',
                      '所有LOD的几何/骨映射/权重均有边界核对，实际动态表面核对范围由各机方案另列']}
    args.output.write_bytes((json.dumps(result,ensure_ascii=False,indent=2)+'\n').encode('utf-8'))
    print(json.dumps({'meshes':len(rows),'lods':sum(len(m['lods']) for m in rows.values()),
                      'vertices':sum(sum(l['vertices'] for l in m['lods']) for m in rows.values())},ensure_ascii=False))


if __name__=='__main__':
    main()
