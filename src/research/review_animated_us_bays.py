"""美系活动弹舱候选离线复核；不连接游戏、不生成游戏资产、不运行测试。

命令输入manifest可重复指定（亦支持嵌套清单），batch为三批研究JSON，animation为
已有动画研究JSON。输出只含资源指纹、站位、OBB和表面对照摘要，不保存原始顶点。
"""
import argparse
import base64
import hashlib
import itertools
import json
import math
from pathlib import Path
import sys

ROOT=Path(__file__).resolve().parents[2]
assets=None

def sub(a,b):return tuple(x-y for x,y in zip(a,b))
def dot(a,b):return sum(x*y for x,y in zip(a,b))
def cross(a,b):return(a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0])
def add(a,b):return tuple(x+y for x,y in zip(a,b))
def digest(x):return hashlib.sha256(json.dumps(x,sort_keys=True,separators=(',',':')).encode()).hexdigest()
def qrot(r):
    while len(r)==1:r=next(iter(r.values()))
    p,y,rr=[math.radians(float(r[k]))/2 for k in ['Pitch','Yaw','Roll']]
    sp,cp,sy,cy,sr,cr=math.sin(p),math.cos(p),math.sin(y),math.cos(y),math.sin(rr),math.cos(rr)
    return(cr*sp*sy-sr*cp*cy,-cr*sp*cy-sr*cp*sy,cr*cp*sy-sr*sp*cy,cr*cp*cy+sr*sp*sy)

def mesh_socket(code,group,socket):
    name=code+'_'+group
    mesh=read_skeletal_mesh(assets.load(name),name)
    sd=assets.load(name+'_Skeleton')
    se=next(e for e in sd['Exports'] if e['ObjectName']==name+'_Skeleton')
    bones=reference_bones(sd,se)
    tracks,meta=animation_tracks(assets.load(name+'_Anim'),bones)
    if len(tracks)!=len(bones):raise ValueError('不接受缺动画轨占位')
    ps=poses(bones,tracks,meta['frames'])
    by={b['name']:i for i,b in enumerate(bones)}
    sp=next(properties(sd['Exports'][i-1]) for i in properties(se)['Sockets']
            if properties(sd['Exports'][i-1])['SocketName']==socket)
    idx=by[sp['BoneName']]
    socket_poses=[(add(pose[idx][0],rotate(pose[idx][1],vector(sp['RelativeLocation']))),
                   multiply(pose[idx][1],qrot(sp['RelativeRotation']))) for pose in ps]
    lod=mesh['lods'][0]
    # 逐section局部骨号 -> 网格骨号 -> Skeleton同名骨号；不套用偶然相等的数字索引。
    global_mesh=[]
    for i,b in enumerate(mesh['bones']):
        p,q=b['position'],b['quaternion']
        if b['parent']>=0:
            pp,pq=global_mesh[b['parent']];p=add(pp,rotate(pq,p));q=multiply(pq,q)
        global_mesh.append((p,q))
    rp=[];local_vertices=[]
    for sec in lod['sections']:
        for vi in range(sec['start'],sec['start']+sec['vertices']):
            weights=[(sec['bone_map'][b],w) for b,w in lod['weights'][vi] if w]
            if len(weights)!=1 or weights[0][1]!=255:raise ValueError('本专项只接受完整刚性权重')
            bi=weights[0][0];si=by[mesh['bones'][bi]['name']]
            p,q=global_mesh[bi];inverse=(-q[0],-q[1],-q[2],q[3])
            local=rotate(inverse,sub(lod['points'][vi],p));local_vertices.append((si,local))
            rp.append(add(ps[0][si][0],rotate(ps[0][si][1],local)))
    triangles=[tuple(rp[i] for i in lod['indices'][j:j+3]) for j in range(0,len(lod['indices']),3)]
    # 相对Socket的几何保持不动时，一次全表面核对可覆盖全部101个键，而不是重复抽静态帧。
    max_relative_shift=0.;max_relative_rotation=0.
    for pose,(p,q) in zip(ps,socket_poses):
        iq=(-q[0],-q[1],-q[2],q[3]);p0,q0=socket_poses[0];iq0=(-q0[0],-q0[1],-q0[2],q0[3])
        for si,local in local_vertices:
            original=rotate(iq0,sub(add(ps[0][si][0],rotate(ps[0][si][1],local)),p0))
            actual=rotate(iq,sub(add(pose[si][0],rotate(pose[si][1],local)),p))
            max_relative_shift=max(max_relative_shift,math.dist(original,actual))
            max_relative_rotation=max(max_relative_rotation,angle(multiply(iq0,ps[0][si][1]),multiply(iq,pose[si][1])))
    return rp,triangles,socket_poses[0],mesh,{'frames':len(ps),'max_relative_vertex_shift':max_relative_shift,'max_relative_bone_rotation_degrees':max_relative_rotation}

def ray_hit(origin,direction,t):
    a,b,c=t;e1=sub(b,a);e2=sub(c,a);h=cross(direction,e2);det=dot(e1,h)
    if abs(det)<1e-10:return None
    inv=1/det;sv=sub(origin,a);u=inv*dot(sv,h)
    if u<-1e-8 or u>1+1e-8:return None
    q=cross(sv,e1);v=inv*dot(direction,q)
    if v<-1e-8 or u+v>1+1e-8:return None
    return inv*dot(e2,q)
def intersects(a,b):
    for source,target in ((a,b),(b,a)):
        for p,q in zip(source,source[1:]+source[:1]):
            hit=ray_hit(p,sub(q,p),target)
            if hit is not None and 1e-7<hit<1-1e-7:return True
    return coplanar_overlap(a,b)

def coplanar_overlap(a,b):
    normal=cross(sub(a[1],a[0]),sub(a[2],a[0]));length=math.sqrt(dot(normal,normal))
    if length<1e-12 or any(abs(dot(sub(p,a[0]),normal))/length>1e-7 for p in b):return False
    drop=max(range(3),key=lambda i:abs(normal[i]));axes=[i for i in range(3) if i!=drop]
    polygon=[tuple(p[i] for i in axes) for p in a];clip=[tuple(p[i] for i in axes) for p in b]
    cross2=lambda p,q:p[0]*q[1]-p[1]*q[0]
    minus=lambda p,q:(p[0]-q[0],p[1]-q[1])
    sign=1 if cross2(minus(clip[1],clip[0]),minus(clip[2],clip[0]))>=0 else -1
    for p,q in zip(clip,clip[1:]+clip[:1]):
        if not polygon:return False
        old=polygon;polygon=[];edge=minus(q,p)
        for start,end in zip(old,old[1:]+old[:1]):
            ds=sign*cross2(edge,minus(start,p));de=sign*cross2(edge,minus(end,p))
            if ds>=-1e-12:polygon.append(start)
            if (ds>1e-12 and de<-1e-12) or (ds<-1e-12 and de>1e-12):
                t=ds/(ds-de);polygon.append((start[0]+t*(end[0]-start[0]),start[1]+t*(end[1]-start[1])))
    area=abs(sum(cross2(p,q) for p,q in zip(polygon,polygon[1:]+polygon[:1])))/2 if len(polygon)>2 else 0
    return area>1e-9
