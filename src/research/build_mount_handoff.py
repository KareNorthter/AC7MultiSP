"""汇总全部原版机体方案与接入优先级，不构建模组或连接游戏。"""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path


def priority(combo):
    """优先级只描述接入顺序，不能代替运行验证。"""
    disposition = combo['disposition']
    if disposition == 'deferred':
        return 'deferred', ['保留条件方案，不进入本轮接入']
    if disposition == 'native_single':
        return 'native_reference', ['原生单挂参考']
    suggested = str(combo.get('suggested_priority', combo.get('priority','')))
    assignments = combo['assignments']
    if any(x.get('position_role') == 'uav_deployment_reference' for x in assignments) or suggested.startswith('P3'):
        return 'P3', ['部署、返回或生命周期仍有重大前提，暂不列入优先实玩接入']
    if suggested.startswith('P1'):
        return 'P1', ['逐机评审列为优先；仍须满足runtime_prerequisites']
    reasons = []
    if any(x.get('position_role') != 'ordnance_station_candidate' for x in assignments):
        reasons.append('固定炮体、持续效果或发射装置需要对应角色管理')
    if any(x.get('below_window') is True for x in assignments):
        reasons.append('部分物理位置少于原生装填窗口')
    if any(x.get('loaded_window') is None for x in assignments):
        reasons.append('部分原生窗口未解析，不能按固定数量接管')
    explanation = str(combo.get('reason','')) + str(combo.get('notes',''))
    if '窄' in explanation:
        reasons.append('逐机评审记录了较窄的参考净空')
    if suggested.startswith('P2'):
        reasons.append('逐机评审要求额外前置核对')
    return ('P2', reasons) if reasons else ('P1', ['现成资源与普通弹体分区可先接入'])


