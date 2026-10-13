"""Offline mount/launcher feasibility prototype, not an installable game mod.

Inputs are reduced data from the user's actual Steam assets. No point is moved
arbitrarily, and a partial salvo is never accepted as preserving the weapon.
"""
import itertools
import json
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
def vector(value):
    while 'X' not in value:
        value=next(v for v in value.values()if isinstance(v,dict))
    return tuple(float(value[k])for k in ['X','Y','Z'])
def socket_map(layout,number):
    return {s['SocketName']:vector(s['RelativeLocation'])for s in layout['meshes'][f'su35_SWP{number}_SM']['sockets']}
def loaded(layout,weapon):
    cdo=next(o for o in layout['weapons'][weapon]if o['object'].startswith('Default__'))
    return cdo['properties']['MaxLoadedCount']
def allocate(required,eligible):
    # Exact station keys are rounded source coordinates. Allocation cannot
    # silently reduce loaded count to make an impossible mount fit.
    names=list(required)
    def search(index,occupied,result):
        if index==len(names):return result
        name=names[index]
        for group in itertools.combinations(sorted(eligible[name]),required[name]):
            if occupied.isdisjoint(group):
                match=search(index+1,occupied|set(group),{**result,name:list(group)})
                if match is not None:return match
        return None
    return search(0,set(),{})
def analyze(layout):
    laam,six,lasm=(socket_map(layout,n)for n in [1,2,3])
    normalize=lambda points:{tuple(round(x,4)for x in p)for p in points.values()}
    full={'LAAM':loaded(layout,'plwp_laam_a0'),'6AAM':loaded(layout,'plwp_6aam_a0')}
    direct=allocate(full,{'LAAM':normalize(laam),'6AAM':normalize(six)})
    # Geometry-only candidate: keep 6AAM on its six original wing stations and
    # use the original LASM fuselage pair. Native LASM still has four queue slots.
    fuse={name:p for name,p in lasm.items()if abs(p[1])<20}
    remap={'sp31':'sp33','sp32':'sp34','sp33':'sp33','sp34':'sp34'}
    queue=['sp31','sp32','sp33','sp34'];window=loaded(layout,'plwp_lasm_a0')
    all_windows=[]
    for cursor in range(len(queue)):
        active=[queue[(cursor+i)%len(queue)]for i in range(window)]
        all_windows.append({'cursor':cursor,'native_slots':active,'candidate_physical_slots':[remap[s]for s in active]})
    return {'game_started':False,'candidate_installed':False,
        'direct_overlay':{'laam_6aam_identical_positions':len(normalize(laam)&normalize(six)),
            'required_loaded_count':full,'collision_free_allocation':direct,
            'result':'Rejected: original eligible stations cannot preserve both loaded counts'},
        'six_aam_lasm_geometry_candidate':{'six_aam_wing_sockets':list(six),'lasm_fuselage_sockets':list(fuse),
            'retained_loaded_counts':{'6AAM':loaded(layout,'plwp_6aam_a0'),'LASM':window},
            'native_lasm_queue_slots':queue,'required_native_remap':remap,'queue_windows':all_windows,
            'geometry_only':True,'unimplemented':['native slot transform/socket remap and loaded actor reattachment',
                'persistent inactive missile models synchronized with reload and remaining ammo',
                'combined pylon mesh visibility and lifecycle','firing, resupply and checkpoint validation']},
        'decision':'Do not install a decorative-only implementation; retain DualSP 0.1.5'}
if __name__=='__main__':
    layout=json.loads((ROOT/'evidence/weapon-model-layout.json').read_text('utf-8'))
    result=analyze(layout)
    (ROOT/'evidence/dual-mount-prototype.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
    print(json.dumps(result,indent=2))
