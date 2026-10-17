"""整理F-22A/Su-47/Su-57TG七组合接入数据；只生成头文件，不编译或操作游戏。"""
import argparse
from copy import deepcopy
import hashlib
import json
import math
import struct
from pathlib import Path

from analyze_sp_switch_animations import Assets, animation_tracks, graph, poses
from read_cooked_skeletal_mount import read_skeletal_mesh
from summarize_mount_assets import properties, resolve, value
from survey_aircraft_mounts import multiply, reference_bones, rotate, vector


ROOT=Path(__file__).resolve().parents[2]
CODES=('f22a','su47','su57tg')


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def group_index(name):
    if name not in ('SWP1','SWP2','SWP3'):
        raise ValueError('不是三组原生SWP身份：'+str(name))
    return int(name[-1])-1


def asset_object_path(assets,name,object_name=None):
    """包路径来自已校验的实际PAK记录，且目标必须确实是该资源导出。"""
    document=assets.load(name)
    target=object_name or name
    if sum(e['ObjectName']==target for e in document['Exports'])!=1:
        raise ValueError('规范对象不在所引资源中：'+target)
    path=assets.used[name.casefold()]['asset'].replace('\\','/')
    prefix='Nimbus/Content/'
    if not path.startswith(prefix) or not path.endswith('.uasset'):
        raise ValueError('未识别的实际PAK资源路径：'+path)
    return '/Game/'+path[len(prefix):-7]+'.'+target


def reference_path(document,index,package,seen=None):
    """沿真实FPackageIndex外层链取得完整对象路径，不凭短名拼目录。"""
    seen=set() if seen is None else seen
    if not index or index in seen:raise ValueError('对象引用为空或外层链成环')
    seen.add(index)
    record=document['Imports'][-index-1] if index<0 else document['Exports'][index-1]
    outer=record['OuterIndex'];name=record['ObjectName']
    if outer:
        parent=reference_path(document,outer,package,seen)
        return parent+(':' if '.' in parent else '.')+name
    if name.startswith('/'):return name
    return package+'.'+name


def component_binding(assets,pawn,component,mesh_field):
    package=assets.used[pawn.casefold()]['asset']
    package='/Game/'+package.removeprefix('Nimbus/Content/').removesuffix('.uasset')
    document=assets.load(pawn)
    matches=[e for e in document['Exports'] if e['ObjectName'] in (component,component+'_GEN_VARIABLE') and mesh_field in properties(e)]
    if len(matches)!=1:raise ValueError('实际组件模板引用不唯一：'+pawn+'/'+component)
    export=matches[0];props=properties(export)
    mesh=resolve(document,props[mesh_field]);expected=asset_object_path(assets,mesh)
    actual=reference_path(document,props[mesh_field],package)
    if actual!=expected:raise ValueError('Pawn实际网格引用与PAK规范路径不一致')
    anim=props.get('AnimClass')
    animname=resolve(document,anim) if anim else None
    animpath=reference_path(document,anim,package) if anim else None
    if anim:
        blueprint=animname.removesuffix('_C')
        if animpath!=asset_object_path(assets,blueprint,animname):raise ValueError('实际AnimClass规范路径不一致')
    return {'mesh':mesh,'mesh_path':actual,'animclass':animname,'animclass_path':animpath,
            'component_template':export['ObjectName'],
            'component_class_path':reference_path(document,export['ClassIndex'],package)}


