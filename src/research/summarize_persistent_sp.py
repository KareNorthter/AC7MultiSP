"""把离线资源名称与历史反汇编整理成精简研究证据，不执行游戏函数。"""

import argparse
import hashlib
import json
import re
from pathlib import Path

# 原生函数范围来自既有指令记录；不把候选入口当作已验证的写入接口。
ROOT = Path(__file__).resolve().parents[2]
FUNCTIONS = (
    ("InitializeWeaponSystem", "自由飞行-native-body-0x81e810.json", 0x81E810, 0x81E9B5, True),
    ("激活器配置初始化候选", "自由飞行-native-body-0x882dc0.json", 0x882E40, 0x882FB0, False),
    ("另一路初始化包装函数", "自由飞行-native-body-0x882dc0.json", 0x882DC0, 0x882E40, True),
    ("另一路分组初始化", "自由飞行-native-body-0x89d980.json", 0x89D980, 0x89DA3D, True),
    ("槽计时与整数状态重置", "自由飞行-native-body-0x89eda0.json", 0x89EDA0, 0x89EEF4, True),
)
# 名称线索只证明资源中包含这些标识，不能证明字段偏移或执行关系。
NAME_INTEREST = re.compile(
    r"^(GetSwpMeshes|ChildSWPMeshes|SWP Meshes|PlayerWeaponActivator|PlayerSpecialWeaponConfig|"
    r"SpecialWeaponConfig[123]|SWP[123](?:_GEN_VARIABLE)?|WeaponMeshBase(?:_GEN_VARIABLE)?|"
    r"RefuelOnMesh|RefuelOffMesh|LoadTime|MaxLoadedCount|MaxWeaponCount|LockonRange|"
    r"Bomb_BaseClass|Missile_BaseClass|Game/Blueprint/Weapons/(?:Bomb_BaseClass|Missile_BaseClass)|"
    r"WeaponParameters|WeaponID|TickClass|"
    r"EWeaponID::WID_[a-z0-9_]+|EPartsEnableGameMode::[A-Za-z]+|"
    r"EPartsParameterList::(?:SpMslAir|SpMslGrd|Msl|Bomb|Laser|EML|MGP|Rkt)[A-Za-z0-9_]+)$"
)


def native_summary():
    """只解析历史记录的相对地址，清楚标记不完整函数体。"""
    # 每个片段的模块基址从其相对指令与历史绝对调用目标换算，结果只保留 RVA。
    report = []
    for name, filename, start, end, complete in FUNCTIONS:
        source = ROOT / "evidence" / "native" / filename
        data = json.loads(source.read_text(encoding="utf-8"))[0]
        instructions = [line for line in data["instructions"] if start <= int(line.split(":", 1)[0], 16) < end]
        instruction_addresses = {int(line.split(":", 1)[0], 16) for line in data["instructions"]}
        base_candidates = set()
        for line in data["instructions"]:
            jump = re.search(r"\bj\w+ (0x[0-9a-f]+)$", line)
            if jump:
                absolute = int(jump.group(1), 16)
                # 只使用确实指向本记录内指令的分支，避免把远跳转距离当作基址。
                for target in instruction_addresses:
                    candidate = absolute - target
                    if candidate > 0 and candidate % 0x10000 == 0:
                        base_candidates.add(candidate)
        if len(base_candidates) != 1:
            raise ValueError(f"无法唯一确认历史模块基址：{filename}")
        module_base = next(iter(base_candidates))
        calls = []
        writes = []
        for line in instructions:
            address = line.split(":", 1)[0]
            direct = re.search(r"\bcall (0x[0-9a-f]+)$", line)
            if direct:
                calls.append({"at_rva": address, "target_rva": hex(int(direct.group(1), 16) - module_base)})
            elif ": call " in line:
                calls.append({"at_rva": address, "indirect": line.split(": ", 1)[1]})
            if re.search(r": mov(?:ss)? (?:dword|qword) ptr \[", line):
                writes.append(line)
        report.append({
            "name": name, "source": "evidence/native/" + filename, "start_rva": hex(start),
            "observed_end_exclusive_rva": hex(end), "complete_function_in_record": complete,
            "source_sha256": hashlib.sha256(source.read_bytes()).hexdigest(),
            "calls": calls, "observed_memory_writes": writes,
        })
    return report


def main():
    """生成精简摘要；不导出完整游戏资源，不连接游戏进程。"""
    # 输出必须是新文件，以免覆盖既有未受 Git 保护的研究结论。
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if args.output.exists():
        raise FileExistsError(f"输出已存在，请指定新文件：{args.output}")
    manifest = json.loads(args.manifest.read_text(encoding="utf-8"))
    assets = []
    for item in manifest:
        # 仅搜索解码副本中的 ASCII 标识；缺少 JSON 时不推断导出对象或默认值。
        source = Path(item["decoded"])
        identifiers = []
        for match in re.finditer(rb"[A-Za-z_][A-Za-z0-9_/. :\-]{3,}", source.read_bytes()):
            value = match.group().decode("ascii")
            if NAME_INTEREST.fullmatch(value):
                identifiers.append({"name": value, "file_offset": hex(match.start())})
        assets.append({
            "pak": item["pak"], "asset": item["asset"],
            "encrypted_sha256": item["encrypted_sha256"], "decoded_sha256": item["decoded_sha256"],
            "evidence_level": "仅离线名称线索，未完成 JSON 结构解析", "identifiers": identifiers,
        })
    report = {
        "date": "2026-10-13", "branch": "codex/persistent-sp-research", "steam_build": 9855922,
        "scope": "自有资源索引及名称线索、历史函数体重新整理；没有实机验证",
        "game_started": False, "game_process_accessed": False, "runtime_modified": False,
        "compiled": False, "tests_run": False,
        "parser_limitation": "UAssetGUI 1.1.0 首个 JSON 导出超时，停止重复调用；无已解析新导出对象",
        "assets": assets, "native_fragments": native_summary(),
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"摘要写入 {args.output}；{len(assets)} 个资源副本，{len(report['native_fragments'])} 个历史函数片段")


if __name__ == "__main__":
    main()
