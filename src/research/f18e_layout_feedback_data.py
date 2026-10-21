"""两机布局反馈只覆盖mask6/7；历史布局、编号及碰撞依据保持原阶段含义。"""
import json


def apply_f18e_layout_feedback(planes, assets, preserved, root):
    data = json.loads((root/'evidence/f18e-layout-feedback-r1.json').read_text(encoding='utf-8'))
    expected_names = ['MSP_f18e_agm_outer_r1','MSP_f18e_lagm_inner_r1','MSP_f18etg_gpb_inner_whole_r1']
    if (data['schema'] != 1 or data['status'] != 'user_requested_layout_runtime_candidate' or
            len(assets) != 74 or assets != data['baseline_assets'] or data['appended_assets'] != expected_names):
        raise ValueError('两机布局反馈与已安装2.25资源基线不符')
    if [r['recipe']['name'] for r in data['resource_reports']] != expected_names:
        raise ValueError('两机新增资源集合不完整')
    for report in data['resource_reports']:
        if not all(report.get(k) is True for k in ('asset_roundtrip','renamed_ac7_roundtrip','all_exports_payload_preserved')):
            raise ValueError('新资源缺少完整回读依据')
    whole = data['resource_reports'][2]
    if (whole['recipe']['source_asset'] != 'f18etg_SWP3_SM' or
            whole['recipe']['whole_geometry_donor'] != 'f18e_SWP2_SM' or whole['donor_lods'] != 3 or
            not all(whole.get(k) is True for k in ('complete_donor_geometry_bytes_preserved',
                'donor_materials_preserved','collision_and_navigation_preserved','socket_rotations_and_scales_preserved'))):
        raise ValueError('TGM必须保留供体完整挂架和自身原生碰撞')
    expected = {
        ('f18e',6):dict(selected=[[],[0,1],[2,3]],racks=6,variants=[-1,74,75]),
        ('f18e',7):dict(selected=[[0,1],[0,1],[2,3]],racks=7,variants=[59,74,75]),
        ('f18etg',6):dict(selected=[[],[0,1],[0,1]],racks=4,variants=[-1,-1,76]),
        ('f18etg',7):dict(selected=[[],[0],[0,1]],racks=4,variants=[-1,-1,76],
                          full_borrowed_kinds=1,borrowed_positions=[dict(kind=0,pool=1,socket=1)])}
    seen = set()
    for change in data['changes']:
        key = change['code'],change['mask']
        plane = next(p for p in planes if p['code'] == change['code'])
        if key not in expected or key in seen or change['replacement'] != expected[key] or plane['plans'][key[1]] != change['previous']:
            raise ValueError('只接受两机四个精确布局差异，不覆盖新成果')
        plane['plans'][key[1]] = change['replacement']
        seen.add(key)
    if seen != set(expected):
        raise ValueError('两机四组合不完整')
    if [e['asset'] for e in data['collision_preservation']] != expected_names:
        raise ValueError('新增资源碰撞保真集合不完整')
    for entry, report in zip(data['collision_preservation'],data['resource_reports'],strict=True):
        plane = next(p for p in planes if p['code'] == entry['aircraft'])
        if (entry['source'] not in [g['rack'] for g in plane['groups']] or
                entry['encoded_hashes'] != report['encoded_hashes'] or len(entry['body_guid']) != 16 or
                not all(entry.get(k) is True for k in ('collision_references_unchanged','complete_body_and_nav_exports_unchanged'))):
            raise ValueError('新增资源碰撞依据与原生来源不符')
        preserved.append({k:entry[k] for k in ('asset','source','source_package','body_name','body_number','body_guid')})
    assets.extend(expected_names)
