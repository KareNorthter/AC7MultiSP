"""当前三 SP 纯规则回归；不模拟引擎对象，不接触游戏。"""
from pathlib import Path
import sys
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools/test-python'))
from lupa.lua54 import LuaRuntime

lua = LuaRuntime(unpack_returned_tuples=True)
lua.globals().script_dir = (ROOT / 'dist/DualSP/Scripts').as_posix()
lua.execute(r'''
local Profile=dofile(script_dir..'/loadout_profile.lua')
local Flow=dofile(script_dir..'/loadout_flow.lua')
local Cycle=dofile(script_dir..'/weapon_cycle.lua')
local function rejects(fn)assert(not pcall(fn))end
for _,bad in ipairs({{a=-1,b=-1,c=-1},{a=0,b=-1,c=2},{a=0,b=0,c=-1},{a=3,b=-1,c=-1}})do
    rejects(function()Profile.count(bad)end)
end
assert(Profile.ammo(0,3)==0 and Profile.ammo(1,3)==1 and Profile.ammo(10,3)==4)
assert(Profile.ammo(10,2)==5 and Profile.ammo(10,1)==10)
-- 整数百分数含上下界，配弹向上取整；旧三挂 1/3 不能冒充整数百分数。
assert(Profile.ammo(100,3)==33 and Profile.ammo(101,3)==34)
assert(Profile.ammo(100,2,.51)==51 and Profile.ammo(3,2,.51)==2)
for count=2,3 do
    local minimum=count==2 and .5 or .33
    assert(Profile.percent(count,minimum)==minimum*100 and Profile.ammo(7,count,1)==7)
    for _,ratio in ipairs({minimum-.01,1.01,.555,1/3})do
        rejects(function()Profile.ammo(10,count,ratio)end)
    end
end
rejects(function()Profile.ammo(10,1,.99)end)
local settings={ammo_percent_two=75,ammo_percent_three=66}
local snapshot=Profile.copy{a=0,b=1,c=2,ratio=.82}
assert(snapshot.ratio==.82 and Profile.copy(snapshot).ratio==.82)
for count=1,3 do
    local selected=Profile.withSettings({a=0,b=count>1 and 1 or -1,c=count>2 and 2 or -1},settings)
    assert(selected.ratio==({1,.75,.66})[count])
end
assert(snapshot.ratio==.82) -- 新管理设置不能改写已有快照。
local options={{owned=true,available=true,code='A'},{owned=true,available=true,code='B'},{owned=true,available=true,code='C'}}
-- 七种集合各访问一次后回到起点；已有SP次序不产生重复组合。
local combo={a=0,b=-1,c=-1}
local expected={{1,-1,-1},{2,-1,-1},{0,1,-1},{0,2,-1},{1,2,-1},{0,1,2},{0,-1,-1}}
for _,values in ipairs(expected)do
    combo=assert(Profile.nextCombination(combo,options,settings))
    assert(combo.a==values[1] and combo.b==values[2] and combo.c==values[3])
    assert(combo.ratio==({1,.75,.66})[Profile.count(combo)])
end
assert(Profile.nextCombination({a=1,b=0,c=-1},options,settings).b==2)
local restricted={{owned=true,available=true},{owned=false,available=true},{owned=true,available=false}}
combo=Profile.nextCombination({a=0,b=-1,c=-1},restricted,settings)
assert(combo.a==2 and combo.b==-1)
combo=Profile.nextCombination(combo,restricted,settings);assert(combo.a==0 and combo.b==-1)
restricted[3].owned=false;assert(Profile.nextCombination(combo,restricted,settings)==nil)
local tr=function(zh,en)return en end
for count=1,3 do
    for _,mode in ipairs({'preparation','resupply'})do
        local original={a=2,b=1,c=0}
        local flow=Flow.new(original,options,'',mode,tr)
        assert(flow:selectWeapon(0));assert(flow:confirm()=='sp2')
        assert(flow:selectWeapon(count==1 and -1 or 1))
        local nextStage=flow:confirm()
        if count>1 then
            assert(nextStage=='sp3');assert(flow:selectWeapon(count==2 and -1 or 2));nextStage=flow:confirm()
        end
        assert(nextStage==(mode=='resupply' and 'review' or 'continue'))
        local result=flow:profile();assert(Profile.count(result)==count)
        assert(result.ratio==({1,.5,.33})[count] and original.a==2 and original.b==1 and original.c==0)
        if mode=='resupply' then
            assert(flow.cursor==2);assert(flow:confirm()=='sp1') -- 默认 NO，不提前提交。
        else
            assert(flow:back()=='sp'..(count==1 and 2 or 3))
        end
    end
end
-- 草稿读取最新设置，已交接方案仍保留取得时的比例。
local dynamic=Flow.new({a=0,b=1,c=-1,ratio=.9},options,'','preparation',tr,function()return settings end)
assert(dynamic.draft.ratio==.75)
dynamic:confirm();dynamic:selectWeapon(1);assert(dynamic:confirm()=='sp3')
dynamic:selectWeapon(-1);assert(dynamic:confirm()=='continue')
local handedOff=dynamic:profile();assert(handedOff.ratio==.75)
settings.ammo_percent_two=90
assert(dynamic:preview().ratio==.9 and dynamic:profile().ratio==.9 and handedOff.ratio==.75)
options[1].available=false
local single=Flow.new({a=0,b=-1,c=-1},options,'','preparation',tr)
single:confirm();assert(#single:rows()==1 and single:rows()[1].weapon==-1)
rejects(function()Profile.validate({a=0,b=1,c=-1},options)end)
options[2].owned=false
rejects(function()Profile.validate({a=1,b=-1,c=-1},options)end)
assert(Cycle.next(0,{0,3,1})==2 and Cycle.next(2,{0,3,1})==3 and Cycle.next(3,{0,3,1})==0)
assert(Cycle.next(0,{0,0,0})==0)
''')
print('三SP规则：六种出击/补给数量组合、百分比边界/取整、草稿设置/快照、默认NO、返回、单挂限制、未购买拒绝、循环通过')
