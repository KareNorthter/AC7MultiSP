-- Extend AC7's own CombinedStatsWidget, not the DX11 overlay.
-- Both visible SP rows have permanent identities. The game's current-SP text
-- controls remain alive under a collapsed native parent: native SetText calls
-- can arrive before or after a switch without ever painting in the wrong row.
local NativeHUD={}
local VISIBLE,COLLAPSED=0,1
local TRANSIENT=0x40
local function valid(o)return o and o:IsValid()end
local function vec(v)return {X=v.X,Y=v.Y}end
local function translation(widget)return vec(widget.RenderTransform.Translation)end
local function shifted(base,dy)return {X=base.X,Y=base.Y+dy}end
local function slotStyle(widget)
    local s=widget.Slot
    return {padding={Left=s.Padding.Left,Top=s.Padding.Top,Right=s.Padding.Right,Bottom=s.Padding.Bottom},
        size={Value=s.Size.Value,SizeRule=s.Size.SizeRule},horizontal=s.HorizontalAlignment,vertical=s.VerticalAlignment}
end
local function applyStyle(slot,style)
    slot:SetPadding(style.padding);slot:SetSize(style.size)
    slot:SetHorizontalAlignment(style.horizontal);slot:SetVerticalAlignment(style.vertical)
end
local required={"PlaneTextStatPanel","SpecialWeaponTextBP","SpecialWeaponCountTextBP","SpecialWeaponSel","NormalWeaponSel",
    "FlareTextBP","FlareCountTextBP","DamageTextBP","DamagePercentageTextBP"}

