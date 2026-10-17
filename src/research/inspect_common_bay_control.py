"""从既有离线采样整理共舱控制接口依据；不连接进程、不编译或运行测试。"""

import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import sys


ROOT = Path(__file__).resolve().parents[2]


def sha(raw):
    return hashlib.sha256(raw).hexdigest()


def captured(root, directory, rva, size):
    """原始区段先匹配原采集manifest，代码仅在内存中解码，不拷入交付回执。"""
    manifest = root / directory / "manifest.json"
    document = json.loads(manifest.read_text(encoding="utf-8"))
    for record in document["ranges"]:
        start = int(record["rva"], 16)
        if start <= rva and rva + size <= start + record["length"]:
            raw = (manifest.parent / record["file"]).read_bytes()
            if len(raw) != record["length"] or sha(raw) != record["sha256"]:
                raise ValueError("旧采样原件指纹改变：" + directory)
            fragment = raw[rva - start:rva - start + size]
            return fragment, {"capture": directory, "manifest_sha256": sha(manifest.read_bytes()),
                              "range_sha256": sha(raw), "rva": hex(rva), "size": size,
                              "fragment_sha256": sha(fragment)}
    raise ValueError("旧采样未覆盖目标区段")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--capture-root", type=Path, required=True)
    parser.add_argument("--tools-root", type=Path, required=True)
    parser.add_argument("--game-exe", type=Path, default=Path(r"E:/SteamLibrary/steamapps/common/ACE COMBAT 7/Ace7Game.exe"))
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    sys.path.insert(0, str(args.tools_root))
    from capstone import Cs, CS_ARCH_X86, CS_MODE_64

    original = ROOT / "evidence/sp-switch-animation-research.json"
    research = json.loads(original.read_text(encoding="utf-8"))
    header = ROOT / "src/common_bay_animation.hpp"
    native_header = ROOT / "src/native_common_bay_parts.hpp"
    mode_header = ROOT / "src/owned_bone_refresh.hpp"
    disassembler = Cs(CS_ARCH_X86, CS_MODE_64)
    raw, pause = captured(args.capture_root, "f35c-cover-native", 0x7f0210, 0xab)
    decoded = {i.address: (i.mnemonic, i.op_str) for i in disassembler.disasm(raw, 0x7f0210)}
    if decoded.get(0x7f0297) != ("call", "qword ptr [rax + 0x308]") or decoded.get(0x7f029d) != ("or", "dword ptr [rbx + 0x9b0], 2"):
        raise ValueError("旧原生暂停顺序与已有文本不一致")
    pause.update(observed_tick_call_rva="0x7f0297", observed_direct_pause_write_rva="0x7f029d",
                 conclusion="原生调用关闭Tick后仍直接写Pause位；仅拦Tick不能保证骨架继续求值",
                 binding_scope="旧F-35C采样的代码依据，不是新机体组件指针证明")
    raw, getter = captured(args.capture_root, "f35c-cover-native", 0x819600, 8)
    if raw != bytes.fromhex("488d8130140000c3"):
        raise ValueError("旧Body动画输出getter不匹配")
    getter.update(conclusion="旧原生Body动画getter返回this+0x1430；不得将其当成新机型Parts地址")
    raw, parts = captured(args.capture_root, "f35c-cover-state-native", 0x1ce7f57, 0x11c)
    decoded_parts = {i.address: (i.mnemonic, i.op_str) for i in disassembler.disasm(raw, 0x1ce7f57)}
    if decoded_parts.get(0x1ce8013) != ("mov", "ecx, dword ptr [rsi + 0x178]"):
        raise ValueError("原生选择编号分派改变")
    parts.update(conclusion="保留原生四态与公共进度，模块不另造逐帧时钟；恢复不改选择编号")
    signatures = []
    for address, hex_bytes in re.findall(r'originalCodeMatches\((0x[0-9a-f]+),"([0-9a-f]+)"\)', native_header.read_text(encoding="utf-8")):
        start, expected = int(address,16), bytes.fromhex(hex_bytes)
        directory = "f35c-cover-native" if start < 0x900000 else "f35c-cover-state-native"
        actual, proof = captured(args.capture_root,directory,start,len(expected))
        if actual != expected:
            raise ValueError("新Parts访问候选的签名与旧明文不一致：" + address)
        signatures.append(proof)
    if len(signatures) != 9:
        raise ValueError("Parts签名集合不完整")
    raw, owner_binding = captured(args.capture_root,"f35c-cover-state-native",0x1cd3150,0x4f)
    raw, generic_update = captured(args.capture_root,"f35c-cover-state-native",0x1ce93a0,0x150)
    wrapper = {"header": "src/native_common_bay_parts.hpp", "sha256": sha(native_header.read_bytes()),
               "signature_fragments": signatures, "owner_binding": owner_binding, "generic_update": generic_update,
               "chain": "PlayerPlane+1408包装器→包装器+8原生实例→实例+1268回指PlayerPlane+b40→实例+f90 Parts→1ce93a0→1ce6d70(input=Parts+c)",
               "parts_vtable_rva": "0x2fff178",
               "qualification": "完整/Script/Nimbus.PlayerPlane父链和规范机型包，代码/自有跳板原始指纹、包装器/owner/原生vtable、公共盖板flag、配置/四态/范围均核对；不把engine或parts当UObject",
               "not_yet_verified": "没有对四种新机的运行实例调用此候选；组件反射/Tick/隐藏刷新/完整Host仍由产品适配；已有第三方跳转不接受"}
    wrapper["write_failure_contract"] = "预检request/phase/七进度完整写区；每笔写前重新核对owner/generation/instance/vtable/选择编号；失败或回读不符时同身份下尽力恢复写前cover并回读；返回Applied/RejectedBeforeWrite/FailedRolledBack/FailedRollbackIncomplete/IdentityChanged，不把false当未写入"
    cache_text = args.capture_root / "f35-node-update9.txt"
    text = cache_text.read_text(encoding="utf-8")
    markers = ("3f867b movups xmmword ptr [rbx + 0x70], xmm0",
               "3f874b mov eax, dword ptr [rax + rbx + 0x70]",
               "3f874f mov dword ptr [rdi + rcx], eax")
    if not all(marker in text for marker in markers):
        raise ValueError("逐节点缓存文本线索不配套")
    cache = {"record": cache_text.name, "sha256": sha(cache_text.read_bytes()),
             "candidate_entry_rva": "0x3f83b0", "binary_entry_verified_by_this_work": False,
             "finding": "旧反汇编文本显示复制Body输出到节点缓存再填入逐节点浮点数组；不是即时读取公共全局值的简单getter",
             "use": "YF-23/Su-47首版保留原生求值，无需改此缓存或安装此入口钩子",
             "limit": "已有47MB小转储不含所查主映像代码页；本轮不为取得字节而附加游戏"}
    evaluation_text = args.capture_root / "f35-animnode9.txt"
    evaluation = evaluation_text.read_text(encoding="utf-8")
    if not all(x in evaluation for x in ("3f705d addss xmm6, xmm7", "3f7061 mulss xmm6, xmm8", "3f7066 mulss xmm6, dword ptr [r10 + 0x88]")):
        raise ValueError("原生序列时间映射文本不配套")
    steam = args.game_exe.parents[2] / "appmanifest_502500.acf"
    if not re.search(r'"buildid"\s+"9855922"',steam.read_text(encoding="utf-8")):
        raise ValueError("不是当前Steam构建")
    image = args.game_exe.read_bytes()
    pe = struct.unpack_from("<I",image,60)[0]
    count, optional = struct.unpack_from("<H",image,pe+6)[0], struct.unpack_from("<H",image,pe+20)[0]
    constants = {}
    for rva in (0x2574490,0x2574494):
        for index in range(count):
            at=pe+24+optional+index*40
            _, start, size, offset=struct.unpack_from("<IIII",image,at+8)
            if start <= rva and rva+4 <= start+size:
                constants[hex(rva)]=struct.unpack_from("<f",image,offset+rva-start)[0]
                break
    if constants != {"0x2574490":.5,"0x2574494":1.0}:
        raise ValueError("序列时间映射常量不匹配")
    sequence_time = {"text_record": evaluation_text.name, "text_sha256": sha(evaluation_text.read_bytes()),
                     "disk_exe_sha256": sha(image), "constants": constants,
                     "requested_time": "(当前节点缓存的channel_output + 1.0) * 0.5 * 序列对象+0x88处float",
                     "sequence_field_semantics": "SequenceLength为该字段的候选解释；旧采样未保存该序列对象属性元数据/内容，本轮不把它当成新机全开资源帧正式映射证明",
                     "important_boundary": "channel_output是AnimGraph节点读到的Body输出，不能直接把Parts公共progress或0..100资源键编号等同于它",
                     "full_open_candidate": "native request=phase=2且public progress=1，再核对实际provider组件空间Socket与登记的端点姿态、同帧骨骼/世界变换，才显示内部ammo",
                     "mapping_limit": "尚未独立证明Parts progress→Body输出完整转换和所有端点/覆盖序列处理，不将-.01直接换算到资源49.5帧或把.87当Parts安全阈值"}
    codes = ("yf23", "su47", "su57tg", "f22a", "fb22", "pkfa", "zoef")
    by_code = {a["code"]: a for a in research["aircraft"]}
    model_summary = []
    for code in codes:
        sample = by_code[code]
        first = code in ("yf23", "su47", "su57tg", "f22a")
        reasons = {
            "yf23": "控制profile保留，但协调线程最新几何结论为借位撞固定隔框或失去支撑，整机暂缓；不可将白名单当放行",
            "su47": "三组同名spwc轨一致；保留实际Socket provider和唯一呈现架，不按武器mask停掉借位组件",
            "su57tg": "明确混合profile：骨架mask=3、静态mask=4；只有0/1同舱互切恢复快照，出入LACM沿原生流程",
            "f22a": "明确三骨架profile，保留原生曲线；若布局全部借SWP3，则requiredRackMask=4，不要求源SWP1/2曲线与provider一致；完整几何资格另给",
            "fb22": "Body同时含spwc/swcc，公共进度与分通道关系未纳入首版控制模块",
            "pkfa": "Body swcc而SWP1 spwc；离开0/1后分通道收拢/聚合及旧SWP1路由另核对，不能直接沿用TG profile",
            "zoef": "沿既有finishPlayerTLS结束x2后再换槽是较小范围；鼻门swc1和SWP1 spwc需独立源域，不新增持续保有",
        }
        model_summary.append({"code": code, "body_channels": sample["body"]["channels"],
                              "rack_channels": {c["group"]: c["channels"] for c in sample["components"]},
                              "all_same_named_rack_tracks_equal": bool(sample["comparisons"]) and all(c["same_named_local_tracks_equal"] for c in sample["comparisons"]),
                              "header_whitelist": first,
                              "animated_rack_mask": 3 if code == "su57tg" else (7 if code in ("yf23","su47","f22a") else None),
                              "snapshot_domain_mask": 3 if code == "su57tg" else (7 if code in ("yf23","su47","f22a") else None),
                              "reason": reasons[code],
                              "host_adapter_implemented": False, "installable": False})
    report = {
        "schema": 1, "steam_build": 9855922, "basis_commit": "e519d02",
        "previous_research_sha256": sha(original.read_bytes()),
        "header": "src/common_bay_animation.hpp", "header_sha256": sha(header.read_bytes()),
        "analyzer_sha256": sha(Path(__file__).read_bytes()),
        "scope": "YF-23/Su-47/Su-57TG/F-22A限定profile及带代码指纹的原生Parts解析/访问候选；不含2.7产品接入或公共支架几何研究",
        "game_process_access": False, "compiled": False, "tests_run": False, "installed": False,
        "models": model_summary,
        "native_evidence": {"pause_helper": pause, "body_output_getter": getter, "parts_control": parts,
                            "node_cache_text_only": cache, "parts_wrapper_candidate": wrapper,
                            "sequence_requested_time": sequence_time},
        "implementation": {
            "equipped_masks": [1,2,3,4,5,6,7],
            "snapshot": "同步换槽前保存请求/phase/七进度；相同Binding+generation+一次性事务号；重建后keepArmed前恢复，回读但不写选择编号",
            "binding": "Host核对规范Pawn包、派生SkeletalMeshComponent、三组网格/Skeleton/AnimClass及原父链，再读取全部原生基线；全预检后才写",
            "leases": "独立接收equipped与requiredRackMask；只对(provider|presenter)&animatedMask记录暂停/Tick租约，不改网格、Skeleton、AnimClass、父链或bNoSkeletonUpdate；两种原生请求分别记账",
            "provider_example": "YF23的HVAA+4AAM可为equipped=3，HVAA借SWP3点、4AAM用SWP2、SWP2呈现架，则requiredRackMask=6；不伪造选择第三SP。此为布局线程接口例子，几何由该线程独立核对",
            "mixed_profile": "TG静态第三组仍校验身份/父链/资源，但Skeleton和AnimClass必须留空，不调用readRack/applyRack；captureSwitch出入配置2返回正常NativeTransition",
            "hidden_provider": "owned_bone_refresh.hpp提供具名反射模式租约：prepare全预检、acquire/maintain、原生请求观察、逐组件归还。只管required骨架；两候选字段必须恰有一个有效，枚举名称/值/宽度/所属Engine类型按实际元数据核对，不猜偏移或数字，不碰bNoSkeletonUpdate",
            "pose_mode_header": "src/owned_bone_refresh.hpp",
            "pose_mode_header_sha256": sha(mode_header.read_bytes()),
            "restore": "恢复最新原生请求；失败逐组件保留待重试，不阻断其他组件；失效UObject丢弃；未恢复完拒绝新绑定",
            "pose": "使用完整原生Parts更新及原生AnimGraph，不新增节点缓存写入；显示与发射均在动画和世界变换完成后取同一Socket",
        },
        "remaining_host_work": [
            "把native_common_bay_parts的Access接到产品安全内存接口、规范PlayerPlane继承检查及自有钩子原始字节记录；再核对新机实例及同步调度，不能省略候选已给出的九处签名/owner检查",
            "在现有模型绑定层按反射父链接受SkeletalMeshComponent派生类，绑定正确的资源/原生池Socket；不直接替换StaticMesh固定偏移",
            "接入原生Tick请求观察及Pause直接写入后的观察；本模块自己的写入用重入标记绕过；读取实际强制状态不能覆盖最新原生意图",
            "绑定/重建后立即维持活动组件，定时巡检仅兜底；处理显示隐藏时的原生动画更新策略，不能强行清除全局隐藏",
            "几何线程证明唯一公共门体/支撑呈现及全部七组合扫掠后，才能置allSevenLayoutQualified；每个组合另给planEquippedMask/providerMask/presentationMask",
            "实现owned_bone_refresh的元数据/标量Access与排他字段所有权；实际枚举/字段不可得或原生请求无法跟踪时完整回退，不能仅以Tick=true填资格通过",
            "关闭、失败、页面退出、补给/检查点、换世界和组件重建先release，RestorePending期间不接新绑定",
        ],
        "runtime_admission": "未放行；Host尚无产品适配器，新机体运行身份/公共架及原生流程仍未验证",
    }
    with args.output.open("x", encoding="utf-8", newline="\n") as stream:
        json.dump(report, stream, ensure_ascii=False, indent=2)
        stream.write("\n")
    print(json.dumps({"control_profiles": [m["code"] for m in model_summary if m["header_whitelist"]],
                      "header": report["header"], "installable": False}, ensure_ascii=False))


if __name__ == "__main__":
    main()
