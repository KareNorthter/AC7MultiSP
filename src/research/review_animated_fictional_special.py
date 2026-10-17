"""DarkStar与ADF-11F特殊兵装的离线整机七组合复核。

只读取显式manifest/batch/目录索引；复用公共几何与动画读取器。
不编译、不运行游戏或测试、不安装、不生成游戏资产；JSON不输出原始网格。
--manifest可重复，另指定--batch、--uav-index、--output。
本轮优先验证唯一完整原架跨provider借位；局部方案不越过整机七组合门槛。
"""
import argparse
import base64
from collections import Counter
import hashlib
import itertools
import json
from pathlib import Path

import numpy as np

import review_animated_sukhoi as geometry
from analyze_sp_switch_animations import Assets, animation_tracks, poses
from build_external_mount_plan import groups, aircraft_context
from read_cooked_skeletal_mount import read_skeletal_mesh, skin_points
from review_animated_dark_falken import canonical_triangles, edge_crossings, triangle_bones
from split_static_mount_asset import parse_mesh
from summarize_mount_assets import properties, resolve, value
from survey_aircraft_mounts import reference_bones


WEAPONS={'dark':['SASM','XSDB','PLSL'],'adf11f':['TLS','QAAM','UAV']}
MESHES={'SASM':'w_sasm_a0_SM','XSDB':'w_xsdb_a0_closed_SM','PLSL':None,
        'TLS':None,'QAAM':'w_qaam_a0_SM','UAV':'w_uav_x0_SM'}
mesh_cache={}


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def static(name):
    if name not in mesh_cache:
        doc=geometry.assets.load(name)
        export=next(e for e in doc['Exports'] if e['ObjectName']==name)
        mesh_cache[name]=parse_mesh(base64.b64decode(export['Extras']))
    return mesh_cache[name]


def triangles(model,lod=0):
    entry=model['lods'][lod]
    points=np.asarray(entry['points'])
    index=np.asarray(entry['buffers'][0]['values']).reshape(-1,3)
    return points[index]


def strict_crossings(first,second):
    """严格边穿面为新增侵入的正见证；相接/共面不按穿面计数。"""
    low,high=first.min(1),first.max(1)
    rlow,rhigh=second.min(1),second.max(1)
    candidates=np.all(low[:,None,:]<=rhigh[None,:,:]+1e-7,2)&np.all(high[:,None,:]>=rlow[None,:,:]-1e-7,2)
    ia,ib=np.nonzero(candidates)
    if not len(ia):return {'strict_crossing_pairs':0,'first_faces':[],'witness':None}
    forward,fw=edge_crossings(first[ia],second[ib])
    reverse,rw=edge_crossings(second[ib],first[ia])
    hit=forward|reverse
    witness=fw or rw
    if witness:
        k=witness['pair']
        witness={'first_triangle':int(ia[k]),'second_triangle':int(ib[k]),
                 'edge_origin':'first' if fw else 'second',
                 'edge_parameter':witness['edge_parameter'],
                 'face_barycentric':witness['face_barycentric']}
    return {'strict_crossing_pairs':int(hit.sum()),
            'first_faces':sorted({int(v) for v in ia[hit]}),'witness':witness}


def rack_frames(group):
    if group['component_class']=='SkeletalMeshComponent':
        frames,index,info,_=geometry.skin(group['rack_mesh'])
        return frames,index,info
    lod=static(group['rack_mesh'])['lods'][0]
    points=np.asarray(lod['points'])
    return np.broadcast_to(points,(101,*points.shape)),np.asarray(lod['buffers'][0]['values']).reshape(-1,3),geometry.geometry(group['rack_mesh'])[1]


