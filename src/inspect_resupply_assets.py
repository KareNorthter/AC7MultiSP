"""只读解析自有游戏的补给页，保留结构摘要，不分发原始资源。"""
import json
import os
import re
import subprocess
from pathlib import Path

from inspect_hud_assets import inspect

# 原始副本和中间 JSON 放入被 Git 排除的 Temp；解析依赖只用项目本地版本。
ROOT = Path(__file__).resolve().parents[1]
PAKS = Path(r"E:\SteamLibrary\steamapps\common\ACE COMBAT 7\Game\Content\Paks")
OUTPUT = ROOT / "Temp/resupply-assets"
JSON_OUTPUT = ROOT / "Temp/resupply-json"
NAMES = {"MissionResupply", "HangarResupplySpecBar", "HangarResupplySpecItem", "HangarResupplySpecGroup"}


def selected(name):
    """仅提取补给页面及其性能栏依赖。"""
    return name.endswith((".uasset", ".uexp")) and Path(name).stem in NAMES


def values(properties):
    """去除资源序列化元数据，保留用于核对布局的属性。"""
    result = {}
    for item in properties:
        value = item.get("Value", item.get("EnumValue"))
        if isinstance(value, list):
            value = values(value)
        if isinstance(value, dict):
            value = {key: entry for key, entry in value.items() if not key.startswith("$")}
        result[item["Name"]] = value
    return result


def summarize(asset):
    """从页面导出中提取原生父类、主要控件、布局与蓝图调用名。"""
    exports, imports = asset["Exports"], asset["Imports"]

    def object_name(index):
        """资源包正索引指向导出，负索引指向导入。"""
        if not index:
            return None
        return (exports[index - 1] if index > 0 else imports[-index - 1])["ObjectName"]

    # 同一资源含默认对象和原型副本，摘要按控件名去重。
    controls = {}
    functions = []
    names = {"TitleText", "DescriptionText", "WeaponDescriptionText", "WeaponAmmo_Label", "WeaponAmmo_Text",
             "WeaponType_Label", "WeaponType_Text", "WeaponConceptPanel", "WeaponSpecifficationGroup",
             "ItemText0", "ItemText1", "ItemText2", "CursorText0", "CursorText1", "CursorText2", "BarA", "BaseImage3"}
    for item in exports:
        kind = object_name(item.get("ClassIndex", 0))
        if kind == "Function":
            functions.append(item["ObjectName"])
        if item["ObjectName"] not in names or kind == "ObjectProperty":
            continue
        properties = values(item.get("Data", []))
        slot_index = properties.get("Slot", 0)
        slot = exports[slot_index - 1] if slot_index > 0 else None
        layout = values(slot.get("Data", [])) if slot else {}
        controls[item["ObjectName"]] = {"class": kind, "font": properties.get("Font"),
            "slot_class": object_name(slot["ClassIndex"]) if slot else None,
            "parent": object_name(layout.get("Parent", 0)), "layout": layout.get("LayoutData")}
    # 从页面蓝图类的父类索引读取名称，不把预期父类硬编码成研究结果。
    page_class = next(item for item in exports if item["ObjectName"] == "MissionResupply_C")
    parent = imports[-page_class["SuperIndex"] - 1]
    parent_path = object_name(parent["OuterIndex"]) + "." + parent["ObjectName"]
    return {"native_parent": parent_path, "functions": functions,
            "blueprint_calls": [item["ObjectName"] for item in imports if item["ClassName"] == "Function"
                                and not item["ObjectName"].startswith("Default__")],
            "controls": controls}


def main():
    """提取、解码并用既有工具解析；任何失败都直接停止。"""
    # 摘要的构建号来自 Steam 清单；发现版本变化就停止，不能沿用旧标签。
    manifest = (PAKS.parents[4] / "appmanifest_502500.acf").read_text(encoding="utf-8")
    build = re.search(r'"buildid"\s+"(\d+)"', manifest)
    assert build and int(build[1]) == 9855922, "游戏构建号不匹配，停止原生补给资源解析"
    runtime = ROOT / "tools/dotnet8"
    parser = ROOT / "tools/assets/UAssetGUI.exe"
    assert (runtime / "dotnet.exe").is_file(), "缺少项目本地 .NET 8"
    assert parser.is_file(), "缺少 UAssetGUI"
    environment = dict(os.environ, DOTNET_ROOT_X64=str(runtime))
    reports = [inspect(PAKS / name, selected, OUTPUT)
               for name in ("pakchunk0-WindowsNoEditor_0_P.pak", "pakchunk0-WindowsNoEditor.pak")]
    subprocess.run([os.sys.executable, "-X", "utf8", str(ROOT / "src/decrypt_hud_for_inspection.py"), str(OUTPUT)],
                   check=True, timeout=30)
    JSON_OUTPUT.mkdir(parents=True, exist_ok=True)
    pages = []
    for report in reports:
        for entry in report["assets"]:
            if Path(entry["name"]).name != "MissionResupply.uasset":
                continue
            source = Path(entry["file"]).parent / "decoded/MissionResupply.uasset"
            target = JSON_OUTPUT / (Path(report["pak"]).stem + ".json")
            subprocess.run([str(parser), "tojson", str(source), str(target), "VER_UE4_18"],
                           env=environment, check=True, timeout=30)
            assert target.is_file(), "资源解析未产生 JSON"
            page = summarize(json.loads(target.read_text(encoding="utf-8")))
            page.update(pak=report["pak"], asset=entry["name"], sha256=entry["sha256"])
            pages.append(page)
    assert pages, "未找到原生补给页面"
    summary = {"steam_build": int(build[1]), "game_started": False, "game_assets_modified": False,
               "runtime_sequence_verified": False, "pages": pages}
    target = ROOT / "evidence/native-resupply-structure.json"
    target.write_text(json.dumps(summary, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(target)


if __name__ == "__main__":
    main()
