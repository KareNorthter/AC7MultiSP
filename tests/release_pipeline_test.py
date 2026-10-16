"""验证来源增删改、旧回执及越界拒绝；只改 Temp 内的复制件。"""
from pathlib import Path
import shutil
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'src'))
from release_tools import source_snapshot, product, verify_build, contained


def rejected(action):
    try:
        action()
    except (ValueError, FileNotFoundError):
        return
    raise AssertionError('应拒绝但被接受')


def main():
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