def rack_weapon(rack_group,provider,socket,weapon):
    frames,index,info=rack_frames(rack_group)
    # 省略Socket旋转只为离线基线作零假设，运行时仍须读取真实原生完整变换。
    projectile,pindex=geometry.projectile(provider,socket,MESHES[weapon],True)
    counts=[];faces=[];selected=[]
    for frame in range(101):
        result=strict_crossings(frames[frame][index],projectile[frame][pindex])
        counts.append(result['strict_crossing_pairs']);faces.append(result['first_faces'])
        if frame in [0,25,50,75,100]:selected.append({'frame':frame,**result})
    return {'rendered_rack':rack_group['group'],'provider_group':provider['group'],'socket':socket,
            'weapon':weapon,'native_weapon_mesh':MESHES[weapon],
            'frames':101,'strict_crossing_pairs_by_frame':counts,
            'contact_rack_faces_by_frame':faces,'selected_witnesses':selected,
            'max_strict_crossing_pairs':max(counts),
            'full_open_strict_crossing_pairs':counts[-1],
            'rack_sweep':info,
            'omitted_socket_rotation_assumption':next(s for s in provider['sockets'] if s['name']==socket)['serialized_rotation'] is None}


def frame_summary(items):
    return {'min':min(items),'max':max(items),'first':items[0],'last':items[-1],
            'nonzero_frames':[i for i,v in enumerate(items) if v],
            'all_samples_sha256':hashlib.sha256(json.dumps(items,separators=(',',':')).encode()).hexdigest()}


def dark_routes(gs):
    # 正向：XSDB整架作为公共架，SASM借前/后两种原点；任何固定Body穿面不能显隐掩盖。
    static_routes=[]
    for socket in ['sp21','sp23']:
        body=geometry.body_contacts('dark',gs['SWP2'],socket,MESHES['SASM'],True)
        rack=rack_weapon(gs['SWP2'],gs['SWP2'],socket,'SASM')
        bf,bi,_,_=geometry.skin('dark')
        pf,pi=geometry.projectile(gs['SWP2'],socket,MESHES['SASM'],True)
        bt=bf[100][bi];pt=pf[100][pi]
        near=np.all(bt.min(1)<=pt.max((0,1))+1e-7,1)&np.all(bt.max(1)>=pt.min((0,1))-1e-7,1)
        strict_body=strict_crossings(bt[near],pt)
        strict_body['body_triangle_ids']=[int(np.flatnonzero(near)[i]) for i in strict_body.pop('first_faces')]
        static_routes.append({'provider_group':'SWP2','socket':socket,'rack':rack,'body':body,
            'full_open_body_strict_crossing':strict_body,
            'verdict':'rejected_fixed_body_intrusion',
            'reason':'固定Body在完全开舱仍相交；不是门盖收纳阶段，可视显隐无法使其成为完整呈现方案'})
    # 反向：只显SASM原生整架，将XSDB移到另一个现有SASM原点。
    native=rack_weapon(gs['SWP1'],gs['SWP1'],'sp12','SASM')
    borrowed=rack_weapon(gs['SWP1'],gs['SWP1'],'sp12','XSDB')
    additions=[sorted(set(new)-set(old)) for old,new in zip(native['contact_rack_faces_by_frame'],borrowed['contact_rack_faces_by_frame'])]
    body=geometry.body_contacts('dark',gs['SWP1'],'sp12',MESHES['XSDB'],True)
    model=geometry.mesh_cache[gs['SWP1']['rack_mesh']][3]
    return {'static_xsdb_rack_routes':static_routes,
        'single_sasm_rack_route':{'rendered_rack_groups':['SWP1'],'required_animation_groups':['SWP1'],
          'SASM':'SWP1.sp11','XSDB':'SWP1.sp12','native_sasm_support_baseline':native,
          'borrowed_xsdb_support':borrowed,'borrowed_xsdb_body':body,
          'new_rack_face_count_by_frame':[len(x) for x in additions],
          'new_rack_face_ids_frame100':additions[-1],
          'new_rack_face_bones_frame100':[{'triangle':i,'weighted_bones':triangle_bones(model,model['lods'][0],i)} for i in additions[-1]],
          'verdict':'deferred_new_support_surface_crossing',
          'reason':'Body不新增相交，但XSDB触及原生SASM未触及的支撑面，全开仍在；总穿面数较少不能证明挂耳正常连接'},
        'two_native_whole_racks_route':'上一批已证实101帧均相交，不作为本轮可行路线重复交付'}


