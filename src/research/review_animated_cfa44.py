"""CFA-44整机七组合的原架与动画域离线研究；不连接游戏或生成游戏资源。"""
import argparse
import hashlib
import itertools
import json
from pathlib import Path

import numpy as np

from analyze_sp_switch_animations import Assets, animation_tracks, poses
from read_cooked_skeletal_mount import read_skeletal_mesh, reference_component
from review_animated_dark_falken import edge_crossings, triangle_bones
from review_animated_sukhoi import quat_matrix
from summarize_mount_assets import properties, value
from survey_aircraft_mounts import reference_bones


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def load_model(assets, name):
    model=read_skeletal_mesh(assets.load(name),name)
    sd=assets.load(name+'_Skeleton')
    ex=next(e for e in sd['Exports'] if e['ObjectName']==name+'_Skeleton')
    bones=reference_bones(sd,ex)
    tracks,metadata=animation_tracks(assets.load(name+'_Anim'),bones)
    if set(tracks)!=set(range(len(bones))):
        raise ValueError('CFA44存在未解释的缺轨，停止推算')
    samples=poses(bones,tracks,metadata['frames'])
    lookup={b['name']:i for i,b in enumerate(bones)}
    reference=reference_component(model['bones'])
    mapped=[]
    for b in model['bones']:
        i=lookup[b['name']]
        parent=model['bones'][b['parent']]['name'] if b['parent']>=0 else None
        expected=bones[bones[i]['parent']]['name'] if bones[i]['parent']>=0 else None
        if parent!=expected:raise ValueError('mesh/Skeleton骨父链不一致')
        mapped.append(i)
    frames=[]
    for lod in model['lods']:
        points=np.asarray(lod['points'])
        influences={}
        for section in lod['sections']:
            for vi in range(section['start'],section['start']+section['vertices']):
                for local,w in lod['weights'][vi]:
                    if w:influences.setdefault(section['bone_map'][local],[]).append((vi,w/255))
        sequence=[]
        for pose in samples:
            out=np.zeros_like(points)
            for bi,entries in influences.items():
                ids=np.array([x[0] for x in entries]);weights=np.array([x[1] for x in entries])
                rp,rq=reference[bi];cp,cq=pose[mapped[bi]]
                rotation=quat_matrix(cq)@quat_matrix(rq).T
                out[ids]+=(points[ids]@rotation.T+np.asarray(cp)-rotation@np.asarray(rp))*weights[:,None]
            sequence.append(out)
        frames.append(np.stack(sequence))
    return model,frames,metadata


def crossings(first,second):
    if np.any(first.max(axis=(0,1))<second.min(axis=(0,1))-1e-7) or np.any(second.max(axis=(0,1))<first.min(axis=(0,1))-1e-7):
        return 0,None
    low,high=second.min(axis=1),second.max(axis=1)
    count=0;witness=None
    for start in range(0,len(first),128):
        part=first[start:start+128]
        ai,bi=np.nonzero(np.all(part.min(axis=1)[:,None,:]<=high[None,:,:]+1e-7,axis=2)&
                         np.all(part.max(axis=1)[:,None,:]>=low[None,:,:]-1e-7,axis=2))
        a,b=part[ai],second[bi]
        forward,fw=edge_crossings(a,b);reverse,rw=edge_crossings(b,a)
        hit=forward|reverse;count+=int(hit.sum())
        if witness is None and hit.any():
            p=int(np.flatnonzero(hit)[0])
            witness={'first_triangle':int(start+ai[p]),'second_triangle':int(bi[p]),
                     'forward_edge_crosses':bool(forward[p]),'reverse_edge_crosses':bool(reverse[p])}
    return count,witness


