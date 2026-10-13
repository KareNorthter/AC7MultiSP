"""Inspect selected weapon/pawn assets in the user's own game, never run AC7."""
import json
import sys
from pathlib import Path
from inspect_hud_assets import inspect

ROOT=Path(__file__).resolve().parents[1]
PAKS=Path(r'E:\SteamLibrary\steamapps\common\ACE COMBAT 7\Game\Content\Paks')
query_only='--index' in sys.argv
def selected(name):
    lower=name.lower()
    if query_only:
        return name.endswith('.uasset')and ('su35' in lower or 'plwp_lasm' in lower or 'plwp_6aam' in lower or 'plwp_qaam' in lower or name.endswith('/AcePlayerPawn.uasset'))
    blueprints=('/Blueprint/' in name)and any(k in name for k in ['AcePlayerPawn_su35.','/AcePlayerPawn.','plwp_lasm','plwp_6aam','plwp_qaam'])
    model=any(name.endswith('/'+asset+suffix)for asset in ['su35_Skeleton','su35_SWP1_SM','su35_SWP2_SM','su35_SWP3_SM']for suffix in ['.uasset','.uexp'])
    return name.endswith(('.uasset','.uexp'))and(blueprints or model)

reports=[inspect(PAKS/name,selected,ROOT/'Temp/weapon-model-assets',query_only)
         for name in ('pakchunk0-WindowsNoEditor_0_P.pak','pakchunk0-WindowsNoEditor.pak')]
target=ROOT/('Temp/weapon-model-index.json' if query_only else 'evidence/weapon-model-asset-extraction.json')
target.write_text(json.dumps(reports,indent=2),encoding='utf-8')
for report in reports:
    print(report['pak'],len(report['assets']))
    for asset in report['assets']:print(asset['bytes'],asset['name'])
