"""苏系三机完整七组合离线复核：原生资源、101帧蒙皮、Socket与三角面见证。

只读取显式清单，不连接游戏、不修改原资源，不编译或运行游戏测试。
--manifest 可重复；--batch 指正式第三批方案；--output 指独占研究JSON。
结果仅为候选，运行时准入必须遵守整机七组合门槛及真实身份/父链校验。
依赖 numpy；资源格式复用公共静态/骨架/动画读取器，不依赖Temp脚本。
"""
import argparse
import base64
import hashlib
import itertools
import json
import math
from pathlib import Path
import sys
import numpy as np
from survey_aircraft_mounts import Survey, reference_bones, multiply, rotate, vector
from summarize_mount_assets import properties, resolve
from build_external_mount_plan import groups, aircraft_context
import analyze_sp_switch_animations as anim
from read_cooked_skeletal_mount import read_skeletal_mesh
from split_static_mount_asset import parse_mesh

geometry_cache={}
pose_cache={}
mesh_cache={}
weapon_cache={}
pair_cache={}
cache={}

class CheckedSurvey(Survey):
    """现有目录/父类解析接口，所有实际读取先经过共同摘要校验。"""
    def __init__(self,checked):
        self.checked=checked
        self.assets={Path(entries[0]['asset']).stem:entries for entries in checked.entries.values()}
        self.used={}
    def load(self,name):
        result=self.checked.load(name)
        self.used[name]=self.checked.used[name.casefold()]
        return result

def geometry(name):
    if name not in geometry_cache:
        doc=assets.load(name)
        ex=next(e for e in doc['Exports'] if e['ObjectName']==name)
        parsed=parse_mesh(base64.b64decode(ex['Extras']))
        points=np.array(parsed['lods'][0]['points'])
        indices=np.array(parsed['lods'][0]['buffers'][0]['values'])
        sections=[]
        for section in parsed['lods'][0]['sections']:
            selected=indices[section['first']:section['first']+section['triangles']*3]
            sections.append({'material':section['material'],'triangles':section['triangles'],
                             'bounds':box(points[np.unique(selected)])})
        triangles=indices.reshape(-1,3)
        tri_y=points[triangles][:,:,1]
        pieces=[]
        for label,keep in [('negative_y',np.all(tri_y<0,axis=1)),
                           ('positive_y',np.all(tri_y>0,axis=1)),
                           ('crosses_midline',~(np.all(tri_y<0,axis=1)|np.all(tri_y>0,axis=1)))]:
            if np.any(keep):pieces.append({'piece':label,'triangles':int(np.sum(keep)),
                                          'bounds':box(points[np.unique(triangles[keep])])})
        info={'asset':name,'lod0_vertices':len(points),'lods':len(parsed['lods']),
              'bounds':box(points),'section_geometry':sections,'lateral_triangle_pieces':pieces,
              'position_sha256':hashlib.sha256(points.astype('<f8').tobytes()).hexdigest(),
              'indices_sha256':hashlib.sha256(indices.astype('<u4').tobytes()).hexdigest(),
              'source':assets.used[name.casefold()]}
        geometry_cache[name]=(points,info)
    return geometry_cache[name]

def quat_matrix(q):
    return np.array([rotate(q, p) for p in np.eye(3)]).T

def rotator(raw):
    while len(raw) == 1:
        raw = next(iter(raw.values()))
    p, y, r = (math.radians(float(raw[k])) / 2 for k in ['Pitch', 'Yaw', 'Roll'])
    sp, cp, sy, cy, sr, cr = math.sin(p), math.cos(p), math.sin(y), math.cos(y), math.sin(r), math.cos(r)
    return (cr*sp*sy-sr*cp*cy, -cr*sp*cy-sr*cp*sy, cr*cp*sy-sr*sp*cy, cr*cp*cy+sr*sp*sy)

def poses(mesh):
    if mesh not in pose_cache:
        doc = assets.load(mesh + '_Skeleton')
        ex = next(e for e in doc['Exports'] if e['ObjectName'].casefold() == (mesh+'_Skeleton').casefold())
        bones = reference_bones(doc, ex)
        driver = anim.graph(assets, mesh + '_AnimBP')
        tracks, summary = anim.animation_tracks(assets.load(driver['sequence']), bones)
        if set(tracks) != set(range(len(bones))):
            raise ValueError('候选计算不允许缺轨占位：' + mesh)
        samples = anim.poses(bones, tracks, summary['frames'])
        pose_cache[mesh] = bones, samples, driver, summary
    return pose_cache[mesh]

def socket_samples(group, socket, allow_native_omitted_rotation=False):
    sock = next(s for s in group['sockets'] if s['name'] == socket)
    location = vector(sock['serialized_location'])
    explicit = sock['serialized_rotation'] is not None
    if not explicit and not allow_native_omitted_rotation:
        raise ValueError('借位不能猜省略的Socket旋转：' + socket)
    # 静态原生单挂只保存原生条件计算；明确记录旋转零仅为计算假设，不放行跨池借用。
    localq = rotator(sock['serialized_rotation']) if explicit else (0, 0, 0, 1)
    if group['component_class'] == 'SkeletalMeshComponent':
        bones, samples, _, _ = poses(group['rack_mesh'])
        index = next(i for i, b in enumerate(bones) if b['name'] == sock['bone'])
        values = [(np.array(p[index][0]) + rotate(p[index][1], location),
                   multiply(p[index][1], localq)) for p in samples]
    else:
        values = [(np.array(location), localq)] * 101
    return values, {'socket': socket, 'bone': sock['bone'], 'bone_chain': sock['bone_chain'],
                    'explicit_rotation': explicit, 'native_omitted_rotation_assumption': not explicit,
                    'serialized_location': sock['serialized_location'],
                    'serialized_rotation': sock['serialized_rotation'],
                    'serialized_scale': sock['serialized_scale'],
                    'frames': len(values),
                    'position_aabb': box(np.array([v[0] for v in values])),
                    'position_frame0': values[0][0].tolist(), 'position_frame100': values[-1][0].tolist(),
                    'parent_channels': sock['ancestor_animation_channels']}

