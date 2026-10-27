-- 选装、HUD和管理页面共用显示简称与名称亮度；内部代号保留用于资源键和兵装身份。
local WeaponNames={}
local steadyLabels={PLSL=true,MGP=true,HGP=true}

-- 炮类的极短装填会让名称闪烁；只豁免文字变暗，不改核心就绪状态。
function WeaponNames.brightness(label,ready)
    return (steadyLabels[label]or ready)and 1 or .35
end

-- 原版三机的IEWS共用ecm目录代号。只修正完整资源身份相符的原版兵装，保留Mod自定义名称。
local nativeIEWS={
    mrgn={weapons={"tls","mpbm","ecm"},class="/Game/Blueprint/Weapons/Player/Base/plwp_esm_x0.plwp_esm_x0_C"},
    fa27={weapons={"sod","mstm","ecm"},class="/Game/Blueprint/Weapons/Player/Variation/plwp_ecmp_x2.plwp_ecmp_x2_C"},
    fa44={weapons={"admm","eml","ecm"},class="/Game/Blueprint/Weapons/Player/Variation/plwp_ecmp_x1.plwp_ecmp_x1_C"}
}
local nativeIEWSClasses={}
for _,profile in pairs(nativeIEWS)do nativeIEWSClasses[profile.class]=true end

-- 运行时以当前配置的真实类核对，不按WeaponID、类短名或机体代号推断效果。
function WeaponNames.forClass(code,classPath)
    assert(type(code)=="string"and code~="","武器简称数据缺失")
    if code=="ecm"and nativeIEWSClasses[classPath]then return "IEWS"end
    return code:upper()
end

function WeaponNames.label(weapon)
    assert(weapon and type(weapon.code)=="string"and weapon.code~="","武器简称数据缺失")
    return weapon.display_code or weapon.code:upper()
end

-- 快速任务显示基础目录，尚无当前机体实例；只接受完整原版机体引用和三槽目录签名。
-- 返回展示副本，不改目录本身；进入选装/任务后仍按真实兵装类重新生成显示名。
function WeaponNames.catalogueOptions(info)
    local profile=nativeIEWS[info.code]
    local reference=type(info.reference)=="string"and (info.reference:match("^%w+'([^']+)'$")or info.reference)or ""
    if reference~=""and not reference:find(".",1,true)then
        reference=reference.."."..(reference:match("([^/]+)$")or "")
    end
    if reference~=""and reference:sub(-2)~="_C"then reference=reference.."_C"end
    local expected="/Game/Blueprint/Player/Pawn/AcePlayerPawn_"..info.code..".AcePlayerPawn_"..info.code.."_C"
    local native=profile~=nil and reference==expected and #info.weapons==3
    if native then
        for i,weapon in ipairs(info.weapons)do if weapon.code~=profile.weapons[i]then native=false;break end end
    end
    local options={}
    for i,weapon in ipairs(info.weapons)do
        local copy={};for field,value in pairs(weapon)do copy[field]=value end
        copy.display_code=native and i==3 and "IEWS"or nil
        options[i]=copy
    end
    return options
end

-- 根据配置索引取得已核对的显示简称。
function WeaponNames.special(options,index)
    -- 无兵装不是可显示的武器，调用方必须先判断 SP2 是否为空。
    local weapon=options[index+1]
    return WeaponNames.label(weapon)
end

-- 只根据最终的武器选择显示；MSL 状态下的 SP 预载入不改变中央简称。
function WeaponNames.current(profile,options,state)
    if not state.armed then return "MSL"end
    return WeaponNames.special(options,profile[({"a","b","c"})[state.active+1]])
end
return WeaponNames
