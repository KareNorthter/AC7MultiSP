"""Read-only inspection of the owned game's hangar menu; never launch AC7."""
import json
import sys
from pathlib import Path
from inspect_hud_assets import inspect

ROOT=Path(__file__).resolve().parents[1]
PAKS=Path(r'E:\SteamLibrary\steamapps\common\ACE COMBAT 7\Game\Content\Paks')
index_only='--index' in sys.argv
def selected(name):
    if not name.endswith(('.uasset','.uexp')):
        return False
    if index_only:
        return name.endswith('.uasset') and ('/UI/' in name or 'SequenceMenu' in name) and any(term in name.lower() for term in ('hangar','weapon','list','menuitem','sequence','title'))
    names={'CampaignHangarWeapon','HangarWeaponInformation','HangarAircraftBoardWeaponsAmmo',
           'CampaignHangarReview','CampaignHangarAircraft','HangarStandbyInformation',
           'SequenceMenuData','SequenceLinkData','SequenceMenuItemData'}
    return Path(name).stem in names

reports=[inspect(PAKS/name,selected,ROOT/'Temp/loadout-assets',index_only)
         for name in ('pakchunk0-WindowsNoEditor_0_P.pak','pakchunk0-WindowsNoEditor.pak')]
target=ROOT/('Temp/loadout-index.json' if index_only else 'evidence/loadout-asset-extraction.json')
target.write_text(json.dumps(reports,indent=2),encoding='utf-8')
for report in reports:
    print(report['pak'],len(report['assets']))
    for asset in report['assets']:print(asset['bytes'],asset['name'])