def box(points):
    return {'min': np.min(points, axis=0).round(6).tolist(), 'max': np.max(points, axis=0).round(6).tolist()}

def weapon_at(group, sock, mesh, native=False):
    points, info = geometry(mesh)
    samples, socket = socket_samples(group, sock, native)
    lo, hi = points.min(axis=0), points.max(axis=0)
    center, extent = (lo+hi)/2, (hi-lo)/2
    bodies = []
    frame_bounds = []
    allpoints = []
    for position, q in samples:
        matrix = quat_matrix(q)
        vertices = points @ matrix.T + position
        bodies.append((center @ matrix.T + position, matrix, extent))
        frame_bounds.append(box(vertices))
        allpoints.append(vertices)
    summary = {'mesh': mesh, 'mesh_geometry': info, 'provider_socket': socket,
               'frame_vertex_aabbs': frame_bounds,
               'full_101_frame_vertex_sweep_aabb': box(np.concatenate(allpoints)),
               'full_101_frame_vertex_sweep_sha256': hashlib.sha256(np.stack(allpoints).astype('<f8').tobytes()).hexdigest(),
               'scale_boundary': '原生Socket/组件未序列化缩放须运行时身份检查确认为单位；这里未猜缺省值已核实',
               'collision_shape': '本次网格全部LOD0顶点的局部紧AABB，随Socket完整位置和朝向逐帧转为OBB；不是只采样挂点原点'}
    return bodies, summary

def pair_separation(first, second):
    rows = []
    for frame, (a, b) in enumerate(zip(first, second)):
        ca, ra, ea = a
        cb, rb, eb = b
        axes = list(ra.T) + list(rb.T) + [np.cross(x, y) for x in ra.T for y in rb.T]
        separations = []
        for axis in axes:
            norm = np.linalg.norm(axis)
            if norm < 1e-9:
                continue
            axis = axis / norm
            sep = abs(np.dot(cb-ca, axis)) - np.dot(np.abs(ra.T @ axis), ea) - np.dot(np.abs(rb.T @ axis), eb)
            separations.append(float(sep))
        rows.append(max(separations))
    return {'frames': len(rows), 'all_frames_separated': all(s > 1e-5 for s in rows),
            'minimum_separating_axis_gap': round(min(rows), 6), 'limiting_frame': int(np.argmin(rows)),
            'overlap_frames': [f for f, s in enumerate(rows) if s <= 1e-5],
            'meaning': '正值为该帧存在分离轴的保守间隙；负值是OBB重叠而不是已证明三角面碰撞'}

NATIVE_MESH = {'SAAM':'w_saam_r0_SM', 'UGB':'w_ugb_r0_SM', 'QAAM':'w_qaam_r0_SM',
               '4AAM':'w_4aam_r1_SM', 'HCAA':'w_4aam_r1_SM', 'GPB':'w_gpb_r0_SM',
               'LACM':'w_lacm_r0_closed_SM', 'PLSL':None}
NAMES = {'su47':['SAAM','UGB','QAAM'], 'pkfa':['4AAM','GPB','PLSL'], 'su57tg':['4AAM','HCAA','LACM']}
# 字典按兵装名：提供组件与原生Socket；只换成每种逻辑兵装自己的原生弹体。
PLANS = {
 'su47': {
  ('SAAM','UGB'):{'SAAM':('SWP2',['sp21']),'UGB':('SWP2',['sp24','sp22'])},
  ('SAAM','QAAM'):{'SAAM':('SWP2',['sp21']),'QAAM':('SWP3',['sp34','sp32'])},
  ('UGB','QAAM'):{'UGB':('SWP2',['sp21','sp23']),'QAAM':('SWP3',['sp34','sp32'])},
  ('SAAM','UGB','QAAM'):{'SAAM':('SWP2',['sp21']),'UGB':('SWP2',['sp24']),'QAAM':('SWP3',['sp32'])}},
 'pkfa': {
  ('4AAM','GPB'):{'4AAM':('SWP2',['sp21']),'GPB':('SWP2',['sp22'])},
  ('4AAM','PLSL'):{'4AAM':('SWP1',['sp11','sp12','sp13','sp14']),'PLSL':('SWP3',['sp31','sp32'])},
  ('GPB','PLSL'):{'GPB':('SWP2',['sp21','sp22']),'PLSL':('SWP3',['sp31','sp32'])},
  ('4AAM','GPB','PLSL'):{'4AAM':('SWP2',['sp21']),'GPB':('SWP2',['sp22']),'PLSL':('SWP3',['sp31','sp32'])}},
 'su57tg': {
  ('4AAM','HCAA'):{'4AAM':('SWP3',['sp31','sp32']),'HCAA':('SWP3',['sp33','sp34'])},
  ('4AAM','LACM'):{'4AAM':('SWP3',['sp31','sp32']),'LACM':('SWP3',['sp33','sp34'])},
  ('HCAA','LACM'):{'HCAA':('SWP3',['sp31','sp32']),'LACM':('SWP3',['sp33','sp34'])},
  ('4AAM','HCAA','LACM'):{'4AAM':('SWP3',['sp31','sp32']),'HCAA':('SWP3',['sp33']),'LACM':('SWP3',['sp34'])}}
}

def hash_array(a):
    return hashlib.sha256(a.astype('<f8').tobytes()).hexdigest()

