"""精简 Su-30SM 只读快照，记录显隐、装填和对象身份的证据边界。"""

import argparse
import hashlib
import json
import struct
from pathlib import Path


def flag(field):
    """按当前快照内反射得到的单字节位掩码还原布尔字段。"""
    # UBoolProperty 的四字节描述由现场采集保留；只接受本次已知格式。
    size, offset, byte_mask, field_mask = bytes.fromhex(field["property_tail"])[:4]
    if size != 1 or offset != 0 or byte_mask != field_mask or field_mask not in (0x10, 0x20):
        raise ValueError("未识别的模型显隐位描述")
    return bool(bytes.fromhex(field["raw"])[offset] & field_mask)


def identity(row):
    """身份同时保存地址、对象表索引和序列号，避免仅比较复用地址。"""
    return {key: row[key] for key in ("address", "index", "serial", "name", "class_name")}


def summarize(path):
    """保留挂载相关结构，不提交整份运行内存快照。"""
    # 每份输入文件独立哈希，保证摘要可以追溯到本机 Temp 原件。
    raw = path.read_bytes()
    snapshot = json.loads(raw.decode("utf-8"))
    objects = snapshot["objects"]
    mounts = {}
    for row in objects.values():
        if not row["name"].startswith(("SWP", "WeapMesh")):
            continue
        fields = row["fields"]
        mounts[row["name"]] = {**identity(row),
            "visible": flag(fields["bVisible"]), "hidden_in_game": flag(fields["bHiddenInGame"]),
            "socket": fields["AttachSocketName"]["name"],
            "mesh": fields["StaticMesh"]["value"].get("name"),
            "parent": fields["AttachParent"]["value"].get("name")}
    projectiles = []  # 只取玩家激活器登记的活动武器，不把静态模型当弹体。
    active = objects[snapshot["activator"]["address"]]["fields"]["ActiveWeapons"]
    for (address,) in struct.iter_unpack("<Q", bytes.fromhex(active["raw"])):
        row = objects[hex(address)]
        fields = row["fields"]
        root = objects.get(fields["RootComponent"]["value"]["address"])
        projectiles.append({**identity(row), "owner": fields["Owner"]["value"].get("name"),
            "root": identity(root) if root else None,
            "mesh": root["fields"]["StaticMesh"]["value"].get("name") if root else None,
            "root_parent": root["fields"]["AttachParent"]["value"] if root else None})
    groups = {}  # 原生发射状态与显示组件保持独立表示。
    for label, group in snapshot["groups"].items():
        groups[label] = {key: group[key] for key in ("address", "ammo", "max_ammo", "window", "cursor", "count", "capacity")}
        groups[label]["weapon_class"] = group["class"]["name"]
        groups[label]["slots"] = [{"index": slot["index"], "socket": slot.get("socket_name_candidate"),
            "duration": slot["duration"], "elapsed": slot["elapsed"],
            "route_indices_0x40_0x44": slot.get("index_candidates_at_0x40_0x44")}
            for slot in group["slots"]]
        groups[label]["launch_index_table"] = group.get("launch_index_table")
    # 旧采样未读路由和计时队列时保留空值，不能把缺失说成空队列。
    routes = [{"group": group["group"], "entries": [
        {"index": entry["index"], "component": entry["component"].get("name"),
         "logical_visible": entry["logical_visible"]} for entry in group["entries"]]}
        for group in snapshot["launch_mesh_routes"]] if "launch_mesh_routes" in snapshot else None
    return {"source_file": path.name, "sha256": hashlib.sha256(raw).hexdigest(),
        "captured_local": snapshot["captured_local"], "plane": identity(snapshot["plane"]),
        "groups": groups, "mounts": mounts, "active_projectiles": projectiles,
        "launch_mesh_routes": routes, "mesh_display_timers": snapshot.get("mesh_display_timers"),
        "mesh_manager": snapshot.get("mesh_manager"),
        "mesh_arrays": {key: [row["name"] for row in entries] for key, entries in snapshot.get("mesh_arrays", {}).items()}}


def main():
    """按用户操作顺序传入快照，并比较前两份的组件身份。"""
    # 输出必须为新路径，避免覆盖已提交的历史证据。
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("snapshots", type=Path, nargs="+")
    args = parser.parse_args()
    samples = [summarize(path) for path in args.snapshots]
    if len(samples) < 2:
        raise ValueError("至少需要切换前后两份快照")
    before, after = samples[:2]
    comparison = []
    for name, row in before["mounts"].items():
        current = after["mounts"].get(name)
        comparison.append({"name": name, "same_identity": bool(current and all(
            row[key] == current[key] for key in ("address", "index", "serial"))),
            "before_visible": row["visible"], "before_hidden": row["hidden_in_game"],
            "after_visible": current["visible"] if current else None,
            "after_hidden": current["hidden_in_game"] if current else None})
    report = {"steam_build": 9855922, "game_operated_by": "用户", "agent_read_only": True,
        "candidate_installed": False, "dual_mount_implemented": False,
        "boundaries": ["仅当前 Su-30SM s03 一次任务", "显隐来自反射标志，不替代画面碰撞检查",
            "发射快照在弹体离架之后，未测量生成瞬间世界坐标", "未验证双模型候选、补给、检查点或 Part 效果"],
        "switch_comparison": comparison, "samples": samples}
    with args.output.open("x", encoding="utf-8") as stream:
        json.dump(report, stream, ensure_ascii=False, indent=2)
        stream.write("\n")
    print(args.output)


if __name__ == "__main__":
    main()
