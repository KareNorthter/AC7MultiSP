"""将Su-47左右分区七组合合入既有动画覆盖层；不生成资源或操作游戏。"""
import argparse
from copy import deepcopy
import hashlib
import json
from pathlib import Path


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--previous',type=Path,required=True)
    parser.add_argument('--batch',type=Path,required=True)
    parser.add_argument('--review',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    if args.output.exists():raise ValueError('输出必须使用新路径')
    result=json.loads(args.previous.read_text(encoding='utf-8'))
    review=json.loads(args.review.read_text(encoding='utf-8'))
    batch=json.loads(args.batch.read_text(encoding='utf-8'))
    plane=next(a for a in result['aircraft'] if a['code']=='su47')
    sample=next(a for a in batch['aircraft'] if a['code']=='su47')
    groups={g['group']:g for g in sample['groups']}
    if len(review['plans'])!=7 or len(plane['combinations'])!=7:
        raise ValueError('须整机七组合齐全')
    review_source={'analysis':args.review.name,'sha256':digest(args.review)}
    plane.update(layout_policy='su47_native_owner_left_partition_v2',
        layout_candidate_complete_seven=False,seven_case_design_complete=True,
        integration_deferred=True,runtime_enabled=False,runtime_ready=False,
        exact_body38_pose_required=False,native_owner_socket_only=True,latest_review=review_source,
        required_resource_recipes=['su47_SWP1_left','su47_SWP2_right','su47_SWP3_right'],
        geometry_boundary='七组合选位完整但整机暂缓；未裁半侧原架会产生较大离架扫架风险，须先完成原生骨架挂架副本与编码/加载依据，不能直接注册整架候选')
    for combo in plane['combinations']:
        label='+'.join(combo['weapons'])
        keys=review['plans'][label]
        singles=len(combo['weapons'])==1
        if singles:continue
        for assignment in combo['assignments']:
            weapon=assignment['weapon'];owner=assignment['owner_group']
            stations=[key.split(':') for key in keys if key.split(':')[0]==weapon]
            if not stations or any(provider!=owner for _,provider,_ in stations):
                raise ValueError('新版每类兵装只取自身原生组件')
            sockets=[s for _,_,s in stations]
            native=groups[owner]
            by_name={s['name'].lower():s for s in native['sockets']}
            if len(sockets)!=len(set(sockets)) or not set(sockets)<=set(by_name):
                raise ValueError('Socket必须属于自身完整原生表且不重复')
            window=native['weapon']['defaults']['MaxLoadedCount']['value']
            assignment.update(provider_group=owner,provider_rack_mesh=native['rack_mesh'],sockets=sockets,
                replacement_mesh=None,weapon_mesh_replacement=False,physical_candidate_count=len(sockets),
                loaded_window=window,loaded_window_from_blueprint=window,below_window=len(sockets)<window,
                fewer_physical_positions_than_window=len(sockets)<window,
                runtime_logical_count=None,runtime_pool_count=None,
                socket_details=[deepcopy(by_name[s]) for s in sockets],
                native_transform_only=True,display_release_same_provider=True,
                mapping_boundary='仅筛选自身原生Socket有序表；显示与真实离架共用实时原生变换，不改Socket、弹体类或逻辑队列')
        rendered=review['rack_plans'][label]
        providers={a['provider_group'] for a in combo['assignments']}
        required=sorted(providers|set(rendered))
        combo['rendered_rack_groups']=rendered
        combo['required_animation_groups']=required
        combo['rack_plan']={'rendered_rack_groups':rendered,'required_animation_groups':required,
            'hidden_self_only':sorted(set(groups)-set(rendered)),'preserve_native_parent_chains':True,
            'new_resource_required':False,'new_station_required':False,
            'support':'保留SAAM自身双轨架；UGB/QAAM同形四列架仅呈现一套，空轨保留',
            'accepted_minor_contact':'含SAAM时两原架八处小边唇有局部穿入，须与实玩观感对照'}
        if 'SAAM' in combo['weapons']:
            combo['rack_plan'].update(new_resource_required=True,existing_whole_racks_allowed=False,
                required_derived_meshes={g:'su47_'+g+('_left' if g=='SWP1' else '_right') for g in rendered},
                support='须以SWP1左半与唯一四列架右半保留各自原生支撑；整架仅为已分析的几何源')
        combo['internal_ammo_visibility']={
            'mode':'native_common_bay_state_only','require_native_fully_open':True,
            'native_state':{'request':1,'phase':2,'public_progress':1.0},
            'require_body38_exact':False,'require_resource_end_pose_exact':False,
            'applies_to':'仅多挂受管舱内ammo；三种单挂完全原生',
            'hide_immediately_on':'原生关闭请求、非全开、组件/池身份不符或原生取位不可用',
            'affects_only_ammo_visibility':True,'changes_native_fire_gate':False,
            'continuous_clearance_claimed':False,
            'boundary':'公共舱状态只提供低成本显隐；不以全部Body骨逐位匹配、逐帧几何或完整池扫描作为方案前提'}
        combo['runtime_prerequisites']=[
            '保留每类原生逻辑队列/窗口/游标/持久模型池及自身弹体类，数量必须实采，不从Socket数推导',
            'owner与provider同组；每个物理点只显示一个弹体，显示与离架共用原生Socket及父链',
            'Body与required集合中原生挂架保持必要更新；每帧一次，不在逐弹取位中重复驱动骨骼',
            '只读已核公共舱状态作显隐，发射继续原生门控；不使用旧38骨精确参照',
            '保留原生离架/延迟/下落参数；实测SAAM与QAAM先下落还是等高前移，不能把假设扫掠当真实轨迹']
        combo['required_animation_work']=[
            '复用原生公共spwc域，三挂更新SWP1/2/3，SAAM组合呈现SWP1加唯一四列架',
            '静态边唇接触按用户允许轻微干涉的取舍保留，不按骨拆左右、不生成派生资源']
        combo['geometry_review']='evidence/mount-expansion-animated.json#latest_review_by_aircraft.su47'
        combo['notes']=['新版左右分区；SAAM不再借低位UGB点。全部单挂保留原生。',
            'SAAM等高向前扫掠可触四列架较大面片，QAAM等高向前可触右侧空SAAM轨；不能承诺实际离架仅轻微干涉。',
            '新方案不负责修复产品热路径，机库/任务性能须由主线改动及用户实测核对。']
        combo.update(gameplay_tested=False,dynamic_clearance_verified=False,compiled=False,tests_run=False,installed=False,
                     integration_allowed=False,integration_mode='deferred_skeletal_half_partition',
                     status='deferred',disposition='deferred_skeletal_partition',blocker_kind='skeletal_partition_assets',
                     deferred_reason='含SAAM多挂需要半侧骨架挂架副本，未生成/编码核对/加载；按七组合门槛整机暂缓',
                     blockers=['不能用未裁整架将较大扫架当成轻微接触放行；须骨架半侧资源及完整原生绑定依据'])
    result['reviews'].append({**review_source,'report':review})
    result.setdefault('latest_review_by_aircraft',{})['su47']=review_source
    result.setdefault('future_resource_recipes',{})['su47']={
        'source_review':review_source,'recipe_ids':plane['required_resource_recipes'],
        'details':'最新Su-47评审report.future_skeletal_half_resources；包含完整原面ID，不是已生成资产',
        'assets_generated':False,'loadability_verified':False}
    admitted=[a for a in result['aircraft'] if a['layout_candidate_complete_seven']]
    result['complete_seven_candidates']=len(admitted)
    result['complete_combination_cases']=sum(len(a['combinations']) for a in admitted)
    result.setdefault('analysis_sources',{})['review_su47_native_partition.py']=digest(Path(__file__).with_name('review_su47_native_partition.py'))
    result['analysis_sources']['build_su47_partition_candidate.py']=digest(Path(__file__))
    result['runtime_intake_boundary']['pose_source']='核对真实原生Socket/单位缩放/当帧组件更新；只有实施数值端点比较时才要求相应容差依据。Su47新分区不依赖旧38Body参照，且半侧资源未完成，整机暂缓。'
    result.setdefault('review_rounds',[]).append({'title':'Su-47原生左右分区替代旧借位与全Body精确门槛',
        'previous_snapshot_sha256':digest(args.previous),'reviewed_aircraft':['su47'],
        'new_admitted_aircraft':[],'review':review_source,'physical_design_cases':7,'admission_withheld':True})
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_bytes((json.dumps(result,ensure_ascii=False,indent=2)+'\n').encode('utf-8'))
    print(json.dumps({'code':'su47','cases':7,'layout_policy':plane['layout_policy'],'output':str(args.output)},ensure_ascii=False))


if __name__=='__main__':main()
