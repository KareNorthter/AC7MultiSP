"""静态研究原版 SP 切换动画；只读离线副本，不编译、不连接游戏或运行测试。"""

import argparse
import base64
import hashlib
import itertools
import json
import math
from pathlib import Path
import re
import struct

from summarize_mount_assets import properties, resolve, value
from survey_aircraft_mounts import multiply, reference_bones, rotate, vector


SP_CHANNELS = {"spwc", "swcc", "swc1", "swc2", "swc3"}
FORMAT_REFERENCE = "https://github.com/gildor2/UEViewer/blob/master/Unreal/UnrealMesh/UnAnim4.cpp"


def digest(raw):
    return hashlib.sha256(raw).hexdigest()


def manifests(path):
    """清单可嵌套；路径清单相对其所在目录解析。"""
    for row in json.loads(path.read_text(encoding="utf-8-sig")):
        if isinstance(row, str):
            nested = Path(row)
            yield from manifests(nested if nested.is_absolute() else path.parent / nested)
        elif isinstance(row, dict) and row.get("json"):
            yield row


class Assets:
    """保留包内完整身份，优先官方更新包，核对实际读取的资源对。"""

    def __init__(self, paths):
        self.entries, self.cache, self.used = {}, {}, {}
        for path in paths:
            for entry in manifests(path):
                if not re.fullmatch(r"pakchunk\d+-WindowsNoEditor(?:_0_P)?\.pak", entry["pak"]):
                    raise ValueError("不接受第三方资源来源")
                key = Path(entry["asset"]).stem.casefold()
                self.entries.setdefault(key, []).append(entry)

    def has(self, name):
        return name.casefold() in self.entries

    def load(self, name):
        key = name.casefold()
        if key not in self.cache:
            choices = self.entries[key]
            entry = sorted(choices, key=lambda x: "_0_P." in x["pak"], reverse=True)[0]
            same_pak = [x for x in choices if x["pak"] == entry["pak"]]
            if len({(x["asset"], x["encrypted_sha256"]) for x in same_pak}) != 1:
                raise ValueError("同名资源身份有冲突：" + name)
            parsed = Path(entry["json"])
            decoded = parsed.with_suffix(".uasset")
            encrypted = decoded.parent.parent / decoded.name
            actual = {"encrypted_sha256": digest(encrypted.read_bytes()),
                      "decoded_sha256": digest(decoded.read_bytes())}
            for field, result in actual.items():
                if entry.get(field) and entry[field].lower() != result:
                    raise ValueError("资源摘要改变：" + name + "/" + field)
            # uexp 没有既有期望摘要，保存本次摘要供后续重现比对，不宣称独立校验通过。
            actual.update(encrypted_uexp_sha256=digest(encrypted.with_suffix(".uexp").read_bytes()),
                          decoded_uexp_sha256=digest(decoded.with_suffix(".uexp").read_bytes()),
                          parsed_json_sha256=digest(parsed.read_bytes()))
            self.used[key] = {"pak": entry["pak"], "asset": entry["asset"], **actual}
            self.cache[key] = json.loads(parsed.read_text(encoding="utf-8-sig"))
        return self.cache[key]


class Reader:
    """有边界检查的小端读取器；不扫描猜测数据流起点。"""

    def __init__(self, raw):
        self.raw, self.pos = raw, 0

    def take(self, size):
        if size < 0 or self.pos + size > len(self.raw):
            raise ValueError("动画数据截断")
        result = self.raw[self.pos:self.pos + size]
        self.pos += size
        return result

    def integer(self):
        return struct.unpack("<i", self.take(4))[0]

    def integers(self):
        count = self.integer()
        if not 0 <= count <= 100000:
            raise ValueError("动画数组数量异常")
        return list(struct.unpack("<" + "i" * count, self.take(count * 4)))

    def name(self, names):
        index, number = struct.unpack("<ii", self.take(8))
        if not 0 <= index < len(names) or number != 0:
            raise ValueError("未支持此 FName")
        return names[index]


