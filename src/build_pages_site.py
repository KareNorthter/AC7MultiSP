#!/usr/bin/env python3
"""从项目玩家正文生成可上传到 Cloudflare Pages 的静态站点目录。"""
from __future__ import annotations

import argparse
import html
import json
import re
from pathlib import Path
from urllib.parse import urlparse


ROOT = Path(__file__).resolve().parents[1]
SITE_SOURCE = ROOT / "site"
PLAYER_DOCS = ROOT / "docs" / "user"
DEFAULT_OUTPUT = ROOT / "Temp" / "tasks" / "pages-site" / "pages-output"
SITE_FILES = ("index.html", "styles.css", "app.js", "stable.json", "preview.json", "_headers")
SITE_COPY_MARKER = "/* SITE_UI_COPY */ {}"
PLAYER_DOC_FILES = (
    "功能说明.txt",
    "功能说明.en.txt",
    "安装说明.txt",
    "安装说明.en.txt",
    "更新日志.txt",
    "更新日志.en.txt",
    "作者与授权.txt",
    "作者与授权.en.txt",
)
DOWNLOAD_HOSTS = {
    "wwbru.lanzoue.com",
    "downloads.iot-node-service-api.online",
    "ac7multisp-updates.pages.dev",
}
VERSION_PATTERN = re.compile(r"(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\Z")


def validate_manifest(path: Path) -> None:
    raw = path.read_bytes()
    if not raw or len(raw) > 32 * 1024:
        raise ValueError("stable.json 必须为非空文件且不超过 32 KiB")

    def unique_object(pairs: list[tuple[str, object]]) -> dict[str, object]:
        result: dict[str, object] = {}
        for key, value in pairs:
            if key in result:
                raise ValueError(f"stable.json 含重复字段：{key}")
            result[key] = value
        return result

    manifest = json.loads(raw.decode("utf-8"), object_pairs_hook=unique_object)
    if not isinstance(manifest, dict):
        raise ValueError("stable.json 顶层必须是对象")
    if (
        type(manifest.get("schema")) is not int
        or manifest.get("schema") != 1
        or manifest.get("product") != "AC7MultiSP"
        or manifest.get("channel") != "stable"
    ):
        raise ValueError("stable.json 的 schema、product 或 channel 不匹配")
    status = manifest.get("status")
    if status == "preparing":
        return
    if status != "ready":
        raise ValueError("status 只能是 preparing 或 ready")

    version = manifest.get("latest_version")
    if (
        not isinstance(version, str)
        or len(version.encode("utf-8")) > 14
        or not VERSION_PATTERN.fullmatch(version)
    ):
        raise ValueError("ready 清单必须使用三段式稳定版本号")
    notes = manifest.get("release_notes")
    if not isinstance(notes, dict):
        raise ValueError("ready 清单缺少中英文更新说明")
    for language in ("zh", "en"):
        note = notes.get(language)
        if (
            not isinstance(note, str)
            or not note.strip()
            or len(note.encode("utf-8")) > 8192
            or any(ord(char) < 32 and char not in "\n\r\t" for char in note)
        ):
            raise ValueError(f"release_notes.{language} 为空或格式无效")

    downloads = manifest.get("downloads")
    if not isinstance(downloads, list) or not 2 <= len(downloads) <= 4:
        raise ValueError("ready 清单需要 1 个 R2 主入口和 1 至 3 个蓝奏云备用入口")
    ids: set[str] = set()
    download_roles: list[tuple[str, str]] = []
    for item in downloads:
        if not isinstance(item, dict):
            raise ValueError("下载入口必须为对象")
        item_id = item.get("id")
        if (
            not isinstance(item_id, str)
            or not re.fullmatch(r"[a-z0-9_-]{1,32}", item_id)
            or item_id in ids
        ):
            raise ValueError("下载入口 id 无效或重复")
        ids.add(item_id)
        label = item.get("label")
        if not isinstance(label, dict) or any(
            not isinstance(label.get(language), str)
            or not label[language].strip()
            or len(label[language].encode("utf-8")) > 96
            or any(ord(char) < 32 for char in label[language])
            for language in ("zh", "en")
        ):
            raise ValueError("每个下载入口都需要有效的中英文名称")
        url = item.get("url")
        if not isinstance(url, str) or len(url) > 2048:
            raise ValueError("下载 URL 无效或过长")
        try:
            url.encode("ascii")
        except UnicodeEncodeError as exc:
            raise ValueError("下载 URL 必须使用 ASCII 字符") from exc
        if any(ord(char) <= 32 or char in {'"', "'", "\\"} for char in url):
            raise ValueError("下载 URL 含空白或特殊字符")
        parsed = urlparse(url)
        try:
            port = parsed.port
        except ValueError as exc:
            raise ValueError("下载 URL 端口无效") from exc
        if (
            parsed.scheme != "https"
            or port not in (None, 443)
            or parsed.username
            or parsed.password
            or parsed.hostname not in DOWNLOAD_HOSTS
        ):
            raise ValueError("下载 URL 必须使用更新程序已登记的公开 HTTPS 域名")
        download_roles.append((item_id, parsed.hostname or ""))

    primary = [(item_id, host) for item_id, host in download_roles if item_id == "r2"]
    backups = [(item_id, host) for item_id, host in download_roles if item_id != "r2"]
    if (
        len(primary) != 1
        or primary[0][1] == "wwbru.lanzoue.com"
        or not 1 <= len(backups) <= 3
        or any(not item_id.startswith("lanzou") or host != "wwbru.lanzoue.com" for item_id, host in backups)
    ):
        raise ValueError("下载入口必须是 1 个 R2 主链接和 1 至 3 个蓝奏云备用链接")


