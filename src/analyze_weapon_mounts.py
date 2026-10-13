"""Reduce original asset inspection to relevant config, socket and class facts."""
import json
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
DIR=ROOT/'Temp/weapon-model-json'
def value(p):
    data=p.get('Value',p.get('EnumValue'))
    if isinstance(data,list):
        if 'ArrayProperty' in p['$type']:return [value(item)for item in data]
        return {item['Name']:value(item)for item in data}
    return data
def props(e):return {p['Name']:value(p)for p in e.get('Data',[])}
def resolve(d,index):
    if index==0:return None
    e=d['Exports'][index-1]if index>0 else d['Imports'][-index-1]
    return e['ObjectName']
def load(name):return json.loads((DIR/(name+'.json')).read_text('utf-8'))

report={'game_started':False,'game_assets_modified':False,'configurations':[],'meshes':{},'weapons':{},'body_sockets':[]}
prefix='pakchunk0-WindowsNoEditor'
pawn=load(prefix+'_0_P-AcePlayerPawn_su35')
activator=next(e for e in pawn['Exports']if e['ObjectName']=='PlayerWeaponActivator')
for key,data in props(activator).items():
    if key.startswith('SpecialWeaponConfig'):
        data['WeaponClassName']=resolve(pawn,data['WeaponClass'])
        report['configurations'].append({'name':key,**data})
for e in pawn['Exports']:
    if e['ObjectName'].startswith('SWP')and 'StaticMesh' in props(e):
        p=props(e);p['mesh_asset']=resolve(pawn,p['StaticMesh'])
        report['meshes'][e['ObjectName']]=p
for number in range(1,4):
    key=f'su35_SWP{number}_SM'
    d=load(prefix+'-'+key)
    sockets=[]
    for e in d['Exports']:
        p=props(e)
        if 'SocketName' in p:sockets.append(p)
    report['meshes'][key]={'sockets':sockets}
d=load(prefix+'-su35_Skeleton')
for e in d['Exports']:
    p=props(e)
    if 'SocketName' in p:report['body_sockets'].append(p)
for name in ['plwp_6aam_a0','plwp_6aam_r0','plwp_lasm_a0','plwp_lasm_r0','plwp_qaam_a0','plwp_laam_a0','plwp_laam_r1']:
    d=load(prefix+'-'+name)
    result=[]
    for e in d['Exports']:
        p=props(e)
        if p:result.append({'object':e['ObjectName'],'class':resolve(d,e['ClassIndex']),'properties':p})
    report['weapons'][name]=result
(ROOT/'evidence/weapon-model-layout.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
print(json.dumps(report['configurations'],indent=2))
for name,mesh in report['meshes'].items():
    if 'sockets' in mesh:print(name,json.dumps(mesh['sockets'],ensure_ascii=False))
    else:print(name,mesh['mesh_asset'])
for name,weapon in report['weapons'].items():
    for obj in weapon:
        if 'Parameters' in obj['class']:print(name,obj['class'],json.dumps(obj['properties'],ensure_ascii=False))
