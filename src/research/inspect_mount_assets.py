"""离线读取自有 AC7 资源，提取持续双 SP 研究线索；不连接游戏进程。"""

import argparse
import hashlib
import json
import os
import re
import struct
import subprocess
import sys
from datetime import datetime
from pathlib import Path

# 研究工作区和默认游戏资源位置；便携依赖由调用方显式指定。
ROOT = Path(__file__).resolve().parents[2]
PAKS = Path(r"E:\SteamLibrary\steamapps\common\ACE COMBAT 7\Game\Content\Paks")
PAK_NAMES = ("pakchunk0-WindowsNoEditor_0_P.pak", "pakchunk0-WindowsNoEditor.pak")
PATTERN = r"su30|plwp_6aam|plwp_lagm|plwp_qaam"


def signed32(number):
    """复现解码算法中的有符号 32 位运算。"""
    return (number + 2**31) % 2**32 - 2**31


def decode_copy(source, key):
    """仅解码临时副本，沿用项目已有的 UAssetAPI AC7 解码算法。"""
    # 由资源文件名计算密钥位置；uasset 和 uexp 共用连续的密钥游标。
    name_key = 0
    for character in source.stem.upper():
        name_key = signed32(name_key ^ ord(character))
        bit = (signed32(~signed32(signed32(name_key * 8) ^ name_key)) >> 7) & 1
        name_key = signed32(signed32(name_key * 2) | bit)
    first = (((name_key * 7) % 2**32) + 4) % 217
    second = (((name_key * 11) % 2**32) + 4) % 1024

    def decode(data):
        """按连续密钥游标解码一个数据段。"""
        nonlocal first, second
        # 新缓冲区避免改写原始游戏文件或提取的加密副本。
        result = bytearray(len(data))
        for index, byte in enumerate(data):
            result[index] = byte ^ key[first * 1024 + second] ^ 0x77
            first = (first + 1) % 217
            second = (second + 1) % 1024
        return result

    # 严格要求完整资源对，缺失或不支持的格式直接停止。
    asset_data = source.read_bytes()
    export_source = source.with_suffix(".uexp")
    if asset_data[:4] != b"ACE7" or not export_source.is_file():
        raise ValueError(f"资源格式不支持或缺少 uexp：{source}")
    decoded_asset = struct.pack("<I", 0x9E2A83C1) + decode(asset_data[4:])
    decoded_export = decode(export_source.read_bytes())
    decoded_export[-4:] = struct.pack("<I", 0x9E2A83C1)
    destination = source.parent / "decoded"
    destination.mkdir(exist_ok=True)
    target = destination / source.name
    target.write_bytes(decoded_asset)
    target.with_suffix(".uexp").write_bytes(decoded_export)
    return target