def skin(name):
    if name in mesh_cache:return mesh_cache[name]
    mesh_document=assets.load(name)
    mesh_export=next(e for e in mesh_document['Exports'] if e['ObjectName'].casefold()==name.casefold())
    actual_skeleton=resolve(mesh_document,properties(mesh_export)['Skeleton'])
    if actual_skeleton.casefold()!=(name+'_Skeleton').casefold():
        raise ValueError('实际网格Skeleton引用与待解骨架不符：'+name)
    parsed=read_skeletal_mesh(mesh_document,name)
    bones=parsed['bones'];lod=parsed['lods'][0]
    skeleton_bones,samples,driver,summary=poses(name)
    by_name={b['name']:i for i,b in enumerate(skeleton_bones)}
    mapping=[]
    for bone in bones:
        if bone['name'] not in by_name:raise ValueError('网格骨骼未在动画Skeleton中找到：'+bone['name'])
        index=by_name[bone['name']];sb=skeleton_bones[index]
        mesh_parent=bones[bone['parent']]['name'] if bone['parent']>=0 else None
        skeleton_parent=skeleton_bones[sb['parent']]['name'] if sb['parent']>=0 else None
        if mesh_parent!=skeleton_parent:raise ValueError('网格/动画父骨名称不同：'+bone['name'])
        mapping.append(index)
    reference=anim.poses(bones,{i:{'translation':[b['position']],'rotation':[b['quaternion']]} for i,b in enumerate(bones)},1)[0]
    points=np.array(lod['points']);weighted=[];per_bone={}
    for section in lod['sections']:
        for vi in range(section['start'],section['start']+section['vertices']):
            for bi,w in lod['weights'][vi]:
                if w:
                    per_bone.setdefault(section['bone_map'][bi],[]).append((vi,w/255))
    for sample in samples:
        output=np.zeros_like(points)
        for bi,assignments in per_bone.items():
            idx=np.array([p[0] for p in assignments]);weights=np.array([p[1] for p in assignments])
            rp,rq=reference[bi];cp,cq=sample[mapping[bi]]
            rot=quat_matrix(cq)@quat_matrix(rq).T
            output[idx]+=(points[idx]@rot.T+np.array(cp)-rot@np.array(rp))*weights[:,None]
        weighted.append(output)
    result=np.stack(weighted)
    indices=np.array(lod['indices']).reshape(-1,3)
    info={'asset':name,'bytes_fully_consumed':parsed['complete'],'lods':len(parsed['lods']),
          'lod0_vertices':len(points),'lod0_triangles':len(indices),'bone_count':len(bones),
          'bones_names_parents_match_socket_skeleton':True,
          'mesh_to_animation_skeleton_by_unique_name':mapping,
          'mesh_reference_used_for_inverse_bind':True,
          'frames':len(result),'full_101_frame_skinned_vertex_sweep_aabb':box(result.reshape(-1,3)),
          'full_101_frame_skinned_vertex_sha256':hash_array(result),
          'reference_position_sha256':hash_array(points),
          'indices_sha256':hashlib.sha256(indices.astype('<u4').tobytes()).hexdigest(),
          'weights_sha256':hashlib.sha256(json.dumps(lod['weights'],separators=(',',':')).encode()).hexdigest(),
          'sections':lod['sections'],'materials':parsed['materials'],'driver':driver,
          'bone_tracks':summary,'source':assets.used[name.casefold()]}
    mesh_cache[name]=(result,indices,info,parsed)
    return mesh_cache[name]

def projectile(group,socket,name,native=False):
    key=(group['rack_mesh'],socket,name,native)
    if key in weapon_cache:return weapon_cache[key]
    doc=assets.load(name);ex=next(e for e in doc['Exports'] if e['ObjectName']==name)
    parsed=parse_mesh(base64.b64decode(ex['Extras']));lod=parsed['lods'][0]
    points=np.array(lod['points']);indices=np.array(lod['buffers'][0]['values']).reshape(-1,3)
    positions,_=socket_samples(group,socket,native)
    frames=np.stack([points@quat_matrix(q).T+p for p,q in positions])
    weapon_cache[key]=(frames,indices)
    return frames,indices

def triangle_contacts(first,second,include_first_indices=False):
    """先逐三角AABB筛选，再以面法线/边叉乘等17轴排除；触边也记接触。"""
    def done(count,envelope,ids=()):
        return (count,envelope,[int(i) for i in ids]) if include_first_indices else (count,envelope)
    first_ids=np.arange(len(first))
    # 三角面法线过小的退化面不参与；所有顶点仍参加扫掠Bounds。
    keep=np.linalg.norm(np.cross(first[:,1]-first[:,0],first[:,2]-first[:,0]),axis=1)>1e-10
    first=first[keep];first_ids=first_ids[keep]
    second=second[np.linalg.norm(np.cross(second[:,1]-second[:,0],second[:,2]-second[:,0]),axis=1)>1e-10]
    if not len(first) or not len(second):return done(0,None)
    lo,hi=second.min((0,1)),second.max((0,1))
    keep=np.all((first.min(1)<=hi+1e-6)&(first.max(1)>=lo-1e-6),axis=1)
    first=first[keep];first_ids=first_ids[keep]
    if not len(first):return done(0,None)
    lo,hi=first.min((0,1)),first.max((0,1))
    second=second[np.all((second.min(1)<=hi+1e-6)&(second.max(1)>=lo-1e-6),axis=1)]
    if not len(second):return done(0,None)
    broad=np.all((first.min(1)[:,None,:]<=second.max(1)[None,:,:]+1e-6)&
                 (first.max(1)[:,None,:]>=second.min(1)[None,:,:]-1e-6),axis=2)
    ia,ib=np.nonzero(broad)
    if not len(ia):return done(0,None)
    a=first[ia];b=second[ib]
    ae=np.stack([a[:,1]-a[:,0],a[:,2]-a[:,1],a[:,0]-a[:,2]],axis=1)
    be=np.stack([b[:,1]-b[:,0],b[:,2]-b[:,1],b[:,0]-b[:,2]],axis=1)
    an=np.cross(ae[:,0],ae[:,1]);bn=np.cross(be[:,0],be[:,1])
    axes=[an,bn]
    axes.extend(np.cross(ae[:,i],be[:,j]) for i in range(3) for j in range(3))
    # 共面三角也能分离；任意附加轴只会提供充分分离证据。
    axes.extend(np.cross(ae[:,i],an) for i in range(3))
    axes.extend(np.cross(be[:,i],bn) for i in range(3))
    live=np.ones(len(a),dtype=bool)
    for axis in axes:
        norm=np.linalg.norm(axis,axis=1);axis=axis/np.maximum(norm[:,None],1e-30)
        pa=np.einsum('nvi,ni->nv',a,axis);pb=np.einsum('nvi,ni->nv',b,axis)
        sep=(pa.max(1)<pb.min(1)-1e-6)|(pb.max(1)<pa.min(1)-1e-6)
        live &= ~(sep & (norm>1e-10))
    if not np.any(live):return done(0,None)
    pts=np.concatenate([a[live].reshape(-1,3),b[live].reshape(-1,3)])
    return done(int(np.sum(live)),box(pts),first_ids[ia[live]])

