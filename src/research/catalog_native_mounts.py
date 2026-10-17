"""汇总已离线解析的原版机体挂载资料，不连接游戏、不编译、不执行测试。"""

import argparse
import hashlib
import json
import re
from collections import Counter
from pathlib import Path

from summarize_mount_assets import properties, resolve


EXISTING = {"f15e", "f35c", "su30sm"}
INTRO = {"f16c", "f02a", "f104", "mr2k", "j39e", "rflm"}
SYSTEM_WEAPONS = {"tls", "eml", "plsl", "mgp", "esm", "ecmp", "WeaponUAV"}
BATCHES = {
    "reference": "既有三机布局参考，本轮不复测",
    "static_intro": "首批六机；须保持三个SWP均为静态组件",
    "static_base": "基础机型的静态常规弹挂载",
    "static_dlc": "官方DLC的静态常规弹挂载",
    "static_system": "静态组件中的固定炮体或持续系统；能力与布局分别研究",
    "skeletal_swp3": "仅SWP3使用骨架；先核对该组活动行为",
    "skeletal_complex": "其他骨架结构；逐机核对舱门、父链及动画",
}


def read_json(path):
    """兼容既有解析器写出的UTF-8 BOM。"""
    return json.loads(path.read_text(encoding="utf-8-sig"))


def manifest_entries(path):
    """输入既可为资源manifest，也可为manifest路径清单。"""
    items = read_json(path)
    if not isinstance(items, list):
        raise ValueError(f"清单必须为数组：{path}")
    for item in items:
        if isinstance(item, str):
            nested = Path(item)
            yield from manifest_entries(nested if nested.is_absolute() else path.parent / nested)
        elif isinstance(item, dict) and item.get("json"):
            yield item


def source_record(entry):
    """复核源字节，只将PAK内路径和摘要写入交付文件。"""
    parsed = Path(entry["json"])
    decoded = parsed.with_suffix(".uasset")
    encrypted = decoded.parent.parent / decoded.name
    hashes = {"encrypted_sha256": hashlib.sha256(encrypted.read_bytes()).hexdigest(),
              "decoded_sha256": hashlib.sha256(decoded.read_bytes()).hexdigest()}
    for key, digest in hashes.items():
        if entry.get(key) and entry[key].lower() != digest:
            raise ValueError(f"资源哈希不匹配：{entry['asset']} 的 {key}")
    if not re.fullmatch(r"pakchunk\d+-WindowsNoEditor(?:_0_P)?\.pak", entry["pak"]):
        raise ValueError(f"不接受第三方PAK来源：{entry['pak']}")
    return {"pak": entry["pak"], "asset": entry["asset"], **hashes}


def object_reference(document, index):
    """同时保存对象名和所属资源包，避免同名组件引用混淆。"""
    if not index:
        return None
    item = document["Exports"][index - 1] if index > 0 else document["Imports"][-index - 1]
    result = {"object": item["ObjectName"]}
    outer = item.get("OuterIndex", 0)
    while outer < 0:
        parent = document["Imports"][-outer - 1]
        if parent["ObjectName"].startswith("/"):
            result["package"] = parent["ObjectName"]
            break
        outer = parent.get("OuterIndex", 0)
    return result


def weapon_configs(document):
    """保留每个Config自己的覆写；缺字段留给基类逐字段合并。"""
    activators = [export for export in document["Exports"]
                  if export["ObjectName"] == "PlayerWeaponActivator"]
    if len(activators) != 1:
        raise ValueError("PlayerWeaponActivator缺失或不唯一")
    result = {}
    for name, value in properties(activators[0]).items():
        if re.fullmatch(r"SpecialWeaponConfig[123]", name):
            result[name] = dict(value)
            if "WeaponClass" in value:
                result[name]["WeaponClass"] = object_reference(document, value["WeaponClass"])
    return result


