"""AC7MultiSP 唯一正式构建入口；整套构建核心与 UI，不安装、不启动游戏。"""

import argparse
import json
import os
from pathlib import Path
import re
import subprocess
import sys
from datetime import datetime
from release_tools import ROOT, product, source_snapshot, tree_fingerprint, contained, digest


def main():
    """整套构建，全部产物放在主工作区的新 Temp 子目录。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tools-root", type=Path, default=ROOT / "tools")
    parser.add_argument("--mount-assets-dir", type=Path, default=ROOT / "Temp/mount-assets-2.6.0/source",
                        help="按研究回执核对的16份AC7编码资源对，仅使用本地自有资源")
    parser.add_argument("--output-dir", type=Path, default=ROOT / "Temp" / ("build-" + datetime.now().strftime("%Y%m%d-%H%M%S-%f")))
    args = parser.parse_args()
    root = ROOT
    info = product()
    source_before = source_snapshot()
    output = contained(args.output_dir, root / "Temp").resolve()  # 不覆盖此前候选或正式 DLL。
    temporary = (root / "Temp").resolve()
    if output == temporary or not output.is_relative_to(temporary):
        raise ValueError("候选输出必须是主工作区 Temp 下的全新子目录")
    compiler = args.tools_root.resolve() / info["toolchain"] / "bin"
    for name in ("clang.exe", "clang++.exe"):
        if not (compiler / name).is_file():
            raise FileNotFoundError(compiler / name)
    toolchain_before = tree_fingerprint(compiler.parent)
    protected = {name: digest(root / name) for name in (
        "dist/DualSP/bin/DualSP.dll", "dist/DualSP/cores/1.0.1/DualSP.dll", "dist/DualSP/cores/1.1.0/DualSP.dll", "dist/DualSP/cores/2.0/DualSP.dll")}
    if protected["dist/DualSP/bin/DualSP.dll"].upper() != "D7D7055CA7909021EEACA15E63FF5FF253D2EB7E60A3FE3B79DCEEAB231DE339":
        raise ValueError("旧核心保护哈希不符")
    sys.path[:0] = [str(args.tools_root.resolve() / "test-python"), str(args.tools_root.resolve())]
    from lupa.lua54 import LuaRuntime
    import pefile
    lua = LuaRuntime(unpack_returned_tuples=True)  # 仅编译文本，不执行运行脚本。
    compile_only = lua.eval("function(s,n) local f,e=load(s,n,'t',{}); if not f then error(e) end; return true end")
    scripts = {}  # 本次产品 Lua 的来源清单。
    for path in sorted((root / "dist/DualSP/Scripts").glob("*.lua")):
        compile_only(path.read_text(encoding="utf-8"), "@" + str(path))
        scripts[path.relative_to(root).as_posix()] = digest(path)
    output.mkdir(parents=True, exist_ok=False)
    from build_mount_assets import build as build_mount_assets
    mount_assets = build_mount_assets(args.mount_assets_dir.resolve(strict=True), output)
    compiler_temp = output / "compiler-temp"  # 编译器的中间临时文件也限制在 Temp。
    compiler_temp.mkdir()
    environment = dict(os.environ, TMP=str(compiler_temp), TEMP=str(compiler_temp))
    minhook = root / "vendor/minhook-1.3.4"
    sources = [minhook / "src" / name for name in ("buffer.c", "hook.c", "trampoline.c", "hde/hde64.c")]
    commands = []
    def run(command, **kwargs):
        commands.append(command)
        return subprocess.run(command, **kwargs)
    run([str(compiler / "clang.exe"), "-O2", "-c", "-I", str(minhook / "include"),
                    *map(str, sources)], cwd=output, env=environment, check=True)
    candidate = output / "DualSP.dll"  # DLL 只留在独立构建目录，不写 dist/bin。
    # 每次完整构建；旧回执缺少依赖，不能作为复用凭据。
    run([str(compiler / "clang++.exe"), "-std=c++17", "-O2", "-shared", "-static",
         "-D" + info["core_define"], "-I", str(root / "vendor"),
         "-I", str(minhook / "include"), str(root / "src/weapon_bridge.cpp"),
         "buffer.o", "hook.o", "trampoline.o", "hde64.o", "-lole32", "-o", str(candidate)],
        cwd=output, env=environment, check=True)
    # 三 SP 管理协议与核心成套构建；所有中间文件仍在独立 Temp。
    imgui = root / "vendor/imgui-1.92.9b"
    ui_sources = [root / "src/overlay.cpp"] + [imgui / name for name in (
        "imgui.cpp", "imgui_draw.cpp", "imgui_tables.cpp", "imgui_widgets.cpp",
        "backends/imgui_impl_win32.cpp", "backends/imgui_impl_dx11.cpp")]
    run([str(compiler / "clang++.exe"), "-std=c++17", "-O2", "-c",
                    "-I", str(root / "vendor"), "-I", str(minhook / "include"),
                    "-I", str(imgui), "-I", str(imgui / "backends"), *map(str, ui_sources)],
                   cwd=output, env=environment, check=True)
    ui = output / "DualSPUI.dll"  # 不覆盖工作区中任何已验证的二进制。
    run([str(compiler / "clang++.exe"), "-shared", "-static",
                    *[path.stem + ".o" for path in ui_sources],
                    "buffer.o", "hook.o", "trampoline.o", "hde64.o", "-o", str(ui),
                    "-ld3d11", "-ldxgi", "-lxinput", "-ld3dcompiler", "-lgdi32", "-ldwmapi"],
                   cwd=output, env=environment, check=True)
    with pefile.PE(str(candidate)) as library:
        exports = {item.name.decode("ascii") for item in library.DIRECTORY_ENTRY_EXPORT.symbols if item.name}
        if library.FILE_HEADER.Machine != 0x8664:
            raise ValueError("候选核心不是 Windows x64")
    main_source = (root / "dist/DualSP/Scripts/main.lua").read_text(encoding="utf-8")
    required = set(re.findall(r'loadExport\("([^"]+)"\)', main_source))  # 排除明确指定 UI DLL 的接口。
    if "dualsp_fixed_mount_update" not in required or required - exports:
        raise ValueError("候选核心接口不完整：" + repr(sorted(required - exports)))
    ui_required = set(re.findall(r'loadExport\("([^"]+)","DualSPUI.dll"\)', main_source))
    with pefile.PE(str(ui)) as library:
        ui_exports = {item.name.decode("ascii") for item in library.DIRECTORY_ENTRY_EXPORT.symbols if item.name}
        if library.FILE_HEADER.Machine != 0x8664 or ui_required - ui_exports:
            raise ValueError("候选管理界面架构或接口不完整")
    for name, expected in protected.items():
        if digest(root / name) != expected:
            raise ValueError("构建期间保护文件改变：" + name)
    if source_before != source_snapshot() or toolchain_before != tree_fingerprint(compiler.parent):
        raise ValueError("构建期间来源或工具链发生变化，产物不能交付")
    report = {"receipt_schema": 2, "name": info["name"], "version": info["version"], "mount_assets": mount_assets,
              "source_files": source_before, "toolchain": toolchain_before, "toolchain_path": str(compiler.parent),
              "compiler_version": subprocess.check_output([str(compiler / "clang++.exe"), "--version"], text=True),
              "commands": commands, "objects": {p.name: digest(p) for p in sorted(output.glob('*.o'))},
              "candidate": info['version'], "candidate_sha256": digest(candidate),
              "ui_sha256": digest(ui), "required_ui_exports_verified": sorted(ui_required),
              "build_define": info["core_define"], "compiled": True,
              "core_recompiled": True, "core_reused_from": None,
              "lua_syntax_only": True, "required_exports_verified": sorted(required),
              "game_installed": False, "game_validated": False, "test_modules_run": False,
              "installation_blockers": [],
              "protected_files": protected, "lua_sources": scripts,
              "cpp_sources": {name: digest(root / name) for name in ("src/weapon_bridge.cpp", "src/su30sm_fixed_mount.hpp", "src/mount_mesh_override.hpp", "src/continuous_sp.hpp", "src/raven_sp.hpp", "src/keyboard_shortcuts.hpp", "src/overlay.cpp")}}
    (output / "build-receipt.json").write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"candidate": str(candidate), "sha256": report["candidate_sha256"],
                      "status": "已构建；安装前须核对本阶段证据和正式安装文件"}, ensure_ascii=False))


if __name__ == "__main__":
    main()
