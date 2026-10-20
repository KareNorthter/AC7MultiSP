"""追加四机局部机构源码候选；旧65资源及其他线程布局修订保持。"""
import hashlib
import json


def append_local_four(planes, assets, root):
    source = root/'evidence/local-four-mount-runtime.json'
    data = json.loads(source.read_text(encoding='utf-8'))
    design = root/'evidence/active-five-mount-static-plan.json'
    if (data['schema'] != 1 or data['status'] != 'runtime_source_candidate_not_validated' or
            data['steam_build'] != 9855922 or assets != data['baseline_assets'] or
            hashlib.sha256(design.read_bytes()).hexdigest() != data['static_design_sha256']):
        raise ValueError('局部机构候选与静态设计或65资源基线不符')
    additions = data['appended_assets']
    reports = {r['recipe']['name']: r for r in data['resource_reports']}
    if len(additions) != 8 or len(set(assets+additions)) != len(assets)+8 or set(additions) != set(reports):
        raise ValueError('局部机构新增资源重复或缺少证据')
    for name, report in reports.items():
        if not all(report.get(k) is True for k in ('asset_roundtrip', 'renamed_ac7_roundtrip',
                'all_exports_payload_preserved', 'non_index_bytes_preserved', 'sockets_and_materials_preserved')):
            raise ValueError('局部机构资源离线依据不完整：'+name)
    candidates = data['planes']
    codes = ['f14d', 'f18f', 'su33', 'f18x']
    if [p['code'] for p in candidates] != codes or set(codes) & {p['code'] for p in planes}:
        raise ValueError('局部机构候选机型顺序或集合不符')
    for plane in candidates:
        if not plane['local_mechanism'] or len(plane['groups']) != 3 or len(plane['plans']) != 8:
            raise ValueError('局部机构资格或七组合缺失')
        for mask, plan in enumerate(plane['plans'][1:], 1):
            occupied = set()
            for pool, chosen in enumerate(plan['selected']):
                group = plane['groups'][pool]
                if bool(chosen) != bool(mask & (1 << pool)):
                    raise ValueError('局部机构所选兵装与占位不一致')
                for socket in chosen:
                    if not 0 <= socket < len(group['sockets']) or (pool, socket) in occupied:
                        raise ValueError('局部机构挂点重复或越界')
                    occupied.add((pool, socket))
                variant = plan['variants'][pool]
                if variant >= 0 and (group['skeletal'] or not len(assets) <= variant < len(assets)+8):
                    raise ValueError('局部机构不能替换原生骨架，静态资源编号须为本次追加')
    assets.extend(additions)
    planes.extend(candidates)
