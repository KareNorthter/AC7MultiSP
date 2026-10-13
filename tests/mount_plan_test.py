"""Check the offline planner against real extracted mount coordinates."""
import json
import sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'src'))
from dual_mount_plan import analyze,allocate
layout=json.loads((ROOT/'evidence/weapon-model-layout.json').read_text('utf-8'))
result=analyze(layout)
assert result['direct_overlay']['laam_6aam_identical_positions']==6
assert result['direct_overlay']['collision_free_allocation']is None
assert result['direct_overlay']['required_loaded_count']=={'LAAM':2,'6AAM':6}
candidate=result['six_aam_lasm_geometry_candidate']
assert candidate['retained_loaded_counts']=={'6AAM':6,'LASM':2}
assert len(candidate['lasm_fuselage_sockets'])==2
for window in candidate['queue_windows']:
    assert len(set(window['candidate_physical_slots']))==2
assert candidate['geometry_only'] and not result['candidate_installed']
# Backtracking must find valid assignments as well as reject conflicts.
assert allocate({'A':1,'B':1},{'A':{1,2},'B':{1}})=={'A':[2],'B':[1]}
assert allocate({'A':2,'B':1},{'A':{1,2},'B':{1,2}})is None
print('mount_planner: exact duplicate positions, preserved salvos, remap windows and rejection checks passed; no game run')