def animation_tracks(document, bones):
    """只支持本批实见的 ConstantKeyLerp/None/Float96NoW/无缩放轨格式。"""
    export = document["Exports"][0]
    props = properties(export)
    frames = props["NumFrames"]
    raw = base64.b64decode(export["Extras"], validate=True)
    reader = Reader(raw)
    reader.take(16)  # 既有 UAssetAPI 未解析的序列 GUID。
    if reader.take(2) != b"\x01\x00" or reader.integer() != 1:
        raise ValueError("不是本批 cooked 压缩序列")
    if reader.take(4) != b"\x00\x00\x01\x00":
        raise ValueError("遇到其他压缩格式，不能套用本批解码")
    offsets = reader.integers()
    scale_offsets, scale_strip = reader.integers(), reader.integer()
    if scale_offsets or scale_strip != 2:
        raise ValueError("本批不支持缩放轨")
    mapping = reader.integers()
    if mapping != [p["BoneTreeIndex"] for p in props["TrackToSkeletonMapTable"]]:
        raise ValueError("压缩轨与属性骨骼映射不一致")
    if len(offsets) != len(mapping) * 4 or len(set(mapping)) != len(mapping):
        raise ValueError("轨数量或骨骼映射重复")
    # CompressedCurveData 的带标签属性；骨骼轨之外的曲线只保存数量和摘要，不参与位姿推断。
    if reader.name(document["NameMap"]) != "FloatCurves" or reader.name(document["NameMap"]) != "ArrayProperty":
        raise ValueError("压缩曲线结构改变")
    size, array_index = reader.integer(), reader.integer()
    if array_index != 0 or reader.name(document["NameMap"]) != "StructProperty" or reader.take(1) != b"\x00":
        raise ValueError("压缩曲线标签改变")
    curve = reader.take(size)
    if size < 53:
        raise ValueError("压缩曲线标签负载截断")
    curve_count = struct.unpack_from("<i", curve)[0]
    if not 0 <= curve_count <= 1000:
        raise ValueError("压缩曲线数量异常")
    if reader.name(document["NameMap"]) != "None":
        raise ValueError("压缩曲线结束标签异常")
    raw_size, stream_size = reader.integer(), reader.integer()
    stream = reader.take(stream_size)
    if reader.integer() != 0 or reader.pos != len(raw):
        raise ValueError("存在未解释的序列尾部或启用了 RawDataOnly")
    tracks, ranges = {}, []
    for track_index, bone_index in enumerate(mapping):
        if not 0 <= bone_index < len(bones):
            raise ValueError("轨指向骨架范围外")
        trans_offset, trans_count, rot_offset, rot_count = offsets[track_index * 4:track_index * 4 + 4]
        arrays = []
        for offset, count in ((trans_offset, trans_count), (rot_offset, rot_count)):
            if count not in (1, frames) or offset < 0 or offset + count * 12 > len(stream):
                raise ValueError("当前只接受单键或每帧一键的完整采样")
            values = [struct.unpack_from("<3f", stream, offset + i * 12) for i in range(count)]
            if not all(math.isfinite(v) for row in values for v in row):
                raise ValueError("动画键不是有限数")
            arrays.append(values)
            ranges.append((offset, offset + count * 12))
        translation, xyz = arrays
        rotations = []
        for q in xyz:
            norm = sum(v * v for v in q)
            if norm > 1.00001:
                raise ValueError("Float96NoW 四元数无效")
            rotation = (*q, math.sqrt(max(0, 1 - norm)))
            magnitude = math.sqrt(sum(v * v for v in rotation))
            rotations.append(tuple(v / magnitude for v in rotation))
        tracks[bone_index] = {"translation": translation, "rotation": rotations}
    cursor = 0
    for start, end in sorted(ranges):
        if start != cursor:
            raise ValueError("数据轨存在重叠或未解释的间隙")
        cursor = end
    if cursor != len(stream):
        raise ValueError("存在未解释的压缩数据")
    return tracks, {"frames": frames, "asset_length": props["SequenceLength"],
                    "track_count": len(mapping), "compressed_stream_bytes": stream_size,
                    "compressed_raw_data_size": raw_size,
                    "non_bone_curve_count": curve_count,
                    "non_bone_curve_payload_sha256": digest(curve),
                    "non_bone_curve_values_interpreted": False,
                    "compressed_stream_sha256": digest(stream),
                    "all_track_bytes_accounted_for": True,
                    "format": "ConstantKeyLerp / None / Float96NoW / no scale tracks"}