def bounds(t):return(tuple(min(p[i] for p in t) for i in range(3)),tuple(max(p[i] for p in t) for i in range(3)))

def point_distance2(p,t):
    a,b,c=t;ab,ac,ap=sub(b,a),sub(c,a),sub(p,a);d1,d2=dot(ab,ap),dot(ac,ap)
    if d1<=0 and d2<=0:return dot(ap,ap)
    bp=sub(p,b);d3,d4=dot(ab,bp),dot(ac,bp)
    if d3>=0 and d4<=d3:return dot(bp,bp)
    vc=d1*d4-d3*d2
    if vc<=0 and d1>=0 and d3<=0:
        q=sub(ap,tuple(d1/(d1-d3)*x for x in ab));return dot(q,q)
    cp=sub(p,c);d5,d6=dot(ab,cp),dot(ac,cp)
    if d6>=0 and d5<=d6:return dot(cp,cp)
    vb=d5*d2-d1*d6
    if vb<=0 and d2>=0 and d6<=0:
        q=sub(ap,tuple(d2/(d2-d6)*x for x in ac));return dot(q,q)
    va=d3*d6-d5*d4
    if va<=0 and d4-d3>=0 and d5-d6>=0:
        v=(d4-d3)/((d4-d3)+(d5-d6));q=sub(bp,tuple(v*x for x in sub(c,b)));return dot(q,q)
    normal=cross(ab,ac);norm=dot(normal,normal)
    if norm<1e-18:return min(dot(ap,ap),dot(bp,bp),dot(cp,cp))
    return dot(ap,normal)**2/norm

def segment_distance2(p,q,a,b):
    u,v,w=sub(q,p),sub(b,a),sub(p,a);aa,bb,cc,dd,ee=dot(u,u),dot(u,v),dot(v,v),dot(u,w),dot(v,w)
    denom=aa*cc-bb*bb
    sn,sd,tn,td=0.,denom,0.,denom
    if denom<1e-18:sn,sd,tn,td=0.,1.,ee,cc
    else:
        sn,tn=bb*ee-cc*dd,aa*ee-bb*dd
        if sn<0:sn,tn,td=0.,ee,cc
        elif sn>sd:sn,tn,td=sd,ee+bb,cc
    if tn<0:
        tn=0.
        if -dd<0:sn=0.
        elif -dd>aa:sn=sd
        else:sn,sd=-dd,aa
    elif tn>td:
        tn=td
        if -dd+bb<0:sn=0.
        elif -dd+bb>aa:sn=sd
        else:sn,sd=-dd+bb,aa
    sc=0. if abs(sn)<1e-18 else sn/sd
    tc=0. if abs(tn)<1e-18 else tn/td
    delta=tuple(w[i]+sc*u[i]-tc*v[i] for i in range(3))
    return dot(delta,delta)

def triangle_distance2(a,b):
    if intersects(a,b):return 0.
    ds=[point_distance2(p,b) for p in a]+[point_distance2(p,a) for p in b]
    ds.extend(segment_distance2(p,q,r,s) for p,q in zip(a,a[1:]+a[:1]) for r,s in zip(b,b[1:]+b[:1]))
    return min(ds)

def inspect(label,code,group,socket,weapon):
    rp,rt,tr,sk,relative=mesh_socket(code,group,socket)
    d=assets.load(weapon);ex=next(e for e in d['Exports'] if e['ObjectName']==weapon)
    sm=parse_mesh(base64.b64decode(ex['Extras']));lod=sm['lods'][0]
    wp=[add(tr[0],rotate(tr[1],p)) for p in lod['points']]
    ids=lod['buffers'][0]['values'];wt=[tuple(wp[i] for i in ids[j:j+3]) for j in range(0,len(ids),3)]
    wb=[bounds(t) for t in wt];hits=[]
    for i,ra in enumerate(rt):
        rb=bounds(ra)
        for j,(ta,tb) in enumerate(zip(wt,wb)):
            if any(rb[1][k]<tb[0][k]-1e-8 or tb[1][k]<rb[0][k]-1e-8 for k in range(3)):continue
            if intersects(ra,ta):hits.append((i,j))
    # 奇偶射线仅作辅助，渲染网格未保证封闭，不作刚体体积证明。
    direction=(1.,.137,.291);box=bounds(wp);inside=[]
    for i,p in enumerate(rp):
        if any(p[k]<box[0][k] or p[k]>box[1][k] for k in range(3)):continue
        ts=sorted(t for tri in wt if (t:=ray_hit(p,direction,tri)) is not None and t>1e-7)
        unique=[]
        for t in ts:
            if not unique or abs(t-unique[-1])>1e-5:unique.append(t)
        if len(unique)%2:inside.append((i,p))
    best=1.;best_pair=None
    for i,ra in enumerate(rt):
        rb=bounds(ra)
        for j,(ta,tb) in enumerate(zip(wt,wb)):
            lower=sum(max(0.,rb[0][k]-tb[1][k],tb[0][k]-rb[1][k])**2 for k in range(3))
            if lower>best:continue
            d2=triangle_distance2(ra,ta)
            if d2<best:best,best_pair=d2,(i,j)
    return {'label':label,'provider':group+'.'+socket,'weapon_mesh':weapon,
            'rack_vertices':len(rp),'rack_triangles':len(rt),'weapon_vertices':len(wp),'weapon_triangles':len(wt),
            'triangle_crossing_pairs':len(hits),'rack_crossing_faces':len({i for i,j in hits}),
            'weapon_crossing_faces':len({j for i,j in hits}),'crossing_pair_sha256':digest(hits),
            'crossing_rack_face_indices':sorted({i for i,j in hits}),
            'auxiliary_rack_vertices_odd_ray_inside':len(inside),'auxiliary_inside_bounds':bounds([p for i,p in inside]) if inside else None,
            'closest_surface_distance_resource_units':math.sqrt(best) if best_pair is not None else None,
            'closest_surface_face_pair':best_pair,'distance_search_cap_resource_units':1.,
            'all_101_frames_rack_to_socket_relative_geometry':relative,
            'surface_method':'真实LOD0三角面AABB仅作粗筛，双向边段-三角面求交；计入共面正面积重叠，端点相贴不计穿越，原生同样算法基线对照；奇偶射线只辅助。',
            'mesh_parse_complete':sk['complete'],'lod_count':len(sk['lods'])}


