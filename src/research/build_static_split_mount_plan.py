"""将已核对的静态分区配方落实到四机七组合；只生成方案数据，不生成游戏资产。"""
import argparse
from copy import deepcopy
import hashlib
import json
from pathlib import Path

from build_mount_handoff import aircraft_gate, validate_split_evidence

ROOT = Path(__file__).resolve().parents[2]
RECEIPT = 'evidence/sp-combination-static-split-experiment.json'
RECIPES = 'src/research/static_mount_split_recipes.json'
UPSTREAM_COMMIT = '7ae97b6'
UPSTREAM_THREAD = '01a14764-4328-7923-a865-22b7efba4d8e'

# 每项列出可见几何载体；未列组恢复原网格后仅隐藏架自身，保留Socket与子组件。
LAYOUTS = {
    'f16c': {
        ('4AAM','GPB'): {'SWP1':'MSP_f16c_missile_outer','SWP2':'native'},
        ('4AAM','SASM'): {'SWP1':'native'},
        ('GPB','SASM'): {'SWP2':'native','SWP3':'MSP_f16c_sasm_outer'},
        ('4AAM','GPB','SASM'): {'SWP1':'MSP_f16c_missile_outer','SWP2':'native'},
    },
    'f02a': {
        ('LASM','HVAA'): {'SWP1':'MSP_f02a_lasm_inner','SWP2':'MSP_f02a_hvaa_outer'},
        ('LASM','RKT'): {'SWP1':'MSP_f02a_lasm_inner','SWP3':'MSP_f02a_rocket_outer'},
        ('HVAA','RKT'): {'SWP2':'MSP_f02a_hvaa_inner','SWP3':'MSP_f02a_rocket_outer'},
        ('LASM','HVAA','RKT'): {'SWP1':'MSP_f02a_lasm_inner_neg','SWP2':'MSP_f02a_hvaa_inner_pos',
                             'SWP3':'MSP_f02a_rocket_outer'},
    },
    'mr2k': {
        ('GPB','4AAM'): {'SWP1':'native','SWP2':'MSP_mr2k_aam_rear'},
        ('GPB','LASM'): {'SWP1':'MSP_mr2k_gpb_neg_aux','SWP3':'MSP_mr2k_lasm_pos'},
        ('4AAM','LASM'): {'SWP2':'native','SWP3':'native'},
        ('GPB','4AAM','LASM'): {'SWP1':'MSP_mr2k_gpb_neg_aux','SWP2':'MSP_mr2k_aam_rear',
                             'SWP3':'MSP_mr2k_lasm_pos'},
    },
    'rflm': {
        ('HCAA','LACM'): {'SWP1':'MSP_rflm_hcaa_belly','SWP2':'native'},
        ('HCAA','LAAM'): {'SWP1':'MSP_rflm_hcaa_outer','SWP3':'MSP_rflm_laam_belly'},
        ('LACM','LAAM'): {'SWP2':'native','SWP3':'MSP_rflm_laam_belly'},
        ('HCAA','LACM','LAAM'): {'SWP1':'MSP_rflm_hcaa_belly_neg','SWP2':'native',
                              'SWP3':'MSP_rflm_laam_belly_pos'},
    },
}
LAYOUTS = {code:{frozenset(k):v for k,v in plans.items()} for code,plans in LAYOUTS.items()}

