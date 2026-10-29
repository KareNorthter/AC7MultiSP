"""将F-22现场核对的网格骨序与内外布局追加至多挂源码表。"""
import copy
import hashlib
import json


def append_f22_internal(data, root):
    record = json.loads((root/'evidence/f22-internal-runtime-source-20261029.json').read_text(encoding='utf-8'))
    if (record['schema'] != 1 or record['status'] != 'runtime_source_candidate_not_validated'
            or len(data['planes']) != 29 or len(data['assets']) != 77
            or data['planes'][-1]['code'] != 'pkfa' or any(p['code'] == 'f22a' for p in data['planes'])):
        raise ValueError('F-22候选的29机/77资源基线改变')
    for key in ('design', 'observation'):
        if hashlib.sha256((root/record[key]).read_bytes()).hexdigest() != record[key+'_sha256']:
            raise ValueError('F-22候选证据已变化')
    design = json.loads((root/record['design']).read_text(encoding='utf-8'))
    observation = json.loads((root/record['observation']).read_text(encoding='utf-8'))
    plane = copy.deepcopy(design['plane_candidate'])
    if plane['code'] != 'f22a' or plane['local_pool'] != 2 or len(plane['plans']) != 8:
        raise ValueError('F-22候选机型或组合不完整')
    plane['groups'][2]['bones'] = observation['live_observation']['mesh_bones']
    pose = record['closed_side_pose']
    if [p['index'] for p in pose] != observation['live_observation']['side_candidates'] or any(
            plane['groups'][2]['bones'][p['index']]['name'] != p['name'] for p in pose):
        raise ValueError('F-22侧舱骨序或关闭姿态不符')
    plane['closed_side_pose'] = pose
    data['planes'].append(plane)
    return data
