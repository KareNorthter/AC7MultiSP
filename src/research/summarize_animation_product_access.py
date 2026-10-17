"""离线归纳主管只读采样；不打开进程、不调用游戏、不编译或运行测试。"""
from __future__ import annotations

import argparse
import hashlib
import json
import re
import struct
from pathlib import Path


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def build(captures: Path, workspace: Path) -> dict:
    names = [
        "f22a-8aam-paused-20261017-1144.json",
        "f22a-reflection-details-20261017-1144.json",
        "f22a-enum-entries-20261017-1144.json",
    ]
    source = [json.loads((captures / name).read_text(encoding="utf-8")) for name in names]
    for record in source:
        if record.get("process_write") is not False or record.get("game_functions_called") is not False:
            raise ValueError("原件没有明确声明只读且未调用游戏函数")
    main, details, enums = source
    metadata = details["metadata_objects"]
    bools = []
    for value in metadata.values():
        if value["name"] not in ("bNoSkeletonUpdate", "bPauseAnims"):
            continue
        raw = bytes.fromhex(value["raw"])
        bools.append({
            "name": value["name"], "kind": value["class"],
            "offset": hex(struct.unpack_from("<i", raw, 0x44)[0]),
            "field_size": raw[0x70], "byte_offset": raw[0x71],
            "byte_mask": raw[0x72], "field_mask": raw[0x73],
            "metadata_serial": value["serial"],
        })
    enum_rows = []
    for enum in enums["enums"]:
        raw = bytes.fromhex(enum["raw"])
        entries = enum["entries"]
        if len(raw) != enum["count"] * 16 or len(entries) != enum["count"]:
            raise ValueError("枚举原件长度或条目数不一致")
        for i, entry in enumerate(entries):
            if struct.unpack_from("<IIq", raw, i * 16) != (entry["index"], entry["number"], entry["value"]):
                raise ValueError("枚举具名条目与原始字节不一致")
        enum_rows.append({"name": enum["identity"]["name"],
                          "entries": [{"name": e["name"], "value": e["value"]} for e in entries]})
    parts = bytes.fromhex(details["parts_candidate"]["raw"])
    racks = []
    base = int(details["base"], 16)
    for rack in details["racks"]:
        vtable = bytes.fromhex(rack["vtable_raw"])
        racks.append({
            "name": rack["identity"]["name"], "serial": rack["identity"]["serial"],
            "flags_9b0": rack["fields"]["bNoSkeletonUpdate"]["raw_value"],
            "refresh_mode": rack["fields"]["MeshComponentUpdateFlag"]["raw_value"],
            "get_socket_transform_rva": hex(struct.unpack_from("<Q", vtable, 0x3d0)[0] - base),
        })
    method_rows = []
    methods = main["native_methods_by_class"]["SkeletalMeshComponent"]
    for name in ("GetSocketTransform", "SetComponentTickEnabled"):
        function = methods[name]
        method_rows.append({"name": name, "declaring_class": function["declaring_class"],
                            "rva": function["entry_minus_image_base"],
                            "parameters": [{"name": p["identity"]["name"],
                                            "kind": p["identity"]["class_name"],
                                            "size": p["element_size"], "offset": p["offset"]}
                                           for p in function["parameter_metadata"]]})
    folder = captures / "f22a-vtable-native-extended-20261017-1144"
    publication = folder / "16e5de0-bc.bin"
    tracker = workspace / "src/mount_animation_publication_tracker.hpp"
    text = tracker.read_text(encoding="utf-8")
    code = re.search(r'publishCode\s*=\s*((?:\s*"[0-9a-f]+")+)\s*;', text)
    if not code:
        raise ValueError("源码缺少完整发布入口签名")
    signature = bytes.fromhex("".join(re.findall(r'"([0-9a-f]+)"', code.group(1))))
    if signature != publication.read_bytes():
        raise ValueError("发布入口声明与本轮原始字节不一致")
    signature_rows = []
    search_folders = [folder, captures / "f22a-rack-control-native-20261017-1144",
                      captures / "f22a-socket-native-20261017-1144",
                      captures / "f22a-pose-publish-native-20261017-1144"]
    for rva, declared in re.findall(r'originalCodeMatches\((0x[0-9a-f]+),"([0-9a-f]+)"\)', text):
        target = int(rva, 16)
        expected = bytes.fromhex(declared)
        match = None
        for directory in search_folders:
            for path in directory.glob("*.bin"):
                parts_name = path.stem.split("-")
                start_text = parts_name[1] if parts_name[0] == "mount" else parts_name[0]
                try:
                    start = int(start_text, 16)
                except ValueError:
                    continue
                data = path.read_bytes()
                if start <= target and target + len(expected) <= start + len(data):
                    if data[target - start:target - start + len(expected)] != expected:
                        raise ValueError(f"声明签名与原件不符：{rva}")
                    match = "Temp/" + path.relative_to(captures).as_posix()
                    break
            if match:
                break
        if not match:
            raise ValueError(f"签名缺少原件：{rva}")
        signature_rows.append({"rva": rva, "hex": declared, "source": match})
    receipts = [{"path": f"Temp/{name}", "sha256": digest(captures / name)} for name in names]
    code_sources = [
        (publication, "0x16e5de0", "完整发布入口，0xbc字节，自然ret"),
        (folder / "16fa010-bf.bin", "0x16fa010", "当帧比较、Pause4、NoSkeleton2"),
        (captures / "f22a-rack-control-native-20261017-1144/mount-16e79d0-600.bin", "0x16e79d0", "已发布骨骼缓冲索引+0x74c读取"),
        (captures / "f22a-rack-control-native-20261017-1144/mount-7f0000-1000.bin", "0x7f0000", "禁用入口及两处分支直接置NoSkeletonUpdate"),
        (captures / "f22a-pose-publish-native-20261017-1144/mount-16fb660-220.bin", "0x16fb660", "全局原生帧写入组件+0xd24；不是完整逻辑函数"),
        (captures / "f22a-native-20261017-1144/mount-1ce6d70-1900.bin", "0x1ce6d70", "request1推进进度，phase端点2"),
    ]
    for path, rva, finding in code_sources:
        receipts.append({"path": "Temp/" + path.relative_to(captures).as_posix(), "rva": rva,
                         "bytes": path.stat().st_size, "sha256": digest(path), "finding": finding})
    sequence_path = captures / "f22a-native-pose-sequence-20261017-1144.jsonl"
    records = [json.loads(line) for line in sequence_path.read_text(encoding="utf-8").splitlines()]
    samples = [row for row in records if "racks" in row]
    sequence_racks = []
    for name in ("SWP1", "SWP2", "SWP3"):
        states = [next(r for r in row["racks"] if r["name"] == name) for row in samples]
        sequence_racks.append({
            "name": name,
            "animation_flag_values": sorted({r["animation_flags"] for r in states}),
            "raw_3f_values_not_tick_enabled": sorted({r["tick"] for r in states}),
            "buffer_changes": sum(a["buffers"] != b["buffers"] for a, b in zip(states, states[1:])),
            "coincident_native_frame_samples": sum(row["frame_before"] == row["frame_after"] == state["pose_tick_frame_candidate"]
                                                   for row, state in zip(samples, states)),
            "animation_frame_values_count": len({r["pose_tick_frame_candidate"] for r in states}),
        })
    transitions = []
    for row in samples:
        state = (row["native_request"], row["phase"])
        if not transitions or tuple(transitions[-1]["request_phase"]) != state:
            transitions.append({"seconds": row["t"], "request_phase": state, "public_progress": row["progress"][0]})
    receipts.append({"path": "Temp/" + sequence_path.name, "sha256": digest(sequence_path)})
    index_path = captures / "su30sm-fixed-mount-prototype/Temp/three-sp-failure/object-index.json"
    index = json.loads(index_path.read_text(encoding="utf-8"))
    by_address = {row["address"]: row for row in index}
    bone_functions = []
    for row in index:
        if row["name"] in ("HideBoneByName", "UnHideBoneByName", "IsBoneHiddenByName"):
            owner = by_address.get(row["outer"], {})
            bone_functions.append({"name": row["name"], "owner": owner.get("name"),
                                   "parameters_qualified": False, "current_instance_identity": False})
    receipts.append({"path": "Temp/" + index_path.relative_to(captures).as_posix(), "sha256": digest(index_path)})
    return {
        "scope": "三机产品访问适配研究；原件由主管只读采集，本线程仅离线分析",
        "runtime_ready": False, "compiled": False, "tests_run": False, "installed": False,
        "process_access_by_this_work": False, "game_functions_called_by_this_work": False,
        "sources": receipts, "bool_metadata": bools, "enums": enum_rows, "racks": racks,
        "tracker_declared_code_signatures": signature_rows,
        "implementation_sha256": {name: digest(workspace / "src" / name) for name in (
            "mount_animation_reflection_adapter.hpp", "mount_animation_product_access.hpp",
            "mount_animation_publication_tracker.hpp")},
        "methods": method_rows,
        "continuous_polling": {"samples": len(samples), "racks": sequence_racks,
                               "request_phase_transitions": transitions,
                               "boundary": "轮询不证明函数调用或全部调度顺序；没有生成发布见证"},
        "historical_bone_visibility_methods": bone_functions,
        "parts": {"request": struct.unpack_from("<i", parts, 0x58)[0],
                  "phase": struct.unpack_from("<i", parts, 0x274)[0],
                  "native_config": struct.unpack_from("<i", parts, 0x178)[0],
                  "progress": struct.unpack_from("<7f", parts, 0x17c)},
        "qualified_candidate": {
            "publication_rva": "0x16e5de0", "publication_vtable_slot": "0x890",
            "native_frame_rva": "0x3ac5520", "component_animation_frame_offset": "0xd24",
            "published_bone_buffer_offset": "0x74c", "publication_pending_offset": "0x828",
            "token": "真实原生帧+1，仅当前发布钩子尾部同步消费",
        },
        "supersedes_old_contract": [
            "mask2为NoSkeletonUpdate，mask4才是Pause；旧两字段RackRequest不足",
            "旧不碰NoSkeletonUpdate约定阻断非当前provider更新，必须修订",
            "request1可已达到打开端点，旧request==2门槛过严",
        ],
        "remaining": [
            "四项独立租约、完整原生请求观察及独占登记",
            "几何注册与真实Host接线",
            "原生发布钩子及同步显示/发射消费",
            "嵌套Transform元数据实机反射匹配及实际调用验证",
            "MasterPose弱属性元数据与四份资源引用的运行绑定核对",
            "隐藏provider每帧真实发布与稳定性/性能实测",
        ],
    }


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--captures", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    workspace = Path(__file__).resolve().parents[2]
    result = build(args.captures, workspace)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("w", encoding="utf-8", newline="\n") as stream:
        json.dump(result, stream, ensure_ascii=False, indent=2)
        stream.write("\n")
    print(f"已生成离线归纳：{args.output}；未编译、测试、安装或访问游戏。")


if __name__ == "__main__":
    main()
