"""新版发布的本地准备与核对；仅build命令编译，不登录或上传任何服务。"""
from __future__ import annotations

import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys
from urllib.parse import quote, urlparse
from urllib.request import Request, urlopen
import zipfile

from build_pages_site import ROOT, PLAYER_DOC_FILES, SITE_FILES, build as build_site, validate_manifest

EXE = "AC7MultiSP_Installer.exe"


def digest(path: Path) -> str:
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def read(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8-sig"))


def write(path: Path, value: object) -> None:
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2) + "\n", encoding="utf-8", newline="\n")


def temp_roots() -> tuple[Path, ...]:
    common = Path(subprocess.check_output(
        ["git", "-C", str(ROOT), "rev-parse", "--path-format=absolute", "--git-common-dir"],
        text=True, encoding="utf-8").strip())
    return ((ROOT / "Temp").resolve(), (common.parent / "Temp").resolve())


def fresh(path: Path) -> Path:
    path = path.resolve()
    if path.exists():
        raise ValueError(f"输出已存在，拒绝覆盖：{path}")
    # 发布准备不能写回源码、旧Release或备份；每轮使用新任务目录。
    if not any(path.is_relative_to(root) and path != root for root in temp_roots()):
        raise ValueError("输出必须是当前仓库Temp下的全新子目录")
    path.mkdir(parents=True)
    return path


def resolve(path: str | Path) -> Path:
    path = Path(path)
    return path.resolve() if path.is_absolute() else (ROOT / path).resolve()


def checked_url(value: str, host: str) -> str:
    value = value.strip().rstrip("，,。")
    value.encode("ascii")
    parsed = urlparse(value)
    if (parsed.scheme != "https" or parsed.hostname != host or parsed.port not in (None, 443)
            or parsed.username or parsed.password or parsed.fragment
            or any(ord(c) <= 32 or c in {'"', "'", "\\"} for c in value)):
        raise ValueError(f"下载地址必须是已登记的HTTPS主机：{host}")
    return value


def notes_for(version: str, language: str) -> str:
    suffix = ".en" if language == "en" else ""
    text = (ROOT / f"docs/user/更新日志{suffix}.txt").read_text(encoding="utf-8-sig")
    series = ".".join(version.split(".")[:2]) + ".X"
    found = re.search(r"(?m)^" + re.escape(series) + r"[^\n]*\n(.*?)(?=^\d+\.\d+\.(?:X|\d+)|\Z)", text, re.S)
    if not found or not found[1].strip():
        raise ValueError(f"唯一更新日志中缺少{series}的{language}摘要")
    return found[1].strip()


def compile_delivery(args: argparse.Namespace) -> None:
    output = fresh(args.output_dir)
    game, package = output / "game-build", output / "delivery"
    command = [sys.executable, str(ROOT / "src/build.py"), "--mount-assets-dir", str(args.mount_assets_dir.resolve()),
               "--temp-root", str(next(root for root in temp_roots() if output.is_relative_to(root))),
               "--output-dir", str(game), "--tools-root", str(args.tools_root.resolve())]
    subprocess.run(command, cwd=ROOT, check=True)
    subprocess.run([sys.executable, str(ROOT / "src/package_delivery.py"), "--build-dir", str(game),
                    "--output-dir", str(package), "--audio-source", str(args.audio_source.resolve())], cwd=ROOT, check=True)
    subprocess.run([args.pwsh, "-NoProfile", "-File", str(ROOT / "src/delivery/build_setup.ps1"),
                    "-InputDirectory", str(package)], cwd=ROOT, check=True)
    packaged, exe_receipt = read(package / "封装回执.json"), read(package / "EXE编译回执.json")
    artifact = package / EXE
    if digest(artifact) != exe_receipt["sha256"]:
        raise ValueError("EXE与编译回执不一致")
    receipt = {"schema": 1, "version": packaged["version"], "candidate": packaged["candidate"],
               "artifact": {"path": str(artifact), "size": artifact.stat().st_size, "sha256": digest(artifact)},
               "package": {"path": str(package), "receipt_sha256": digest(package / "封装回执.json"),
                           "exe_receipt_sha256": digest(package / "EXE编译回执.json"),
                           "internal_zip_sha256": packaged["universal_sha256"]},
               "tests_run": False, "installed": False, "game_started": False, "uploaded": False}
    write(output / "delivery-receipt.json", receipt)
    print(f"完整构建和EXE封装完成：{output / 'delivery-receipt.json'}")


