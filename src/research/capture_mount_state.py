"""只读采集暂停中的指定机体挂载、原生槽及对象引用，不调用游戏函数。"""

import argparse
import json
import struct
import sys
from datetime import datetime
from pathlib import Path


def main():
    """校验构建、机体身份和数组边界后，保存本次进程的研究快照。"""
    # 工具、进程与输出由调用方指定；输出只能是新文件。
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("pid", type=int)
    parser.add_argument("--project-root", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--expected-sp", choices=("6AAM", "LAGM", "QAAM"))
    parser.add_argument("--expected-plane", choices=("su30sm", "fa27", "f35c", "f15e"), default="su30sm")  # 新机体先核对真实父类，不复用旧玩家地址。
    args = parser.parse_args()
    # 完整运行快照只能进入本研究工作区的忽略目录。
    temporary_root = Path(__file__).resolve().parents[2] / "Temp"
    if not args.output.resolve().is_relative_to(temporary_root.resolve()):
        raise ValueError("完整快照必须保存在本工作区 Temp 内")
    sys.path[:0] = [str(args.project_root / "tools"), str(args.project_root / "src")]
    from inspect_native import GameMemory

    # 既有接口仅申请 PROCESS_QUERY_INFORMATION 与 PROCESS_VM_READ。
    memory = GameMemory(args.pid)
    try:
        def unpack(address, fmt):
            """完整读取一个已知结构，读取失败即停止。"""
            return struct.unpack(fmt, memory.read(address, struct.calcsize(fmt)))

        def pointer(address):
            """读取当前进程指针。"""
            return unpack(address, "<Q")[0]

        # 与正式核心相同的原生 getter 签名，拒绝未知构建。
        signature = bytes.fromhex("488b8160020000488b88f00000008b8118010000c3")
        if memory.read(memory.base + 0x882390, len(signature)) != signature:
            raise ValueError("游戏原生签名不匹配")
        names = pointer(memory.base + 0x3bc7070)
        array, maximum, count = unpack(memory.base + 0x3bcfdb0, "<Qii")
        if not 0 < count <= maximum < 3000000:
            raise ValueError("UObject 数组边界无效")
        items = memory.read(array, count * 24)
        known = {struct.unpack_from("<Q", items, index * 24)[0]: index for index in range(count)}
        known.pop(0, None)
        cache = {}  # 名称只在本次进程内缓存。

        def fname(index):
            """按当前构建名称表解码，不接受超出范围的索引。"""
            if not 0 <= index < 4000000:
                raise ValueError("FName 索引无效")
            if index not in cache:
                entry = pointer(pointer(names + index // 0x4000 * 8) + index % 0x4000 * 8)
                raw = memory.read(entry, 256)
                if raw[0] & 1:
                    end = next(i for i in range(16, len(raw), 2) if raw[i:i+2] == bytes(2))
                    cache[index] = raw[16:end].decode("utf-16le")
                else:
                    cache[index] = raw[16:].split(b"\0")[0].decode("utf-8")
            return cache[index]

        def describe(address):
            """仅解析已在 UObject 表中的对象；非 UObject 明确保留为原始地址。"""
            if address not in known:
                return {"address": hex(address), "uobject": False}
            table, flags, index, cls, name, number, outer = unpack(address, "<QIiQIIQ")
            if index != known[address]:
                raise ValueError("采样期间 UObject 身份发生变化，请保持游戏暂停")
            return {"address": hex(address), "uobject": True, "index": index,
                "serial": struct.unpack_from("<i", items, index * 24 + 16)[0],
                "name": fname(name), "number": number, "class": hex(cls),
                "class_name": fname(unpack(cls + 0x18, "<I")[0]), "outer": hex(outer),
                "flags": flags, "vtable_rva": hex(table - memory.base)}

        def fields(address):
            """通过反射读对象引用、数组和变换，不把候选偏移当作已证实字段。"""
            result = {}
            animation_instance = 'AnimBP' in describe(address).get('class_name', '')  # 动画实例完整记录标量，避免逐字段重复读身份。
            cls = pointer(address + 0x10)
            seen = set()  # 拒绝循环或异常反射链。
            while cls:
                if cls in seen:
                    raise ValueError("反射继承链循环")
                seen.add(cls)
                child = pointer(cls + 0x38)
                while child:
                    if child in seen:
                        raise ValueError("反射字段链循环")
                    seen.add(child)
                    prop = describe(child)
                    kind = prop.get("class_name", "")
                    label = prop.get("name", "")
                    if kind.endswith("Property"):
                        offset = unpack(child + 0x44, "<I")[0]
                        if offset > 0x10000:
                            raise ValueError("反射字段偏移异常")
                        if address == plane:
                            report.setdefault('plane_field_layout', []).append({'name': label, 'kind': kind, 'offset': hex(offset),
                                'element_size': unpack(child + 0x34, '<I')[0]})  # 只记录布局，用于核对动画配置来源。
                        if kind in ('IntProperty', 'ByteProperty', 'FloatProperty') and (
                                animation_instance or offset == 0xb4c
                                or any(term in label.lower() for term in ('weapon', 'cover', 'spw', 'anim'))):
                            fmt = '<f' if kind == 'FloatProperty' else '<B' if kind == 'ByteProperty' else '<i'  # 按反射类型读取标量。
                            result[label] = {'offset': hex(offset), 'scalar': unpack(address + offset, fmt)[0]}
                        if kind in ("ObjectProperty", "ClassProperty"):
                            result[label] = {"offset": hex(offset), "value": describe(pointer(address + offset))}
                        elif kind == "ArrayProperty" and label in (
                            "SpawnedWeapons", "ActiveWeapons", "AttachChildren"):
                            data, length, capacity = unpack(address + offset, "<Qii")
                            if not 0 <= length <= capacity <= 4096:
                                raise ValueError(f"{label} 数组边界异常")
                            result[label] = {"offset": hex(offset), "data": hex(data), "count": length,
                                "capacity": capacity, "raw": memory.read(data, length * 8).hex() if length else ""}
                        elif kind == "NameProperty" and label in ("AttachSocketName", "SocketName"):
                            name, number = unpack(address + offset, "<II")
                            result[label] = {"offset": hex(offset), "name": fname(name), "number": number}
                        elif kind == "StructProperty" and label in ("RelativeLocation", "RelativeRotation", "RelativeScale3D"):
                            result[label] = {"offset": hex(offset), "xyz": unpack(address + offset, "<fff")}
                        elif kind == "BoolProperty" and (animation_instance
                                or label in ("bVisible", "bHiddenInGame", "bHidden") or 'cover' in label.lower()):
                            # 位掩码语义留待核对，先记录值和属性元数据，不直接猜布尔值。
                            result[label] = {"offset": hex(offset), "raw": memory.read(address + offset, 8).hex(),
                                "property_tail": memory.read(child + 0x70, 8).hex()}
                    child = pointer(child + 0x28)
                cls = pointer(cls + 0x30)
            return result

        def native_methods(address):
            """记录挂载相关反射函数入口，仅读取地址，不执行函数。"""
            methods = {}  # 同名子类方法优先保留，不覆盖为父类入口。
            cls = pointer(address + 0x10)
            visited = set()  # 拒绝损坏或采样期间变化的反射链。
            while cls:
                if cls in visited:
                    raise ValueError("函数反射继承链循环")
                visited.add(cls)
                child = pointer(cls + 0x38)
                while child:
                    if child in visited:
                        raise ValueError("函数反射字段链循环")
                    visited.add(child)
                    item = describe(child)
                    label = item.get("name", "")
                    if item.get("class_name") == "Function" and label not in methods and any(
                        term in label.lower() for term in ("weapon", "reload", "fire", "shot", "socket", "visib", "hidden", "transform", "cover", "flightengine")):
                        entry = pointer(child + 0xb0)  # 当前构建已用过的 UFunction 原生入口字段。
                        methods[label] = {"declaring_class": describe(cls).get("name"),
                                          "function": describe(child), "entry": hex(entry),
                                          "entry_minus_image_base": hex(entry - memory.base)}
                    child = pointer(child + 0x28)
                cls = pointer(cls + 0x30)
            return methods

        # 从当前模组命令取玩家线索，再核验 UObject 身份与真实归属；不写命令文件。
        mod = Path(r"E:\SteamLibrary\steamapps\common\ACE COMBAT 7\ue4ss\Mods\DualSP")
        command = (mod / "native-command.txt").read_text(encoding="utf-8").split()
        plane, activator = int(command[1], 16), int(command[2], 16)
        identity = describe(plane)
        cls = pointer(plane + 0x10)  # 沿实际对象父类确认原版机体，允许原生涂装派生类。
        matched = False  # 只有类名及所属资源完整匹配才接受采样。
        for depth in range(32):
            if not cls:
                break
            row = describe(cls)
            if row.get('name') == 'AcePlayerPawn_' + args.expected_plane + '_C':
                matched = describe(pointer(cls + 0x20)).get('name') == '/Game/Blueprint/Player/Pawn/AcePlayerPawn_' + args.expected_plane
                break
            cls = pointer(cls + 0x30)
        if not matched or pointer(plane + 0x11a8) != activator:
            raise ValueError('当前模组玩家线索不是有效 ' + args.expected_plane + '，请进入任务并暂停')
        if args.expected_sp and args.expected_plane != 'su30sm':
            raise ValueError('--expected-sp 目前仅登记 Su-30SM 的实际武器类')
        owner = pointer(activator + 0x260)
        state = pointer(owner + 0xf0)
        report = {"captured_local": datetime.now().astimezone().isoformat(), "pid": args.pid,
            "process_write": False, "game_functions_called": False, "base": hex(memory.base),
            "plane": identity, "activator": describe(activator), "owner": describe(owner),
            "state": hex(state), "command": command, "groups": {}, "objects": {}}
        if args.expected_plane == 'f35c':
            # 舱门通道偏移已由 .8 原生更新函数核对，原件只保存在 Temp。
            engine = pointer(plane + 0x1410)
            parts = memory.read(engine + 0xf90, 0x2a0)
            report['f35_parts'] = {'engine': hex(engine), 'raw': parts.hex(),
                'selection': struct.unpack_from('<i', parts, 0x178)[0],
                'channels': {hex(offset): struct.unpack_from('<f', parts, offset)[0]
                    for offset in (0x17c, 0x180, 0x184, 0x188, 0x18c, 0x190, 0x194)},
                'sp_request': struct.unpack_from('<i', parts, 0x58)[0]}
        queued = {plane, activator, owner}  # 后续仅展开挂载相关对象，不扫描全世界对象内容。
        # 本轮原生 0x89e7a0 与 0x880720 指向的显示管理对象，先验证 UObject 身份。
        mesh_manager = pointer(owner + 0x150)
        if mesh_manager not in known:
            raise ValueError("显示管理对象不在当前 UObject 表中")
        queued.add(mesh_manager)
        report["mesh_manager"] = describe(mesh_manager)
        report["mesh_arrays"] = {}
        for label, offset in (("MSL", 0x140), ("SWP1", 0x150), ("SWP2", 0x160), ("SWP3", 0x170)):
            # 原生 0x8a0400/0x8a0480 分别将组件写入这四个数组。
            data, length, capacity = unpack(mesh_manager + offset, "<Qii")
            if not 0 <= length <= capacity <= 128:
                raise ValueError("显示组件数组边界无效")
            entries = [pointer(data + index * 8) for index in range(length)]
            if any(address not in known for address in entries):
                raise ValueError("显示数组存在非 UObject 引用")
            report["mesh_arrays"][label] = [describe(address) for address in entries]
            queued.update(entries)
        # 0x8a0960 实际读取的分组路由；与 +0x140 起的持久模型池分开保存。
        route_data, route_count, route_capacity = unpack(mesh_manager + 0xf0, "<Qii")
        if not 0 <= route_count <= route_capacity <= 16:
            raise ValueError("显示路由分组边界无效")
        report["launch_mesh_routes"] = []
        for group_index in range(route_count):
            entries_data, entries_count, entries_capacity = unpack(route_data + group_index * 16, "<Qii")
            if not 0 <= entries_count <= entries_capacity <= 4096:
                raise ValueError("显示路由元素边界无效")
            entries = []  # 每项为组件、显示状态、备用父组件及 Socket 的 0x20 字节结构。
            for entry_index in range(entries_count):
                raw = memory.read(entries_data + entry_index * 0x20, 0x20)
                component, visible, parent, socket_index, socket_number = struct.unpack("<QB7xQII", raw)
                entries.append({"index": entry_index, "component": describe(component),
                    "logical_visible": visible, "parent_without_mesh": describe(parent),
                    "socket_without_mesh": fname(socket_index), "socket_number": socket_number})
                if component:
                    if component not in known:
                        raise ValueError("显示路由引用不是有效 UObject")
                    queued.add(component)
            report["launch_mesh_routes"].append({"group": group_index, "count": entries_count,
                "entries": entries})
        # 0x8a12f0 消费的延迟显示队列：保存组号和槽号，不擅自解释为物理挂点。
        timer_data, timer_count, timer_capacity = unpack(mesh_manager + 0x110, "<Qii")
        if not 0 <= timer_count <= timer_capacity <= 4096:
            raise ValueError("延迟显示队列边界无效")
        report["mesh_display_timers"] = []
        for timer_index in range(timer_count):
            group_id, slot_id, remaining, animation, consumed = unpack(
                timer_data + timer_index * 16, "<B3xifBB2x")
            report["mesh_display_timers"].append({"group": group_id, "slot": slot_id,
                "remaining": remaining, "animation": animation, "consumed": consumed})
        for label, offset in (("MSL", 0x88), ("SP", 0x110)):
            group = state + offset
            data, length, capacity = unpack(group + 0x58, "<Qii")
            ammo, max_ammo, window, cursor = unpack(group + 8, "<iiii")
            if not 0 < length <= capacity <= 4096 or not 0 <= window <= length or not 0 <= cursor < length:
                raise ValueError("原生槽数组或游标无效")
            slots = []
            for index in range(length):
                raw = memory.read(data + index * 0x60, 0x60)
                refs = {}  # 只把匹配当前 UObject 表的八字节值称为对象引用候选。
                weak_refs = {}  # 以索引和序列号共同匹配的弱引用候选，仍不假设槽字段语义。
                for field in range(0, 0x60, 8):
                    candidate = struct.unpack_from("<Q", raw, field)[0]
                    if candidate in known:
                        refs[hex(field)] = describe(candidate)
                        queued.add(candidate)
                    object_index, object_serial = struct.unpack_from("<ii", raw, field)
                    if 0 <= object_index < count and object_serial > 0:
                        observed_serial = struct.unpack_from("<i", items, object_index * 24 + 16)[0]
                        if observed_serial == object_serial:
                            candidate = struct.unpack_from("<Q", items, object_index * 24)[0]
                            if candidate in known:
                                weak_refs[hex(field)] = describe(candidate)
                                queued.add(candidate)
                slots.append({"index": index, "raw": raw.hex(), "object_candidates": refs,
                    "weak_object_candidates": weak_refs,
                    "socket_name_candidate": fname(struct.unpack_from("<I", raw, 8)[0]),
                    "transform_candidate": {"rotation_xyzw": struct.unpack_from("<4f", raw, 0x10),
                        "translation": struct.unpack_from("<3f", raw, 0x20),
                        "scale": struct.unpack_from("<3f", raw, 0x30)},
                    "index_candidates_at_0x40_0x44": struct.unpack_from("<2i", raw, 0x40),
                    "duration": struct.unpack_from("<f", raw, 0x48)[0],
                    "elapsed": struct.unpack_from("<f", raw, 0x4c)[0]})
            report["groups"][label] = {"address": hex(group), "class": describe(pointer(group)),
                "ammo": ammo, "max_ammo": max_ammo, "window": window, "cursor": cursor,
                "count": length, "capacity": capacity, "raw": memory.read(group, 0x88).hex(), "slots": slots}
            # 0x8997d6 与 0x89ce61 读取的整数索引表，不能当成当前可用弹数。
            indices_data, indices_count, indices_capacity = unpack(group + 0x68, "<Qii")
            if not 0 <= indices_count <= indices_capacity <= 4096:
                raise ValueError("发射索引表边界无效")
            report["groups"][label]["launch_index_table"] = list(unpack(indices_data, "<" + "i" * indices_count)) if indices_count else []
        if args.expected_sp:
            expected_class = {"6AAM": "plwp_6aam_r0_C", "LAGM": "plwp_lagm_r0_C", "QAAM": "plwp_qaam_r0_C"}[args.expected_sp]
            if report["groups"]["SP"]["class"]["name"] != expected_class:
                raise ValueError(f"当前活动 SP 不是 {args.expected_sp}，实际为 {report['groups']['SP']['class']['name']}")
        for depth in range(3):
            pending = queued - {int(key, 16) for key in report["objects"]}
            for address in pending:
                if address not in known:
                    continue
                row = describe(address)
                row["fields"] = fields(address)
                if row["class_name"] == "StaticMeshComponent":
                    # 此偏移由本轮 0x8a0960 的三次向量读取直接定位，不以通用 UE 布局猜测。
                    row["launch_reader_world_transform"] = {"rotation_xyzw": unpack(address + 0x180, "<4f"),
                        "translation": unpack(address + 0x190, "<3f"), "scale": unpack(address + 0x1a0, "<3f")}
                report["objects"][hex(address)] = row
                for label, entry in row["fields"].items():
                    if label in ("SWP1", "SWP2", "SWP3", "ActiveSWPMeshComponent", "WeaponMeshBase",
                                 "StaticMesh", "RootComponent", "AttachParent", "HiddenMissileMesh", "AnimScriptInstance"):
                        value = entry.get("value", {})
                        if value.get("uobject"):
                            queued.add(int(value["address"], 16))
                    if label in ("SpawnedWeapons", "ActiveWeapons", "AttachChildren") and "raw" in entry:
                        raw = bytes.fromhex(entry["raw"])
                        queued.update(value[0] for value in struct.iter_unpack("<Q", raw) if value[0] in known)
        # 每个相关类只取一次函数表，减少重复数据；保留挂架组件的虚表用于定位原生覆盖。
        report["native_methods_by_class"] = {}
        for address_text, row in report["objects"].items():
            cls_name = row["class_name"]
            if cls_name not in report["native_methods_by_class"] and (
                int(address_text, 16) in (plane, activator, owner, mesh_manager)
                or row["name"].startswith(("SWP", "WeapMesh"))):
                report["native_methods_by_class"][cls_name] = native_methods(int(address_text, 16))
                table = pointer(int(address_text, 16))  # 地址只作为候选，后续按 PE 节范围核验。
                row["vtable_candidates"] = [hex(value[0] - memory.base)
                    for value in struct.iter_unpack("<Q", memory.read(table, 0x600))]
        # 二次核对玩家身份，防止加载/退关时把两个时刻的数据拼成一个快照。
        if describe(plane) != identity or pointer(plane + 0x11a8) != activator:
            raise ValueError("采样期间玩家身份变化")
        with args.output.open("x", encoding="utf-8") as stream:
            json.dump(report, stream, ensure_ascii=False, indent=2)
            stream.write("\n")
        print(json.dumps({"output": str(args.output), "plane": identity["class_name"],
            "objects": len(report["objects"]), "SP": {key: report["groups"]["SP"][key]
                for key in ("ammo", "window", "cursor", "count")}}, ensure_ascii=False))
    finally:
        memory.close()


if __name__ == "__main__":
    main()
