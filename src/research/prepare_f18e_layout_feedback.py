"""按用户反馈生成两机四组合的资源与静态依据；不编译、安装或操作游戏。"""
import argparse
import base64
import copy
import hashlib
import itertools
import json
from pathlib import Path
import re
import shutil
import struct
import subprocess
import sys

from split_static_mount_asset import load_mesh, run_converter, encode_ac7_pair
from inspect_mount_assets import decode_copy
from survey_aircraft_mounts import rotate
from review_animated_us_bays import qrot, separating_margin

ROOT = Path(__file__).resolve().parents[2]
NAMES = ['MSP_f18e_agm_outer_r1', 'MSP_f18e_lagm_inner_r1', 'MSP_f18etg_gpb_inner_whole_r1']


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def write(path, data):
    path.write_bytes((json.dumps(data, ensure_ascii=False, indent=2)+'\n').encode('utf-8'))


def collision_entry(source, candidate, report):
    a, ae, ar, _ = load_mesh(source)
    b, be, br, _ = load_mesh(candidate)
    if ar[6:14] != br[6:14]:
        raise ValueError('碰撞引用变化')
    body_index, nav_index = struct.unpack_from('<2i', ar, 6)
    keys = ('ObjectName', 'Data', 'Extras', 'ClassIndex', 'OuterIndex', 'TemplateIndex', 'SuperIndex')
    for index in (body_index, nav_index):
        if index <= 0 or any(a['Exports'][index-1].get(k) != b['Exports'][index-1].get(k) for k in keys):
            raise ValueError('完整碰撞/导航子对象未保持')
    # TGM只允许替换材质导入；碰撞子对象引用的引擎导入逐项保持。
    if any(a['Imports'][i] != b['Imports'][i] for i in range(len(a['Imports'])) if i not in (5, 7)):
        raise ValueError('非材质导入变化')
    body = a['Exports'][body_index-1]
    match = re.fullmatch(r'(.+)_([0-9]+)', body['ObjectName'])
    if not match or a['Imports'][-body['ClassIndex']-1]['ObjectName'] != 'BodySetup':
        raise ValueError('碰撞身份不完整')
    code = report['recipe']['aircraft']
    return dict(asset=report['recipe']['name'], aircraft=code, source=source.stem,
                source_package=f'/Game/Vehicles/Aircraft/{code}/{source.stem}',
                body_name=match[1], body_number=int(match[2])+1,
                body_guid=list(base64.b64decode(body['Extras'])[:16]),
                collision_references_unchanged=True, complete_body_and_nav_exports_unchanged=True,
                body_export_sha256=hashlib.sha256(json.dumps(body, sort_keys=True).encode()).hexdigest(),
                nav_export_sha256=hashlib.sha256(json.dumps(a['Exports'][nav_index-1], sort_keys=True).encode()).hexdigest(),
                source_json_sha256=digest(source), candidate_json_sha256=digest(candidate),
                encoded_hashes=report['encoded_hashes'])