def prepare(args: argparse.Namespace) -> None:
    receipt_path = args.receipt.resolve()
    receipt = read(receipt_path)
    version = receipt["version"]
    if not re.fullmatch(r"\d+\.\d+\.\d+", version):
        raise ValueError("发布版本必须是三段数字")
    artifact = resolve(receipt["artifact"]["path"])
    expected = receipt["artifact"]
    if artifact.stat().st_size != expected["size"] or digest(artifact) != expected["sha256"]:
        raise ValueError("现有EXE与交付回执不一致；不以重建掩盖差异")
    with artifact.open("rb") as stream:
        if stream.read(2) != b"MZ":
            raise ValueError("发布文件不是EXE")
    packaged_info = receipt.get("package", {})
    package = resolve(packaged_info["path"]) if packaged_info.get("path") else None
    for field, filename in (("receipt_sha256", "封装回执.json"), ("exe_receipt_sha256", "EXE编译回执.json")):
        if packaged_info.get(field) and (package is None or digest(package / filename) != packaged_info[field]):
            raise ValueError(f"历史{filename}摘要不一致")
    defaults = read(ROOT / "site/stable.json")
    if version != defaults.get("latest_version") and not args.lanzou_url:
        raise ValueError("发布新版本须显式提供本次--lanzou-url，不能自动沿用上一版分享页")
    default_r2 = next(x["url"] for x in defaults["downloads"] if x["id"] == "r2")
    default_lanzou = [x["url"] for x in defaults["downloads"] if x["id"].startswith("lanzou")]
    r2 = checked_url(args.r2_url or default_r2, "downloads.iot-node-service-api.online")
    lanzou = [checked_url(x, "wwbru.lanzoue.com") for x in (args.lanzou_url or default_lanzou)]
    if not 1 <= len(lanzou) <= 3 or len(set(lanzou)) != len(lanzou):
        raise ValueError("需要一至三个不同的蓝奏云备用链接")
    output = fresh(args.output_dir)
    upload = output / "upload"; upload.mkdir()
    shutil.copy2(artifact, upload / EXE)
    if digest(upload / EXE) != expected["sha256"]:
        raise ValueError("复制后的EXE摘要不一致")
    nexus_package = None
    if not args.exe_only:
        if package is None:
            raise ValueError("回执缺少通用ZIP来源目录")
        packaged = read(package / "封装回执.json")
        if packaged["version"] != version or packaged["universal_sha256"] != packaged_info["internal_zip_sha256"]:
            raise ValueError("通用ZIP版本或摘要与EXE交付来源不一致")
        archive = package / packaged["universal_zip"]
        if digest(archive) != packaged["universal_sha256"]:
            raise ValueError("通用ZIP与封装回执不一致")
        shutil.copy2(archive, upload / archive.name)
        nexus_package = {"filename": archive.name, "size": archive.stat().st_size,
                         "sha256": digest(archive), "uploaded": False}
    manifest = {"schema": 1, "product": "AC7MultiSP", "channel": "stable", "status": "ready", "latest_version": version,
                "release_notes": {"zh": notes_for(version, "zh"), "en": notes_for(version, "en")},
                "downloads": [{"id": "r2", "label": {"zh": "主要下载 · R2安装程序", "en": "Primary · R2 installer"}, "url": r2}]
                    + [{"id": "lanzou" if i == 0 else f"lanzou_{i+1}", "label": {"zh": f"备用下载 · 蓝奏云{i+1}", "en": f"Backup · Lanzou {i+1}"}, "url": u} for i, u in enumerate(lanzou)]}
    write(output / "stable.ready.json", manifest)
    validate_manifest(output / "stable.ready.json")
    plan = {"schema": 1, "version": version, "publication_channel": "stable", "stable_confirmed_by_operator": args.confirm_stable,
            "source_receipt": str(receipt_path),
            "source_receipt_sha256": digest(receipt_path), "original_receipt_candidate": receipt.get("candidate"),
            "artifact": {"filename": EXE, "size": expected["size"], "sha256": expected["sha256"]},
            "nexus_package": nexus_package,
            "player_documents": {name: digest(ROOT / "docs/user" / name) for name in PLAYER_DOC_FILES},
            "manifest_sha256": digest(output / "stable.ready.json"), "r2_url": r2, "lanzou_urls": lanzou,
            "r2_object_key": urlparse(r2).path.lstrip("/"),
            "r2_optional_version_key": f"releases/{version}/{EXE}", "built_by_this_command": False,
            "uploaded": False, "game_validated_by_this_command": False}
    write(output / "release-plan.json", plan)
    (output / "SHA256SUMS.txt").write_text("".join(f"{digest(p)}  upload/{p.name}\n" for p in sorted(upload.iterdir())), encoding="utf-8")
    steps = f"""本次发布：AC7MultiSP {version}

1. 将upload/{EXE}同一个文件上传R2和蓝奏云，不解压、不重新压缩。
2. R2固定对象键：{plan['r2_object_key']}。可先将同文件另存为{plan['r2_optional_version_key']}，再替换固定入口，保留回退版本。
3. 蓝奏云上传后取得本次真实分享链接。若链接变化，用finalize的--lanzou-url参数传入，可重复一至三次；不要把网页地址当作直接文件下载。
4. 将upload/{nexus_package['filename'] if nexus_package else '（本轮未准备通用ZIP）'}手动上传到Nexus本Mod文件页，核对版本{version}及适用说明；不要把Pages ZIP或EXE代替通用ZIP。Nexus下载说明使用同套中英文玩家正文，不再维护另一份说明。
5. 下载R2固定入口核对SHA256；finalize可自动流式核对。蓝奏云及Nexus请人工下载核对SHA256SUMS.txt。
6. 运行finalize准备完整Pages目录和ZIP，再上传Pages；不要提前部署stable.ready.json草稿。
7. Pages部署后运行verify-online核对线上stable/preview与本次准备目录，再核对浏览器双语和客户端提示。

EXE摘要：{expected['sha256']}
R2：{r2}
蓝奏云：{', '.join(lanzou)}

此目录仅为本地发布准备，没有代你上传、安装或宣布游戏实测通过。
"""
    (output / "手动发布步骤.txt").write_text(steps, encoding="utf-8", newline="\n")
    print(f"上传文件和发布计划已准备：{output}")