def body_review(animation,code,mesh_name=None,configs_override=None):
    mesh_name=mesh_name or code
    body=read_skeletal_mesh(assets.load(mesh_name),mesh_name)
    sd=assets.load(mesh_name+'_Skeleton');se=next(e for e in sd['Exports'] if e['ObjectName']==mesh_name+'_Skeleton')
    bones=reference_bones(sd,se);by={b['name']:i for i,b in enumerate(bones)}
    tracks,meta=animation_tracks(assets.load(mesh_name+'_Anim'),bones)
    upstream=animation
    controlled=({b['bone'] for b in next(a for a in upstream['aircraft'] if a['code']==code)['body']['controlled_bones']} if mesh_name==code else {b['name'] for b in bones})
    filtered={}
    for i,b in enumerate(body['bones']):
        si=by[b['name']]
        filtered[si]=tracks[si] if b['name'] in controlled else {'translation':[b['position']],'rotation':[b['quaternion']]}
    ps=poses(bones,filtered,meta['frames'])
    ref=[]
    for b in body['bones']:
        p,q=b['position'],b['quaternion']
        if b['parent']>=0:
            pp,pq=ref[b['parent']];p=add(pp,rotate(pq,p));q=multiply(pq,q)
        ref.append((p,q))

    def provider_sequence(group,socket):
        n=code+'_'+group;d=assets.load(n+'_Skeleton');e=next(e for e in d['Exports'] if e['ObjectName']==n+'_Skeleton')
        bs=reference_bones(d,e);tr,me=animation_tracks(assets.load(n+'_Anim'),bs);poses_=poses(bs,tr,me['frames'])
        sp=next(properties(d['Exports'][r-1]) for r in properties(e)['Sockets'] if properties(d['Exports'][r-1])['SocketName']==socket)
        i=next(i for i,b in enumerate(bs) if b['name']==sp['BoneName'])
        return [(add(p[i][0],rotate(p[i][1],vector(sp['RelativeLocation']))),multiply(p[i][1],qrot(sp['RelativeRotation']))) for p in poses_]

    def cells(b,cell=3.):
        return itertools.product(*(range(math.floor(b[0][i]/cell),math.floor(b[1][i]/cell)+1) for i in range(3)))

    configs=([('native_HVAA','SWP1','sp11','w_saam_a0_SM'),
              ('candidate_HVAA_sp33','SWP3','sp33','w_saam_a0_SM'),
              ('native_4AAM_outer','SWP2','sp22','w_4aam_a0_SM'),
              ('native_UGB_inner','SWP3','sp34','w_ugb_a0_SM')] if code=='yf23' else
             [('native_QAAM_inner','SWP1','sp15','w_qaam_a0_SM'),
              ('native_QAAM_inner_positive','SWP1','sp16','w_qaam_a0_SM'),
              ('native_XSDB_outer_positive','SWP2','sp22','w_xsdb_a0_closed_SM'),
              ('native_XSDB_mid_negative','SWP2','sp23','w_xsdb_a0_closed_SM'),
              ('native_XSDB_mid_positive','SWP2','sp24','w_xsdb_a0_closed_SM'),
              ('candidate_QAAM_inner','SWP3','sp43','w_qaam_a0_SM'),
              ('candidate_QAAM_inner_positive','SWP3','sp44','w_qaam_a0_SM'),
              ('native_XSDB_outer','SWP2','sp21','w_xsdb_a0_closed_SM'),
              ('candidate_XSDB_outer','SWP3','sp39','w_xsdb_a0_closed_SM'),
              ('candidate_XSDB_outer_positive','SWP3','sp40','w_xsdb_a0_closed_SM'),
              ('candidate_XSDB_mid_negative','SWP3','sp41','w_xsdb_a0_closed_SM'),
              ('candidate_XSDB_mid_positive','SWP3','sp42','w_xsdb_a0_closed_SM')])
    if configs_override is not None:configs=configs_override
    weapons=[]
    for label,group,socket,name in configs:
        d=assets.load(name);e=next(e for e in d['Exports'] if e['ObjectName']==name)
        mesh=parse_mesh(base64.b64decode(e['Extras']))
        lod=mesh['lods'][0];idx=lod['buffers'][0]['values']
        # 以弹体局部坐标建立空间表；每帧把机体蒙皮表面转入该原生provider。
        triangles=[tuple(lod['points'][i] for i in idx[j:j+3]) for j in range(0,len(idx),3)]
        boxes=[bounds(t) for t in triangles];grid={}
        for i,b in enumerate(boxes):
            for cell in cells(b):grid.setdefault(cell,set()).add(i)
        weapons.append(dict(label=label,group=group,socket=socket,mesh=name,poses=provider_sequence(group,socket),triangles=triangles,boxes=boxes,grid=grid,bounds=bounds(lod['points'])))

    sweep=[]
    for weapon in weapons:
        corners=list(itertools.product(*zip(*weapon['bounds'])))
        sweep.extend(add(pos,rotate(q,p)) for pos,q in weapon['poses'] for p in corners)
    sweep_box=bounds(sweep)
    moving_bones={i for i,b in enumerate(bones) if set(b['chain']) & controlled}
    results=[]
    for li,lod in enumerate(body['lods']):
        local=[]
        for sec in lod['sections']:
            for vi in range(sec['start'],sec['start']+sec['vertices']):
                influence=[]
                for local_bone,w in lod['weights'][vi]:
                    if not w:continue
                    bi=sec['bone_map'][local_bone];p,q=ref[bi];inv=(-q[0],-q[1],-q[2],q[3])
                    influence.append((by[body['bones'][bi]['name']],w/255,rotate(inv,sub(lod['points'][vi],p))))
                local.append(influence)
        animated=[any(i in moving_bones for i,w,p in infl) for infl in local]
        faces=[]
        for face in range(len(lod['indices'])//3):
            ids=lod['indices'][face*3:face*3+3]
            bb=bounds([lod['points'][i] for i in ids])
            if any(animated[i] for i in ids) or not any(bb[1][k]<sweep_box[0][k] or sweep_box[1][k]<bb[0][k] for k in range(3)):
                faces.append(face)
        rows={w['label']:{'per_frame_crossing_pairs':[],'body_faces':set(),'weapon_faces':set()} for w in weapons}
        for frame,pose in enumerate(ps):
            points=[]
            for vi,infl in enumerate(local):
                if not animated[vi]:
                    points.append(lod['points'][vi]);continue
                vals=[tuple(w*v for v in add(pose[i][0],rotate(pose[i][1],loc))) for i,w,loc in infl]
                points.append(tuple(sum(v[k] for v in vals) for k in range(3)))
            for weapon in weapons:
                p,q=weapon['poses'][frame];iq=(-q[0],-q[1],-q[2],q[3])
                transformed=[rotate(iq,sub(v,p)) for v in points]
                paircount=0;row=rows[weapon['label']]
                for face in faces:
                    tri=tuple(transformed[i] for i in lod['indices'][face*3:face*3+3]);bb=bounds(tri);wb=weapon['bounds']
                    if any(bb[1][k]<wb[0][k] or wb[1][k]<bb[0][k] for k in range(3)):continue
                    candidates=set()
                    clipped=(tuple(max(bb[0][k],wb[0][k]) for k in range(3)),tuple(min(bb[1][k],wb[1][k]) for k in range(3)))
                    for cell in cells(clipped):candidates.update(weapon['grid'].get(cell,()))
                    for wi in candidates:
                        tri_box=weapon['boxes'][wi]
                        if any(bb[1][k]<tri_box[0][k] or tri_box[1][k]<bb[0][k] for k in range(3)):continue
                        if intersects(tri,weapon['triangles'][wi]):
                            paircount+=1;row['body_faces'].add(face);row['weapon_faces'].add(wi)
                row['per_frame_crossing_pairs'].append(paircount)
            if frame%25==0:print(code,'body LOD',li,'frame',frame,flush=True)
        for weapon in weapons:
            row=rows[weapon['label']];counts=row['per_frame_crossing_pairs']
            results.append({'lod':li,'body_vertices':len(lod['points']),'body_triangles':len(lod['indices'])//3,'label':weapon['label'],'provider':weapon['group']+'.'+weapon['socket'],'weapon_mesh':weapon['mesh'],
                            'frames':len(counts),'per_frame_crossing_pairs':counts,'frames_with_crossings':sum(c>0 for c in counts),'peak_crossing_pairs':max(counts),'body_face_count':len(row['body_faces']),'weapon_face_count':len(row['weapon_faces']),
                            'body_face_indices_sha256':digest(sorted(row['body_faces'])),'body_face_indices':sorted(row['body_faces']),
                            'method':'完整机体LOD蒙皮LBS，按section bone_map解析所有255权重；仅spwc域使用原始101键，其余通道保留参考姿态。空间网格粗筛后双向边段/三角面穿越；包含共面正面积重叠，不含单边/点接触。'})
        print('LOD completed',li,flush=True)
    return {'mesh':mesh_name,'parsed_complete':body['complete'],'lod_count':len(body['lods']),'spwc_bones':sorted(controlled),'rows':results,'other_channels':'仅spwc域使用原始键，其余通道保持参考姿态；不宣称全部FlightEngineBlend运行输出。'}


NAMES={'yf23':['HVAA','4AAM','UGB'],'f22a':['QAAM','XSDB','8AAM'],'fb22':['XSDB','LAAM','SFFS']}
MESHES={'yf23':['w_saam_a0_SM','w_4aam_a0_SM','w_ugb_a0_SM'],
        'f22a':['w_qaam_a0_SM','w_xsdb_a0_closed_SM','w_4aam_a0_SM'],
        'fb22':['w_xsdb_a0_closed_SM','w_laam_x0_SM','w_sffs_a0_closed_SM']}
# 每项是owner组、provider组、真实Socket、替换完整网格。禁止创建补偿坐标。
MULTI={
 'yf23':[
  [(1,3,['sp33'],'w_saam_a0_SM'),(2,2,['sp21','sp22','sp24'],None)],
  [(1,3,['sp33'],'w_saam_a0_SM'),(3,3,['sp31','sp32','sp34'],None)],
  [(2,2,['sp21','sp22'],None),(3,3,['sp33','sp34'],None)],
  [(1,3,['sp33'],'w_saam_a0_SM'),(2,2,['sp21','sp22'],None),(3,3,['sp34'],None)]],
 'f22a':[
  [(1,3,['sp43','sp44'],'w_qaam_a0_SM'),(2,3,['sp39','sp40','sp41','sp42'],'w_xsdb_a0_closed_SM')],
  [(1,3,['sp43','sp44'],'w_qaam_a0_SM'),(3,3,['sp31','sp32','sp33','sp34','sp35','sp36','sp37','sp38'],None)],
  [(2,3,['sp39','sp40','sp41','sp42'],'w_xsdb_a0_closed_SM'),(3,3,['sp31','sp32','sp33','sp34','sp35','sp36','sp37','sp38'],None)],
  [(1,3,['sp43','sp44'],'w_qaam_a0_SM'),(2,3,['sp39','sp40','sp41','sp42'],'w_xsdb_a0_closed_SM'),(3,3,['sp31','sp32','sp33','sp34','sp35','sp36','sp37','sp38'],None)]],
 'fb22':[
  [(1,1,['sp11','sp12'],None),(2,2,['sp25','sp26'],None)],
  [(1,1,['sp11'],None),(3,3,['sp32'],None)],
  [(2,2,['sp21'],None),(3,3,['sp32'],None)],
  [(1,1,['sp11'],None),(2,2,['sp25'],None),(3,3,['sp32'],None)]]}
SOCKET_CACHE={}
STATIC_CACHE={}

def socket_samples(code,group,component):
    key=(code,group)
    if key in SOCKET_CACHE:return SOCKET_CACHE[key]
    name=component['skeleton'];doc=assets.load(name)
    ex=next(e for e in doc['Exports'] if e['ObjectName'].casefold()==name.casefold())
    bones=reference_bones(doc,ex);tracks,meta=animation_tracks(assets.load(component['sequence']),bones)
    if len(tracks)!=len(bones):raise ValueError('不接受缺动画轨占位')
    frames=poses(bones,tracks,meta['frames']);by={b['name']:i for i,b in enumerate(bones)};result={}
    for ref in properties(ex)['Sockets']:
        p=properties(doc['Exports'][ref-1]);n=p['SocketName']
        if not n.lower().startswith('sp'):continue
        if p.get('BoneName') not in by or 'RelativeLocation' not in p or 'RelativeRotation' not in p:
            raise ValueError('Socket缺省变换，不能填猜测值')
        i=by[p['BoneName']];local=vector(p['RelativeLocation']);q=qrot(p['RelativeRotation'])
        result[n]={'bone':p['BoneName'],'rotation':p['RelativeRotation'],
                   'samples':[(add(pose[i][0],rotate(pose[i][1],local)),multiply(pose[i][1],q)) for pose in frames]}
    SOCKET_CACHE[key]=result;return result

def static_mesh(name):
    if name not in STATIC_CACHE:
        doc=assets.load(name);ex=next(e for e in doc['Exports'] if e['ObjectName']==name)
        parsed=parse_mesh(base64.b64decode(ex['Extras']))
        points=parsed['lods'][0]['points'];b=bounds(points)
        STATIC_CACHE[name]={'name':name,'bounds':b,'vertices':len(points),'lod_count':len(parsed['lods']),
                            'max_original_lod_bounds_overflow':max(parsed['original_lod_bounds_overflow'])}
    return STATIC_CACHE[name]

def obb(mesh,pose):
    b=static_mesh(mesh)['bounds'];c=tuple((lo+hi)/2 for lo,hi in zip(*b));h=tuple((hi-lo)/2 for lo,hi in zip(*b))
    p,q=pose
    return add(p,rotate(q,c)),tuple(rotate(q,a) for a in ((1,0,0),(0,1,0),(0,0,1))),h

def separating_margin(a,b):
    delta=sub(b[0],a[0]);axes=list(a[1])+list(b[1])+[cross(x,y) for x in a[1] for y in b[1]];values=[]
    for axis in axes:
        length=math.sqrt(dot(axis,axis))
        if length<1e-9:continue
        u=tuple(v/length for v in axis)
        values.append(abs(dot(delta,u))-sum(h*abs(dot(v,u)) for h,v in zip(a[2],a[1]))-sum(h*abs(dot(v,u)) for h,v in zip(b[2],b[1])))
    return max(values)

def scan_pairs(code,assignments,components):
    results=[]
    for a,b in itertools.combinations(assignments,2):
        for sa,sb in itertools.product(a['sockets'],b['sockets']):
            ta=socket_samples(code,a['provider_group'],components[a['provider_group']])[sa]['samples']
            tb=socket_samples(code,b['provider_group'],components[b['provider_group']])[sb]['samples']
            cases=[('closed_mounted',a['visible_mesh'],b['visible_mesh'])]
            for label,ma,mb in [('a_deployed_at_mount',a['visible_mesh'].replace('_closed_SM','_SM'),b['visible_mesh']),
                                ('b_deployed_at_mount',a['visible_mesh'],b['visible_mesh'].replace('_closed_SM','_SM'))]:
                if (ma,mb)!=(a['visible_mesh'],b['visible_mesh']):cases.append((label,ma,mb))
            for mode,ma,mb in cases:
                gaps=[separating_margin(obb(ma,x),obb(mb,y)) for x,y in zip(ta,tb)]
                results.append({'pair':[a['weapon']+':'+a['provider_group']+'.'+sa,b['weapon']+':'+b['provider_group']+'.'+sb],
                                'mode':mode,'frames':len(gaps),'overlap_frame_count':sum(g<=0 for g in gaps),
                                'minimum_separating_axis_margin':min(gaps),'margins_sha256':digest(gaps),
                                'boundary':'正值为完整局部Bounds OBB存在分离轴；负值不证明三角面相交。展开体放在挂点只是保守筛查，不是下落/展开时序。'})
    return results

def mesh_equivalence(code,first,second):
    ds=[assets.load(f'{code}_SWP{i}') for i in (first,second)]
    normalize=lambda text:text.replace(f'{code}_SWP{first}',f'{code}_SWP#').replace(f'{code}_SWP{second}',f'{code}_SWP#')
    es=[next(e for e in d['Exports'] if e['ObjectName']==f'{code}_SWP{i}') for d,i in zip(ds,(first,second))]
    cooked=[read_skeletal_mesh(d,f'{code}_SWP{i}') for d,i in zip(ds,(first,second))]
    return {'code':code,'groups':[f'SWP{first}',f'SWP{second}'],
            'payload_bytes_equal':es[0]['Extras']==es[1]['Extras'],
            'all_export_properties_equal':properties(es[0])==properties(es[1]),
            'name_map_indices_semantically_equal_except_self_identity':normalize(json.dumps(ds[0]['NameMap']))==normalize(json.dumps(ds[1]['NameMap'])),
            'imports_semantically_equal_except_skeleton_identity':normalize(json.dumps(ds[0]['Imports'],sort_keys=True))==normalize(json.dumps(ds[1]['Imports'],sort_keys=True)),
            'parsed_bones_equal':cooked[0]['bones']==cooked[1]['bones'],'parsed_materials_equal':cooked[0]['materials']==cooked[1]['materials'],
            'parsed_all_lod_points_indices_weights_sections_equal':cooked[0]['lods']==cooked[1]['lods'],
            'complete_payload_consumed':all(c['complete'] for c in cooked),
            'lods':[[{'vertices':len(l['points']),'triangles':len(l['indices'])//3,'sections':len(l['sections'])} for l in c['lods']] for c in cooked],
            'boundary':'只断言渲染几何/蒙皮与索引语义，Socket及资源身份各自保留，不称整包资源相等。'}

def f22_rack_cases():
    return [(label,'SWP3',socket,mesh) for socket,mesh in [('sp43','w_qaam_a0_SM'),('sp44','w_qaam_a0_SM'),('sp39','w_xsdb_a0_closed_SM'),('sp40','w_xsdb_a0_closed_SM'),('sp41','w_xsdb_a0_closed_SM'),('sp42','w_xsdb_a0_closed_SM')]
            for label,mesh in [('native8AAM_'+socket,'w_4aam_a0_SM'),('candidate_'+socket,mesh)]]


def xsdb_deployment_review(animated):
    if not assets.has('w_xsdb_a0_Anim'):
        return {'available':False,'required_assets':['w_xsdb_a0','w_xsdb_a0_Skeleton','w_xsdb_a0_Anim']}
    mesh=read_skeletal_mesh(assets.load('w_xsdb_a0'),'w_xsdb_a0')
    d=assets.load('w_xsdb_a0_Skeleton');e=next(e for e in d['Exports'] if e['ObjectName']=='w_xsdb_a0_Skeleton')
    bones=reference_bones(d,e);by={b['name']:i for i,b in enumerate(bones)}
    tracks,meta=animation_tracks(assets.load('w_xsdb_a0_Anim'),bones)
    frames=poses(bones,tracks,meta['frames']);lod=mesh['lods'][0];local=[]
    for sec in lod['sections']:
        for vi in range(sec['start'],sec['start']+sec['vertices']):
            inf=[]
            for bi,w in lod['weights'][vi]:
                if not w:continue
                bone=bones[by[mesh['bones'][sec['bone_map'][bi]]['name']]];q=bone['quaternion'];inverse=(-q[0],-q[1],-q[2],q[3])
                inf.append((by[bone['name']],w/255,rotate(inverse,sub(lod['points'][vi],bone['position']))))
            local.append(inf)
    # 读取实际组件播放速率，不能把滚转延迟当开翼延迟。
    weapon=assets.load('plwp_xsdb_a0')
    component=properties(next(e for e in weapon['Exports'] if e['ObjectName']=='WeaponAnimationMesh_GEN_VARIABLE'))
    rate=component['AnimationData']['SavedPlayRate']
    default=properties(next(e for e in weapon['Exports'] if e['ObjectName']=='Default__plwp_xsdb_a0_C'))
    c=next(c for c in animated['components'] if c['group']=='SWP3');sockets=socket_samples('f22a','SWP3',c)
    def transformed_box(bb,transform):
        p,q=transform
        return bounds([add(p,rotate(q,x)) for x in itertools.product(*zip(*bb))])
    rows=[]
    for frame,pose in enumerate(frames):
        pts=[]
        for infl in local:
            vals=[tuple(w*v for v in add(pose[i][0],rotate(pose[i][1],loc))) for i,w,loc in infl]
            pts.append(tuple(sum(p[k] for p in vals) for k in range(3)))
        bb=bounds(pts);required=0.
        for bomb,missile in itertools.product(('sp39','sp40','sp41','sp42'),('sp43','sp44')):
            for bomb_pose,missile_pose in zip(sockets[bomb]['samples'],sockets[missile]['samples']):
                a=transformed_box(bb,bomb_pose);b=transformed_box(static_mesh('w_qaam_a0_SM')['bounds'],missile_pose)
                if all(a[0][k]<b[1][k] and b[0][k]<a[1][k] for k in (0,1)):
                    required=max(required,a[1][2]-b[0][2]+1e-5)
        t=frame*meta['asset_length']/(meta['frames']-1)/rate
        rows.append({'key':frame,'resource_playback_seconds':t,'skinned_local_bounds':bb,
                     'required_aircraft_local_downward_displacement':required,
                     'level_zero_vertical_speed_freefall_world_scale_upper_bound':.5*default['FreefallAcceleration']*t*t/required if required>0 else None})
    relevant=[r for r in rows if r['required_aircraft_local_downward_displacement']>0]
    return {'available':True,'frames':meta['frames'],'asset_length':meta['asset_length'],'saved_play_rate':rate,
            'first_widening_key':next(r['key'] for r in rows if r['skinned_local_bounds'][1][1]>rows[0]['skinned_local_bounds'][1][1]+.001),
            'native_properties':{k:default[k] for k in ('FreefallAcceleration','FreefallRotation','InitialSpeed','RollAnimationDelay','RollAnimationRate')},
            'rows':rows,'minimum_conditional_world_scale_upper_bound':min(r['level_zero_vertical_speed_freefall_world_scale_upper_bound'] for r in relevant),
            'boundary':'下落包络是假设机体局部-Z离开其余挂载的充分条件；数值自由落下只假设平飞、零垂向初速且该加速度实际生效。未证明原生Bomb物理、世界缩放、机体机动/横滚或动画启动时刻。滚转2秒延迟不当作开翼延迟。',
            'runtime_prerequisites':['离架后才启动武器动画；不能把展开动画用于机体挂载模型。','实机核对平飞/滚转投放时，相对飞机局部位移先越过本包络再进入宽翼阶段；保留原生物理与动画速度。']}


def classify_results(out):
    """分开记录资源方案可实现与运行实测；不能因面配对数不同就直接报机体碰撞。"""
    comparisons=[]
    mapping={'candidate_QAAM_inner':'native_QAAM_inner','candidate_QAAM_inner_positive':'native_QAAM_inner_positive',
             'candidate_XSDB_outer':'native_XSDB_outer','candidate_XSDB_outer_positive':'native_XSDB_outer_positive',
             'candidate_XSDB_mid_negative':'native_XSDB_mid_negative','candidate_XSDB_mid_positive':'native_XSDB_mid_positive'}
    review=out['body_surface_review']['f22a'];rows={(r['lod'],r['label']):r for r in review['rows']}
    for (lod,label),candidate in rows.items():
        if label not in mapping or (lod,mapping[label]) not in rows:continue
        native=rows[(lod,mapping[label])]
        added=sorted(set(candidate['body_face_indices'])-set(native['body_face_indices']))
        comparisons.append({'lod':lod,'candidate':label,'baseline':mapping[label],
                            'new_body_face_indices':added,'removed_body_face_count':len(set(native['body_face_indices'])-set(candidate['body_face_indices'])),
                            'candidate_peak_pair_count':candidate['peak_crossing_pairs'],'native_peak_pair_count':native['peak_crossing_pairs'],
                            'same_or_subset_of_native_body_faces':not added})
    out['f22_body_template_comparisons']=comparisons
    complete=len(comparisons)==review['lod_count']*len(mapping)
    all_native=complete and all(c['same_or_subset_of_native_body_faces'] for c in comparisons)
    for a in out['aircraft']:
        if a['code']=='yf23':
            for c in a['combinations']:
                if len(c['weapons'])>1 and 'HVAA' in c['weapons']:
                    c['status']='deferred';c['disposition']='deferred'
                    c['blockers']=[{'kind':'station_overlap','detail':'UGB原点前移2资源单位，长HVAA进入原生HVAA未碰到的固定前舱隔框；外列sp31还扫侧舱门。'},
                                   {'kind':'unknown_support','detail':'不前移的4AAM原点抬高0.394并沿架体穿入；改唯一SWP1仅能让四列弹体侧翼擦双列轨，导轨轴不对应。禁止以补偿坐标、拆蒙皮或新站掩盖。'}]
            a['integration_gate']={'eligible':False,'reason':'HVAA三个多挂在不改坐标/不拆架的约束下仍有固定隔框或支撑阻塞；不交付半套整机。'}
        elif a['code']=='f22a':
            a['integration_gate']={'eligible':all_native,'meaning':'完整离线方案可进入运行实现，附明确XSDB离架/展开及身份生命周期前提；未安装/未实机验证。',
                                   'body_comparison_complete':complete,'all_body_faces_same_or_subset_of_corresponding_native_baseline':all_native}
            for c in a['combinations']:
                if len(c['weapons'])==1:continue
                if not all_native:
                    c['status']='deferred';c['disposition']='deferred'
                    c['blockers'].append({'kind':'station_overlap','detail':'Body逐LOD原生对应面增量仍待核对；精确差异见f22_body_template_comparisons，不能只比较面数。'})
                c['accepted_tradeoffs']=['多挂统一唯一完整SWP3，未选8AAM时其外部原生结构可空置；不叠加第二套架。',
                                         'QAAM只使用内两点，XSDB只用四点，8AAM用外八点；不改变逻辑队列/库存/窗口/锁定/齐射或冷却。']
                if 'XSDB' in c['weapons'] and 'QAAM' in c['weapons']:
                    c['runtime_prerequisites'].append('XSDB离架展开按xsdb_deployment_review逐键下落包络核对，特别是滚转/大姿态投放；展开模型不得留在挂点。')
    out['verification']['f22_baseline_comparisons_complete']=complete


def verify_group_binding(code,index,group,component):
    """重新核对原始Pawn/父配置/AnimBP，摘要里的名称不直接当可信绑定。"""
    pawn=assets.load('AcePlayerPawn_'+code)
    template=next(e for e in pawn['Exports'] if e['ObjectName']==f'SWP{index}_GEN_VARIABLE')
    p=properties(template)
    mesh=resolve(pawn,p['SkeletalMesh']);anim=resolve(pawn,p['AnimClass']).removesuffix('_C')
    if mesh!=component['mesh'] or anim!=component['driver']['anim_blueprint']:
        raise ValueError('Pawn挂架/动画类绑定与研究清单不符')
    parents=[]
    for e in pawn['Exports']:
        node=properties(e)
        if node.get('ComponentTemplate') and resolve(pawn,node['ComponentTemplate'])==template['ObjectName']:
            parents.append(node.get('ParentComponentOrVariableName'))
    if parents!=['WeaponMeshBase']:raise ValueError('挂架父链没有唯一对应WeaponMeshBase')
    wd=assets.load(anim);default=next(e for e in wd['Exports'] if e['ObjectName'].startswith('Default__'))
    nodes=[v for k,v in properties(default).items() if k.startswith('AnimGraphNode_FlightEngineBlend')]
    if len(nodes)!=1 or resolve(wd,nodes[0]['SequenceConfig']['Sequence'])!=component['sequence']:
        raise ValueError('AnimBP实际序列改变')
    md=assets.load(mesh);me=next(e for e in md['Exports'] if e['ObjectName']==mesh)
    if resolve(md,properties(me)['Skeleton'])!=component['skeleton']:raise ValueError('网格Skeleton引用改变')
    cls=None;source=None
    for name in ['AcePlayerPawn_'+code,'AcePlayerPawn']:
        doc=assets.load(name);activator=next(e for e in doc['Exports'] if e['ObjectName']=='PlayerWeaponActivator')
        cfg=properties(activator).get(f'SpecialWeaponConfig{index}',{})
        if cfg.get('WeaponClass'):
            cls=resolve(doc,cfg['WeaponClass']);source=name;break
    if cls!=group['weapon']['class']:raise ValueError('真实兵装类继承配置改变')
    return {'verified':True,'parent':'WeaponMeshBase','mesh':mesh,'skeleton':component['skeleton'],
            'anim_blueprint':anim,'sequence':component['sequence'],'weapon_class':cls,'weapon_class_source':source}


def main(args):
    global assets
    assets=Assets(args.manifest)
    batch=json.loads(args.batch.read_text(encoding='utf-8'));animation=json.loads(args.animation.read_text(encoding='utf-8'))
    original={p['code']:p for p in batch['aircraft']};animated={p['code']:p for p in animation['aircraft']}
    out={'schema':'animated-us-bays-review-v2','method':'101帧原始序列/完整cooked蒙皮及静态弹体LOD/OBB/真实三角面原生增量对照',
         'compiled':False,'tests_run':False,'installed':False,'game_process_access':False,'assets_generated':False,'aircraft':[],
         'inputs':[{'name':p.name,'sha256':hashlib.sha256(p.read_bytes()).hexdigest()} for p in (args.batch,args.animation)],
         'limits':['资源姿态不等于游戏FlightEngineBlend混合/世界变换。','原生附着表面接触允许，三角面交叉数量不是游戏碰撞体或故障指标。','未模拟离架下落/点火/展开时序；候选须接入运行身份与生命周期后实机验收。']}
    out['whole_mesh_comparisons']=[mesh_equivalence(*p) for p in [('yf23',2,3),('fb22',1,2),('f22a',1,2)]]
    for code in ('yf23','f22a','fb22'):
        base=original[code];anim=animated[code];components={g['group']:g for g in anim['components']};groups=[]
        pawn=assets.load('AcePlayerPawn_'+code);assets.load('AcePlayerPawn');assets.load('PlaneBodyConfig');assets.load(anim['parts_asset'])
        for i,g in enumerate(base['groups'],1):
            component=components['SWP'+str(i)];assets.load(component['mesh']);assets.load(component['driver']['anim_blueprint'])
            cls=g['weapon']['class'].removesuffix('_C')
            while assets.has(cls):
                d=assets.load(cls);ce=next(e for e in d['Exports'] if e['ObjectName']==cls+'_C');cls=(resolve(d,ce.get('SuperIndex',0)) or '').removesuffix('_C')
            groups.append({'group':'SWP'+str(i),'weapon':NAMES[code][i-1],'weapon_class':g['weapon']['class'],'window':g['weapon']['defaults'].get('MaxLoadedCount'),
                           'rack_mesh':component['mesh'],'socket_asset':component['skeleton'],'parent':'WeaponMeshBase','driver':component['driver'],
                           'actual_binding':verify_group_binding(code,i,g,component),
                           'runtime_pool_count':None,'runtime_logical_count':None,'socket_names':list(socket_samples(code,'SWP'+str(i),component))})
        selections=[[(i,i,groups[i-1]['socket_names'],None)] for i in (1,2,3)]+MULTI[code];cases=[]
        for selection in selections:
            assignments=[]
            for owner,provider,ns,replacement in selection:
                g=groups[owner-1];pg='SWP'+str(provider);samples=socket_samples(code,pg,components[pg]);mesh=replacement or MESHES[code][owner-1]
                assignments.append({'weapon':g['weapon'],'weapon_class':g['weapon_class'],'owner_group':g['group'],'provider_group':pg,'sockets':ns,
                                    'replacement_mesh':replacement,'visible_mesh':mesh,'loaded_window':g['window']['value'],'below_window':len(ns)<g['window']['value'],
                                    'runtime_logical_count':None,'runtime_pool_count':None,'logical_to_provider_map':None,
                                    'launch_map':{'provider_group':pg,'sockets':ns,'source':'provider组件当帧世界变换','after_animation_update':True,'preserve_mount_ordinal':True},
                                    'socket_details':[{'socket':n,'bone':samples[n]['bone'],'serialized_rotation':samples[n]['rotation'],'frame0':samples[n]['samples'][0][0],
                                                       'frame100':samples[n]['samples'][-1][0],'pose_sha256':digest(samples[n]['samples'])} for n in ns]})
            single=len(selection)==1
            render=[assignments[0]['owner_group']] if single else (['SWP2'] if code=='yf23' else ['SWP3'] if any(o==3 for o,p,n,r in selection) else ['SWP3'] if code=='f22a' else ['SWP1'])
            required=sorted(set(render+[a['provider_group'] for a in assignments]))
            blockers=[] if single or code in ('yf23','f22a') or (code=='fb22' and len(selection)==2 and all(s[0]!=3 for s in selection)) else [{'kind':'unknown_support','detail':'公共架子几何或SFFS不同蒙皮/动画域支撑尚未完全对应，不能只按同名骨骼放行。'}]
            case={'weapons':[a['weapon'] for a in assignments],'assignments':assignments,'status':'deferred' if blockers else 'native_reuse',
                  'disposition':'deferred' if blockers else 'native_baseline' if single else 'candidate_existing_assets','blockers':blockers,
                  'weapon_mask':sum(1<<(owner-1) for owner,provider,ns,replacement in selection),
                  'rack_plan':{'render_groups':render,'hidden_mesh_groups':[g for g in ('SWP1','SWP2','SWP3') if g not in render],
                               'required_animation_groups':required,'whole_native_skeletons_preserved':True,'hide_mesh_without_child_propagation':True,
                               'asset_split':False,'new_station':False,'owner_weapon_pool_hidden_when_provider_differs':True},
                  'animation_domain':anim['animation_design'],'sweep_101':scan_pairs(code,assignments,components),
                  'display_granularity':'一套完整公共架呈现；provider弹体逐组件显隐，不传播隐架状态到弹体，不停止provider骨架更新。',
                  'runtime_prerequisites':['weapon_mask、required_animation_groups、render_groups分离；不伪选未选SP，不改库存/冷却/原生选择编号。',
                                           'owner原池隐藏；借位显示和真实离架使用同一provider组件，分别核对serial身份。',
                                           '共舱连续进度保持，所有required组更新当帧骨架后再取世界变换；关闭/失败/换机/重建归还最新原生状态。',
                                           '少于窗口的位置保留原持久mountOrdinal和真实队列，不能直接把provider数组下标当逻辑顺序。'],
                  'runtime_verified':False}
            if code=='yf23' and not single:
                columns={int(n[-1]) for a in assignments for n in a['sockets']}
                case['rack_plan']['unused_four_column_candidates']=['SWP2.sp2'+str(i) for i in (1,2,3,4) if i not in columns]
                case['accepted_tradeoffs']=['一套原生公共四列整架允许未分配列空置，不叠第二套空架。','含HVAA借用原生UGB导轨sp33；与完整HVAA网格最近间隙0.0798246资源单位，接受近贴合，不称原生HVAA轨，不添加偏移。']
            cases.append(case)
        out['aircraft'].append({'code':code,'name':anim['name'],'groups':groups,'combinations':cases,'animation_track_comparisons':anim['comparisons'],
                                'integration_gate':{'eligible':False,'meaning':'完整离线方案可进入运行实现；不是已安装/已实机通过'},'runtime_verified':False})
    out['yf23_rack_surface_review']=[inspect(*p) for p in [('native_4AAM','yf23','SWP2','sp21','w_4aam_a0_SM'),
        ('rejected_HVAA_high_4AAM_provider','yf23','SWP2','sp21','w_saam_a0_SM'),('native_HVAA','yf23','SWP1','sp11','w_saam_a0_SM'),
        ('native_UGB_matching_four_column','yf23','SWP3','sp34','w_ugb_a0_SM'),('candidate_HVAA_lower_native_provider','yf23','SWP3','sp33','w_saam_a0_SM')]]
    out['body_surface_review']={code:body_review(animation,code) for code in ('yf23','f22a')}
    out['f22_rack_surface_review']=body_review(animation,'f22a','f22a_SWP3',f22_rack_cases())
    out['xsdb_deployment_review']=xsdb_deployment_review(animated['f22a'])
    out['static_ordnance_meshes']=STATIC_CACHE;out['sources']=assets.used
    out['verification']={'aircraft_count':len(out['aircraft']),'combination_count':sum(len(a['combinations']) for a in out['aircraft']),
                         'source_hashes_rechecked':len(assets.used),'all_component_frames':101,'code_unique':len({a['code'] for a in out['aircraft']})==3}
    classify_results(out)
    out['analyzer_sha256']=hashlib.sha256(Path(__file__).read_bytes()).hexdigest()
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(json.dumps(out,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print('研究摘要：',args.output,flush=True)


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--manifest',type=Path,action='append',required=True)
    parser.add_argument('--batch',type=Path,required=True)
    parser.add_argument('--animation',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--analysis-tools-root',type=Path,help='公共动画分析模块所在目录；缺省为本脚本目录')
    args=parser.parse_args()
    if args.analysis_tools_root:sys.path.insert(0,str(args.analysis_tools_root))
    from analyze_sp_switch_animations import Assets,animation_tracks,poses,angle
    from summarize_mount_assets import properties,resolve
    from survey_aircraft_mounts import reference_bones,rotate,multiply,vector
    from read_cooked_skeletal_mount import read_skeletal_mesh
    from split_static_mount_asset import parse_mesh
    main(args)