def components(document):
    """只摘取真实SWP组件和网格引用，不收录蓝图属性声明。"""
    result = []
    for export in document["Exports"]:
        kind = resolve(document, export.get("ClassIndex", 0))
        if not re.fullmatch(r"SWP[123]_GEN_VARIABLE", export["ObjectName"]):
            continue
        if kind not in ("StaticMeshComponent", "SkeletalMeshComponent"):
            continue
        props = properties(export)
        row = {"group": export["ObjectName"][:4], "component": export["ObjectName"], "class": kind}
        for key in ("StaticMesh", "SkeletalMesh", "AnimClass", "AttachParent"):
            if key in props:
                row[key] = object_reference(document, props[key])
        for key in ("AttachSocketName", "RelativeLocation", "RelativeRotation", "RelativeScale3D"):
            if key in props:
                row[key] = props[key]
        result.append(row)
    if {row["group"] for row in result} != {"SWP1", "SWP2", "SWP3"} or len(result) != 3:
        raise ValueError("未取得唯一的三个SWP组件")
    return sorted(result, key=lambda row: row["group"])


def select_batch(code, category, swp, weapons):
    """除既有参考和已选首批外，由实际组件与兵装类决定分组。"""
    skeletal = [row["group"] for row in swp if row["class"] == "SkeletalMeshComponent"]
    if code in EXISTING:
        return "reference"
    if code in INTRO:
        if skeletal:
            raise ValueError(f"首批机体不再符合静态前提：{code}")
        return "static_intro"
    if skeletal:
        return "skeletal_swp3" if skeletal == ["SWP3"] else "skeletal_complex"
    if any(weapon["class"].removeprefix("plwp_").split("_")[0] in SYSTEM_WEAPONS for weapon in weapons):
        return "static_system"
    return "static_dlc" if category == "official_dlc" else "static_base"


