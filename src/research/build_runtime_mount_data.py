"""将完整七组合离线方案编译为静态运行白名单；不操作游戏。"""
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
CODES = ['f02a','f15c','f15j','f15m','f16c','fa27','mr2k','mrgn','rflm','typn','su35','su37','f04e','f14atg','j39e','su30','su34','m29a','f16x','m35d']
LAYOUT_REVIEW = 'evidence/static-mount-five-aircraft-review.json'
LAYOUT_FEEDBACK = 'evidence/static-mount-five-aircraft-feedback.json'


def apply_layout_review(planes, assets, source, targets):
    """按精确旧方案应用修订；资源可只复用，旧证据和其余组合不回改。"""
    review = json.loads((ROOT/source).read_text(encoding='utf-8'))
    if review['schema'] != 1 or assets != review['baseline_assets']:
        raise ValueError('五机复核的旧资源基线不符，须先复核差异')
    appended = review['appended_assets']
    reports = {r['recipe']['name']: r for r in review['resource_reports']}
    if (len(set(assets + appended)) != len(assets) + len(appended) or
            len(reports) != len(review['resource_reports']) or set(appended) != set(reports)):
        raise ValueError('五机复核的新增资源清单无效')
    for name, report in reports.items():
        if not all(report.get(k) is True for k in (
                'asset_roundtrip', 'renamed_ac7_roundtrip', 'all_exports_payload_preserved')):
            raise ValueError('新增挂架缺少完整资源往返依据：'+name)
    assets.extend(appended)
    by_code = {p['code']: p for p in planes}
    seen = set()
    for item in review['plan_overrides']:
        code, mask = item['code'], item['mask']
        if (code, mask) in seen or (code, mask) not in targets:
            raise ValueError('五机复核存在重复或超范围组合')
        seen.add((code, mask))
        plane = by_code[code]
        if plane['plans'][mask] != item['expected_plan']:
            raise ValueError('五机复核的旧布局已变化：'+code+'/'+str(mask))
        if len(item['selected_sockets']) != 3 or len(item['variants']) != 3:
            raise ValueError('挂载修订必须明确三组选择及载体')
        selected, variants = [], []
        for pool, group in enumerate(plane['groups']):
            wanted = item['selected_sockets'][pool]
            if (len(set(wanted)) != len(wanted) or not set(wanted) <= set(group['sockets']) or
                    bool(wanted) != bool(mask & (1 << pool))):
                raise ValueError('双挂修订含未知、重复或未选兵装Socket')
            selected.append([i for i, socket in enumerate(group['sockets']) if socket in wanted])
            name = item['variants'][pool]
            if name is not None:
                report = reports[name]
                if (not wanted or report['recipe']['aircraft'] != code or
                        report['recipe']['source_asset'] != group['rack']):
                    raise ValueError('双挂副本必须属于该兵装原生挂架')
            variants.append(assets.index(name) if name is not None else -1)
        if item['racks'] != mask:
            raise ValueError('本轮挂载只显示已选兵装的原组挂架')
        # 首次使用派生挂架时沿用静态分区守卫；纯原生补位不新增碰撞配置要求。
        if any(name is not None for name in item['variants']):
            plane['requires_no_collision'] = True
        plane['plans'][mask] = {'selected':selected, 'racks':item['racks'], 'variants':variants}
    if seen != targets:
        raise ValueError('五机复核缺少已登记的挂载修订')


