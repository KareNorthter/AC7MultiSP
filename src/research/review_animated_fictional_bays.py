"""ASF-X/X-02S完整原生架与真实兵装的离线研究；不连接游戏或生成资产。

--manifest可重复，清单由统一提取线程提供；--batch/--animation/--output均显式指定。
复用严格cooked解析、骨名父链/LBS及三角面粗筛；输出只保留摘要，不输出原始顶点。
"""
import argparse
import copy
import hashlib
import itertools
import json
from pathlib import Path

import numpy as np

from analyze_sp_switch_animations import Assets
from summarize_mount_assets import properties,resolve
from survey_aircraft_mounts import vector
from review_animated_fb22 import Animated,static,crossings,rotation,rotator,digest
from review_animated_dark_falken import triangle_bones


class Static:
    def __init__(self,assets,name):
        self.name=name;self.lods=static(assets,name);self.model=None
        self.sample_keys=['static']*101;self.sockets={}
        d=assets.load(name);e=next(e for e in d['Exports'] if e['ObjectName']==name)
        for ref in properties(e).get('Sockets',[]):
            p=properties(d['Exports'][ref-1]);n=p['SocketName']
            if not n.startswith('sp'):continue
            # 不为省略旋转填零；本批借用点缺明确旋转时直接拒绝数值计算。
            samples=None
            if 'RelativeLocation' in p and 'RelativeRotation' in p:
                samples=[(np.asarray(vector(p['RelativeLocation'])),rotation(rotator(p['RelativeRotation'])))]*101
            self.sockets[n]={'properties':p,'samples':samples}
    def points(self,lod,frame):return self.lods[lod]['points']
    def summary(self):
        return {'name':self.name,'class':'StaticMeshComponent','lods':[{'vertices':len(l['points']),'triangles':len(l['indices'])} for l in self.lods],
                'sockets':{n:{'properties':v['properties'],'explicit_rotation':v['samples'] is not None} for n,v in self.sockets.items()}}


class Frozen:
    def __init__(self,original,frame=0):
        self.original=original;self.name=original.name;self.model=original.model;self.lods=original.lods
        self.frame=frame;self.sample_keys=[original.sample_keys[frame]]*101
    def points(self,lod,frame):return self.original.points(lod,self.frame)


def index_array(surface,lod):return surface.lods[lod]['indices']


def audit_station(label,provider,socket,weapon,surfaces,baseline=None):
    samples=provider.sockets[socket]['samples']
    if samples is None:raise ValueError('Socket缺明确旋转，不能猜测：'+provider.name+'.'+socket)
    report={'label':label,'provider':provider.name+'.'+socket,'rows':[]};by_frame={}
    for sn,surface in surfaces.items():
        for li,lod in enumerate(surface.lods):
            for wi,wlod in enumerate(weapon):
                cache={};counts=[];faces=[];new_counts=[];unions=set();last_witness=[]
                for frame,(p,m) in enumerate(samples):
                    key=(surface.sample_keys[frame],digest([p.tolist(),m.tolist()]))
                    if key not in cache:
                        cache[key]=crossings((surface.points(li,frame)-p)@m,index_array(surface,li),wlod['points'],wlod['indices'])
                    pairs,witnesses=cache[key];face_set={a for a,b in pairs};unions.update(face_set)
                    native=baseline.get((sn,li,wi,frame),set()) if baseline else set()
                    counts.append(len(pairs));faces.append(sorted(face_set));new_counts.append(len(face_set-native))
                    by_frame[(sn,li,wi,frame)]=face_set
                    if frame in (0,25,50,75,100):
                        last_witness.extend({'frame':frame,**w,'surface_bones':triangle_bones(surface.model,surface.model['lods'][li],w['surface_face']) if surface.model else None} for w in witnesses[:2])
                report['rows'].append({'surface':sn,'surface_lod':li,'weapon_lod':wi,'frames':len(samples),
                                       'crossing_pairs_by_frame':counts,'surface_faces_by_frame':faces,
                                       'new_surface_face_count_by_frame':new_counts if baseline else None,
                                       'union_surface_faces':sorted(unions),'all_surface_faces_sha256':digest(faces),
                                       'full_open_face_count':len(faces[-1]),'selected_witnesses_without_vertices':last_witness})
    return report,by_frame