def angle(a, b):
    magnitude = math.sqrt(sum(v * v for v in a) * sum(v * v for v in b))
    return math.degrees(2 * math.acos(min(1, abs(sum(x * y for x, y in zip(a, b))) / magnitude)))


def poses(bones, tracks, frames):
    """按原始键采样复合局部变换；这不是 FlightEngineBlend 多通道的实机输出。"""
    result = []
    for frame in range(frames):
        pose = []
        for index, bone in enumerate(bones):
            track = tracks[index]
            location = track["translation"][min(frame, len(track["translation"]) - 1)]
            quaternion = track["rotation"][min(frame, len(track["rotation"]) - 1)]
            if bone["parent"] >= 0:
                parent_location, parent_quaternion = pose[bone["parent"]]
                location = tuple(a + b for a, b in zip(parent_location, rotate(parent_quaternion, location)))
                quaternion = multiply(parent_quaternion, quaternion)
            pose.append((location, quaternion))
        result.append(pose)
    return result


def skeleton_analysis(assets, skeleton, sequence, channels):
    document = assets.load(skeleton)
    export = next(e for e in document["Exports"] if e["ObjectName"].casefold() == skeleton.casefold())
    bones = reference_bones(document, export)
    seq_document = assets.load(sequence)
    seq_props = properties(seq_document["Exports"][0])
    if resolve(seq_document, seq_props["Skeleton"]).casefold() != skeleton.casefold():
        raise ValueError("序列骨架引用与目标不符")
    tracks, summary = animation_tracks(seq_document, bones)
    missing = sorted(set(range(len(bones))) - set(tracks))
    raw_reference = base64.b64decode(export["Extras"], validate=True)
    for index in missing:
        # 缺轨只用明确读出的局部参考姿态作计算占位，并在结果中标出；不宣称引擎缺轨策略已验证。
        transform = struct.unpack_from("<10f", raw_reference, 8 + len(bones) * 12 + index * 40)
        tracks[index] = {"translation": [tuple(transform[4:7])], "rotation": [tuple(transform[:4])]}
    summary["missing_animation_tracks"] = [bones[i]["name"] for i in missing]
    summary["missing_track_policy"] = "计算使用局部参考姿态占位；游戏实际缺轨处理未核对" if missing else None
    samples = poses(bones, tracks, summary["frames"])
    controlled = []
    for index, bone in enumerate(bones):
        channel = channels.get(bone["name"])
        if channel not in SP_CHANNELS:
            continue
        track = tracks[index]
        trans, rot = track["translation"], track["rotation"]
        controlled.append({"bone": bone["name"], "parent": bone["parent"], "channel": channel,
                           "translation_span_from_frame0": round(max(math.dist(trans[0], t) for t in trans), 6),
                           "rotation_span_degrees_from_frame0": round(max(angle(rot[0], q) for q in rot), 5),
                           "serialized_track_present": index not in missing,
                           "track_sha256": digest(json.dumps(track, separators=(",", ":")).encode())})
    by_name = {bone["name"]: i for i, bone in enumerate(bones)}
    sockets = []
    for reference in properties(export).get("Sockets", []):
        if reference <= 0:
            raise ValueError("Socket 不是本资源导出")
        prop = properties(document["Exports"][reference - 1])
        if not prop["SocketName"].lower().startswith(("sp", "ws")):
            continue
        index = by_name.get(prop.get("BoneName"))
        if index is None:
            sockets.append({"socket": prop["SocketName"], "bone": prop.get("BoneName"),
                            "trajectory": None, "reason": "BoneName 省略或未匹配，不猜根骨骼"})
            continue
        driven = sorted({channels[n] for n in bones[index]["chain"] if n in channels})
        row = {"socket": prop["SocketName"], "bone": prop["BoneName"],
               "ancestor_channels": driven, "bone_chain": bones[index]["chain"]}
        if "RelativeLocation" not in prop:
            row.update(trajectory=None, reason="省略 RelativeLocation，不猜默认变换")
        else:
            local = vector(prop["RelativeLocation"])
            points = [tuple(a + b for a, b in zip(pose[index][0], rotate(pose[index][1], local))) for pose in samples]
            rotations = [pose[index][1] for pose in samples]
            row["trajectory"] = {
                "sampling": "整个组件的序列同步逐帧取样，不能替代游戏多通道混合、机翼姿态或世界变换",
                "frame0": [round(v, 6) for v in points[0]],
                "last_frame": [round(v, 6) for v in points[-1]],
                "aabb": [[round(min(p[a] for p in points), 6), round(max(p[a] for p in points), 6)] for a in range(3)],
                "max_distance_from_frame0": round(max(math.dist(points[0], p) for p in points), 6),
                "max_parent_rotation_degrees_from_frame0": round(max(angle(rotations[0], q) for q in rotations), 5),
                "position_samples_sha256": digest(json.dumps(points, separators=(",", ":")).encode()),
                "resource_units_only": True,
            }
        sockets.append(row)
    summary.update(skeleton=skeleton, sequence=sequence, bone_count=len(bones),
                   reference_bones_sha256=digest(json.dumps(bones, separators=(",", ":")).encode()),
                   controlled_bones=controlled, sockets=sockets,
                   channels=sorted({row["channel"] for row in controlled}),
                   moving_controlled_bones=sum(row["translation_span_from_frame0"] > .0001 or
                                               row["rotation_span_degrees_from_frame0"] > .01 for row in controlled))
    return summary


