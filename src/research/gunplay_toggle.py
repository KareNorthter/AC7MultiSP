"""本机一键启用或停用已核对的 Enhanced Gunplay 主文件。"""
import ctypes
import hashlib
import os
from pathlib import Path
import shutil
import subprocess
import sys


GAME = Path(r'E:\SteamLibrary\steamapps\common\ACE COMBAT 7')
MODS = GAME / 'Game/Content/Paks/~mods'
MANAGEMENT = Path(r'E:\AC7_MOD\管理入口')
STORE = MANAGEMENT / 'Gunplay停用仓库'
NAME = '!EOM_EnhancedGunplay_A5_P.pak'
ACTIVE = MODS / NAME
INACTIVE = STORE / NAME
PENDING = STORE / (NAME + '.partial')
BACKUP = Path(r'E:\AC7_MultiSP\backup\gunplay-hangar-isolation-20261021-230355') / NAME
EXPECTED = 'e46a28ad1e64c0c079e859183d8f4b54793692d221dd089e7fa72ef54e39f8c0'


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def sha256(path):
    require(path.is_file() and not path.is_symlink(), '文件不存在或是链接：' + str(path))
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def game_closed():
    result = subprocess.run([str(Path(os.environ['SystemRoot']) / 'System32/tasklist.exe'),
                             '/FO', 'CSV', '/NH'], capture_output=True, check=True, timeout=15)
    require(b'ace7game' not in result.stdout.lower(), '请先退出游戏，再运行脚本。')


def gunplay_files():
    def onerror(error):
        raise error

    matches = []
    for directory, subdirs, files in os.walk(MODS, onerror=onerror, followlinks=False):
        for name in subdirs:
            require(not (Path(directory) / name).is_symlink() and not (Path(directory) / name).is_junction(),
                    'Mod目录包含文件夹链接，无法安全盘点：' + str(Path(directory) / name))
        for name in files:
            if 'gunplay' in name.casefold() and name.casefold().endswith('.pak'):
                matches.append(Path(directory) / name)
    return matches


def check_environment():
    require(GAME.is_dir() and MODS.is_dir() and MANAGEMENT.is_dir(), '游戏或管理入口目录不存在。')
    require(MODS.resolve().is_relative_to(GAME.resolve()), '游戏Mod目录指向了游戏目录之外。')
    require(STORE.resolve().is_relative_to(MANAGEMENT.resolve()), '停用仓库指向了管理目录之外。')
    require(not STORE.is_symlink() and not STORE.is_junction(), '停用仓库是链接，请先检查。')
    found = gunplay_files()
    unexpected = [p for p in found if str(p).casefold() != str(ACTIVE).casefold()]
    require(not unexpected, '发现其他Gunplay文件或补丁，请先人工核对：\n' + '\n'.join(map(str, unexpected)))
    require(not ACTIVE.is_symlink() and not INACTIVE.is_symlink(), 'Gunplay文件是链接，请先检查部署工具。')
    require(not PENDING.exists(), '发现未完成的准备文件，请先人工核对：' + str(PENDING))
    require(not (ACTIVE.exists() and INACTIVE.exists()), '启用目录和停用仓库都有主文件，请先人工核对。')


def toggle():
    game_closed()
    kernel = ctypes.WinDLL('kernel32', use_last_error=True)
    kernel.CreateFileW.argtypes = [ctypes.c_wchar_p, ctypes.c_uint32, ctypes.c_uint32,
                                   ctypes.c_void_p, ctypes.c_uint32, ctypes.c_uint32, ctypes.c_void_p]
    kernel.CreateFileW.restype = ctypes.c_void_p
    kernel.CloseHandle.argtypes = [ctypes.c_void_p]
    handle = kernel.CreateFileW(str(GAME / 'AC7MultiSP-install.lock'), 0xC0000000, 0, None, 4, 0x80, None)
    require(handle not in (None, ctypes.c_void_p(-1).value), '另一项安装或切换正在进行，请稍后再试。')
    try:
        check_environment()
        STORE.mkdir(parents=True, exist_ok=True)
        if ACTIVE.exists():
            require(sha256(ACTIVE) == EXPECTED, '启用文件与已核对的版本不同，停止切换。')
            game_closed()
            check_environment()
            os.rename(ACTIVE, INACTIVE)
            require(sha256(INACTIVE) == EXPECTED and not ACTIVE.exists(), '停用后文件核对失败，请检查停用仓库。')
            print('Gunplay 已停用。主文件保存在：' + str(INACTIVE))
        else:
            if INACTIVE.exists():
                require(sha256(INACTIVE) == EXPECTED, '停用文件与已核对的版本不同，停止切换。')
                prepared = INACTIVE
            else:
                require(sha256(BACKUP) == EXPECTED, '历史原件与已核对的版本不同，停止启用。')
                with BACKUP.open('rb') as source, PENDING.open('xb') as target:
                    shutil.copyfileobj(source, target, 4 * 1024 * 1024)
                    target.flush()
                    os.fsync(target.fileno())
                require(sha256(PENDING) == EXPECTED, '准备文件摘要不符，请检查：' + str(PENDING))
                prepared = PENDING
            game_closed()
            # 准备文件本轮自行创建；其他状态仍要在放入游戏前再次核对。
            if prepared == PENDING:
                require(not ACTIVE.exists() and not INACTIVE.exists() and not gunplay_files(),
                        '准备期间Gunplay文件发生变化，请先人工核对。')
            else:
                check_environment()
            os.rename(prepared, ACTIVE)
            require(sha256(ACTIVE) == EXPECTED, '启用后文件核对失败，请检查游戏Mod目录。')
            print('Gunplay 已启用，使用此前确认的低优先级名称。')
            print('其他Mod可能覆盖Gunplay的部分效果，具体表现需进入游戏确认。')
    finally:
        kernel.CloseHandle(handle)


if __name__ == '__main__':
    try:
        toggle()
    except (OSError, RuntimeError, subprocess.SubprocessError) as error:
        print('未完成切换：' + str(error), file=sys.stderr)
        sys.exit(1)
