"""在已完成的65/73资源基线后追加后腹观察副本，旧候选与其他机体保持。"""
import hashlib
import json


def append_m31b_offset(planes, assets, root):
    observation = root/'evidence/m31b-mount-observation.json'
    original = json.loads(observation.read_text(encoding='utf-8'))
    data = json.loads((root/'evidence/m31b-saam-offset-r1.json').read_text(encoding='utf-8'))
    if (data['schema'] != 1 or data['status'] != 'user_requested_offset_observation_not_validated' or
            data['observation_sha256'] != hashlib.sha256(observation.read_bytes()).hexdigest() or
            data['masks'] != [6, 7] or assets[:65] != original['baseline_assets'] or len(assets) not in (65, 73)):
        raise ValueError('MiG-31B偏移依据与已交付观察版或资源基线不符')
    plane = next(p for p in planes if p['code'] == 'm31b')
    if plane != original['plane'] or data['asset'] in assets:
        raise ValueError('MiG-31B旧方案已变更或新副本已追加，不覆盖')
    report, = data['resource_reports']
    if (report['recipe']['name'] != data['asset'] or data['offset'] != {'X': -6.0, 'Y': 0.0, 'Z': -2.0} or
            not all(report.get(k) is True for k in ('asset_roundtrip', 'renamed_ac7_roundtrip',
                    'all_exports_payload_preserved', 'all_lod_geometry_bytes_unchanged',
                    'rotations_scales_and_other_sockets_unchanged'))):
        raise ValueError('MiG-31B偏移资源缺少限定改动与离线回读依据')
    for mask in data['masks']:
        plane['plans'][mask]['variants'][0] = len(assets)
    # 只读NoCollision资格，不更改碰撞参数；原模板的三个SWP均已静态核实为NoCollision。
    plane['requires_no_collision'] = True
    assets.append(data['asset'])


def apply_m31b_offset_r2(planes, assets, root):
    previous = root/'evidence/m31b-saam-offset-r1.json'
    old = json.loads(previous.read_text(encoding='utf-8'))
    data = json.loads((root/'evidence/m31b-saam-offset-r2.json').read_text(encoding='utf-8'))
    if (data['schema'] != 1 or data['status'] != 'user_requested_offset_observation_not_validated' or
            data['previous_revision_sha256'] != hashlib.sha256(previous.read_bytes()).hexdigest() or
            data['offset'] != {'X': -7.0, 'Y': 0.0, 'Z': -2.0} or data['masks'] != [6, 7] or
            data['asset'] != 'MSP_m31b_saam_rear_lower_r2' or assets.count(old['asset']) != 1 or data['asset'] in assets):
        raise ValueError('MiG-31B二次后移与旧观察副本不符，不覆盖')
    index = assets.index(old['asset'])
    plane = next(p for p in planes if p['code'] == 'm31b')
    if any(plane['plans'][mask]['variants'][0] != index for mask in data['masks']):
        raise ValueError('MiG-31B旧观察副本占位已变化，不能替换')
    report, = data['resource_reports']
    if (report['recipe']['name'] != data['asset'] or not all(report.get(k) is True for k in
            ('asset_roundtrip', 'renamed_ac7_roundtrip', 'all_exports_payload_preserved',
             'all_lod_geometry_bytes_unchanged', 'rotations_scales_and_other_sockets_unchanged'))):
        raise ValueError('MiG-31B二次后移缺少限定改动或回读依据')
    assets[index] = data['asset']  # 仅原观察副本槽位替换为r2，其他资源编号保持。


def apply_m31b_offset_r3(planes, assets, root):
    previous = root/'evidence/m31b-saam-offset-r2.json'
    old = json.loads(previous.read_text(encoding='utf-8'))
    data = json.loads((root/'evidence/m31b-saam-offset-r3.json').read_text(encoding='utf-8'))
    if (data['schema'] != 1 or data['status'] != 'user_requested_offset_observation_not_validated' or
            data['previous_revision_sha256'] != hashlib.sha256(previous.read_bytes()).hexdigest() or
            data['offset'] != {'X': -11.0, 'Y': 0.0, 'Z': -2.0} or data['masks'] != [6, 7] or
            data['asset'] != 'MSP_m31b_saam_rear_lower_r3' or assets.count(old['asset']) != 1 or data['asset'] in assets):
        raise ValueError('MiG-31B三次后移与旧观察副本不符，不覆盖')
    index = assets.index(old['asset'])
    plane = next(p for p in planes if p['code'] == 'm31b')
    if any(plane['plans'][mask]['variants'][0] != index for mask in data['masks']):
        raise ValueError('MiG-31B旧观察副本占位已变化，不能替换')
    report, = data['resource_reports']
    if (report['recipe']['name'] != data['asset'] or report['recipe']['socket_offset'] != data['offset'] or
            not all(report.get(k) is True for k in ('asset_roundtrip', 'renamed_ac7_roundtrip',
                'all_exports_payload_preserved', 'all_lod_geometry_bytes_unchanged',
                'rotations_scales_and_other_sockets_unchanged'))):
        raise ValueError('MiG-31B三次后移缺少限定改动或回读依据')
    assets[index] = data['asset']  # 仅r2同一槽位改用r3，组合表和其他73项不动。