def skeletal_binding(assets,mesh,animclass):
    document=assets.load(mesh);export=next(e for e in document['Exports'] if e['ObjectName']==mesh)
    socket_skeleton=resolve(document,properties(export)['Skeleton'])
    mesh_path=asset_object_path(assets,mesh);package=mesh_path.rsplit('.',1)[0]
    skeleton_path=reference_path(document,properties(export)['Skeleton'],package)
    if skeleton_path!=asset_object_path(assets,socket_skeleton):raise ValueError('网格实际Skeleton路径不一致')
    driver=graph(assets,animclass.removesuffix('_C'))
    sequence_path=asset_object_path(assets,driver['sequence'])
    config_path=asset_object_path(assets,driver['bone_config'])
    table=assets.load(driver['bone_config'])
    channels={}
    for raw in table['Exports'][0]['Table']['Data']:
        row=value(raw)
        if 'BoneName' in row and 'FlightEngineOuputName' in row:channels[row['BoneName']]=row['FlightEngineOuputName']
    model=read_skeletal_mesh(document,mesh)
    sp={'spwc','swcc','swc1','swc2','swc3'}
    resource_channels=sorted({channels[b['name']] for b in model['bones'] if channels.get(b['name']) in sp})
    return {'skeleton':socket_skeleton,'skeleton_path':skeleton_path,'sequence':driver['sequence'],
            'sequence_path':sequence_path,'bone_config':driver['bone_config'],'bone_config_path':config_path,
            'resource_sp_channels':resource_channels,'resource_only_public_spwc':resource_channels==['spwc'],
            'runtime_identity_and_channel_qualification':False,
            'boundary':'仅证明实际资源绑定；宿主还须核对运行Body/挂架/Skeleton/AnimClass完整身份及原生求值，不能直接把资格布尔设true'}


def local_rotation(raw):
    if raw is None:return None
    while len(raw)==1 and 'Pitch' not in raw:raw=next(iter(raw.values()))
    p,y,r=(math.radians(float(raw[k]))/2 for k in ('Pitch','Yaw','Roll'))
    sp,cp,sy,cy,sr,cr=math.sin(p),math.cos(p),math.sin(y),math.cos(y),math.sin(r),math.cos(r)
    return (cr*sp*sy-sr*cp*cy,-cr*sp*cy-sr*cp*sy,cr*cp*sy-sr*sp*cy,cr*cp*cy+sr*sp*sy)


def f32(values):
    return struct.unpack('<'+'f'*len(values),struct.pack('<'+'f'*len(values),*values))


def sampled_vector(properties_by_name,name):
    field=properties_by_name[name]
    layout=field['layout'];meta=layout['metadata']
    raw=bytes.fromhex(field['raw'])
    if layout['size']!=12 or len(raw)!=12 or meta['class']!='StructProperty' or meta['name']!=name:
        raise ValueError('实际Socket向量属性的类型/尺寸/名字不匹配')
    result=struct.unpack('<3f',raw)
    if not all(math.isfinite(v) for v in result):raise ValueError('实际Socket字段含非有限数')
    return result