def surface_pair(label,first,second,frames=None):
    rows=[]
    fs=frames or [(f,f) for f in range(101)]
    for ai in range(len(first.lods)):
        for bi in range(len(second.lods)):
            cache={};counts=[];witness=[];common=[]
            for af,bf in fs:
                key=(first.sample_keys[af],second.sample_keys[bf])
                if key not in cache:
                    ap,bp=first.points(ai,af),second.points(bi,bf)
                    cache[key]=crossings(ap,index_array(first,ai),bp,index_array(second,bi))
                hits,ws=cache[key];counts.append(len(hits))
                if ws and (len(witness)<6 or af==bf==100):
                    witness.extend({'first_frame':af,'second_frame':bf,**w,
                        'first_bones':triangle_bones(first.model,first.model['lods'][ai],w['surface_face']) if first.model else None,
                        'second_bones':triangle_bones(second.model,second.model['lods'][bi],w['weapon_face']) if second.model else None} for w in ws[:1])
            rows.append({'first_lod':ai,'second_lod':bi,'sample_pairs':fs,'crossing_pair_counts':counts,'witnesses_without_vertices':witness})
    return {'label':label,'first':first.name,'second':second.name,'rows':rows,
            'boundary':'严格边穿面为正证据；共面重复面另查，不把无命中当全插值/体包含/运行碰撞证明。'}


def native_assignment(group,label,sockets=None):
    names=sockets or [s['name'] for s in group['sockets']]
    mesh=next((r['mesh'] for r in group['weapon_mesh_references'] if r['mesh'].startswith('w_')),None)
    fixed=label in ('EML','RKT')
    return {'weapon':label,'owner_group':group['group'],'provider_group':group['group'],'owner_class':group['weapon']['class'],
            'native_weapon_mesh':None if fixed else mesh,'native_projectile_mesh':mesh if label=='RKT' else None,
            'native_load_window':group['weapon']['defaults'].get('MaxLoadedCount',{}).get('value'),
            'native_load_window_source':group['weapon']['defaults'].get('MaxLoadedCount',{}).get('source'),
            'sockets':names,'position_role':'fixed_emitter' if label=='EML' else 'rocket_launcher' if label=='RKT' else 'ordnance_station_candidate',
            'resource_role':'fixed_emitter' if label=='EML' else 'rocket_launcher' if label=='RKT' else 'native_ordnance_mesh',
            'native_fixed_body_mesh':group['rack_mesh'] if label in ('EML','RKT') else None,'runtime_pool_count':None,'runtime_logical_count':None,
            'physical_count_unit':'rocket_muzzle_not_launcher_body_or_queue' if label=='RKT' else 'fixed_emitter' if label=='EML' else 'existing_socket',
            'physical_candidate_count':len(names),'fewer_physical_positions_than_window':group['weapon']['defaults'].get('MaxLoadedCount',{}).get('value') is not None and len(names)<group['weapon']['defaults']['MaxLoadedCount']['value'],
            'source_socket_array_order_preserved':names==[s['name'] for s in group['sockets']]}


def assignment(groups,labels,owner,provider,sockets):
    result=native_assignment(groups[owner-1],labels[owner-1],sockets)
    result['provider_group']='SWP'+str(provider)
    result['source_socket_array_order_preserved']=sockets==[s['name'] for s in groups[provider-1]['sockets']]
    result['display_release_same_provider']=True
    return result


def combination(weapons,assignments,rendered,groups,status='deferred',blockers=None):
    required=sorted({g for g in set(rendered)|{a['provider_group'] for a in assignments} if groups[int(g[-1])-1]['component_class']=='SkeletalMeshComponent'})
    return {'weapons':weapons,'assignments':assignments,'rendered_rack_groups':rendered,'required_animation_groups':required,
            'rack_plan':{'render_groups':rendered,'required_animation_groups':required,'hidden_self_only':[g['group'] for g in groups if g['group'] not in rendered],
                         'preserve_native_parent_chains':True,'new_resource_required':False,'new_station_required':False},
            'status':status,'disposition':'native_single_baseline' if len(weapons)==1 else 'candidate_existing_assets' if status=='native_reuse' else 'deferred',
            'blockers':blockers or [],'runtime_prerequisites':['owner真实类/库存/持久序号/窗口/齐射/冷却保持原生；provider只改变物理映射。',
                '显示与离架共同使用当帧骨架更新后的provider世界变换，任务/预览/补给各自校验serial身份并归还最新原生状态。',
                '未序列化缩放及原生池/逻辑长度未知；不能把Socket或管口数量写回运行队列。'],
            'internal_ammo_visibility':None,'runtime_verified':False,'integration_allowed':False}


