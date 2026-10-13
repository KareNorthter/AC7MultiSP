"""Exercise the shipped native preparation UI and its real draft state machine."""
import json
import sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools/test-python'))
from lupa.lua54 import LuaRuntime
lua=LuaRuntime(unpack_returned_tuples=True)
lua.globals().adapterPath=str(ROOT/'dist/DualSP/Scripts/native_loadout.lua').replace('\\','/')
lua.execute((ROOT/'tests/native_loadout_fixture.lua').read_text('utf-8'))
lua.execute(r'''
checks=0
local function check(fn)fn();checks=checks+1 end
check(function()step();assert(adapter.cache and adapter.status=='sp1'and not adapter.error,adapter.error);assert(not owner.enabled)end)
local c=adapter.cache
check(function()
    assert(c.title.text=='选择 SP WEAPON 1'and c.rows[1].text=='QAAM')
    assert(c.ammo[1].count.text=='26'and c.ammo[2].count.text=='---')
    assert(c.rows[1].Font.Size==originals.ItemText0.Font.Size and originals.ItemText0.RenderOpacity==0)
end)
check(function()
    local size=c.rows[1].Slot:GetSize();local pos=c.rows[1].Slot:GetPosition()
    for _,scale in ipairs({1,4/3,2})do
        assert(size.X*scale==originals.ItemText0.Slot:GetSize().X*scale)
        assert(pos.Y*scale==originals.ItemText0.Slot:GetPosition().Y*scale)
    end
    assert(originals.ItemText0.Slot.Parent==base.WidgetTree.RootWidget,'Native menu geometry must remain intact')
    assert(originals.name.Slot.Parent==c.holder and c.holder.visibility==1,'An invisible Auto-width header must not push its replacement sideways')
    assert(c.name.Slot.Parent.class=='HorizontalBox')
end)
check(function()press('Down');assert(c.flow.cursor==2 and c.name.text:find('XSDB',1,true))end)
check(function()
    assert(c.bars[1].widget.Slot.size.X==0 and c.bars[2].widget.Slot.size.X==128)
    assert(c.bars[4].widget.Slot.size.X==48 and c.bars[6].widget.Slot.size.X==96,'Native RANGE/RELOAD bar order')
    assert(c.bars[1].widget.RenderOpacity==1,'Selection highlight must not hide the first specification bar')
end)
check(function()
    press('Enter');assert(c.flow.stage=='sp2'and c.flow.draft.a==1)
    assert(c.rows[1].text=='QAAM'and c.rows[2].text=='4AAM'and c.rows[3].text:find('NO WEAPON',1,true))
    assert(#commits==0 and instance.selected==0,'An intermediate choice must not equip/save a profile')
end)
check(function()press('Escape');assert(c.flow.stage=='sp1'and c.flow.cursor==2 and #commits==0)end)
check(function()press('Enter');c.flow:selectWeapon(-1);step();assert(c.description.text:find('完整载弹量',1,true))end)
check(function()
    pressed.Enter=true;step()
    assert(c.flow.stage=='review'and c.flow.draft.b==-1 and not owner.enabled and not c.handedOff)
    for i=1,6 do step()end
    assert(#commits==0 and not owner.enabled,'Holding the SP2 confirmation must not continue natively')
    pressed.Enter=false;step();assert(c.handedOff and owner.enabled)
    assert(c.ammo[1].count.text=='48'and c.ammo[2].count.text=='---')
end)
check(function()
    local weapon=remote(2)
    adapter:beforeNativeCommit(remote(instance),remote(999),weapon)
    assert(weapon.value==2 and adapter.transaction==nil,'Other aircraft must not be intercepted')
end)
check(function()
    local weapon=remote(2)
    adapter:beforeNativeCommit(remote(instance),remote(110),weapon)
    assert(weapon.value==2 and adapter.transaction==nil and #commits==0,'A native hover/preview write is not a confirmed loadout')
end)
check(function()
    local weapon=remote(0)
    pressed.Enter=true -- The original menu received a real decide input.
    adapter:beforeNativeCommit(remote(instance),remote(110),weapon)
    assert(weapon.value==1)
    instance.selected=weapon.value;adapter:afterNativeCommit()
    pressed.Enter=false
    assert(#commits==1 and commits[1].profile.a==1 and commits[1].profile.b==-1)
    assert(adapter.cache==nil and originals.ItemText0.RenderOpacity==.9 and marker.RenderOpacity==.9 and owner.enabled)
    assert(#base.WidgetTree.RootWidget.Slots.items==5,'Cloned preparation rows must be released')
    assert(originals.name.Slot.Parent.class=='HorizontalBox'and #originals.name.Slot.Parent.Slots.items==1)
end)
check(function()step(.3);assert(adapter.cache==nil,'Do not reopen the same page during its outgoing transition')end)
check(function()
    owner.enabled=false;step(.3);owner.enabled=true;step(.3)
    assert(adapter.cache and adapter.status=='sp1')
    local current=adapter.cache;press('Gamepad_FaceButton_Bottom');assert(current.flow.stage=='sp2')
    axis=-1;step();axis=0;step();axis=-1;step();axis=0;step();assert(current.flow.cursor==3)
    press('Gamepad_FaceButton_Right');assert(current.flow.stage=='sp1')
end)
check(function()
    local previous=#commits
    press('Escape');assert(adapter.cache==nil and owner.enabled and #commits==previous)
    assert(originals.ItemText1.RenderOpacity==.9)
end)
check(function()
    adapter:reset();step(.3)
    adapter:reset(false) -- Same restoration used before F8 owns input.
    assert(adapter.cache==nil and owner.enabled and #base.WidgetTree.RootWidget.Slots.items==5)
end)
check(function()
    step(.3);local current=adapter.cache;local beforeWrites=0
    for _,w in ipairs(current.added)do beforeWrites=beforeWrites+w.textCalls end
    for i=1,50 do step()end
    local afterWrites=0;for _,w in ipairs(current.added)do afterWrites=afterWrites+w.textCalls end
    assert(beforeWrites==afterWrites,'Idle selection must not keep rewriting native text')
end)
check(function()
    local beforeInputs,beforeScans=inputCalls,scans
    for i=1,500 do adapter:pump(instance,110,function()return profile end,function()return options end,clock,true,false)end
    assert(adapter.cache==nil and inputCalls==beforeInputs and scans==beforeScans,'No preparation input/scans in flight')
end)
check(function()
    local name=originals.name;name.alive=false
    adapter:reset();owner.enabled=true;step(.3) -- Native page has entered again after the simulated flight.
    assert(adapter.error and adapter.status=='error'and owner.enabled)
    assert(originals.ItemText0.RenderOpacity==.9 and #base.WidgetTree.RootWidget.Slots.items==5,'Partial construction must restore the native page')
    name.alive=true
    local before=scans;for i=1,100 do step()end
    assert(adapter.cache==nil and #commits==1,'Rejected interface must not retry or equip anything')
    adapter:reset(false);assert(adapter.error,'F8 must preserve the preparation error for diagnosis')
end)
check(function()
    adapter:reset();step(.3)
    local current=adapter.cache
    profile={a=2,b=1,ratio=.3}
    adapter:reset();step(.3);assert(adapter.cache.flow.cursor==3)
    adapter:pump(instance,111,function()return profile end,function()return options end,clock+.3,false,false)
    assert(adapter.cache.planeId==111,'A different aircraft must receive a fresh draft')
    assert(#commits==1)
end)
check(function()
    adapter:reset();step(.3)
    options[2].owned=false;adapter.cache.flow:selectWeapon(1);step();press('Enter')
    assert(adapter.cache.flow.stage=='sp1'and adapter.cache.flow.message and #commits==1)
    options[2].owned=true
end)
check(function()
    adapter:reset();step(.3)
    press('Enter');local current=adapter.cache;current.flow:selectWeapon(0);step();press('Enter')
    assert(current.flow.stage=='review')
    owner.enabled=false;step()
    assert(adapter.cache==nil and #commits==1 and not owner.enabled,'Native review cancellation must not save or reactivate the outgoing menu')
end)
check(function()
    owner.enabled=true;adapter:reset();step(.3)
    local current=adapter.cache
    pressed.Enter=true;step();assert(current.flow.stage=='sp2')
    for i=1,30 do step()end
    assert(current.flow.stage=='sp2','One held confirm must not select both SPs')
    pressed.Enter=false;step();adapter:reset()
end)
check(function()
    mappings[1].Key.KeyName=FName('K');mappings[1].bCtrl=true
    step(.3);local current=adapter.cache
    press('Enter');assert(current.flow.stage=='sp1','Do not hardcode Enter instead of native mappings')
    press('K');assert(current.flow.stage=='sp1','Respect native modifier requirements')
    pressed.LeftControl=true;press('K');pressed.LeftControl=false
    assert(current.flow.stage=='sp2')
    adapter:reset();mappings[1].Key.KeyName=FName('Enter');mappings[1].bCtrl=false
end)
check(function()
    profile={a=0,b=1,ratio=.3};options[1].available=false
    step(.3);press('Enter')
    local current=adapter.cache
    assert(#current.flow:rows()==1 and current.flow:rows()[1].weapon==-1,'Unsupported dual weapon may only choose empty SP2')
    press('Enter');assert(current.flow.stage=='review'and current.flow.draft.b==-1)
    assert(current.ammo[1].count.text=='26','Single SP keeps its full ammo')
    adapter:reset();options[1].available=true
end)
''')
# Verify all legal pairs, empty SP2, and the stage transition/back contract.
flow=lua.eval("dofile(adapterPath:gsub('native_loadout.lua','loadout_flow.lua'))")
matrix=0
for a in range(3):
    for b in [-1,*[i for i in range(3)if i!=a]]:
        model=flow.new(lua.table_from({'a':a,'b':b,'ratio':.5}),lua.globals().options)
        model.confirm(model)
        model.selectWeapon(model,b)
        model.confirm(model)
        result=model.profile(model)
        assert result.a==a and result.b==b
        matrix+=1
result={'native_preparation_scenarios':lua.globals().checks,'legal_pair_matrix':matrix,
        'native_confirmation_handoff':'requires_release_then_real_confirmation','game_started':False,'rendering_verified_in_game':False}
print(json.dumps(result,indent=2))
(ROOT/'evidence/0.1.7-native-loadout-tests.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