def supplement_endpoints(result,assets,endpoint_files):
    """只消费主管已采文件；只用当时选中、未停止更新的provider证明对应端点。"""
    plane=next(p for p in result['planes'] if p['code']=='su47')
    verified=set()
    for path in endpoint_files:
        capture=json.loads(path.read_text(encoding='utf-8'))
        parts=capture['parts'];config=parts['config']
        if capture['process_write'] or capture['game_functions_called'] or capture['plane']['name']!='AcePlayerPawn_su47_C' or \
           config not in (1,2) or (parts['request'],parts['phase'],parts['progress'][0])!=(1,2,1.0):
            raise ValueError('不是Su47当前UGB/QAAM原生全开只读证据')
        raw_parts=bytes.fromhex(parts['raw'])
        if (struct.unpack_from('<i',raw_parts,0x58)[0],struct.unpack_from('<i',raw_parts,0x274)[0],
            struct.unpack_from('<i',raw_parts,0x178)[0])!=(parts['request'],parts['phase'],config):
            raise ValueError('Parts摘要与原始片段不一致')
        name='SWP'+str(config+1)
        if name in verified:raise ValueError('同provider重复端点采样，需明确选择当前证据版本')
        verified.add(name)
        group=plane['groups'][config];endpoint=plane['endpoints'][name]
        rack=next(r for r in capture['racks'] if r['identity']['name']==name)
        if rack['animation_flags']!=0 or not rack['pose_frame'] or rack['mesh_sockets']:
            raise ValueError('provider停更、无发布帧或存在尚未处理的mesh级Socket覆盖')
        for key,expected in [('mesh',group['rack_path']),('skeleton',group['skeleton_path'])]:
            item=rack[key]
            if item['outer_name']+'.'+item['name']!=expected:raise ValueError('采样实际资产与规范注册不符')
        model=read_skeletal_mesh(assets.load(group['rack']),group['rack'])
        sd=assets.load(endpoint['skeleton'])
        sx=next(e for e in sd['Exports'] if e['ObjectName']==endpoint['skeleton'])
        sb=reference_bones(sd,sx);tracks,metadata=animation_tracks(assets.load(endpoint['sequence']),sb)
        expected_bones=dict(zip((b['name'] for b in sb),poses(sb,tracks,metadata['frames'])[-1]))
        resource_sockets={properties(sd['Exports'][i-1])['SocketName'].lower():properties(sd['Exports'][i-1]) for i in properties(sx)['Sockets']}
        buffer=rack['published_component_bones'];raw=bytes.fromhex(buffer['raw'])
        if buffer['count']!=len(model['bones']) or len(raw)!=buffer['count']*48:
            raise ValueError('已发布骨骼数组不能与真实mesh参考骨架双射')
        transforms=[struct.unpack_from('<12f',raw,i*48) for i in range(buffer['count'])]
        by_bone={b['name']:transforms[i] for i,b in enumerate(model['bones'])}
        defaults=capture['socket_class_defaults']
        if len(defaults)!=1 or defaults[0]['identity']['name']!='Default__SkeletalMeshSocket':
            raise ValueError('没有唯一实际Socket CDO默认值')
        default_scale=sampled_vector(defaults[0]['properties'],'RelativeScale')
        sockets={s['properties']['SocketName']['name'].lower():s for s in rack['skeleton_sockets']}
        if len(sockets)!=len(rack['skeleton_sockets']) or set(sockets)!=set(group['sockets']):
            raise ValueError('实际原生Socket集合与注册不双射')
        for e in endpoint['sockets']:
            props=sockets[e['socket']]['properties']
            if props['SocketName'].get('number',0) or props['BoneName'].get('number',0) or props['BoneName']['name']!=e['bone']:
                raise ValueError('实际Socket/Bone FName身份不符')
            location=sampled_vector(props,'RelativeLocation');angles=sampled_vector(props,'RelativeRotation')
            scale=sampled_vector(props,'RelativeScale');bone=by_bone[e['bone']]
            if not all(math.isfinite(v) for v in bone) or any(v<=0 for v in bone[8:11]):
                raise ValueError('已发布骨骼变换不适合端点组合')
            q=bone[:4];position=bone[4:7];bone_scale=bone[8:11]
            offset=rotate(q,tuple(v*s for v,s in zip(location,bone_scale)))
            observed_position=[a+b for a,b in zip(position,offset)]
            observed_rotation=list(multiply(q,local_rotation(dict(zip(('Pitch','Yaw','Roll'),angles)))))
            observed_scale=[a*b for a,b in zip(scale,bone_scale)]
            if sum(a*b for a,b in zip(observed_rotation,e['rotation_xyzw']))<0:
                observed_rotation=[-v for v in observed_rotation]
            rp,rq=expected_bones[e['bone']]
            if sum(a*b for a,b in zip(rq,q))<0:rq=tuple(-v for v in rq)
            bone_exact=f32(rp)==f32(position) and f32(rq)==f32(q) and bone_scale==(1.0,1.0,1.0)
            rs=resource_sockets[e['socket']]
            local_exact=f32(location)==f32(vector(rs['RelativeLocation'])) and \
                        f32(local_rotation(dict(zip(('Pitch','Yaw','Roll'),angles))))==f32(local_rotation(rs['RelativeRotation']))
            expected_socket_scale=f32(e['scale']) if e['socket_scale_serialized'] else default_scale
            scale_exact=scale==expected_socket_scale
            exact=f32(observed_position)==f32(e['translation']) and f32(observed_rotation)==f32(e['rotation_xyzw']) and bone_exact and local_exact and scale_exact
            e.update(scale=list(expected_socket_scale),runtime_units_verified=exact,complete_runtime_transform=exact,
                     scale_source='实际Socket属性及Default__SkeletalMeshSocket采样；不是省略字段填默认1',
                     coordinate_space='原生组件空间；与资源第100键的float32分量逐项对照，不转世界空间',
                     runtime_observation={'source_sha256':digest(path),'published_pose_frame':rack['pose_frame'],
                         'translation':observed_position,'rotation_xyzw':observed_rotation,'scale':observed_scale,
                         'native_socket_scale':list(scale),'native_socket_cdo_default_scale':list(default_scale),
                         'scale_equals_cdo':scale==default_scale,
                         'source_runtime_f32_components_equal':exact,
                         'published_bone_matches_resource_key100_f32':bone_exact,
                         'published_bone_component':{'translation':list(position),'rotation_xyzw':list(q),'scale':list(bone_scale)},
                         'socket_local_properties_match_resource':local_exact,'socket_scale_matches_resource_or_actual_cdo':scale_exact,
                         'socket_local_properties':{'translation':list(location),'rotation_pyr':list(angles),'scale':list(scale)},
                         'translation_absolute_difference':[abs(a-b) for a,b in zip(observed_position,e['translation'])],
                         'bone_plus_socket_composition':True,'GetSocketTransform_was_not_called':True,
                         'same_frame_runtime_scheduling_certified':False})
        endpoint['units']='resource_to_observed_RTS_Component_1_to_1' if all(e['runtime_units_verified'] for e in endpoint['sockets']) else 'not_verified'
        endpoint['missing']=['缺有净空依据的距离/角度/缩放容差；本次float32对照一致不是允许误差预算',
                             '两次单挂暂停采样不证明多挂所需隐藏provider在当前帧更新的运行调度']
        if not all(e['runtime_units_verified'] for e in endpoint['sockets']):
            endpoint['missing'].append('至少一处发布骨骼/局部Socket/单位分量与资源不一致，不能认证单位映射')
        endpoint['native_observation']={'record':path.name,'sha256':digest(path),'state_source_sha256':capture['source_sha256'],
             'native_configuration':config,'parts_request':parts['request'],'parts_phase':parts['phase'],
             'published_pose_frame':rack['pose_frame'],'only_active_provider_used':True,
             'inactive_provider_caches_not_used':True}
    for plan in plane['plans'][1:]:
        if plan['gate']:
            plan['gate']['missing']='；'.join(sorted({m for name in plan['gate']['groups'] for m in plane['endpoints'][name]['missing']}))