def branch_subset(model, root):
    """只按完整原生骨支路分组，所有非退化三角面须完整落在一支，不切跨支路面。"""
    names={b['name']:i for i,b in enumerate(model['bones'])}
    descendants={names[root]}
    for i,b in enumerate(model['bones']):
        if b['parent'] in descendants:descendants.add(i)
    lod=model['lods'][0];vertex_bones={}
    for section in lod['sections']:
        for i in range(section['start'],section['start']+section['vertices']):
            vertex_bones[i]={section['bone_map'][b] for b,w in lod['weights'][i] if w}
    kept=[];mixed=[]
    for i,triangle in enumerate(np.asarray(lod['indices']).reshape(-1,3)):
        influenced=set().union(*(vertex_bones[int(v)] for v in triangle))
        if influenced<=descendants:kept.append(i)
        elif influenced&descendants:mixed.append(i)
    if mixed:raise ValueError('发现跨可见/隐藏支路权重的三角面，不能只按骨隐藏：'+root)
    return np.asarray(kept),{'root':root,'descendants':[model['bones'][i]['name'] for i in sorted(descendants)],
                            'kept_triangles':len(kept),'cross_branch_triangles':len(mixed)}


def asymmetric_review(assets,sample,models,original_path,bone_api_index):
    """按用户指定左ADMM／右EML／内部IEWS，只研究原生具名骨显隐，不生成副本。"""
    left_model,left_frames,_=models['fa44_SWP1'];right_model,right_frames,_=models['fa44_SWP2']
    left_ids,left=branch_subset(left_model,'SWPCover_01')
    right_ids,right=branch_subset(right_model,'SWPCover_10')
    li=np.asarray(left_model['lods'][0]['indices']).reshape(-1,3)[left_ids]
    ri=np.asarray(right_model['lods'][0]['indices']).reshape(-1,3)[right_ids]
    rows=[]
    for lk,rk in sorted({(i,i) for i in range(101)}|set(itertools.product([0,25,50,75,100],repeat=2))):
        left_triangles,right_triangles=left_frames[0][lk][li],right_frames[0][rk][ri]
        count,witness=crossings(left_triangles,right_triangles)
        gap=float(np.maximum(left_triangles.min(axis=(0,1))-right_triangles.max(axis=(0,1)),
                             right_triangles.min(axis=(0,1))-left_triangles.max(axis=(0,1))).max())
        rows.append({'left_key':lk,'right_key':rk,'strict_crossing_pairs':count,
                     'axis_aligned_separation_resource_units':gap,'witness':witness})
    body,body_frames,_=models['fa44']
    extra_cover=[]
    for lod_index,lod in enumerate(body['lods']):
        influenced=set()
        for section in lod['sections']:
            for vi in range(section['start'],section['start']+section['vertices']):
                if any(w and body['bones'][section['bone_map'][b]]['name'] in {'SWPCover1_01','SWPCover1_02'} for b,w in lod['weights'][vi]):
                    influenced.add(vi)
        ids=[i for i,triangle in enumerate(np.asarray(lod['indices']).reshape(-1,3)) if set(map(int,triangle))&influenced]
        samples=[]
        if ids:
            bi=np.asarray(lod['indices']).reshape(-1,3)[ids]
            for body_key,weapon_key in sorted({(i,i) for i in range(101)}|set(itertools.product([0,25,50,75,100],repeat=2))):
                target=body_frames[lod_index][body_key][bi]
                lc,lw=crossings(left_frames[0][weapon_key][li],target)
                rc,rw=crossings(right_frames[0][weapon_key][ri],target)
                samples.append({'body_swc1_key':body_key,'weapons_spwc_key':weapon_key,
                                'left_crossings':lc,'right_crossings':rc,'witness':lw or rw})
        extra_cover.append({'body_lod':lod_index,'affected_vertices':len(influenced),
                            'affected_triangles':len(ids),'samples':samples,
                            'all_reviewed_pairs_clear':not any(r['left_crossings'] or r['right_crossings'] for r in samples)})
    groups={g['group']:g for g in sample['groups']}
    admm=[s['name'] for s in groups['SWP1']['sockets'] if s.get('bone')=='SWPCover_01']
    # 资源按sp001,sp004...sp124轮列，不能取字典顺序冒充逻辑队列顺序。
    admm=sorted(admm,key=lambda s:int(s[2:]))
    if len(admm)!=42 or admm!=[f'sp{i:03}' for i in range(1,125,3)]:raise ValueError('左ADMM42原生管口身份不符')
    names=['ADMM','EML','IEWS'];combos=[]
    for count in (1,2,3):
        for selected in itertools.combinations(range(3),count):
            singles=count==1;assignments=[]
            for i in selected:
                group=sample['groups'][i];sockets=[s['name'] for s in group['sockets']]
                if not singles:sockets=[admm,['sp22'],['sp31']][i]
                assignments.append({'weapon':names[i],'owner_group':group['group'],'provider_group':group['group'],
                    'owner_class':group['config']['WeaponClass'],'sockets':sockets,'native_weapon_mesh':None,
                    'native_fixed_body_mesh':group['rack_mesh'],'resource_role':['missile_array','fixed_emitter','effect_source'][i],
                    'native_load_window':group['weapon']['defaults'].get('MaxLoadedCount',{}).get('value'),
                    'native_load_window_source':group['weapon']['defaults'].get('MaxLoadedCount'),
                    'runtime_logical_count':None,'runtime_pool_count':None})
            rendered=['SWP'+str(i+1) for i in selected if singles or i!=2]
            combos.append({'weapons':[names[i] for i in selected],'assignments':assignments,
                'integration_mode':'native_passthrough' if singles else 'asymmetric_native_bone_visibility',
                'rendered_rack_groups':rendered,'required_animation_groups':['SWP'+str(i+1) for i in selected],
                'native_bone_visibility':{} if singles else {
                    'SWP1':{'show_roots':['SWPCover_01'],'hide_roots':['SWPCover_04','SWPCover_07']} if 0 in selected else None,
                    'SWP2':{'show_roots':['SWPCover_10'],'hide_roots':['SWPCover_01']} if 1 in selected else None,
                    'SWP3':{'hide_component_self_only':True,'preserve_original_effect_source':'sp31'} if 2 in selected else None},
                'runtime_prerequisites':['原生HideBoneByName/UnHideBoneByName/IsBoneHiddenByName通过反射核对完整签名与PBO_None枚举，保存并归还最新原生骨可见请求',
                    '只操作未使用完整支路，可见Socket不在隐藏根的后代；完整原生曲线与当帧可见支路继续求值',
                    '公共spwc连续性与Body原生额外swc1须保持；不能把bodyOnlyPublic资格强填true，CFA44新profile仍须专门接入',
                    'ADMM保留真实原生队列与持久mountOrdinal，轮映射42个左管口；EML映射右侧原sp22，不按资源点数填写运行池',
                    'IEWS只隐藏外观组件本身，保留真实plwp_ecmp_x1效果源、原生生命周期、朝向与清理，不假造TLS/ECM互斥'],
                'required_animation_work':['保留native Parts完整更新一次、隐藏骨支路租约、可见支路当帧姿态和Socket取位；退出/失败逐项归还'],
                'status':'native_single' if singles else 'candidate_pending_common_spwc_and_body_review',
                'gameplay_tested':False,'runtime_enabled':False})
    # 可见部件仍为原生原位，Body接触基线与单挂相同；不同机构独立进度须以附加25点核对为边界。
    original=json.loads(original_path.read_text(encoding='utf-8'))
    index=json.loads(bone_api_index.read_text(encoding='utf-8'))
    objects={o['address']:o for o in index}
    api=[]
    for name in ['HideBoneByName','UnHideBoneByName','IsBoneHiddenByName']:
        matches=[o for o in index if o['name']==name and objects.get(o['outer'],{}).get('name')=='SkinnedMeshComponent']
        if len(matches)!=1:raise ValueError('旧AC7反射索引没有唯一具名骨可视接口：'+name)
        o=matches[0]
        if objects.get(o['cls'],{}).get('name')!='Function':raise ValueError('目标不是原生UFunction身份')
        api.append({'function':'/Script/Engine.SkinnedMeshComponent.'+name,'identity_present_in_captured_index':True,
                    'parameters_and_enum_not_verified_here':True})
    options=sample['aircraft_context']['parts_serialized_rows'][0]
    flags={k:options[k] for k in ['bPartsExistsPartsSpwccoverExist','bPartsExistsPartsSpw1coverExist','bPartsExistsPartsSpw2coverExist','bPartsExistsPartsSpw3coverExist','bOptionsAnimWpCover']}
    if list(flags.values())!=[True,False,False,False,False]:raise ValueError('公共cover原生配置与设计不符')
    complete=not any(r['strict_crossing_pairs'] for r in rows) and all(x['all_reviewed_pairs_clear'] for x in extra_cover)
    return {'schema':1,'aircraft':[{'code':'fa44','name':'CFA-44 Nosferatu','combinations':combos,
        'left_branch':left,'right_branch':right,'visible_branch_pairs':rows,
        'native_extra_body_swc1_review':extra_cover,
        'all_reviewed_visible_pairs_clear':not any(r['strict_crossing_pairs'] for r in rows),
        'native_bone_api':{'captured_index_sha256':digest(bone_api_index),'functions':api,
                          'interface_source':'https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/USkinnedMeshComponent/HideBoneByName',
                          'boundary':'Epic页面解释接口行为，不作AC7特定签名证明；旧AC7索引确认具名身份，参数与枚举必须由实际反射Host核对'},
        'parts_common_flags':flags,'body_extra_swc1_policy':'保留原生输出，不复制公共spwc到swc1；可见左右机构对该组舱盖的独立采样均无穿面',
        'original_full_racks':{'analysis_sha256':digest(original_path),'summary':[
            {'first':p['first'],'second':p['second'],'maximum':max(x['strict_crossing_pairs'] for x in p['samples'])}
            for p in original['aircraft'][0]['rack_pairs']]},
        'layout_candidate_complete_seven':complete,'runtime_ready':False,'runtime_enabled':False,
        'decision':'完整七组合布局候选；原生骨显隐租约与CFA44公共cover专用profile须后续接入，不自动放行运行'}],
        'sources':assets.used,'analyzer_sha256':digest(Path(__file__)),
        'reader_sha256':digest(Path(__file__).with_name('read_cooked_skeletal_mount.py')),
        'compiled':False,'tests_run':False,'installed':False,'game_process_access':False,'assets_generated':False}


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--manifest',type=Path,action='append',required=True)
    parser.add_argument('--batch',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--asymmetric-original-review',type=Path,help='用户左右分工追加研究，输入已完成的整架报告')
    parser.add_argument('--bone-api-index',type=Path,help='旧AC7真实反射索引，不访问当前进程')
    args=parser.parse_args()
    if args.output.exists():raise ValueError('请使用新输出路径')
    assets=Assets(args.manifest)
    sample=next(a for a in json.loads(args.batch.read_text(encoding='utf-8'))['aircraft'] if a['code']=='fa44')
    models={name:load_model(assets,name) for name in ['fa44','fa44_SWP1','fa44_SWP2','fa44_SWP3']}
    if args.asymmetric_original_review:
        if not args.bone_api_index:raise ValueError('左右原生骨方案需要旧AC7反射接口身份依据')
        result=asymmetric_review(assets,sample,models,args.asymmetric_original_review,args.bone_api_index)
        args.output.write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
        print(json.dumps({'mode':'asymmetric','visible_pairs_clear':result['aircraft'][0]['all_reviewed_visible_pairs_clear']},ensure_ascii=False))
        return
    geometry={}
    for name,(model,frames,metadata) in models.items():
        geometry[name]={'complete_payload':model['complete'],'bone_count':len(model['bones']),
                        'animation':metadata,'lods':[{'lod':i,'vertices':len(lod['points']),
                        'triangles':len(lod['indices'])//3,'sweep_bounds':np.column_stack((seq.min(axis=(0,1)),seq.max(axis=(0,1)))).tolist(),
                        'sampled_skinned_points_sha256':hashlib.sha256(seq.astype('<f8').tobytes()).hexdigest()}
                        for i,(lod,seq) in enumerate(zip(model['lods'],frames))]}
    pairs=[]
    for left,right in itertools.combinations(['fa44_SWP1','fa44_SWP2','fa44_SWP3'],2):
        lm,lf,_=models[left];rm,rf,_=models[right]
        li=np.asarray(lm['lods'][0]['indices']).reshape(-1,3);ri=np.asarray(rm['lods'][0]['indices']).reshape(-1,3)
        rows=[]
        # 全101键同进度加九组独立开合端点；发现穿面只认正证据，不以稀疏采样宣称全净空。
        coordinates=sorted({(i,i) for i in range(101)}|set(itertools.product([0,50,100],repeat=2)))
        for lkey,rkey in coordinates:
            count,witness=crossings(lf[0][lkey][li],rf[0][rkey][ri])
            if witness:
                witness['first_bones']=triangle_bones(lm,lm['lods'][0],witness['first_triangle'])
                witness['second_bones']=triangle_bones(rm,rm['lods'][0],witness['second_triangle'])
            rows.append({'first_key':lkey,'second_key':rkey,'strict_crossing_pairs':count,'witness':witness})
        pairs.append({'first':left,'second':right,'samples':rows,'has_strict_crossing':any(r['strict_crossing_pairs'] for r in rows)})
        print(json.dumps({'pair':[left,right],'max_crossings':max(r['strict_crossing_pairs'] for r in rows)},ensure_ascii=False),flush=True)
    channels={}
    for raw in assets.load('PlaneBodyConfig')['Exports'][0]['Table']['Data']:
        row=value(raw)
        if 'BoneName' in row and 'FlightEngineOuputName' in row:channels[row['BoneName']]=row['FlightEngineOuputName']
    model=models['fa44'][0];body_channels=[]
    for i,lod in enumerate(model['lods']):
        counts={}
        for section in lod['sections']:
            for vi in range(section['start'],section['start']+section['vertices']):
                used={channels.get(model['bones'][section['bone_map'][b]]['name']) for b,w in lod['weights'][vi] if w}
                for ch in used&{'spwc','swc1','swc2','swc3','swcc'}:counts[ch]=counts.get(ch,0)+1
        body_channels.append({'lod':i,'weighted_vertices_per_sp_channel':counts})
    names=['ADMM','EML','IEWS'];combos=[]
    for n in (1,2,3):
        for indices in itertools.combinations(range(3),n):
            selected=[names[i] for i in indices]
            combo={'weapons':selected,'status':'native_single' if n==1 else 'deferred',
                   'integration_mode':'native_passthrough' if n==1 else 'whole_native_mechanisms_under_review',
                   'assignments':[{'weapon':names[i],'owner_group':'SWP'+str(i+1),'provider_group':'SWP'+str(i+1),
                    'owner_class':sample['groups'][i]['config']['WeaponClass'],'sockets':[s['name'] for s in sample['groups'][i]['sockets']],
                    'native_weapon_mesh':None,'native_fixed_body_mesh':sample['groups'][i]['rack_mesh'],
                    'resource_role':['missile_array','fixed_emitter','effect_source'][i],
                    'runtime_logical_count':None,'runtime_pool_count':None} for i in indices],
                   'rendered_rack_groups':['SWP'+str(i+1) for i in indices],
                   'required_animation_groups':['SWP'+str(i+1) for i in indices],
                   'integration_allowed':False,'gameplay_tested':False}
            if n>1:combo['deferred_reason']='三类完整机构的公共Body输出与独立状态尚未完成原生路由/生命周期证据；ADMM126处资源点不能作为三类运行池数量。严格穿面记录另列。'
            combos.append(combo)
    result={'schema':1,'aircraft':[{'code':'fa44','name':'CFA-44 Nosferatu','combinations':combos,
             'complete_seven_layout_candidates':False,'runtime_ready':False,'geometry':geometry,'rack_pairs':pairs,
             'body_channels':body_channels,'decision':'最后单独研究，未解决的整机继续暂缓，不交付部分组合'}],
             'method':'原生组件空间，mesh inverse bind与骨名/父链映射，完整骨架LOD、101键；严格边穿面正证据不等于完整碰撞检测',
             'compiled':False,'tests_run':False,'installed':False,'game_process_access':False,'assets_generated':False,
             'sources':assets.used,'analyzer_sha256':digest(Path(__file__)),
             'reader_sha256':digest(Path(__file__).with_name('read_cooked_skeletal_mount.py'))}
    args.output.write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')


if __name__=='__main__':main()
