"""仅以新名资源替换TGM第76项错误引用；不改变布局、借位、碰撞或配弹规则。"""
import hashlib
import json


def apply_f18etg_socket_r2(planes,assets,preserved,root):
    old='MSP_f18etg_gpb_inner_whole_r1'
    new='MSP_f18etg_gpb_inner_whole_r2'
    evidence=root/'evidence/f18etg-whole-rack-socket-r2.json'
    data=json.loads(evidence.read_text(encoding='utf-8'))
    previous=root/data['previous_evidence']
    if (data['schema']!=1 or data['status']!='cooked_socket_reference_fixed_runtime_unverified' or
            data['previous_evidence_sha256']!=hashlib.sha256(previous.read_bytes()).hexdigest() or
            data['previous_asset']!=old or data['asset']!=new or data['asset_index']!=76 or
            data['masks']!=[6,7] or data['layout_changed'] or len(assets)!=77 or assets[76]!=old or new in assets):
        raise ValueError('TGM Socket r2与原观察副本不符')
    plane=next(p for p in planes if p['code']=='f18etg')
    if any(plane['plans'][mask]['variants'][2]!=76 for mask in (6,7)):
        raise ValueError('TGM布局资源槽位已变化，不能覆盖')
    report,=data['resource_reports']
    if (report['recipe']['name']!=new or report['cooked_socket_references']!=[4,5] or
            report['geometry_offset']!=42 or report['donor_geometry_offset']!=50 or
            not all(report.get(k) is True for k in ('asset_roundtrip','renamed_ac7_roundtrip',
                'all_exports_payload_preserved','cooked_socket_references_verified',
                'only_socket_reference_and_own_name_changed','all_lod_geometry_and_mount_transforms_unchanged'))):
        raise ValueError('TGM Socket r2缺少完整引用与改动范围依据')
    entry,=data['collision_preservation']
    prior=next(e for e in preserved if e['asset']==old)
    keys=('source','source_package','body_name','body_number','body_guid')
    if (entry['asset']!=new or any(entry[k]!=prior[k] for k in keys) or
            entry['encoded_hashes']!=report['encoded_hashes'] or
            not entry['collision_references_unchanged'] or not entry['complete_body_and_nav_exports_unchanged']):
        raise ValueError('TGM Socket r2不能改变既有碰撞资格')
    prior['asset']=new
    assets[76]=new  # 77项及其余76项编号保持；错误旧名不再进入新PAK或加载清单。
