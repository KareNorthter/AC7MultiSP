"""开发故障候选的离线用例选择、日志保存及测试前安装恢复；不启动游戏。"""
import argparse
import ctypes
from datetime import datetime
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

CASES = ['off', 'T1', 'T2', 'T3-weapon', 'T3-ammo', 'T3-aircraft', 'T4', 'T5', 'T6', 'T7',
         'U-current', 'U-newer', 'U-failed', 'U-unpublished']


def require(ok, reason):
    if not ok:
        raise RuntimeError(reason)


def read(path):
    return json.loads(path.read_text(encoding='utf-8-sig'))


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def closed():
    result = subprocess.run([str(Path(os.environ['SystemRoot'])/'System32/tasklist.exe'),
                             '/FO', 'CSV', '/NH'], capture_output=True, check=True, timeout=15)
    require(b'ace7game' not in result.stdout.lower(), '请先完整退出游戏。')


def save_json(path, data):
    with path.open('x', encoding='utf8', newline='\n') as stream:
        json.dump(data, stream, ensure_ascii=False, indent=2)
        stream.write('\n')


def local_path(root, relative):
    path = root/relative
    require(path.resolve().is_relative_to(root.resolve()) and path != root, '路径越界：'+str(path))
    require(not path.is_symlink() and not path.is_junction(), '路径是链接：'+str(path))
    return path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--session', required=True, type=Path)
    parser.add_argument('action', choices=['select', 'archive', 'status', 'restore'])
    parser.add_argument('--case', choices=CASES, default='off')
    args = parser.parse_args()
    session = args.session.resolve()
    require(session.is_dir(), '测试目录不存在。')
    data = read(session/'session.json')
    game = Path(data['game']).resolve()
    mod = local_path(game, 'ue4ss/Mods/DualSP')
    selector = local_path(mod, 'DevFaultTest.json')
    if args.action == 'status':
        print('测试开关：'+(json.dumps(read(selector), ensure_ascii=False) if selector.exists() else '关闭'))
        print('目标机：'+str(data['target'])+'；对照机：'+str(data['control']))
        print('已装版本：'+(mod/'VERSION').read_text(encoding='utf8').strip())
        return
    closed()
    if args.action == 'restore':
        profiles = game/'AC7AircraftProfiles'
        if read(profiles/'state.json')['profile'] != data['profile_before']:
            subprocess.run([sys.executable, '-X', 'utf8', str(profiles/'aircraft_mod_profiles.py'),
                '--manifest', str(profiles/'manifest.json'), '--profile', data['profile_before']], check=True)
    kernel = ctypes.WinDLL('kernel32', use_last_error=True)
    kernel.CreateFileW.argtypes = [ctypes.c_wchar_p, ctypes.c_uint32, ctypes.c_uint32,
                                  ctypes.c_void_p, ctypes.c_uint32, ctypes.c_uint32, ctypes.c_void_p]
    kernel.CreateFileW.restype = ctypes.c_void_p
    kernel.CloseHandle.argtypes = [ctypes.c_void_p]
    lock = kernel.CreateFileW(str(game/'AC7MultiSP-install.lock'), 0xC0000000, 0, None, 4, 0x80, None)
    require(lock not in (None, ctypes.c_void_p(-1).value), '另一个安装或切换操作正在进行。')
    try:
        stamp = datetime.now().astimezone().strftime('%Y%m%d-%H%M%S-%f')
        if args.action == 'select':
            require(mod.is_dir(), '本Mod未安装。')
            if args.case != 'off':
                require(digest(mod/'bin/DualSP.dll') == data['development_core_sha256'] and
                        digest(mod/'bin/DualSPUI.dll') == data['development_ui_sha256'],
                        '请先安装本轮开发故障测试EXE；当前DLL与测试来源不一致。')
            history = session/'selector-history'
            history.mkdir(exist_ok=True)
            if selector.exists():
                os.rename(selector, history/(stamp+'.json'))
            if args.case != 'off':
                save_json(selector, {'case': args.case, 'target': data['target']['id']})
            print('下次启动用例：'+args.case+'；目标机：'+data['target']['name'])
            print('只在完整重启后生效。脚本没有启动游戏。')
        elif args.action == 'archive':
            destination = session/'results'/stamp
            destination.mkdir(parents=True, exist_ok=False)
            for file in mod.iterdir():
                if file.is_file() and not file.is_symlink() and ('.log' in file.name or file.suffix=='.json' or file.name=='VERSION'):
                    shutil.copy2(file, destination/file.name)
            save_json(destination/'result.json', {'time': stamp, 'case': read(selector) if selector.exists() else None,
                      'target': data['target'], 'control': data['control'], 'result': '未填写，等待作者实机结果'})
            print('本轮日志已保存：'+str(destination))
        elif args.action == 'restore':
            baseline = Path(data['baseline']).resolve()
            records = read(baseline/'snapshot.json')
            for relative, expected in records['files'].items():
                require(digest(local_path(baseline/'game', relative))==expected, '恢复副本摘要不符：'+relative)
            require(read((game/'AC7AircraftProfiles')/'state.json')['profile']==data['profile_before'], '分组状态在准备期间变化。')
            closed()
            previous = session/'restore-history'/stamp
            previous.mkdir(parents=True, exist_ok=False)
            touched = []
            try:
                for relative in records['restore_roots']:
                    source = local_path(baseline/'game', relative)
                    target = local_path(game, relative)
                    saved = local_path(previous, relative)
                    if target.exists():
                        saved.parent.mkdir(parents=True, exist_ok=True)
                        os.rename(target, saved)
                    touched.append((target, saved))
                    target.parent.mkdir(parents=True, exist_ok=True)
                    if source.is_dir():
                        shutil.copytree(source, target)
                    elif source.is_file():
                        shutil.copy2(source, target)
                for relative, expected in records['files'].items():
                    require(digest(local_path(game, relative))==expected, '恢复后摘要不符：'+relative)
            except Exception:
                for index, (target, saved) in enumerate(reversed(touched)):
                    if target.exists():
                        os.rename(target, previous/('incomplete-'+str(index)))
                    if saved.exists():
                        os.rename(saved, target)
                raise
            print('已恢复测试前安装及分组；测试期间文件保存在：'+str(previous))
    finally:
        kernel.CloseHandle(lock)


if __name__ == '__main__':
    try:
        main()
    except (OSError, RuntimeError, subprocess.SubprocessError) as error:
        print('操作停止：'+str(error), file=sys.stderr)
        sys.exit(1)
