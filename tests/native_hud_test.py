"""Run the shipped native HUD adapter against a native-shaped mock widget tree."""
import json
import sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools/test-python'))
from lupa.lua54 import LuaRuntime
lua=LuaRuntime(unpack_returned_tuples=True)
lua.globals().adapterPath=str(ROOT/'dist/DualSP/Scripts/native_hud.lua').replace('\\','/')
lua.execute(r'''
local id=0
world={IsValid=function()return true end,GetAddress=function()return 900 end}
function name(value)return {ToString=function()return value end}end
function FName(value)return name(value)end
function FText(value)return {ToString=function()return value end}end
local function widget(label,panel)
    id=id+1
    local w={id=id,label=label,panel=panel,alive=true,RenderTransform={Translation={X=0,Y=0}},
        ColorAndOpacity={SpecifiedColor={R=.1,G=1,B=.2,A=.5}},Font={Size=22,Typeface='ACES-Regular'},textCalls=0}
    w.Margin={Top=-6};w.desiredOffset=14
    function w:IsValid()return self.alive end
    function w:IsA()return self.panel end
    function w:GetAddress()return self.id end
    function w:GetFName()return name(self.label)end
    function w:GetClass()return 'TextBlock' end
    function w:SetRenderTranslation(v)self.RenderTransform.Translation={X=v.X,Y=v.Y}end
    function w:SetVisibility(v)self.visibility=v end
    function w:SetText(v)self.text=v:ToString();self.textCalls=self.textCalls+1 end
    function w:SetColorAndOpacity(v)self.ColorAndOpacity=v end
    function w:SetFont(v)self.Font={Size=v.Size,Typeface=v.Typeface}end
    function w:GetDesiredSize()return {X=60,Y=self.Font.Size+self.desiredOffset+self.Margin.Top}end
    function w:ForceLayoutPrepass()self.prepasses=(self.prepasses or 0)+1 end
    w.Slots={items={}}
    function w.Slots:ForEach(fn)for i,s in ipairs(self.items)do fn(i,{get=function()return s end})end end
    return w
end
local function slot(parent,child,x,y,width,height)
    local s={Parent=parent,Content=child,pos={X=x or 0,Y=y or 0},size={X=width or 0,Y=height or 0},anchors={Minimum={X=0,Y=0},Maximum={X=0,Y=0}}}
    s.Padding={Left=0,Top=0,Right=0,Bottom=0};s.Size={Value=1,SizeRule=0};s.HorizontalAlignment=0;s.VerticalAlignment=0
    function s:IsValid()return self.Content:IsValid()end
    function s:GetPosition()return self.pos end
    function s:GetSize()return self.size end
    function s:GetAnchors()return self.anchors end
    function s:SetPosition(v)self.pos={X=v.X,Y=v.Y}end
    function s:SetSize(v)if v.X then self.size={X=v.X,Y=v.Y}else self.Size={Value=v.Value,SizeRule=v.SizeRule}end end
    function s:SetPadding(v)self.Padding=v end
    function s:SetHorizontalAlignment(v)self.HorizontalAlignment=v end
    function s:SetVerticalAlignment(v)self.VerticalAlignment=v end
    function s:SetAlignment(v)self.alignment=v end
    child.Slot=s;parent.Slots.items[#parent.Slots.items+1]=s
    return s
end
root=widget('MainCanvasPanel',true)
panel=widget('PlaneTextStatPanel',true);slot(root,panel,-100,-200,170,180)
hbox=widget('StatsInfoCanvas',true);slot(panel,hbox,24,0,146,180)
labels=widget('StatsTextVBox',true);slot(hbox,labels)
numbers=widget('StatsInfoVBox',true);slot(hbox,numbers)
controls={}
for _,k in ipairs({'GunTextBP','MainWeaponTextBP','SpecialWeaponTextBP','FlareTextBP','DamageTextBP'})do
    controls[k]=widget(k,false);slot(labels,controls[k])
end
for _,k in ipairs({'GunCountTextBP','MainWeaponCountTextBP','SpecialWeaponCountTextBP','FlareCountTextBP','DamagePercentageTextBP'})do
    controls[k]=widget(k,false);slot(numbers,controls[k])
end
controls.SpecialWeaponSel=widget('SpecialWeaponSel',false);slot(panel,controls.SpecialWeaponSel,0,65,24,40)
controls.NormalWeaponSel=widget('NormalWeaponSel',false);slot(panel,controls.NormalWeaponSel,0,37,24,40)
controls.SpecialWeaponTextBP.text='QAAM';controls.SpecialWeaponCountTextBP.text='13'
controls.GunTextBP.Margin.Top=0;controls.GunCountTextBP.Margin.Top=0
function rowY(box,target)
    local y=0
    for _,s in ipairs(box.Slots.items)do
        local w=s.Content
        if w.visibility~=1 then
            if w==target then return y+s.Padding.Top+w.RenderTransform.Translation.Y end
            y=y+w:GetDesiredSize().Y+s.Padding.Top+s.Padding.Bottom
        end
    end
    error('Missing visible row: '..target.label)
end
function order(box)
    local result={}
    for _,s in ipairs(box.Slots.items)do if s.Content.visibility~=1 then result[#result+1]=s.Content end end
    return result
end
function expectRows(active,scale)
    local c=adapter.cache
    local expectedNames={controls.GunTextBP,controls.MainWeaponTextBP,c.columns[1].rows[1],
        c.columns[1].rows[2],controls.FlareTextBP,controls.DamageTextBP}
    local expectedCounts={controls.GunCountTextBP,controls.MainWeaponCountTextBP,c.columns[2].rows[1],
        c.columns[2].rows[2],controls.FlareCountTextBP,controls.DamagePercentageTextBP}
    local namesNow,countsNow=order(labels),order(numbers)
    assert(#namesNow==6 and #countsNow==6)
    for i=1,6 do
        assert(namesNow[i]==expectedNames[i] and countsNow[i]==expectedCounts[i],'Incorrect native row order')
        local nameY,countY=rowY(labels,namesNow[i]),rowY(numbers,countsNow[i])
        assert(math.abs((nameY-countY)*scale)<.001,'Name/count baselines diverged')
        if i>1 then
            local prior=namesNow[i-1]
            assert((nameY-rowY(labels,prior))*scale>=prior:GetDesiredSize().Y*scale-.001,'Overlapping rows')
        end
    end
    assert(controls.SpecialWeaponSel.RenderTransform.Translation.Y==active*c.row,'Arrow row delta does not follow layout')
    assert(expectedNames[3].RenderTransform.Translation.Y==0 and expectedNames[4].RenderTransform.Translation.Y==0)
end
owner={WidgetTree={RootWidget=root,IsValid=function()return true end},IsValid=function()return true end,GetWorld=function()return world end,
    GetFullName=function()return 'CombinedStatsWidget_C /Game/Maps/Mission/mock.mock:PersistentLevel.HUD.WidgetTree.CombinedStatsWidget'end}
plane={GetWorld=function()return world end}
function StaticFindObject(path)return {path=path,IsValid=function()return true end}end
function FindAllOf()return {owner}end
constructed={}
function StaticConstructObject(class,tree,fn,flags,internal,copy,archetype,template)
    assert(tree==owner.WidgetTree and flags==0x40)
    if class.path=='/Script/UMG.VerticalBox' then
        assert(template==nil,'Source holder must not copy a live container Slots array')
        local w=widget(fn:ToString(),true)
        w.AddChildToVerticalBox=labels.AddChildToVerticalBox;w.RemoveChild=labels.RemoveChild
        constructed[#constructed+1]=w
        return w
    end
    assert(class=='TextBlock')
    local w=widget(fn:ToString(),false);w.Font={Size=template.Font.Size,Typeface=template.Font.Typeface}
    w.Margin={Top=template.Margin.Top};w.desiredOffset=template.desiredOffset
    w.Slot=template.Slot -- Adapter must clear this before attaching the clone.
    constructed[#constructed+1]=w
    return w
end
function labels:AddChildToVerticalBox(w)assert(w.Slot==nil,'Inherited native slot must not be detached');return slot(self,w)end
numbers.AddChildToVerticalBox=labels.AddChildToVerticalBox
function labels:RemoveChild(w)
    for i,s in ipairs(self.Slots.items)do if s.Content==w then table.remove(self.Slots.items,i);w.Slot=nil;return true end end
    return false
end
numbers.RemoveChild=labels.RemoveChild
panel.RemoveChild=labels.RemoveChild
function panel:AddChildToCanvas(w)assert(w.Slot==nil);return slot(self,w)end
notes={}
adapter=dofile(adapterPath).new(function(s)notes[#notes+1]=s end)
profile={a=0,b=1};options={{code='qaam'},{code='xsdb'}}
state={active=0,cached_sp={13,24}}
checks=0
local function check(fn)fn();checks=checks+1 end
check(function()adapter:update(plane,profile,options,state,true,0);assert(adapter.status=='native' and not adapter.error,adapter.error)end)
c=adapter.cache
check(function()assert(panel.Slot.size.Y==210 and hbox.Slot.size.Y==210 and c.row==30)end)
check(function()assert(#constructed==5 and c.columns[1].rows[2].Font.Size==22 and c.columns[1].rows[2].Font.Typeface=='ACES-Regular')end)
check(function()assert(c.columns[1].rows[2].text=='XSDB' and c.columns[2].rows[2].text=='24' and c.columns[1].rows[2].Slot.Parent==labels and c.columns[2].rows[2].Slot.Parent==numbers);expectRows(0,1)end)
check(function()assert(controls.SpecialWeaponTextBP.textCalls==0 and controls.SpecialWeaponCountTextBP.textCalls==0)end)
check(function()assert(controls.FlareTextBP.RenderTransform.Translation.Y==0 and controls.DamageTextBP.RenderTransform.Translation.Y==0)end)
check(function()
    state.active=1;state.cached_sp={6,14}
    controls.SpecialWeaponTextBP.text='XSDB';controls.SpecialWeaponCountTextBP.text='14' -- Native game updates.
    adapter:update(plane,profile,options,state,true,.1)
    assert(c.columns[1].rows[1].text=='QAAM' and c.columns[2].rows[1].text=='6');expectRows(1,1)
    assert(controls.SpecialWeaponTextBP.RenderTransform.Translation.Y==0 and controls.SpecialWeaponSel.RenderTransform.Translation.Y==30)
end)
check(function()assert(controls.SpecialWeaponTextBP.text=='XSDB' and controls.SpecialWeaponCountTextBP.text=='14')end)
check(function()
    state.cached_sp[1]=0;adapter:update(plane,profile,options,state,true,.2)
    assert(c.columns[2].rows[1].text=='0')
end)
check(function()
    adapter:update(plane,profile,options,state,false,.3)
    assert(adapter.status=='stock' and panel.Slot.size.Y==180 and hbox.Slot.size.Y==180)
    assert(c.columns[1].rows[2].visibility==1 and controls.SpecialWeaponSel.RenderTransform.Translation.Y==0)
end)
check(function()
    adapter:update(plane,profile,options,state,true,.4)
    assert(controls.SpecialWeaponSel.RenderTransform.Translation.Y==30 and c.columns[1].rows[2].visibility==0);expectRows(1,1)
end)
check(function()
    state.active=0;adapter:update(plane,profile,options,state,true,.5)
    assert(controls.SpecialWeaponSel.RenderTransform.Translation.Y==0);expectRows(0,1)
end)
check(function()
    controls.SpecialWeaponSel.alive=false
    adapter:update(plane,profile,options,state,true,.6)
    assert(adapter.error and adapter.status=='error' and panel.Slot.size.Y==180 and c.columns[1].rows[2].visibility==1)
end)
check(function()controls.SpecialWeaponSel.alive=true;adapter:reset();assert(adapter.error==nil and adapter.cache==nil)end)
check(function()
    -- Native runtime geometry differs from cooked archetypes; this was the
    -- actual 0.1.3 rejection. Neither dimensions nor row pitch are fixed.
    panel.Slot.size.Y=152;hbox.Slot.size.Y=160
    controls.SpecialWeaponSel.Slot.pos.Y=74;controls.NormalWeaponSel.Slot.pos.Y=42
    adapter:update(plane,profile,options,state,true,1)
    assert(adapter.status=='native' and not adapter.error,adapter.error)
    assert(panel.Slot.size.Y==182 and hbox.Slot.size.Y==190)
    expectRows(0,1)
    adapter:reset();assert(panel.Slot.size.Y==152 and hbox.Slot.size.Y==160)
end)
check(function()
    panel.Slot.anchors.Maximum.Y=1;hbox.Slot.anchors.Maximum.Y=1
    panel.Slot.size.Y=0;hbox.Slot.size.Y=0
    local top=panel.Slot.pos.Y
    adapter:update(plane,profile,options,state,true,2)
    assert(adapter.status=='native' and panel.Slot.pos.Y==top-30)
    assert(panel.Slot.size.Y==0 and hbox.Slot.size.Y==0)
    adapter:reset();assert(panel.Slot.pos.Y==top and #labels.Slots.items==5 and #numbers.Slots.items==5)
end)
check(function()
    panel.Slot.anchors.Maximum.Y=0;hbox.Slot.anchors.Maximum.Y=0
    panel.Slot.size.Y=180;hbox.Slot.size.Y=170
    adapter:update(plane,profile,options,state,true,3)
    for _,scale in ipairs({1,4/3,2})do
        for _,selection in ipairs({false,0,1})do
            state.armed=selection~=false;state.active=selection==false and 0 or selection
            adapter:update(plane,profile,options,state,true,3.1)
            expectRows(state.active,scale)
        end
    end
end)
check(function()
    -- Simulate a native font change, in addition to inherited viewport scaling.
    for _,w in pairs(controls)do w.Font.Size=30 end
    state.active=1;adapter:update(plane,profile,options,state,true,4)
    assert(adapter.cache.columns[1].rows[2].Font.Size==30 and adapter.cache.columns[2].rows[2].Font.Size==30)
    assert(adapter.cache.row==38 and panel.Slot.size.Y==218 and hbox.Slot.size.Y==208)
    expectRows(1,4/3)
end)
check(function()
    local before=panel.prepasses
    for i=1,50 do adapter:update(plane,profile,options,state,true,4+i*.1)end
    assert(panel.prepasses==before,'Idle HUD must not force layout prepasses')
end)
check(function()
    -- Render samples between HUD polls: the native setter can lead or lag the
    -- bridge's active index, or write the old name once again in a later Tick.
    -- Also simulate native visibility writes on the retained source controls.
    local c=adapter.cache
    local slots={c.columns[1].rows[1].Slot,c.columns[1].rows[2].Slot,c.columns[2].rows[1].Slot,c.columns[2].rows[2].Slot}
    local nativeNames={'XSDB','QAAM','XSDB','QAAM'}
    local counts={'14','6','0','24'}
    local visibleWrites=c.columns[1].rows[1].textCalls+c.columns[1].rows[2].textCalls
    for i=1,100 do
        for j=1,4 do
            controls.SpecialWeaponTextBP:SetText(FText(nativeNames[j]))
            controls.SpecialWeaponCountTextBP:SetText(FText(counts[j]))
            controls.SpecialWeaponTextBP:SetVisibility(0);controls.SpecialWeaponCountTextBP:SetVisibility(0)
            assert(c.name.Slot.Parent==c.holder and c.count.Slot.Parent==c.holder and c.holder.visibility==1)
            -- Before the mod's next update, each painted row still has its own
            -- identity/count; source text is never part of a visible column.
            assert(order(labels)[3].text=='QAAM' and order(labels)[4].text=='XSDB')
            assert(order(numbers)[3].text==tostring(state.cached_sp[1]) and order(numbers)[4].text==tostring(state.cached_sp[2]))
            state.active=1-state.active
            state.cached_sp[state.active+1]=i+j
            adapter:update(plane,profile,options,state,true,10+i*.1+j*.001)
            expectRows(state.active,4/3)
            assert(order(numbers)[3].text==tostring(state.cached_sp[1]) and order(numbers)[4].text==tostring(state.cached_sp[2]))
            for k,w in ipairs({c.columns[1].rows[1],c.columns[1].rows[2],c.columns[2].rows[1],c.columns[2].rows[2]})do
                assert(w.Slot==slots[k],'A switch must not reparent a visible row')
            end
        end
    end
    assert(c.columns[1].rows[1].textCalls+c.columns[1].rows[2].textCalls==visibleWrites,'Switching must not rewrite names')
end)
check(function()
    local before=adapter.writeCount
    local c=adapter.cache
    state.cached_sp={0,0};adapter:update(plane,profile,options,state,true,22)
    assert(c.columns[2].rows[1].text=='0' and c.columns[2].rows[2].text=='0')
    state.cached_sp={13,24};adapter:update(plane,profile,options,state,true,22.1)
    assert(c.columns[2].rows[1].text=='13' and c.columns[2].rows[2].text=='24')
    assert(adapter.writeCount==before+4,'Only the two ammo controls need writes on resupply')
end)
check(function()
    adapter:reset()
    assert(#labels.Slots.items==5 and #numbers.Slots.items==5)
    assert(order(labels)[3]==controls.SpecialWeaponTextBP and controls.SpecialWeaponSel.RenderTransform.Translation.Y==0)
end)
check(function()
    -- Fail halfway through enabling the second column, after the first column
    -- has already been rebuilt. Both original columns must be recoverable.
    local add=numbers.AddChildToVerticalBox
    local failed=false
    function numbers:AddChildToVerticalBox(w)
        if not failed and w.label:find('DualSP1Count',1,true)and w.visibility==1 and adapter.cache.columnsTouched then
            failed=true;return nil
        end
        return add(self,w)
    end
    adapter:update(plane,profile,options,state,true,25)
    assert(failed and adapter.status=='error' and adapter.error:find('Cannot attach reordered',1,true))
    assert(order(labels)[3]==controls.SpecialWeaponTextBP and order(numbers)[3]==controls.SpecialWeaponCountTextBP)
    assert(#order(labels)==5 and #order(numbers)==5 and panel.Slot.size.Y==180)
    numbers.AddChildToVerticalBox=add
    adapter:reset()
    assert(#labels.Slots.items==5 and #numbers.Slots.items==5)
end)
''')
result={'native_hud_scenarios':lua.globals().checks,'interleaved_switches':400,'simulated_resolutions':['1920x1080','2560x1440','3840x2160'],
        'simulated_selection_states':['MSL','SP1','SP2'],'game_started':False,'rendering_verified_in_game':False}
print(json.dumps(result,indent=2))
(ROOT/'evidence/0.1.7-native-hud-tests.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