def build():
    samples = {}
    for batch in range(1,4):
        for sample in json.loads((ROOT/f'evidence/mount-expansion-batch{batch}.json').read_text(encoding='utf-8'))['aircraft']:
            samples[sample['code']] = sample
    # 新批次独立留证，不改旧资源/工具执行回执的历史含义。
    for sample in json.loads((ROOT/'evidence/static-expansion-2.11.0.json').read_text(encoding='utf-8'))['aircraft']:
        samples[sample['code']] = sample
    native_overrides = []
    for name in ('static-expansion-2.12.0.json', 'static-expansion-2.13.0.json', 'static-expansion-2.14.0.json'):
        feedback = json.loads((ROOT/'evidence'/name).read_text(encoding='utf-8'))
        for sample in feedback['aircraft']:
            samples[sample['code']] = sample
        native_overrides.extend(feedback.get('native_carrier_overrides', []))
    # 旧资源即使本版不再加载，也保留原编号与成套字节；只在末尾增加本轮副本。
    assets = list(feedback['baseline_assets'])
    appended = feedback['appended_assets']
    if len(set(assets + appended)) != len(assets) + len(appended):
        raise ValueError('资源编号基线或新增清单重复')
    if set(appended) != {r['recipe']['name'] for r in feedback['resource_reports']}:
        raise ValueError('新增资源清单与逐资源回执不符')
    allowed_assets = set(assets + appended)
    for override in native_overrides:
        sample = samples[override['code']]
        singles = [c for c in sample['proposal']['combinations'] if len(c['weapons']) == 1]
        weapons = [c['weapons'][0] for c in singles]
        group = 'SWP' + str(override['pool'])
        if override['mode'] != 'native' or not 1 <= override['pool'] <= 3:
            raise ValueError('原生挂架覆盖参数无效')
        found = set()
        for combo in sample['proposal']['combinations']:
            mask = sum(1 << weapons.index(w) for w in combo['weapons'])
            if mask not in override['masks']:
                continue
            assignment = next(a for a in combo['assignments'] if a['group'] == group)
            if assignment['sockets'] != override['expected_selected_sockets']:
                raise ValueError('恢复原生挂架不能改变既有弹位')
            carrier = next(c for c in combo['rack_plan']['carriers'] if c['group'] == group)
            if carrier.get('variant') != override['previous_variant']:
                raise ValueError('待恢复的派生挂架与覆盖依据不符')
            carrier['mode'] = 'native'
            carrier.pop('variant', None)
            carrier.pop('package', None)
            found.add(mask)
        if found != set(override['masks']):
            raise ValueError('原生挂架覆盖遗漏组合')
    planes = []
    for code in CODES:
        sample = samples[code]
        if not sample['integration_gate']['eligible']:
            raise ValueError('整机七组合门槛未通过：'+code)
        proposal = sample['proposal']
        singles = [c for c in proposal['combinations'] if len(c['weapons'])==1]
        weapons = [c['weapons'][0] for c in singles]
        groups = []
        for index, group in enumerate(sample['groups']):
            assignment = singles[index]['assignments'][0]
            mesh = next((x['mesh'] for x in group['weapon_mesh_references'] if x['object']=='Static Mesh'),None)
            fixed = assignment['position_role'] not in ('ordnance_station_candidate','launcher_muzzle')
            if fixed and (not mesh or not mesh.startswith('w_')):
                mesh = None
            groups.append({'class':group['config']['WeaponClass'], 'rack':group['rack_mesh'], 'mesh':mesh,
                           'sockets':[x['name'].lower() for x in group['sockets']],
                           'window':group['weapon']['defaults'].get('MaxLoadedCount',{}).get('value',0),
                           'fixed':fixed, 'launcher':assignment['position_role']=='launcher_muzzle'})
        plans = [None]*8
        for combo in proposal['combinations']:
            mask = sum(1<<weapons.index(w) for w in combo['weapons'])
            selected = [[],[],[]]
            for assignment in combo['assignments']:
                pool = int(assignment['group'][-1])-1
                wanted = {x.lower() for x in assignment['sockets']}
                selected[pool] = [i for i,s in enumerate(groups[pool]['sockets']) if s in wanted]
                if len(selected[pool])!=len(wanted):
                    raise ValueError('方案中存在未知或重复Socket')
            racks, variants = mask, [-1]*3
            rack_plan = combo['rack_plan']
            if isinstance(rack_plan,dict) and len(combo['weapons'])>1:
                racks = 0
                for carrier in rack_plan['carriers']:
                    pool = int(carrier['group'][-1])-1
                    if carrier['mode']!='hidden_self':
                        racks |= 1<<pool
                    if carrier['mode']=='split_variant':
                        name = carrier['variant']
                        if name not in allowed_assets:
                            raise ValueError('方案引用了未登记资源：' + name)
                        if name not in assets:
                            assets.append(name)
                        variants[pool] = assets.index(name)
            elif code=='typn' and mask in (3,5,6,7):
                racks = {3:2,5:4,6:2,7:2}[mask]
            plan = {'selected':selected,'racks':racks,'variants':variants}
            # 可选借位只使用已登记原生池/Socket；旧方案没有此字段，仍走原路径。
            borrowed = combo.get('borrowed_positions', [])
            if borrowed:
                seen = set()
                for item in borrowed:
                    if set(item) != {'kind', 'pool', 'socket'} or any(type(v) is not int for v in item.values()):
                        raise ValueError('借位必须明确逻辑kind、原生pool及Socket序号')
                    kind, pool, socket = item['kind'], item['pool'], item['socket']
                    if not (0 <= kind < 3 and 0 <= pool < 3 and kind != pool and
                            0 <= socket < len(groups[pool]['sockets'])):
                        raise ValueError('借位原生池或Socket越界')
                    if not selected[kind] or selected[pool] or not (racks & (1 << pool)):
                        raise ValueError('借位要求逻辑兵装已选、提供池未选且挂架可见')
                    key = (pool, socket)
                    if key in seen:
                        raise ValueError('借位不能重复使用同一原生Socket')
                    seen.add(key)
                plan['borrowed_positions'] = borrowed
            plans[mask] = plan
        if any(p is None for p in plans[1:]):
            raise ValueError('必须同时生成整机七组合')
        plane={'code':code,'groups':groups,'plans':plans}
        if sample.get('requires_no_collision'):
            plane['requires_no_collision']=True
        planes.append(plane)
    if assets[len(feedback['baseline_assets']):] != appended:
        raise ValueError('新增资源编号顺序与追加清单不符')
    apply_layout_review(planes, assets, LAYOUT_REVIEW, {('m35d',3),('m35d',5),('m35d',6),('fa27',3)})
    apply_layout_review(planes, assets, LAYOUT_FEEDBACK, {('f15m',3),('f15m',7)})
    if __package__:
        from .native_static_batch_data import append_native_static_batch
    else:
        from native_static_batch_data import append_native_static_batch
    append_native_static_batch(planes, assets, ROOT)
    return {'planes':planes,'assets':assets}


