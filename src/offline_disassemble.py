"""Inspect the verified AC7 executable on disk; never open the game/process."""
import json
import struct
import sys
from pathlib import Path

# 从当前项目读取便携依赖，旧实验目录不再存在。
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from capstone import Cs, CS_ARCH_X86, CS_MODE_64

exe = Path(r"E:\SteamLibrary\steamapps\common\ACE COMBAT 7\Ace7Game.exe").read_bytes()
pe = struct.unpack_from("<I", exe, 0x3C)[0]
count = struct.unpack_from("<H", exe, pe + 6)[0]
optional = struct.unpack_from("<H", exe, pe + 20)[0]
sections = []
for i in range(count):
    offset = pe + 24 + optional + i * 40
    virtual_size, rva, size, raw = struct.unpack_from("<IIII", exe, offset + 8)
    sections.append((rva, size, raw))

start, length = int(sys.argv[1], 16), int(sys.argv[2], 16)
for rva, size, raw in sections:
    if rva <= start and start + length <= rva + size:
        data = exe[raw + start - rva:raw + start - rva + length]
        break
else:
    raise ValueError("Range outside executable sections")
lines = [f"{i.address:#x}: {i.mnemonic} {i.op_str}" for i in Cs(CS_ARCH_X86, CS_MODE_64).disasm(data, start)]
if len(sys.argv) > 3:
    Path(sys.argv[3]).write_text(json.dumps(lines, indent=2), encoding="utf-8")
print("\n".join(lines))
