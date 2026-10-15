"""比较本机已解析的 Su-30SM 挂架几何；只输出范围摘要，不提交原始顶点。"""

import argparse
import base64
import hashlib
import json
import math
import struct
from pathlib import Path


def bounds(points):
    """返回一组顶点的三轴包围盒，空分组明确报错。"""
    if not points:
        raise ValueError("挂架位置分组为空")
    return {"min": [min(point[axis] for point in points) for axis in range(3)],
            "max": [max(point[axis] for point in points) for axis in range(3)]}


def read_mesh(path):
    """按本轮两个资源共同确认的 LOD0 布局读取顶点，并与资源 Bounds 交叉核对。"""
    document = json.loads(path.read_text(encoding="utf-8-sig"))  # UAssetGUI 本地导出。
    export = next(item for item in document["Exports"] if item["ObjectName"] == path.stem)
    raw = base64.b64decode(export["Extras"], validate=True)  # 不写回资产。
    section_count = struct.unpack_from("<I", raw, 64)[0]
    stride, count, element_size, array_count = struct.unpack_from("<4I", raw, 100)
    if section_count != 1 or stride != 12 or element_size != 12 or count != array_count or not 0 < count < 10000:
        raise ValueError("不是已核对的单 section、float3 顶点布局")
    points = list(struct.iter_unpack("<3f", raw[116:116 + count * 12]))
    if len(points) != count or not all(math.isfinite(value) for point in points for value in point):
        raise ValueError("顶点数量或数值无效")
    actual = bounds(points)  # 与序列化包围盒一致才使用该候选位置缓冲。
    extended = next(item["Value"] for item in export["Data"] if item["Name"] == "ExtendedBounds")
    vectors = {item["Name"]: item["Value"][0]["Value"] for item in extended if item["Name"] in ("Origin", "BoxExtent")}
    for axis, name in enumerate(("X", "Y", "Z")):
        center, extent = float(vectors["Origin"][name]), float(vectors["BoxExtent"][name])
        if abs(actual["min"][axis] - (center - extent)) > 0.001 or abs(actual["max"][axis] - (center + extent)) > 0.001:
            raise ValueError("LOD0 顶点范围与资源包围盒不一致")
    groups = {}  # 已核对 Socket 的六处横向带；不宣称这是材质分组或可独立隐藏的 section。
    for side in (-1, 1):
        for band, low, high in (("腹部", 0, 25), ("内翼", 25, 40), ("外翼", 40, 60)):
            selected = [point for point in points if point[1] * side > 0 and low <= abs(point[1]) < high]
            groups[f"Y{'负' if side < 0 else '正'}_{band}"] = {"vertices": len(selected), **bounds(selected)}
    if sum(group["vertices"] for group in groups.values()) != count:
        raise ValueError("存在不属于六处横向带的顶点")
    return {"asset": path.stem, "json_sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
            "lod0_section_count": section_count, "lod0_vertex_count": count, "bounds": actual, "groups": groups}


def main():
    """只读两份本地资产，保存几何摘要及使用边界。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--asset-root", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    meshes = [read_mesh(args.asset_root / f"su30sm_SWP{index}_SM.json") for index in (1, 3)]
    overlap = {}  # 包围盒相交只证明空间范围重叠，不等同于逐三角面交叉检测。
    for name, first in meshes[0]["groups"].items():
        second = meshes[1]["groups"][name]
        overlap[name] = [max(0, min(first["max"][axis], second["max"][axis]) - max(first["min"][axis], second["min"][axis])) for axis in range(3)]
    report = {"steam_build": 9855922, "method": "LOD0 顶点布局与序列化 Bounds 交叉核对", "meshes": meshes,
              "overlap_extent_per_band": overlap,
              "conclusions": ["两套挂架均覆盖六处横向带，不能直接叠加整套。", "LOD0 各仅一个 section，不能按原 section 分别隐藏翼下和腹部结构。"],
              "prototype_choice": "先只显示 SWP3 一套挂架，保留 SWP1/3 父链与原武器 Socket；六个武器模型单独显隐。",
              "boundary": "只核对原版资产的空间范围；武器与共用挂架的贴合、其他LOD和外部PAK替换必须实机检查，尚未通过。"}
    with args.output.open("x", encoding="utf-8") as stream:
        json.dump(report, stream, ensure_ascii=False, indent=2)
        stream.write("\n")
    print(json.dumps({"output": str(args.output), "vertices": [mesh["lod0_vertex_count"] for mesh in meshes]}, ensure_ascii=False))


if __name__ == "__main__":
    main()