def candidate_plans(sample):
    code=sample['code'];groups=sample['groups'];labels=['6AAM','RKT','LASM'] if code=='asfx' else ['4AAM','LASM','EML']
    cases=[combination([labels[i]],[native_assignment(g,labels[i])],[g['group']],groups,'native_reuse') for i,g in enumerate(groups)]
    if code=='asfx':
        rocket=[s['name'] for s in groups[1]['sockets']]
        selected=[([(1,3,['sp31','sp32']),(2,2,rocket)],['SWP2','SWP3']),
                  ([(1,3,['sp31']),(3,3,['sp32'])],['SWP3']),
                  ([(2,2,rocket),(3,3,['sp31','sp32'])],['SWP2','SWP3']),
                  ([(1,3,['sp31']),(2,2,rocket),(3,3,['sp32'])],['SWP2','SWP3'])]
    else:
        selected=[([(1,1,['sp12','sp15']),(2,2,['sp21'])],['SWP2']),
                  ([(1,1,[s['name'] for s in groups[0]['sockets']]),(3,3,['sp'])],['SWP1','SWP3']),
                  ([(2,2,['sp21','sp22']),(3,3,['sp'])],['SWP2','SWP3']),
                  ([(1,1,['sp12','sp15']),(2,2,['sp21']),(3,3,['sp'])],['SWP2','SWP3'])]
    for selection,render in selected:
        assignments=[assignment(groups,labels,*s) for s in selection]
        cases.append(combination([a['weapon'] for a in assignments],assignments,render,groups))
    return {'code':code,'name':sample['proposal'].get('name',code),'native_groups':groups,'combinations':cases,
            'complete_seven_design':False,'integration_gate':{'eligible':False},'runtime_verified':False}


