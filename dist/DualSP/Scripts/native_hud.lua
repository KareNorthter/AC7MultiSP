-- Extend AC7's own CombinedStatsWidget, not the DX11 overlay.
-- Both visible SP rows have permanent identities. The game's current-SP text
-- controls remain alive under a collapsed native parent: native SetText calls
-- can arrive before or after a switch without ever painting in the wrong row.
local NativeHUD={}
-- 两处 HUD 共用武器简称生成规则。
local modulePath=debug.getinfo(1,"S").source:gsub("^@",""):gsub("\\","/")
local weaponNames=dofile(assert(modulePath:match("^(.*)/[^/]+$")).."/weapon_names.lua")
local Profile=dofile(assert(modulePath:match("^(.*)/[^/]+$")).."/loadout_profile.lua") -- 按实际槽数排列。
local PwHUD=dofile(assert(modulePath:match("^(.*)/[^/]+$")).."/pw_weapon_hud.lua")
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
-- 箭头保留原生 Canvas 位置；源箭头的可见性不再直接控制可见克隆。
local function canvasStyle(slot)
    local anchors=slot:GetAnchors()
    return {anchors={Minimum=vec(anchors.Minimum),Maximum=vec(anchors.Maximum)},alignment=vec(slot:GetAlignment()),
        position=vec(slot:GetPosition()),size=vec(slot:GetSize()),z=slot:GetZOrder()}
end
local function applyCanvasStyle(slot,style)
    slot:SetAnchors(style.anchors);slot:SetAlignment(style.alignment)
    slot:SetPosition(style.position);slot:SetSize(style.size);slot:SetZOrder(style.z)
end
local required={"PlaneTextStatPanel","SpecialWeaponTextBP","SpecialWeaponCountTextBP","SpecialWeaponSel","NormalWeaponSel",
    "GunTextBP","GunCountTextBP","MainWeaponTextBP","MainWeaponCountTextBP",
    "FlareTextBP","FlareCountTextBP","DamageTextBP","DamagePercentageTextBP"}

-- 旧引擎通过文字/阴影颜色的 Alpha 控制透明度，克隆名称不会被游戏抢写。
local function syncNameStyle(widget,source,factor,cache,key)
    local tint,shadow=source.ColorAndOpacity.SpecifiedColor,source.ShadowColorAndOpacity
    local alpha,shadowAlpha=tint.A*factor,shadow.A*factor
    local signature=table.concat({tint.R,tint.G,tint.B,alpha,shadow.R,shadow.G,shadow.B,shadowAlpha},":")
    if cache[key]==signature then return end
    widget:SetColorAndOpacity({SpecifiedColor={R=tint.R,G=tint.G,B=tint.B,A=alpha},ColorUseRule=0})
    widget:SetShadowColorAndOpacity({R=shadow.R,G=shadow.G,B=shadow.B,A=shadowAlpha})
    cache[key]=signature
end