def remote_bytes(url: str, maximum: int) -> bytes:
    request = Request(url, headers={"User-Agent": "AC7MultiSP-release-workflow", "Cache-Control": "no-cache"})
    with urlopen(request, timeout=30) as response:
        if urlparse(response.url).scheme != "https" or urlparse(response.url).hostname != urlparse(url).hostname:
            raise ValueError("远端发生跨主机或非HTTPS跳转，停止核验")
        data = response.read(maximum + 1)
        if len(data) > maximum:
            raise ValueError("远端内容超过核验大小限制")
        return data


def verify_r2(plan: dict) -> dict:
    expected = plan["artifact"]
    h = hashlib.sha256(); size = 0
    request = Request(plan["r2_url"], headers={"User-Agent": "AC7MultiSP-release-workflow", "Cache-Control": "no-cache"})
    with urlopen(request, timeout=30) as response:
        if urlparse(response.url).scheme != "https" or urlparse(response.url).hostname != urlparse(plan["r2_url"]).hostname:
            raise ValueError("R2下载跳转到未登记的主机")
        while chunk := response.read(1024 * 1024):
            size += len(chunk)
            if size > expected["size"]:
                raise ValueError("R2文件大于本次EXE，停止读取")
            h.update(chunk)
    if size != expected["size"] or h.hexdigest() != expected["sha256"]:
        raise ValueError("R2文件与本次EXE不一致；先上传正确文件，不更新Pages")
    return {"size": size, "sha256": expected["sha256"], "verified": True, "method": "streaming-download"}


