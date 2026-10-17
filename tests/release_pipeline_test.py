"""验证来源增删改、旧回执及越界拒绝；只改 Temp 内的复制件。"""
from pathlib import Path
import json
import shutil
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'src'))
from release_tools import source_snapshot, product, verify_build, contained, verify_mount_manifest


def rejected(action):
    try:
        action()
    except (ValueError, FileNotFoundError):
        return
    raise AssertionError('应拒绝但被接受')


def main():
    # 同一来源必须生成完全一致的核心表与Lua加载清单，防止只改其中一份。
    from research.build_runtime_mount_data import build as mount_data
    data = mount_data()
    header = (ROOT / 'src/mount_expansion_data.hpp').read_text(encoding='utf-8')
    assert json.loads(header.split('R"MSP(', 1)[1].split(')MSP"', 1)[0]) == data
    loader = '-- 独立挂架资源清单；由同一份七组合数据生成。\nreturn {\n'
    for plane in data['planes']:
        entries = sorted({(i,pool+1) for plan in plane['plans'][1:] for pool,i in enumerate(plan['variants']) if i >= 0})
        rendered = []
        for i, pool in entries:
            masks = sum(1 << mask for mask, plan in enumerate(plane['plans'])
                        if mask and plan['variants'][pool-1] == i)
            rendered.append('{' + str(i) + ',"' + data['assets'][i] + '",' + str(pool) + ',' + str(masks) + '}')
        loader += '    ' + plane['code'] + '={' + ','.join(rendered) + '},\n'
    loader += '}\n'
    assert loader == (ROOT / 'dist/DualSP/Scripts/mount_assets.lua').read_text(encoding='utf-8')
    # 成套资源数量增长时必须逐成员对应；缺件、多件、重复名或伪造数量均拒绝。
    for amount in (2, 18, 39, 46):
        variants = ['MSP_fixture_' + str(i) for i in range(amount)]
        members = {'Nimbus/Content/AC7MultiSP/Research/' + name + suffix: 'a' * 64
                   for name in variants for suffix in ('.uasset', '.uexp')}
        assets = dict(file='AC7MultiSP_Mounts_P.pak', variants=variants, members=members,
                      file_count=len(members), independent_readback=True)
        verify_mount_manifest(assets, variants)
        rejected(lambda: verify_mount_manifest(dict(assets, file_count=32), variants))
        missing = dict(members);missing.pop(next(iter(missing)))
        rejected(lambda: verify_mount_manifest(dict(assets, members=missing), variants))
        extra = dict(members);extra['unexpected.uasset'] = 'a' * 64
        rejected(lambda: verify_mount_manifest(dict(assets, members=extra), variants))
        rejected(lambda: verify_mount_manifest(dict(assets, variants=list(reversed(variants))), variants))
        rejected(lambda: verify_mount_manifest(assets, variants + [variants[0]]))
    fixture = Path(tempfile.mkdtemp(prefix='source-check-', dir=ROOT / 'Temp'))
    for name in source_snapshot():
        target = fixture / name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(ROOT / name, target)
    info = product(fixture)
    receipt = dict(receipt_schema=2, name=info['name'], version=info['version'],
                   source_files=source_snapshot(fixture), build_define=info['core_define'], protected_files={})
    verify_build(receipt, root=fixture)
    # 原先会漏检的头文件、第三方源和参数文件必须全部拒绝。
    for name in ['src/reload_timer.hpp', 'src/common.hpp', 'src/ue4ss_lua_api.hpp',
                 'vendor/minhook-1.3.4/src/hook.c', 'vendor/imgui-1.92.9b/imgui.h', 'src/product.json']:
        path = fixture / name
        original = path.read_bytes()
        path.write_bytes(original + b'\n ')
        rejected(lambda: verify_build(receipt, root=fixture))
        path.write_bytes(original)
    extra = fixture / 'dist/DualSP/Scripts/unregistered.lua'
    extra.write_text('return {}', encoding='utf-8')
    rejected(lambda: verify_build(receipt, root=fixture))
    extra.rename(fixture / 'unregistered.lua.saved')
    missing = fixture / 'src/common.hpp'
    saved = fixture / 'common.hpp.saved'
    missing.rename(saved)
    rejected(lambda: verify_build(receipt, root=fixture))
    saved.rename(missing)
    rejected(lambda: verify_build(dict(receipt, receipt_schema=1), root=fixture))
    rejected(lambda: verify_build(dict(receipt, build_define=''), root=fixture))
    rejected(lambda: contained(fixture / '../../outside', fixture))
    verify_build(receipt, root=fixture)
    print('来源检查：原样接受；6类修改、新增Lua、删除头文件、旧回执、错误宏、越界均拒绝')


if __name__ == '__main__':
    main()