def contacts(rack,group,socket,weapon,native=False):
    key=(rack,group['rack_mesh'],socket,weapon,native)
    if key in pair_cache:return pair_cache[key]
    if group['component_class']=='StaticMeshComponent' and rack==group['rack_mesh']:
        doc=assets.load(rack);ex=next(e for e in doc['Exports'] if e['ObjectName']==rack)
        parsed=parse_mesh(base64.b64decode(ex['Extras']));lod=parsed['lods'][0]
        frames=np.array([lod['points']]*101);indices=np.array(lod['buffers'][0]['values']).reshape(-1,3)
        rack_info={'asset':rack,'static':True,'vertices':len(lod['points']),'triangles':len(indices)}
    else:
        frames,indices,rack_info,_=skin(rack)
    pf,pi=projectile(group,socket,weapon,native)
    counts=[];boxes=[]
    for frame in range(101):
        count,box=triangle_contacts(frames[frame][indices],pf[frame][pi]);counts.append(count)
        if box:boxes.append(box)
    result={'rack_mesh':rack,'provider_group':group['group'],'socket':socket,'weapon_mesh':weapon,'frames':101,
       'surface_contact_or_intersection_pairs_by_frame':counts,
       'frames_with_surface_contact_or_intersection':[i for i,c in enumerate(counts) if c],
       'max_surface_contact_or_intersection_pairs':max(counts),'max_frame':int(np.argmax(counts)),
       'contact_triangle_envelope':{'min':np.min([b['min'] for b in boxes],0).tolist(),'max':np.max([b['max'] for b in boxes],0).tolist()} if boxes else None,
       'interpretation':'三角面相交/触边对计数；挂耳原生接触须与同机原生基线比较，不能直接当碰撞通过/失败',
       'native_omitted_socket_rotation_assumption':native and next(s for s in group['sockets'] if s['name']==socket)['serialized_rotation'] is None}
    pair_cache[key]=result
    return result

def body_contacts(code,group,socket,weapon,native):
    key=(code,group['group'],socket,weapon,native)
    if key in cache:return cache[key]
    frames,indices,_,_=skin(code)
    pf,pi=projectile(group,socket,weapon,native)
    parsed=mesh_cache[code][3];lod=parsed['lods'][0]
    vertex_bones={}
    for section in lod['sections']:
        for vi in range(section['start'],section['start']+section['vertices']):
            vertex_bones[vi]={parsed['bones'][section['bone_map'][bi]]['name'] for bi,w in lod['weights'][vi] if w}
    face_bones=['+'.join(sorted(set.union(*(vertex_bones[int(v)] for v in tri)))) for tri in indices]
    counts=[];boxes=[];bone_contacts={};witness_ids=[]
    for f in range(101):
        count,envelope,ids=triangle_contacts(frames[f][indices],pf[f][pi],True);counts.append(count)
        for index in ids:
            label=face_bones[index]
            bone_contacts.setdefault(label,[0]*101)[f]+=1
        witness_ids.append(ids)
        box=envelope
        if box:boxes.append(box)
    result={'body_mesh':code,'provider_group':group['group'],'socket':socket,'weapon_mesh':weapon,
            'frames':101,'surface_contact_or_intersection_pairs_by_frame':counts,
            'frames_with_surface_contact_or_intersection':[i for i,c in enumerate(counts) if c],
            'max_surface_contact_or_intersection_pairs':max(counts),'max_frame':int(np.argmax(counts)),
            'frame0_pairs':counts[0],'frame100_pairs':counts[-1],
            'body_bone_contact_pairs_by_frame':bone_contacts,
            'body_triangle_witness_ids_first20_by_frame':[v[:20] for v in witness_ids],
            'all_body_triangle_witness_ids_sha256':hashlib.sha256(json.dumps(witness_ids,separators=(',',':')).encode()).hexdigest(),
            'contact_triangle_envelope':{'min':np.min([b['min'] for b in boxes],0).tolist(),'max':np.max([b['max'] for b in boxes],0).tolist()} if boxes else None,
            'native_omitted_socket_rotation_assumption':native and next(s for s in group['sockets'] if s['name']==socket)['serialized_rotation'] is None,
            'phase_boundary':'Body与provider各通道归一化同步采样；未覆盖各通道独立相位及所有飞行姿态，不能称游戏动态验证'}
    cache[key]=result
    print(code,group['group'],socket,weapon,'BODY PAIRS',max(counts),'frame0/100',counts[0],counts[-1],flush=True)
    return result