function NativeHUD.new(note,refreshIcons)
    local self={cache=nil,status="waiting",error=nil,lastScan=-1,waitingSince=nil,writeCount=0,prehidden={}}
    local panelClass=nil
    local verticalBoxClass=nil
    local canvasPanelClass=nil
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
    -- 固定 GUN/MSL/SP/FLR/DMG 顺序；单 SP 也使用独立的 MSL/SP 名称。
    local function arrange(column,enabled,holder,count,pwMode)
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
        if column.main then detach(column.main)end
        local function append(entry)
            if valid(entry.widget)then
                local slot=column.box:AddChildToVerticalBox(entry.widget)
                assert(valid(slot),"Cannot attach reordered native HUD row")
                applyStyle(slot,entry.style)
            end
        end
        if enabled then
            append(column.original[1])
            if pwMode then
                -- 原生统计只保留GUN/FLR/DMG；列表克隆仍保有在折叠容器内，供关闭PW时复用。
                for index=2,3 do assert(valid(holder:AddChildToVerticalBox(column.original[index].widget)),"Cannot retain PW HUD source row")end
                for _,widget in ipairs(column.rows)do assert(valid(holder:AddChildToVerticalBox(widget)),"Cannot retain PW HUD list row")end
                if column.main then assert(valid(holder:AddChildToVerticalBox(column.main)),"Cannot retain PW HUD missile row")end
            else
                if column.main then
                    assert(valid(holder:AddChildToVerticalBox(column.original[2].widget)),"无法保留原生 MSL 名称")
                    append({widget=column.main,style=column.original[2].style})
                else append(column.original[2])end
                assert(valid(holder:AddChildToVerticalBox(column.original[3].widget)),"无法保留原生 SP 来源")
                append({widget=column.rows[1],style=column.original[3].style})
                for index=2,3 do
                    if index<=count then append({widget=column.rows[index],style=column.original[3].style})
                    else assert(valid(holder:AddChildToVerticalBox(column.rows[index])),"无法保留未启用的 SP 行")end
                end
            end
            append(column.original[4]);append(column.original[5])
        else
            for _,entry in ipairs(column.original)do append(entry)end
            -- 关闭时克隆仍留在折叠父容器内，避免失去控件树引用。
            for _,widget in ipairs(column.rows)do assert(valid(holder:AddChildToVerticalBox(widget)),"无法保留已关闭的 SP 行")end
            if column.main then assert(valid(holder:AddChildToVerticalBox(column.main)),"无法保留已关闭的 MSL 行")end
        end
    end
    local function resize(c)
        if c.pwMode then
            -- 先为选择区向上扩父面板，再将三行统计放到预留区下面；不依赖越界绘制。
            local reserve=PwHUD.reserveRows*PwHUD.unit(c)
            local delta=reserve-2*c.row
            if c.panelStretched then c.panelSlot:SetPosition(shifted(c.panelPos,-delta))
            else
                c.panelSlot:SetSize({X=c.panelSize.X,Y=c.panelSize.Y+delta})
                c.panelSlot:SetPosition(shifted(c.panelPos,-delta*(1-c.panelAlignment.Y)))
            end
            local shift=reserve-c.hboxAnchors.Minimum.Y*delta
            if c.hboxStretched then
                c.hboxSlot:SetPosition(shifted(c.hboxPos,shift))
                c.hboxSlot:SetSize({X=c.hboxSize.X,Y=c.hboxSize.Y-(1-c.hboxAnchors.Maximum.Y)*delta})
            else
                c.hboxSlot:SetPosition(shifted(c.hboxPos,shift-2*c.row*c.hboxAlignment.Y))
                c.hboxSlot:SetSize({X=c.hboxSize.X,Y=c.hboxSize.Y-2*c.row})
            end
        else
            local extra=c.countSP-1
            if c.panelStretched then c.panelSlot:SetPosition(shifted(c.panelPos,-c.row*extra))
            else c.panelSlot:SetSize({X=c.panelSize.X,Y=c.panelSize.Y+c.row*extra})end
            if not c.hboxStretched then c.hboxSlot:SetSize({X=c.hboxSize.X,Y=c.hboxSize.Y+c.row*extra})end
        end
    end
    -- HUD 创建和武器初始化不同步。先把原生 SP 行折叠，避免双栏接管前
    -- 先画出 MSL+单 SP；接管或失败时再恢复原始可见性。
    local function restorePrehidden(entries)
        for _,entry in ipairs(entries or self.prehidden)do
            if valid(entry.widget)then entry.widget:SetVisibility(entry.visibility)end
        end
    end
    local function hideStock(owner,widgets)
        local seen={}
        for _,entry in ipairs(self.prehidden)do
            if valid(entry.widget)then seen[entry.widget:GetAddress()]=true end
        end
        for _,name in ipairs({"SpecialWeaponTextBP","SpecialWeaponCountTextBP","SpecialWeaponSel"})do
            local widget=widgets[name]
            if valid(widget)and not seen[widget:GetAddress()]then
                self.prehidden[#self.prehidden+1]={widget=widget,visibility=widget:GetVisibility()}
                widget:SetVisibility(COLLAPSED)
                seen[widget:GetAddress()]=true
                note("NATIVE_HUD_STOCK_ROW_HIDDEN "..owner:GetFullName().." "..name)
            end
        end
    end
    local function restore()
        local c=self.cache
        if not c then restorePrehidden();self.prehidden={};return end
        if c.pw then c.pw:hide()end
        if c.columnsTouched then
            for _,column in ipairs(c.columns)do
                if valid(column.box)then arrange(column,false,c.holder)end
            end
            c.columnsTouched=false
        end
        for _,widget in ipairs(c.clones or {})do if valid(widget)then widget:SetVisibility(COLLAPSED)end end
        -- 原生箭头继续接收游戏更新，禁用模组时恢复其原父容器和布局。
        for _,entry in ipairs(c.arrowSources or {})do
            if valid(entry.source)and valid(entry.parent)then
                if not valid(entry.source.Slot)or entry.source.Slot.Parent:GetAddress()~=entry.parent:GetAddress()then
                    if valid(entry.source.Slot)then assert(entry.source.Slot.Parent:RemoveChild(entry.source),"无法释放原生选择箭头")end
                    local slot=entry.parent:AddChildToCanvas(entry.source)
                    assert(valid(slot),"无法恢复原生选择箭头");applyCanvasStyle(slot,entry.style)
                end
            end
        end
        if valid(c.panelSlot)and c.panelSize then c.panelSlot:SetSize(c.panelSize)end
        if valid(c.hboxSlot)and c.hboxSize then c.hboxSlot:SetSize(c.hboxSize)end
        if valid(c.panelSlot)and c.panelPos then c.panelSlot:SetPosition(c.panelPos)end
        if valid(c.hboxSlot)and c.hboxPos then c.hboxSlot:SetPosition(c.hboxPos)end
        for _,item in ipairs(c.shifted)do
            if valid(item.widget)then item.widget:SetRenderTranslation(item.original)end
        end
        restorePrehidden(c.prehidden)
        c.applied=false
        c.lastActive=nil
        c.lastSelection=nil
        c.pwUnit=nil
        c.iconRevision=nil -- 再次显示时重新提交原生剪影，避免沿用关闭前的缓存。
    end
    local function preflight()
        -- Reject an unsupported engine API before modifying any visible widget.
        local functions={"/Script/UMG.VerticalBox:AddChildToVerticalBox","/Script/UMG.CanvasPanel:AddChildToCanvas","/Script/UMG.CanvasPanelSlot:SetSize",
            "/Script/UMG.CanvasPanelSlot:GetSize","/Script/UMG.CanvasPanelSlot:GetPosition",
            "/Script/UMG.CanvasPanelSlot:GetAnchors","/Script/UMG.CanvasPanelSlot:SetPosition",
            "/Script/UMG.CanvasPanelSlot:GetAlignment","/Script/UMG.CanvasPanelSlot:GetZOrder",
            "/Script/UMG.CanvasPanelSlot:SetAnchors","/Script/UMG.CanvasPanelSlot:SetAlignment","/Script/UMG.CanvasPanelSlot:SetZOrder",
            "/Script/UMG.PanelWidget:RemoveChild",
            "/Script/UMG.VerticalBoxSlot:SetPadding","/Script/UMG.VerticalBoxSlot:SetSize",
            "/Script/UMG.VerticalBoxSlot:SetHorizontalAlignment","/Script/UMG.VerticalBoxSlot:SetVerticalAlignment",
            "/Script/UMG.Widget:ForceLayoutPrepass","/Script/UMG.Widget:GetDesiredSize","/Script/UMG.TextBlock:SetFont",
            "/Script/UMG.TextBlock:SetText","/Script/UMG.TextBlock:SetColorAndOpacity","/Script/UMG.TextBlock:SetShadowColorAndOpacity",
            "/Script/UMG.Widget:SetRenderTranslation","/Script/UMG.Widget:SetVisibility","/Script/UMG.Widget:GetVisibility"}
        for _,path in ipairs(functions)do assert(valid(StaticFindObject(path)),"Native HUD API is unavailable: "..path)end
        panelClass=StaticFindObject("/Script/UMG.PanelWidget")
        assert(valid(panelClass),"Native panel class is unavailable")
        verticalBoxClass=StaticFindObject("/Script/UMG.VerticalBox")
        assert(valid(verticalBoxClass),"Native source holder class is unavailable")
        canvasPanelClass=StaticFindObject("/Script/UMG.CanvasPanel")
        assert(valid(canvasPanelClass),"箭头源容器类不可用")
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
    -- 原生会先把选择箭头切到 MSL；通过独立克隆显示最终逻辑选择。
    local function attachArrows(c)
        cloneSerial=cloneSerial+1
        c.arrowHolder=StaticConstructObject(canvasPanelClass,c.owner.WidgetTree,FName("DualSPArrowSource_"..cloneSerial),TRANSIENT,0,false,false)
        assert(valid(c.arrowHolder),"无法建立原生箭头源容器")
        c.arrowHolder:SetVisibility(COLLAPSED)
        assert(valid(c.panel:AddChildToCanvas(c.arrowHolder)),"无法接入原生箭头源容器")
        c.arrowSources={}
        for index,source in ipairs({c.widgets.NormalWeaponSel,c.widgets.SpecialWeaponSel})do
            assert(source.Slot:GetClass():GetFullName()=="Class /Script/UMG.CanvasPanelSlot","箭头布局槽不是 Canvas")
            local entry={source=source,parent=source.Slot.Parent,style=canvasStyle(source.Slot)}
            c.arrowSources[#c.arrowSources+1]=entry
            cloneSerial=cloneSerial+1
            local widget=StaticConstructObject(source:GetClass(),c.owner.WidgetTree,FName("DualSPSelectedArrow_"..cloneSerial),TRANSIENT,0,false,false,source)
            assert(valid(widget),"无法建立独立选择箭头")
            widget.Slot=nil;c.clones[#c.clones+1]=widget;widget:SetVisibility(COLLAPSED)
            local slot=entry.parent:AddChildToCanvas(widget)
            assert(valid(slot),"无法接入独立选择箭头");applyCanvasStyle(slot,entry.style)
            -- 0.5 来自本构建箭头模板的正常 Alpha，不继承源箭头的瞬时隐藏值。
            local tint=source.ColorAndOpacity.SpecifiedColor
            widget:SetColorAndOpacity({SpecifiedColor={R=tint.R,G=tint.G,B=tint.B,A=.5},ColorUseRule=0})
            assert(entry.parent:RemoveChild(source),"无法隔离原生选择箭头")
            local sourceSlot=c.arrowHolder:AddChildToCanvas(source)
            assert(valid(sourceSlot),"无法保留原生选择箭头");applyCanvasStyle(sourceSlot,entry.style)
            if index==1 then c.mainArrow=widget else c.arrow=widget end
        end
    end
    -- HUD 关闭后原箭头已恢复，再启用时重新隔离来源，不重新创建克隆。
    local function retainArrows(c)
        for index,entry in ipairs(c.arrowSources)do
            local slot=entry.source.Slot
            assert(valid(slot),"原生箭头缺少布局槽")
            if slot.Parent:GetAddress()~=c.arrowHolder:GetAddress()then
                assert(slot.Parent:GetAddress()==entry.parent:GetAddress(),"原生箭头被移到未知父容器")
                entry.style=canvasStyle(slot)
                local widget=index==1 and c.mainArrow or c.arrow
                applyCanvasStyle(widget.Slot,entry.style)
                assert(entry.parent:RemoveChild(entry.source),"无法重新隔离原生选择箭头")
                local retained=c.arrowHolder:AddChildToCanvas(entry.source)
                assert(valid(retained),"无法重新保留原生选择箭头");applyCanvasStyle(retained,entry.style)
            end
        end
    end
    local function attach(owner,widgets)
        local c={owner=owner,widgets=widgets,shifted={},clones={},applied=false,prehidden=self.prehidden}
        self.prehidden={}
        self.cache=c -- Keep partial construction recoverable if an API rejects it.
        c.panel=widgets.PlaneTextStatPanel
        c.panelSlot=c.panel.Slot;c.panelSize=vec(c.panelSlot:GetSize())
        c.panelPos=vec(c.panelSlot:GetPosition())
        c.panelAlignment=vec(c.panelSlot:GetAlignment())
        local panelAnchors=c.panelSlot:GetAnchors()
        c.panelStretched=math.abs(panelAnchors.Maximum.Y-panelAnchors.Minimum.Y)>.001
        c.name=widgets.SpecialWeaponTextBP;c.count=widgets.SpecialWeaponCountTextBP;c.arrow=widgets.SpecialWeaponSel
        local nameBox=c.name.Slot.Parent
        local countBox=c.count.Slot.Parent
        assert(nameBox:GetFName():ToString()=="StatsTextVBox" and countBox:GetFName():ToString()=="StatsInfoVBox","Native SP columns changed")
        local hbox=nameBox.Slot.Parent
        assert(hbox:GetAddress()==countBox.Slot.Parent:GetAddress(),"Native SP columns do not share a row container")
        c.hbox=hbox;c.hboxTranslation=translation(hbox)
        c.hboxSlot=hbox.Slot;c.hboxSize=vec(c.hboxSlot:GetSize());c.hboxPos=vec(c.hboxSlot:GetPosition())
        local hboxAnchors=c.hboxSlot:GetAnchors()
        c.hboxAnchors={Minimum=vec(hboxAnchors.Minimum),Maximum=vec(hboxAnchors.Maximum)}
        c.hboxAlignment=vec(c.hboxSlot:GetAlignment())
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
        c.shifted[#c.shifted+1]={widget=hbox,original=c.hboxTranslation} -- PW为宽度留白时只做可恢复的平移。
        c.columns={}
        for _,spec in ipairs({{box=nameBox,names={"GunTextBP","MainWeaponTextBP","SpecialWeaponTextBP","FlareTextBP","DamageTextBP"}},
            {box=countBox,names={"GunCountTextBP","MainWeaponCountTextBP","SpecialWeaponCountTextBP","FlareCountTextBP","DamagePercentageTextBP"}}})do
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
            for group=1,3 do
                local widget=clone(column.original[3].widget,owner.WidgetTree,"DualSP"..group..(i==1 and "Name" or "Count"),column.box)
                column.rows[group]=widget;c.clones[#c.clones+1]=widget
            end
        end
        c.nameFontSize=c.name.Font.Size;c.countFontSize=c.count.Font.Size
        c.lastNames={};c.lastCounts={};c.lastActive=nil
        -- MSL 也使用固定克隆名称，避免原生每帧更新覆盖装填状态颜色。
        c.main=clone(widgets.MainWeaponTextBP,owner.WidgetTree,"DualSPMainName",nameBox)
        c.columns[1].main=c.main;c.clones[#c.clones+1]=c.main
        c.main:SetText(FText("MSL"));c.mainFontSize=widgets.MainWeaponTextBP.Font.Size
        c.nameStyles={}
        c.arrowFontSize=widgets.SpecialWeaponSel.Font.Size;c.mainArrowFontSize=widgets.NormalWeaponSel.Font.Size
        attachArrows(c)
        note("NATIVE_HUD_ATTACHED "..owner:GetFullName())
        return c
    end
    local function locate(plane,now)
        -- 等待 HUD 实例出现时高频短扫描只持续数秒；接管后不进入此路径，
        -- 不把正常飞行轮询变成全局对象枚举。
        if now-self.lastScan<.05 then return nil end
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
                        hideStock(owner,widgets)
                        local complete=true
                        for _,key in ipairs(required)do if not valid(widgets[key])then complete=false end end
                        if complete then return attach(owner,widgets)end
                    end
                end
            end
        end
        return nil
    end
    local function update(plane,profile,options,state,enabled,now,pwEnabled)
        if not enabled then restore();self.status="stock";self.waitingSince=nil;return end
        if self.error then return end
        -- 开场可能很长，输入开放前只等待控件生成；五秒诊断从正式接管时开始。
        if state.pending then self.waitingSince=nil
        elseif not self.waitingSince then self.waitingSince=now end
        local c=self.cache
        if c and not valid(c.owner)then self.cache=nil;self.waitingSince=now;self.lastScan=-1;c=nil end
        if not c then c=locate(plane,now)end
        if not c then
            self.status="waiting"
            if self.waitingSince and now-self.waitingSince>5 then error("Cannot locate this mission's native CombinedStatsWidget; no supplemental HUD is drawn")end
            return
        end
        -- 补给可从双 SP 改为单 SP，或反向改变；同帧还原尺寸再重排，不跨帧露出原行。
        local pwMode=pwEnabled==true
        if c.applied and (c.countSP~=Profile.count(profile)or c.pwMode~=pwMode)then restore()end
        for _,key in ipairs(required)do assert(valid(c.widgets[key]),"Native HUD control was destroyed: "..key)end
        if not c.applied then
            -- 列表模式按SP槽数扩行；PW模式另为选择区预留高度。
            c.countSP=Profile.count(profile)
            c.pwMode=pwMode
            retainArrows(c)
            -- In stretched Canvas slots GetSize is a margin, not a height.
            -- Grow that parent upwards by moving its top edge. A stretched
            -- child follows automatically; only fixed-height children resize.
            -- 原生 SP 源已折叠，开场时从它测量必然可能得到零高。
            -- 先把实际文字放入最终可见行，再测量该行自己的 Slate 布局。
            c.columnsTouched=true -- 即使布局核验失败，也能恢复完整原生控件树。
            for _,column in ipairs(c.columns)do arrange(column,true,c.holder,c.countSP,pwMode)end
            for group=1,c.countSP do
                local weaponIndex=profile[Profile.fields[group]] -- SP 行按逻辑槽固定身份。
                c.columns[1].rows[group]:SetText(FText(weaponNames.special(options,weaponIndex)))
                c.columns[2].rows[group]:SetText(FText(state.pending and "—"or Profile.ammoText(state.cached_sp[group])))
            end
            for _,widget in ipairs(c.clones)do widget:SetVisibility(VISIBLE)end
            local measured=c.columns[1].rows[1] -- 测量控件自身，不依赖尚未显示的 HUD 祖先。
            measured:ForceLayoutPrepass()
            local padding=c.columns[1].original[3].style.padding
            local pitch=measured:GetDesiredSize().Y+padding.Top+padding.Bottom
            assert(pitch>0 and pitch<100,"Invalid native text row height: "..tostring(pitch))
            c.row=pitch;if c.countSP>1 or pwMode then resize(c)end
            note(string.format("NATIVE_HUD_ROW_FLOW pitch=%.3f label_font=%d count_font=%d slot_rules=%s,%s",c.row,c.nameFontSize,c.countFontSize,tostring(c.columns[1].original[3].style.size.SizeRule),tostring(c.columns[2].original[3].style.size.SizeRule)))
            c.applied=true
        end
        local active=state.active
        assert(active>=0 and active<c.countSP,"Invalid native HUD SP selection")
        if c.lastActive~=active then
            c.arrow:SetRenderTranslation(shifted(c.shifted[3].original,active*c.row))
            c.lastActive=active
        end
        -- 预热期间尚未初始化武器。正式提交按重建编号和类别更新一次，
        -- 原生每帧继续负责真实装填进度；不按物理挂点数量伪造剪影。
        if not state.pending and (c.iconRevision~=state.weapon_revision or c.iconArmed~=state.armed)then
            assert(type(state.weapon_revision)=="number","HUD 缺少兵装重建编号")
            assert(valid(c.owner.PlayerPlane)and c.owner.PlayerPlane:GetAddress()==plane:GetAddress(),"剪影 HUD 玩家身份不符")
            local weaponActivator=c.owner.PlayerWeaponActivator -- 当前 HUD 自己绑定的原生组件。
            assert(valid(weaponActivator),"剪影 HUD 尚未绑定兵装组件")
            local ok,err=refreshIcons(plane:GetAddress(),weaponActivator:GetAddress(),c.owner:GetAddress())
            assert(ok,err or "原生装填剪影刷新失败")
            c.iconRevision=state.weapon_revision;c.iconArmed=state.armed
            note("NATIVE_HUD_ICONS_COMMITTED revision="..state.weapon_revision.." armed="..tostring(state.armed))
        end
        -- 两个可见箭头只接受最终选择；原生 MSL 中间态被源容器隔离。
        local selection=state.armed and active+1 or 0
        if c.lastSelection~=selection then
            c.mainArrow:SetVisibility(not pwMode and selection==0 and VISIBLE or COLLAPSED)
            c.arrow:SetVisibility(not pwMode and selection>0 and VISIBLE or COLLAPSED)
            c.lastSelection=selection
        end
        if c.widgets.SpecialWeaponSel.Font.Size~=c.arrowFontSize then
            c.arrow:SetFont(c.widgets.SpecialWeaponSel.Font);c.arrowFontSize=c.widgets.SpecialWeaponSel.Font.Size
        end
        if c.widgets.NormalWeaponSel.Font.Size~=c.mainArrowFontSize then
            c.mainArrow:SetFont(c.widgets.NormalWeaponSel.Font);c.mainArrowFontSize=c.widgets.NormalWeaponSel.Font.Size
        end
        -- Ancestor scaling handles resolution. Match a native font change too,
        -- without invalidating layout on every normal 100 ms HUD update.
        if c.widgets.MainWeaponTextBP.Font.Size~=c.mainFontSize then
            c.main:SetFont(c.widgets.MainWeaponTextBP.Font);c.mainFontSize=c.widgets.MainWeaponTextBP.Font.Size
        end
        if c.name.Font.Size~=c.nameFontSize or c.count.Font.Size~=c.countFontSize then
            for _,widget in ipairs(c.columns[1].rows)do widget:SetFont(c.name.Font)end
            for _,widget in ipairs(c.columns[2].rows)do widget:SetFont(c.count.Font)end
            c.nameFontSize=c.name.Font.Size;c.countFontSize=c.count.Font.Size
            c.columns[1].rows[1]:ForceLayoutPrepass()
            local padding=c.columns[1].original[3].style.padding
            -- The game-owned source is collapsed; measure a visible twin.
            c.row=c.columns[1].rows[1]:GetDesiredSize().Y+padding.Top+padding.Bottom
            assert(c.row>0 and c.row<100,"Invalid resized native text row height")
            if c.countSP>1 or pwMode then resize(c)end
            c.arrow:SetRenderTranslation(shifted(c.shifted[3].original,active*c.row))
        end
        assert(state.ready and type(state.ready[1])=="boolean","MSL 就绪状态缺失")
        -- 未就绪时仅保留正常亮度的35%，使装填状态更容易辨认。
        syncNameStyle(c.main,c.widgets.MainWeaponTextBP,weaponNames.brightness("MSL",state.ready[1],state.main_ammo),c.nameStyles,"main")
        local countColor=c.count.ColorAndOpacity
        local countTint=countColor.SpecifiedColor -- 只有源颜色实际变化时才同步弹药文字。
        local countStyle=table.concat({countTint.R,countTint.G,countTint.B,countTint.A,tostring(countColor.ColorUseRule)},":")
        for group=1,c.countSP do
            local weaponIndex=profile[Profile.fields[group]]
            local label=weaponNames.special(options,weaponIndex)
            local count=state.pending and "—"or Profile.ammoText(state.cached_sp[group]) -- 无限显示---；初始化前不猜数量。
            local nameWidget,countWidget=c.columns[1].rows[group],c.columns[2].rows[group]
            if label~=c.lastNames[group]then nameWidget:SetText(FText(label));c.lastNames[group]=label;self.writeCount=self.writeCount+1 end
            if count~=c.lastCounts[group]then countWidget:SetText(FText(count));c.lastCounts[group]=count;self.writeCount=self.writeCount+1 end
            if c.lastCountStyle~=countStyle then countWidget:SetColorAndOpacity(countColor)end
            -- 普通兵装按核心就绪结果变暗；三种炮类只在库存为零时变暗。
            assert(state.ready and type(state.ready[group+1])=="boolean","SP 就绪状态缺失")
            syncNameStyle(nameWidget,c.name,weaponNames.brightness(label,state.ready[group+1],state.cached_sp[group]),c.nameStyles,"sp"..group)
        end
        c.lastCountStyle=countStyle
        if pwMode then
            local unit=PwHUD.unit(c)
            if c.pwUnit~=unit then resize(c);c.pwUnit=unit end
            if not c.pw then PwHUD.attach(c)end -- 默认关闭时不构造PW控件或查询其专用API。
            c.pw:update(profile,options,state)
        end
        self.status="native"
    end
    function self:update(plane,profile,options,state,enabled,now,pwEnabled)
        local ok,err=xpcall(function()update(plane,profile,options,state,enabled,now,pwEnabled)end,debug.traceback)
        if not ok then
            self.error=tostring(err);self.status="error";note("NATIVE_HUD_ERROR "..self.error)
            local restored,restoreErr=pcall(restore)
            if not restored then note("NATIVE_HUD_RESTORE_ERROR "..tostring(restoreErr))end
        end
    end
    function self:reset(strict)
        local ok,err=pcall(restore)
        local cleanupError=not ok and tostring(err)or nil
        if not ok then note("NATIVE_HUD_RESTORE_ERROR "..tostring(err))end
        local c=self.cache
        if c then
            -- A HUD toggle keeps its cached row; a world/actor transition
            -- releases the old row so retained widgets cannot accumulate slots.
            local removed,removeErr=pcall(function()
                if c.pw then c.pw:release()end
                for _,widget in ipairs(c.clones or {})do
                    if valid(widget)and valid(widget.Slot)and valid(widget.Slot.Parent)then
                        assert(widget.Slot.Parent:RemoveChild(widget),"Cannot release native SP row")
                    end
                end
                if valid(c.holder)and valid(c.holder.Slot)and valid(c.holder.Slot.Parent)then
                    assert(c.holder.Slot.Parent:RemoveChild(c.holder),"Cannot release native SP source holder")
                end
                if valid(c.arrowHolder)and valid(c.arrowHolder.Slot)and valid(c.arrowHolder.Slot.Parent)then
                    assert(c.arrowHolder.Slot.Parent:RemoveChild(c.arrowHolder),"无法释放箭头源容器")
                end
            end)
            if not removed then cleanupError=cleanupError or tostring(removeErr);note("NATIVE_HUD_RELEASE_ERROR "..tostring(removeErr))end
        end
        if strict and cleanupError then error(cleanupError)end -- 保留未完成责任，交由统一退让升级故障。
        self.cache=nil;self.error=nil;self.status="waiting";self.lastScan=-1;self.waitingSince=nil
    end
    return self
end
return NativeHUD
