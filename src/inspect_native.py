"""Read-only inspection of known AC7 function pointers; never scans other apps."""
import argparse
import ctypes as C
from ctypes import wintypes as W
import json
from pathlib import Path
import struct
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from capstone import Cs, CS_ARCH_X86, CS_MODE_64

kernel = C.WinDLL("kernel32", use_last_error=True)
psapi = C.WinDLL("psapi", use_last_error=True)
kernel.OpenProcess.argtypes = [W.DWORD, W.BOOL, W.DWORD]
kernel.OpenProcess.restype = W.HANDLE
kernel.ReadProcessMemory.argtypes = [W.HANDLE, C.c_void_p, C.c_void_p, C.c_size_t, C.POINTER(C.c_size_t)]
kernel.ReadProcessMemory.restype = W.BOOL
kernel.CloseHandle.argtypes = [W.HANDLE]
psapi.EnumProcessModules.argtypes = [W.HANDLE, C.POINTER(W.HMODULE), W.DWORD, C.POINTER(W.DWORD)]
psapi.GetModuleFileNameExW.argtypes = [W.HANDLE, W.HMODULE, W.LPWSTR, W.DWORD]


class GameMemory:
    def __init__(self, pid):
        self.handle = kernel.OpenProcess(0x410, False, pid)
        if not self.handle:
            raise C.WinError(C.get_last_error())
        modules = (W.HMODULE * 1024)()
        needed = W.DWORD()
        if not psapi.EnumProcessModules(self.handle, modules, C.sizeof(modules), C.byref(needed)):
            raise C.WinError(C.get_last_error())
        name = C.create_unicode_buffer(32768)
        psapi.GetModuleFileNameExW(self.handle, modules[0], name, len(name))
        expected = Path(r"E:\SteamLibrary\steamapps\common\ACE COMBAT 7\Ace7Game.exe")
        assert Path(name.value) == expected, "Refusing to read a different executable"
        self.base = modules[0]

    def read(self, address, size):
        data = C.create_string_buffer(size)
        count = C.c_size_t()
        if not kernel.ReadProcessMemory(self.handle, address, data, size, C.byref(count)):
            raise C.WinError(C.get_last_error())
        assert count.value == size
        return data.raw

    def close(self):
        kernel.CloseHandle(self.handle)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("pid", type=int)
    parser.add_argument("--rva", nargs="*", type=lambda s: int(s, 16))
    parser.add_argument("--size", type=int, default=192)
    args = parser.parse_args()
    mem = GameMemory(args.pid)
    dis = Cs(CS_ARCH_X86, CS_MODE_64)
    report = []
    try:
        if args.rva:
            for rva in args.rva:
                body = list(dis.disasm(mem.read(mem.base+rva, args.size), mem.base+rva))
                lines = [f"{i.address-mem.base:#x}: {i.mnemonic} {i.op_str}" for i in body]
                report.append({"native_rva": hex(rva), "instructions": lines})
            (ROOT / "evidence" / ("native-body-" + hex(args.rva[0]) + ".json")).write_text(json.dumps(report, indent=2), encoding="utf-8")
            print(json.dumps(report, indent=2))
            return
        for row in (ROOT / "evidence/function-addresses.tsv").read_text(encoding="utf-8").splitlines():
            name, address = row.split("\t")
            # Func offset is measured by UE4SS in this exact process, not guessed.
            native = struct.unpack("<Q", mem.read(int(address, 16) + 0xB0, 8))[0]
            body = list(dis.disasm(mem.read(native, 192), native))
            lines = [f"{i.address-mem.base:#x}: {i.mnemonic} {i.op_str}" for i in body]
            report.append({"function": name, "native_rva": hex(native-mem.base), "instructions": lines})
        (ROOT / "evidence/native-functions.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
        print(json.dumps(report, indent=2))
    finally:
        mem.close()


if __name__ == "__main__":
    main()