def copy_utf8(source: Path, target: Path) -> None:
    content = source.read_bytes()
    content.decode("utf-8")
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_bytes(content)


def read_site_texts() -> dict[str, dict[str, str]]:
    """主页固定词条只维护interface里的双语JSON。"""
    texts = {}
    for language in ("zh-CN", "en"):
        path = PLAYER_DOCS / "interface" / f"site.{language}.json"
        values = json.loads(path.read_text(encoding="utf-8"))
        if not isinstance(values, dict) or not values or any(
            not isinstance(key, str) or not isinstance(value, str) or not value.strip()
            or "\0" in value for key, value in values.items()
        ):
            raise ValueError(f"主页词条无效：{path}")
        texts[language] = values
    if texts["zh-CN"].keys() != texts["en"].keys():
        raise ValueError("主页中英文词条键不一致")
    return texts


def render_html(template: str, texts: dict[str, str]) -> str:
    """HTML首次显示的文字也由同一中文资源提供，加载后可切换语言。"""
    template = re.sub(
        r'(<(?P<tag>[a-z][\w:-]*)\b[^>]*\bdata-i18n="(?P<key>[^"]+)"[^>]*>)([^<]*)(</(?P=tag)>)',
        lambda match: match[1] + html.escape(texts[match["key"]]) + match[5],
        template,
    )

    def aria(match: re.Match[str]) -> str:
        tag = match[0]
        key = re.search(r'data-i18n-aria="([^"]+)"', tag)[1]
        label = html.escape(texts[key], quote=True)
        if 'aria-label="' in tag:
            return re.sub(r'aria-label="[^"]*"', lambda _: f'aria-label="{label}"', tag)
        return tag[:-1] + f' aria-label="{label}">'

    template = re.sub(r'<[^>]+\bdata-i18n-aria="[^"]+"[^>]*>', aria, template)
    return re.sub(
        r'(<meta\s+name="description"\s+content=")[^"]*(")',
        lambda match: match[1] + html.escape(texts["metaDescription"], quote=True) + match[2],
        template,
    )


def build(output: Path, manifest: Path | None = None) -> None:
    output = output.resolve()
    for source_directory in (SITE_SOURCE, PLAYER_DOCS):
        if output.is_relative_to(source_directory) or source_directory.is_relative_to(output):
            raise ValueError("站点输出不能覆盖源码、玩家正文或其上级目录")
    for name in SITE_FILES:
        source = SITE_SOURCE / name
        if not source.is_file():
            raise FileNotFoundError(f"缺少站点文件：{source}")
    manifest = manifest or SITE_SOURCE / "stable.json"
    validate_manifest(manifest)
    # preview为旧更新器保留的独立协议，不把它强改成stable结构。
    preview = json.loads((SITE_SOURCE / "preview.json").read_text(encoding="utf-8"))
    if not isinstance(preview, dict) or preview.get("product") != "AC7MultiSP" or preview.get("channel") != "preview":
        raise ValueError("preview.json 产品或通道不匹配")
    texts = read_site_texts()
    app = (SITE_SOURCE / "app.js").read_text(encoding="utf-8")
    if app.count(SITE_COPY_MARKER) != 1:
        raise ValueError("主页脚本必须包含唯一词条占位标记")
    serialized = json.dumps(texts, ensure_ascii=False, indent=2).replace("<", "\\u003c")
    app = app.replace(SITE_COPY_MARKER, serialized)
    page = render_html((SITE_SOURCE / "index.html").read_text(encoding="utf-8"), texts["zh-CN"])
    for name in PLAYER_DOC_FILES:
        source = PLAYER_DOCS / name
        if not source.is_file():
            raise FileNotFoundError(f"缺少玩家正文：{source}")
        source.read_bytes().decode("utf-8")

    output.mkdir(parents=True, exist_ok=True)
    for name in SITE_FILES:
        copy_utf8(SITE_SOURCE / name, output / name)
    copy_utf8(manifest, output / "stable.json")
    (output / "app.js").write_text(app, encoding="utf-8", newline="\n")
    (output / "index.html").write_text(page, encoding="utf-8", newline="\n")
    for name in PLAYER_DOC_FILES:
        copy_utf8(PLAYER_DOCS / name, output / "user-docs" / name)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="生成 Cloudflare Pages 静态站点；不删除输出目录中的其他文件。"
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=DEFAULT_OUTPUT,
        help=f"部署目录（默认：{DEFAULT_OUTPUT}）",
    )
    parser.add_argument("--manifest", type=Path, help="使用本次发布暂存清单，不改写站点源清单")
    args = parser.parse_args()
    output = args.output if args.output.is_absolute() else ROOT / args.output
    build(output.resolve(), args.manifest.resolve() if args.manifest else None)
    print(f"Pages 文件已生成：{output.resolve()}")
    print("保留目标目录中的其他文件；四份玩家正文均从 docs/user 原样复制。")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