def main():
    """列出候选资源，或提取指定资源并导出供人工分析的 JSON。"""
    # 命令行限定只读输入，所有中间产物都放在工作区 Temp 中。
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tools-root", type=Path, required=True)
    parser.add_argument("--paks-root", type=Path, default=PAKS)
    parser.add_argument("--pattern", default=PATTERN)
    parser.add_argument("--asset", action="append", default=[])
    parser.add_argument("--decode-only", action="store_true", help="仅提取与解码副本，不调用 JSON 解析器")
    args = parser.parse_args()
    tools_root = args.tools_root.resolve(strict=True)
    # 与当前安装基线一致才解析，避免把不同版本资源混入布局结论。
    steam_manifest = args.paks_root.parents[4] / "appmanifest_502500.acf"
    build = re.search(r'"buildid"\s+"(\d+)"', steam_manifest.read_text(encoding="utf-8"))
    if not build or build[1] != "9855922":
        raise ValueError("Steam 构建号不匹配，停止解析")
    sys.path.insert(0, str(tools_root / "test-python"))
    sys.path.insert(0, str(ROOT / "src"))
    from inspect_hud_assets import inspect

    # 每次使用新目录，不覆盖旧研究结果；不会执行编译或测试。
    run_root = ROOT / "Temp" / ("mount-assets-" + datetime.now().strftime("%Y%m%d-%H%M%S-%f"))
    run_root.mkdir(parents=True, exist_ok=False)
    pattern = re.compile(args.pattern, re.IGNORECASE)
    selected_stems = set(args.asset)
    reports = []
    for pak_name in PAK_NAMES:
        # 索引读取只访问 PAK 目录；提取时仅匹配明确指定的资源名称。
        pak_path = args.paks_root / pak_name
        selector = lambda name: name.endswith(".uasset") and (
            Path(name).stem in selected_stems if selected_stems else bool(pattern.search(name))
        )
        index = inspect(pak_path, selector, index_only=True)
        reports.append(index)
        for asset in index["assets"]:
            if not selected_stems or Path(asset["name"]).stem in selected_stems:
                print(f"{pak_name}\t{asset['bytes']}\t{asset['name']}")
    (run_root / "index.json").write_text(json.dumps(reports, ensure_ascii=False, indent=2), encoding="utf-8")

    if selected_stems:
        # 核对名称唯一性，避免已有提取器的扁平目录覆盖同名文件。
        present_stems = set()
        for report in reports:
            found = [item for item in report["assets"] if Path(item["name"]).stem in selected_stems]
            stems = [Path(item["name"]).stem for item in found]
            if len(stems) != len(set(stems)):
                raise ValueError(f"同一 PAK 中资源名称重复：{report['pak']}")
            present_stems.update(stems)
        missing = selected_stems - present_stems
        if missing:
            raise ValueError(f"索引中缺少指定资源：{sorted(missing)}")

        # 依赖已存在于本机，不下载新工具；只调用无界面的 JSON 导出入口。
        key = (tools_root / "assets" / "AC7Key.bin").read_bytes()
        converter = tools_root / "assets" / "UAssetGUI.exe"
        # 采用当前主线已成功使用的本地运行库配置，不沿用旧研究遗漏环境的调用。
        runtime = tools_root / "dotnet8"
        environment = dict(os.environ, DOTNET_ROOT_X64=str(runtime))
        if not args.decode_only and not (runtime / "dotnet.exe").is_file():
            raise ValueError("缺少已验证的本地 .NET 运行库")
        if len(key) != 217 * 1024 or (not args.decode_only and not converter.is_file()):
            raise ValueError("缺少有效的本机资源解码依赖")
        manifest = []
        for pak_name in PAK_NAMES:
            # 资源对的提取和转换顺序执行，任何错误都显式停止。
            selector = lambda name: name.endswith((".uasset", ".uexp")) and Path(name).stem in selected_stems
            report = inspect(args.paks_root / pak_name, selector, run_root / "assets")
            for asset in report["assets"]:
                if not asset["name"].endswith(".uasset"):
                    continue
                source = decode_copy(Path(asset["file"]), key)
                output = source.with_suffix(".json")
                if args.decode_only:
                    manifest.append({
                        "pak": pak_name, "asset": asset["name"], "encrypted_sha256": asset["sha256"],
                        "decoded_sha256": hashlib.sha256(source.read_bytes()).hexdigest(),
                        "decoded": str(source), "json": None, "parsed": False,
                    })
                    continue
                result = subprocess.run(
                    [str(converter), "tojson", str(source), str(output), "VER_UE4_18"],
                    capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=60,
                    env=environment,
                    creationflags=subprocess.CREATE_NO_WINDOW,
                )
                if result.returncode != 0 or not output.is_file():
                    raise RuntimeError(f"资源 JSON 导出失败：{source}\n{result.stdout}\n{result.stderr}")
                document = json.loads(output.read_text(encoding="utf-8-sig"))
                if not document.get("Exports"):
                    raise ValueError(f"资源缺少导出结构：{source}")
                manifest.append({
                    "pak": pak_name, "asset": asset["name"], "encrypted_sha256": asset["sha256"],
                    "decoded_sha256": hashlib.sha256(source.read_bytes()).hexdigest(),
                    "json": str(output), "exports": len(document["Exports"]),
                })
        (run_root / "manifest.json").write_text(json.dumps(manifest, ensure_ascii=False, indent=2), encoding="utf-8")
    print(f"研究中间产物：{run_root}")


if __name__ == "__main__":
    main()
