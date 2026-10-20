"""把已交付四机活动挂架方案接为源码候选，生成离线静态副本；不编译、打包或连接游戏。"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys

from analyze_sp_switch_animations import Assets
from survey_aircraft_mounts import reference_bones

ROOT = Path(__file__).resolve().parents[2]
CODES = ('f14d', 'f18f', 'su33', 'f18x')


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main(args):
    design_path = ROOT/'evidence/active-five-mount-static-plan.json'
    design = json.loads(design_path.read_text(encoding='utf-8'))
    # 原静态方案和观察回执只读，新接线单独留证。
    observed = json.loads((ROOT/'evidence/m31b-mount-observation.json').read_text(encoding='utf-8'))
    baseline = observed['baseline_assets']
    assets = Assets(args.manifest)
    output = args.output.resolve()
    if not output.is_relative_to(ROOT/'Temp') or (output.exists() and not args.resume) or args.evidence.exists():
        raise ValueError('输出必须是本工作树Temp内的新目录，不能覆盖旧资源或回执')
    output.mkdir(parents=True, exist_ok=args.resume)
    resources = output/'source'
    resources.mkdir(exist_ok=args.resume)
    for name in baseline:
        for suffix in ('.uasset', '.uexp'):
            before, target = args.baseline_resources/(name+suffix), resources/(name+suffix)
            if target.exists():
                if digest(before) != digest(target):
                    raise ValueError('已生成目录的旧资源变化，不覆盖：'+target.name)
            else:
                shutil.copy2(before, target)
    appended, reports, planes = [], [], []
    for code in CODES:
        source = next(p for p in design['aircraft'] if p['code'] == code)
        variants = {}
        for label, part in source['static_partition_reports'].items():
            if part['status'] == 'native_whole_already_in_requested_region':
                continue
            if part['status'] != 'all_lods_partitionable_not_generated':
                raise ValueError('静态分区资格未知：'+code+'/'+label)
            native = part['source']
            assets.load(native)
            if assets.used[native.casefold()] != design['sources'][native.casefold()]:
                raise ValueError('分区来源与旧方案不同：'+native)
            entry = next(x for x in assets.entries[native.casefold()]
                         if x['pak'] == assets.used[native.casefold()]['pak'])
            recipe = {'name': 'MSP_'+code+'_'+label, 'source_asset': native,
                      'source_extras_sha256': part['source_extras_sha256'], 'keep_boxes': part['keep_boxes']}
            recipe_path = output/(recipe['name']+'-recipe.json')
            recipe_path.write_text(json.dumps(recipe, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
            destination = output/recipe['name']
            if not destination.exists():
                subprocess.run([sys.executable, '-X', 'utf8', str(ROOT/'src/research/split_static_mount_asset.py'),
                                '--source-json', entry['json'], '--recipe', str(recipe_path), '--output', str(destination),
                                '--tools-root', str(args.tools_root.resolve())], check=True)
            report = json.loads((destination/'report.json').read_text(encoding='utf-8'))
            if report['recipe'] != recipe or report['json_sha256'] != digest(Path(entry['json'])) or not all(
                    report.get(k) is True for k in ('asset_roundtrip', 'renamed_ac7_roundtrip', 'all_exports_payload_preserved')):
                raise ValueError('已有副本的来源、配方或回读依据不符：'+recipe['name'])
            reports.append(report)
            appended.append(recipe['name'])
            variants[label] = (part['pool'], len(baseline)+len(appended)-1)
            for suffix in ('.uasset', '.uexp'):
                encoded = destination/'encoded'/(recipe['name']+suffix)
                if digest(encoded) != report['encoded_hashes'][suffix]:
                    raise ValueError('派生资源与回执字节不符：'+encoded.name)
                target = resources/encoded.name
                if target.exists() and digest(target) != digest(encoded):
                    raise ValueError('已生成目录的派生资源变化，不覆盖：'+target.name)
                if not target.exists():
                    shutil.copy2(encoded, target)
        groups = []
        for pool, original in enumerate(source['groups']):
            fixed = code in ('f18f', 'su33') and pool == 2
            meshes = [x['mesh'] for x in original['weapon_mesh_references']
                      if x['object'] == 'Static Mesh' and x['mesh'].startswith('w_')]
            group = {'class': original['config']['WeaponClass'], 'rack': original['rack_mesh'],
                     'mesh': None if fixed else meshes[0], 'sockets': [s['name'].lower() for s in original['sockets']],
                     'window': original['weapon']['defaults'].get('MaxLoadedCount', {}).get('value', 0),
                     'fixed': fixed, 'launcher': False, 'skeletal': pool == 2}
            if pool == 2:
                name = code+'_SWP3_Skeleton'
                document = assets.load(name)
                if assets.used[name.casefold()] != design['sources'][name.casefold()]:
                    raise ValueError('骨架来源与旧方案不同：'+name)
                bones = reference_bones(document, next(e for e in document['Exports'] if e['ObjectName'] == name))
                group.update(skeleton=name, anim=source['bindings'][pool]['anim_class'],
                             bones=[{'name': b['name'], 'parent': b['parent']} for b in bones])
            groups.append(group)
        plans = [None]*8
        for combo in source['combinations']:
            mask = combo['mask']
            selected, chosen = [[], [], []], [-1]*3
            for assignment in combo['assignments']:
                pool = int(assignment['group'][-1])-1
                selected[pool] = [groups[pool]['sockets'].index(s.lower()) for s in assignment['sockets']]
            for label in combo['static_partitions']:
                if label in variants:
                    pool, index = variants[label]
                    chosen[pool] = index
            plans[mask] = {'selected': selected, 'racks': mask, 'variants': chosen}
        planes.append({'code': code, 'groups': groups, 'plans': plans,
                       'requires_no_collision': True, 'local_mechanism': True})
    receipt = {'schema': 1, 'status': 'runtime_source_candidate_not_validated', 'steam_build': 9855922,
               'static_design_sha256': digest(design_path), 'baseline_assets': baseline,
               'appended_assets': appended, 'resource_reports': reports, 'planes': planes,
               'resource_input': str(resources), 'compiled': False, 'tests_run': False,
               'installed': False, 'game_process_access': False, 'assets_generated': True,
               'limits': ['保留原生公共spwc及SWP3局部动画，不接入旧完整共舱控制器',
                          'Block III仍待实测上层朝向及LACM实际展开时序，不把包络重叠认作穿模',
                          'MiG-31B等待用户重试日志，本次不变更其观察布局或保护条件']}
    args.evidence.write_text(json.dumps(receipt, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
    print('四机源码方案', len(planes), '新增资源', len(appended), '资源输入', resources)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--manifest', type=Path, action='append', required=True)
    parser.add_argument('--baseline-resources', type=Path, required=True)
    parser.add_argument('--tools-root', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--evidence', type=Path, required=True)
    parser.add_argument('--resume', action='store_true', help='核对后复用已生成资源，不覆盖差异')
    main(parser.parse_args())