def build_candidates():
    output = {'schema':1, 'purpose':'保留兵装原生弹体的完整七组合离线深化；本文件不授予运行准入',
              'manifest':str(manifest), 'aircraft':[], 'compiled':False, 'tests_run':False,
              'installed':False, 'game_process_access':False,
              'method':['从当前同摘要资源回读真实Pawn、父类、组件父链和Socket',
                        '原生cooked序列逐轨解码101帧，包含Socket序列化欧拉朝向',
                        '弹体LOD0所有顶点逐帧变换；15轴OBB保守排除弹体间碰撞',
                        '静态Socket省略旋转仅对原生条件场景零旋转占位，不能证明原生默认值',
                        '公共挂架蒙皮扫掠须单独补充，未用骨架原点或总Bounds冒充网格碰撞'],
              'format_reference':'https://github.com/gildor2/UEViewer/blob/master/Unreal/UnrealMesh/UnMesh4.cpp'}
    for code, names in NAMES.items():
        gs = groups(survey, code)
        by_group = {g['group']:g for g in gs}
        by_weapon = dict(zip(names, gs))
        for g in gs:
            for name in g['weapon']['blueprint_chain']:
                assets.load(name)
            assets.load(g['rack_mesh'])
            assets.load(g['socket_asset'])
        assets.load('AcePlayerPawn_'+code)
        assets.load('PartsAnimationConfig_'+code)
        row = {'code':code,'weapons':names,'groups':gs,'native_context':aircraft_context(survey,code),
               'combinations':[], 'complete_seven_layout_candidates':True, 'runtime_ready':False}
        for count in [1,2,3]:
            for combo in itertools.combinations(names,count):
                plan = PLANS[code].get(combo)
                if plan is None:
                    w = combo[0]
                    group = by_weapon[w]
                    plan = {w:(group['group'],[s['name'] for s in group['sockets']])}
                c = {'weapons':list(combo),'status':'deferred','blocker_kind':'animation_control',
                     'candidate_route':'完整原架单一呈现＋原生Socket借位＋逻辑兵装原生弹体',
                     'assignments':[], 'rack_plan':{}, 'body_geometry_sweep':'pending',
                     'required_animation_work':[], 'runtime_prerequisites':[
                         '原生池index不改；池身份/serial/class/mesh/父链全校验，通过逻辑兵装→provider映射取位',
                         '显示、真实发射、冷却归还同一provider组件和Socket；不能只移动陈列模型',
                         '原生窗口、库存和装填决定实际显示，Socket数不作为逻辑槽数或运行池数',
                         '恢复原生弹体和原生附着，保留兵装本来的发射后展开/效果路径',
                         '未序列化缩放/静态朝向须以运行时原生值核对，不能硬编码猜默认'],
                     'primitive_sweeps':[], 'projectile_pair_checks':[]}
                bodies = []
                shown = {}
                for weapon,(provider,sockets) in plan.items():
                    owner = by_weapon[weapon]
                    pg = by_group[provider]
                    mesh = NATIVE_MESH[weapon]
                    native = len(combo)==1 or (code=='pkfa' and combo==('GPB','PLSL'))
                    window = owner['weapon']['defaults']['MaxLoadedCount']['value']
                    assignment = {'weapon':weapon, 'owner_group':owner['group'], 'owner_class':owner['weapon']['class'],
                       'provider_group':provider,'provider_component':provider+'_GEN_VARIABLE',
                       'provider_socket_asset':pg['socket_asset'], 'sockets':sockets,
                       'replacement_mesh':mesh if provider != owner['group'] else None,
                       'native_weapon_mesh':mesh, 'weapon_mesh_identity':'沿逻辑兵装自身蓝图继承链，不伪造武器外形',
                       'resource_station_count':len(sockets),'native_load_window':window,
                       'display_target':('固定双炮体，按效果生命周期显示，不能逐发隐藏' if weapon=='PLSL' else 'min(本站位上限,本逻辑兵装当前已装填数,库存)'),
                       'display_cap':min(len(sockets),window),'below_native_window':len(sockets)<window,
                       'animation_domain':('body.swcc + provider.SWP1.spwc' if code=='pkfa' and provider=='SWP1' else
                          'body.swcc，原生静态架不随spwc' if code=='pkfa' and provider=='SWP2' else
                          '固定炮体/PLSL效果域' if weapon=='PLSL' else 'body.spwc + provider.spwc' if pg['component_class']=='SkeletalMeshComponent' else 'body.spwc（原生腹舱静态dummy载体）'),
                       'same_provider_for_display_and_release':True,
                       'parent_chain_preserved':pg['parent_references'],
                       'provider_native_mesh':pg['rack_mesh']}
                    c['assignments'].append(assignment)
                    shown.setdefault(provider,[]).extend(sockets)
                    if mesh:
                        for socket in sockets:
                            body, info = weapon_at(pg,socket,mesh,native or pg['component_class']=='StaticMeshComponent')
                            key = weapon+':'+provider+':'+socket
                            bodies.append((key,body))
                            c['primitive_sweeps'].append({'key':key,**info})
                for (n1,b1),(n2,b2) in itertools.combinations(bodies,2):
                    c['projectile_pair_checks'].append({'first':n1,'second':n2,**pair_separation(b1,b2)})
                c['all_projectile_obbs_separated_101_frames'] = all(p['all_frames_separated'] for p in c['projectile_pair_checks'])
                c['rack_plan'] = {'visible_whole_native_groups':list(shown),
                      'hidden_whole_unselected_groups':[g['group'] for g in gs if g['group'] not in shown],
                      'empty_sockets_kept_visible':{g:[s['name'] for s in by_group[g]['sockets'] if s['name'] not in sockets] for g,sockets in shown.items()},
                      'split_asset':False,'new_mesh':False,'new_socket':False,'changed_parent':False,
                      'no_duplicate_presenting_racks':True,
                      'boundary':'完整选中架允许有可见空导轨；不通过少显示弹体掩盖重叠的第二套整架'}
                if code=='pkfa':
                    c['required_animation_work']=['4AAM与GPB使用body.swcc公共舱门；SWP1活动架spwc独立路由，跨武器切换不重启动画实例',
                        '固定PLSL保留其原生效果与炮体状态；不能把其两Socket当两枚导弹',
                        'swcc和spwc相位关系需核对；同步101帧不能替代两域所有相位的Body净空']
                else:
                    c['required_animation_work']=['Body与唯一呈现provider继续原生骨架更新，同域开舱请求跨SP连续保留',
                        '未呈现组保留逻辑兵装，但不能冻结提供Socket的骨架或覆盖另一个组件的spwc',
                        '读取当前骨架完成更新后的世界Socket变换；发射位置/朝向不可锁定参考帧']
                c['blockers']=['公共架蒙皮网格与弹体/Body全帧关系尚待补算','运行时多SP动画域路由和provider映射尚未实现/实测']
                row['combinations'].append(c)
        row['whole_aircraft_gate']={'required':'7/7全部完成才进入下轮整机接入清单','ready':False,
                                     'reason':'全部七组都有明确原资源候选，蒙皮扫掠及运行时动画/provider边界仍待处理'}
        output['aircraft'].append(row)
    output['sources'] = assets.used
    return output


