"""Read-only extraction of selected HUD assets from the user's own AC7 paks."""
import hashlib
import io
import json
import struct
import sys
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools/test-python"))
from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes

KEY = b"https://ace7.acecombat.jp/specia"
assert len(KEY) == 32

def decrypt(data):
    context = Cipher(algorithms.AES(KEY), modes.ECB()).decryptor()
    return context.update(data) + context.finalize()

def unpack(stream, fmt):
    return struct.unpack(fmt, stream.read(struct.calcsize(fmt)))

def string(stream):
    n, = unpack(stream, "<i")
    assert 0 < n < 4096
    return stream.read(n)[:-1].decode("utf-8")

def entry(stream):
    offset, size, original, compression = unpack(stream, "<QQQI")
    digest = stream.read(20)
    blocks = []
    if compression:
        count, = unpack(stream, "<I")
        assert count < 100000
        blocks = [unpack(stream, "<QQ") for _ in range(count)]
    flags, block_size = unpack(stream, "<BI")
    return dict(offset=offset, size=size, original=original, compression=compression,
                digest=digest.hex(), blocks=blocks, encrypted=bool(flags & 1), block_size=block_size)

def inspect(path, asset_filter=None, output_root=None, index_only=False):
    with path.open("rb") as pak:
        pak.seek(-256, 2)
        tail = pak.read()
        pos = tail.rfind(bytes.fromhex("e1126f5a"))
        assert pos > 0, "Missing pak footer"
        magic, version, index_offset, index_size = struct.unpack_from("<IIQQ", tail, pos)
        assert version == 4, f"Unsupported pak version {version}"
        encrypted_index = bool(tail[pos-1])
        pak.seek(index_offset)
        index = pak.read(index_size)
        if encrypted_index:
            index = decrypt(index)
        stream = io.BytesIO(index)
        mount = string(stream)
        count, = unpack(stream, "<i")
        assert 0 < count < 500000
        matches = []
        for _ in range(count):
            name, info = string(stream), entry(stream)
            selected=asset_filter(name) if asset_filter else (("AceMain" in name and "HUD" in name) or "CombinedStatsWidget." in name)
            if selected:
                matches.append((name, info))
        report = {"pak": path.name, "mount": mount, "count": count, "assets": []}
        if index_only:
            report['assets']=[{'name':name,'bytes':info['original']}for name,info in matches]
            return report
        output = (Path(output_root) if output_root else ROOT / "Temp/hud-assets") / path.stem
        output.mkdir(parents=True, exist_ok=True)
        for name, info in matches:
            pak.seek(info["offset"])
            header = entry(pak)
            assert header["size"] == info["size"]
            if info["compression"]:
                chunks = []
                assert info["compression"] == 1
                for start, end in info["blocks"]:
                    pak.seek(start)
                    data = pak.read(((end-start+15)//16)*16 if info["encrypted"] else end-start)
                    if info["encrypted"]:
                        data = decrypt(data)[:end-start]
                    chunks.append(zlib.decompress(data))
                data = b"".join(chunks)
            else:
                data = pak.read(((info["size"]+15)//16)*16 if info["encrypted"] else info["size"])
                if info["encrypted"]:
                    data = decrypt(data)
                data = data[:info["size"]]
            assert len(data) == info["original"]
            target = output / Path(name).name
            target.write_bytes(data)
            report["assets"].append({"name": name, "bytes": len(data), "sha256": hashlib.sha256(data).hexdigest(), "file": str(target)})
        return report

if __name__=='__main__':
    directory = Path(r"E:\SteamLibrary\steamapps\common\ACE COMBAT 7\Game\Content\Paks")
    reports = [inspect(directory / name) for name in ("pakchunk0-WindowsNoEditor_0_P.pak", "pakchunk0-WindowsNoEditor.pak")]
    (ROOT / "evidence/native-hud-asset-extraction.json").write_text(json.dumps(reports, indent=2), encoding="utf-8")
    print(json.dumps(reports, indent=2))
