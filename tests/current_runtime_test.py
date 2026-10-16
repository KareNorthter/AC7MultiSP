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
assert(Profile.ammo(0,3)==0 and Profile.ammo(1,3)==1 and Profile.ammo(10,3)==3)
assert(Profile.ammo(10,2)==5 and Profile.ammo(10,1)==10)
local options={{owned=true,available=true,code='A'},{owned=true,available=true,code='B'},{owned=true,available=true,code='C'}}
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
        assert(result.ratio==1/count and original.a==2 and original.b==1 and original.c==0)
        if mode=='resupply' then
            assert(flow.cursor==2);assert(flow:confirm()=='sp1') -- 默认 NO，不提前提交。
        else
            assert(flow:back()=='sp'..(count==1 and 2 or 3))
        end
    end
end
options[1].available=false
local single=Flow.new({a=0,b=-1,c=-1},options,'','preparation',tr)
single:confirm();assert(#single:rows()==1 and single:rows()[1].weapon==-1)
rejects(function()Profile.validate({a=0,b=1,c=-1},options)end)
options[2].owned=false
rejects(function()Profile.validate({a=1,b=-1,c=-1},options)end)
assert(Cycle.next(0,{0,3,1})==2 and Cycle.next(2,{0,3,1})==3 and Cycle.next(3,{0,3,1})==0)
assert(Cycle.next(0,{0,0,0})==0)
''')
print('三SP规则：六种出击/补给数量组合、默认NO、返回、均分、单挂限制、未购买拒绝、循环通过')