def finalize(out):
    """由严格面见证下结论；不以旧域策略或零穿面充当通过条件。"""
    for a in out['aircraft']:
        code=a['code']
        if code=='x02s':
            a['previous_animation_design']=a.pop('animation_design')
            a['animation_design']={'mode':'common_progress_reviewed_and_rejected_by_geometry',
                'native_curves_retained':True,'body_sp_channels':[],
                'common_cover_resource_flag':True,'all_three_weapon_request_flags_proven':False,
                'rejection':'同步各原生曲线101键仍有活动舱盖/固定炮体和弹体/炮体严格穿面；不以旧distinct_mechanisms策略拒绝。'}
        for c in a['combinations']:
            c['required_animation_work']=[]
            c['launch_mapping']=[{'weapon':p['weapon'],'owner_group':p['owner_group'],'provider_group':p['provider_group'],
                                  'sockets':p['sockets'],'native_class':p['owner_class'],'same_provider_for_display_and_release':True}
                                 for p in c['assignments']]
            if len(c['weapons'])==1:
                c.update(integration_mode='native_passthrough',integration_allowed=True,blocker_kind=None,
                         priority='native_reference',native_single_kept_completely=True)
                continue
            c.update(status='deferred',disposition='deferred',integration_allowed=False,priority='deferred_whole_aircraft')
            if code=='asfx':
                if '6AAM' in c['weapons']:
                    c['blocker_kind']='station_overlap'
                    c['blockers']=[{'kind':'station_overlap','evidence':'asfx_6aam_on_lasm',
                         'detail':'真实6AAM弹体借SWP3.sp31/32，Body LOD0在全开仍穿9个固定Body骨面；同点原生LASM为0，不能以原生模板接触豁免。'},
                        {'kind':'animation_control','detail':'6AAM原Spw1cover=false，不请求LASM专用Body.swc3；仅要求SWP3骨骼更新不能产生真实开门/发射门控。'},
                        {'kind':'resource_split','evidence':'asfx_static_racks_rejection',
                         'detail':'原SWP1与RKT整架真实穿面且重复翼架；RKT架范围不能覆盖6AAM外列。恢复翼下原点需另行研究拆分资源，当前不拆。'}]
                    c['condition_to_reopen']='同时解决固定Body/支撑几何与真实6AAM对专用开门域的请求/发射门控，不能移动Socket、缩弹体、伪选LASM或仅全开显示。'
                else:
                    c['blocker_kind']='unknown_support'
                    c['blockers']=[{'kind':'unknown_support','detail':'完整RKT+LASM架同步101键各LOD零严格穿面，只是局部几何候选；未审查火箭实际离巢/展开与LASM释放扫掠、全部混合机体通道，且整机其他组合失败。'}]
                    c['condition_to_reopen']='可保留原生四火箭巢+原生LASM两点路线；整机七组合齐备前不单独启用。'
                c['animation_domains']=[{'body_channels':['swc3'],'rack_channels':{'SWP3':['spwc']},
                    'request_boundary':'真实LASM请求可用；6AAM自有原生类请求该专用域尚未解决，不替换owner类。'}]
            else:
                c['animation_domains']=[{'proposal':'共同原生spwc进度、保留各整架自身101键曲线',
                    'parts_common_cover_present':True,'body_sp_channels':[],
                    'geometry_accepted':False if 'EML' in c['weapons'] else None,
                    'class_request_verified':False,'eml_lifecycle':'蓄能/发射/收拢保持真实EML生命周期，不冻结或按库存隐藏炮体。'}]
                if 'EML' in c['weapons']:
                    c['blocker_kind']='station_overlap'
                    c['blockers']=[{'kind':'station_overlap','evidence':'x02s_racks_common_progress',
                        'detail':'同进度保留不同曲线，SWP1/3与SWP2/3全101键均严格穿面，全开仍1910/1633对；见证包含活动舱盖穿SWP3固定Body骨面，不只是共面重复壳。'},
                        {'kind':'station_overlap','evidence':'x02s_native_and_borrow_support',
                         'detail':'仅呈现完整EML架也失败：原4AAM六点全开仍穿63/65/16/53/53/4个架面（sp11–16顺序，弹体LOD0）；原LASM两点各139面。前中央点为SWPCover_09，其余有固定炮体Body骨见证，不能隐导弹架解除。'},
                        {'kind':'animation_control','detail':'Parts有公共Spwccover；各真实类的请求/发射门控及EML蓄能边界未独立证明。即使证明共同请求，固定结构冲突仍存在。'}]
                    c['condition_to_reopen']='需要另行证明无需拆分/新站的完整固定炮架与导弹共存布局；当前原生舱内点被炮体占用。不可冻结、隐藏炮体或只在全开显示掩盖。'
                else:
                    c['blocker_kind']='unknown_support'
                    c['blockers']=[{'kind':'unknown_support','detail':'只呈现SWP2、hidden SWP1保留sp12/sp15 provider时，sp15全开穿9个SWP2面，原SWP1为6面；尚不能证明SWP2导轨真实支撑4AAM两列。'},
                        {'kind':'resource_split','detail':'完整SWP1/2按共同进度叠放101键均穿面（3726–6028对），不能保留两套架掩盖支撑疑点。'}]
                    c['condition_to_reopen']='需证明单一完整原架对两类自身弹体的真实支撑和增量接触；目前不得把零Body穿面等同于架体适配。'
                c['body_baseline_boundary']='Body没有专用SP动画骨；本次保持非SP通道参考姿态。保留原Socket/原曲线的原生接触属于基线，未证明全机飞控混合姿态。'
            c['internal_ammo_visibility']={'mode':'not_a_clearance_workaround','allowed_to_resolve_fixed_body_or_support':False}
        a['integration_gate']={'eligible':False,'reason':'三原生单挂保持原样，但四多挂未全部解除真实结构/支撑/门控阻塞，整机七组合不准入。'}
        a['complete_seven_design']=False
        a['seven_cases_recorded']=len(a['combinations'])==7
        a['conclusions']=['整机暂缓；不为凑完整机型降低固定Body/支撑与真实门控要求。',
                          '旧distinct_mechanisms仅旧设计意图；X02S已实际评审共同进度，依据真实几何拒绝，不以旧域策略拒绝。'] if code=='x02s' else ['整机暂缓；腹舱借位6AAM新增固定Body穿面，翼下整架路线需拆分研究。']
    return out


