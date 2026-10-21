-- 逻辑选择 0=MSL、1/2/3=SP1/SP2/SP3；真实按键才跳过耗尽槽。
local Cycle={}
local modulePath=debug.getinfo(1,"S").source:gsub("^@",""):gsub("\\","/")
local Profile=dofile(assert(modulePath:match("^(.*)/[^/]+$")).."/loadout_profile.lua")
-- 从当前槽向后选择有弹药的 SP，末尾回到普通导弹。
function Cycle.next(current,ammo)
    assert(current>=0 and current<=#ammo and #ammo>=1 and #ammo<=3,"逻辑武器选择无效")
    for slot=current+1,#ammo do if Profile.hasAmmo(ammo[slot])then return slot end end
    return 0
end
return Cycle