def finalize(args: argparse.Namespace) -> None:
    prepared = args.prepared_dir.resolve()
    plan = read(prepared / "release-plan.json")
    if digest(prepared / "upload" / EXE) != plan["artifact"]["sha256"] or digest(prepared / "stable.ready.json") != plan["manifest_sha256"]:
        raise ValueError("准备文件已改变，请重新prepare到新目录")
    manifest = read(prepared / "stable.ready.json")
    for name, expected in plan["player_documents"].items():
        if digest(ROOT / "docs/user" / name) != expected:
            raise ValueError("玩家正文在prepare后发生变化，请重新prepare到新目录")
    nexus = plan.get("nexus_package")
    if nexus and digest(prepared / "upload" / nexus["filename"]) != nexus["sha256"]:
        raise ValueError("Nexus通用ZIP在prepare后发生变化")
    if args.lanzou_url:
        urls = [checked_url(x, "wwbru.lanzoue.com") for x in args.lanzou_url]
        if not 1 <= len(urls) <= 3 or len(set(urls)) != len(urls):
            raise ValueError("备用链接数量或唯一性无效")
        manifest["downloads"] = [x for x in manifest["downloads"] if x["id"] == "r2"] + [
            {"id": "lanzou" if i == 0 else f"lanzou_{i+1}", "label": {"zh": f"备用下载 · 蓝奏云{i+1}", "en": f"Backup · Lanzou {i+1}"}, "url": u} for i, u in enumerate(urls)]
    if args.manual_r2_sha256:
        if args.manual_r2_sha256.lower() != plan["artifact"]["sha256"]:
            raise ValueError("人工下载的R2摘要与EXE不一致")
        remote = {"verified": True, "method": "operator-provided-download-sha256"}
    else:
        remote = verify_r2(plan)
    output = fresh(args.output_dir)
    write(output / "stable.json", manifest)
    build_site(output / "pages", output / "stable.json")
    archive = output / "AC7MultiSP-Pages-homepage.zip"
    members = {}
    with zipfile.ZipFile(archive, "x", zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        for p in sorted((output / "pages").rglob("*")):
            if p.is_file():
                name = p.relative_to(output / "pages").as_posix(); z.write(p, name)
                members[name] = {"size": p.stat().st_size, "sha256": digest(p)}
    with zipfile.ZipFile(archive) as z:
        if z.testzip() is not None or set(z.namelist()) != set(members):
            raise ValueError("Pages ZIP回读不一致")
        for name, record in members.items():
            if hashlib.sha256(z.read(name)).hexdigest() != record["sha256"]:
                raise ValueError("Pages成员摘要不一致")
    receipt = {"schema": 1, "version": plan["version"], "prepared_dir": str(prepared), "release_plan_sha256": digest(prepared / "release-plan.json"),
               "original_receipt_candidate": plan["original_receipt_candidate"], "r2": remote,
               "lanzou_urls_supplied": [x["url"] for x in manifest["downloads"] if x["id"].startswith("lanzou")],
               "lanzou_binary_verified": False, "pages_zip_sha256": digest(archive), "members": members,
               "nexus_package": nexus, "nexus_binary_verified": False,
               "cloudflare_deployed": False, "tests_run": False, "installed": False, "generated_at": datetime.now(timezone.utc).isoformat()}
    write(output / "publication-receipt.json", receipt)
    (output / "上传Cloudflare前请读.txt").write_text(
        "请人工完成蓝奏云EXE及Nexus通用ZIP上传并核对下载摘要，再将AC7MultiSP-Pages-homepage.zip或pages目录内容上传到现有Pages项目。\n"
        "Pages会替换整站；请保留旧部署，上传完整包，不单独上传stable.json。\n"
        "本工具已核对R2和本地ZIP，尚未部署或核对蓝奏云、Nexus文件、浏览器排版、游戏联网。\n", encoding="utf-8")
    print(f"完整Pages上传包已准备：{archive}")


def verify_online(args: argparse.Namespace) -> None:
    folder = args.pages_dir.resolve()
    receipt = args.receipt.resolve()
    if receipt.exists():
        raise ValueError("线上核对回执已存在，请使用新文件名")
    if not any(receipt.is_relative_to(root) for root in temp_roots()):
        raise ValueError("线上核对回执必须放在仓库Temp下")
    if not receipt.parent.is_dir():
        raise ValueError("请先准备回执所在目录")
    base = args.base_url.rstrip("/") + "/"
    checked_url(base, "ac7multisp-updates.pages.dev")
    names = [name for name in SITE_FILES if name != "_headers"] + ["user-docs/" + name for name in PLAYER_DOC_FILES]
    results = {}
    for name in names:
        local = folder / name
        remote = remote_bytes(base + quote(name), local.stat().st_size)
        results[name] = hashlib.sha256(remote).hexdigest() == digest(local)
    write(receipt, {"verified_at": datetime.now(timezone.utc).isoformat(), "url": base,
                                  "files_equal": results, "all_equal": all(results.values()), "browser_or_game_checked": False})
    if not all(results.values()):
        raise ValueError("线上内容与本次目录不一致，保留核对记录并检查部署")
    print("线上五个核心文件及八份玩家正文与本次目录一致；未替代响应头、浏览器和游戏验证。")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    build = commands.add_parser("build", help="显式编译游戏、封装通用ZIP和EXE；不安装测试")
    build.add_argument("--mount-assets-dir", type=Path, required=True)
    build.add_argument("--output-dir", type=Path, required=True)
    build.add_argument("--tools-root", type=Path, default=ROOT / "tools")
    build.add_argument("--audio-source", type=Path, default=ROOT / "dist/DualSP/Audio/SP2.wav")
    build.add_argument("--pwsh", default=r"C:\Program Files\PowerShell\7\pwsh.exe")
    build.set_defaults(handler=compile_delivery)
    prep = commands.add_parser("prepare", help="复用有摘要回执的现成EXE，准备人工上传材料")
    prep.add_argument("--receipt", type=Path, required=True)
    prep.add_argument("--output-dir", type=Path, required=True)
    prep.add_argument("--confirm-stable", action="store_true", required=True, help="明确确认本次版本获准正式发布；不代表游戏验收")
    prep.add_argument("--r2-url")
    prep.add_argument("--lanzou-url", action="append")
    prep.add_argument("--exe-only", action="store_true", help="仅明确不更新Nexus文件时省略通用ZIP；完整发布默认同时准备ZIP")
    prep.set_defaults(handler=prepare)
    final = commands.add_parser("finalize", help="核对R2并生成完整Pages ZIP；不上传")
    final.add_argument("--prepared-dir", type=Path, required=True)
    final.add_argument("--output-dir", type=Path, required=True)
    final.add_argument("--lanzou-url", action="append")
    final.add_argument("--manual-r2-sha256", help="网络不可用时，由操作者下载并核对后提供的实际摘要")
    final.set_defaults(handler=finalize)
    online = commands.add_parser("verify-online", help="手动部署后回读Pages核心文件")
    online.add_argument("--pages-dir", type=Path, required=True)
    online.add_argument("--base-url", default="https://ac7multisp-updates.pages.dev/")
    online.add_argument("--receipt", type=Path, required=True)
    online.set_defaults(handler=verify_online)
    args = parser.parse_args()
    try:
        args.handler(args)
    except Exception as error:
        parser.exit(1, f"发布准备停止：{error}\n保留现有输出；不自动重试或改写旧产物。\n")


if __name__ == "__main__":
    main()
