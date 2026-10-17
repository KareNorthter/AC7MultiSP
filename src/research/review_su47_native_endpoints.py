"""用已保存的Su-47发布姿态重算全开静态几何；不连接游戏、不编译或运行测试。"""
import argparse
import base64
from copy import deepcopy
import hashlib
import itertools
import json
from pathlib import Path
import struct

import numpy as np

from analyze_sp_switch_animations import Assets
from build_animated_runtime_data import asset_object_path, digest, exact_f32, exact_pose_contract, local_rotation, sampled_vector
from read_cooked_skeletal_mount import read_skeletal_mesh, skin_points
from review_animated_sukhoi import triangle_contacts
from split_static_mount_asset import parse_mesh
from survey_aircraft_mounts import multiply, rotate


def read_capture(path):
    data=json.loads(path.read_text(encoding='utf-8'))
    if data['process_write'] or data['game_functions_called'] or data['plane']['name']!='AcePlayerPawn_su47_C':
        raise ValueError('只接受现有Su47只读磁盘证据')
    if (data['parts']['request'],data['parts']['phase'],data['parts']['progress'][0])!=(1,2,1.0):
        raise ValueError('不是已核原生全开状态')
    return data


def transform(raw):
    v=struct.unpack('<12f',bytes.fromhex(raw))
    if not all(np.isfinite(v)) or v[8:11]!=(1.,1.,1.):
        raise ValueError('当前窄分析只支持已采单位缩放变换')
    return {'rotation':v[:4],'position':v[4:7],'scale':v[8:11]}


def identity_path(item):
    return item['outer_name']+'.'+item['name']


def captured_pose(assets,component,mesh,skeleton,sequence,source):
    if identity_path(component['mesh'])!=asset_object_path(assets,mesh) or identity_path(component['skeleton'])!=asset_object_path(assets,skeleton):
        raise ValueError('实际mesh/Skeleton不符合明确注册')
    model=read_skeletal_mesh(assets.load(mesh),mesh)
    expected=exact_pose_contract(assets,mesh,skeleton,sequence)
    buffer=component['published_component_bones'];raw=bytes.fromhex(buffer['raw'])
    if buffer['count']!=len(model['bones']) or len(raw)!=48*len(model['bones']):
        raise ValueError('发布数组和实际mesh参考骨架不双射')
    pose={}
    for entry in expected['bones']:
        v=transform(raw[entry['mesh_bone_index']*48:(entry['mesh_bone_index']+1)*48].hex())
        pose[entry['name']]=(v['position'],v['rotation'])
        entry['component']=exact_f32([*v['rotation'],*v['position'],*v['scale']])
    expected.update(reference_kind='native_observed_pending_geometry_review',source_sha256=source,
                    observed_pose_frame=component['pose_frame'],runtime_gate_ready=False,key=None,
                    mapping_source='实际mesh/Skeleton唯一骨名及父链；资源第100键只提供对照，不作为本次有效分量来源',
                    boundary='有效分量全部来自指定实采发布数组；几何结论仅对该全骨姿态及经核对的相对父链成立，尚无运行准入')
    return model,pose,expected


def relative_contract(component,parent):
    props=component['relative_properties']
    values={name:sampled_vector(props,name) for name in ('RelativeLocation','RelativeRotation','RelativeScale3D')}
    if values['RelativeLocation']!=(0.,0.,0.) or values['RelativeRotation']!=(0.,0.,0.) or values['RelativeScale3D']!=(1.,1.,1.):
        raise ValueError('实际相对变换不是本次所需共同原点单位变换')
    if struct.unpack('<Q',bytes.fromhex(props['AttachParent']['raw']))[0]!=int(parent['identity']['address'],16):
        raise ValueError('实际组件父链不符')
    if bytes.fromhex(props['AttachSocketName']['raw'])!=bytes(8):
        raise ValueError('存在尚未核对的附着Socket')
    return {'parent':parent['identity']['name'],'attach_socket':'None',
            'local_position_rotation_scale':exact_f32([*values['RelativeLocation'],*values['RelativeRotation'],*values['RelativeScale3D']]),
            'required_identity_local_transform':True}