def frame_ranges(frames):
    result=[]
    for _,items in itertools.groupby(enumerate(sorted(frames)),lambda p:p[1]-p[0]):
        values=[p[1] for p in items]
        result.append([values[0],values[-1]])
    return result


def native_presentation(weapon):
    """记录逻辑武器自己的展开引用；不把provider旧弹体当作逻辑武器。"""
    result={'class':weapon['class'],'effective_defaults':{},'animation_component_overrides':[]}
    for blueprint in weapon['blueprint_chain']:
        doc=assets.load(blueprint)
        default=next(e for e in doc['Exports'] if e['ObjectName']=='Default__'+blueprint+'_C')
        for key,item in properties(default).items():
            if key in ['AnimationEndStaticMesh','StaticMesh','RootComponent'] and key not in result['effective_defaults']:
                result['effective_defaults'][key]={'value':resolve(doc,item) if item else None,'source':blueprint,'serialized_null':item==0}
        for export in doc['Exports']:
            if 'WeaponAnimationMesh' not in export['ObjectName']:continue
            prop=properties(export);row={'source':blueprint,'object':export['ObjectName']}
            for key in ['SkeletalMesh','AnimationData','RelativeLocation','RelativeRotation','RelativeScale3D']:
                if key not in prop:continue
                item=prop[key]
                if key=='SkeletalMesh':row[key]=resolve(doc,item) if item else None
                elif key=='AnimationData':
                    row[key]={k:(resolve(doc,v) if k=='AnimToPlay' and v else v) for k,v in item.items()}
                else:row[key]=item
            result['animation_component_overrides'].append(row)
    result['restore_contract']='模型借位仅改挂载显示/实际取位；展开组件、AnimationEndStaticMesh与逻辑类保持原生，解除时归还'
    return result


def actual_component_bindings(code,aircraft_groups):
    doc=assets.load('AcePlayerPawn_'+code)
    body=properties(next(e for e in doc['Exports'] if e['ObjectName']=='PlaneBodyMesh'))
    body_mesh=resolve(doc,body['SkeletalMesh'])
    body_anim=resolve(doc,body['AnimClass']).removesuffix('_C')
    if body_mesh.casefold()!=code.casefold() or body_anim.casefold()!=(code+'_AnimBP').casefold():
        raise ValueError('真实Body绑定与当前计算目标不符：'+code)
    result={'body':{'mesh':body_mesh,'anim_blueprint':body_anim,
                    'attach_parent':resolve(doc,body['AttachParent']) if body.get('AttachParent') else None},
            'swp':[]}
    for group in aircraft_groups:
        prop=properties(next(e for e in doc['Exports'] if e['ObjectName']==group['group']+'_GEN_VARIABLE'))
        key='SkeletalMesh' if group['component_class']=='SkeletalMeshComponent' else 'StaticMesh'
        mesh_name=resolve(doc,prop[key])
        if mesh_name!=group['rack_mesh']:raise ValueError('真实SWP网格引用不符')
        row={'group':group['group'],'mesh':mesh_name,'parent_chain':group['parent_references'],
             'serialized_transform_overrides':group['component_transform_overrides']}
        if 'AnimClass' in prop:
            actual_anim=resolve(doc,prop['AnimClass']).removesuffix('_C')
            if actual_anim.casefold()!=(mesh_name+'_AnimBP').casefold():
                raise ValueError('真实SWP AnimClass不符')
            row['anim_blueprint']=actual_anim
        result['swp'].append(row)
    return result


