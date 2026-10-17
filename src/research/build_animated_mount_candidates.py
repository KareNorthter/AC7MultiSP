"""将人工评审通过的整机七组合整理成动画批次；不启用产品运行机型。"""
import argparse
from copy import deepcopy
import hashlib
import itertools
import json
from pathlib import Path


def fingerprint(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def compact(value):
    """大采样保留摘要与关键帧，完整报告由同源分析器重现，不提交原始顶点。"""
    if isinstance(value,dict):
        return {key:compact(item) for key,item in value.items()}
    if not isinstance(value,list):
        return value
    if len(value)<=16:
        return [compact(item) for item in value]
    result={'sample_count':len(value),'all_samples_sha256':hashlib.sha256(json.dumps(value,ensure_ascii=False,separators=(',',':')).encode()).hexdigest()}
    if all(isinstance(x,(int,float)) and not isinstance(x,bool) for x in value):
        result.update(minimum=min(value),maximum=max(value),nonzero_indices=[i for i,x in enumerate(value) if x])
    result['selected_samples']={str(i):compact(value[i]) for i in sorted({0,len(value)//4,len(value)//2,len(value)*3//4,len(value)-1})}
    return result


def normalize(sample,review):
    original={tuple(sorted(c['weapons'])):c for c in sample['proposal']['combinations']}
    groups={g['group']:g for g in sample['groups']}
    labels={c['weapons'][0]:c['assignments'][0]['group'] for c in original.values() if len(c['weapons'])==1}
    expected={tuple(sorted(parts)) for size in (1,2,3) for parts in itertools.combinations(labels,size)}
    combos=review['combinations']
    if len(combos)!=7 or {tuple(sorted(c['weapons'])) for c in combos}!=expected:
        raise ValueError('整机七组合不完整：'+sample['code'])
    result=[]
    for item in combos:
        # 多挂是覆盖后的新方案，不能夹带旧批次conditional_plan/reason/priority。
        base=deepcopy(original[tuple(sorted(item['weapons']))]) if len(item['weapons'])==1 else {'weapons':item['weapons']}
        base.update(status='native_reuse',disposition='native_single' if len(item['weapons'])==1 else 'candidate_existing_assets',
                    deferred_reason=None,blocker_kind=None,blockers=[],integration_allowed=True,
                    priority='native_reference' if len(item['weapons'])==1 else 'P2',
                    gameplay_tested=False,dynamic_clearance_verified=False,compiled=False,tests_run=False,installed=False)
        if len(item['weapons'])==1:
            base.update(integration_mode='native_passthrough',required_animation_work=[],runtime_prerequisites=[])
            result.append(base)
            continue
        assigned=[]
        seen=set()
        for source in item['assignments']:
            weapon=source['weapon']
            owner=source.get('owner_group',source.get('group'))
            provider=source.get('provider_group',owner)
            if owner!=labels[weapon] or provider not in groups:
                raise ValueError('逻辑兵装/原生provider身份不符')
            cls=groups[owner]['config']['WeaponClass']
            if source.get('owner_class',source.get('weapon_class',cls))!=cls:
                raise ValueError('不得替换逻辑武器类')
            sockets=source['sockets']
            available={s['name'].casefold() for s in groups[provider]['sockets']}
            if not sockets or len(set(s.casefold() for s in sockets))!=len(sockets) or any(s.casefold() not in available for s in sockets):
                raise ValueError('借位须使用provider真实Socket且不得重复')
            mesh=source.get('native_weapon_mesh',source.get('visible_mesh',source.get('replacement_mesh')))
            native_meshes={r['mesh'] for r in groups[owner]['weapon_mesh_references'] if r.get('mesh') and r['mesh']!='Static Mesh'}
            role=source.get('resource_role','native_ordnance_mesh')
            fixed=role in {'fixed_emitter','fixed_emitter_no_ordnance_mesh','rocket_launcher','missile_array','effect_source'}
            emitter=source.get('native_fixed_body_mesh',source.get('native_emitter_mesh'))
            if fixed:
                if mesh is not None or owner!=provider or emitter!=groups[owner]['rack_mesh']:
                    raise ValueError('固定发射装置须保留自身完整原生组件，不能伪造弹体或借位')
            elif mesh not in native_meshes:
                raise ValueError('不得用另一种武器的网格缩小体积：'+str(mesh))
            for socket in sockets:
                key=(provider,socket.casefold())
                if key in seen:
                    raise ValueError('两类武器不能同时占用同一provider组件')
                seen.add(key)
            window=source.get('native_load_window',source.get('loaded_window'))
            if isinstance(window,dict):
                window=window.get('value')
            native=next(a for a in original[tuple(sorted([weapon]))]['assignments'] if a['weapon']==weapon)
            assignment={key:deepcopy(native[key]) for key in ('weapon','native_weapon_id','position_role','physical_count_unit',
                        'loaded_window_from_blueprint','runtime_logical_count','runtime_pool_count','native_reference_rotation_boundary') if key in native}
            assignment.update(group=owner,owner_group=owner,weapon_class=cls,provider_group=provider,sockets=sockets,
                              provider_rack_mesh=groups[provider]['rack_mesh'],weapon_mesh=mesh,replacement_mesh=mesh,
                              weapon_mesh_replacement=provider!=owner,physical_candidate_count=len(sockets),loaded_window=window,
                              below_window=window is not None and len(sockets)<window,
                              fewer_physical_positions_than_window=window is not None and len(sockets)<window,
                              logical_count_policy='保留原生实际队列数量、持久序号和装填窗口；仅物理位置按有序列表轮转',
                              display_policy='每个物理位置最多显示一个弹体；受实际已装填数和库存限制，不从站位数推断原生队列或模型池',
                              display_release_same_provider=True,native_transform_only=True,
                              mapping_boundary='provider为现有原生组件；显示与真实离架共同读取动画后的实时世界变换，不新建Socket或改相对位置')
            if fixed:
                assignment.update(position_role=role,resource_role=role,native_fixed_body_mesh=emitter,
                                  display_policy='完整原生炮体/巢体/效果装置不随装填数或库存隐藏；原生生命周期控制其动作与发射',
                                  logical_count_policy='保留真实原生队列及窗口；资源管口/射点数不作为逻辑队列或持久弹体池数量',
                                  weapon_mesh_replacement=False)
            # 原owner的细节不能冒充provider坐标；保留完整的真实provider资源记录。
            assignment['socket_details']=[deepcopy(next(s for s in groups[provider]['sockets'] if s['name'].casefold()==name.casefold())) for name in sockets]
            assigned.append(assignment)
        rack=item.get('rack_plan',{})
        rendered=item.get('rendered_rack_groups') or rack.get('render_groups') or rack.get('visible_whole_native_groups')
        if not rendered or any(g not in groups for g in rendered):
            raise ValueError('必须明确完整原生呈现架')
        needed=set(rendered)|{a['provider_group'] for a in assigned}
        animated=sorted(g for g in needed if groups[g]['component_class']=='SkeletalMeshComponent')
        if item.get('required_animation_groups',rack.get('required_animation_groups',animated))!=animated:
            raise ValueError('所需动画组与真实provider/呈现架不一致')
        visibility=deepcopy(item.get('internal_ammo_visibility'))
        if sample['code']=='su47':
            # 不把离线第87键等同于Parts进度0.87；仅采用已核对的全开端点。
            visibility={'mode':'native_fully_open_and_current_end_pose_only',
                        'applies_to':'所有本方案管理的舱内ammo；三种单挂不接管',
                        'native_state':'request=2, phase=2, progress[0]=1；须经Steam9855922指纹与实例身份校验',
                        'pose_witness':'所需provider当帧骨骼和Socket均更新，组件空间变换与本序列第100键原生全开端点匹配',
                        'hide_immediately_on':'关闭请求、非全开、旧帧、身份/姿态校验失败或任何证明缺失',
                        'affects_only_ammo_visibility':True,'changes_native_fire_gate':False,
                        'continuous_clearance_claimed':False,'offline_sample_review':visibility}
        base.update(assignments=assigned,rack_plan={'rendered_rack_groups':rendered,'required_animation_groups':animated,
                    'hidden_self_only':[g for g in groups if g not in rendered],
                    'preserve_native_parent_chains':True,'new_resource_required':False,'new_station_required':False},
                    rendered_rack_groups=rendered,required_animation_groups=animated,integration_mode='animated_provider_plan',
                    suggested_priority='P2',internal_ammo_visibility=visibility,
                    runtime_prerequisites=item.get('runtime_prerequisites',[]),
                    required_animation_work=item.get('required_animation_work',[]),
                    geometry_review='evidence/mount-expansion-animated.json#'+sample['code'],
                    notes=['各呈现架/固定装置保留完整原生资源，重复区域不能叠架；保留真实弹体，逻辑owner与实际provider分开',
                           '必须实现所列动画/显隐/原生骨骼刷新与归还后，按整机七组合接入；此表不自动打开运行白名单'])
        for key in ('native_bone_visibility','parts_domains','tls_switch_policy','mechanism_policy'):
            if key in item:base[key]=deepcopy(item[key])
        result.append(base)
    return result


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--batch',type=Path,required=True)
    p.add_argument('--review',type=Path,action='append',required=True)
    p.add_argument('--geometry-audit',type=Path,required=True)
    p.add_argument('--control-source',type=Path,action='append',required=True)
    p.add_argument('--analysis-source',type=Path,action='append',default=[],help='本轮分析源码指纹；不修改此前轮次的历史依据')
    p.add_argument('--previous',type=Path,help='合并既有动画覆盖层，保留前轮候选和证据版本')
    p.add_argument('--round-title',default='first_animated_review')
    p.add_argument('--admit',action='append',default=[],help='人工评审通过的整机候选，不是产品启用指令；允许本轮全部暂缓')
    p.add_argument('--output',type=Path,required=True)
    args=p.parse_args()
    if args.output.exists():
        raise ValueError('输出已存在，请用新路径')
    batch=json.loads(args.batch.read_text(encoding='utf-8'))
    samples={a['code']:a for a in batch['aircraft']}
    reviews={}
    evidence=[]
    for path in args.review:
        data=json.loads(path.read_text(encoding='utf-8'))
        evidence.append({'analysis':path.name,'sha256':fingerprint(path),'report':compact(data)})
        for row in data.get('aircraft',[]) if isinstance(data.get('aircraft'),list) else []:
            reviews[row['code']]=row
    admitted=[]
    for code in args.admit:
        combos=normalize(samples[code],reviews[code])
        admitted.append({'code':code,'name':samples[code]['proposal']['name'],'layout_candidate_complete_seven':True,
                         'runtime_enabled':False,'runtime_ready':False,'combinations':combos})
    previous=json.loads(args.previous.read_text(encoding='utf-8')) if args.previous else None
    reviewed=list(reviews)
    if previous:
        existing={a['code']:a for a in previous['aircraft']}
        existing.update({a['code']:a for a in admitted})
        admitted=list(existing.values())
        reviewed=list(dict.fromkeys(previous['reviewed_aircraft']+reviewed))
    result={'schema':1,'purpose':'动画机体完整七组合候选与排除依据；供接入线程取用，不是安装包或运行白名单',
            'baseline':'v2.4.0','steam_build':9855922,'compiled':False,'tests_run':False,'installed':False,
            'game_process_access':False,'assets_generated':False,'runtime_enabled':False,
            'reviewed_aircraft':reviewed,
            'complete_seven_candidates':len(admitted),'complete_combination_cases':len(admitted)*7,
            'aircraft':admitted,'reviews':(previous['reviews'] if previous else [])+evidence,
            'geometry_audit':previous['geometry_audit'] if previous else compact(json.loads(args.geometry_audit.read_text(encoding='utf-8'))),
            'base_batch_sha256':fingerprint(args.batch),'control_sources':{str(path).replace('\\','/'):fingerprint(path) for path in args.control_source},
            'analysis_sources':{
                name:{'repository_lf_sha256':fingerprint(Path(__file__).with_name(name)),
                      'crlf_representation_sha256':hashlib.sha256(Path(__file__).with_name(name).read_bytes().replace(b'\r\n',b'\n').replace(b'\n',b'\r\n')).hexdigest()}
                for name in ('review_animated_us_bays.py','review_animated_sukhoi.py','review_animated_dark_falken.py',
                             'review_animated_fb22.py','read_cooked_skeletal_mount.py','analyze_sp_switch_animations.py')},
            'source_normalization_note':'逐机回执保留分析当时指纹；US/Sukhoi脚本入库仅统一行尾及末尾空行，不改逻辑；保留原分析指纹，另列入库源码指纹，不重新运行几何。',
            'generator_sha256':fingerprint(Path(__file__)),
            'qualification_boundary':'布局候选完整不等于运行接入。接入前必须同时实现provider显示/发射、Parts连续性、必要骨骼刷新、可视门槛及逐实例失败归还；仍按全部七组合一起启用。'}
    result['runtime_intake_boundary']={
        'queue_and_pool_counts':'仍以null保留未知值；MaxLoadedCount是装填窗口，Socket数及几何位置数不证明实际逻辑队列Num或持久组件池Num',
        'native_queue_source':'src/weapon_bridge.cpp::weaponSlotArray/capture读取当前有效数组Num/Max/窗口/游标并保存持久mountOrdinal，不读取备用容量作有效槽',
        'native_pool_source':'src/su30sm_fixed_mount.hpp的独立持久组件数组Num身份核对；新机不能照抄三正式机规格',
        'pose_source':'资源全开端点不等于运行组件空间/世界空间；必须核对原生Socket变换、单位/缩放、容差和当帧骨骼/世界变换见证',
        'bone_update_flags':'bNoSkeletonUpdate、bPauseAnims与组件Tick须按真实反射分别核对和归还；旧控制头不等于已完成新机宿主，不能把停止骨骼更新当成仅暂停动画',
        'current_game_read':False,'install_ready':False}
    if previous:
        result['analysis_sources']=previous['analysis_sources']
        result['source_normalization_note']=previous['source_normalization_note']
        result['review_rounds']=previous.get('review_rounds',[])+[{
            'title':args.round_title,'previous_snapshot_sha256':fingerprint(args.previous),
            'reviewed_aircraft':list(reviews),'new_admitted_aircraft':args.admit,
            'geometry_audit':compact(json.loads(args.geometry_audit.read_text(encoding='utf-8'))),
            'analysis_sources':{str(path).replace('\\','/'):fingerprint(path) for path in args.analysis_source},
            'control_sources':{str(path).replace('\\','/'):fingerprint(path) for path in args.control_source},
            'generator_sha256':fingerprint(Path(__file__))}]
        result['latest_review_by_aircraft']=dict(previous.get('latest_review_by_aircraft',{}))
        for item in evidence:
            for a in item['report'].get('aircraft',[]) if isinstance(item['report'].get('aircraft'),list) else []:
                result['latest_review_by_aircraft'][a['code']]={'analysis':item['analysis'],'sha256':item['sha256']}
    # 面/顶点/索引块ID是未来配方，不是可丢弃的101键采样；保留完整ID列表供资源专题使用。
    result['future_resource_recipes']=deepcopy(previous.get('future_resource_recipes',{})) if previous else {}
    for code,review in reviews.items():
        recipe=review.get('geometry_routes',{}).get('requested_asymmetric_layout',{}).get('future_resource_recipe')
        if recipe:result['future_resource_recipes'][code]=deepcopy(recipe)
    args.output.write_bytes((json.dumps(result,ensure_ascii=False,indent=2)+'\n').encode('utf-8'))
    print(json.dumps({'aircraft':[a['code'] for a in admitted],'cases':len(admitted)*7},ensure_ascii=False))


if __name__=='__main__':
    main()
