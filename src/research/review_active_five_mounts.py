"""五机活动挂架第一版静态方案；只读原生提取副本，不生成资产或修改运行白名单。"""
import argparse
from copy import deepcopy
import hashlib
import itertools
import json
from pathlib import Path
import sys
import numpy as np


def main(args):
    root=args.project_root.resolve()
    sys.path.insert(0,str(root/'src/research'))
    import analyze_sp_switch_animations as anim
    import review_animated_sukhoi as geo
    from summarize_mount_assets import properties,resolve,value
    from split_static_mount_asset import load_mesh,split_geometry
    geo.assets=anim.Assets(args.manifest)
    source=json.loads((root/'evidence/mount-expansion-batch3.json').read_text(encoding='utf-8-sig'))
    targets=['asfx','f14d','f18f','su33','f18x']
    native={a['code']:a for a in source['aircraft'] if a['code'] in targets}
    config=geo.assets.load('PlaneBodyConfig')
    channels={row['BoneName']:row['FlightEngineOuputName'] for row in
              (value(x) for x in config['Exports'][0]['Table']['Data'])}
    W=1000
    # 每个四项布局均独立设计；三种单挂在下面从完整原生集合生成。
    layouts={
        'asfx':{
            3:[['sp11','sp12'],'all',[]],5:['all',[],['sp31','sp32']],
            6:[[],'all',['sp31','sp32']],7:[['sp11','sp12'],'all',['sp31','sp32']]},
        'f14d':{
            3:[['sp11','sp12','sp15','sp16'],['sp21','sp22'],[]],
            5:[['sp13','sp14','sp15','sp16'],[],'all'],6:[[],['sp21','sp22'],'all'],
            7:[['sp15','sp16'],['sp21','sp22'],'all']},
        'f18f':{
            3:['all',['sp23','sp24'],[]],5:['all',[],['sp01']],
            6:[[],'all',['sp01']],7:['all',['sp23','sp24'],['sp01']]},
        'su33':{
            3:[['sp11','sp12'],'all',[]],5:['all',[],['sp01']],
            6:[[],'all',['sp01']],7:[['sp11','sp12'],'all',['sp01']]},
        'f18x':{
            3:[['sp11','sp12'],['sp21','sp22','sp23','sp24','sp25','sp26','sp33','sp34','sp35','sp36','sp37','sp38'],[]],
            5:['all',[],'all'],6:[[],'all','all'],
            7:[['sp11','sp12'],['sp21','sp22','sp23','sp24','sp25','sp26','sp33','sp34','sp35','sp36','sp37','sp38'],'all']} }
    partitions={
        'asfx':{'6aam_outer':(0,[[-W,-W,-W,W,-48,W],[-W,48,-W,W,W,W]])},
        'f14d':{'laam_wing_rear':(0,[[-W,-10,-W,14,10,W],[-W,-W,-W,W,-20,W],[-W,20,-W,W,W,W]]),
                'laam_belly':(0,[[-W,-10,-W,W,10,W]]),'laam_rear':(0,[[-W,-10,-W,14,10,W]]),
                'gpb_front':(1,[[14,-10,-W,W,10,W]])},
        'f18f':{'lasm_inner':(1,[[-W,-30,-W,W,30,W]])},
        'su33':{'hvaa_outer':(0,[[-W,-W,-W,W,-40,W],[-W,40,-W,W,W,W]]),
                'lasm_mid_belly':(1,[[-W,-40,-W,W,40,W]])},
        'f18x':{'lacm_outer':(0,[[-W,-W,-W,W,-30,W],[-W,30,-W,W,W,W]]),
                'agm_inner_tips_sensor':(1,[[-W,-30,-W,W,30,W],[-W,-W,-W,W,-40,W],[-W,40,-W,W,W,W]])}}
    plans_parts={
        'asfx':{3:['6aam_outer'],5:[],6:[],7:['6aam_outer']},
        'f14d':{3:['laam_wing_rear','gpb_front'],5:['laam_belly'],6:['gpb_front'],7:['laam_rear','gpb_front']},
        'f18f':{3:['lasm_inner'],5:[],6:[],7:['lasm_inner']},
        'su33':{3:['hvaa_outer','lasm_mid_belly'],5:[],6:[],7:['hvaa_outer','lasm_mid_belly']},
        'f18x':{3:['lacm_outer','agm_inner_tips_sensor'],5:[],6:[],7:['lacm_outer','agm_inner_tips_sensor']}}
    aircraft=[];cache={};pair_cache={}

    def envelopes(group,socket,name):
        key=(group['rack_mesh'],socket,name)
        if key not in cache:
            s=next(s for s in group['sockets'] if s['name']==socket)
            if s['serialized_rotation'] is None:
                # 省略旋转时不用假定零的OBB；只取真实位置，建立覆盖任意旋转的保守立方包络。
                positions,meta=geo.socket_samples(group,socket,True)
                points,_=geo.geometry(name);radius=float(np.max(np.linalg.norm(points,axis=1)))
                bodies=[(p,np.eye(3),np.full(3,radius)) for p,q in positions]
                info=dict(method='任意旋转保守包络；未推定省略的Socket旋转',radius=radius,
                          socket=socket,position_frame0=positions[0][0].tolist(),position_frame100=positions[-1][0].tolist())
            else:
                bodies,raw=geo.weapon_at(group,socket,name)
                info=dict(method='原生Socket完整位置/旋转逐资源帧OBB',socket=socket,
                          sweep=raw['full_101_frame_vertex_sweep_aabb'],geometry_sha256=raw['full_101_frame_vertex_sweep_sha256'])
            cache[key]=(bodies,info)
        return cache[key]

    def separation(first,second):
        ca,ra,ea=(np.array([x[i] for x in first]) for i in range(3))
        cb,rb,eb=(np.array([x[i] for x in second]) for i in range(3))
        aa=ra.transpose(0,2,1);bb=rb.transpose(0,2,1)
        axes=np.concatenate((aa,bb,np.cross(aa[:,:,None,:],bb[:,None,:,:]).reshape(-1,9,3)),axis=1)
        norms=np.linalg.norm(axes,axis=2);u=axes/np.where(norms>1e-9,norms,1)[:,:,None]
        sep=np.abs(np.einsum('fai,fi->fa',u,cb-ca))-np.einsum('faj,fj->fa',np.abs(np.einsum('fai,fij->faj',u,ra)),ea)-np.einsum('faj,fj->fa',np.abs(np.einsum('fai,fij->faj',u,rb)),eb)
        sep[norms<=1e-9]=-np.inf;gaps=sep.max(axis=1)
        return dict(frames=len(gaps),minimum_gap=float(gaps.min()),overlap_frames=np.flatnonzero(gaps<=1e-5).tolist(),
                    meaning='正值表示包络存在分离轴；重叠只要求继续面集/原生对照，不等于已证实碰撞')

    for code in targets:
        sample=native[code];groups=sample['groups'];labels=[];meshes=[]
        for g in groups:
            wid=g['weapon']['defaults']['WeaponID']['value'];labels.append(wid.split('WID_')[-1].upper())
            refs=[r['mesh'] for r in g['weapon_mesh_references'] if r['object']=='Static Mesh' and r['mesh'].startswith('w_')]
            meshes.append(refs[0] if refs else None)
        # 核对真实Pawn中组件、网格和动画类，不能仅凭旧报告文件名推定。
        pawn=geo.assets.load('AcePlayerPawn_'+code);bindings=[]
        for k,g in enumerate(groups):
            export=next(e for e in pawn['Exports'] if e['ObjectName']==g['group']+'_GEN_VARIABLE');props=properties(export)
            field='SkeletalMesh' if g['component_class']=='SkeletalMeshComponent' else 'StaticMesh'
            actual=resolve(pawn,props[field])
            if actual!=g['rack_mesh'] or resolve(pawn,export['ClassIndex'])!=g['component_class']:
                raise ValueError('组件/网格身份不同：'+code+'/'+g['group'])
            bindings.append(dict(group=g['group'],mesh=actual,component_class=g['component_class'],
                                 anim_class=resolve(pawn,props['AnimClass']) if field=='SkeletalMesh' else None,
                                 relative_overrides={key:val for key,val in props.items() if key.startswith('Relative')}))
        sk=groups[2]['rack_mesh'];driver=anim.graph(geo.assets,sk+'_AnimBP')
        motion=anim.skeleton_analysis(geo.assets,sk+'_Skeleton',driver['sequence'],channels)
        sk_frames,sk_indices,sk_info,sk_parsed=geo.skin(sk)
        # 活动机构按完整三角面的原生左右分块做包络，避免把两侧之间的空腔当实体。
        sk_points=np.array(sk_parsed['lods'][0]['points']);tri_y=sk_points[sk_indices][:,:,1]
        negative=np.all(tri_y<0,axis=1);positive=np.all(tri_y>0,axis=1)
        mechanism_envelopes=[]
        for label,keep in [('negative',negative),('positive',positive),('midline',~(negative|positive))]:
            if not np.any(keep):continue
            ids=np.unique(sk_indices[keep]);points=sk_frames[:,ids,:];lo=points.min(axis=1);hi=points.max(axis=1)
            mechanism_envelopes.append((label,[((a+b)/2,np.eye(3),(b-a)/2) for a,b in zip(lo,hi)]))
        body_export=next(e for e in pawn['Exports'] if e['ObjectName']=='PlaneBodyMesh')
        body_props=properties(body_export)
        body_mesh=resolve(pawn,body_props['SkeletalMesh'])
        body_anim=resolve(pawn,body_props['AnimClass']).removesuffix('_C')
        if body_mesh!=code:
            raise ValueError('Body网格与本批骨架名称不同，需重新建立引用：'+code)
        body_driver=anim.graph(geo.assets,body_anim)
        body=anim.skeleton_analysis(geo.assets,code+'_Skeleton',body_driver['sequence'],channels)
        comparison=anim.compare_tracks(geo.assets,dict(body,group='Body'),dict(motion,group='SWP3'))
        default=next(e for e in pawn['Exports'] if e['ObjectName']=='Default__AcePlayerPawn_'+code+'_C')
        parts_name=resolve(pawn,properties(default)['PartsAnimationParamDataTable'])
        parts=geo.assets.load(parts_name)
        part_rows=[value(x) for x in parts['Exports'][0]['Table']['Data']]
        if len(part_rows)!=1:raise ValueError('Parts参数行不是唯一：'+code)
        cover_options={key:val for key,val in part_rows[0].items() if 'Spw' in key or key=='bOptionsAnimWpCover'}
        partition_reports={}
        for label,(pool,boxes) in partitions[code].items():
            name=groups[pool]['rack_mesh'];geo.assets.load(name)
            entry=sorted(geo.assets.entries[name.casefold()],key=lambda x:'_0_P.' in x['pak'],reverse=True)[0]
            doc,ex,raw,parsed=load_mesh(Path(entry['json']))
            if all(all(any(all(box[k]<=point[k]<=box[k+3] for k in range(3)) for box in boxes)
                           for point in lod['points']) for lod in parsed['lods']):
                partition_reports[label]=dict(status='native_whole_already_in_requested_region',pool=pool,source=name,keep_boxes=boxes)
                continue
            try:
                candidate,details=split_geometry(raw,parsed,boxes)
                partition_reports[label]=dict(status='all_lods_partitionable_not_generated',pool=pool,source=name,keep_boxes=boxes,
                                             lods=details,source_extras_sha256=hashlib.sha256(raw).hexdigest())
            except ValueError as error:
                partition_reports[label]=dict(status='requires_review',pool=pool,source=name,keep_boxes=boxes,reason=str(error))
        combinations=[]
        for mask in range(1,8):
            selections=layouts[code].get(mask,[('all' if mask&(1<<k) else []) for k in range(3)])
            selected=[list(g['sockets'][i]['name'] for i in range(len(g['sockets']))) if selections[k]=='all' else selections[k]
                      for k,g in enumerate(groups)]
            assignments=[]
            for k,names in enumerate(selected):
                if len(set(names))!=len(names) or not set(names)<={s['name'] for s in groups[k]['sockets']}:
                    raise ValueError('重复或未知Socket：'+code)
                if not names:continue
                assignments.append(dict(weapon=labels[k],group=groups[k]['group'],sockets=names,
                    display_capacity=len(names),capacity_unit='管口' if labels[k]=='RKT' else '固定炮口' if labels[k]=='EML' else '弹体位置',
                    mesh=meshes[k],native_parent_chain_preserved=True,borrowed=False))
            checks=[]
            if mask not in (1,2,4):
                for ka,kb in itertools.combinations(range(3),2):
                    if not selected[ka] or not selected[kb] or not meshes[ka] or not meshes[kb]:continue
                    modes=[(meshes[ka],meshes[kb])]
                    for ma,mb in itertools.product([meshes[ka],meshes[ka].replace('_closed_SM','_SM')],
                                                   [meshes[kb],meshes[kb].replace('_closed_SM','_SM')]):
                        if (ma,mb) not in modes and geo.assets.has(ma) and geo.assets.has(mb):modes.append((ma,mb))
                    for sa,sb in itertools.product(selected[ka],selected[kb]):
                        for ma,mb in modes:
                            key=(code,ka,sa,ma,kb,sb,mb)
                            if key not in pair_cache:
                                a,ai=envelopes(groups[ka],sa,ma);b,bi=envelopes(groups[kb],sb,mb)
                                pair_cache[key]=dict(first=[groups[ka]['group'],sa,ma],second=[groups[kb]['group'],sb,mb],
                                                     first_method=ai['method'],second_method=bi['method'],**separation(a,b))
                            checks.append(pair_cache[key])
                if selected[2]:
                    for k in (0,1):
                        if not selected[k] or not meshes[k]:continue
                        modes=[meshes[k]]
                        expanded=meshes[k].replace('_closed_SM','_SM')
                        if expanded not in modes and geo.assets.has(expanded):modes.append(expanded)
                        for socket,name,(piece,body_envelope) in itertools.product(selected[k],modes,mechanism_envelopes):
                            key=(code,'mechanism',piece,k,socket,name)
                            if key not in pair_cache:
                                bodies,meta=envelopes(groups[k],socket,name)
                                pair_cache[key]=dict(first=['SWP3',piece,sk],second=[groups[k]['group'],socket,name],
                                    first_method='完整三角面左右分块的逐帧蒙皮包络',second_method=meta['method'],
                                    **separation(body_envelope,bodies))
                            checks.append(pair_cache[key])
            combinations.append(dict(mask=mask,weapons=[labels[k] for k in range(3) if mask&(1<<k)],
                assignments=assignments,static_partitions=plans_parts[code].get(mask,[]),geometry_checks=checks,
                geometry_overlap_cases=sum(bool(x['overlap_frames']) for x in checks),
                runtime_integration_allowed=False,status='native_single' if mask in (1,2,4) else 'first_static_design',
                mechanism_policy='当前组保留原生发射门槛和动画；非当前SWP3外观需逐组件动画输出资格及场景归还，不写全机公共进度'))
        aircraft.append(dict(code=code,weapons=labels,bindings=bindings,groups=deepcopy(groups),
            mechanism=dict(driver=driver,frames=motion['frames'],controlled_bones=motion['controlled_bones'],
                sockets=motion['sockets'],skeletal_geometry={k:sk_info[k] for k in ['lods','lod0_vertices','lod0_triangles','full_101_frame_skinned_vertex_sweep_aabb','full_101_frame_skinned_vertex_sha256']},
                body_mesh=body_mesh,body_driver=body_driver,parts_asset=parts_name,cover_options=cover_options,
                body_controlled_bones=body['controlled_bones'],body_rack_track_comparison=comparison),
            static_partition_reports=partition_reports,combinations=combinations,
            runtime_gate=dict(eligible=False,reason='第一版离线设计，尚未实现/核对五机活动挂架运行接线；几何筛查不能替代整机资格')))
        print(code,'完成七组合','包络重叠案例',sum(c['geometry_overlap_cases'] for c in combinations),flush=True)

    for name,actual in geo.assets.used.items():
        previous=source['sources'].get(Path(actual['asset']).stem)
        if previous and previous['sha256']!=actual['encrypted_sha256']:
            raise ValueError('当前原生来源与旧研究不同：'+name)
    result=dict(schema=1,baseline='main/0b75b21/v2.21.1; layout follows existing four-aircraft observation candidate',
        steam_build=9855922,status='first_static_design_not_runtime_integrated',compiled=False,tests_run=False,
        installed=False,game_process_access=False,assets_generated=False,aircraft=aircraft,sources=geo.assets.used,
        analyzer_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        tools_sha256={n:hashlib.sha256((root/'src/research'/n).read_bytes()).hexdigest() for n in
                      ['analyze_sp_switch_animations.py','review_animated_sukhoi.py','read_cooked_skeletal_mount.py','split_static_mount_asset.py']},
        limits=['101帧是资源序列采样，不是实机开舱耗时、原生实际控制顺序或所有通道混合姿态',
                '包络分离只覆盖所列弹体配对；未覆盖整机表面、所有支架干涉、真实离架/下落/点火和补装',
                'EML炮口沿原生固定Body骨，不从省略旋转字段猜新发射方向',
                'ASF-X的76管口不等于76座火箭巢；原生组件全池维护仍可能产生F-2A类似开销',
                'ASF-X含RKT多挂超过当前64位逐位置映射容量，需要独立发射装置角色或继续专题；不反改配弹/窗口',
                '本次不修改核心/Lua/运行白名单，不影响正在测试的四机候选'])
    if args.output.exists():raise ValueError('不覆盖已有研究回执')
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print('输出',args.output,'来源资源',len(result['sources']),flush=True)


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--project-root',type=Path,required=True)
    parser.add_argument('--manifest',type=Path,action='append',required=True)
    parser.add_argument('--output',type=Path,required=True)
    main(parser.parse_args())
