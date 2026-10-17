"""离线核对F22腹/侧机构和TG静态挂点所在舱域；不编译、不运行测试或访问游戏。"""
from __future__ import annotations

import argparse
import base64
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import struct

from analyze_sp_switch_animations import Assets, animation_tracks, graph, poses
from summarize_mount_assets import properties, value
from survey_aircraft_mounts import reference_bones, vector


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def skeleton(assets, name):
    doc = assets.load(name)
    export = next(e for e in doc["Exports"] if e["ObjectName"] == name)
    bones = reference_bones(doc, export)
    raw = base64.b64decode(export["Extras"], validate=True)
    local = [struct.unpack_from("<10f", raw, 8 + len(bones) * 12 + i * 40) for i in range(len(bones))]
    return doc, export, bones, local


def channels(assets, blueprint):
    driver = graph(assets, blueprint)
    table = assets.load(driver["bone_config"])
    mapping = {r["BoneName"]: r["FlightEngineOuputName"] for r in map(value, table["Exports"][0]["Table"]["Data"])}
    return driver, mapping


def bone_rows(bones, mapping, channel):
    return [{"name": b["name"], "parent": b["parent"], "reference_component": b["position"]}
            for b in bones if mapping.get(b["name"]) == channel]


def sockets(document, export):
    result = []
    for ref in properties(export).get("Sockets", []):
        row = properties(document["Exports"][ref - 1])
        result.append({"name": row["SocketName"], "bone": row.get("BoneName"),
                       "local_position": vector(row["RelativeLocation"]),
                       "rotation_serialized": "RelativeRotation" in row,
                       "scale_serialized": "RelativeScale" in row})
    return result


def sub(a, b):
    return tuple(x - y for x, y in zip(a, b))


def cross(a, b):
    return (a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0])


def dot(a, b):
    return sum(x*y for x, y in zip(a, b))


def ray_hits(point, direction, points, triangles):
    hits = []
    for triangle in triangles:
        a, b, c = (points[i] for i in triangle)
        edge1, edge2 = sub(b, a), sub(c, a)
        h = cross(direction, edge2)
        det = dot(edge1, h)
        if abs(det) < 1e-11:
            continue
        s = sub(point, a)
        u = dot(s, h) / det
        if u < 0 or u > 1:
            continue
        q = cross(s, edge1)
        v = dot(direction, q) / det
        if v < 0 or u + v > 1:
            continue
        distance = dot(edge2, q) / det
        if distance > 1e-8:
            hits.append(distance)
    # 同一表面被两个三角形共同命中只保留一次；这里不是运行容差或净空预算。
    unique = []
    for hit in sorted(hits):
        if not unique or abs(hit - unique[-1]) > 1e-6:
            unique.append(hit)
    return unique


