"""修复TGM完整借架的cooked Socket引用；只生成离线新名资源，不构建或操作游戏。"""
import argparse
import base64
import copy
import json
from pathlib import Path
import shutil
import struct

from prepare_f18e_layout_feedback import whole_rack, collision_entry, digest, write
from split_static_mount_asset import load_mesh, parse_mesh

ROOT = Path(__file__).resolve().parents[2]
OLD = 'MSP_f18etg_gpb_inner_whole_r1'
NEW = 'MSP_f18etg_gpb_inner_whole_r2'


def main(args):
    if args.output.exists() or args.evidence.exists() or not args.output.resolve().is_relative_to(ROOT/'Temp'):
        raise ValueError('必须使用当前工作树Temp内新目录及新依据，不覆盖旧交付')
    baseline_path = ROOT/'evidence/f18e-layout-feedback-r1.json'
    baseline = json.loads(baseline_path.read_text(encoding='utf-8'))
    previous_report = baseline['resource_reports'][2]
    if previous_report['recipe']['name'] != OLD:
        raise ValueError('旧观察资源不符')
    previous = json.loads(args.previous_json.read_text(encoding='utf-8-sig'))
    old_export = next(e for e in previous['Exports'] if e['ObjectName'] == OLD)
    old_raw = base64.b64decode(old_export['Extras'])
    old_mesh = parse_mesh(old_raw)  # 故意不走load_mesh：旧资源应被新增引用资格拒绝。
    refs = [p['Value'] for p in next(p for p in old_export['Data'] if p['Name']=='Sockets')['Value']]
    if (old_mesh['socket_references'] != [4,7] or refs != [4,5] or
            old_mesh['geometry_offset'] != 42 or len(previous['Exports']) != 5):
        raise ValueError('并非已确认的r1第7导出越界，不套用修复')
    for suffix, expected in previous_report['encoded_hashes'].items():
        if digest(args.previous_json.parent.parent/'encoded'/(OLD+suffix)) != expected:
            raise ValueError('旧资源与已交付r1字节不符')
    load_mesh(args.source_json)
    if digest(args.source_json) != previous_report['json_sha256']:
        raise ValueError('受体原生来源指纹改变')
    if digest(args.donor_json) != previous_report['recipe']['whole_geometry_donor_json_sha256']:
        raise ValueError('完整供体原生来源指纹改变')
    args.output.mkdir(parents=True)
    candidate_dir = args.output/NEW
    report = whole_rack(args.source_json,args.donor_json,candidate_dir,args.tools_root,name=NEW)
    candidate_path = candidate_dir/'roundtrip'/(NEW+'.json')
    actual, export, raw, mesh = load_mesh(candidate_path)
    expected_raw = bytearray(old_raw)
    struct.pack_into('<i',expected_raw,38,5)
    if raw != bytes(expected_raw) or mesh['socket_references'] != refs:
        raise ValueError('新资源不是只将错误Socket导出7修复为5')
    # 排除自身名称和这一引用后，所有导出属性/cooked载荷逐项等于旧观察资源。
    reverted = copy.deepcopy(actual)
    reverted['NameMap'] = [s.replace(NEW,OLD) for s in reverted['NameMap']]
    keys = ('ObjectName','Data','Extras','OuterIndex','ClassIndex','SuperIndex','TemplateIndex')
    for before,after in zip(previous['Exports'],reverted['Exports'],strict=True):
        if after['ObjectName'] == NEW:
            after['ObjectName'] = OLD
            after['Extras'] = old_export['Extras']
        if any(before.get(k) != after.get(k) for k in keys):
            raise ValueError('引用与自身名称之外的导出载荷发生变化')
    if previous['Imports'] != actual['Imports'] or previous['NameMap'] != reverted['NameMap']:
        raise ValueError('新资源引入其他导入或名称差异')
    report.update(previous_encoded_hashes=previous_report['encoded_hashes'],
                  only_socket_reference_and_own_name_changed=True,
                  all_lod_geometry_and_mount_transforms_unchanged=True,
                  old_cooked_references=[4,7],new_cooked_references=refs,
                  corrected_int32_offset=38)
    collision = collision_entry(args.source_json,candidate_path,report)
    source = args.output/'source';source.mkdir()
    for suffix in ('.uasset','.uexp'):
        shutil.copyfile(candidate_dir/'encoded'/(NEW+suffix),source/(NEW+suffix))
    result = dict(schema=1,status='cooked_socket_reference_fixed_runtime_unverified',
                  baseline_commit='983f1c36a6ee08880d49d7a1df81e61ae84e17bf',baseline_version='2.25.1',
                  previous_evidence='evidence/f18e-layout-feedback-r1.json',previous_evidence_sha256=digest(baseline_path),
                  previous_asset=OLD,asset=NEW,asset_index=76,masks=[6,7],layout_changed=False,
                  previous_candidate_json_sha256=digest(args.previous_json),
                  resource_reports=[report],collision_preservation=[collision],
                  defect=dict(cooked_references=[4,7],property_references=[4,5],export_count=5,
                              cause='拼接少计4字节Socket数量字段，将供体末项导出7混入受体第二项；通用Extras回读不能发现此错误',
                              source_geometry_offset=42,donor_geometry_offset=50),
                  resource_input=str(source),compiled=False,pak_built=False,tests_run=False,installed=False,
                  game_process_access=False,game_operated=False,
                  limits=['该资源错误已静态确认；是否为本次卡死唯一原因尚无卡死栈证明',
                          '模型布局、GPB内侧位置、完整借架、弹药和运行借位规则保持',
                          '新资源游戏加载、预览、离架和还原仍待构建安装后由用户观察'])
    write(args.evidence,result)
    print(json.dumps(dict(asset=NEW,refs=refs,geometry_offset=mesh['geometry_offset'],layout_changed=False),ensure_ascii=False))


if __name__ == '__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--source-json',type=Path,required=True)
    p.add_argument('--donor-json',type=Path,required=True)
    p.add_argument('--previous-json',type=Path,required=True)
    p.add_argument('--tools-root',type=Path,required=True)
    p.add_argument('--output',type=Path,required=True)
    p.add_argument('--evidence',type=Path,required=True)
    main(p.parse_args())
