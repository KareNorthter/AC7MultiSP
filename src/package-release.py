"""按显式载荷清单生成 AC7DualSP 手动安装包，不打包个人状态或旧模组副本。"""
import argparse
import hashlib
import json
import subprocess
import zipfile
from datetime import datetime, timezone
from pathlib import Path

# 当前成品复用已实测核心；旧正式核心和原音频分别核对固定值。
ROOT = Path(__file__).resolve().parents[1]
VERSION = "1.1.0"
NAME = "AC7DualSP"
CORE_HASH = "DA449197F3C2EE7A375589AB9475D454D6DA72D4419B652C1464D3CE388FA597"
OLD_CORE_HASH = "D7D7055CA7909021EEACA15E63FF5FF253D2EB7E60A3FE3B79DCEEAB231DE339"
WAVE_HASH = "278B8F8815823522ED5F121711A1A12EDA2AE8E89991F8522D1FFF22832D9640"
PWSH = r"C:\Program Files\PowerShell\7\pwsh.exe"
SCRIPTS = ("main.lua", "json.lua", "weapon_cycle.lua", "native_hud.lua", "loadout_flow.lua",
           "native_loadout.lua", "weapon_names.lua", "weapon_presentation.lua", "runtime_context.lua", "loadout_profile.lua")
LICENSES = ("UE4SS.txt", "DearImGui.txt", "MinHook.txt", "nlohmann-json.txt")


def digest(data):
    """给发布字节计算统一大写 SHA256。"""
    return hashlib.sha256(data).hexdigest().upper()


def microsoft_signature(path):
    """只读校验原安装程序签名，不执行安装，也不接受未知发布者。"""
    # LiteralPath 的单引号按 PowerShell 规则转义，不拼接可执行用户输入。
    literal = str(path).replace("'", "''")
    command = (f"$s=Get-AuthenticodeSignature -LiteralPath '{literal}'; "
               "$s | Select-Object @{n='status';e={$_.Status.ToString()}},"
               "@{n='signer';e={$_.SignerCertificate.Subject}} | ConvertTo-Json -Compress")
    result = subprocess.run([PWSH, "-NoProfile", "-NonInteractive", "-Command", command],
                            check=True, capture_output=True, text=True, encoding="utf-8")
    signature = json.loads(result.stdout)
    if signature["status"] != "Valid" or "CN=Microsoft Corporation," not in signature["signer"]:
        raise RuntimeError("微软安装程序签名不符合要求：" + str(path))
    return signature