PREREQUISITES = [
    '逐机确认原生运行池、逻辑槽身份与真实轮转；保持库存、装填窗口、锁定、齐射、冷却及飞行参数',
    '按独立包路径在游戏线程预加载，核对StaticMesh身份并保留引擎管理引用；本轮未核对本机加载接口',
    '仅在绑定或组合边界换网格；副本只用于原所属SWP，保留父链、Socket与子弹体，不跨组换元数据',
    '隐藏挂架自身不递归隐藏子组件；真实离架与弹体显隐共用映射，取动画和世界变换更新后的Socket',
    '退化面未处理BodySetup、NavCollision或烘焙数据；确认外观组为NoCollision且无相关烘焙依赖，否则停止接入',
    '改单挂、关闭模型、换机、重建、补给、检查点、失败及退出时归还原网格与最近原生显隐，再释放引用',
    '整机七组合一起交付；验证导轨贴合、下落/点火、低库存、补装空位、LOD/阴影/材质及场景交接',
]
CONCLUSIONS = {
    'f16c':[
        '4AAM与SASM共用原SWP1四导轨；含GPB时用已完成全部LOD及编码往返的外翼副本配原SWP2内翼架。',
        '三挂固定4AAM负Y外侧、GPB内侧双位、SASM正Y外侧；SWP3保留原网格和sp3x，仅隐藏架自身。',
        '外侧与内侧横向距离约9.454；静态闭合弹体间隙不能替代下落、点火和真实导轨贴合观察。',
    ],
    'f02a':[
        '四巢共76原生管口，每巢19；多挂保留外翼两巢38口，窗口仍为8，不据资源管口数量确定运行逻辑长度。',
        '已核对七种全部LOD分区副本；LASM+HVAA双挂按用户要求大LASM内侧、小HVAA外侧；rocket_outer同时保留两种材质的巢体及支架。',
        '三挂内翼负Y为LASM、正Y为HVAA，各一处少于两窗口；显式排除内翼火箭口，固定巢体不按单发耗尽隐藏。',
        '旧双挂约1.367的静态间隙不适用于反向内外侧布局；新选位及HVAA原生下落须重新观察，不能继承旧净空结论。',
    ],
    'mr2k':[
        'GPB前腹正Y固定结构用途未明，已核对的gpb_neg_aux仍保留该结构；GPB共存时4AAM用后腹sp23/24。',
        '三挂为负Y翼下GPB一处、后腹4AAM两处、正Y翼下LASM一处，使用各原组副本，不跨组借用导轨。',
        '三种分区副本全部LOD与编码往返成立；4AAM+LASM仍用两组完整原架，七组合载体齐全。',
    ],
    'rflm':[
        '三套整架都占翼下区域；已核对五种全部LOD分区，保留各弹种自身翼下或腹侧导轨。',
        '三挂为负Y腹侧HCAA一处、原翼下LACM一对、正Y腹侧LAAM一处；HCAA/LAAM减位不改变窗口或队列。',
        '几何分区与编码往返不证明导轨贴合、碰撞、阴影或完整离架动作；七组合一起接入与实测。',
    ],
}


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def build(source):
    """保留已有资源与精确选位，只给有逐副本依据的四机补上载体表。"""
    result = deepcopy(source)
    receipt = json.loads((ROOT/RECEIPT).read_text(encoding='utf-8'))
    recipes = json.loads((ROOT/RECIPES).read_text(encoding='utf-8'))
    if receipt['recipes_sha256'] != digest(ROOT/RECIPES):
        raise ValueError('配方与上游实验回执不匹配')
    for name, expected in receipt['executed_tool_sources'].items():
        if digest(ROOT/name) != expected:
            raise ValueError('导入的实验工具与回执指纹不符：'+name)
    proof_map = {v['recipe']['name']:v for v in receipt['variants']}
    recipe_map = {v['name']:v for v in recipes['variants']}
    proof_reference = {'upstream_thread':UPSTREAM_THREAD, 'upstream_commit':UPSTREAM_COMMIT,
                       'receipt':RECEIPT, 'receipt_sha256':digest(ROOT/RECEIPT),
                       'recipes':RECIPES, 'recipes_sha256':digest(ROOT/RECIPES)}
    if receipt.get('local_revision'):
        proof_reference['local_revision'] = receipt['local_revision']
    found = set()
    for sample in result['aircraft']:
        code = sample['code']
        if code not in LAYOUTS:
            continue
        found.add(code)
        native = {g['group']:g for g in sample['groups']}
        plans = LAYOUTS[code]
        sample['proposal']['conclusions'] = list(CONCLUSIONS[code])
        sample['proposal']['unknowns'] = [
            '运行逻辑队列、模型池、原始槽身份及轮转顺序未现场采样，资源Socket顺序不能直接当发射顺序',
            '原生省略变换与C++父类默认值不补猜测，保持原生行为',
            '副本加载接口、引擎引用、真实碰撞与烘焙依赖仍待接入核对',
            '材质/涂装、LOD/阴影、真实离架/下落/展开/补装、低库存及场景交接未实测',
            '第三方PAK覆盖不在原版离线依据中；关闭/失败/改单挂必须完整归还',
        ]
        for combo in sample['proposal']['combinations']:
            for assignment in combo['assignments']:
                group = native[assignment['group']]
                window = group['weapon']['defaults'].get('MaxLoadedCount',{}).get('value')
                assignment['loaded_window'] = window
                assignment['below_window'] = len(assignment['sockets']) < window if window is not None else None
                if not set(assignment['sockets']) <= {s['name'] for s in group['sockets']}:
                    raise ValueError('原生选位不存在：'+code)
            if len(combo['weapons']) == 1:
                combo['rack_plan'] = {'selection_policy':'delegate_native_after_restore', 'carriers':[],
                                      'description':'恢复三组原网格与原生显隐，再由单挂原流程控制'}
                continue
            visible = plans[frozenset(combo['weapons'])]
            carriers, used_variants = [], []
            for name, group in native.items():
                if group['component_class'] != 'StaticMeshComponent':
                    raise ValueError('静态载体不支持活动骨架：'+code+'/'+name)
                chosen = visible.get(name)
                carrier = {'group':name, 'original_mesh':group['rack_mesh'],
                           'mode':'hidden_self' if chosen is None else 'native',
                           'socket_metadata_policy':'保留本组原生Socket与父链；同组副本元数据已原样保持',
                           'hide_children':False, 'restore_native_mesh_required':True}
                if chosen not in (None,'native'):
                    recipe, proof = recipe_map[chosen], proof_map[chosen]
                    if recipe['aircraft'] != code or recipe['source_asset'] != group['rack_mesh']:
                        raise ValueError('副本不属于此SWP原组：'+chosen)
                    source_hash = result['sources'][group['rack_mesh']]['sha256']
                    if source_hash != proof['source_encrypted_uasset_sha256']:
                        raise ValueError('拆分实验与挂载资源来源不一致：'+chosen)
                    carrier.update(mode='split_variant',variant=chosen,package=proof['package'])
                    used_variants.append(chosen)
                    # 副本保留全部Socket，但本组合仍只允许已选中的子集参与离架。
                    sockets = {s['name']:s for s in group['sockets']}
                    for assignment in combo['assignments']:
                        if assignment['group'] != name:
                            continue
                        for socket in assignment['sockets']:
                            point = sockets[socket]['reference_position']
                            if point is None or not any(all(box[i] <= point[i] <= box[i+3] for i in range(3))
                                                        for box in recipe['keep_boxes']):
                                raise ValueError('选用Socket不在副本保留区域：'+chosen+'/'+socket)
                carriers.append(carrier)
            combo['rack_plan'] = {'selection_policy':'fixed_by_weapon_set', 'carriers':carriers,
                                  'visibility_policy':'仅隐藏架自身，不递归隐藏弹体；原Socket持续更新'}
            combo.pop('mesh_replacement',None)
            combo['projectile_mesh_replacement_required'] = False
            combo['rack_mesh_replacement_required'] = bool(used_variants)
            combo['runtime_prerequisites'] = list(PREREQUISITES)
            if code=='f02a' and 'RKT' in combo['weapons']:
                combo['runtime_prerequisites'].append('外翼38口只是两巢发射口；副本仍有全部76个Socket，显式排除内翼口；巢体耗尽不隐藏')
            if any(x.get('below_window') is True for x in combo['assignments']):
                combo['runtime_prerequisites'].append('明确接受部分位置少于装填窗口；保留原队列身份，不缩短队列或按排序编号简单取余')
            if used_variants:
                if 'required_asset_work' in combo:
                    combo['geometry_partition_description'] = combo.pop('required_asset_work')
                combo['status_description'] = '已核对完整LOD分区与资源往返的布局候选，运行加载未验证'
                combo['previous_research_status'] = combo.get('previous_research_status',combo['status'])
                combo['status'] = 'split_asset_candidate'
                combo['disposition'] = 'candidate_split_assets'
                combo['suggested_priority'] = 'P2'
                combo['split_evidence'] = {**proof_reference, 'variants':used_variants,
                                          'runtime_asset_loading_verified':False, 'game_verified':False}
                for field in ('deferred_reason','blocker_kind','blockers'):
                    combo.pop(field,None)
                validate_split_evidence(combo)
        sample['integration_gate'] = aircraft_gate(sample['proposal']['combinations'])
        if not sample['integration_gate']['eligible']:
            raise ValueError('未补齐完整七组合，不允许输出部分方案：'+code)
        for combo in sample['proposal']['combinations']:
            combo['integration_allowed'] = True
        sample['split_layout_research'] = {**proof_reference,
            'scope':'七组合载体完整，仅解除无分区配方的离线阻塞；加载、渲染、碰撞与动作仍待运行接入',
            'game_assets_generated_this_run':False, 'compiled':False, 'installed':False}
    if found != set(LAYOUTS):
        raise ValueError('输入没有全部四机，拒绝部分交付')
    result['research_policy'].update(existing_whole_assets_only=False, defer_resource_splitting=False,
        defer_unvalidated_resource_splitting=True, require_all_seven_combinations=True,
        validated_static_split_aircraft=sorted(found))
    result['static_split_research'] = {**proof_reference,
        'experimental_integration_allowed':False, 'meaning':'上游副本不能直接安装；本表eligible只允许继续整机运行接入'}
    result['assets_generated_by_upstream_research'] = True
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input',type=Path,default=ROOT/'evidence/mount-expansion-batch1.json')
    parser.add_argument('--output',type=Path,required=True)
    args = parser.parse_args()
    result = build(json.loads(args.input.read_text(encoding='utf-8')))
    with args.output.open('x',encoding='utf-8',newline='\n') as stream:
        json.dump(result,stream,ensure_ascii=False,indent=2)
        stream.write('\n')
    print(json.dumps({'complete_aircraft':sorted(LAYOUTS),'cases':28,'game_assets_generated':False},ensure_ascii=False))


if __name__=='__main__':
    main()