function NativeHUD.new(note)
    local self={cache=nil,status="waiting",error=nil,lastScan=-1,waitingSince=nil,writeCount=0}
    local panelClass=nil
    local verticalBoxClass=nil
    local cloneSerial=0
    local function inspectTree(root,result)
        if not valid(root)then return end
        result[root:GetFName():ToString()]=root
        if root:IsA(panelClass)then
            root.Slots:ForEach(function(_,entry)
                local slot=entry:get()
                if valid(slot)then inspectTree(slot.Content,result)end
            end)
        end
    end
    local function arrange(column,withExtra,holder)
        -- Build the fixed order once on enable, restore it once on disable.
        -- Never reparent visible rows during weapon switching.
        local function detach(widget)
            if valid(widget)and valid(widget.Slot)then
                local parent=widget.Slot.Parent
                assert(parent:GetAddress()==column.box:GetAddress() or (holder and parent:GetAddress()==holder:GetAddress()),"Native row moved to an unexpected parent")
                assert(parent:RemoveChild(widget),"Cannot reorder native HUD row")
            end
        end
        for _,entry in ipairs(column.original)do detach(entry.widget)end
        for _,widget in ipairs(column.rows)do detach(widget)end
        local function append(entry)
            if valid(entry.widget)then
                local slot=column.box:AddChildToVerticalBox(entry.widget)
                assert(valid(slot),"Cannot attach reordered native HUD row")
                applyStyle(slot,entry.style)
            end
        end
        if withExtra then
            -- A collapsed parent protects against native child visibility writes
            -- and keeps the original controls rooted in the live widget tree.
            assert(valid(holder:AddChildToVerticalBox(column.original[1].widget)),"Cannot retain native SP source")
            for _,widget in ipairs(column.rows)do append({widget=widget,style=column.original[1].style})end
        else append(column.original[1])end
        append(column.original[2]);append(column.original[3])
    end
    local function resize(c)
        if c.panelStretched then c.panelSlot:SetPosition(shifted(c.panelPos,-c.row))
        else c.panelSlot:SetSize({X=c.panelSize.X,Y=c.panelSize.Y+c.row})end
        if not c.hboxStretched then c.hboxSlot:SetSize({X=c.hboxSize.X,Y=c.hboxSize.Y+c.row})end
    end
    local function restore()
        local c=self.cache
        if not c then return end
        if c.columnsTouched then
            for _,column in ipairs(c.columns)do
                if valid(column.box)then arrange(column,false,c.holder)end
            end
            c.columnsTouched=false
        end
        for _,widget in ipairs(c.clones or {})do if valid(widget)then widget:SetVisibility(COLLAPSED)end end
        if valid(c.panelSlot)and c.panelSize then c.panelSlot:SetSize(c.panelSize)end
        if valid(c.hboxSlot)and c.hboxSize then c.hboxSlot:SetSize(c.hboxSize)end
        if valid(c.panelSlot)and c.panelPos then c.panelSlot:SetPosition(c.panelPos)end
        for _,item in ipairs(c.shifted)do
            if valid(item.widget)then item.widget:SetRenderTranslation(item.original)end
        end
        c.applied=false
        c.lastActive=nil
    end
    local function preflight()
        -- Reject an unsupported engine API before modifying any visible widget.
        local functions={"/Script/UMG.VerticalBox:AddChildToVerticalBox","/Script/UMG.CanvasPanel:AddChildToCanvas","/Script/UMG.CanvasPanelSlot:SetSize",
            "/Script/UMG.CanvasPanelSlot:GetSize","/Script/UMG.CanvasPanelSlot:GetPosition",
            "/Script/UMG.CanvasPanelSlot:GetAnchors","/Script/UMG.CanvasPanelSlot:SetPosition",
            "/Script/UMG.PanelWidget:RemoveChild",
            "/Script/UMG.VerticalBoxSlot:SetPadding","/Script/UMG.VerticalBoxSlot:SetSize",
            "/Script/UMG.VerticalBoxSlot:SetHorizontalAlignment","/Script/UMG.VerticalBoxSlot:SetVerticalAlignment",
            "/Script/UMG.Widget:ForceLayoutPrepass","/Script/UMG.Widget:GetDesiredSize","/Script/UMG.TextBlock:SetFont",
            "/Script/UMG.TextBlock:SetText","/Script/UMG.TextBlock:SetColorAndOpacity",
            "/Script/UMG.Widget:SetRenderTranslation","/Script/UMG.Widget:SetVisibility"}
        for _,path in ipairs(functions)do assert(valid(StaticFindObject(path)),"Native HUD API is unavailable: "..path)end
        panelClass=StaticFindObject("/Script/UMG.PanelWidget")
        assert(valid(panelClass),"Native panel class is unavailable")
        verticalBoxClass=StaticFindObject("/Script/UMG.VerticalBox")
        assert(valid(verticalBoxClass),"Native source holder class is unavailable")
    end
    local function clone(source,tree,name,parent)
        -- Use the live native template so font, margins, alignment and color
        -- are inherited. The new control has its own Slate widget and lifetime.
        cloneSerial=cloneSerial+1
        local widget=StaticConstructObject(source:GetClass(),tree,FName(name.."_"..cloneSerial),TRANSIENT,0,false,false,source)
        assert(valid(widget),"Cannot construct native SP row: "..name)
        widget.Slot=nil -- Never inherit a pointer to the source's layout slot.
        local slot=parent:AddChildToVerticalBox(widget)
        assert(valid(slot),"Cannot attach native SP row: "..name)
        widget:SetVisibility(COLLAPSED)
        return widget,slot
    end
    local function attach(owner,widgets)
        local c={owner=owner,widgets=widgets,shifted={},clones={},applied=false}
        self.cache=c -- Keep partial construction recoverable if an API rejects it.
        c.panel=widgets.PlaneTextStatPanel
        c.panelSlot=c.panel.Slot;c.panelSize=vec(c.panelSlot:GetSize())
        c.panelPos=vec(c.panelSlot:GetPosition())
        local panelAnchors=c.panelSlot:GetAnchors()
        c.panelStretched=math.abs(panelAnchors.Maximum.Y-panelAnchors.Minimum.Y)>.001
        c.name=widgets.SpecialWeaponTextBP;c.count=widgets.SpecialWeaponCountTextBP;c.arrow=widgets.SpecialWeaponSel
        local nameBox=c.name.Slot.Parent
        local countBox=c.count.Slot.Parent
        assert(nameBox:GetFName():ToString()=="StatsTextVBox" and countBox:GetFName():ToString()=="StatsInfoVBox","Native SP columns changed")
        local hbox=nameBox.Slot.Parent
        assert(hbox:GetAddress()==countBox.Slot.Parent:GetAddress(),"Native SP columns do not share a row container")
        c.hboxSlot=hbox.Slot;c.hboxSize=vec(c.hboxSlot:GetSize());c.hboxPos=vec(c.hboxSlot:GetPosition())
        local hboxAnchors=c.hboxSlot:GetAnchors()
        c.hboxStretched=math.abs(hboxAnchors.Maximum.Y-hboxAnchors.Minimum.Y)>.001
        -- Runtime native code changes Canvas sizes. Read the live geometry;
        -- cooked archetype values are not a valid runtime compatibility test.
        c.row=c.arrow.Slot:GetPosition().Y-widgets.NormalWeaponSel.Slot:GetPosition().Y
        assert(c.row>0 and c.row<100,"Invalid native HUD row spacing: "..tostring(c.row))
        note(string.format("NATIVE_HUD_LAYOUT panel=%.1f,%.1f columns=%.1f,%.1f row=%.1f stretched=%s,%s",c.panelSize.X,c.panelSize.Y,c.hboxSize.X,c.hboxSize.Y,c.row,tostring(c.panelStretched),tostring(c.hboxStretched)))
        for _,name in ipairs({"SpecialWeaponTextBP","SpecialWeaponCountTextBP","SpecialWeaponSel",
            "FlareTextBP","FlareCountTextBP","DamageTextBP","DamagePercentageTextBP"})do
            local widget=widgets[name]
            c.shifted[#c.shifted+1]={widget=widget,original=translation(widget)}
        end
        c.columns={}
        for _,spec in ipairs({{box=nameBox,names={"SpecialWeaponTextBP","FlareTextBP","DamageTextBP"}},
            {box=countBox,names={"SpecialWeaponCountTextBP","FlareCountTextBP","DamagePercentageTextBP"}}})do
            local column={box=spec.box,original={}}
            for _,name in ipairs(spec.names)do
                column.original[#column.original+1]={widget=widgets[name],style=slotStyle(widgets[name])}
            end
            c.columns[#c.columns+1]=column
        end
        -- Use an empty native container, never a template with copied Slots.
        cloneSerial=cloneSerial+1
        c.holder=StaticConstructObject(verticalBoxClass,owner.WidgetTree,FName("DualSPNativeSource_"..cloneSerial),TRANSIENT,0,false,false)
        assert(valid(c.holder),"Cannot construct native SP source holder")
        c.holder:SetVisibility(COLLAPSED)
        assert(valid(c.panel:AddChildToCanvas(c.holder)),"Cannot retain native SP source holder")
        -- Each row inherits the native font, margins and original slot style.
        -- Names/counts stay in the same column slots for the entire sortie.
        for i,column in ipairs(c.columns)do
            column.rows={}
            for group=1,2 do
                local widget=clone(column.original[1].widget,owner.WidgetTree,"DualSP"..group..(i==1 and "Name" or "Count"),column.box)
                column.rows[group]=widget;c.clones[#c.clones+1]=widget
            end
        end
        c.nameFontSize=c.name.Font.Size;c.countFontSize=c.count.Font.Size
        c.lastNames={};c.lastCounts={};c.lastActive=nil
        note("NATIVE_HUD_ATTACHED "..owner:GetFullName())
        return c
    end
    local function locate(plane,now)
        if now-self.lastScan<.5 then return nil end
        self.lastScan=now
        if not panelClass then preflight()end
        local world=plane:GetWorld():GetAddress()
        for _,owner in ipairs(FindAllOf("CombinedStatsWidget_C")or{})do
            if valid(owner)then
                local name=owner:GetFullName()
                if not name:find("WidgetArchetype",1,true) and not name:find("Default__",1,true)then
                    local ownerWorld=owner:GetWorld()
                    if valid(ownerWorld)and ownerWorld:GetAddress()==world and valid(owner.WidgetTree)then
                        local widgets={};inspectTree(owner.WidgetTree.RootWidget,widgets)
                        local complete=true
                        for _,key in ipairs(required)do if not valid(widgets[key])then complete=false end end
                        if complete then return attach(owner,widgets)end
                    end
                end
            end
        end
        return nil
    end
    local function update(plane,profile,options,state,enabled,now)
        if not enabled then restore();self.status="stock";return end
        if self.error then return end
        if not self.waitingSince then self.waitingSince=now end
        local c=self.cache
        if c and not valid(c.owner)then self.cache=nil;self.waitingSince=now;self.lastScan=-1;c=nil end
        if not c then c=locate(plane,now)end
        if not c then
            self.status="waiting"
            if now-self.waitingSince>5 then error("Cannot locate this mission's native CombinedStatsWidget; no supplemental HUD is drawn")end
            return
        end
        for _,key in ipairs(required)do assert(valid(c.widgets[key]),"Native HUD control was destroyed: "..key)end
        if not c.applied then
            -- In stretched Canvas slots GetSize is a margin, not a height.
            -- Grow that parent upwards by moving its top edge. A stretched
            -- child follows automatically; only fixed-height children resize.
            c.panel:ForceLayoutPrepass()
            local padding=c.columns[1].original[1].style.padding
            local pitch=c.name:GetDesiredSize().Y+padding.Top+padding.Bottom
            assert(pitch>0 and pitch<100,"Invalid native text row height: "..tostring(pitch))
            c.row=pitch;resize(c)
            note(string.format("NATIVE_HUD_ROW_FLOW pitch=%.3f label_font=%d count_font=%d slot_rules=%s,%s",c.row,c.nameFontSize,c.countFontSize,tostring(c.columns[1].original[1].style.size.SizeRule),tostring(c.columns[2].original[1].style.size.SizeRule)))
            c.columnsTouched=true -- Partial changes must remain recoverable.
            for _,column in ipairs(c.columns)do arrange(column,true,c.holder)end
            for _,widget in ipairs(c.clones)do widget:SetVisibility(VISIBLE)end
            c.applied=true
        end
        local active=state.active
        assert(active==0 or active==1,"Invalid native HUD SP selection")
        if c.lastActive~=active then
            c.arrow:SetRenderTranslation(shifted(c.shifted[3].original,active*c.row))
            c.lastActive=active
        end
        -- Ancestor scaling handles resolution. Match a native font change too,
        -- without invalidating layout on every normal 100 ms HUD update.
        if c.name.Font.Size~=c.nameFontSize or c.count.Font.Size~=c.countFontSize then
            for _,widget in ipairs(c.columns[1].rows)do widget:SetFont(c.name.Font)end
            for _,widget in ipairs(c.columns[2].rows)do widget:SetFont(c.count.Font)end
            c.nameFontSize=c.name.Font.Size;c.countFontSize=c.count.Font.Size
            c.panel:ForceLayoutPrepass()
            local padding=c.columns[1].original[1].style.padding
            -- The game-owned source is collapsed; measure a visible twin.
            c.row=c.columns[1].rows[1]:GetDesiredSize().Y+padding.Top+padding.Bottom
            assert(c.row>0 and c.row<100,"Invalid resized native text row height")
            resize(c);c.arrow:SetRenderTranslation(shifted(c.shifted[3].original,active*c.row))
        end
        local nameColor,countColor=c.name.ColorAndOpacity,c.count.ColorAndOpacity
        for group=1,2 do
            local weaponIndex=group==1 and profile.a or profile.b
            local label=options[weaponIndex+1].code:upper()
            local count=tostring(state.cached_sp[group])
            local nameWidget,countWidget=c.columns[1].rows[group],c.columns[2].rows[group]
            if label~=c.lastNames[group]then nameWidget:SetText(FText(label));c.lastNames[group]=label;self.writeCount=self.writeCount+1 end
            if count~=c.lastCounts[group]then countWidget:SetText(FText(count));c.lastCounts[group]=count;self.writeCount=self.writeCount+1 end
            nameWidget:SetColorAndOpacity(nameColor)
            countWidget:SetColorAndOpacity(countColor)
        end
        self.status="native"
    end
    function self:update(plane,profile,options,state,enabled,now)
        local ok,err=xpcall(function()update(plane,profile,options,state,enabled,now)end,debug.traceback)
        if not ok then
            self.error=tostring(err);self.status="error";note("NATIVE_HUD_ERROR "..self.error)
            local restored,restoreErr=pcall(restore)
            if not restored then note("NATIVE_HUD_RESTORE_ERROR "..tostring(restoreErr))end
        end
    end
    function self:reset()
        local ok,err=pcall(restore)
        if not ok then note("NATIVE_HUD_RESTORE_ERROR "..tostring(err))end
        local c=self.cache
        if c then
            -- A HUD toggle keeps its cached row; a world/actor transition
            -- releases the old row so retained widgets cannot accumulate slots.
            local removed,removeErr=pcall(function()
                for _,widget in ipairs(c.clones or {})do
                    if valid(widget)and valid(widget.Slot)and valid(widget.Slot.Parent)then
                        assert(widget.Slot.Parent:RemoveChild(widget),"Cannot release native SP row")
                    end
                end
                if valid(c.holder)and valid(c.holder.Slot)and valid(c.holder.Slot.Parent)then
                    assert(c.holder.Slot.Parent:RemoveChild(c.holder),"Cannot release native SP source holder")
                end
            end)
            if not removed then note("NATIVE_HUD_RELEASE_ERROR "..tostring(removeErr))end
        end
        self.cache=nil;self.error=nil;self.status="waiting";self.lastScan=-1;self.waitingSince=nil
    end
    return self
end
return NativeHUD