def review(args):
    assets=Assets(args.manifest);batch={a['code']:a for a in json.loads(args.batch.read_text(encoding='utf-8'))['aircraft']}
    animations={a['code']:a for a in json.loads(args.animation.read_text(encoding='utf-8'))['aircraft']}
    out={'schema':'animated-fictional-bays-review-v1','aircraft':[candidate_plans(batch[c]) for c in ('asfx','x02s')],
         'compiled':False,'tests_run':False,'installed':False,'game_process_access':False,'assets_generated':False,'raw_vertices_emitted':False,
         'limits':['源序列101原始键不是运行FlightEngineBlend所有独立通道/插值状态。','无穿面不单独证明支撑；原生模板接触与新增固定Body穿面分开。']}
    surfaces={};weapons={}
    for code in ('asfx','x02s'):
        a=next(a for a in out['aircraft'] if a['code']==code)
        controlled={b['bone'] for b in animations[code]['body']['controlled_bones']}
        body=Animated(assets,code,controlled);racks={g['group']:(Animated(assets,g['rack_mesh']) if g['component_class']=='SkeletalMeshComponent' else Static(assets,g['rack_mesh'])) for g in batch[code]['groups']}
        surfaces[code]={'Body':body,**racks}
        a['meshes']={'Body':body.summary(),**{k:v.summary() for k,v in racks.items()}}
        a['animation_design']=animations[code]['animation_design'];a['body_sampled_sp_bones']=sorted(controlled)
        a['native_parts_options']={'source':animations[code]['parts_asset'],'flags':animations[code]['sp_options'],
                                  'boundary':'Parts资源存在相应通道，不能据此替代真实weapon类请求与原生发射门控的证明。'}
        for assignment_ in a['combinations'][:3]:
            for ass in assignment_['assignments']:
                mesh=ass['native_weapon_mesh'] or ass.get('native_projectile_mesh')
                if mesh:weapons[ass['weapon']+'_'+code]=static(assets,mesh)
    asfx=surfaces['asfx'];x=surfaces['x02s']
    # 先审查最有希望替代重复翼下架的腹舱路线，不反复探索已拒绝静态叠架。
    out['asfx_static_racks_rejection']=surface_pair('native_SWP1_and_RKT_overlap',asfx['SWP1'],asfx['SWP2'],[(0,0)])
    out['asfx_rkt_lasm_whole_racks']=surface_pair('RKT_whole_and_LASM_whole',asfx['SWP2'],asfx['SWP3'])
    out['asfx_native_lasm_support']=[];out['asfx_6aam_on_lasm']=[]
    for sock in ('sp31','sp32'):
        native,_=audit_station('native_LASM_'+sock,asfx['SWP3'],sock,weapons['LASM_asfx'],{'Body':asfx['Body'],'SWP3':asfx['SWP3']})
        candidate,_=audit_station('own_6AAM_on_LASM_'+sock,asfx['SWP3'],sock,weapons['6AAM_asfx'],{'Body':asfx['Body'],'SWP3':asfx['SWP3']})
        out['asfx_native_lasm_support'].append(native);out['asfx_6aam_on_lasm'].append(candidate)
        print('ASF-X腹舱真实弹体/Body/架完成',sock,flush=True)
    # X-02S独立机构先查端点组合；不能把同步101键当全部独立域的笛卡尔积。
    out['x02s_racks_endpoint_pairs']=[surface_pair('bay_vs_EML_'+g,x[g],x['SWP3'],list(itertools.product((0,100),repeat=2))) for g in ('SWP1','SWP2')]
    out['x02s_racks_common_progress']=[surface_pair('common_spwc_'+ga+'_'+gb,x[ga],x[gb]) for ga,gb in itertools.combinations(('SWP1','SWP2','SWP3'),2)]
    print('X02S共同进度完整三架101键完成',flush=True)
    out['x02s_native_and_borrow_support']=[]
    for group,sock,label,support in [('SWP1','sp12','4AAM','SWP1'),('SWP1','sp15','4AAM','SWP1'),('SWP2','sp21','LASM','SWP2'),('SWP2','sp22','LASM','SWP2')]:
        row,_=audit_station('native_'+label+'_'+sock,x[group],sock,weapons[label+'_x02s'],{'Body':x['Body'],support:x[support]})
        out['x02s_native_and_borrow_support'].append(row)
        if group=='SWP1':
            candidate,_=audit_station('4AAM_SWP1_provider_SWP2_render_'+sock,x[group],sock,weapons['4AAM_x02s'],{'SWP2':x['SWP2']})
            out['x02s_native_and_borrow_support'].append(candidate)
        # 尝试只呈现完整EML架时，自己弹体仍不能穿过炮体；不凭外壳重叠直接拒绝。
        emitter,_=audit_station(label+'_with_complete_EML_'+sock,x[group],sock,weapons[label+'_x02s'],{'SWP3':x['SWP3']})
        out['x02s_native_and_borrow_support'].append(emitter)
        print('X02S站位真实表面完成',group,sock,flush=True)
    # 对称不能替代实测；中央列也用真实原点逐一核对完整炮架。
    for sock in ('sp11','sp13','sp14','sp16'):
        emitter,_=audit_station('4AAM_with_complete_EML_'+sock,x['SWP1'],sock,weapons['4AAM_x02s'],{'SWP3':x['SWP3']})
        out['x02s_native_and_borrow_support'].append(emitter)
    out['asset_sources']=assets.used
    out['inputs']=[{'name':p.name,'sha256':hashlib.sha256(p.read_bytes()).hexdigest()} for p in args.manifest+[args.batch,args.animation]]
    out['analyzer_sha256']=hashlib.sha256(Path(__file__).read_bytes()).hexdigest()
    return finalize(out)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--manifest',type=Path,action='append',required=True)
    parser.add_argument('--batch',type=Path,required=True);parser.add_argument('--animation',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();result=review(args)
    args.output.write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print('离线报告',args.output,flush=True)


if __name__=='__main__':main()
