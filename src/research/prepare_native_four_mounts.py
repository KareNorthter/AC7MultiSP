"""四机静态挂载候选：只读已提取资源，生成独立挂架和离线记录；不编译或安装。"""
import argparse
import hashlib
import itertools
import json
from pathlib import Path
import shutil
import subprocess
import sys

from split_static_mount_asset import load_mesh
from summarize_mount_assets import properties
from survey_aircraft_mounts import rotate
from review_animated_us_bays import qrot, separating_margin

ROOT = Path(__file__).resolve().parents[2]
CODES = ('f14a', 'f18e', 'f18etg', 'm31b')


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def make_plans(groups):
    return [None] + [dict(selected=[list(range(len(g['sockets']))) if mask & (1 << k) else []
                                  for k, g in enumerate(groups)], racks=mask, variants=[-1]*3)
                     for mask in range(1, 8)]


def main(args):
    intake = json.loads(args.intake.read_text(encoding='utf-8'))
    recipes = json.loads(args.recipes.read_text(encoding='utf-8'))
    if args.output.exists():
        raise ValueError('新证据路径已存在，不覆盖旧记录')
    output = args.resources.resolve()
    if not output.is_relative_to(ROOT/'Temp') or (output.exists() and not args.resume_resources):
        raise ValueError('资源输出须为当前快照Temp下的新目录')
    output.mkdir(parents=True,exist_ok=args.resume_resources)
    native = {}
    for batch in range(1, 4):
        for sample in json.loads((ROOT/f'evidence/mount-expansion-batch{batch}.json').read_text(encoding='utf-8'))['aircraft']:
            if sample['code'] in CODES:
                native[sample['code']] = sample
    baseline = dict(assets=json.loads((ROOT/'evidence/static-mount-five-aircraft-feedback.json').read_text(encoding='utf-8'))['baseline_assets'])
    assets = list(baseline['assets'])
    reports = []
    for recipe in recipes['variants']:
        source = intake[recipe['source_asset']]
        path = Path(source['json'])
        encrypted = path.parent.parent/path.with_suffix('.uasset').name
        if digest(encrypted) != source['encrypted_sha256']:
            raise ValueError('原生来源指纹变化：'+recipe['source_asset'])
        destination = output/recipe['name']
        if not destination.exists():
            subprocess.run([sys.executable, '-X', 'utf8', str(ROOT/'src/research/split_static_mount_asset.py'),
                            '--source-json', str(path), '--recipe', str(args.recipes.resolve()),
                            '--variant', recipe['name'], '--output', str(destination),
                            '--tools-root', str(args.tools_root.resolve())], check=True)
        report = json.loads((destination/'report.json').read_text(encoding='utf-8'))
        if report['recipe'] != recipe or report['json_sha256'] != digest(path):
            raise ValueError('既有副本的来源/配方不符，不覆盖或重建')
        for suffix in ('.uasset','.uexp'):
            if digest(destination/'encoded'/(recipe['name']+suffix)) != report['encoded_hashes'][suffix]:
                raise ValueError('既有编码资源字节不符')
        report.update(source_pak=source['pak'], source_pak_asset=source['asset'],
                      source_encrypted_uasset_sha256=source['encrypted_sha256'])
        reports.append(report)
        assets.append(recipe['name'])

    planes = []
    sockets = {}
    for code in CODES:
        sample = native[code]
        groups = []
        for group in sample['groups']:
            refs = [r['mesh'] for r in group['weapon_mesh_references']
                    if r['object']=='Static Mesh' and r['mesh'].startswith('w_')]
            fixed = code=='m31b' and group['group']=='SWP3'
            groups.append(dict(class_=group['config']['WeaponClass'], rack=group['rack_mesh'],
                               mesh=None if fixed else refs[0], sockets=[s['name'].lower() for s in group['sockets']],
                               window=group['weapon']['defaults'].get('MaxLoadedCount', {}).get('value', 0),
                               fixed=fixed, launcher=False))
            groups[-1]['class'] = groups[-1].pop('class_')
            sockets[(code, len(groups)-1)] = {s['name'].lower():s for s in group['sockets']}
        plans = make_plans(groups)
        def plan(mask, chosen, racks, variants):
            selected = [[i for i, s in enumerate(g['sockets']) if s in chosen[k]] for k, g in enumerate(groups)]
            if any(len(selected[k]) != len(chosen[k]) for k in range(3)):
                raise ValueError('未知或重复Socket')
            plans[mask] = dict(selected=selected, racks=racks,
                               variants=[assets.index('MSP_'+code+'_'+v) if v else -1 for v in variants])
        if code=='f14a':
            plan(3, [['sp13','sp14','sp15','sp16'],['sp21','sp22'],[]], 3, ['laam_belly',None,None])
            plan(5, [['sp11','sp12'],[],['sp31','sp32','sp33','sp34']], 5, ['laam_wings',None,'sffs_belly'])
            plan(6, [[],['sp21','sp22'],['sp31','sp32','sp33','sp34']], 6, [None,None,'sffs_belly'])
            plan(7, [['sp13','sp14'],['sp21','sp22'],['sp33','sp34']], 7, ['laam_front',None,'sffs_rear'])
        elif code=='f18e':
            plan(3, [['sp11','sp12'],groups[1]['sockets'],[]], 3, ['hcaa_tips',None,None])
            plan(5, [['sp11','sp12'],[],groups[2]['sockets']], 5, ['hcaa_tips',None,None])
            plan(6, [[],['sp23','sp24'],['sp31','sp32']], 6, [None,'agm_inner','lagm_outer'])
            plan(7, [['sp11','sp12'],['sp23','sp24'],['sp31','sp32']], 7, ['hcaa_tips','agm_inner','lagm_outer'])
        elif code=='f18etg':
            plan(3, [['sp11','sp12'],groups[1]['sockets'],[]], 3, ['hcaa_tips',None,None])
            plan(5, [['sp11','sp12'],[],groups[2]['sockets']], 5, ['hcaa_tips',None,None])
            plan(6, [[],['sp21'],['sp32']], 6, [None,'hvaa_neg','gpb_pos_sensor'])
            plan(7, [['sp11','sp12'],['sp21'],['sp32']], 7, ['hcaa_tips','hvaa_neg','gpb_pos_sensor'])
        else:
            plan(3, [groups[0]['sockets'],groups[1]['sockets'],[]], 2, [None,None,None])
            plan(5, [groups[0]['sockets'],[],groups[2]['sockets']], 4, [None,None,None])
            plan(6, [[],[],groups[2]['sockets']], 4, [None,None,None])
            plan(7, [['sp11','sp12'],[],groups[2]['sockets']], 4, [None,None,None])
            for mask in (6,7):
                plans[mask]['borrowed_positions'] = [dict(kind=1,pool=0,socket=i) for i in (2,3)]
        planes.append(dict(code=code, groups=groups, plans=plans, requires_no_collision=code!='m31b'))

    # 所有弹体LOD互配，闭合/展开网格分别筛查；OBB重叠只记待复核，不当作穿模结论。
    cache = {}
    def mesh(name):
        if name not in cache:
            path = Path(intake[name]['json'])
            doc, ex, raw, parsed = load_mesh(path)
            cache[name] = parsed
        return cache[name]
    def obb(name, lod, socket):
        points = mesh(name)['lods'][lod]['points']
        lo = [min(p[i] for p in points) for i in range(3)]
        hi = [max(p[i] for p in points) for i in range(3)]
        q = qrot(socket['serialized_rotation'])
        c = rotate(q, [(a+b)/2 for a,b in zip(lo,hi)])
        center = tuple(a+b for a,b in zip(socket['reference_position'],c))
        return center, tuple(rotate(q, a) for a in ((1,0,0),(0,1,0),(0,0,1))), tuple((b-a)/2 for a,b in zip(lo,hi))
    geometry = []
    for plane in planes:
        code = plane['code']
        for mask in (3,5,6,7):
            p = plane['plans'][mask]
            positions = [(k,k,i) for k,ids in enumerate(p['selected']) for i in ids if not plane['groups'][k]['fixed']]
            positions += [(b['kind'],b['pool'],b['socket']) for b in p.get('borrowed_positions', [])]
            for a,b in itertools.combinations(positions,2):
                if a[0]==b[0]:
                    continue
                ma,mb = [plane['groups'][x[0]]['mesh'] for x in (a,b)]
                sa,sb = [sockets[(code,x[1])][plane['groups'][x[1]]['sockets'][x[2]]] for x in (a,b)]
                if sa['serialized_rotation'] is None or sb['serialized_rotation'] is None:
                    geometry.append(dict(code=code,mask=mask,first=a,second=b,meshes=[ma,mb],
                                         status='原生Socket省略旋转；未猜测默认值',obb_overlap=None))
                    continue
                modes = [(ma,mb)]
                for na,nb in [(ma.replace('_closed_SM','_SM'),mb),(ma,mb.replace('_closed_SM','_SM')),
                              (ma.replace('_closed_SM','_SM'),mb.replace('_closed_SM','_SM'))]:
                    if (na,nb) not in modes and na in intake and nb in intake:
                        modes.append((na,nb))
                for na,nb in modes:
                    gaps=[separating_margin(obb(na,la,sa),obb(nb,lb,sb))
                          for la in range(len(mesh(na)['lods'])) for lb in range(len(mesh(nb)['lods']))]
                    geometry.append(dict(code=code,mask=mask,first=a,second=b,meshes=[na,nb],
                                         lod_pairs=len(gaps), minimum_margin=min(gaps),obb_overlap=min(gaps)<=0))

    # 原生组件的NoCollision只从实际模板字段读取，省略字段不冒充已有证明。
    collision = {}
    for code in CODES:
        doc=json.loads(Path(intake['AcePlayerPawn_'+code]['json']).read_text(encoding='utf-8-sig'))
        collision[code]={e['ObjectName']:properties(e).get('BodyInstance') for e in doc['Exports']
                         if e['ObjectName'] in ('SWP1_GEN_VARIABLE','SWP2_GEN_VARIABLE','SWP3_GEN_VARIABLE')}
    args.output.parent.mkdir(parents=True,exist_ok=True)
    evidence = dict(schema=1,baseline='main/0b75b21/v2.21.1',steam_build=9855922,
                    compiled=False,tests_run=False,installed=False,game_process_access=False,game_verified=False,
                    status='static_candidate_runtime_unverified',baseline_assets=baseline['assets'],
                    appended_assets=assets[len(baseline['assets']):],planes=planes,resource_reports=reports,
                    recipes=recipes,geometry=geometry,component_collision_templates=collision,
                    source_manifest={name:{k:v for k,v in row.items() if k!='json'} for name,row in intake.items()
                                     if name in cache or name in {r['source'] for r in reports} or name.startswith('AcePlayerPawn_')},
                    executed_tool_sources={name:digest(ROOT/name) for name in (
                        'src/research/prepare_native_four_mounts.py','src/research/split_static_mount_asset.py',
                        'src/research/review_animated_us_bays.py','src/research/survey_aircraft_mounts.py')},
                    local_resource_input=output.relative_to(ROOT).as_posix()+'/source',
                    limits=['未编译、安装或实机验证；动态离架、补装、LOD及场景归还待测',
                            'OBB为保守粗筛；重叠需进一步面集复核；展开网格放在挂点不代表真实展开时序',
                            'MiG-31B后腹借位保留原生父链和Socket；不裁PLSL炮体、不改其发射源',
                            '原生省略的碰撞字段不猜值，运行接入仍须NoCollision守卫'])
    deferred = next(p for p in planes if p['code']=='m31b')
    evidence['planes'] = [p for p in planes if p['code']!='m31b']
    evidence['geometry'] = [g for g in geometry if g['code']!='m31b']
    evidence['deferred_designs'] = [dict(code='m31b',status='rejected_belly_borrowing',
        rejected_runtime_plan=deferred,
        reason='原生大尺寸SAAM后腹双挂相互穿插，三挂还与同侧前腹LAAM穿插；不能准入',
        rejected_geometry=[g for g in geometry if g['code']=='m31b'],
        surface_review=json.loads(args.rejected_surface_review.read_text(encoding='utf-8')),
        next_step='另查真实支撑及分侧布局；不裁双PLSL、不靠修改配弹或缩模型绕开净空')]
    source=output/'source';source.mkdir(exist_ok=args.resume_resources)
    for report in reports:
        name=report['recipe']['name']
        for suffix in ('.uasset','.uexp'):
            original=output/name/'encoded'/(name+suffix)
            target=source/(name+suffix)
            if target.exists():
                if digest(target) != digest(original):
                    raise ValueError('资源汇总目录已有不同字节，不覆盖')
            else:
                shutil.copyfile(original,target)
    args.output.write_text(json.dumps(evidence,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(dict(evidence=str(args.output),resources=len(reports),planes=len(evidence['planes']),
                          obb_overlaps=sum(g['obb_overlap'] is True for g in evidence['geometry'])),ensure_ascii=False))


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--intake',type=Path,required=True)
    parser.add_argument('--recipes',type=Path,required=True)
    parser.add_argument('--tools-root',type=Path,required=True)
    parser.add_argument('--resources',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--rejected-surface-review',type=Path,required=True)
    parser.add_argument('--resume-resources',action='store_true',help='只复用配方、来源和编码摘要精确一致的既有副本')
    main(parser.parse_args())