def graph(assets, name):
    """只接受资源明确指定的单一 FlightEngineBlend，不根据文件名猜驱动链。"""
    document = assets.load(name)
    default = next(e for e in document["Exports"] if e["ObjectName"].startswith("Default__"))
    nodes = [v for k, v in properties(default).items() if k.startswith("AnimGraphNode_FlightEngineBlend")]
    if len(nodes) != 1:
        raise ValueError("不是单一 FlightEngineBlend：" + name)
    node = nodes[0]
    return {"anim_blueprint": name,
            "bone_config": resolve(document, node["BoneConfig"]),
            "sequence": resolve(document, node["SequenceConfig"]["Sequence"]),
            "sequence_options": {k: v for k, v in node["SequenceConfig"].items() if k != "Sequence"}}


def compare_tracks(assets, first, second):
    """同时报告精确相等与数值差异，避免把浮点微差写成显著不同的动作。"""
    decoded = []
    for component in (first, second):
        name = component["skeleton"]
        doc = assets.load(name)
        bones = reference_bones(doc, next(e for e in doc["Exports"] if e["ObjectName"].casefold() == name.casefold()))
        tracks, _ = animation_tracks(assets.load(component["sequence"]), bones)
        decoded.append({bone["name"]: tracks[i] for i, bone in enumerate(bones) if i in tracks})
    names = sorted({r["bone"] for r in first["controlled_bones"]} & {r["bone"] for r in second["controlled_bones"]})
    differences = []
    for name in names:
        if name not in decoded[0] or name not in decoded[1]:
            continue
        a, b = decoded[0][name], decoded[1][name]
        distance = max(math.dist(a["translation"][min(f, len(a["translation"]) - 1)],
                                 b["translation"][min(f, len(b["translation"]) - 1)]) for f in range(first["frames"]))
        rotation = max(angle(a["rotation"][min(f, len(a["rotation"]) - 1)],
                             b["rotation"][min(f, len(b["rotation"]) - 1)]) for f in range(first["frames"]))
        if a != b:
            differences.append({"bone": name, "max_local_position_difference": round(distance, 6),
                                "max_local_rotation_difference_degrees": round(rotation, 5)})
    return {"groups": [first["group"], second["group"]], "shared_controlled_bones": len(names),
            "reference_bones_exactly_equal": first["reference_bones_sha256"] == second["reference_bones_sha256"],
            "same_named_local_tracks_equal": bool(names) and not differences,
            "different_local_tracks": differences,
            "boundary": "同名局部曲线相等不证明父链、网格、蒙皮或挂点可互换"}


