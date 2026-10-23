"""切换本机机体研究分组；只使用已准备且有摘要的原作者文件。"""
import argparse
import ctypes
from datetime import datetime
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess


def load(path):
    return json.loads(path.read_text(encoding='utf-8'))


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def game_closed():
    result = subprocess.run(
        [r'C:/Program Files/PowerShell/7/pwsh.exe', '-NoProfile', '-Command',
         '@(Get-Process Ace7Game* -ErrorAction SilentlyContinue).Count'],
        capture_output=True, text=True, check=True)
    require(result.stdout.strip() == '0', '请先退出游戏。 / Please close the game first.')


def switch(manifest_path, profile):
    manifest_path = manifest_path.resolve()
    manifest = load(manifest_path)
    home = manifest_path.parent
    mods = Path(manifest['mods']).resolve()
    require(profile in manifest['profiles'], '分组不存在。 / Unknown profile.')
    state_path = home/'state.json'
    state = load(state_path)
    game_closed()
    # 与现有安装器共用独占文件锁，不删除锁文件，也不改写其中内容。
    kernel = ctypes.WinDLL('kernel32', use_last_error=True)
    kernel.CreateFileW.argtypes = [ctypes.c_wchar_p, ctypes.c_uint32, ctypes.c_uint32,
                                  ctypes.c_void_p, ctypes.c_uint32, ctypes.c_uint32, ctypes.c_void_p]
    kernel.CreateFileW.restype = ctypes.c_void_p
    kernel.CloseHandle.argtypes = [ctypes.c_void_p]
    lock = kernel.CreateFileW(str(Path(manifest['game'])/'AC7MultiSP-install.lock'),
                              0xC0000000, 0, None, 4, 0x80, None)
    require(lock not in (None, ctypes.c_void_p(-1).value),
            '另一个安装或切换操作正在进行。 / Another installation or switch is running.')
    try:
        require(load(state_path) == state, '分组状态已变化。 / Profile state has changed.')
        changes = []
        desired = {}
        for entry in manifest['files']:
            identity = entry['id']
            target = (mods/entry['target']).resolve()
            require(target.is_relative_to(mods) and target.suffix.lower() == '.pak',
                    '目标路径越界。 / Target path is outside the mod directory.')
            require(not target.is_symlink(), '目标为链接，请先处理部署工具。 / Target is a link; check your deployment tool.')
            actual = digest(target) if target.is_file() else None
            require(actual == state['files'][identity],
                    '文件已有外部改动，停止切换： / External file change; switch stopped: '+str(target))
            variant = entry['variants'].get(profile)
            wanted = variant['sha256'] if variant else None
            desired[identity] = wanted
            if actual != wanted:
                if variant:
                    source = Path(variant['source'])
                    require(digest(source) == wanted, '原文件摘要不符： / Source hash mismatch: '+str(source))
                changes.append((entry, target, variant))
        if not changes:
            print('当前已启用该组。 / This profile is already active.')
            return
        transaction = home/'transactions'/datetime.now().strftime('%Y%m%d-%H%M%S-%f')
        transaction.mkdir(parents=True, exist_ok=False)
        shutil.copy2(state_path, transaction/'state-before.json')
        staged = transaction/'staged'
        previous = transaction/'previous'
        staged.mkdir(); previous.mkdir()
        for entry, target, variant in changes:
            if variant:
                candidate = staged/(entry['id']+'.pak')
                shutil.copy2(variant['source'], candidate)
                require(digest(candidate) == variant['sha256'],
                        '准备文件摘要不符。 / Staged file hash mismatch.')
        game_closed()
        for entry, target, variant in changes:
            actual = digest(target) if target.is_file() else None
            require(actual == state['files'][entry['id']],
                    '准备期间文件发生变化。 / A file changed during preparation.')
        touched = []
        try:
            for entry, target, variant in changes:
                identity = entry['id']
                saved = previous/(identity+'.pak')
                existed = target.exists()
                if existed:
                    os.replace(target, saved)
                touched.append((identity, target, saved, existed))
                if variant:
                    target.parent.mkdir(parents=True, exist_ok=True)
                    os.replace(staged/(identity+'.pak'), target)
                    require(digest(target) == variant['sha256'],
                            '部署文件摘要不符。 / Deployed file hash mismatch.')
            updated = dict(profile=profile, files=desired,
                           switched_at=datetime.now().astimezone().isoformat(), transaction=str(transaction))
            temporary = transaction/'state-after.json'
            temporary.write_text(json.dumps(updated, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
            shutil.copy2(temporary, transaction/'state-next.json')
            os.replace(transaction/'state-next.json', state_path)
        except Exception:
            failed = transaction/'failed'
            failed.mkdir(exist_ok=True)
            for identity, target, saved, existed in reversed(touched):
                if target.exists():
                    os.replace(target, failed/(identity+'.pak'))
                if existed:
                    os.replace(saved, target)
            shutil.copy2(transaction/'state-before.json', state_path)
            raise
        print('已启用 / Enabled: '+manifest['profiles'][profile]['label'])
        print('机体目录记录 / Aircraft catalogue rows: '+str(manifest['profiles'][profile]['catalogue_rows']))
        print('旧文件保留于 / Previous files retained in: '+str(transaction))
    finally:
        kernel.CloseHandle(lock)


def main():
    parser = argparse.ArgumentParser(description='机体分组切换 / Aircraft profile switcher')
    parser.add_argument('--manifest', required=True, type=Path, help='本机清单 / Local manifest')
    parser.add_argument('--profile', help='清单中登记的分组名称 / Profile name registered in the manifest')
    args = parser.parse_args()
    if args.profile:
        switch(args.manifest, args.profile)
    else:
        data = load(args.manifest)
        state = load(args.manifest.parent/'state.json')
        print('当前分组 / Current profile: '+data['profiles'][state['profile']]['label'])


if __name__ == '__main__':
    try:
        main()
    except Exception as error:
        print('操作停止 / Operation stopped: '+str(error))
        raise SystemExit(1)
