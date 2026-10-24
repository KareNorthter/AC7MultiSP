"""由 docs/user 唯一正文和界面资源生成游戏侧文字；不编译、不运行游戏。"""

import argparse
import json
from pathlib import Path
import re
from urllib.parse import urlsplit


ROOT = Path(__file__).resolve().parent.parent
CATALOG = ROOT / "docs/user/interface"
DOCUMENTS = {
    "features": ("功能说明.txt", "Features.txt"),
    "installation": ("安装说明.txt", "Installation.txt"),
    "changelog": ("更新日志.txt", "Changelog.txt"),
    "credits": ("作者与授权.txt", "Author-and-License.txt"),
}
OUTPUTS = ("src/generated/user_texts.hpp", "dist/DualSP/Scripts/user_texts.lua")


def extract_compact_credits(text, language):
    """从作者正文提取短声明、作者链接及反馈；缺少分节时拒绝生成旧信息。"""
    headings = ("作者：", "反馈：", "第三方组件与素材") if language == "zh-CN" else (
        "Author:", "Feedback:", "Third-party components and assets")
    lines = [line.strip() for line in text.lstrip("\ufeff").splitlines()]
    try:
        author, feedback, end = (lines.index(title) for title in headings)
    except ValueError as error:
        raise ValueError("作者与授权缺少紧凑信息所需分节：" + language) from error
    if not 0 < author < feedback < end:
        raise ValueError("作者与授权分节顺序无效：" + language)
    introduction = lines[1:author] if lines[0] == "AC7MultiSP" else lines[:author]
    notice = "\n".join(line for line in introduction if line)
    authors = []
    for line in lines[author + 1:feedback]:
        if not line:
            continue
        if line.startswith("https://"):
            address = urlsplit(line)
            if not authors or authors[-1]["url"] or not address.hostname or address.username or address.password or any(c.isspace() for c in line):
                raise ValueError("作者网页地址或对应作者无效：" + language)
            authors[-1]["url"] = line
        else:
            authors.append({"label": line.lstrip("• "), "url": ""})
    contacts = [line.lstrip("• ") for line in lines[feedback + 1:end] if line]
    if not notice or not authors or not contacts:
        raise ValueError("紧凑作者和反馈信息不完整：" + language)
    return {"notice": notice, "author_heading": headings[0], "authors": authors,
            "feedback_heading": headings[1], "feedback": contacts}


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError("资源包含重复键：" + key)
        result[key] = value
    return result


def load_catalog(language):
    path = CATALOG / ("game." + language + ".json")
    data = json.loads(path.read_text(encoding="utf-8-sig"), object_pairs_hook=unique_object)
    if data.get("schema") != 1 or data.get("language") != language:
        raise ValueError("资源格式或语言不符：" + str(path))
    if not isinstance(data.get("texts"), dict) or not data["texts"]:
        raise ValueError("界面资源为空：" + str(path))
    for key, value in data["texts"].items():
        if not re.fullmatch(r"[a-z][a-z0-9_.]*", key) or not isinstance(value, str):
            raise ValueError("资源键或正文无效：" + key)
    if not isinstance(data.get("message_replacements"), list):
        raise ValueError("缺少有序消息映射：" + str(path))
    for key in data["message_replacements"]:
        if key not in data["texts"]:
            raise ValueError("消息映射缺少资源：" + key)
        match = data.get("message_matches", {}).get(key)
        if not isinstance(match, dict) or set(match) != {"zh", "en"} or not all(isinstance(v, str) for v in match.values()):
            raise ValueError("消息映射缺少稳定原文匹配键：" + key)
        if not data["texts"][key] or not match["zh"] or not match["en"]:
            raise ValueError("消息映射不能使用空串：" + key)
    return data


def cpp_string(text):
    # 三位八进制保留 Combo 内的 NUL，不吞掉紧接的数字；其余控制字符同理。
    escapes = {"\\": "\\\\", '"': '\\"', "\n": "\\n", "\r": "\\r", "\t": "\\t"}
    return '"' + "".join(escapes.get(c, "\\%03o" % ord(c) if ord(c) < 32 else c) for c in text) + '"'


