"""整理F-22A/Su-47/Su-57TG七组合接入数据；只生成头文件，不编译或操作游戏。"""
import argparse
from copy import deepcopy
import hashlib
import json
import math
from pathlib import Path

from analyze_sp_switch_animations import Assets, animation_tracks, poses
from read_cooked_skeletal_mount import read_skeletal_mesh
from survey_aircraft_mounts import multiply, reference_bones, rotate, vector


ROOT=Path(__file__).resolve().parents[2]
CODES=('f22a','su47','su57tg')


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def group_index(name):
    if name not in ('SWP1','SWP2','SWP3'):
        raise ValueError('不是三组原生SWP身份：'+str(name))
    return int(name[-1])-1


def local_rotation(raw):
    if raw is None:return None
    while len(raw)==1 and 'Pitch' not in raw:raw=next(iter(raw.values()))
    p,y,r=(math.radians(float(raw[k]))/2 for k in ('Pitch','Yaw','Roll'))
    sp,cp,sy,cy,sr,cr=math.sin(p),math.cos(p),math.sin(y),math.cos(y),math.sin(r),math.cos(r)
    return (cr*sp*sy-sr*cp*cy,-cr*sp*cy-sr*cp*sy,cr*cp*sy-sr*sp*cy,cr*cp*cy+sr*sp*sy)


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
                       'translation':translation,'rotation_xyzw':rotation,'scale':scale,
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
        planes.append({'code':code,'groups':groups,'plans':plans,'endpoints':endpoints,'runtime_enabled':False})
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
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    if args.output.exists():raise ValueError('输出必须是新路径')
    assets=Assets(args.manifest)
    result=build(json.loads(args.overlay.read_text(encoding='utf-8')),json.loads(args.batch.read_text(encoding='utf-8')),assets)
    result['sources']={'overlay_sha256':digest(args.overlay),'batch_sha256':digest(args.batch),'generator_sha256':digest(Path(__file__)),
                       'manifests':{p.name:digest(p) for p in args.manifest}}
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
                    'pool_sizes':{k:len(captured['mesh_arrays'][k]) for k in ('SWP1','SWP2','SWP3')}}
    encoded=json.dumps(result,ensure_ascii=False,separators=(',',':'),allow_nan=False)
    if ')AC7ANIM"' in encoded:raise ValueError('原始字符串分隔符冲突')
    header='// 由build_animated_runtime_data.py生成；需要宿主已有Json类型，不自动注册机体。\n#pragma once\n'
    header+='inline const Json& animatedMountData() { static const Json data=Json::parse(R"AC7ANIM('+encoded+')AC7ANIM"); return data; }\n'
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_bytes(header.encode('utf-8'))
    print(json.dumps({'planes':list(CODES),'cases':21,'su47_runtime_gate_ready':False,
                      'missing':['scale','runtime_units','clearance_based_tolerance'],'output':str(args.output)},ensure_ascii=False))


if __name__=='__main__':main()