def tg_geometry(assets, reader):
    driver, mapping = channels(assets, "su57tg_AnimBP")
    _, _, bones, local = skeleton(assets, "su57tg_Skeleton")
    tracks, info = animation_tracks(assets.load(driver["sequence"]), bones)
    if len(tracks) != len(bones) or info["frames"] != 101:
        raise ValueError("TG Body轨道不完整")
    # 只推进SPWC；飞控/起落架等保持各骨骼实际局部参考姿态。
    isolated = {i: tracks[i] if mapping.get(b["name"]) == "spwc" else
                {"rotation": [local[i][:4]], "translation": [local[i][4:7]]} for i, b in enumerate(bones)}
    sample = poses(bones, isolated, 101)
    model = reader.read_skeletal_mesh(assets.load("su57tg"), "su57tg")
    sm = assets.load("su57tg_SWP3_SM")
    export = next(e for e in sm["Exports"] if e["ObjectName"] == "su57tg_SWP3_SM")
    sites = sockets(sm, export)
    records = []
    for lod_index, lod in enumerate(model["lods"]):
        influenced = set()
        for section in lod["sections"]:
            for index in range(section["start"], section["start"] + section["vertices"]):
                if any(weight and mapping.get(model["bones"][section["bone_map"][bone]]["name"]) == "spwc"
                       for bone, weight in lod["weights"][index]):
                    influenced.add(index)
        triangles = [tuple(lod["indices"][i:i+3]) for i in range(0, len(lod["indices"]), 3)]
        covers = [t for t in triangles if any(i in influenced for i in t)]
        frames = []
        for frame in (0, 100):
            by_name = {b["name"]: pose for b, pose in zip(bones, sample[frame])}
            points = reader.skin_points(model, lod, by_name)
            observations = []
            for site in sites:
                # 三条略有差别的向下射线，避免仅依赖恰好压在三角面公共边上的单次结果。
                rays = []
                for direction in ((0., 0., -1.), (.013, .007, -1.), (-.011, -.005, -1.)):
                    hits = ray_hits(site["local_position"], direction, points, covers)
                    rays.append({"direction": direction, "nearest_spwc_cover_below": hits[0] if hits else None,
                                 "surface_hit_count": len(hits)})
                observations.append({"socket": site["name"], "rays": rays})
            frames.append({"resource_spwc_key": frame, "sites": observations})
        records.append({"lod": lod_index, "cover_triangles": len(covers), "samples": frames})
    return {"sockets": sites, "body_spwc_bones": bone_rows(bones, mapping, "spwc"),
            "lods": records,
            "boundary": "只证明资源姿态下从静态挂点向下是否被SPWC门体遮挡；不是实机世界坐标、全封闭体积或完整武器扫掠证明"}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", action="append", type=Path, required=True)
    parser.add_argument("--runtime-header", type=Path, required=True)
    parser.add_argument("--geometry-reader", type=Path, required=True)
    parser.add_argument("--core-source", type=Path, required=True)
    parser.add_argument("--bridge-source", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    assets = Assets(args.manifest)
    spec = importlib.util.spec_from_file_location("bay_geometry_reader", args.geometry_reader)
    reader = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(reader)
    text = args.runtime_header.read_text(encoding="utf-8")
    runtime = json.loads(re.search(r'R"AC7ANIM\((.*)\)AC7ANIM"', text, re.S).group(1))
    f22_plan = next(p for p in runtime["planes"] if p["code"] == "f22a")
    _, body_map = channels(assets, "f22a_AnimBP")
    _, _, body_bones, _ = skeleton(assets, "f22a_Skeleton")
    _, rack_map = channels(assets, "f22a_SWP3_AnimBP")
    doc, export, rack_bones, _ = skeleton(assets, "f22a_SWP3_Skeleton")
    sp_bones = bone_rows(rack_bones, rack_map, "spwc")
    levels = sorted({abs(b["reference_component"][1]) for b in sp_bones})
    low, high = max(zip(levels, levels[1:]), key=lambda pair: pair[1]-pair[0])
    cut = (low + high) / 2
    if high - low < 10:
        raise ValueError("F22骨骼参考横向分组没有清晰间隔，不能套用此分区")
    lateral = [b for b in sp_bones if abs(b["reference_component"][1]) > cut]
    abdominal = [b for b in sp_bones if abs(b["reference_component"][1]) < cut]
    actual_slots = {}
    for mask in (5, 6, 7):
        plan = f22_plan["plans"][mask]
        provider = plan["providers"][2]
        if provider != 2:
            raise ValueError("当前F22布局不再由SWP3提供8AAM位置")
        actual_slots[str(mask)] = [f22_plan["groups"][provider]["sockets"][i] for i in plan["selected"][2]]
    if any(set(names) != {f"sp{i}" for i in range(31, 39)} for names in actual_slots.values()):
        raise ValueError("当前8AAM选位不再是sp31..38，不能沿用侧部机构结论")
    tg_driver1, _ = channels(assets, "su57tg_SWP1_AnimBP")
    tg_driver2, _ = channels(assets, "su57tg_SWP2_AnimBP")
    tg_sequences = []
    for group, driver in ((1, tg_driver1), (2, tg_driver2)):
        _, _, bones, _ = skeleton(assets, f"su57tg_SWP{group}_Skeleton")
        tracks, _ = animation_tracks(assets.load(driver["sequence"]), bones)
        tg_sequences.append({b["name"]: tracks[i] for i, b in enumerate(bones) if b["name"] != "Body"})
    native_flags = {}
    for name in ("f22a", "su57tg"):
        row = value(assets.load("PartsAnimationConfig_" + name)["Exports"][0]["Table"]["Data"][0])
        native_flags[name] = {k: v for k, v in row.items() if "cover" in k.lower()}
    # 只给目标资源所需的逐骨改表配方；不生成资源，不覆盖原版全局PlaneBodyConfig。
    remaps = [{"bone": b["name"], "from": "spwc", "to": "swcc"} for b in
              bone_rows(body_bones, body_map, "spwc") + abdominal]
    remaps += [{"bone": b["name"], "from": "spwc", "to": "swc3"} for b in lateral]
    if len({r["bone"] for r in remaps}) != len(remaps):
        raise ValueError("F22腹/侧目标骨名意外重叠")
    core = args.core_source.read_text(encoding="utf-8")
    start = core.index("void mountPartsUpdate(")
    end = core.index("// 原生显隐请求", start)
    f35 = core[start:end]
    required = ("selected!=f35Cover.selection && selected!=2", "std::max(before[0],before[1])",
                "index!=selected", "delta/f35Cover.seconds*speed", "std::max(first,second)")
    if any(term not in f35 for term in required):
        raise ValueError("F35对照源码已改变，需要重新审查，不能沿用本摘要")
    bridge = args.bridge_source.read_text(encoding="utf-8")
    switch_start = bridge.index('} else if(command.op=="switch")')
    switch_end = bridge.index('} else if(command.op=="checkpoint_save")', switch_start)
    switch = bridge[switch_start:switch_end]
    armed_at = switch.index("keepArmed(command,armed);") + len("keepArmed(command,armed);")
    absolute_position = switch_start + armed_at
    tg = tg_geometry(assets, reader)
    result = {"steam_build": 9855922, "game_process_access": False, "compiled": False, "tests_run": False,
              "installed": False, "runtime_ready": False,
              "f22a": {"body_abdominal_spwc": bone_rows(body_bones, body_map, "spwc"),
                        "body_native_msl_lateral_misc": bone_rows(body_bones, body_map, "misc"),
                        "swp3_abdominal_spwc": abdominal, "swp3_8aam_lateral_spwc": lateral,
                        "reference_y_partition_gap": [low, high], "current_8aam_slots": actual_slots,
                        "swp3_sockets": sockets(doc, export),
                        "parts_flags": native_flags["f22a"],
                        "conclusion": "Body普通MSL侧门misc不是当前sp31..38所在8AAM侧部机构；SWP3腹侧两组同读spwc，逐组件单一通道覆盖也不能拆开它们",
                        "resource_route": {"clone_table": "PlaneBodyConfig → F22专用表，不修改全局原表",
                                           "rebind_existing_blueprint_clones": ["f22a_AnimBP", "f22a_SWP3_AnimBP"],
                                           "bone_remaps": remaps,
                                           "unchanged": "原Mesh/Skeleton/AnimSequence/Socket选位及所有非目标骨行；misc/gunc原生",
                                           "native_anim_instance_new_variables_required_by_routing": False,
                                           "independent_visual_output_writer_qualified": False,
                                           "reason": "原生表已实际使用swcc/swc3，但改表本身不提供独立域收尾与新写端资格；不把两个现有名字当新机输出写端已经完成"},
                        "unqualified": "未证明可安全写的第二独立视觉输出字段/逐骨节点参数入口；不猜Parts偏移，不改原生发射门控所需public状态"},
              "su57tg": {"first_two_tracks_equal": tg_sequences[0] == tg_sequences[1],
                         "parts_flags": native_flags["su57tg"], "animated_rack_mask": 3,
                         "snapshot_domain_mask_candidate": 7, "static_third": tg,
                         "conclusion": "静态第三组的四个实际点在LOD0共舱盖板上方，不能仅按StaticMesh或LACM名字排除同舱快照；三配置共享Body舱域，第三组仍无骨架租约"},
              "f35_contrast": {"source_fragment_sha256": hashlib.sha256(f35.encode()).hexdigest(),
                               "native_profile": "F35为独立SP1/SP2的6模式；本批F22/TG为公共1模式",
                               "per_frame_repair": "每帧保存三独立通道；切配置时用旧0/1最大进度补公共值，原生只执行一次，非当前通道按原生delta/seconds/speed收拢，再补合成输出",
                               "meaning": "F35不是只靠一次初始化前后快照；公共域候选不能据其before-keepArmed位置成功便推断同样时序安全",
                               "not_proven": "本轮没有精确追到SetSpecialWeaponArmed直接重置公共1模式的最终写指令；不把相关性写成该setter已定因"},
              "post_armed_position": {"operation": "switch", "after": "keepArmed(command,armed); 的完整返回（包含Pressed/Released和实际armed确认）",
                                     "source_line_after_call": bridge[:absolute_position].count("\n") + 1,
                                     "before": "ravenRestoreReload/fixedMountCommit；不是initialize内fixedMountAfterRebuild",
                                     "scope": "同一request/Binding/generation、重建前已armed的同舱SP互切；一次消费；setup/resupply/checkpoint及跨域不套旧快照"},
              "sources": assets.used,
              "inputs": {"runtime_header_sha256": sha(args.runtime_header),
                         "geometry_reader_sha256": sha(args.geometry_reader), "analyzer_sha256": sha(Path(__file__)),
                         "core_source_sha256": sha(args.core_source), "bridge_source_sha256": sha(args.bridge_source)}}
    with args.output.open("w", encoding="utf-8", newline="\n") as stream:
        json.dump(result, stream, ensure_ascii=False, indent=2)
        stream.write("\n")
    print("F22 SWP3腹/侧骨数：", len(abdominal), len(lateral))
    print("TG 4AAM/HCAA轨一致：", result["su57tg"]["first_two_tracks_equal"])
    for lod in result["su57tg"]["static_third"]["lods"]:
        print("TG LOD", lod["lod"], [(s["resource_spwc_key"],
              [(p["socket"], p["rays"][0]["nearest_spwc_cover_below"]) for p in s["sites"]]) for s in lod["samples"]])


if __name__ == "__main__":
    main()
