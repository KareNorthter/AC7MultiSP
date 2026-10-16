-- 中央提示和右下角列表共用简称，避免两份兵装名称映射。
local WeaponNames={}

-- 根据配置索引取得数据表中的大写简称。
function WeaponNames.special(options,index)
    -- 无兵装不是可显示的武器，调用方必须先判断 SP2 是否为空。
    local weapon=options[index+1]
    assert(weapon and type(weapon.code)=="string" and weapon.code~="","武器简称数据缺失")
    return weapon.code:upper()
end

-- 只根据最终的武器选择显示；MSL 状态下的 SP 预载入不改变中央简称。
function WeaponNames.current(profile,options,state)
    if not state.armed then return "MSL"end
    return WeaponNames.special(options,profile[({"a","b","c"})[state.active+1]])
end
return WeaponNames
