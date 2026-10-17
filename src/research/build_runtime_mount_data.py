"""将十机完整七组合离线方案编译为运行白名单；不操作游戏。"""
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
CODES = ['f02a','f15c','f15j','f15m','f16c','fa27','mr2k','mrgn','rflm','typn']


def build():
    samples = {}
    for batch in range(1,4):
        for sample in json.loads((ROOT/f'evidence/mount-expansion-batch{batch}.json').read_text(encoding='utf-8'))['aircraft']:
            samples[sample['code']] = sample
    planes, assets = [], []
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
                        if name not in assets:
                            assets.append(name)
                        variants[pool] = assets.index(name)
            elif code=='typn' and mask in (3,5,6,7):
                racks = {3:2,5:4,6:2,7:2}[mask]
            plans[mask] = {'selected':selected,'racks':racks,'variants':variants}
        if any(p is None for p in plans[1:]):
            raise ValueError('必须同时生成整机七组合')
        planes.append({'code':code,'groups':groups,'plans':plans})
    return {'planes':planes,'assets':assets}


if __name__=='__main__':
    data = build()
    encoded = json.dumps(data,ensure_ascii=False,separators=(',',':'))
    header = '// 由 build_runtime_mount_data.py 从七组合研究表生成，禁止手工改选位。\n#pragma once\n'
    header += 'const Json& mountExpansionData() { static const Json data=Json::parse(R"MSP('+encoded+')MSP");return data; }\n'
    (ROOT/'src/mount_expansion_data.hpp').write_bytes(header.encode('utf-8'))
    lua = '-- 独立挂架资源清单；由同一份七组合数据生成。\nreturn {\n'
    for plane in data['planes']:
        ids = sorted({i for p in plane['plans'][1:] for i in p['variants'] if i>=0})
        lua += '    '+plane['code']+'={' + ','.join('{'+str(i)+',"'+data['assets'][i]+'"}' for i in ids) + '},\n'
    lua += '}\n'
    (ROOT/'dist/DualSP/Scripts/mount_assets.lua').write_bytes(lua.encode('utf-8'))
    print('已生成十机70种组合，独立挂架资源',len(data['assets']))
