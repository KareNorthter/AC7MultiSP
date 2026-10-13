"""Inspect and load the UI DLL without starting AC7 or enabling its hooks."""
import ctypes
import hashlib
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import pefile

path = ROOT / 'dist/DualSP/bin/DualSPUI.dll'
pe = pefile.PE(str(path))
exports = {item.name.decode() for item in pe.DIRECTORY_ENTRY_EXPORT.symbols if item.name}
expected = {'dualsp_publish', 'dualsp_drain_actions', 'dualsp_overlay_start'}
assert expected <= exports, exports
imports = sorted(item.dll.decode() for item in pe.DIRECTORY_ENTRY_IMPORT)
assert not any('libc++' in name or 'libgcc' in name or 'libwinpthread' in name for name in imports), imports
assert pe.FILE_HEADER.Machine == 0x8664
dll = ctypes.WinDLL(str(path))
for name in expected:
    getattr(dll, name)
core = ROOT / 'dist/DualSP/bin/DualSP.dll'
core_hash = hashlib.sha256(core.read_bytes()).hexdigest().upper()
assert core_hash == 'D7D7055CA7909021EEACA15E63FF5FF253D2EB7E60A3FE3B79DCEEAB231DE339'
result = {'exports': sorted(exports), 'imports': imports, 'dll_loaded': True,
          'ui_sha256': hashlib.sha256(path.read_bytes()).hexdigest(),
          'tested_core_preserved': True, 'game_started': False}
(ROOT / 'evidence/0.1.7-ui-binary-tests.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
print(json.dumps(result, indent=2))