if __name__=='__main__':
    data = build()
    encoded = json.dumps(data,ensure_ascii=False,separators=(',',':'))
    header = '// 由 build_runtime_mount_data.py 从七组合研究表生成，禁止手工改选位。\n#pragma once\n'
    header += 'const Json& mountExpansionData() { static const Json data=Json::parse(R"MSP('+encoded+')MSP");return data; }\n'
    (ROOT/'src/mount_expansion_data.hpp').write_bytes(header.encode('utf-8'))
    lua = '-- 独立挂架资源清单；由同一份七组合数据生成。\nreturn {\n'
    for plane in data['planes']:
        entries = sorted({(i, pool + 1) for p in plane['plans'][1:]
                          for pool, i in enumerate(p['variants']) if i >= 0})
        def entry(i, pool):
            # 第四项按组合mask登记使用范围，单挂无需加载派生资源。
            masks = sum(1 << mask for mask, plan in enumerate(plane['plans'])
                        if mask and plan['variants'][pool-1] == i)
            return '{'+str(i)+',"'+data['assets'][i]+'",'+str(pool)+','+str(masks)+'}'
        lua += '    '+plane['code']+'={' + ','.join(entry(i, pool) for i, pool in entries) + '},\n'
    lua += '}\n'
    (ROOT/'dist/DualSP/Scripts/mount_assets.lua').write_bytes(lua.encode('utf-8'))
    print('已生成静态机体',len(data['planes']),'完整组合',len(data['planes'])*7,'独立挂架资源',len(data['assets']))
