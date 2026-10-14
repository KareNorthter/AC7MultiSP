"""从离线资源解析结果提取配置、挂点与装填信息，不读取或修改游戏进程。"""

import argparse
import json
from pathlib import Path


def value(prop):
    """保留数组与结构区别，移除解析器元数据。"""
    # 向量等内嵌字典继续清理类型信息。
    data = prop.get("Value", prop.get("EnumValue"))
    if isinstance(data, list):
        if "IntPointPropertyData" in prop["$type"]:
            # UAssetAPI 将二维整数尺寸直接写为两个整数，而不是具名子属性。
            if len(data) != 2 or not all(isinstance(item, int) for item in data):
                raise ValueError("IntPoint 结构无效")
            return {"X": data[0], "Y": data[1]}
        if "ArrayProperty" in prop["$type"]:
            return [value(item) for item in data]
        return {item["Name"]: value(item) for item in data}
    if isinstance(data, dict):
        return {key: item for key, item in data.items() if not key.startswith("$")}
    return data


def properties(export):
    """只处理具名导出属性。"""
    return {prop["Name"]: value(prop) for prop in export.get("Data", [])}


def resolve(document, index):
    """把资源包内索引解析为对象名，避免把索引误当游戏地址。"""
    if not index:
        return None
    return (document["Exports"][index - 1] if index > 0 else document["Imports"][-index - 1])["ObjectName"]


def summarize(document):
    """提取研究必需字段；不保存网格顶点、纹理或完整蓝图。"""
    # 摘要分开保存父类、组件、配置、挂点和武器参数，避免混淆继承默认值。
    result = {"classes": [], "configs": [], "components": [], "sockets": [], "parameters": []}
    for export in document["Exports"]:
        # 每个导出对象的类型与去除元数据后的属性。
        name = export["ObjectName"]
        kind = resolve(document, export.get("ClassIndex", 0))
        props = properties(export)
        if "ClassExport" in export.get("$type", ""):
            result["classes"].append({"name": name, "parent": resolve(document, export.get("SuperIndex", 0))})
        if name == "PlayerWeaponActivator":
            for key, config in props.items():
                if key.startswith("SpecialWeaponConfig"):
                    result["configs"].append({"name": key, "class": resolve(document, config["WeaponClass"])})
        if "SocketName" in props:
            result["sockets"].append(props)
        if "Parameters" in (kind or ""):
            result["parameters"].append({"name": name, "class": kind, "properties": props})
        if name.startswith("Default__plwp"):
            # 默认对象只保留与挂载、发射和装填直接相关的覆盖值；缺省项不能当成零。
            selected = {key: entry for key, entry in props.items() if key in (
                "WeaponID", "MaxLoadedCount", "LoadTime", "bFireAllInOneShot", "LockOnTargetType",
                "InitialSpeed", "InitialDropSpeed", "IgnitionInitialSpeed", "StaticMesh", "RootComponent")}
            for key in ("StaticMesh", "RootComponent"):
                if key in selected:
                    selected[key] = resolve(document, selected[key])
            result["parameters"].append({"name": name, "class": kind, "properties": selected})
        if kind == "SCS_Node" or name.startswith(("SWP", "WeaponMeshBase")) or "Mesh" in (kind or ""):
            # 只保存挂接结构和模型引用；碰撞配置等长列表不复制进证据。
            subset = {key: entry for key, entry in props.items() if key in (
                "ParentComponentOrVariableName", "InternalVariableName", "VariableName", "AttachToName", "AttachParent",
                "AttachSocketName", "RelativeLocation", "RelativeRotation", "RelativeScale3D",
                "StaticMesh", "SkeletalMesh", "ComponentTemplate", "ChildNodes", "bVisible", "bHiddenInGame")}
            for key in ("StaticMesh", "SkeletalMesh", "ComponentTemplate", "AttachParent"):
                if key in subset:
                    subset[key] = resolve(document, subset[key])
            if subset:
                result["components"].append({"name": name, "class": kind, "properties": subset})
    return result


def main():
    """按提取清单生成精简证据；输出必须是新文件。"""
    # 入口参数明确指定真实清单，避免使用时间上最新但属于其他任务的文件。
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("manifest", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    # 同名资源保留各 PAK 版本，不推断运行时覆盖顺序。
    manifest = json.loads(args.manifest.read_text(encoding="utf-8"))
    report = {"game_process_read": False, "game_files_modified": False, "runtime_verified": False, "assets": []}
    for entry in manifest:
        document = json.loads(Path(entry["json"]).read_text(encoding="utf-8-sig"))
        report["assets"].append({"pak": entry["pak"], "asset": entry["asset"],
            "encrypted_sha256": entry["encrypted_sha256"], "decoded_sha256": entry["decoded_sha256"],
            **summarize(document)})
    with args.output.open("x", encoding="utf-8") as stream:
        json.dump(report, stream, ensure_ascii=False, indent=2)
        stream.write("\n")
    print(args.output)


if __name__ == "__main__":
    main()