def socket_contract(component,model,pose,defaults):
    if component['mesh_sockets']:raise ValueError('不接受未知mesh级Socket覆盖')
    default_scale=sampled_vector(defaults[0]['properties'],'RelativeScale')
    result={}
    for item in component['skeleton_sockets']:
        props=item['properties'];name=props['SocketName']['name'].lower();bone=props['BoneName']['name']
        if name in result or props['SocketName']['number'] or props['BoneName']['number']:
            raise ValueError('Socket名字重复或带编号')
        indices=[i for i,b in enumerate(model['bones']) if b['name']==bone]
        if len(indices)!=1:raise ValueError('Socket依附骨不唯一')
        location=sampled_vector(props,'RelativeLocation');angles=sampled_vector(props,'RelativeRotation')
        scale=sampled_vector(props,'RelativeScale')
        if scale!=(1.,1.,1.) or default_scale!=(1.,1.,1.):raise ValueError('Socket/CDO实际缩放不是单位')
        p,q=pose[bone]
        localq=local_rotation(dict(zip(('Pitch','Yaw','Roll'),angles)))
        position=tuple(a+b for a,b in zip(p,rotate(q,location)))
        rotation=multiply(q,localq)
        result[name]={'socket':name,'bone':bone,'mesh_bone_index':indices[0],
                      'local':exact_f32([*location,*angles,*scale]),
                      'local_order':['px','py','pz','pitch_degrees','yaw_degrees','roll_degrees','sx','sy','sz'],
                      'position':list(position),'rotation':list(rotation),'scale':list(scale),
                      'cdo_scale':exact_f32(default_scale)}
    return result


def skinned(model,pose):
    return [(np.asarray(skin_points(model,lod,pose)),np.asarray(lod['indices']).reshape(-1,3)) for lod in model['lods']]


def static_mesh(assets,name):
    document=assets.load(name);export=next(e for e in document['Exports'] if e['ObjectName']==name)
    model=parse_mesh(base64.b64decode(export['Extras']))
    return [(np.asarray(lod['points']),np.asarray(lod['buffers'][0]['values']).reshape(-1,3)) for lod in model['lods']]


def projectile(lods,socket):
    q=socket['rotation'];p=np.array(socket['position'])
    matrix=np.array([rotate(q,v) for v in np.eye(3)]).T
    return [(points@matrix.T+p,indices) for points,indices in lods]


def check(first,second):
    rows=[]
    for i,(p,ix) in enumerate(first):
        for j,(q,jx) in enumerate(second):
            count,envelope,ids=triangle_contacts(p[ix],q[jx],True)
            ids=sorted(set(ids))
            rows.append({'first_lod':i,'second_lod':j,'contact_pairs':count,'first_face_count':len(ids),
                         'first_face_ids':ids,'contact_envelope':envelope})
    return rows