def whole_rack(source, donor, output, tools):
    """借用普通E完整4AGM挂架：全部LOD/索引/材质原样；保留TGM碰撞及两个Socket身份。"""
    name = NAMES[2]
    original, ex, raw, mesh = load_mesh(source)
    dd, de, dr, dm = load_mesh(donor)
    if mesh['socket_count'] != 2 or dm['socket_count'] != 4:
        raise ValueError('供受挂架Socket结构改变')
    result = copy.deepcopy(original)
    target = next(e for e in result['Exports'] if e['ObjectName'] == source.stem)
    # cooked头为30字节，再接本地Socket引用；不搬入供体BodySetup或Socket引用。
    prefix = 30+4*mesh['socket_count']
    donor_prefix = 30+4*dm['socket_count']
    changed_raw = raw[:prefix]+dr[donor_prefix:]
    target['Extras'] = base64.b64encode(changed_raw).decode('ascii')
    fields = {p['Name']: p for p in target['Data']}
    for prop in de['Data']:
        if prop['Name'] in ('StaticMaterials', 'LightmapUVDensity', 'LightMapResolution', 'ExtendedBounds'):
            fields[prop['Name']].update(copy.deepcopy(prop))
    for i in (5, 7):
        result['Imports'][i] = copy.deepcopy(dd['Imports'][i])
    for text in dd['NameMap']:
        if text not in result['NameMap']:
            result['NameMap'].append(text)
    changes = []
    # 同一整机坐标中，按普通E的内外侧原生站距平移，保留GPB相对安装基准与旋转。
    delta = (2.11085, 11.78657, .40082)
    for item in result['Exports']:
        props = {p['Name']: p for p in item.get('Data', [])}
        socket = props.get('SocketName', {}).get('Value')
        if socket not in ('sp31', 'sp32'):
            continue
        loc = props['RelativeLocation']['Value'][0]['Value']
        before = {k: float(loc[k]) for k in ('X', 'Y', 'Z')}
        for axis, value in zip(('X', 'Y', 'Z'), (delta[0], delta[1] if before['Y'] < 0 else -delta[1], delta[2])):
            loc[axis] = struct.unpack('<f', struct.pack('<f', before[axis]+value))[0]
        changes.append(dict(socket=socket, before=before, after=dict(loc)))
    if len(changes) != 2:
        raise ValueError('GPB两个Socket缺失')
    own = [s for s in original['NameMap'] if s.endswith('/'+source.stem)]
    if len(own) != 1:
        raise ValueError('资源包身份不唯一')
    package = '/Game/AC7MultiSP/Research/'+name
    result['NameMap'] = [name if s == source.stem else package if s == own[0] else s for s in result['NameMap']]
    target['ObjectName'] = name
    output.mkdir()
    converted = output/(name+'.json'); write(converted, result)
    asset = output/(name+'.uasset')
    run_converter(tools, ['fromjson', converted, asset], asset)
    reread = output/'roundtrip'/(name+'.json'); reread.parent.mkdir()
    run_converter(tools, ['tojson', asset, reread, 'VER_UE4_18'], reread)
    actual, _, actual_raw, actual_mesh = load_mesh(reread)
    if actual['Imports'] != result['Imports'] or actual['NameMap'] != result['NameMap'] or actual_raw != changed_raw:
        raise ValueError('完整挂架回读改变导入、命名或几何')
    for a, b in zip(result['Exports'], actual['Exports'], strict=True):
        if any(a.get(k) != b.get(k) for k in ('ObjectName','Data','Extras','OuterIndex','ClassIndex','SuperIndex','TemplateIndex')):
            raise ValueError('候选导出回读变化')
    if actual_raw[prefix:] != dr[donor_prefix:]:
        raise ValueError('供体完整LOD、索引或尾部字节被改变')
    key = (tools/'assets/AC7Key.bin').read_bytes()
    for p in (source, donor):
        encoded = p.parent.parent/p.with_suffix('.uasset').name
        pair = encode_ac7_pair(p.with_suffix('.uasset').read_bytes(), p.with_suffix('.uexp').read_bytes(), key, p.stem)
        if pair != (encoded.read_bytes(), encoded.with_suffix('.uexp').read_bytes()):
            raise ValueError('原生来源编码无法逐字节复原')
    encoded = output/'encoded'; encoded.mkdir()
    pair = encode_ac7_pair(asset.read_bytes(), asset.with_suffix('.uexp').read_bytes(), key, name)
    target_asset = encoded/asset.name
    target_asset.write_bytes(pair[0]); target_asset.with_suffix('.uexp').write_bytes(pair[1])
    decoded = decode_copy(target_asset, key)
    if any(decoded.with_suffix(s).read_bytes() != asset.with_suffix(s).read_bytes() for s in ('.uasset', '.uexp')):
        raise ValueError('独立解码回读不符')
    return dict(source=source.stem, json_sha256=digest(source),
                recipe=dict(name=name, aircraft='f18etg', source_asset=source.stem, whole_geometry_donor=donor.stem,
                            whole_geometry_donor_json_sha256=digest(donor)),
                asset_roundtrip=True, renamed_ac7_roundtrip=True, all_exports_payload_preserved=True,
                source_ac7_encoding_reproduced=True, complete_donor_geometry_bytes_preserved=True,
                donor_lods=len(actual_mesh['lods']), donor_triangle_counts=[len(l['buffers'][0]['values'])//3 for l in dm['lods']],
                donor_materials_preserved=True, collision_and_navigation_preserved=True,
                socket_changes=changes, socket_rotations_and_scales_preserved=True,
                encoded_hashes={s:digest(encoded/(name+s)) for s in ('.uasset','.uexp')})


def main(args):
    if args.output.exists() or args.evidence.exists() or not args.output.resolve().is_relative_to(ROOT/'Temp'):
        raise ValueError('只写本工作树Temp内新目录及新依据，不覆盖旧结果')
    sources = {}
    for manifest in args.manifest:
        for item in json.loads(manifest.read_text(encoding='utf-8-sig')):
            path = Path(item['json'])
            if path.is_file():
                sources.setdefault(path.stem, path)
    args.output.mkdir(parents=True)
    recipes = dict(variants=[])
    for name, source, boxes in (
            (NAMES[0], 'f18e_SWP2_SM', [[-1000,-1000,-1000,1000,-30,1000],[ -1000,30,-1000,1000,1000,1000]]),
            (NAMES[1], 'f18e_SWP3_SM', [[-1000,-30,-1000,1000,30,1000]])):
        _, _, raw, _ = load_mesh(sources[source])
        recipes['variants'].append(dict(name=name, aircraft='f18e', source_asset=source,
                                       source_extras_sha256=hashlib.sha256(raw).hexdigest(), keep_boxes=boxes,
                                       reason='用户反馈：较大的LAGM改内侧，4AGM改外侧；保留原生各自安装点与支撑'))
    recipe_path = args.output/'recipes.json'; write(recipe_path, recipes)
    reports = []
    for recipe in recipes['variants']:
        name = recipe['name']
        subprocess.run([sys.executable, '-X', 'utf8', str(ROOT/'src/research/split_static_mount_asset.py'),
                        '--source-json', str(sources[recipe['source_asset']]), '--recipe', str(recipe_path),
                        '--variant', name, '--output', str(args.output/name), '--tools-root', str(args.tools_root)], check=True)
        reports.append(json.loads((args.output/name/'report.json').read_text(encoding='utf-8')))
    reports.append(whole_rack(sources['f18etg_SWP3_SM'], sources['f18e_SWP2_SM'], args.output/NAMES[2], args.tools_root))
    collision = [collision_entry(sources[r['source']], args.output/r['recipe']['name']/'roundtrip'/(r['recipe']['name']+'.json'), r) for r in reports]
    baseline_header = subprocess.check_output(['git','show',args.baseline+':src/mount_expansion_data.hpp'],cwd=ROOT).decode('utf-8')
    baseline = json.loads(baseline_header.split('R"MSP(',1)[1].split(')MSP"',1)[0])
    if len(baseline['assets']) != 74:
        raise ValueError('研究资源基线必须为2.25的74项')
    changes = []
    for code in ('f18e', 'f18etg'):
        plane = next(p for p in baseline['planes'] if p['code'] == code)
        for mask in (6, 7):
            if code == 'f18e':
                plan = dict(selected=[[0,1] if mask == 7 else [], [0,1], [2,3]], racks=mask,
                            variants=[59 if mask == 7 else -1,74,75])
            else:
                plan = dict(selected=[[], [0,1] if mask == 6 else [0], [0,1]], racks=4, variants=[-1,-1,76])
                if mask == 7:
                    plan.update(full_borrowed_kinds=1, borrowed_positions=[dict(kind=0,pool=1,socket=1)])
            changes.append(dict(code=code, mask=mask, previous=plane['plans'][mask], replacement=plan))
    # 对四组合全部待挂弹体作全LOD包围体筛查；GPB展开版另记为保守假设。
    sockets = {}
    for batch in range(1,4):
        for sample in json.loads((ROOT/f'evidence/mount-expansion-batch{batch}.json').read_text(encoding='utf-8'))['aircraft']:
            if sample['code'] in ('f18e','f18etg'):
                for k,g in enumerate(sample['groups']):
                    sockets[sample['code'],k] = copy.deepcopy(g['sockets'])
    for c in reports[2]['socket_changes']:
        s = next(s for s in sockets['f18etg',2] if s['name'] == c['socket'])
        s['reference_position'] = [c['after'][k] for k in ('X','Y','Z')]
    meshes = {}
    def obbs(mesh_name, socket):
        if mesh_name not in meshes:
            meshes[mesh_name] = load_mesh(sources[mesh_name])[3]
        q = qrot(socket['serialized_rotation'])
        result = []
        for lod in meshes[mesh_name]['lods']:
            lo = [min(p[i] for p in lod['points']) for i in range(3)]
            hi = [max(p[i] for p in lod['points']) for i in range(3)]
            center = rotate(q, [(a+b)/2 for a,b in zip(lo,hi)])
            result.append((tuple(a+b for a,b in zip(socket['reference_position'],center)),
                           tuple(rotate(q,v) for v in ((1,0,0),(0,1,0),(0,0,1))), tuple((b-a)/2 for a,b in zip(lo,hi))))
        return result
    geometry = []
    for change in changes:
        code, plan = change['code'], change['replacement']
        plane = next(p for p in baseline['planes'] if p['code']==code)
        positions = [(k,k,i) for k, selected in enumerate(plan['selected']) for i in selected]
        positions += [(x['kind'],x['pool'],x['socket']) for x in plan.get('borrowed_positions',[])]
        for a,b in itertools.combinations(positions,2):
            ma, mb = [plane['groups'][p[0]]['mesh'] for p in (a,b)]
            sa, sb = [sockets[code,p[1]][p[2]] for p in (a,b)]
            modes = [(ma,mb)]
            if code=='f18etg':
                expanded = tuple(n.replace('_closed_SM','_SM') for n in (ma,mb))
                if expanded != (ma,mb): modes.append(expanded)
            for na,nb in modes:
                gaps = [separating_margin(x,y) for x in obbs(na,sa) for y in obbs(nb,sb)]
                geometry.append(dict(code=code,mask=change['mask'],first=a,second=b,meshes=[na,nb],
                                     lod_pairs=len(gaps),minimum_margin=min(gaps),obb_overlap=min(gaps)<=0,
                                     expanded_at_mount_hypothesis=(na,nb)!=(ma,mb)))
    source_dir = args.output/'source'; source_dir.mkdir()
    for r in reports:
        name = r['recipe']['name']
        for suffix in ('.uasset','.uexp'):
            shutil.copyfile(args.output/name/'encoded'/(name+suffix), source_dir/(name+suffix))
    result = dict(schema=1,status='user_requested_layout_runtime_candidate',baseline_commit=args.baseline,
                  baseline_assets=baseline['assets'],appended_assets=NAMES,changes=changes,
                  resource_reports=reports,collision_preservation=collision,geometry=geometry,
                  source_fingerprints={n:dict(json_sha256=digest(sources[n])) for n in sorted(set(meshes)|{'f18e_SWP2_SM','f18e_SWP3_SM','f18etg_SWP3_SM'})},
                  compiled=False,tests_run=False,installed=False,game_process_access=False,
                  limits=['只修改两机mask6/7，单挂及其他双挂不变', 'TGM整套借普通E的4AGM挂架，三个LOD全部几何原样，不拆分、不缩放',
                          'GPB只平移原生两个挂点至内侧，保留旋转；三挂HCAA借外侧一个HVAA点',
                          '供体与TGM机身对接、弹体与挂架/机身净空、实际离架及显隐归还仍需用户观察',
                          'OBB重叠不等于模型穿插，展开网格放在挂点不代表真实展开时序'])
    write(args.evidence,result)
    print(json.dumps(dict(resources=3,changed_combinations=4,obb_overlaps=sum(x['obb_overlap'] for x in geometry)),ensure_ascii=False))


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--manifest',type=Path,action='append',required=True)
    p.add_argument('--output',type=Path,required=True)
    p.add_argument('--evidence',type=Path,required=True)
    p.add_argument('--tools-root',type=Path,required=True)
    p.add_argument('--baseline',required=True)
    main(p.parse_args())
