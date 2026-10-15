"""只构建 Su-30SM 独立研究候选并核对接口；不安装、不加载 DLL、不运行测试。"""

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys


def digest(path):
    """记录候选、来源和保护文件的 SHA256。"""
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    """所有编译产物限制在研究工作区的新 Temp 子目录。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tools-root", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]  # 只构建当前研究工作区。
    output = args.output_dir.resolve()  # 不覆盖此前候选或正式 DLL。
    temporary = (root / "Temp").resolve()
    if output == temporary or not output.is_relative_to(temporary):
        raise ValueError("候选输出必须是研究工作区 Temp 下的全新子目录")
    compiler = args.tools_root.resolve() / "llvm-mingw-20261006-ucrt-x86_64/bin"
    for name in ("clang.exe", "clang++.exe"):
        if not (compiler / name).is_file():
            raise FileNotFoundError(compiler / name)
    protected = {name: digest(root / name) for name in (
        "dist/DualSP/bin/DualSP.dll", "dist/DualSP/cores/1.0.1/DualSP.dll")}
    if protected["dist/DualSP/bin/DualSP.dll"].upper() != "D7D7055CA7909021EEACA15E63FF5FF253D2EB7E60A3FE3B79DCEEAB231DE339":
        raise ValueError("旧核心保护哈希不符")
    sys.path[:0] = [str(args.tools_root.resolve() / "test-python"), str(args.tools_root.resolve())]
    from lupa.lua54 import LuaRuntime
    import pefile
    lua = LuaRuntime(unpack_returned_tuples=True)  # 仅编译文本，不执行运行脚本。
    compile_only = lua.eval("function(s,n) local f,e=load(s,n,'t',{}); if not f then error(e) end; return true end")
    scripts = {}  # 本次候选 Lua 的来源清单。
    for path in sorted((root / "dist/DualSP/Scripts").glob("*.lua")):
        compile_only(path.read_text(encoding="utf-8"), "@" + str(path))
        scripts[path.relative_to(root).as_posix()] = digest(path)
    output.mkdir(parents=True, exist_ok=False)
    compiler_temp = output / "compiler-temp"  # 编译器的中间临时文件也限制在 Temp。
    compiler_temp.mkdir()
    environment = dict(os.environ, TMP=str(compiler_temp), TEMP=str(compiler_temp))
    minhook = root / "vendor/minhook-1.3.4"
    sources = [minhook / "src" / name for name in ("buffer.c", "hook.c", "trampoline.c", "hde/hde64.c")]
    subprocess.run([str(compiler / "clang.exe"), "-O2", "-c", "-I", str(minhook / "include"),
                    *map(str, sources)], cwd=output, env=environment, check=True)
    candidate = output / "DualSP.dll"  # DLL 只留在独立构建目录，不写 dist/bin。
    subprocess.run([str(compiler / "clang++.exe"), "-std=c++17", "-O2", "-shared", "-static",
                    "-DDUALSP_SU30SM_MOUNT_PROTOTYPE", "-I", str(root / "vendor"),
                    "-I", str(minhook / "include"), str(root / "src/weapon_bridge.cpp"),
                    "buffer.o", "hook.o", "trampoline.o", "hde64.o", "-lole32", "-o", str(candidate)],
                   cwd=output, env=environment, check=True)
    with pefile.PE(str(candidate)) as library:
        exports = {item.name.decode("ascii") for item in library.DIRECTORY_ENTRY_EXPORT.symbols if item.name}
        if library.FILE_HEADER.Machine != 0x8664:
            raise ValueError("候选核心不是 Windows x64")
    main_source = (root / "dist/DualSP/Scripts/main.lua").read_text(encoding="utf-8")
    required = set(re.findall(r'loadExport\("([^"]+)"\)', main_source))  # 排除明确指定 UI DLL 的接口。
    if "dualsp_fixed_mount_update" not in required or required - exports:
        raise ValueError("候选核心接口不完整：" + repr(sorted(required - exports)))
    for name, expected in protected.items():
        if digest(root / name) != expected:
            raise ValueError("构建期间保护文件改变：" + name)
    report = {"candidate": "1.0.2-su30sm-mount.2", "candidate_sha256": digest(candidate),
              "build_define": "DUALSP_SU30SM_MOUNT_PROTOTYPE", "compiled": True,
              "lua_syntax_only": True, "required_exports_verified": sorted(required),
              "game_installed": False, "game_validated": False, "test_modules_run": False,
              "installation_blockers": [], "native_research_evidence": "evidence/su30sm-fixed-mount-stage4.json",
              "protected_files": protected, "lua_sources": scripts,
              "cpp_sources": {name: digest(root / name) for name in ("src/weapon_bridge.cpp", "src/su30sm_fixed_mount.hpp")}}
    (output / "build-receipt.json").write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"candidate": str(candidate), "sha256": report["candidate_sha256"],
                      "status": "已构建；安装前须核对本阶段证据和正式安装文件"}, ensure_ascii=False))


if __name__ == "__main__":
    main()