def end_poses(assets,group):
    """只导出资源证明的分量；缺省Socket缩放与运行单位/容差不填经验值。"""
    mesh=read_skeletal_mesh(assets.load(group['rack_mesh']),group['rack_mesh'])
    skeleton=group['socket_asset']
    document=assets.load(skeleton)
    export=next(e for e in document['Exports'] if e['ObjectName']==skeleton)
    bones=reference_bones(document,export)
    driver=group['animation']['driver_nodes']
    if len(driver)!=1:raise ValueError('端点需要唯一实际动画序列绑定')
    sequence=driver[0]['sequence']
    tracks,metadata=animation_tracks(assets.load(sequence),bones)
    if metadata['frames']!=101 or set(tracks)!=set(range(len(bones))):
        raise ValueError('Su47端点须完整101键且不能用缺轨占位')
    sample=poses(bones,tracks,metadata['frames'])[-1]
    names={b['name']:i for i,b in enumerate(bones)}
    for bone in mesh['bones']:
        index=names[bone['name']]
        a=mesh['bones'][bone['parent']]['name'] if bone['parent']>=0 else None
        b=bones[bones[index]['parent']]['name'] if bones[index]['parent']>=0 else None
        if a!=b:raise ValueError('端点mesh/Skeleton父骨名称不一致')
    result=[]
    for socket in group['sockets']:
        position,quaternion=sample[names[socket['bone']]]
        offset=vector(socket['serialized_location'])
        translation=[float(a+b) for a,b in zip(position,rotate(quaternion,offset))]
        localq=local_rotation(socket['serialized_rotation'])
        rotation=[float(v) for v in multiply(quaternion,localq)] if localq else None
        if rotation and abs(sum(v*v for v in rotation)-1)>.001:raise ValueError('端点四元数未归一')
        # 骨参考缩放为单位且动画不含缩放轨，不能替代省略的Socket.RelativeScale证明。
        scale=vector(socket['serialized_scale']) if socket['serialized_scale'] is not None else None
        result.append({'socket':socket['name'].lower(),'bone':socket['bone'],'key':100,
                       'translation':translation,'rotation_xyzw':rotation,'position':translation,'rotation':rotation,'scale':scale,
                       'coordinate_space':'资源Skeleton组件空间，尚未证明与运行组件单位/缩放一致',
                       'runtime_units_verified':False,'complete_runtime_transform':False,
                       'bone_reference_scale_verified_unit':True,
                       'socket_scale_serialized':socket['serialized_scale'] is not None})
    return {'group':group['group'],'skeleton':skeleton,'sequence':sequence,'frames':101,
            'sockets':result,'units':'serialized_resource_transform_units',
            'tolerance':{'distance':None,'angle_radians':None,'scale_difference':None},
            'runtime_gate_ready':False,
            'missing':['Socket.RelativeScale未序列化，缺该机实际原生值或可核对CDO默认依据',
                       '缺资源空间到运行组件空间的单位/缩放实证',
                       '现有接触面/采样键分析没有给出三类姿态容差的可靠净空下界'],
            'sources':deepcopy(assets.used)}


