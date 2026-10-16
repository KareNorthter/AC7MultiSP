"""对指定完整构建运行当前适用检查；不启动或注入游戏。"""
import argparse
import ctypes
from datetime import datetime, timezone
import os
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'src'))
from release_tools import read_json, write_json, verify_build, digest, tree_fingerprint, contained


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-dir', type=Path, required=True)
    args = parser.parse_args()
    build = contained(args.build_dir, ROOT / 'Temp').resolve(strict=True)
    output = build / ('tests-' + datetime.now().strftime('%Y%m%d-%H%M%S-%f'))
    output.mkdir()
    if (build / 'test-receipt.json').exists():
        (build / 'test-receipt.json').rename(output / 'previous-test-receipt.json')
    receipt = read_json(build / 'build-receipt.json')
    info = verify_build(receipt, build)
    compiler_root = Path(receipt['toolchain_path'])
    if tree_fingerprint(compiler_root) != receipt['toolchain']:
        raise ValueError('测试工具链与构建不一致')
    test_sources = {p.relative_to(ROOT).as_posix(): digest(p) for p in (ROOT / 'tests').iterdir()
                    if p.suffix in ('.py', '.cpp', '.lua', '.cmd')}
    environment = dict(os.environ, TMP=str(output), TEMP=str(output))
    checks = []
    def run(name, command):
        result = subprocess.run(list(map(str, command)), cwd=output, env=environment,
                                capture_output=True, text=True, encoding='utf-8', errors='replace')
        (output / (name + '.log')).write_text(result.stdout + result.stderr, encoding='utf-8')
        if result.returncode:
            raise RuntimeError(f'{name} 失败：{result.stdout}{result.stderr}')
        checks.append(name)
        print(name + ': 通过', flush=True)
    for name in ('release_pipeline_test', 'current_runtime_test'):
        run(name, [sys.executable, '-X', 'utf8', ROOT / 'tests' / (name + '.py')])
    compiler = compiler_root / 'bin/clang++.exe'
    for name in ('reload_timer_test', 'panel_pointer_test', 'weapon_group_test'):
        command = [compiler, '-std=c++17', '-O2', '-static', '-D' + info['core_define'],
                   '-I', ROOT / 'vendor', '-I', ROOT / 'vendor/minhook-1.3.4/include',
                   ROOT / 'tests' / (name + '.cpp'), '-o', output / (name + '.exe')]
        if name == 'weapon_group_test':
            command += [build / (n + '.o') for n in ('buffer', 'hook', 'trampoline', 'hde64')]
            command += ['-lole32']
        run(name + '_compile', command)
        run(name, [output / (name + '.exe')])
    # 只加载本轮 DLL 并查询导出符号，不调用钩子、武器操作或游戏接口。
    for name in ('DualSP.dll', 'DualSPUI.dll'):
        library = ctypes.WinDLL(str(build / name))
        support = library.dualsp_three_sp_supported
        support.restype = ctypes.c_int
        # 导出使用 Lua 参数协议；这里只核对符号存在，不能以空指针调用。
        checks.append(name + '_loaded_exports')
    if (info['name'] + ' ' + info['version'] + ' 管理').encode('utf-8') not in (build / 'DualSPUI.dll').read_bytes():
        raise ValueError('UI 标题不是当前产品版本')
    verify_build(receipt, build)
    if any(digest(ROOT / name) != value for name, value in test_sources.items()):
        raise ValueError('测试期间测试来源改变')
    result = {'name': info['name'], 'version': info['version'], 'passed': True,
              'created_at': datetime.now(timezone.utc).isoformat(), 'checks': checks,
              'build_receipt_sha256': digest(build / 'build-receipt.json'), 'test_sources': test_sources,
              'game_started': False, 'gameplay_verified': False,
              'excluded': ['旧双SP控制器/HUD/选装模拟：协议过期', '真实引擎挂载、补给、检查点、长时稳定性：待用户实测']}
    write_json(build / 'test-receipt.json', result)
    print('本轮自动检查通过；实玩验证待用户。')


if __name__ == '__main__':
    main()