def geometry_summary(lods):
    return [{'lod':i,'vertices':len(p),'triangles':len(ix),
             'positions_sha256':hashlib.sha256(p.astype('<f8').tobytes()).hexdigest(),
             'indices_sha256':hashlib.sha256(ix.astype('<u4').tobytes()).hexdigest()}
            for i,(p,ix) in enumerate(lods)]


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--manifest',action='append',type=Path,required=True)
    parser.add_argument('--overlay',type=Path,required=True)
    parser.add_argument('--full-state',type=Path,required=True)
    parser.add_argument('--ugb-state',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    if args.output.exists():raise ValueError('输出必须是新路径')
    full=read_capture(args.full_state);ugb=read_capture(args.ugb_state)
    if not full['published_pose_stable_before_after'] or full['parts']['config']!=2 or ugb['parts']['config']!=1:
        raise ValueError('来源不是稳定QAAM全开与旧UGB有效端点')
    assets=Assets(args.manifest)
    overlay=json.loads(args.overlay.read_text(encoding='utf-8'))
    aircraft=next(a for a in overlay['aircraft'] if a['code']=='su47')
    current={r['identity']['name']:r for r in full['racks']}
    active_ugb=next(r for r in ugb['racks'] if r['identity']['name']=='SWP2')
    if active_ugb['animation_flags'] or current['SWP3']['animation_flags'] or full['body']['animation_flags']:
        raise ValueError('本次有效源不得是停止更新的缓存')
    if full['body']['pose_frame']!=current['SWP3']['pose_frame']:
        raise ValueError('Body与当前SWP3发布帧不同')
    common=full['root']['component_to_world_raw']
    if any(c['component_to_world_raw']!=common for c in (full['body'],full['weapon_base'],current['SWP3'])):
        raise ValueError('当前Body/WeaponMeshBase/呈现SWP3不是共同世界变换')
    transform(common)
    relatives={'body':relative_contract(full['body'],full['root']),
               'weapon_base':relative_contract(full['weapon_base'],full['root'])}
    for name in ('SWP1','SWP2','SWP3'):relatives[name]=relative_contract(current[name],full['weapon_base'])
    body_model,body_pose,body_expected=captured_pose(assets,full['body'],'su47','su47_Skeleton','su47_Anim',digest(args.full_state))
    if identity_path(full['body']['animclass'])!=asset_object_path(assets,'su47_AnimBP','su47_AnimBP_C'):
        raise ValueError('Body实际AnimClass完整资源身份不同')
    body_geometry=skinned(body_model,body_pose)
    providers={};rack_geometry={};socket_maps={};models={}
    for name,component,source,defaults in [('SWP2',active_ugb,digest(args.ugb_state),ugb['socket_class_defaults']),
                                           ('SWP3',current['SWP3'],digest(args.full_state),full['socket_class_defaults'])]:
        mesh='su47_'+name
        model,pose,expected=captured_pose(assets,component,mesh,mesh+'_Skeleton',mesh+'_Anim',source)
        providers[name]=expected;models[name]=model;rack_geometry[name]=skinned(model,pose)
        socket_maps[name]=socket_contract(component,model,pose,defaults)
    weapons={'SAAM':'w_saam_r0_SM','UGB':'w_ugb_r0_SM','QAAM':'w_qaam_r0_SM'}
    rack_equivalence={'first':'SWP2','second':'SWP3','lods':[]}
    for i,((p,ix),(q,jx)) in enumerate(zip(rack_geometry['SWP2'],rack_geometry['SWP3'])):
        rack_equivalence['lods'].append({'lod':i,'positions_equal':bool(np.array_equal(p,q)),
            'indices_equal':bool(np.array_equal(ix,jx)),
            'position_sha256':hashlib.sha256(p.astype('<f8').tobytes()).hexdigest(),
            'indices_sha256':hashlib.sha256(ix.astype('<u4').tobytes()).hexdigest()})
    equivalent=len(rack_geometry['SWP2'])==len(rack_geometry['SWP3']) and all(
        x['positions_equal'] and x['indices_equal'] for x in rack_equivalence['lods'])
    rack_equivalence['complete_geometry_equal']=equivalent
    weapon_lods={name:static_mesh(assets,mesh) for name,mesh in weapons.items()}
    projectile_cache={};check_cache={}
    def ammo(weapon,provider,socket):
        key=(weapon,provider,socket)
        if key not in projectile_cache:projectile_cache[key]=projectile(weapon_lods[weapon],socket_maps[provider][socket])
        return projectile_cache[key]
    def surfaces(first_name,first,second_name,second):
        key=(first_name,second_name)
        if key not in check_cache:check_cache[key]=check(first,second)
        return deepcopy(check_cache[key])
    # 原生UGB/QAAM布局在同一实采Body姿态重放；不是声称两次单挂同时更新。
    baselines=[]
    for weapon,provider in [('UGB','SWP2'),('QAAM','SWP3')]:
        for socket in socket_maps[provider]:
            key=weapon+':'+provider+':'+socket
            baselines.append({'weapon':weapon,'provider':provider,'socket':socket,
                              'body':surfaces('body',body_geometry,key,ammo(weapon,provider,socket)),
                              'rack':surfaces(provider,rack_geometry[provider],key,ammo(weapon,provider,socket)),
                              'scope':'原生布局在明确实采姿态组成的离线场景；UGB与当前Body不是同次采样'})
    combinations=[]
    for combo in aircraft['combinations']:
        if len(combo['weapons'])==1:continue
        rendered=combo['rendered_rack_groups']
        row={'weapons':combo['weapons'],'rendered':rendered,'body_ammo':[],'rack_ammo':[],
             'ammo_ammo':[],'rack_body':[],'new_contact_or_uncertified':False}
        instances=[]
        for assignment in combo['assignments']:
            weapon=assignment['weapon'];provider=assignment['provider_group']
            for socket in assignment['sockets']:
                key=weapon+':'+provider+':'+socket;geometry=ammo(weapon,provider,socket)
                instances.append((key,geometry))
                hit=surfaces('body',body_geometry,key,geometry)
                # 新借位SAAM没有同位置原生基线，必须直接无接触，不能用较小总数放行。
                row['body_ammo'].append({'ammo':key,'checks':hit})
                if any(x['contact_pairs'] for x in hit):
                    native=next((b for b in baselines if (b['weapon'],b['provider'],b['socket'])==(weapon,provider,socket)),None)
                    if native is None or hit!=native['body']:row['new_contact_or_uncertified']=True
                for rack in rendered:
                    hit=surfaces(rack,rack_geometry[rack],key,geometry)
                    row['rack_ammo'].append({'rack':rack,'ammo':key,'checks':hit})
                    native=next((b for b in baselines if (b['weapon'],b['provider'],b['socket'])==(weapon,rack,socket)),None)
                    # 只有全部顶点与逐面索引确实相同，才允许同形呈现架复用provider的同位置基线。
                    if native is None and equivalent and {rack,provider}=={'SWP2','SWP3'}:
                        native=next((b for b in baselines if (b['weapon'],b['provider'],b['socket'])==(weapon,provider,socket)),None)
                        row['rack_ammo'][-1]['native_baseline_via_proven_identical_geometry']=native is not None
                    if any(x['contact_pairs'] for x in hit) and (native is None or hit!=native['rack']):row['new_contact_or_uncertified']=True
        for (ak,ag),(bk,bg) in itertools.combinations(instances,2):
            hit=surfaces(ak,ag,bk,bg)
            row['ammo_ammo'].append({'first':ak,'second':bk,'checks':hit})
            if any(x['contact_pairs'] for x in hit):row['new_contact_or_uncertified']=True
        for rack in rendered:
            hit=surfaces('body',body_geometry,rack,rack_geometry[rack])
            row['rack_body'].append({'rack':rack,'checks':hit,
                                    'scope':'同一呈现架与同一Body实际姿态的原生附着基线；多挂不增加第二套架'})
        row['geometry_passed_at_reference']=not row['new_contact_or_uncertified']
        combinations.append(row)
        print('+'.join(combo['weapons']),'geometry_passed',row['geometry_passed_at_reference'],flush=True)
    all_pass=all(r['geometry_passed_at_reference'] for r in combinations)
    for expected in [body_expected,*providers.values()]:
        expected['reference_kind']='native_observed_geometry_checked' if all_pass else 'native_observed_geometry_rejected'
    result={'schema':1,'code':'su47','purpose':'真实发布全开端点的全LOD离线几何重核对与严格零偏差参照候选',
            'compiled':False,'tests_run':False,'installed':False,'game_process_access':False,'assets_generated':False,
            'geometry_passed_at_reference':all_pass,'runtime_gate_ready':False,'tolerance':None,
            'body':body_expected,'providers':providers,'socket_expectations':socket_maps,'relative_components':relatives,
            'native_baselines':baselines,'combinations':combinations,'rack_equivalence':rack_equivalence,
            'geometry_inventory':{'body':geometry_summary(body_geometry),
                'racks':{name:geometry_summary(mesh) for name,mesh in rack_geometry.items()},
                'weapon_local_meshes':{name:geometry_summary(mesh) for name,mesh in weapon_lods.items()}},
            'observations':{'body_swp3_same_published_pose_frame':full['body']['pose_frame'],
                'native_frame_before':full['native_frame_before'],'native_frame_after':full['native_frame_after'],
                'stable_paused_buffers':True,'same_render_frame_claimed':False,
                'stability_scope':'来源采样工具声明前后发布数据稳定；输入文件没有保存前后双份原始数组，本分析不独立复查该采样声明',
                'SWP2_from_previous_active_UGB_capture':True,'inactive_SWP2_cache_used':False},
            'limits':['只对记录的实采Body全部38骨姿态成立；不覆盖其他飞控/起落架/空速/襟翼姿态。严格门槛须同时匹配全部Body有效分量，否则隐藏。',
                      'SWP2旧有效端点与本次Body/SWP3组成明确离线场景，不声称多挂运行同帧更新已验证。',
                      '组件共同坐标依据当前Body/SWP3/WeaponMeshBase完整世界变换相同与各组实际单位相对父链；旧SWP2世界缓存不搬进当前场景。',
                      'SAAM原生SWP1仍无有效全开现场采样。本次借位SAAM须直接零接触，不以资源或旧停更缓存作为豁免基线。',
                      '三角面触边与相交共同统计，沿用既有SAT数值判定；数值判定阈值不是运行姿态容差。零表面穿入不独立证明封闭体包含、支撑贴合或发射后净空。',
                      '原生UGB/QAAM挂耳接触仅同位置/同Body场景原样保留；跨呈现架另证全部顶点及逐面索引相同，不通过碰面总数比较放行新增接触。',
                      '产品仍须校验资源完整身份、真实池映射、各组件当帧发布与离架重验；本报告不授权启用。'],
            'sources':{'full_state':{'record':args.full_state.name,'sha256':digest(args.full_state)},
                       'ugb_state':{'record':args.ugb_state.name,'sha256':digest(args.ugb_state)},
                       'overlay_sha256':digest(args.overlay),'analyzer_sha256':digest(Path(__file__)),
                       'helpers':{n:digest(Path(__file__).with_name(n)) for n in ['read_cooked_skeletal_mount.py','review_animated_sukhoi.py','build_animated_runtime_data.py']},
                       'assets':deepcopy(assets.used)}}
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_bytes((json.dumps(result,ensure_ascii=False,indent=2,allow_nan=False)+'\n').encode('utf-8'))
    print(json.dumps({'geometry_passed_at_reference':all_pass,'combinations':len(combinations),'output':str(args.output)},ensure_ascii=False))


if __name__=='__main__':main()
