"""只读汇总CFA切换日志；输出实测样本与分段统计，不启动或操作游戏。"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import statistics


def stats(values):
    ordered = sorted(values)
    return {"count": len(values), "min": min(values), "median": statistics.median(values),
            "p95": ordered[math.ceil(len(ordered) * .95) - 1], "max": max(values)}


def summarize(path, after_tick, profile_revision=None):
    data = path.read_bytes()
    samples = []
    for line_number, line in enumerate(data.decode("utf-8").splitlines(), 1):
        if '"event":"CFA_SWITCH_COST"' not in line:
            continue
        tick, body = line.split(" ", 1)
        if int(tick) < after_tick:
            continue
        row = json.loads(body)
        if profile_revision is not None and row.get('profile_revision') != profile_revision:
            continue
        samples.append({"line": line_number, "tick": int(tick), **row})
    passed = [s for s in samples if s.get("ok")]
    if not passed:
        raise ValueError("所选范围没有成功CFA切换样本")
    stages = sorted({k for s in passed for k in s["stages_ms"]})
    groups = {}
    # 旧日志未记录模型开关/目标；不把未知值猜成模型关闭。
    for sample in passed:
        key = f'revision={sample.get("profile_revision", "未记录")},models={sample.get("models", "未记录")},target={sample.get("target_config", "未记录")}'
        groups.setdefault(key, []).append(sample["total_ms"])
    return {"source": str(path.resolve()), "sha256": hashlib.sha256(data).hexdigest(),
            "bytes": len(data), "after_tick": after_tick, "profile_revision": profile_revision,
            "scope": "核心事务耗时；不含Lua输入/HUD和后续渲染，不是帧时间或FPS",
            "total_ms": stats([s["total_ms"] for s in passed]),
            "stages_ms": {k: stats([s["stages_ms"][k] for s in passed if k in s["stages_ms"]]) for k in stages},
            "groups_ms": {k: stats(v) for k, v in groups.items()}, "samples": samples,
            "warning": "子项可能重叠，不与总耗时相加；首次峰值保留，前后比较须使用相同组合和模型开关"}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("log", type=Path)
    parser.add_argument("--after-tick", type=int, default=0, help="显式选择本次测试起点，避免混入历史版本")
    parser.add_argument("--profile-revision", type=int, help="只汇总指定计时修订，避免历史日志混入新成绩")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    report = summarize(args.log, args.after_tick, args.profile_revision)
    with args.output.open("x", encoding="utf-8") as stream:
        json.dump(report, stream, ensure_ascii=False, indent=2)
        stream.write("\n")
    print(json.dumps({"output": str(args.output), "total_ms": report["total_ms"]}, ensure_ascii=False))


if __name__ == "__main__":
    main()