def aircraft_gate(combos):
    """用户要求整机七组合齐全才接入；部分可复用不放行整机。"""
    blocked = []
    for combo in combos:
        level, _ = priority(combo)
        if level in ('deferred','P3'):
            blocked.append({'weapons':combo['weapons'], 'research_priority':level,
                            'reason':combo.get('deferred_reason') or combo.get('reason') or
                                     ('UAV部署／返回等重大前提未解决' if level=='P3' else '组合依赖尚未解决'),
                            'blocker_kind':combo.get('blocker_kind') or {'requires_rack_asset':'resource_split',
                                                                     'requires_new_station':'new_station'}.get(combo['status'])})
    eligible = len(combos) == 7 and not blocked
    return {'policy':'all_seven_required', 'eligible':eligible,
            'status':'complete_seven_candidate' if eligible else 'defer_whole_aircraft',
            'combination_count':len(combos), 'blocked_combinations':blocked,
            'reason':'七组合均有现成资源方案，可整机接入并逐项验证前提' if eligible
                     else '有组合尚未解决；按用户要求整机暂缓，不做部分组合'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--catalog', type=Path, required=True)
    parser.add_argument('--batch', type=Path, action='append', required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    catalog = json.loads(args.catalog.read_text(encoding='utf-8'))
    names = {a['code']:a['display_name_en'] for a in catalog['aircraft']}
    baselines = {a['code'] for a in catalog['aircraft'] if a['existing_layout_reference']}
    seen, aircraft, candidates, source_files = set(), [], [], []
    for path in args.batch:
        batch = json.loads(path.read_text(encoding='utf-8'))
        source_files.append({'record':'evidence/'+path.name, 'sha256':hashlib.sha256(path.read_bytes()).hexdigest()})
        for sample in batch['aircraft']:
            code = sample['code']
            if code in seen or code in baselines or code not in names:
                raise ValueError('机体重复或不属于目标全集：' + code)
            seen.add(code)
            combos = sample['proposal']['combinations']
            if len(combos) != 7 or len({tuple(sorted(c['weapons'])) for c in combos}) != 7:
                raise ValueError('组合覆盖不完整：' + code)
            gate = aircraft_gate(combos)
            if sample.get('integration_gate',gate) != gate:
                raise ValueError('整机七组合准入与批次记录不一致：' + code)
            overview = []
            for combo in combos:
                if combo['status'] != 'native_reuse' and combo['disposition'] != 'deferred':
                    raise ValueError('未解决的组合没有暂缓：' + code)
                level, reasons = priority(combo)
                row = {'weapons':combo['weapons'], 'research_status':combo['status'], 'disposition':combo['disposition'],
                       'priority':level, 'priority_reasons':reasons,
                       'integration_allowed':gate['eligible'],
                       'blocker_kind':combo.get('blocker_kind') or {'requires_rack_asset':'resource_split',
                                                                   'requires_new_station':'new_station'}.get(combo['status']),
                       'runtime_prerequisites':combo.get('runtime_prerequisites', []),
                       'deferred_reason':combo.get('deferred_reason') if level=='deferred' else None}
                overview.append(row)
                if gate['eligible'] and level in ('P1','P2'):
                    candidates.append({'code':code,'name':names[code], 'record':'evidence/'+path.name, **row,
                                       'assignments':combo['assignments'], 'rack_plan':combo.get('rack_plan'),
                                       'layout_notes':combo.get('reason',combo.get('notes',[]))})
            aircraft.append({'code':code, 'name':names[code], 'record':'evidence/'+path.name,
                             'baseline':False, 'combination_count':7, 'integration_gate':gate, 'combinations':overview})
    if seen | baselines != set(names) or len(names) != 47:
        raise ValueError('研究批次和既有基线未覆盖全部47机')
    for code in sorted(baselines):
        aircraft.append({'code':code,'name':names[code], 'baseline':True, 'combination_count':7,
                         'record':'docs/multi-sp-mount-design.md',
                         'validation':'evidence/2.4.0-user-validation.json',
                         'scope':'沿用正式版三机方案；不表示全部任务与故障边界均已实测'})
    counts = Counter(row['priority'] for a in aircraft if not a['baseline'] for row in a['combinations'])
    eligible = [a for a in aircraft if not a['baseline'] and a['integration_gate']['eligible']]
    eligible_counts = Counter(c['priority'] for c in candidates)
    report = {'schema':1, 'baseline':'v2.4.0', 'steam_build':9855922,
              'scope':'全部47架有武装原版机体，含官方DLC；无武装f104av和第三方机体不在范围内',
              'compiled':False, 'tests_run':False, 'installed':False, 'assets_generated':False, 'installable_package':False,
              'integration_policy':'整机七组合全部可行才接入；有任何暂缓或P3组合则整机后置，不交付部分组合',
              'verification':{'aircraft_count':47,'baseline_aircraft':len(baselines), 'researched_new_aircraft':len(seen),
                              'total_combination_cases':47*7, 'new_combination_cases':len(seen)*7,
                              'eligible_aircraft':len(eligible), 'eligible_combination_cases':len(eligible)*7,
                              'eligible_multi_combinations':len(candidates),
                              'deferred_whole_aircraft':len(seen)-len(eligible),
                              'research_priority_counts':dict(counts), 'integration_priority_counts':dict(eligible_counts)},
              'source_files':source_files,
              'priority_meanings':{'P1':'优先实现模型绑定，再交付实玩；并非可直接安装或已实测',
                                   'P2':'布局候选成立，但先完成固定角色、窗口或窄净空等前提',
                                   'P3':'重大生命周期/部署前提尚未解决，不进入优先实玩',
                                   'deferred':'拆分、新站、动画或支撑等阻塞未解决，保留条件方案',
                                   'native_reference':'原生单挂参考'},
              'eligible_aircraft':[{'code':a['code'],'name':a['name'],'record':a['record'],
                                    'combination_count':7} for a in sorted(eligible,key=lambda a:a['code'])],
              'aircraft':sorted(aircraft,key=lambda a:a['code']),
              'integration_candidates':sorted(candidates,key=lambda c:(c['priority'],c['code'],len(c['weapons']),c['weapons'])),
              'notes':['所有新增布局均为离线研究；机体、真实离架、装填及场景交接仍须接入和实测',
                       '旧逐组合可复用状态只保留研究事实；整机integration_gate.eligible为最终接入门槛',
                       '持续效果可跨SP不等于已登记所有机型/类；本研究不预设TLS与ECM/IEWS互斥',
                       '拆架/拆巢/新增站工作继续后置；不安装或发布任何产物']}
    with args.output.open('x',encoding='utf-8',newline='\n') as stream:
        json.dump(report,stream,ensure_ascii=False,indent=2)
        stream.write('\n')
    print(json.dumps(report['verification'],ensure_ascii=False))


if __name__ == '__main__':
    main()