def main():
    """收集显式文件、核对版本和依赖、打包并逐成员回读后才交付。"""
    arguments = argparse.ArgumentParser(description=__doc__)
    arguments.add_argument("--redist-root", type=Path, required=True,
                           help="Steamworks Shared 下的 _CommonRedist 目录")
    options = arguments.parse_args()
    if (ROOT / "VERSION").read_text(encoding="utf-8").strip() != VERSION:
        raise RuntimeError("发布脚本与 VERSION 不符，请先统一版本")
    controller = (ROOT / "dist/DualSP/Scripts/main.lua").read_text(encoding="utf-8")
    title = (ROOT / "src/overlay.cpp").read_text(encoding="utf-8")
    if f"DUALSP_LOADED {VERSION} MANAGEMENT_UI" not in controller or f"{NAME} {VERSION} 管理" not in title:
        raise RuntimeError("控制器或 UI 源码的版本未统一")
    # 要求本轮已构建的 UI 包含相同标题，防止只更新源码却打包旧 DLL。
    ui = ROOT / "dist/DualSP/bin/DualSPUI.dll"
    if f"{NAME} {VERSION} 管理".encode("utf-8") not in ui.read_bytes():
        raise RuntimeError("UI DLL 未包含当前正式标题，请先构建")
    build = json.loads((ROOT / f"evidence/{VERSION}-build.json").read_text(encoding="utf-8"))  # 只打包本轮冻结来源。
    for name, expected in (build["lua_sources"] | build["cpp_sources"] | build["protected_files"]).items():
        if digest((ROOT / name).read_bytes()) != expected.upper():
            raise RuntimeError("构建后来源已改变：" + name)
    if digest(ui.read_bytes()) != build["ui_sha256"].upper() or CORE_HASH != build["candidate_sha256"].upper():
        raise RuntimeError("发布 DLL 与构建回执不一致")

    # 只选择当前所需文件，明确绕开 payload 中的旧 Lua 与武器核心。
    core = ROOT / "dist/DualSP/cores/1.1.0/DualSP.dll"  # 原样沿用用户已验证的 .11 核心。
    wave = ROOT / "dist/DualSP/Audio/SP2.wav"
    for path, expected in ((core, CORE_HASH), (ROOT / "dist/DualSP/bin/DualSP.dll", OLD_CORE_HASH), (wave, WAVE_HASH)):
        if digest(path.read_bytes()) != expected:
            raise RuntimeError("受保护文件不匹配：" + str(path))
    mod_prefix = "GameFiles/ue4ss/Mods/DualSP/"
    sources = {
        "GameFiles/dwmapi.dll": ROOT / "payload/dwmapi.dll",
        "GameFiles/ue4ss/UE4SS.dll": ROOT / "payload/ue4ss/UE4SS.dll",
        "GameFiles/ue4ss/UE4SS-settings.ini": ROOT / "payload/ue4ss/UE4SS-settings.ini",
        mod_prefix + "bin/DualSP.dll": core,
        mod_prefix + "bin/DualSPUI.dll": ui,
        mod_prefix + "Audio/SP2.wav": wave,
        mod_prefix + "VERSION": ROOT / "VERSION",
        "安装说明.txt": ROOT / "docs/AC7DualSP-安装说明.txt",
        "版本说明.txt": ROOT / "docs/release-1.1.0.txt",
    }
    sources.update({mod_prefix + "Scripts/" + name: ROOT / "dist/DualSP/Scripts" / name for name in SCRIPTS})
    sources.update({mod_prefix + "licenses/" + name: ROOT / "payload/ue4ss/Mods/DualSP/licenses" / name for name in LICENSES})

    # 保留 Steam 提供的整个 DirectX 离线安装集合，只排除 Steam 自身安装描述。
    redist = options.redist_root.resolve(strict=True)
    vc = redist / "vcredist/2022/VC_redist.x64.exe"
    directx = redist / "DirectX/Jun2010"
    signatures = {"VC_redist.x64.exe": microsoft_signature(vc),
                  "DXSETUP.exe": microsoft_signature(directx / "DXSETUP.exe")}
    required_dx = {"dxsetup.exe", "dsetup.dll", "dsetup32.dll", "dxupdate.cab", "apr2007_xinput_x64.cab"}
    dx_files = [path for path in directx.iterdir() if path.is_file() and path.name.lower() != "installscript.vdf"]
    if not required_dx <= {path.name.lower() for path in dx_files}:
        raise RuntimeError("DirectX 离线安装文件不完整")
    if any(path.suffix.lower() not in (".exe", ".dll", ".cab") for path in dx_files):
        raise RuntimeError("DirectX 目录出现非安装文件，请先人工核对")
    sources["Runtimes/VC_redist.x64.exe"] = vc
    sources.update({"Runtimes/DirectX/" + path.name: path for path in dx_files})

    # 载荷放在内存中固定后再写 ZIP，避免文件在两次读取间变化。
    payload = {name: path.read_bytes() for name, path in sources.items()}
    payload["GameFiles/ue4ss/Mods/mods.txt"] = b"DualSP : 1\r\n"
    manifest = {"name": NAME, "version": VERSION, "files": {
        name: {"size": len(data), "sha256": digest(data)} for name, data in sorted(payload.items())}}
    payload["文件清单.json"] = (json.dumps(manifest, ensure_ascii=False, indent=2) + "\n").encode("utf-8")
    root_name = f"{NAME}-{VERSION}"
    output = ROOT / "Release" / (root_name + ".zip")
    if output.exists():
        raise RuntimeError("同名发布包已存在，不覆盖已交付快照：" + str(output))
    staging = ROOT / "Temp" / ("package-" + datetime.now().strftime("%Y%m%d-%H%M%S-%f"))
    staging.mkdir(parents=True)
    temporary = staging / output.name
    with zipfile.ZipFile(temporary, "x", compression=zipfile.ZIP_DEFLATED, compresslevel=6) as archive:
        for name, data in sorted(payload.items()):
            archive.writestr(root_name + "/" + name, data)

    # 逐成员解压到内存并核对，既验证压缩数据，也拒绝意外夹带文件。
    with zipfile.ZipFile(temporary) as archive:
        expected_names = {root_name + "/" + name for name in payload}
        if set(archive.namelist()) != expected_names or len(archive.infolist()) != len(expected_names):
            raise RuntimeError("ZIP 成员清单不一致")
        for name, data in payload.items():
            if digest(archive.read(root_name + "/" + name)) != digest(data):
                raise RuntimeError("ZIP 回读不一致：" + name)
    output.parent.mkdir(exist_ok=True)
    temporary.rename(output)
    # 发布回执独立于 ZIP，供 Git 留存；不包含用户配置或个人存档。
    result = {"name": NAME, "version": VERSION, "created_at": datetime.now(timezone.utc).isoformat(),
              "archive": output.relative_to(ROOT).as_posix(), "size": output.stat().st_size,
              "sha256": digest(output.read_bytes()), "member_count": len(payload),
              "all_members_verified": True, "test_modules_run": False,
              "runtime_source": "从已核验 1.0.2 发布包恢复的 Steam 离线运行库，逐文件哈希及微软签名复核，原安装文件未改",
              "microsoft_signatures": signatures, "files": manifest["files"]}
    (ROOT / f"evidence/{VERSION}-release.json").write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({key: value for key, value in result.items() if key != "files"}, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