def native_context(manifest):
    """复核旧只读采样的摘要和入口；本次不采集新进程，不信任磁盘加壳代码。"""
    proof = json.loads(manifest.read_text(encoding="utf-8"))
    region = next(row for row in proof["ranges"] if row["rva"] == "0x1ce0000")
    raw = (manifest.parent / region["file"]).read_bytes()
    if len(raw) != region["length"] or digest(raw) != region["sha256"]:
        raise ValueError("原生旧采样指纹改变")
    signature = bytes.fromhex("4d85c00f84fe170000488bc455564881ecb8000000")
    if raw[0x6d70:0x6d70 + len(signature)] != signature:
        raise ValueError("旧采样的 PartsUpdate 入口不匹配")
    return {"basis": "复核既有F-35C只读内存采样，不是本轮游戏采样",
            "manifest_sha256": digest(manifest.read_bytes()), "region_sha256": digest(raw),
            "function_rva": "0x1ce6d70", "entry_signature_verified": True,
            "switch_dispatch_rva": "0x1ce8013",
            "switch_dispatch_sha256": digest(raw[0x8013:0x8073]),
            "state_machine_rva": "0x1ce7f57",
            "state_machine_sha256": digest(raw[0x7f57:0x8013]),
            "selection_field": "PartsAnimation+0x178",
            "public_progress": "PartsAnimation+0x17c",
            "native_selected_writes": {"0": ["0x180", "0x194", "0x190"],
                                       "1": ["0x184", "0x18c", "0x194"],
                                       "2": ["0x188", "0x18c", "0x190"]},
            "request_states": {"0": "关闭，-0.01", "1": "打开中，沿原生速率推进至1",
                               "2": "全开，1", "3": "关闭中，沿原生速率退至-0.01"},
            "observation": "这里只复制当前配置命中的分通道，未命中分通道没有自动收拢写入",
            "boundary": "偏移只描述此版本函数和旧采样；不证明其他机体/展示实例运行指针已定位，不能直接据此写进程"}