def main():
    """仅生成研究摘要；一致性检查属于资料校验，不调用测试入口。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path, action="append", required=True)
    parser.add_argument("--inventory-summary", type=Path, required=True)
    parser.add_argument("--base-pawn", type=Path, required=True)
    parser.add_argument("--native-index", type=Path, required=True)
    parser.add_argument("--names-source", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    # 原版索引定义目标全集；展示型单列，不能靠显示名猜机型数量。
    indexes = read_json(args.native_index)
    indexed = {Path(asset["name"]).stem for pak in indexes for asset in pak["assets"]
               if "/Pawn/AcePlayerPawn_" in asset["name"]}
    expected = {name.removeprefix("AcePlayerPawn_") for name in indexed} - {"f104av"}
    if len(expected) != 47:
        raise ValueError(f"当前基线应为47个有武装机型，实际为{len(expected)}")
    summaries = {row["code"]: row for row in read_json(args.inventory_summary)}
    source_text = args.names_source.read_text(encoding="utf-8-sig")
    match = re.search(r"local names=\{([^\n]+)\}", source_text)
    if not match:
        raise ValueError("未找到main.lua机型显示名表")
    names = dict(re.findall(r'(\w+)="([^"\\]*)"', match[1]))

    # 同名资源仅选原版更新包；同一优先级若字节不同则停止，不能静默混用。
    entries = {}
    for path in args.manifest:
        for entry in manifest_entries(path):
            stem = Path(entry["asset"]).stem
            if not stem.startswith("AcePlayerPawn_") or stem.removeprefix("AcePlayerPawn_") not in expected:
                continue
            previous = entries.get(stem)
            priority = "_0_P.pak" in entry["pak"]
            if previous and priority == ("_0_P.pak" in previous["pak"]):
                if previous["encrypted_sha256"] != entry["encrypted_sha256"]:
                    raise ValueError(f"相同优先级资源冲突：{stem}")
            elif not previous or priority:
                entries[stem] = entry
    if {name.removeprefix("AcePlayerPawn_") for name in entries} != expected:
        raise ValueError("manifest机体集合与原版索引不一致")

    base_document = read_json(args.base_pawn)
    base_source = source_record({"json": str(args.base_pawn), "pak": "pakchunk0-WindowsNoEditor_0_P.pak",
                                 "asset": "Nimbus/Content/Blueprint/Player/Pawn/AcePlayerPawn.uasset"})
    base_configs = weapon_configs(base_document)
    aircraft = []
    for code in sorted(expected):
        entry = entries["AcePlayerPawn_" + code]
        document = read_json(Path(entry["json"]))
        pawn_source = source_record(entry)
        class_export = next(item for item in document["Exports"] if item["ObjectName"] == f"AcePlayerPawn_{code}_C")
        if resolve(document, class_export["SuperIndex"]) != "AcePlayerPawn_C":
            raise ValueError(f"需要补充未支持的父类链：{code}")
        own = weapon_configs(document)
        weapons = []
        for index in range(1, 4):
            key = "SpecialWeaponConfig" + str(index)
            inherited = base_configs[key]
            overrides = own.get(key, {})
            effective = {**inherited, **overrides}
            weapon = effective.get("WeaponClass")
            if not weapon or not weapon.get("object"):
                raise ValueError(f"合并基类后仍缺武器class：{code}.{key}")
            weapons.append({"config": key, "class": weapon["object"], "class_package": weapon.get("package"),
                            "class_source": "pawn" if "WeaponClass" in overrides else "base_pawn",
                            "effective_fields": effective,
                            "field_sources": {field: "pawn" if field in overrides else "base_pawn" for field in effective}})
        swp = components(document)
        edition = summaries.get(code, {}).get("edition", "基础普通机")
        category = {"官方DLC": "official_dlc", "基础特殊机": "base_special"}.get(edition, "base")
        aircraft.append({"code": code, "display_name_en": names.get(code, code), "display_name_zh": code,
                         "category": category, "batch": select_batch(code, category, swp, weapons),
                         "weapons": weapons, "swp_components": swp, "pawn_source": pawn_source,
                         "plane_config_reference": next((name for name in document["NameMap"]
                                                          if "/Capabilities/PlayerPlaneConfig_" in name), None),
                         "runtime_verified_by_this_catalog": False,
                         "existing_layout_reference": code in EXISTING})
    report = {
        "schema_version": 1, "steam_build": 9855922, "baseline": "v2.4.0",
        "scope": "本机官方资源中的有武装玩家机体；不含第三方模组与f104av无武装展示型",
        "display_names": {"source": "dist/DualSP/Scripts/main.lua 的 local names 表",
                          "zh_policy": "原表未提供中文显示名，保留资源code，不自行翻译"},
        "category_note": "沿用已核对清单：基础28架（含X-02S特殊机）、官方DLC19架；PAK包位置不表示购买许可",
        "batch_rules": BATCHES, "base_pawn_source": base_source,
        "verification": {"aircraft_count": len(aircraft), "unique_codes": len({row["code"] for row in aircraft}),
                         "config_count": sum(len(row["weapons"]) for row in aircraft),
                         "swp_component_count": sum(len(row["swp_components"]) for row in aircraft),
                         "hashed_source_records": len(aircraft) + 1,
                         "pawn_manifest_hash_pairs_verified": len(aircraft),
                         "base_hash_status": "本轮原版提取副本已计算哈希，无独立既存manifest期望值",
                         "category_counts": dict(Counter(row["category"] for row in aircraft)),
                         "batch_counts": dict(Counter(row["batch"] for row in aircraft)),
                         "verified": ["原版PAK索引的机体身份", "Pawn直接父类", "逐Config字段继承与真实武器class",
                                      "三个SWP组件类及网格引用", "所选Pawn加密/解码副本SHA256"],
                         "not_verified": ["PlayerPlaneConfig正文与购买限制", "多数新机挂点坐标与网格拆分粒度",
                                          "动画通道与动态净空", "运行时模型池与发射/补装行为", "新机布局与持续效果能力"],
                         "runtime_verified": False,
                         "notes": ["MaxWeaponCount等字段保留原生覆写值，包括-1；不是运行时实际库存或装填窗口",
                                   "网格与兵装包路径为Pawn引用，不代表所有被引用资源正文已解析",
                                   "TLS/ECM/IEWS不因持续效果预设与其他SP互斥；正式运行代码的能力范围另核对",
                                   "既有三机布局已有正式基线反馈；本目录生成过程没有复测任何机型"]},
        "aircraft": aircraft,
    }
    args.output.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report["verification"], ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