def add_native_comparison(aircraft):
    """先建立同一武器原生基线，再解释借位新增接触及舱内显隐。"""
    code=aircraft['code'];gs={g['group']:g for g in aircraft['groups']}
    aircraft['native_surface_contact_baselines']=[]
    for weapon,group in zip(NAMES[code],aircraft['groups']):
        name=NATIVE_MESH[weapon]
        if not name:continue
        for socket in group['sockets']:
            r=body_contacts(code,group,socket['name'],name,True)
            aircraft['native_surface_contact_baselines'].append({'weapon':weapon,**r})
    for combo in aircraft['combinations']:
        providers={a['provider_group'] for a in combo['assignments']}
        # Su47仅显一套同形四列架；逻辑兵装组选中与provider更新集合分开。
        rendered=({'SWP3'} if 'SWP3' in providers else {'SWP2'}) if code=='su47' and len(combo['weapons'])>1 else providers
        required={g for g in rendered|providers if gs[g]['component_class']=='SkeletalMeshComponent'}
        combo['rendered_rack_groups']=sorted(rendered)
        combo['required_animation_groups']=sorted(required)
        combo['rack_plan'].update(rendered_rack_groups=sorted(rendered),required_animation_groups=sorted(required),
            hidden_whole_unselected_groups=sorted(set(gs)-rendered),
            visible_whole_native_groups=sorted(rendered),
            empty_sockets_kept_visible='按完整呈现架保留；provider别名Socket不等于新增可分离导轨')
        combo['rack_weapon_surface_checks']=[];combo['body_weapon_surface_checks']=[]
        combo['internal_ammo_visibility']=[]
        for assignment in combo['assignments']:
            name=assignment['native_weapon_mesh']
            if not name:continue
            group=gs[assignment['provider_group']]
            native_group=gs[assignment['owner_group']]
            native_sockets={s['name'] for s in native_group['sockets']}
            for socket in assignment['sockets']:
                native_station=assignment['provider_group']==assignment['owner_group'] and socket in native_sockets
                allow_omitted=group['component_class']=='StaticMeshComponent'
                rendered_for_provider=group['rack_mesh']
                if code=='su47' and group['group'] not in rendered:
                    rendered_for_provider=gs[next(iter(rendered))]['rack_mesh']
                rack_check=contacts(rendered_for_provider,group,socket,name,allow_omitted)
                body_check=body_contacts(code,group,socket,name,allow_omitted)
                combo['rack_weapon_surface_checks'].append(rack_check)
                combo['body_weapon_surface_checks'].append(body_check)
                frame_hits=body_check['frames_with_surface_contact_or_intersection']
                native_own=[b for b in aircraft['native_surface_contact_baselines'] if b['weapon']==assignment['weapon']]
                body_check['same_weapon_native_baseline_max_pairs']=max((b['max_surface_contact_or_intersection_pairs'] for b in native_own),default=0)
                body_check['new_provider_station']=not native_station
                safe=[f for f in range(101) if f not in frame_hits]
                visibility={'weapon':assignment['weapon'],'provider_group':group['group'],'socket':socket,
                    'actual_body_contact_frame_ranges':frame_ranges(frame_hits),'safe_sample_frame_ranges':frame_ranges(safe),
                    'body_contact_bones':sorted(body_check['body_bone_contact_pairs_by_frame']),
                    'native_station':native_station,'hide_airframe_or_doors':False,
                    'changes_logical_ammunition_or_fire_gate':False}
                if not native_station and frame_hits:
                    last=max(frame_hits)
                    visibility.update(strategy='仅舱内ammo可视门控，保留原生装填与开舱发射门控',
                        hide_on_sample_frames=frame_ranges(frame_hits),
                        first_fully_clear_sample=last+1 if last<100 else None,
                        guarded_show_sample=last+2 if last<99 else None,
                        guarded_normalized_sequence_coordinate=(last+2)/100 if last<99 else None,
                        host_progress_contract='读取真实Parts/FlightEngineBlend对应通道进度，并核对其与本序列取样坐标的映射；不使用自造时钟。关闭时在进入接触区之前反向隐藏。',
                        continuous_interpolation_verified=False,
                        extra_blocker=None if last<99 else '序列结尾仍有新增相交，显隐不能解决完整可用呈现')
                else:
                    visibility.update(strategy='保留原生可视规则' if native_station else '本轮101帧未检出对Body表面相交；仍继承原生内部弹体可视规则')
                combo['internal_ammo_visibility'].append(visibility)
        combo['body_geometry_sweep']='complete_101_synchronized_frames_LOD0'
        combo['skinned_rack_sweep']='全部活动原架LOD0完整蒙皮101帧；固定provider沿原生静态变换'
        combo['status']='deferred'
        combo['blocker_kind']='animation_control'
        combo['blockers']=['运行时provider显示/真实离架/归还同映射尚未实现与核对',
            '未现场核对原生池容量、逻辑队列、静态Socket缺省旋转和缩放；必须读取原生完整世界变换',
            '101帧同步采样不等于所有独立通道相位、连续插值、发射后轨迹或游戏验证']
        if any(v.get('guarded_show_sample') is not None for v in combo['internal_ammo_visibility']):
            combo['blockers'].append('新增闭舱穿面必须落实舱内ammo显隐，真实Parts坐标映射未核对前不准入')
        if code=='su57tg':
            combo['required_animation_work']=[
                '只呈现原生SWP3 dummy，Body.spwc保留原生门运动；不需要活动SWP1/2作为provider',
                '原SWP1/2只隐藏架体，不递归隐藏已借位ammo；源池身份与原生index不改',
                '所有实际取位/朝向使用SWP3原生完整世界Socket变换，源池的原生弹体mesh绑定到同一provider',
                '保留LACM自身WeaponAnimationMesh与AnimationEndStaticMesh展开流程；真实发射沿原生开舱门控']
        elif code=='pkfa':
            combo['required_animation_work']=[
                'Body使用swcc；有SWP1 provider时另更新该架spwc，禁止套TG的spwc本体域',
                'GPB双站公共静态架方案：4AAM借sp21一处，GPB保留sp22；完整SWP2可见，原SWP1整架隐藏',
                'PLSL原生双炮体保留效果生命周期，不逐发隐藏；与内部舱门分域']
            if any(a['weapon']=='4AAM' and a['provider_group']=='SWP2' for a in combo['assignments']):
                combo['blockers'].append('4AAM借GPB中央挂架的支撑贴合仍需确认，碰面为0不等于有物理挂耳支撑')
                combo['support_fit_status']='未检出架体侵入，原生GPB中央支撑与较细导弹是否可接受尚未确认'
        else:
            combo['required_animation_work']=[
                'SAAM借较低SWP2原点，UGB保留SWP2原点，QAAM保留SWP3原点；完整同形四列架只呈现一套',
                'required_animation_groups包含全部provider，即使其对应逻辑兵装未选也继续骨架更新',
                'Body.spwc与provider同域进度持续；SAAM舱内显隐基于真实Parts坐标，不能隐藏机体/门盖']
        combo['layout_feasibility']='有明确原生资源/原生Socket条件方案，尚不授予运行准入'
        if len(combo['weapons'])==1:
            combo.update(status='native_reuse',blocker_kind=None,blockers=[],
                candidate_route='完全原生单SP透传，保留原组/挂架/Socket/动画，不实施借位或显隐接管',
                integration_mode='native_passthrough',required_animation_work=[],
                runtime_prerequisites=['直接使用原生单SP流程，不执行多SP provider映射或动画接管'],
                layout_feasibility='原生单挂保留，无新增布局开发事项')
        else:
            combo['integration_mode']='multi_sp_candidate_not_yet_integrated'
        combo['fixed_plsl_body_checks']=[]
        if 'PLSL' in combo['weapons']:
            _,info=geometry(gs['SWP3']['rack_mesh'])
            for section in info['lateral_triangle_pieces']:
                for projectile_info in combo['primitive_sweeps']:
                    first=section['bounds'];second=projectile_info['full_101_frame_vertex_sweep_aabb']
                    gaps=[max(first['min'][i]-second['max'][i],second['min'][i]-first['max'][i]) for i in range(3)]
                    combo['fixed_plsl_body_checks'].append({'fixed_mesh':info['asset'],
                        'triangle_piece':section['piece'],'projectile':projectile_info['key'],
                        'whole_sweep_aabb_separated':max(gaps)>0,'best_axis_separation':max(gaps),
                        'method':'按真实三角面全部顶点的左右半区分组；跨中线面另组完整保留，未用材质section误当左右炮体'})