def layout_overlay(batch, targets):
    """保留参考线程的七组合阻塞，不因动画取得新证据就放行整个机体。"""
    samples = {a["code"]: a for a in json.loads(batch.read_text(encoding="utf-8"))["aircraft"]}
    result = []
    for target in targets:
        if not target["components"] or target["code"] == "f35c":
            continue
        source = samples[target["code"]]
        combinations = source["proposal"]["combinations"]
        groups = {g["group"]: g for g in source["groups"]}
        if len(combinations) != 7 or len({tuple(sorted(c["weapons"])) for c in combinations}) != 7:
            raise ValueError("上游七组合覆盖不完整")
        cases = []
        for combo in combinations:
            assignments = [{"weapon": row["weapon"], "group": row["group"], "sockets": row["sockets"]}
                           for row in combo["assignments"]]
            selected = sorted({groups[row["group"]]["configuration"] for row in assignments})
            cases.append({"weapons": combo["weapons"], "configs": selected, "assignments": assignments,
                          "upstream_layout_status": combo["status"],
                          "upstream_blocker_kind": combo.get("blocker_kind"),
                          "animation_mode": target["animation_design"]["mode"],
                          "animation_design_ref": target["code"],
                          "whole_aircraft_integration_allowed": False,
                          "reason": "本轮补齐动画设计与采样依据；七组合支撑/几何及运行时交接条件仍未全部满足"})
        result.append({"code": target["code"], "combination_count": 7, "combinations": cases})
    return {"upstream_record": batch.name, "upstream_sha256": digest(batch.read_bytes()),
            "source_thread": "01a1441b-45a7-7d91-80f2-cf03b98647b1",
            "policy": "all_seven_required", "aircraft": result,
            "combination_count": sum(row["combination_count"] for row in result)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--catalog", type=Path, required=True)
    parser.add_argument("--manifest", type=Path, action="append", required=True)
    parser.add_argument("--batch", type=Path, required=True, help="参考线程第三批七组合方案")
    parser.add_argument("--native-manifest", type=Path, help="旧只读原生采样的显式manifest")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    catalog = json.loads(args.catalog.read_text(encoding="utf-8"))
    if catalog["steam_build"] != 9855922 or len(catalog["aircraft"]) != 47:
        raise ValueError("不是当前47架官方目录")
    policy_path = Path(__file__).with_name("sp_switch_animation_policies.json")
    policy = json.loads(policy_path.read_text(encoding="utf-8"))
    assets = Assets(args.manifest)
    scanned, targets = [], []
    for aircraft in catalog["aircraft"]:
        code = aircraft["code"]
        pawn = assets.load("AcePlayerPawn_" + code)
        if assets.used[("AcePlayerPawn_" + code).casefold()]["encrypted_sha256"] != aircraft["pawn_source"]["encrypted_sha256"]:
            raise ValueError("Pawn来源与目录不一致：" + code)
        default = next(e for e in pawn["Exports"] if e["ObjectName"] == "Default__AcePlayerPawn_" + code + "_C")
        parts = resolve(pawn, properties(default)["PartsAnimationParamDataTable"])
        parts_doc = assets.load(parts)
        options = [value(row) for row in parts_doc["Exports"][0]["Table"]["Data"]]
        if len(options) != 1:
            raise ValueError("不是单一动画参数行：" + parts)
        sp_flags = {k: v for k, v in options[0].items() if "Spw" in k or k == "bOptionsAnimWpCover"}
        skeletal = [g for g in aircraft["swp_components"] if g["class"] == "SkeletalMeshComponent"]
        has_sp_flags = any(v for k, v in sp_flags.items() if "Spw" in k)
        row = {"code": code, "name": aircraft["display_name_en"], "parts_asset": parts,
               "sp_options": sp_flags, "skeletal_groups": [g["group"] for g in skeletal],
               "target": bool(skeletal or has_sp_flags)}
        scanned.append(row)
        if not row["target"]:
            continue
        components = []
        for group in skeletal:
            source = next(e for e in pawn["Exports"] if e["ObjectName"] == group["component"])
            prop = properties(source)
            mesh = resolve(pawn, prop["SkeletalMesh"])
            anim = resolve(pawn, prop["AnimClass"]).removesuffix("_C")
            driver = graph(assets, anim)
            mesh_doc = assets.load(mesh)
            mesh_export = next(e for e in mesh_doc["Exports"] if e["ObjectName"].casefold() == mesh.casefold())
            mesh_props = properties(mesh_export)
            mesh_skeleton = resolve(mesh_doc, mesh_props["Skeleton"])
            if mesh_skeleton.casefold() != (mesh + "_Skeleton").casefold():
                raise ValueError("网格实际Skeleton引用与所解析骨架不同：" + mesh)
            table = assets.load(driver["bone_config"])
            channels = {r["BoneName"]: r["FlightEngineOuputName"] for r in
                        (value(raw) for raw in table["Exports"][0]["Table"]["Data"])}
            analysis = skeleton_analysis(assets, mesh + "_Skeleton", driver["sequence"], channels)
            components.append({"group": group["group"], "mesh": mesh, "driver": driver,
                               "mesh_extras_sha256": digest(base64.b64decode(mesh_export["Extras"], validate=True)),
                               "mesh_extras_boundary": "仅核对不透明payload摘要，未解释材质索引、蒙皮或完整几何，不据此放行网格替换",
                               **analysis})
        body_component = next(e for e in pawn["Exports"] if e["ObjectName"] == "PlaneBodyMesh")
        body_properties = properties(body_component)
        body_anim = resolve(pawn, body_properties["AnimClass"]).removesuffix("_C")
        body_mesh = resolve(pawn, body_properties["SkeletalMesh"])
        body_driver = graph(assets, body_anim)
        table = assets.load(body_driver["bone_config"])
        channels = {r["BoneName"]: r["FlightEngineOuputName"] for r in
                    (value(raw) for raw in table["Exports"][0]["Table"]["Data"])}
        body = skeleton_analysis(assets, body_mesh + "_Skeleton", body_driver["sequence"], channels)
        comparisons = []
        for first, second in itertools.combinations(components, 2):
            comparisons.append(compare_tracks(assets, first, second))
        design = policy["policies"].get(code)
        if bool(components) != bool(design):
            raise ValueError("活动SWP和逐机动画设计不配套：" + code)
        targets.append({**row, "body_driver": body_driver, "body": body,
                        "components": components, "comparisons": comparisons,
                        "classification": "animated_swp" if components else "flags_only_no_sp_bones_in_body",
                        "animation_design": design,
                        "integration_allowed": False, "gameplay_verified": False})
    report = {"schema": 1, "steam_build": 9855922, "baseline": "v2.4.0",
              "method": "原生Pawn实际参数引用、cooked动画轨解码、骨架父链及101帧资源取样",
              "format_reference": FORMAT_REFERENCE,
              "catalog_sha256": digest(args.catalog.read_bytes()),
              "analyzer_sha256": digest(Path(__file__).read_bytes()),
              "policy_sha256": digest(policy_path.read_bytes()),
              "animation_contract": policy["shared_contract"],
              "game_process_access": False, "compiled": False, "tests_run": False, "installed": False,
              "scan": scanned, "aircraft": targets, "sources": assets.used,
              "layout_overlay": layout_overlay(args.batch, targets),
              "native_control": native_context(args.native_manifest) if args.native_manifest else None,
              "counts": {"scanned": len(scanned), "targets": len(targets),
                         "skeletal_components": sum(len(a["components"]) for a in targets),
                         "animated_aircraft": sum(bool(a["components"]) for a in targets),
                         "flags_only_aircraft": sum(not a["components"] for a in targets),
                         "unique_decoded_sequences": len({s["sequence"] for a in targets for s in [a["body"], *a["components"]]}),
                         "decoded_sequences": sum(len(a["components"]) + 1 for a in targets)},
              "limits": ["只含官方资源，第三方覆盖另核对", "采样长度不等于开舱耗时",
                         "资源坐标未换算为米，不用原点轨迹冒充网格扫掠或碰撞",
                         "没有运行时模型池、逻辑队列和动画状态交接证据",
                         "integration_allowed=false不否认既有F-35C适配，只表示本报告不新增运行准入"]}
    with args.output.open("x", encoding="utf-8", newline="\n") as stream:
        json.dump(report, stream, ensure_ascii=False, indent=2)
        stream.write("\n")
    print(json.dumps(report["counts"], ensure_ascii=False))
    for aircraft in targets:
        print(aircraft["code"], "Body=" + "/".join(aircraft["body"]["channels"]),
              [(g["group"], g["channels"], g["moving_controlled_bones"]) for g in aircraft["components"]])


if __name__ == "__main__":
    main()
