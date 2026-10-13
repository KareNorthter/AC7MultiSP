"""Decode copies of AC7 HUD assets, using UAssetAPI's documented MIT algorithm.

Reference: https://github.com/atenfyr/UAssetAPI/blob/master/UAssetAPI/AC7Decrypt.cs
Game files and executable are never modified.
"""
from pathlib import Path
import struct
import sys

ROOT = Path(__file__).resolve().parents[1]
key = (ROOT / "tools/assets/AC7Key.bin").read_bytes()
assert len(key) == 217 * 1024

def int32(n):
    return (n + 2**31) % 2**32 - 2**31

asset_root=Path(sys.argv[1]) if len(sys.argv)>1 else ROOT / "Temp/hud-assets"
for source in asset_root.glob("pakchunk*/*.uasset"):
    value = 0
    for c in source.stem.upper():
        value = int32(value ^ ord(c))
        bit = (int32(~int32(int32(value * 8) ^ value)) >> 7) & 1
        value = int32(int32(value * 2) | bit)
    p1 = ((value * 7) % 2**32 + 4) % 217
    p2 = ((value * 11) % 2**32 + 4) % 1024
    def decode(data):
        global p1, p2
        result = bytearray(len(data))
        for i, byte in enumerate(data):
            result[i] = byte ^ key[p1 * 1024 + p2] ^ 0x77
            p1 = (p1 + 1) % 217
            p2 = (p2 + 1) % 1024
        return result
    data = source.read_bytes()
    assert data[:4] == b"ACE7"
    asset = struct.pack("<I", 0x9E2A83C1) + decode(data[4:])
    export = decode(source.with_suffix(".uexp").read_bytes())
    export[-4:] = struct.pack("<I", 0x9E2A83C1)
    output = source.parent / "decoded"
    output.mkdir(exist_ok=True)
    (output / source.name).write_bytes(asset)
    (output / source.with_suffix(".uexp").name).write_bytes(export)
    print(source.name, len(asset), "legacy_version", struct.unpack_from("<i", asset, 4)[0])