def lua_string(text):
    escapes = {"\\": "\\\\", '"': '\\"', "\n": "\\n", "\r": "\\r", "\t": "\\t"}
    return '"' + "".join(escapes.get(c, "\\%03d" % ord(c) if ord(c) < 32 else c) for c in text) + '"'


def check_pair(key, zh, en):
    placeholders = re.compile(r"%(?:\d+\$)?[-+#0 ]*\d*(?:\.\d+)?[diuoxXfFeEgGaAcsp%]")
    if placeholders.findall(zh) != placeholders.findall(en):
        raise ValueError("中英文格式占位符不符：" + key)
    if zh.count("\0") != en.count("\0"):
        raise ValueError("中英文列表分隔数不符：" + key)
    if re.findall(r"###[^\0]*", zh) != re.findall(r"###[^\0]*", en):
        raise ValueError("中英文 ImGui 标识不符：" + key)


def render():
    zh, en = load_catalog("zh-CN"), load_catalog("en")
    if zh["texts"].keys() != en["texts"].keys():
        raise ValueError("中英文资源键不成套")
    if zh["message_replacements"] != en["message_replacements"]:
        raise ValueError("中英文消息映射顺序不符")
    if zh["message_matches"] != en["message_matches"]:
        raise ValueError("中英文稳定原文匹配键不符")
    keys = sorted(zh["texts"])
    for key in keys:
        check_pair(key, zh["texts"][key], en["texts"][key])
    # 只校对静态键引用；外部清单、游戏提供的动态文本仍走原选语路径。
    for relative in ("src/overlay.cpp", "src/management_panel.hpp", "src/version_updates_panel.hpp", "src/quick_mission_panel.hpp",
                     "dist/DualSP/Scripts/main.lua", "dist/DualSP/Scripts/native_loadout.lua",
                     "dist/DualSP/Scripts/loadout_flow.lua", "dist/DualSP/Scripts/debug_aircraft.lua", "dist/DualSP/Scripts/quick_mission.lua"):
        source = (ROOT / relative).read_text(encoding="utf-8-sig")
        for key in re.findall(r'"([a-z][a-z0-9_.]+\.[0-9a-f]{8})"', source):
            if key not in zh["texts"]:
                raise ValueError(relative + " 引用了未登记文字：" + key)
        for key in re.findall(r'"(quick\.[a-z0-9_.]+)"', source):
            if key.endswith("."):
                continue  # 运行时状态后缀在资源中逐项维护。
            if key not in zh["texts"]:
                raise ValueError(relative + " 引用了未登记文字：" + key)
    docs = {}
    for key, filenames in DOCUMENTS.items():
        docs[key] = tuple((ROOT / "docs/user" / filename).read_text(encoding="utf-8-sig")
                          for filename in filenames)
    header = ["#pragma once", "// 自动生成；唯一来源为 docs/user 正文和 interface/game.*.json，请勿手改。",
              "#include <cstddef>", "#include <string_view>", "#include <utility>",
              "namespace dualsp::user_texts {", "struct Entry {std::string_view key;const char* zh;const char* en;};",
              "inline constexpr Entry entries[]={"]
    for key in keys:
        header.append("    {" + ",".join(cpp_string(v) for v in (key, zh["texts"][key], en["texts"][key])) + "},")
    header += ["};", "inline const char* get(std::string_view key,bool chinese) noexcept {",
               "    std::size_t first=0,last=sizeof(entries)/sizeof(entries[0]);",
               "    while(first<last) {const auto middle=first+(last-first)/2;",
               "        if(entries[middle].key<key)first=middle+1;else last=middle;}",
               '    if(first==sizeof(entries)/sizeof(entries[0]) || entries[first].key!=key)return "[missing text]";',
               "    return chinese?entries[first].zh:entries[first].en;", "}",
               "// 顺序及重复项沿用原 uiMessages；子串转换不可按资源键排序。",
               "struct MessageReplacement {const char* sourceZh;const char* sourceEn;const char* zh;const char* en;};",
               "inline constexpr MessageReplacement messageReplacements[]={"]
    for key in zh["message_replacements"]:
        match = zh["message_matches"][key]
        header.append("    {" + ",".join(cpp_string(v) for v in (match["zh"], match["en"], zh["texts"][key], en["texts"][key])) + "},")
    header += ["};", "namespace documents {"]
    for key, (chinese, english) in docs.items():
        header += ["inline constexpr char " + key + "Zh[]=" + cpp_string(chinese) + ";",
                   "inline constexpr char " + key + "En[]=" + cpp_string(english) + ";",
                   "inline const char* " + key + "(bool chinese) noexcept {return chinese?" + key + "Zh:" + key + "En;}"]
    compact = [extract_compact_credits(text, language) for text, language in zip(docs["credits"], ("zh-CN", "en"))]
    for suffix, info in zip(("Zh", "En"), compact):
        for field, value in (("Notice", info["notice"]), ("AuthorHeading", info["author_heading"]),
                             ("Feedback", info["feedback_heading"] + " " + "    ".join(info["feedback"])),
                             ("FeedbackHeading", info["feedback_heading"]),
                             ("FeedbackBody", "\n".join(info["feedback"]))):
            header.append("inline constexpr char compact" + field + suffix + "[]=" + cpp_string(value) + ";")
        for field in ("label", "url"):
            title = "Label" if field == "label" else "Url"
            header.append("inline constexpr const char* compactAuthor" + title + "s" + suffix + "[]={" +
                          ",".join(cpp_string(author[field]) for author in info["authors"]) + "};")
    for field in ("Notice", "AuthorHeading", "Feedback", "FeedbackHeading", "FeedbackBody"):
        header.append("inline const char* compact" + field + "(bool chinese) noexcept {return chinese?compact" + field + "Zh:compact" + field + "En;}")
    header += ["inline std::size_t compactAuthorCount(bool chinese) noexcept {return chinese?" +
               str(len(compact[0]["authors"])) + ":" + str(len(compact[1]["authors"])) + ";}"]
    for field in ("Label", "Url"):
        header.append("inline const char* compactAuthor" + field + "(std::size_t index,bool chinese) noexcept {" +
                      'if(index>=compactAuthorCount(chinese))return "";return chinese?compactAuthor' + field +
                      "sZh[index]:compactAuthor" + field + "sEn[index];}")
    header += ["}", "}", ""]
    lua = ["-- 自动生成；唯一来源为 docs/user/interface/game.*.json，请勿手改。", "local Texts={}"]
    for language, catalog in (("zh", zh), ("en", en)):
        lua.append("Texts." + language + "={")
        for key in keys:
            lua.append("    [" + lua_string(key) + "]=" + lua_string(catalog["texts"][key]) + ",")
        lua.append("}")
    lua += ["function Texts.get(key,language)",
            '    return assert((language=="zh"and Texts.zh or Texts.en)[key],"Missing user text: "..tostring(key))',
            "end", "return Texts", ""]
    return {OUTPUTS[0]: "\n".join(header).encode("utf-8"), OUTPUTS[1]: "\n".join(lua).encode("utf-8")}


def generate(check=False):
    stale = []
    for relative, content in render().items():
        path = ROOT / relative
        if not path.is_file() or path.read_bytes() != content:
            if check:
                stale.append(relative)
            else:
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(content)
    if stale:
        raise ValueError("生成文字已过期，请运行 python src/user_texts.py：" + "、".join(stale))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true", help="只静态核对资源与已生成文件，不写文件")
    args = parser.parse_args()
    generate(args.check)
    print("游戏文字资源静态核对一致。" if args.check else "已从唯一正文生成游戏文字资源。")


if __name__ == "__main__":
    main()