def main():
    global assets,survey,manifest
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--manifest',type=Path,action='append',required=True)
    parser.add_argument('--batch',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    assets=anim.Assets(args.manifest);survey=CheckedSurvey(assets);manifest=args.manifest[0]
    batch=json.loads(args.batch.read_text(encoding='utf-8'))
    original={a['code']:a for a in batch['aircraft'] if a['code'] in NAMES}
    if set(original)!=set(NAMES):raise ValueError('显式批次必须包含三机原生研究基线')
    out=build_candidates()
    out['manifests']=[str(p.resolve()) for p in args.manifest]
    out['batch_source']={'path':str(args.batch.resolve()),'sha256':hashlib.sha256(args.batch.read_bytes()).hexdigest()}
    for aircraft in out['aircraft']:
        code=aircraft['code'];aircraft['skinned_rack_sweeps']=[]
        aircraft['actual_component_bindings']=actual_component_bindings(code,aircraft['groups'])
        aircraft['static_rack_geometries']=[]
        if [g['weapon']['class'] for g in aircraft['groups']] != [g['weapon']['class'] for g in original[code]['groups']]:
            raise ValueError('当前资源真实兵装与显式批次基线不一致：'+code)
        for group in aircraft['groups']:
            if group['component_class']=='SkeletalMeshComponent':
                _,_,info,_=skin(group['rack_mesh']);aircraft['skinned_rack_sweeps'].append(info)
            else:
                _,info=geometry(group['rack_mesh']);aircraft['static_rack_geometries'].append(info)
        _,_,info,_=skin(code);aircraft['body_skinned_geometry_sweep']=info
        aircraft['whole_rack_comparisons']=[]
        for first,second in itertools.combinations(aircraft['skinned_rack_sweeps'],2):
            f=mesh_cache[first['asset']];s=mesh_cache[second['asset']]
            if f[0].shape!=s[0].shape:continue
            aircraft['whole_rack_comparisons'].append({'first':first['asset'],'second':second['asset'],
                'all_101_frame_positions_exactly_equal':bool(np.array_equal(f[0],s[0])),
                'maximum_vertex_distance':float(np.linalg.norm(f[0]-s[0],axis=2).max()),
                'indices_exactly_equal':bool(np.array_equal(f[1],s[1])),
                'weights_exactly_equal':f[3]['lods'][0]['weights']==s[3]['lods'][0]['weights'],
                'sections_exactly_equal':f[3]['lods'][0]['sections']==s[3]['lods'][0]['sections'],
                'materials_exactly_equal':f[3]['materials']==s[3]['materials']})
        add_native_comparison(aircraft)
        aircraft['native_weapon_presentation']=[native_presentation(g['weapon']) for g in aircraft['groups']]
        aircraft['whole_aircraft_gate']={'required':'7/7全部完成才进入整机接入清单','ready':False,
            'offline_candidates':7,'runtime_verified':False,
            'reason':'候选已明确，provider/动画/显隐与支撑检查须落实；不交付部分组合'}
        aircraft['priority']={'su57tg':1,'su47':2,'pkfa':3}[code]
        aircraft['support_boundary']=(
            'SWP3只有原生dummy，不能据此声称有四套独立实体导轨。4AAM/HCAA沿LACM原站模板借位，Body对弹体101帧无新增相交；缺乏具名独立支撑几何，贴合须由原生挂载模板和后续外观核对共同确认。'
            if code=='su57tg' else
            '四列实体完整同形架，较低SAAM原UGB站无架体三角面相交；未用原点距离代替整架检查。' if code=='su47' else
            '原GPB完整双站中央架保留；较细4AAM借中线站无表面侵入，仍需核对挂耳贴合，不能用无碰面冒充有支撑。')
    out['method']=[
        '官方清单字节摘要与当前真实Pawn/兵装继承链，逐组件父链/Socket及原生mesh',
        '参考网格inverse bind与动画Skeleton按唯一骨名映射、验证父骨名称，解码101帧蒙皮',
        '弹体LOD0全部顶点按Socket位置与朝向取样；静态省略旋转仅以零作为明确假设',
        '15轴OBB分离弹体对；完整三角面17轴分离/触面见证检查架体和Body',
        '同武器原生碰面基线、具体Body骨面及帧段；新增门盖碰面以真实Parts显隐条件处理']
    out['offline_body_sweep_complete']=True
    out['source_script_sha256']=hashlib.sha256(Path(__file__).read_bytes()).hexdigest()
    out['skinning_reader_sha256']=hashlib.sha256(Path(__file__).with_name('read_cooked_skeletal_mount.py').read_bytes()).hexdigest()
    out['sources']=assets.used
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(json.dumps(out,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(json.dumps([{'code':a['code'],'combinations':len(a['combinations']),
        'all_projectile_obb_separated':all(c['all_projectile_obbs_separated_101_frames'] for c in a['combinations'])}
        for a in out['aircraft']],ensure_ascii=False))


if __name__=='__main__':main()