def uav_shared_rack(gs):
    large,small=static(gs['SWP2']['rack_mesh']),static(gs['SWP3']['rack_mesh'])
    if len(large['lods'])!=len(small['lods']):raise ValueError('ADF11原架LOD数不同')
    subset=[]
    for i,(a,b) in enumerate(zip(large['lods'],small['lods'])):
        x,y=canonical_triangles(a),canonical_triangles(b)
        subset.append({'lod':i,'qaam_rack_faces':len(x),'uav_rack_faces':len(y),
                       'uav_faces_missing_from_qaam':len(y-x)})
    base=canonical_triangles(small['lods'][0])
    lod=large['lods'][0];points=lod['points'];indices=lod['buffers'][0]['values']
    added=[];added_source_ids=[]
    for i in range(0,len(indices),3):
        tri=[points[j] for j in indices[i:i+3]]
        canonical=tuple(sorted(tuple(round(v,5) for v in p) for p in tri))
        if canonical not in base:added.append(tri);added_source_ids.append(i//3)
    rows=[]
    for socket in ['sp31','sp32']:
        frame,index=geometry.projectile(gs['SWP3'],socket,MESHES['UAV'],True)
        original=strict_crossings(triangles(small),frame[0][index])
        shared=strict_crossings(triangles(large),frame[0][index])
        extra=strict_crossings(np.asarray(added),frame[0][index])
        body=geometry.body_contacts('adf11f',gs['SWP3'],socket,MESHES['UAV'],True)
        extra['qaam_rack_triangle_ids']=[added_source_ids[i] for i in extra.pop('first_faces')]
        rows.append({'socket':socket,'original_uav_rack':original,'whole_qaam_rack':shared,
                     'qaam_only_additional_geometry':extra,'body':body})
    return {'all_lod_subset':subset,'quantization_resource_units':1e-5,
      'geometric_subset_proven':all(v['uav_faces_missing_from_qaam']==0 for v in subset),
      'uav_new_missile_rail_crossings':rows,
      'verdict':'deferred_extra_qaam_rail_intrusion',
      'reason':'UAV原架虽是QAAM整架面子集，QAAM额外导轨仍穿入原生UAV形状；不通过藏住UAV或假折叠缩小模型掩盖'}


def asymmetric_uav_recipe(gs):
    """仅设计原生静态索引分区配方；不生成、副本转换或写入任何游戏资产。"""
    qaam,uav=static(gs['SWP2']['rack_mesh']),static(gs['SWP3']['rack_mesh'])
    rows=[]
    for li,(large,small) in enumerate(zip(qaam['lods'],uav['lods'])):
        source_triangles=np.asarray(large['buffers'][0]['values']).reshape(-1,3)
        points=np.asarray(large['points']);base=canonical_triangles(small)
        keep=[]
        for tri in source_triangles:
            vertex=points[tri]
            canonical=tuple(sorted(tuple(round(float(v),5) for v in p) for p in vertex))
            keep.append(bool(np.all(vertex[:,1]>0) or (np.all(vertex[:,1]<0) and canonical in base)))
        kept_vertices=set(source_triangles[np.asarray(keep)].reshape(-1).tolist())
        buffer_plans=[]
        for buffer in large['buffers']:
            values,group=buffer['values'],buffer['group'];kept=[];removed=[];cross=[]
            for start in range(0,len(values),group):
                block=values[start:start+group];flags={int(v) in kept_vertices for v in block}
                (cross if len(flags)>1 else kept if True in flags else removed).append(start//group)
            buffer_plans.append({'name':buffer['name'],'group_width':group,'index_byte_width':buffer['width'],
                'kept_blocks_zero_based':kept,'removed_blocks_zero_based':removed,'cross_boundary_blocks_zero_based':cross,
                'future_write_rule':'保留块字节不动；移除块全部索引退化为该块原首顶点，保持长度与元数据；本轮不执行'})
        if any(p['cross_boundary_blocks_zero_based'] for p in buffer_plans):
            raise ValueError('计划分区跨邻接块，不能交付假完整索引配方')
        negative=int(sum(keep[i] and np.all(points[t,1]<0) for i,t in enumerate(source_triangles)))
        positive=int(sum(keep[i] and np.all(points[t,1]>0) for i,t in enumerate(source_triangles)))
        proposed_triangles=points[source_triangles[np.asarray(keep)]]
        geometry_checks=[]
        def canonical_face(tri):
            return tuple(sorted(tuple(round(float(v),5) for v in p) for p in tri))
        for label,group,socket,original_rack in [
                ('UAV',gs['SWP3'],'sp31',small),
                ('QAAM',gs['SWP2'],'sp22',large),
                ('QAAM',gs['SWP2'],'sp24',large)]:
            original_points=np.asarray(original_rack['points'])
            original_index=np.asarray(original_rack['buffers'][0]['values']).reshape(-1,3)
            original_triangles=original_points[original_index]
            position,quaternion=geometry.socket_samples(group,socket,True)[0][0]
            for weapon_lod,weapon_model in enumerate(static(MESHES[label])['lods']):
                weapon_points=np.asarray(weapon_model['points'])@geometry.quat_matrix(quaternion).T+position
                weapon_index=np.asarray(weapon_model['buffers'][0]['values']).reshape(-1,3)
                proposed_crossing=strict_crossings(proposed_triangles,weapon_points[weapon_index])
                native_crossing=strict_crossings(original_triangles,weapon_points[weapon_index])
                proposed_faces={canonical_face(proposed_triangles[i]) for i in proposed_crossing['first_faces']}
                native_faces={canonical_face(original_triangles[i]) for i in native_crossing['first_faces']}
                geometry_checks.append({'weapon':label,'socket':socket,'weapon_lod':weapon_lod,
                    'native_original_rack_crossing_pairs':native_crossing['strict_crossing_pairs'],
                    'future_recipe_crossing_pairs':proposed_crossing['strict_crossing_pairs'],
                    'new_crossed_geometry_faces_beyond_native_template':len(proposed_faces-native_faces),
                    'analysis_only':True,'omitted_socket_rotation_assumed_zero':True})
        rows.append({'lod':li,'original_triangles':len(source_triangles),'keep_triangles':int(sum(keep)),
            'left_original_uav_support_triangles':negative,'right_original_qaam_triangles':positive,
            'kept_main_triangle_ids_zero_based':[i for i,v in enumerate(keep) if v],
            'kept_source_vertex_ids_zero_based':sorted(kept_vertices),'all_index_buffers':buffer_plans,
            'geometry_only_native_template_comparisons':geometry_checks})
    left=next(s for s in gs['SWP3']['sockets'] if s['name']=='sp31')
    right=[next(s for s in gs['SWP2']['sockets'] if s['name']==n) for n in ['sp22','sp24']]
    # 原生完整QAAM架和UAV架都为单section单material；这不提供左右显隐能力。
    materials={}
    for name in [gs['SWP2']['rack_mesh'],gs['SWP3']['rack_mesh']]:
        doc=geometry.assets.load(name);prop=properties(next(e for e in doc['Exports'] if e['ObjectName']==name))
        materials[name]=[{'material':resolve(doc,item['MaterialInterface']),
                          'slot':item.get('MaterialSlotName')} for item in prop.get('StaticMaterials',[])]
    return {'requested_layout':{'UAV':{'provider_group':'SWP3','sockets':['sp31'],'side':'negative_Y_left',
            'resource_position':left['reference_position'],'physical_display_cap':1,'native_window':2},
        'QAAM':{'provider_group':'SWP2','sockets':['sp22','sp24'],'side':'positive_Y_right',
                'resource_positions':[s['reference_position'] for s in right],'physical_display_cap':2,'native_window':2}},
        'whole_current_qaam_rack_usable':False,
        'native_left_right_material_visibility_available':False,
        'native_component_split_available':False,
        'native_component_boundary':'Pawn只有SWP2、SWP3两个完整静态组件，没有左右可独立组件；每级原架仅一个section且材质共用',
        'material_sources':materials,
        'future_resource_recipe':{'source_mesh':gs['SWP2']['rack_mesh'],
            'source':geometry.assets.used[gs['SWP2']['rack_mesh'].casefold()],
            'subset_support_source':geometry.assets.used[gs['SWP3']['rack_mesh'].casefold()],
            'keep_rule':'原QAAM架所有Y>0三角面，加上Y<0且完整三角面属于原UAV架的面；不按原点或单个顶点裁切',
            'quantization_resource_units':1e-5,'lods':rows,
            'requires_new_derived_static_asset':True,'requires_new_mesh_geometry':False,'requires_new_socket':False,
            'render_plan':'未来只显示这一派生SWP2复合整架；原SWP3仅隐藏架体，保留原生Socket与父链；不能再叠加完整SWP3造成右底座重复',
            'untouched_fields':['原全部顶点/UV/材质','原全部Socket/父链','原长度/LOD结构/Bounds/碰撞元数据'],
            'required_resource_topic_checks':['全部LOD/全部索引块退化规则','编码往返与非索引字节不变','新资源独立身份及哈希','实际QAAM和UAV支撑/姿态'],
            'asset_generated':False},
        'current_disposition':'requires_rack_asset_then_lifecycle_review',
        'not_currently_deliverable':True,'persistent_uav_allowlist_enabled':False}


def xsdb_animation():
    name='w_xsdb_a0';document=geometry.assets.load(name+'_Skeleton')
    export=next(e for e in document['Exports'] if e['ObjectName']==name+'_Skeleton')
    bones=reference_bones(document,export)
    sequence=geometry.assets.load(name+'_Anim');tracks,meta=animation_tracks(sequence,bones)
    if set(tracks)!=set(range(len(bones))):raise ValueError('XSDB动画缺轨，不猜缺省骨姿态')
    model=read_skeletal_mesh(geometry.assets.load(name),name)
    bone_names=[b['name'] for b in bones]
    bp=geometry.assets.load('plwp_xsdb_a0')
    component=properties(next(e for e in bp['Exports'] if e['ObjectName']=='WeaponAnimationMesh_GEN_VARIABLE'))
    native=properties(next(e for e in bp['Exports'] if e['ObjectName']=='Default__plwp_xsdb_a0_C'))
    values=[]
    for i,pose in enumerate(poses(bones,tracks,meta['frames'])):
        points=np.asarray(skin_points(model,model['lods'][0],dict(zip(bone_names,pose))))
        values.append({'frame':i,'skinned_local_bounds':geometry.box(points),
                       'local_vertex_sha256':hashlib.sha256(points.astype('<f8').tobytes()).hexdigest()})
    return {'logical_class':'plwp_xsdb_a0_C','folded_mesh':'w_xsdb_a0_closed_SM',
        'animation_component_mesh':name,'animation':name+'_Anim','after_animation_mesh':resolve(bp,native['AnimationEndStaticMesh']),
        'native_animation_data':component['AnimationData'],'sequence_metadata':meta,'sampled_frames':values,
        'first_widening_key':next((r['frame'] for r in values if r['skinned_local_bounds']['max'][1]>values[0]['skinned_local_bounds']['max'][1]+.001),None),
        'nominal_resource_seconds_at_saved_rate':meta['asset_length']/component['AnimationData']['SavedPlayRate'],
        'native_ballistic_defaults':{k:native.get(k) for k in ['InitialSpeed','FreefallAcceleration','FreefallRotation','RollAnimationDelay','RollAnimationRate','bFireAllInOneShot']},
        'preserve_native_deployment':True,
        'boundary':'25帧和SavedPlayRate=0.25是本型号资源事实；未确认动画启动相对于真实离架的时刻、实际播放倍率及飞行姿态，不把2秒滚转延迟当展开延迟，也不把展开模型用于未离架陈列'}


def uav_lifecycle(index_path):
    doc=geometry.assets.load('plwp_WeaponUAV')
    native=properties(next(e for e in doc['Exports'] if e['ObjectName']=='Default__plwp_WeaponUAV_C'))
    child=resolve(doc,native['WeaponData']['SpawnWeaponClass']).removesuffix('_C')
    cd=geometry.assets.load(child)
    cp=properties(next(e for e in cd['Exports'] if e['ObjectName']=='Default__'+child+'_C'))
    curves={}
    for field in ['DeviationShootingCurve','DeviationShootingCurve_Ground','ShootingPositionOffsetCurve']:
        name=resolve(doc,native['WeaponData'][field]);data=geometry.assets.load(name)
        curves[field]={'asset':name,'exports':[{'name':e['ObjectName'],'properties':properties(e)} for e in data['Exports']]}
    inventory=json.loads(index_path.read_text(encoding='utf-8'))
    listed=[a['name'] for pak in inventory for a in pak['assets']]
    folding_exports=[e['ObjectName'] for e in doc['Exports'] if any(k in properties(e) for k in ['SkeletalMesh','AnimationData','AnimationEndStaticMesh'])]
    return {'logical_class':'plwp_WeaponUAV_C','native_parent':'WeaponUAV','native_weapon_mesh':'w_uav_x0_SM',
        'default_static_shape_bounds':geometry.geometry('w_uav_x0_SM')[1]['bounds'],
        'max_loaded_count':native['MaxLoadedCount'],'load_time':native['LoadTime'],'lifetime':native['LifeTime'],
        'attack_start_time':native['AttackStartTime'],'attack_time':native['AttackTime'],
        'attack_projectile_class':child+'_C','attack_projectile_defaults':{k:cp.get(k) for k in ['WeaponID','Damage','LifeTime','MaxLoadedCount']},
        'attack_projectile_role':'SpawnWeaponClass是UAV攻击弹体，不是第二个已部署UAV，也不是返回Actor身份',
        'shot_interval':native['WeaponData']['ShotIntervalTime'],'shot_range':native['WeaponData']['ShotRange'],
        'shooting_curves':curves,'last_destination_point_component':resolve(doc,native['LastDestinationPointSceneComponent']),
        'directory_index':{'path':str(index_path),'sha256':digest(index_path),'assets':listed},
        'weapon_blueprint_fold_animation_exports':folding_exports,
        'folding_model_boundary':'两份明确PAK索引的w_uav_x0目录只有静态网格、材质和纹理；该武器蓝图没有SkeletalMesh/AnimationData引用。不能据此发明折叠模型，也不能证明原生C++永不改显示姿态。',
        'roles':{'docked':'SWP3.sp31/sp32部署参考，任务实际Actor/静态显示池角色未读取',
                 'deploying':'原生WeaponUAV类负责离架与加速，具体Actor绑定未核对',
                 'attacking':'WeaponData生成plwp_plsl_UAV_C攻击弹体，按真实类生命周期处理',
                 'returning':'原生返回/销毁与停挂重现的回调、对象身份及是否返回原Actor未核对',
                 'reload':'LoadTime与LifeTime不是同一个状态，不从15.2与15的差推断返回或补装规则'},
        'required_lifecycle_evidence':['原生UAV武器实例、飞行Actor、攻击弹体与激活器归属的身份/serial映射',
            '切SP后继续/结束/返回/销毁分别由真实WeaponUAV状态驱动，失败不得释放仍在飞行的对象',
            '返回与补装不能复制扣弹/冷却，也不能按剩余库存直接画回停挂UAV',
            '补给、检查点、死亡及组件重建的归还；TLS_x3自然收尾另核，不套ADFX-01持续类特判'],
        'persistent_allowlist_enabled':False,'lifecycle_verified':False,
        'verdict':'deferred_native_lifecycle_and_pose_roles'}


def assignment(weapon,owner,provider,sockets):
    window=owner['weapon']['defaults'].get('MaxLoadedCount',{}).get('value')
    fixed=weapon in ['TLS','PLSL']
    return {'weapon':weapon,'owner_group':owner['group'],'owner_class':owner['weapon']['class'],
        'provider_group':provider['group'],'sockets':sockets,'native_weapon_mesh':MESHES[weapon],
        'replacement_mesh':MESHES[weapon] if provider['group']!=owner['group'] else None,
        'native_load_window':window,'display_cap':min(len(sockets),window) if window is not None else None,
        'below_native_window':window is not None and len(sockets)<window,
        'position_role':'native_effect_source' if fixed else 'uav_deployment_reference' if weapon=='UAV' else 'ordnance_station_candidate',
        'native_emitter_rack_mesh':provider['rack_mesh'] if fixed else None,
        'fixed_body_policy':'保留完整原生效果源，Socket数不冒充消耗弹/逻辑窗口' if fixed else None,
        'display_release_same_provider':True,'runtime_pool_count':None,'runtime_logical_count':None}


def make_aircraft(code,route):
    group_list=groups(geometry.survey,code);gs={g['group']:g for g in group_list}
    owners=dict(zip(WEAPONS[code],group_list))
    row={'code':code,'alias':'ad11' if code=='adf11f' else None,'name':'DarkStar' if code=='dark' else 'ADF-11F Raven',
         'groups':group_list,'native_context':aircraft_context(geometry.survey,code),
         'actual_component_bindings':geometry.actual_component_bindings(code,group_list),
         'combinations':[],'geometry_routes':route,'complete_seven_design':False,
         'layout_candidate_complete_seven':False,'runtime_ready':False,'runtime_enabled':False}
    for size in [1,2,3]:
        for combo in itertools.combinations(WEAPONS[code],size):
            native=size==1
            if native:
                plan={w:(owners[w]['group'],[s['name'] for s in owners[w]['sockets']]) for w in combo}
            elif code=='dark' and 'SASM' in combo and 'XSDB' in combo:
                plan={'SASM':('SWP1',['sp11']),'XSDB':('SWP1',['sp12'])}
                if 'PLSL' in combo:plan['PLSL']=('SWP3',['sp31','sp32'])
            elif code=='adf11f':
                plan={w:(owners[w]['group'],['sp22','sp24'] if w=='QAAM' and 'UAV' in combo else
                          ['sp31'] if w=='UAV' and 'QAAM' in combo else
                          [s['name'] for s in owners[w]['sockets']]) for w in combo}
            else:plan={w:(owners[w]['group'],[s['name'] for s in owners[w]['sockets']]) for w in combo}
            rendered=sorted({g for g,_ in plan.values()})
            if code=='adf11f' and 'QAAM' in combo and 'UAV' in combo:rendered=[g for g in rendered if g!='SWP3']
            required=sorted(g for g in set(rendered)|{g for g,_ in plan.values()} if gs[g]['component_class']=='SkeletalMeshComponent')
            c={'weapons':list(combo),'assignments':[assignment(w,owners[w],gs[g],s) for w,(g,s) in plan.items()],
               'rendered_rack_groups':rendered,'required_animation_groups':required,
               'rack_plan':{'visible_whole_native_groups':rendered,'required_animation_groups':required,
                            'hidden_self_only':[g for g in gs if g not in rendered],'new_socket':False,'split_asset':False},
               'integration_mode':'native_passthrough' if native else 'research_only_whole_aircraft_deferred',
               'status':'native_reuse' if native else 'deferred','disposition':'native_single' if native else 'deferred_whole_aircraft',
               'integration_allowed':native,'runtime_ready':native,'runtime_verified':False,
               'required_animation_work':[] if native else ['按真实owner与provider路由，显示与发射取同一个更新后Socket，不改原父链、网格或逻辑队列'],
               'blockers':[],'projectile_pair_checks':[],'blocker_kind':None}
            if not native:
                if code=='dark' and 'SASM' in combo and 'XSDB' in combo:
                    c['blocker_kind']='rack_support_geometry';c['blockers']=['唯一活动SASM架借XSDB仍新增支撑面穿越，全开持续，不能靠隐藏挂架/门体或始终藏弹体解决']
                elif code=='adf11f' and 'UAV' in combo:
                    c['blocker_kind']='uav_lifecycle'
                    c['blockers']=['真实WeaponUAV离架、部署Actor、攻击弹、返回/销毁和补装归属未核对；不开放生命周期白名单']
                    if 'QAAM' in combo:
                        c.update(status='requires_rack_asset',blocker_kind='derived_static_rack_and_uav_lifecycle',resource_split_required=True)
                        c['blockers'].append('用户指定左UAV/右两QAAM：完整QAAM左侧额外导轨仍穿入UAV，原单材质不能左右隐藏；需未来原生静态索引分区派生资源')
                        c['rack_plan'].update(split_asset=True,asset_generated=False,
                            derived_render_resource='future_recipe_only_not_existing_asset',
                            future_recipe='geometry_routes/requested_asymmetric_layout/future_resource_recipe',
                            visible_whole_native_groups=[],component_mesh_override_required=True,
                            original_full_SWP2_must_not_be_shown_with_UAV=True)
                else:
                    c['blocker_kind']='whole_aircraft_gate';c['blockers']=['本组合可保留原生区域，整机其余组合受阻，不单独交付']
            bodies=[]
            for a in c['assignments']:
                if not a['native_weapon_mesh']:continue
                for socket in a['sockets']:
                    body,info=geometry.weapon_at(gs[a['provider_group']],socket,a['native_weapon_mesh'],True)
                    bodies.append((a['weapon']+':'+socket,body))
            for (an,ab),(bn,bb) in itertools.combinations(bodies,2):
                if an.split(':')[0]==bn.split(':')[0]:continue
                c['projectile_pair_checks'].append({'first':an,'second':bn,**geometry.pair_separation(ab,bb)})
            c['all_different_weapon_obbs_separated']=all(v['all_frames_separated'] for v in c['projectile_pair_checks'])
            row['combinations'].append(c)
    row['whole_aircraft_gate']={'required':'全部七组合完整才交付；三单仍直通原生',
        'eligible':False,'status':'defer_whole_aircraft','combination_count':7,
        'reason':'SASM＋XSDB和三挂存在新增支撑面问题' if code=='dark' else 'UAV生命周期/姿态重大未知，QAAM并架另有新增导轨穿面'}
    return row


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--manifest',type=Path,action='append',required=True)
    parser.add_argument('--batch',type=Path,required=True)
    parser.add_argument('--uav-index',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    geometry.assets=Assets(args.manifest);geometry.survey=geometry.CheckedSurvey(geometry.assets)
    batch=json.loads(args.batch.read_text(encoding='utf-8'))
    expected={a['code']:a for a in batch['aircraft'] if a['code'] in WEAPONS}
    if set(expected)!=set(WEAPONS):raise ValueError('显式基线必须包含DarkStar与官方adf11f')
    current={c:{g['group']:g for g in groups(geometry.survey,c)} for c in WEAPONS}
    for code,gs in current.items():
        if [g['weapon']['class'] for g in gs.values()] != [g['weapon']['class'] for g in expected[code]['groups']]:
            raise ValueError('当前资源真实武器类与基线不同：'+code)
    dark=dark_routes(current['dark']);uav=uav_shared_rack(current['adf11f'])
    uav['requested_asymmetric_layout']=asymmetric_uav_recipe(current['adf11f'])
    result={'schema':'animated-fictional-special-review-v1',
        'compiled':False,'tests_run':False,'installed':False,'game_process_access':False,'assets_generated':False,
        'purpose':'有限度核对整架借位与专用UAV角色；不从几何通过推导生命周期或运行白名单',
        'aircraft':[make_aircraft('dark',dark),make_aircraft('adf11f',uav)],
        'xsdb_deployment_review':xsdb_animation(),'uav_lifecycle_review':uav_lifecycle(args.uav_index),
        'sources':{'manifests':[{'path':str(p),'sha256':digest(p)} for p in args.manifest],
                   'batch':{'path':str(args.batch),'sha256':digest(args.batch)}},
        'asset_sources':geometry.assets.used,
        'analyzer':{'path':str(Path(__file__).resolve()),'sha256':digest(__file__),
                    'dependencies':{name:digest(Path(__file__).with_name(name)) for name in
                       ['review_animated_sukhoi.py','read_cooked_skeletal_mount.py','review_animated_dark_falken.py','analyze_sp_switch_animations.py','split_static_mount_asset.py']}},
        'limits':['候选新增侵入以LOD0正见证否定，不把LOD0零相交外推全部LOD通过',
                  '原生Body/SWP整序列同步101帧，不等于独立通道混合、连续插值或所有飞行姿态',
                  '省略Socket旋转只以零作为明确离线假设；运行时必须读取并核验原生完整变换',
                  '不隐藏固定Body或导轨，不把另一武器模型冒充逻辑兵装，不把UAV部署点当普通一次性导弹槽']}
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({'output':str(args.output),'dark_complete':False,'adf11f_complete':False,
        'dark_xsdb_new_support_faces_at_full_open':len(dark['single_sasm_rack_route']['new_rack_face_ids_frame100']),
        'uav_extra_qaam_rail_crossings':[v['qaam_only_additional_geometry']['strict_crossing_pairs'] for v in uav['uav_new_missile_rail_crossings']]},ensure_ascii=False))


if __name__=='__main__':main()
