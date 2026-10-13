import json
import sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
d=json.loads((ROOT/'Temp/hud-assets'/ (sys.argv[1] if len(sys.argv)>1 else 'AceMainFlightHUD.json')).read_text('utf-8'))
def simple(value):
    if isinstance(value,list):
        return {v['Name']: simple(v.get('Value',v.get('CultureInvariantString'))) for v in value}
    if isinstance(value,dict):
        return {k:simple(v) for k,v in value.items() if k!='$type'}
    return value
def props(e):return simple(e.get('Data',[]))
def resolve(index):
    if index==0:return None
    e=d['Exports'][index-1] if index>0 else d['Imports'][-index-1]
    return f"{index}:{e['ObjectName']}"
names={'SpecialWeaponTextBP','SpecialWeaponCountTextBP','MainWeaponTextBP','MainWeaponCountTextBP','NormalWeaponSel','SpecialWeaponSel','FlareTextBP','FlareCountTextBP','DamageTextBP','DamageRateTextBP','Weapons','GunTextBP','GunCountTextBP','WeaponMessagePanel'}
report=[]
for i,e in enumerate(d['Exports']):
    if e['ObjectName'] in names and e['ClassIndex'] in [-30,-40]:
        data=props(e)
        slot=d['Exports'][data['Slot']-1] if data.get('Slot') else {}
        report.append({'index':i+1,'name':e['ObjectName'],'class':resolve(e['ClassIndex']),'parent':resolve(e['OuterIndex']),
                       'props':data,'slot':props(slot)})
if len(sys.argv)==1: (ROOT/'evidence/native-hud-layout.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
if len(sys.argv)==1: print(json.dumps(report,indent=2))
print('Class:', [(i+1,e['ObjectName'],resolve(e['ClassIndex']),resolve(e['SuperIndex'])) for i,e in enumerate(d['Exports']) if 'ClassExport' in e['$type']])
if len(sys.argv)>1:
    print('class functions', [e.get('FuncMap') for e in d['Exports'] if 'ClassExport' in e['$type']])
    print('class imports', [(i+1,e['ObjectName'])for i,e in enumerate(d['Imports']) if e['ClassName']=='Class'])
    print('widgets', [(i+1,e['ObjectName'],resolve(e['ClassIndex'])) for i,e in enumerate(d['Exports']) if e['ClassIndex'] in [-8,-14,-15]])
    for name in ['PlaneTextStatPanel','StatsTextHBox','StatsTextVBox','StatsInfoVBox','SpecialWeaponTextBP','SpecialWeaponCountTextBP','SpecialWeaponSel']:
        for i,e in enumerate(d['Exports']):
            if e['ObjectName']==name:
                data=props(e)
                slot=d['Exports'][data['Slot']-1] if data.get('Slot')else{}
                print(name,i+1,data,'slot',props(slot))