def build(overlay,batch,assets):
    candidates={a['code']:a for a in overlay['aircraft']}
    samples={a['code']:a for a in batch['aircraft']}
    planes=[]
    for code in CODES:
        candidate=candidates[code];sample=samples[code]
        if not candidate['layout_candidate_complete_seven']:raise ValueError('整机布局未获评审')
        combinations=candidate['combinations']
        if len(combinations)!=7:raise ValueError('整机七组合缺失')
        singles={c['weapons'][0]:c for c in combinations if len(c['weapons'])==1}
        labels={weapon:group_index(c['assignments'][0]['group']) for weapon,c in singles.items()}
        if set(labels.values())!={0,1,2}:raise ValueError('三武器owner必须与三原生配置双射')
        pawn='AcePlayerPawn_'+code
        assets.load(pawn)
        pawn_path=asset_object_path(assets,pawn,pawn+'_C')
        body=component_binding(assets,pawn,'PlaneBodyMesh','SkeletalMesh')
        if not body['animclass']:raise ValueError('Body缺实际AnimClass')
        body.update(skeletal_binding(assets,body['mesh'],body['animclass']))
        body['runtime_component_property']='PlaneBodyMesh'
        groups=[]
        for i,g in enumerate(sample['groups']):
            if group_index(g['group'])!=i:raise ValueError('原生组顺序改变')
            meshes={a['weapon_mesh'] for c in combinations if len(c['weapons'])>1 for a in c['assignments'] if group_index(a['owner_group'])==i}
            native={r['mesh'] for r in g['weapon_mesh_references'] if r.get('mesh') and r['mesh']!='Static Mesh'}
            if len(meshes)!=1 or not meshes<=native:raise ValueError('必须使用每类武器自己的唯一原生弹体')
            sockets=[s['name'].lower() for s in g['sockets']]
            if len(sockets)!=len(set(sockets)):raise ValueError('原组Socket不能重名')
            skeletal=g['component_class']=='SkeletalMeshComponent'
            animation=g['animation']['anim_blueprint'] if skeletal else None
            groups.append({'class':g['config']['WeaponClass'],'rack':g['rack_mesh'],'mesh':next(iter(meshes)),
                           'skeleton':g['socket_asset'] if skeletal else None,
                           'animclass':animation+'_C' if animation and not animation.endswith('_C') else animation,
                           'componentkind':'skeletal' if skeletal else 'static','component_class':g['component_class'],
                           'sockets':sockets,'window':g['weapon']['defaults'].get('MaxLoadedCount',{}).get('value'),
                           'runtime_logical_count':None,'runtime_pool_count':None,
                           'pool_binding':'按实际AttachSocketName与此完整原生Socket集合建立双射，不按数组下标或数量猜对应'})
            binding=component_binding(assets,pawn,g['group'],'SkeletalMesh' if skeletal else 'StaticMesh')
            if binding['mesh']!=g['rack_mesh'] or binding['animclass']!=groups[-1]['animclass']:
                raise ValueError('批次挂架身份与实际Pawn引用不一致')
            groups[-1].update(class_path=asset_object_path(assets,g['config']['WeaponClass'].removesuffix('_C'),g['config']['WeaponClass']),
                              rack_path=binding['mesh_path'],mesh_path=asset_object_path(assets,next(iter(meshes))),
                              animclass_path=binding['animclass_path'],component_template=binding['component_template'],
                              component_class_path=binding['component_class_path'])
            if skeletal:
                registered=skeletal_binding(assets,g['rack_mesh'],binding['animclass'])
                if registered['skeleton']!=g['socket_asset']:raise ValueError('批次Skeleton与真实网格引用不一致')
                groups[-1].update(registered)
            else:groups[-1]['skeleton_path']=None
            groups[-1]['packages']={k:groups[-1].get(k+'_path').rsplit('.',1)[0] if groups[-1].get(k+'_path') else None
                                    for k in ('class','rack','mesh','skeleton','animclass')}
        endpoints={}
        if code=='su47':
            needed={g for c in combinations if len(c['weapons'])>1 for g in c['required_animation_groups']}
            endpoints={name:end_poses(assets,sample['groups'][group_index(name)]) for name in sorted(needed)}
        plans=[None]*8
        for combo in combinations:
            mask=sum(1<<labels[w] for w in combo['weapons'])
            if plans[mask] is not None:raise ValueError('重复组合')
            selected=[[],[],[]];providers=[-1,-1,-1];used=set()
            for a in combo['assignments']:
                owner=group_index(a.get('owner_group',a['group']))
                provider=group_index(a.get('provider_group',a['group']))
                if owner!=labels[a['weapon']]:raise ValueError('兵装owner与原生配置不符')
                sockets=[s.lower() for s in a['sockets']]
                if len(sockets)!=len(set(sockets)):raise ValueError('同一兵装不能重复选物理点')
                # 保留方案有序列表；selected的索引属于provider原生Socket表，绝非运行池下标。
                selected[owner]=[groups[provider]['sockets'].index(s) for s in sockets]
                providers[owner]=provider
                for socket in sockets:
                    if (provider,socket) in used:raise ValueError('两类武器占用同一物理Socket')
                    used.add((provider,socket))
            native=len(combo['weapons'])==1
            rendered=[a['group'] for a in combo['assignments']] if native else combo['rendered_rack_groups']
            racks=sum(1<<group_index(g) for g in rendered)
            required=mask if native else sum(1<<group_index(g) for g in combo['required_animation_groups'])
            required&=sum(1<<i for i,g in enumerate(groups) if g['componentkind']=='skeletal')
            plan={'native':native,'selected':selected,'providers':providers,'racks':racks,'required':required,
                  'selected_index_space':'provider组的资源Socket有序表；运行时先按名字双射到真实组件池',
                  'gate':None}
            if code=='su47' and not native:
                original=deepcopy(combo['internal_ammo_visibility'])
                original.pop('offline_sample_review',None)
                plan['gate']={'mode':'native_fully_open_and_current_end_pose_only','require_native_fully_open':True,
                              'require_current_provider_end_pose':True,'runtime_gate_ready':False,
                              'groups':[g for g in combo['required_animation_groups']],
                              'original_policy':original,
                              'historical_numeric_state_literal_is_not_runtime_authority':True,
                              'native_state_values':'由已核对当前Steam原生Host判断全开；不把旧研究文字的request=2硬编码为运行判据',
                              'missing':'详见plane.endpoints各provider的scale/units/tolerance缺项；缺失即禁止可视接管'}
            plans[mask]=plan
        if any(p is None for p in plans[1:]):raise ValueError('必须同时生成全部七组合')
        body['packages']={k:body[k+'_path'].rsplit('.',1)[0] for k in ('mesh','skeleton','animclass')}
        planes.append({'code':code,'pawn_path':pawn_path,'body':deepcopy(body),'body_driver':body,
                       'groups':groups,'plans':plans,'endpoints':endpoints,'runtime_enabled':False})
    return {'schema':1,'purpose':'三机21组合产品接入数据候选；不自动注册或授权启用','planes':planes,
            'numbering':'groups/providers使用原生SWP1/2/3的零基索引；plans[mask]使用原生配置位，不是用户逻辑SP槽顺序',
            'logical_count_policy':'从每类武器原生capture动态取得队列Num/窗口/游标及持久mountOrdinal；从不把Socket数写为队列长度',
            'pool_policy':'实际持久组件池按owner、类、网格、父链和Socket完整集合逐项双射；provider取位与弹体显示/真实离架共用映射',
            'compiled':False,'tests_run':False,'installed':False,'game_process_access':False}


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--overlay',type=Path,default=ROOT/'evidence/mount-expansion-animated.json')
    parser.add_argument('--batch',type=Path,default=ROOT/'evidence/mount-expansion-batch3.json')
    parser.add_argument('--manifest',type=Path,action='append',required=True)
    parser.add_argument('--native-state',type=Path,help='只读已有F22A单8AAM采样，仅作为特定场景观察，不填组的固定长度')
    parser.add_argument('--endpoint-state',type=Path,action='append',default=[],help='主管已有Su47单UGB/QAAM全开端点文件，可重复；不访问进程')
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    if args.output.exists():raise ValueError('输出必须是新路径')
    assets=Assets(args.manifest)
    result=build(json.loads(args.overlay.read_text(encoding='utf-8')),json.loads(args.batch.read_text(encoding='utf-8')),assets)
    if args.endpoint_state:supplement_endpoints(result,assets,args.endpoint_state)
    result['sources']={'overlay_sha256':digest(args.overlay),'batch_sha256':digest(args.batch),'generator_sha256':digest(Path(__file__)),
                       'manifests':[{'record':p.name,'sha256':digest(p)} for p in args.manifest]}
    result['sources']['actual_resource_bindings']=deepcopy(assets.used)
    if args.native_state:
        captured=json.loads(args.native_state.read_text(encoding='utf-8'))
        sp=captured['groups']['SP']
        plane_class=captured['plane'].get('class_name','')
        if captured['process_write'] or captured['game_functions_called'] or sp['class']['name']!='plwp_8aam_a0_C' or \
           'f22a' not in plane_class.casefold():
            raise ValueError('原生观察必须是只读且真实8AAM类')
        result['native_observation']={'sha256':digest(args.native_state),'record':args.native_state.name,
                    'scope':'F22A单8AAM任务暂停态，不能扩大为另两类逻辑队列或跨场景证据',
                    'class':sp['class']['name'],'logical_count':sp['count'],'window':sp['window'],
                    'pool_sizes':{k:len(captured['mesh_arrays'][k]) for k in ('SWP1','SWP2','SWP3')},
                    'body_property_layout':[r for r in captured['plane_field_layout'] if r['name']=='PlaneBodyMesh']}
    encoded=json.dumps(result,ensure_ascii=False,separators=(',',':'),allow_nan=False)
    if ')AC7ANIM"' in encoded:raise ValueError('原始字符串分隔符冲突')
    header='// 由build_animated_runtime_data.py生成；需要宿主已有Json类型，不自动注册机体。\n#pragma once\n'
    header+='inline const Json& animatedMountData() { static const Json data=Json::parse(R"AC7ANIM('+encoded+')AC7ANIM"); return data; }\n'
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_bytes(header.encode('utf-8'))
    missing=sorted({m for plane in result['planes'] for endpoint in plane['endpoints'].values() for m in endpoint['missing']})
    print(json.dumps({'planes':list(CODES),'cases':21,'su47_runtime_gate_ready':False,
                      'missing':missing,'output':str(args.output)},ensure_ascii=False))


if __name__=='__main__':main()
