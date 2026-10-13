-- Extend the existing CampaignHangarWeapon page. No viewport popup, Win32
-- input hook, synthetic key press or weapon-memory write is used here.
-- The AC7 native continuation remains responsible for leaving this page.
local path=debug.getinfo(1,"S").source:gsub("^@",""):gsub("\\","/")
local Flow=dofile(assert(path:match("^(.*)/[^/]+$")).."/loadout_flow.lua")
local NativeLoadout={}
local TRANSIENT,VISIBLE,COLLAPSED=0x40,0,1
local function valid(o)return o and o:IsValid()end
local function vec(v)return {X=v.X,Y=v.Y}end
local function key(name)return {KeyName=FName(name)}end
local function color(r,g,b,a)return {SpecifiedColor={R=r,G=g,B=b,A=a},ColorUseRule=0}end
local function copyProfile(p)return {a=p.a,b=p.b,ratio=p.ratio}end

function NativeLoadout.new(note,commit)
    local self={cache=nil,status="waiting",error=nil,lastScan=-1,ignored=nil,serial=0,transaction=nil}
    local panelClass,userClass,canvasClass,horizontalClass,verticalBoxClass
    local function walk(widget,found)
        if not valid(widget)then return end
        found[widget:GetFName():ToString()]=widget
        if widget:IsA(panelClass)then widget.Slots:ForEach(function(_,slot)walk(slot:get().Content,found)end)end
        if widget:IsA(userClass)and valid(widget.WidgetTree)then walk(widget.WidgetTree.RootWidget,found)end
    end
    local function controls(widget)
        local result={};walk(widget,result);return result
    end
    local function api()
        for _,name in ipairs({"/Script/UMG.PanelWidget:AddChild","/Script/UMG.PanelWidget:RemoveChild",
            "/Script/UMG.Widget:SetRenderOpacity","/Script/UMG.Widget:SetVisibility","/Script/UMG.Image:SetBrush",
            "/Script/UMG.Image:SetBrushTintColor","/Script/UMG.Image:SetColorAndOpacity","/Script/UMG.TextBlock:SetText",
            "/Script/UMG.TextBlock:SetColorAndOpacity","/Script/UMG.CanvasPanelSlot:SetPosition",
            "/Script/UMG.CanvasPanelSlot:SetSize","/Script/UMG.CanvasPanelSlot:SetAnchors",
            "/Script/UMG.CanvasPanelSlot:SetAlignment","/Script/UMG.CanvasPanelSlot:SetZOrder",
            "/Script/UMG.HorizontalBoxSlot:SetPadding","/Script/UMG.HorizontalBoxSlot:SetSize",
            "/Script/UMG.HorizontalBoxSlot:SetHorizontalAlignment","/Script/UMG.HorizontalBoxSlot:SetVerticalAlignment",
            "/Script/Engine.PlayerController:IsInputKeyDown","/Script/Engine.PlayerController:GetInputAnalogKeyState"})do
            assert(valid(StaticFindObject(name)),"Native preparation API unavailable: "..name)
        end
        panelClass=StaticFindObject("/Script/UMG.PanelWidget");userClass=StaticFindObject("/Script/UMG.UserWidget")
        canvasClass=StaticFindObject("/Script/UMG.CanvasPanelSlot");horizontalClass=StaticFindObject("/Script/UMG.HorizontalBoxSlot")
        verticalBoxClass=StaticFindObject("/Script/UMG.VerticalBox");assert(valid(verticalBoxClass),"Native preparation source holder is unavailable")
    end
    local function clone(c,source,isImage,retainSource)
        assert(valid(source)and valid(source.Slot),"Native preparation source is missing")
        local oldSlot=source.Slot;local parent=oldSlot.Parent
        assert(oldSlot:IsA(canvasClass)or oldSlot:IsA(horizontalClass),"Unsupported native preparation slot")
        self.serial=self.serial+1
        local widget=StaticConstructObject(source:GetClass(),c.owner.WidgetTree,FName("DualSPPreparation_"..self.serial),TRANSIENT,0,false,false,source)
        assert(valid(widget),"Cannot construct native preparation control")
        widget.Slot=nil
        c.added[#c.added+1]=widget -- Rollback also owns a partially attached clone.
        local slot=parent:AddChild(widget);assert(valid(slot),"Cannot attach native preparation control")
        if oldSlot:IsA(canvasClass)then
            slot:SetAnchors(oldSlot:GetAnchors());slot:SetAlignment(oldSlot:GetAlignment())
            slot:SetPosition(oldSlot:GetPosition());slot:SetSize(oldSlot:GetSize());slot:SetZOrder(oldSlot:GetZOrder()+10)
        else
            slot:SetPadding(oldSlot.Padding);slot:SetSize(oldSlot.Size)
            slot:SetHorizontalAlignment(oldSlot.HorizontalAlignment);slot:SetVerticalAlignment(oldSlot.VerticalAlignment)
            -- Opacity alone leaves the source consuming Auto width and pushes
            -- the replacement to the right. Retain the header in a collapsed
            -- parent. Canvas list sources keep their original menu geometry.
            if not c.holder then
                self.serial=self.serial+1
                c.holder=StaticConstructObject(verticalBoxClass,c.owner.WidgetTree,FName("DualSPPreparationSource_"..self.serial),TRANSIENT,0,false,false)
                assert(valid(c.holder),"Cannot construct native preparation source holder")
                c.added[#c.added+1]=c.holder;c.holder:SetVisibility(COLLAPSED)
                assert(valid(c.owner.WidgetTree.RootWidget:AddChild(c.holder)),"Cannot retain native preparation source holder")
            end
            local p=oldSlot.Padding;local s=oldSlot.Size
            c.moved[#c.moved+1]={widget=source,parent=parent,padding={Left=p.Left,Top=p.Top,Right=p.Right,Bottom=p.Bottom},
                size={Value=s.Value,SizeRule=s.SizeRule},horizontal=oldSlot.HorizontalAlignment,vertical=oldSlot.VerticalAlignment}
            assert(parent:RemoveChild(source),"Cannot detach native preparation header source")
            assert(valid(c.holder:AddChild(source)),"Cannot retain native preparation header source")
        end
        -- Source widgets retain their original slots and geometry, including
        -- the native menu's cached references. Native color/visibility writes
        -- cannot change the replacement's text or opacity.
        if retainSource~=false then
            c.masked[#c.masked+1]={widget=source,opacity=source.RenderOpacity}
            source:SetRenderOpacity(0)
        end
        widget:SetRenderOpacity(1);widget:SetVisibility(VISIBLE)
        if isImage then widget.bIsImageHidingEnable=false
        else widget.bIsTextHidingEnable=false end
        return widget
    end
    local function setText(c,widget,value)
        local address=widget:GetAddress()
        if c.texts[address]~=value then widget:SetText(FText(value));c.texts[address]=value end
    end
    local function mask(c,widget)
        assert(valid(widget),"Native preparation presentation is missing")
        c.masked[#c.masked+1]={widget=widget,opacity=widget.RenderOpacity};widget:SetRenderOpacity(0)
    end
    local function restore(releaseInput)
        local c=self.cache;if not c then return end
        for _,entry in ipairs(c.moved)do
            if valid(entry.widget)and valid(entry.parent)then
                if valid(entry.widget.Slot)then assert(entry.widget.Slot.Parent:RemoveChild(entry.widget),"Cannot release native preparation header source")end
                local slot=entry.parent:AddChild(entry.widget);assert(valid(slot),"Cannot restore native preparation header source")
                slot:SetPadding(entry.padding);slot:SetSize(entry.size)
                slot:SetHorizontalAlignment(entry.horizontal);slot:SetVerticalAlignment(entry.vertical)
            end
        end
        for _,entry in ipairs(c.masked)do if valid(entry.widget)then entry.widget:SetRenderOpacity(entry.opacity)end end
        for _,widget in ipairs(c.added)do
            if valid(widget)and valid(widget.Slot)and valid(widget.Slot.Parent)then
                assert(widget.Slot.Parent:RemoveChild(widget),"Cannot release native preparation control")
            end
        end
        -- Once handed back, the native menu owns its enable/disable lifecycle.
        -- In particular, never reactivate a menu that is leaving the screen.
        if releaseInput~=false and not c.handedOff and valid(c.owner)then c.owner:SetIsEnableControl(c.originalControl)end
        self.cache=nil;self.transaction=nil
    end
    local function fail(err)
        local c=self.cache;self.ignored=c and c.owner:GetAddress()or self.ignored
        self.error=tostring(err);self.status="error";note("NATIVE_LOADOUT_ERROR "..self.error)
        local ok,why=pcall(restore);if not ok then note("NATIVE_LOADOUT_RESTORE_ERROR "..tostring(why))end
    end
    local function bindings(c)
        local settings=StaticFindObject("/Script/Engine.Default__InputSettings")
        assert(valid(settings),"Native input settings are unavailable")
        c.accept={};c.cancel={}
        settings.ActionMappings:ForEach(function(_,entry)
            local mapping=entry:get();local action=mapping.ActionName:ToString()
            if action=="INP_MenuAccept"or action=="INP_MenuCancel"then
                local target=action=="INP_MenuAccept"and c.accept or c.cancel
                target[#target+1]={key=key(mapping.Key.KeyName:ToString()),shift=mapping.bShift,ctrl=mapping.bCtrl,alt=mapping.bAlt,cmd=mapping.bCmd}
            end
        end)
        assert(#c.accept>0 and #c.cancel>0,"AC7 native menu confirmation/back bindings were not found")
        c.up={key("Up"),key("W"),key("Gamepad_DPad_Up")};c.down={key("Down"),key("S"),key("Gamepad_DPad_Down")}
        c.stick=key("Gamepad_LeftY")
    end
    local function modifier(c,left,right)return c.controller:IsInputKeyDown(key(left))or c.controller:IsInputKeyDown(key(right))end
    local function mappedDown(c,entries)
        for _,entry in ipairs(entries)do
            if c.controller:IsInputKeyDown(entry.key)and (not entry.shift or modifier(c,"LeftShift","RightShift"))
                and(not entry.ctrl or modifier(c,"LeftControl","RightControl"))and(not entry.alt or modifier(c,"LeftAlt","RightAlt"))
                and(not entry.cmd or modifier(c,"LeftCommand","RightCommand"))then return true end
        end
        return false
    end
    local function down(c,keys)
        for _,k in ipairs(keys)do if c.controller:IsInputKeyDown(k)then return true end end
        return false
    end
    local function inputs(c)
        local axis=c.controller:GetInputAnalogKeyState(c.stick)
        return {accept=mappedDown(c,c.accept),cancel=mappedDown(c,c.cancel),up=down(c,c.up)or axis>.55,down=down(c,c.down)or axis<-.55}
    end
    local function attach(owner,instance,planeId,profile,options)
        local c={owner=owner,instance=instance,planeId=planeId,added={},masked={},moved={},texts={},lastPaint=nil,originalControl=owner:IsEnableControl(),flow=Flow.new(profile,options)}
        self.cache=c
        c.controller=FindFirstOf("NimbusPlayerController");assert(valid(c.controller),"Native preparation controller is unavailable")
        c.world=instance:GetWorld():GetAddress();bindings(c)
        local base=owner.LightMenuBaseWidget;assert(valid(base),"Campaign native list is missing")
        local list=controls(base)
        for _,name in ipairs({"TitleText","DescriptionText","ItemText0","ItemText1","ItemText2"})do assert(valid(list[name]),"Native list control is missing: "..name)end
        -- Preserve the game's own footer glyphs and device-dependent labels.
        c.title=clone(c,list.TitleText,false);c.description=clone(c,list.DescriptionText,false)
        c.rows={};for i=1,3 do c.rows[i]=clone(c,list["ItemText"..(i-1)],false)end
        c.rowSources={list.ItemText0,list.ItemText1,list.ItemText2}
        local info=owner.WeaponInformation;assert(valid(info),"Native weapon information is missing")
        local nameControls=controls(info.HangarAircraftWeaponNameLong)
        c.name=clone(c,assert(nameControls.Name,"Native weapon name is missing"),false)
        local group=controls(info.HangarSpecGroupR)
        c.bars={}
        for i=1,6 do
            local item=assert(group["SpecItem"..i],"Native specification item is missing")
            local bar=item.SpecBar
            assert(valid(bar)and valid(bar.BarA)and valid(bar.BaseImage3),"Native specification bar is missing")
            local image=clone(c,bar.BarA,true)
            local pos=bar.BarA.Slot:GetPosition();local endPos=bar.BaseImage3.Slot:GetPosition()
            local width=endPos.X-pos.X;assert(width>0 and width<1000,"Invalid native specification bar width")
            c.bars[i]={widget=image,width=width,height=bar.BarA.Slot:GetSize().Y}
            mask(c,bar.BarB);mask(c,bar.BarC);mask(c,bar.Transition)
        end
        -- The stock panel contains all three candidate weapons. Reuse its
        -- first two name/ammo cells for the prepared pair; keep GUN/MSL/FLR.
        local ammo=info.HangarAircraftAllWeaponsAmmo;assert(valid(ammo),"Native ammunition panel is missing")
        c.ammo={}
        for i=1,2 do
            local widgets=controls(ammo["WeaponsSP"..i])
            c.ammo[i]={name=clone(c,assert(widgets.WeaponsName),false),count=clone(c,assert(widgets.WeaponsAmmo),false)}
        end
        mask(c,ammo.WeaponsSP3)
        -- Do not present the old weapon's concept animation/type as the new
        -- choice. Its native playback and hangar camera are deferred work.
        mask(c,info.HangarAircraftWeaponProperties);mask(c,info.HangarAircraftWeaponType)
        c.icons={};c.iconSources={}
        local icons=controls(owner.LightMenuItemIconCategoryWidget)
        for i=1,3 do
            local source=assert(icons["ItemIconL"..(i-1)],"Native weapon category icon is missing")
            c.iconSources[i]=source;c.icons[i]=clone(c,source,true)
        end
        for _,widget in ipairs(FindAllOf("LightMenuItemMarkerWidget")or{})do
            if valid(widget)and not widget:GetFullName():find("WidgetArchetype",1,true)and not widget:GetFullName():find("Default__",1,true)
                and widget:GetWorld():GetAddress()==c.world then mask(c,widget)end
        end
        -- A native solid Image uses the game's own bar brush for the highlight.
        mask(c,owner.LightMenuItemIconStatusWidget)
        c.highlight=clone(c,c.bars[1].widget,true,false)
        c.highlight.Slot.Parent:RemoveChild(c.highlight)
        local target=list.ItemText0.Slot.Parent:AddChild(c.highlight);assert(valid(target),"Cannot attach native selection highlight")
        target:SetAnchors(list.ItemText0.Slot:GetAnchors());target:SetAlignment({X=0,Y=0});target:SetZOrder(list.ItemText0.Slot:GetZOrder()+9)
        c.highlight:SetBrushTintColor(color(1,1,1,1));c.highlight:SetColorAndOpacity({R=1,G=1,B=1,A=.85})
        owner:SetIsEnableControl(false)
        c.previous=inputs(c);c.repeatAt=0
        note("NATIVE_LOADOUT_ATTACHED "..owner:GetFullName())
        self.status="sp1";self.error=nil
        return c
    end
    local function paint(c)
        local flow=c.flow;local rows=flow:rows();local stage=flow.stage
        local signature=stage..":"..flow.cursor..":"..flow.draft.a..":"..flow.draft.b..":"..tostring(flow.message)
        if signature==c.lastPaint then return end
        c.lastPaint=signature
        local blue=color(.401978,.651406,1,1);local dark=color(.12,.18,.25,1)
        setText(c,c.title,stage=="sp1"and "选择 SP WEAPON 1"or stage=="sp2"and "选择 SP WEAPON 2"or "确认特殊兵装 / CONFIRM LOADOUT")
        for i=1,3 do
            local row=rows[i]
            c.rows[i]:SetVisibility(row and VISIBLE or COLLAPSED);c.icons[i]:SetVisibility(row and row.weapon and row.weapon>=0 and VISIBLE or COLLAPSED)
            if row then
                setText(c,c.rows[i],row.code..(row.enabled and ""or "  未购买"))
                c.rows[i]:SetColorAndOpacity(not row.enabled and color(.3,.36,.43,1)or (i==flow.cursor and dark or blue))
                if row.weapon and row.weapon>=0 then c.icons[i]:SetBrush(c.iconSources[row.weapon+1].Brush)end
            end
        end
        local source=c.rowSources[flow.cursor];local position=source.Slot:GetPosition();local size=source.Slot:GetSize()
        local left=c.iconSources[1].Slot:GetPosition().X
        c.highlight.Slot:SetPosition({X=left-8,Y=position.Y-3});c.highlight.Slot:SetSize({X=position.X+size.X-left+16,Y=size.Y+6})
        local row=rows[flow.cursor]
        local index=stage=="review"and flow.draft.a or row.weapon
        local option=index>=0 and flow.options[index+1]or nil
        local description
        if stage=="review"then description="SP1 / SP2 已选择。松开按键后，再按游戏原生的决定键继续；返回键取消本次准备。"
        elseif not option then description="不搭载 SP2，SP1 使用完整载弹量。普通导弹始终保留。"
        else description=option.description.."。"..(stage=="sp1"and "决定：选择 SP1，进入 SP2。"or "决定：选择 SP2，进入确认。")end
        if flow.message then description=flow.message.."。"..description end
        setText(c,c.description,description);c.description:SetColorAndOpacity(blue)
        setText(c,c.name,option and (option.code:upper().."  "..option.description)or "无兵装 / NO WEAPON");c.name:SetColorAndOpacity(blue)
        -- Catalogue order is AIR / GROUND / HOMING / RELOAD / RANGE / AREA;
        -- AC7's native right column is AIR / GROUND / HOMING / RANGE / AREA / RELOAD.
        local statOrder={1,2,3,5,6,4}
        for i,bar in ipairs(c.bars)do
            local percent=option and option.stats[statOrder[i]]or 0
            bar.widget.Slot:SetSize({X=bar.width*math.max(0,math.min(1,percent/100)),Y=bar.height})
        end
        local a=stage=="sp1"and row.weapon or flow.draft.a
        local b=stage=="sp1"and -1 or stage=="sp2"and row.weapon or flow.draft.b
        for i,cell in ipairs(c.ammo)do
            local weapon=i==1 and a or b
            local selected=weapon>=0 and flow.options[weapon+1]or nil
            local amount=selected and (b<0 and selected.max or math.max(1,math.floor(selected.max*flow.draft.ratio)))or 0
            setText(c,cell.name,selected and selected.code:upper()or "---");setText(c,cell.count,selected and tostring(amount)or "---")
            cell.name:SetColorAndOpacity(blue);cell.count:SetColorAndOpacity(blue)
        end
        self.status=stage
        note("NATIVE_LOADOUT_STAGE "..signature)
    end
    local function pump(instance,planeId,profile,options,now,inFlight,panelOpen)
        if inFlight or panelOpen then if self.cache then restore(not inFlight)end;return end
        local c=self.cache
        if c and (not valid(c.owner)or not valid(c.controller)or c.planeId~=planeId or instance:GetWorld():GetAddress()~=c.world)then
            restore(instance:GetWorld():GetAddress()==c.world);c=nil
        end
        if not c then
            if now-self.lastScan<.25 then return end
            self.lastScan=now
            local candidate=nil
            for _,owner in ipairs(FindAllOf("CampaignHangarWeaponWidget")or{})do
                if valid(owner)and owner:IsEnableControl()and owner:GetWorld():GetAddress()==instance:GetWorld():GetAddress()
                    and not owner:GetFullName():find("WidgetArchetype",1,true)and not owner:GetFullName():find("Default__",1,true)then candidate=owner;break end
            end
            if not candidate then self.ignored=nil;if not self.error then self.status="waiting"end;return end
            if candidate:GetAddress()==self.ignored then return end
            local choices=options();if #choices~=3 then return end
            self.ignored=candidate:GetAddress() -- A rejected API must not retry every menu poll.
            if not panelClass then api()end
            c=attach(candidate,instance,planeId,profile(),choices)
            self.ignored=nil
        end
        local input=inputs(c)
        if c.flow.stage=="review"then
            -- Do not let the SP2 confirmation leak into AC7's original handler.
            -- Hand it back only after release. The next real confirmation is
            -- interpreted entirely by the native menu and its normal routing.
            if not c.handedOff and not input.accept and not input.cancel and not input.up and not input.down then
                c.handedOff=true;c.owner:SetIsEnableControl(true);note("NATIVE_LOADOUT_NATIVE_CONTINUATION_READY")
            elseif c.handedOff then
                if input.accept and not c.previous.accept then c.commitArmedUntil=now+1 end
                if not c.owner:IsEnableControl()then
                    self.ignored=c.owner:GetAddress();restore();self.status="waiting";return
                end
            end
        else
            local direction=input.up and not input.down and -1 or input.down and not input.up and 1 or 0
            local oldDirection=c.previous.up and not c.previous.down and -1 or c.previous.down and not c.previous.up and 1 or 0
            if direction~=0 and(direction~=oldDirection or now>=c.repeatAt)then
                c.flow:move(direction);c.repeatAt=now+(direction~=oldDirection and .35 or .12)
            elseif input.cancel and not c.previous.cancel then
                if c.flow:back()=="cancelled"then
                    self.ignored=c.owner:GetAddress();restore();self.status="stock";return
                end
            elseif input.accept and not c.previous.accept then c.flow:confirm()end
        end
        c.previous=input;paint(c)
    end
    function self:pump(...)
        local args={...};local ok,err=xpcall(function()pump(table.unpack(args))end,debug.traceback)
        if not ok then fail(err)end
    end
    function self:reset(clearError,releaseInput)
        local ok,err=pcall(function()restore(releaseInput)end);if not ok then note("NATIVE_LOADOUT_RESTORE_ERROR "..tostring(err))end
        self.ignored=nil;if clearError~=false then self.error=nil end
        self.status=self.error and "error"or "waiting";self.lastScan=-1
    end
    function self:beforeNativeCommit(ctx,selectedPlane,selectedWeapon)
        local c=self.cache
        if not c or not c.handedOff or c.flow.stage~="review"or ctx:get():GetAddress()~=c.instance:GetAddress()or selectedPlane:get()~=c.planeId then return end
        local ok,err=xpcall(function()
            assert(IsInGameThread(),"Native preparation commit requires the game thread")
            -- SetPlaneAndSWP is a shared native API, not proof of a button
            -- confirmation. Ignore preview/hover writes unless the user has
            -- pressed the native decide binding. Allow a short native delay
            -- after an observed edge; never save merely for entering review.
            if not mappedDown(c,c.accept)and not(c.commitArmedUntil and os.clock()<=c.commitArmedUntil)then return end
            local profile=c.flow:profile()
            selectedWeapon:set(profile.a) -- Never let the stock one-SP choice overwrite SP1.
            self.transaction={instance=c.instance,planeId=c.planeId,profile=profile}
        end,debug.traceback)
        if not ok then fail(err)end
    end
    function self:afterNativeCommit()
        local transaction=self.transaction;if not transaction then return end
        self.transaction=nil
        local ok,err=xpcall(function()
            assert(transaction.instance:GetSelectedPlaneSWP()==transaction.profile.a,"Native SP1 commit did not match the prepared loadout")
            commit(transaction.planeId,copyProfile(transaction.profile))
            self.ignored=self.cache.owner:GetAddress();restore();self.status="committed"
            note("NATIVE_LOADOUT_COMMITTED "..transaction.planeId)
        end,debug.traceback)
        if not ok then fail(err)end
    end
    return self
end
return NativeLoadout
