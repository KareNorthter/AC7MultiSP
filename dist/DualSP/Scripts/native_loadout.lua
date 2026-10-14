-- 在 CampaignHangarWeapon 和 MissionResupply 各自的原生页面内改造双选。
-- 不创建外部窗口、不模拟按键；页面跳转继续交给 AC7 原生菜单。
local path=debug.getinfo(1,"S").source:gsub("^@",""):gsub("\\","/")
local Flow=dofile(assert(path:match("^(.*)/[^/]+$")).."/loadout_flow.lua")
local NativeLoadout={}
local TRANSIENT,VISIBLE,COLLAPSED=0x40,0,1
local function valid(o)return o and o:IsValid()end
local function vec(v)return {X=v.X,Y=v.Y}end
local function key(name)return {KeyName=FName(name)}end
local function color(r,g,b,a)return {SpecifiedColor={R=r,G=g,B=b,A=a},ColorUseRule=0}end
local function copyProfile(p)return {a=p.a,b=p.b,ratio=p.ratio}end

-- 补给适配器复用原生控件操作和输入读取，保持两种页面各自的布局。
function NativeLoadout.new(note,commit,mode,keyboardNavigation,takeoff)
    -- 每个适配器独立保存草稿、控件与原生确认事务。
    local supply=mode=="resupply"
    assert(type(keyboardNavigation)=="function","缺少原生页面键盘导航接口")
    if supply then assert(type(takeoff)=="function","缺少原生补给 YES 桥接")end
    local self={cache=nil,status="waiting",error=nil,lastScan=-1,ignored=nil,ignoredReason=nil,controlDepth=0,serial=0,transaction=nil,phase=nil,phaseObject=nil,
        phaseWorld=nil} -- 阶段的关卡世界独立于 GameInstance 的运行任务世界。
    -- 自己禁用/恢复输入时不触发原生页面重新激活的处理。
    local function setControl(owner,enabled)
        self.controlDepth=self.controlDepth+1
        local ok,err=pcall(function()owner:SetIsEnableControl(enabled)end)
        self.controlDepth=self.controlDepth-1
        assert(ok,err)
    end
    -- 源控件在折叠父容器内保留；Canvas 与横排槽按各自格式恢复。
    local panelClass,userClass,horizontalBoxClass,overlayClass,canvasPanelClass
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
            "/Script/UMG.Widget:SetVisibility","/Script/UMG.Widget:GetVisibility","/Script/UMG.Widget:GetOwningPlayer","/Script/UMG.Widget:IsVisible","/Script/UMG.Image:SetBrush",
            "/Script/UMG.Image:SetColorAndOpacity","/Script/UMG.TextBlock:SetText","/Script/UMG.TextBlock:SetFont","/Script/UMG.UserWidget:SetColorAndOpacity",
            "/Script/UMG.TextBlock:SetColorAndOpacity","/Script/UMG.CanvasPanelSlot:SetPosition",
            "/Script/UMG.CanvasPanelSlot:SetAutoSize",
            "/Script/UMG.CanvasPanelSlot:SetSize","/Script/UMG.CanvasPanelSlot:SetAnchors",
            "/Script/UMG.CanvasPanelSlot:SetAlignment","/Script/UMG.CanvasPanelSlot:SetZOrder",
            "/Script/UMG.HorizontalBoxSlot:SetPadding","/Script/UMG.HorizontalBoxSlot:SetSize",
            "/Script/UMG.HorizontalBoxSlot:SetHorizontalAlignment","/Script/UMG.HorizontalBoxSlot:SetVerticalAlignment",
            "/Script/UMG.OverlaySlot:SetPadding","/Script/UMG.OverlaySlot:SetHorizontalAlignment","/Script/UMG.OverlaySlot:SetVerticalAlignment",
            "/Script/Engine.PlayerController:IsInputKeyDown","/Script/Engine.PlayerController:GetInputAnalogKeyState"})do
            assert(valid(StaticFindObject(name)),"Native preparation API unavailable: "..name)
        end
        panelClass=StaticFindObject("/Script/UMG.PanelWidget");userClass=StaticFindObject("/Script/UMG.UserWidget")
        horizontalBoxClass=StaticFindObject("/Script/UMG.HorizontalBox")
        overlayClass=StaticFindObject("/Script/UMG.Overlay")
        assert(valid(horizontalBoxClass)and valid(overlayClass),"准备页横排或叠层源容器不可用")
        canvasPanelClass=StaticFindObject("/Script/UMG.CanvasPanel");assert(valid(canvasPanelClass),"准备页 Canvas 源容器类不可用")
    end

    -- 按实际槽的原生类全名分派，不依赖另行查找的类对象做 IsA 判断。
    -- 支持范围来自准备页资源核对；其他类型明确记录来源，不猜测布局。
    local function slotKind(slot,source)
        assert(valid(slot)and valid(slot.Parent),"准备页控件缺少布局槽："..source:GetFullName())
        local className=slot:GetClass():GetFullName()
        if className=="Class /Script/UMG.CanvasPanelSlot"then return "canvas",className end
        if className=="Class /Script/UMG.HorizontalBoxSlot"then return "horizontal",className end
        if className=="Class /Script/UMG.OverlaySlot"then return "overlay",className end
        error("准备页布局槽不受支持：source="..source:GetFullName().." slot="..slot:GetFullName()
            .." class="..className.." parent="..slot.Parent:GetFullName())
    end

    -- 槽数据先复制成普通 Lua 值；后续移动不会借用已销毁的原槽对象。
    local function readLayout(slot,widget)
        local kind=slotKind(slot,widget)
        local layout={kind=kind}
        if kind=="canvas"then
            local anchors=slot:GetAnchors()
            layout.anchors={Minimum=vec(anchors.Minimum),Maximum=vec(anchors.Maximum)}
            layout.alignment=vec(slot:GetAlignment());layout.position=vec(slot:GetPosition())
            layout.size=vec(slot:GetSize());layout.z=slot:GetZOrder();layout.autoSize=slot.bAutoSize
            assert(type(layout.autoSize)=="boolean","准备页 Canvas 自动尺寸属性缺失")
        else
            local padding=slot.Padding
            layout.padding={Left=padding.Left,Top=padding.Top,Right=padding.Right,Bottom=padding.Bottom}
            layout.horizontal=slot.HorizontalAlignment;layout.vertical=slot.VerticalAlignment
            if kind=="horizontal"then
                layout.size={Value=slot.Size.Value,SizeRule=slot.Size.SizeRule}
            end
        end
        return layout
    end
    -- 克隆、保留与还原使用同一组槽规则；层级容器必须和原槽类型匹配。
    local function applyLayout(slot,layout,widget,zOffset)
        assert(slotKind(slot,widget)==layout.kind,"准备页重建槽类型改变")
        if layout.kind=="canvas"then
            slot:SetAnchors(layout.anchors);slot:SetAlignment(layout.alignment)
            slot:SetPosition(layout.position);slot:SetSize(layout.size)
            slot:SetZOrder(layout.z+(zOffset or 0));slot:SetAutoSize(layout.autoSize)
        else
            slot:SetPadding(layout.padding)
            slot:SetHorizontalAlignment(layout.horizontal);slot:SetVerticalAlignment(layout.vertical)
            if layout.kind=="horizontal"then slot:SetSize(layout.size)end
        end
    end
    -- 游戏在文字周围插入 Overlay/HorizontalBox；坐标属于它们外层的 Canvas 槽。
    local function canvasSlot(widget)
        local current=widget
        for depth=1,16 do
            local slot=current.Slot
            assert(valid(slot)and valid(slot.Parent),"准备页缺少外层 Canvas 定位："..widget:GetFullName())
            if slot:GetClass():GetFullName()=="Class /Script/UMG.CanvasPanelSlot"then return slot end
            local kind=slotKind(slot,current)
            assert(kind=="overlay"or kind=="horizontal","准备页 Canvas 定位层级改变")
            current=slot.Parent
        end
        error("准备页定位层级过深："..widget:GetFullName())
    end
    -- 源对象放入同类型的折叠容器，保留游戏指针及实际槽格式。
    local function retain(c,widget)
        local slot=widget.Slot
        local layout=readLayout(slot,widget)
        local parent=slot.Parent
        c.holders=c.holders or {}
        local holder=c.holders[layout.kind]
        if not holder then
            local classes={canvas=canvasPanelClass,horizontal=horizontalBoxClass,overlay=overlayClass}
            self.serial=self.serial+1
            holder=StaticConstructObject(classes[layout.kind],c.owner.WidgetTree,FName("DualSPPreparationSource_"..self.serial),TRANSIENT,0,false,false)
            assert(valid(holder),"无法创建准备页同类型源容器")
            c.holders[layout.kind]=holder;c.added[#c.added+1]=holder;holder:SetVisibility(COLLAPSED)
            assert(valid(c.owner.WidgetTree.RootWidget:AddChild(holder)),"无法接入准备页源容器")
        end
        c.moved[#c.moved+1]={widget=widget,parent=parent,layout=layout}
        assert(parent:RemoveChild(widget),"无法隔离原生准备页源控件")
        local retained=holder:AddChild(widget);assert(valid(retained),"无法保留准备页源控件")
        applyLayout(retained,layout,widget)
    end
    -- 原生文字模板含隐藏动画的内部状态，新增文字从干净对象初始化，只复制显示样式。
    local function initializeLeaf(widget,source,isImage)
        if isImage then
            widget.bIsImageHidingEnable=false;widget:SetBrush(source.Brush)
        else
            local font=source.Font
            assert(font.Size>0,"准备页原生字体尚未初始化："..source:GetFullName())
            widget.bIsTextHidingEnable=false
            assert(valid(font.FontObject),"准备页字体资源缺失："..source:GetFullName())
            widget:SetFont(font)
            widget.bAutoWrapText=source.bAutoWrapText;widget.WrapTextAt=source.WrapTextAt
            widget.Justification=source.Justification
            widget:SetText(source.Text)
        end
    end
    local function clone(c,source,isImage,retainSource)
        assert(valid(source)and valid(source.Slot),"Native preparation source is missing")
        local oldSlot=source.Slot;local parent=oldSlot.Parent
        local layout=readLayout(oldSlot,source)
        self.serial=self.serial+1
        local widget=StaticConstructObject(source:GetClass(),c.owner.WidgetTree,FName("DualSPPreparation_"..self.serial),TRANSIENT,0,false,false)
        assert(valid(widget),"Cannot construct native preparation control")
        widget.Slot=nil
        c.added[#c.added+1]=widget -- Rollback also owns a partially attached clone.
        initializeLeaf(widget,source,isImage)
        local slot=parent:AddChild(widget);assert(valid(slot),"Cannot attach native preparation control")
        applyLayout(slot,layout,widget,10)
        -- 复用源对象身份和原槽格式/数值，折叠父级承担遮蔽，不调用缺失接口。
        if retainSource~=false then retain(c,source)end
        widget:SetVisibility(VISIBLE)
        if isImage then
            local tint=source.ColorAndOpacity
            widget:SetColorAndOpacity({R=tint.R,G=tint.G,B=tint.B,A=1})
            widget.bIsImageHidingEnable=false
        else
            local tint=source.ColorAndOpacity.SpecifiedColor
            widget:SetColorAndOpacity(color(tint.R,tint.G,tint.B,1))
            widget.bIsTextHidingEnable=false
        end
        return widget
    end
    local function setText(c,widget,value)
        local address=widget:GetAddress()
        if c.texts[address]~=value then widget:SetText(FText(value));c.texts[address]=value end
    end
    local function mask(c,widget)
        assert(valid(widget),"Native preparation presentation is missing")
        if valid(widget.Slot)then
            retain(c,widget)
        else
            -- 独立光标在视口上，原生动画会改写颜色；遮蔽其根容器而不改原对象身份。
            assert(widget:IsA(userClass),"无布局槽的准备页源控件不是 UserWidget")
            local root=widget.WidgetTree.RootWidget
            assert(valid(root),"独立准备控件的根容器缺失")
            c.masked[#c.masked+1]={widget=root,visibility=root:GetVisibility()}
            root:SetVisibility(COLLAPSED)
        end
    end
    -- 原生关闭前恢复视觉树，不清掉事务和缓存；此操作不等同于完成补给。
    local function restoreVisual(c)
        if not c or c.visualRestored then return end
        for _,entry in ipairs(c.moved)do
            if valid(entry.widget)and valid(entry.parent)then
                local current=entry.widget.Slot
                if not valid(current)or not valid(current.Parent)or current.Parent:GetAddress()~=entry.parent:GetAddress()then
                    if valid(current)and valid(current.Parent)then assert(current.Parent:RemoveChild(entry.widget),"无法释放原生页面源控件")end
                    local slot=entry.parent:AddChild(entry.widget);assert(valid(slot),"无法恢复原生页面源控件")
                    applyLayout(slot,entry.layout,entry.widget)
                end
            end
        end
        for _,entry in ipairs(c.masked)do if valid(entry.widget)then entry.widget:SetVisibility(entry.visibility)end end
        for _,widget in ipairs(c.added)do
            if valid(widget)and valid(widget.Slot)and valid(widget.Slot.Parent)then
                assert(widget.Slot.Parent:RemoveChild(widget),"无法释放原生页面克隆控件")
            end
        end
        c.visualRestored=true
    end
    local function restore(releaseInput)
        local c=self.cache;if not c then return end
        restoreVisual(c)
        -- Once handed back, the native menu owns its enable/disable lifecycle.
        -- In particular, never reactivate a menu that is leaving the screen.
        if releaseInput~=false and not c.handedOff and valid(c.owner)then setControl(c.owner,c.originalControl)end
        self.cache=nil;self.transaction=nil
    end
    local function fail(err)
        local c=self.cache;self.ignored=c and c.owner:GetAddress()or self.ignored
        self.ignoredReason="error"
        self.error=tostring(err);self.status="error";note((supply and "NATIVE_RESUPPLY_ERROR "or "NATIVE_LOADOUT_ERROR ")..self.error)
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
        c.up={key("Gamepad_DPad_Up")};c.down={key("Gamepad_DPad_Down")} -- 键盘导航独立读真实状态；手柄由所属控制器读取。
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
        local keyboardUp,keyboardDown,ready=keyboardNavigation()
        assert(type(keyboardUp)=="boolean"and type(keyboardDown)=="boolean"and ready==true,"原生页面键盘导航尚未初始化或接口失败")
        local result={accept=mappedDown(c,c.accept),cancel=mappedDown(c,c.cancel),up=keyboardUp or down(c,c.up)or axis>.55,down=keyboardDown or down(c,c.down)or axis<-.55}
        return result
    end
    -- 双栏采用原生文字/图片模板的叶控件，空 Canvas 不复制任何 Slots 或子树。
    local function infoWidget(c,source,parent,position,size,isImage)
        assert(valid(source),"兵装信息模板缺失")
        self.serial=self.serial+1
        local widget=StaticConstructObject(source:GetClass(),c.owner.WidgetTree,FName("DualSPWeaponInfo_"..self.serial),TRANSIENT,0,false,false)
        assert(valid(widget),"无法创建兵装信息控件")
        widget.Slot=nil;c.added[#c.added+1]=widget
        initializeLeaf(widget,source,isImage)
        local slot=parent:AddChild(widget);assert(valid(slot),"无法接入兵装信息控件")
        slot:SetAnchors({Minimum={X=0,Y=0},Maximum={X=0,Y=0}});slot:SetAlignment({X=0,Y=0})
        slot:SetPosition(position);slot:SetSize(size);slot:SetAutoSize(false);slot:SetZOrder(isImage and 0 or 1)
        widget:SetVisibility(VISIBLE)
        if isImage then
            widget.bIsImageHidingEnable=false
        else
            widget:SetColorAndOpacity(c.tint);widget.bIsTextHidingEnable=false
        end
        return widget
    end
    -- 两种页面分别计算位置，缩放继续由各自的原生 UMG 容器管理。
    local function infoCanvas(c,parent,position,size)
        self.serial=self.serial+1
        local panel=StaticConstructObject(canvasPanelClass,c.owner.WidgetTree,FName("DualSPWeaponColumn_"..self.serial),TRANSIENT,0,false,false)
        assert(valid(panel),"无法创建兵装信息栏")
        c.added[#c.added+1]=panel
        local slot=parent:AddChild(panel);assert(valid(slot),"无法接入兵装信息栏")
        slot:SetAnchors({Minimum={X=0,Y=0},Maximum={X=0,Y=0}});slot:SetAlignment({X=0,Y=0})
        slot:SetPosition(position);slot:SetSize(size);slot:SetAutoSize(false);slot:SetZOrder(10)
        panel:SetVisibility(VISIBLE)
        return panel
    end
    -- 名称、六项性能、类型和预计弹药统一排列，避免两页的信息项不同步。
    local function informationColumns(c,parent,origin,width,pitch,height,templates,ammoLabel)
        c.columns={}
        local tint=templates.items[1].SpecBar.BarA.ColorAndOpacity -- 原画刷仍保留，不重复乘文字颜色。
        for index=1,2 do
            local panel=infoCanvas(c,parent,{X=origin.X+(index-1)*(width+28),Y=origin.Y},{X=width,Y=height})
            local column={bars={},barWidth=width*.44}
            c.columns[index]=column
            column.name=infoWidget(c,templates.name,panel,{X=0,Y=0},{X=width,Y=30},false)
            column.heading=infoWidget(c,templates.heading,panel,{X=0,Y=pitch},{X=width,Y=24},false)
            setText(c,column.heading,"[WEAPON SPECIFICATIONS]")
            for i,item in ipairs(templates.items)do
                local y=(i+1)*pitch -- 每项一行，文字和性能条共享同一基线。
                infoWidget(c,item.SpecName,panel,{X=0,Y=y},{X=width*.52,Y=pitch},false)
                local track=infoWidget(c,item.SpecBar.BarA,panel,{X=width*.56,Y=y+4},{X=column.barWidth,Y=16},true)
                track:SetColorAndOpacity({R=tint.R,G=tint.G,B=tint.B,A=.2})
                local bar=infoWidget(c,item.SpecBar.BarA,panel,{X=width*.56,Y=y+4},{X=column.barWidth,Y=16},true)
                bar:SetColorAndOpacity({R=tint.R,G=tint.G,B=tint.B,A=1});column.bars[i]=bar
            end
            column.typeHeading=infoWidget(c,templates.typeHeading,panel,{X=0,Y=8.2*pitch},{X=width,Y=24},false)
            column.type=infoWidget(c,templates.type,panel,{X=0,Y=9*pitch},{X=width,Y=48},false)
            column.ammoHeading=infoWidget(c,templates.ammoHeading,panel,{X=0,Y=10.7*pitch},{X=width,Y=24},false)
            column.ammo=infoWidget(c,templates.ammo,panel,{X=0,Y=11.5*pitch},{X=width,Y=32},false)
            setText(c,column.typeHeading,"[TYPE]");setText(c,column.ammoHeading,ammoLabel)
        end
    end
    -- 预览一栏时保留另一栏；冲突的旧 SP2 明确标为待重选，避免显示重复挂载。
    local function paintColumns(c)
        local flow=c.flow;local stage=flow.stage;local row=flow:rows()[flow.cursor]
        local a=stage=="sp1"and row.weapon or flow.draft.a
        local b=stage=="sp2"and row.weapon or flow.draft.b
        local pendingSecond=b>=0 and (a==b or not flow.options[a+1].available or not flow.options[b+1].available or not flow.options[b+1].owned)
        local statOrder={1,2,3,5,6,4} -- 目录与原生行顺序：后三项对应范围、伤害范围、装填。
        for i,column in ipairs(c.columns)do
            local weapon=i==1 and a or b
            local pending=i==2 and pendingSecond
            local option=weapon>=0 and not pending and flow.options[weapon+1]or nil
            local amount=option and (b<0 and option.max or math.max(1,math.floor(option.max*flow.draft.ratio)))or 0
            setText(c,column.name,"SP"..i.."  "..(pending and "待重选"or option and option.code:upper()or "NO WEAPON"))
            setText(c,column.type,pending and "请选择其他兵装"or option and option.description or "无兵装")
            setText(c,column.ammo,option and tostring(amount)or "---")
            for stat,bar in ipairs(column.bars)do
                local percent=option and option.stats[statOrder[stat]]or 0
                bar.Slot:SetSize({X=column.barWidth*math.max(0,math.min(1,percent/100)),Y=16})
            end
            -- 准备页普通弹药汇总中的两个 SP 格与上方双栏保持一致。
            if c.ammo then
                setText(c,c.ammo[i].name,pending and "待重选"or option and option.code:upper()or "---")
                setText(c,c.ammo[i].count,option and tostring(amount)or "---")
            end
        end
        return a,b
    end
    local function attach(owner,instance,planeId,profile,options)
        local c={owner=owner,instance=instance,planeId=planeId,added={},masked={},moved={},texts={},lastPaint=nil,originalControl=owner:IsEnableControl(),originalWeapon=instance:GetSelectedPlaneSWP(),flow=Flow.new(profile,options)}
        self.cache=c
        c.controller=owner:GetOwningPlayer();assert(valid(c.controller),"准备页所属输入控制器缺失")
        c.world=instance:GetWorld():GetAddress();bindings(c)
        local base=owner.LightMenuBaseWidget;assert(valid(base),"Campaign native list is missing")
        local list=controls(base)
        for _,name in ipairs({"TitleText","DescriptionText","ItemText0","ItemText1","ItemText2"})do assert(valid(list[name]),"Native list control is missing: "..name)end
        -- Preserve the game's own footer glyphs and device-dependent labels.
        c.title=clone(c,list.TitleText,false);c.description=clone(c,list.DescriptionText,false)
        c.rows={};for i=1,3 do c.rows[i]=clone(c,list["ItemText"..(i-1)],false)end
        -- 采用资源核对的机库信息蓝；列表第一项可能已被原生高亮改成深色。
        c.tint=color(.401978,.651406,1,1)
        local info=owner.WeaponInformation;assert(valid(info),"原生兵装信息区缺失")
        local wrapper=info.HangarSpecGroupR;assert(valid(wrapper)and valid(wrapper.SpecGroup),"准备性能组缺失")
        local group=wrapper.SpecGroup -- 外层 R 只是定位容器；性能项属于内层 SpecGroup。
        local ammo=info.HangarAircraftAllWeaponsAmmo;assert(valid(ammo),"原生弹药汇总缺失")
        local ammoTemplate=controls(ammo.WeaponsSP1)
        local typeTemplate=controls(info.HangarAircraftWeaponType)
        local items={}
        for i=1,6 do
            local item=group["SpecItem"..i]
            assert(valid(item)and valid(item.SpecName)and valid(item.SpecBar)and valid(item.SpecBar.BarA),"准备性能项模板缺失")
            items[i]=item
        end
        local origin=vec(group.Slot:GetPosition()) -- 右栏沿用原右侧性能区，左栏向左展开。
        local width=canvasSlot(group.SpecGroupName):GetSize().X+items[1].Slot:GetPosition().X
        local pitch=items[2].Slot:GetPosition().Y-items[1].Slot:GetPosition().Y
        assert(width>300 and width<450 and pitch>20 and pitch<60,"准备双栏尺寸不符")
        origin.X=origin.X-width-28;origin.Y=origin.Y-1.5*pitch
        assert(origin.X>900 and origin.Y>100,"准备双栏位置不符，停止接入")
        -- 新双栏挂信息区的根 Canvas，不能挂进随后折叠的原性能组件子树。
        local infoRoot=info.WidgetTree.RootWidget
        assert(valid(infoRoot)and infoRoot:GetClass():GetFullName()=="Class /Script/UMG.CanvasPanel","准备信息区根 Canvas 不符")
        informationColumns(c,infoRoot,origin,width,pitch,13*pitch,
            {name=assert(ammoTemplate.WeaponsName),heading=assert(group.SpecGroupName),items=items,
             typeHeading=assert(typeTemplate.WeaponTypeLabel),type=assert(typeTemplate.WeaponTypeText),
             ammoHeading=assert(typeTemplate.WeaponTypeLabel),ammo=assert(ammoTemplate.WeaponsAmmo)},"出击弹药（预计）")
        c.ammo={}
        for i=1,2 do
            local widgets=controls(ammo["WeaponsSP"..i])
            c.ammo[i]={name=clone(c,assert(widgets.WeaponsName),false),count=clone(c,assert(widgets.WeaponsAmmo),false)}
        end
        mask(c,ammo.WeaponsSP3) -- 保留 GUN/MSL/FLR，避免把第三个候选误看成已挂载兵装。
        mask(c,wrapper);mask(c,info.HangarAircraftWeaponNameLong)
        mask(c,info.HangarAircraftWeaponProperties);mask(c,info.HangarAircraftWeaponType)
        -- 原状态图标是未读标记，不能当作兵装类别；新列表以简称和选中底色识别。
        mask(c,owner.LightMenuItemIconStatusWidget)
        -- A native solid Image uses the game's own bar brush for the highlight.
        mask(c,owner.LightMenuItemIconCategoryWidget) -- 右侧原生单兵装类别图标不能混入新的双栏。
        c.highlight=clone(c,c.columns[1].bars[1],true,false)
        assert(c.highlight.Slot.Parent:RemoveChild(c.highlight),"无法移动准备高亮框")
        local rowSlot=canvasSlot(c.rows[1]) -- 文字的实际槽为 Overlay，高亮挂其可见外层 Canvas。
        local target=rowSlot.Parent:AddChild(c.highlight);assert(valid(target),"无法接入准备高亮框")
        target:SetAnchors(rowSlot:GetAnchors());target:SetAlignment({X=0,Y=0})
        target:SetAutoSize(false);target:SetZOrder(rowSlot:GetZOrder()-1)
        -- 沿用原性能条画刷；AC7 没有 Image:SetBrushTintColor。
        c.highlight:SetColorAndOpacity({R=1,G=1,B=1,A=.22})
        setControl(owner,false)
        c.previous=inputs(c);c.repeatAt=0
        note("NATIVE_LOADOUT_ATTACHED "..owner:GetFullName())
        self.status="sp1";self.error=nil
        return c
    end
    -- 在 MissionResupply 原有背景内双选，并列展示 SP1/SP2。
    local function attachSupply(owner,instance,planeId,profile,options)
        assert(self.phase and valid(self.phaseObject),"补给菜单缺少原生阶段绑定")
        local c={owner=owner,instance=instance,planeId=planeId,added={},masked={},moved={},texts={},lastPaint=nil,
            originalControl=owner:IsEnableControl(),flow=Flow.new(profile,options,nil,"resupply"),phase=self.phase,phaseWorld=self.phaseWorld,
            takeoffPending=false,visualRestored=false}
        self.cache=c
        c.controller=owner:GetOwningPlayer();assert(valid(c.controller),"补给页所属输入控制器缺失")
        c.world=instance:GetWorld():GetAddress();bindings(c)
        local list=controls(owner) -- 保留原生列表、光标和设备决定/返回提示。
        c.title=clone(c,assert(list.TitleText,"补给标题缺失"),false)
        c.description=clone(c,assert(list.DescriptionText,"补给说明缺失"),false)
        c.rows={};c.cursors={}
        for i=1,3 do
            c.rows[i]=clone(c,assert(list["ItemText"..(i-1)],"补给兵装行缺失"),false)
            c.cursors[i]=clone(c,assert(list["CursorText"..(i-1)],"补给原生光标缺失"),false)
            -- 原生光标位于每行 Canvas 外的 HorizontalBox 兄弟节点，直接复制会把箭头排到行末。
            -- 将副本移到对应行的 Canvas 内，用同一行的锚点和纵坐标定位，保持不同分辨率下同步。
            local rowSlot=c.rows[i].Slot
            local cursorSlot=c.cursors[i].Slot
            assert(valid(rowSlot)and valid(cursorSlot)and valid(rowSlot.Parent)and valid(cursorSlot.Parent),"补给光标缺少定位槽")
            assert(slotKind(rowSlot,c.rows[i])=="canvas","补给兵装行不是 Canvas 槽")
            assert(cursorSlot.Parent:RemoveChild(c.cursors[i]),"无法移除补给原生光标副本")
            local aligned=rowSlot.Parent:AddChild(c.cursors[i]);assert(valid(aligned),"无法接入补给对齐光标")
            local position=rowSlot:GetPosition();local size=rowSlot:GetSize()
            -- CanvasPanel 可能裁剪负坐标；先给文字让出左侧空间，再把箭头放在行内零点。
            rowSlot:SetPosition({X=position.X+28,Y=position.Y})
            rowSlot:SetSize({X=math.max(0,size.X-28),Y=size.Y})
            aligned:SetAnchors(rowSlot:GetAnchors());aligned:SetAlignment({X=0,Y=0})
            aligned:SetPosition({X=position.X,Y=position.Y});aligned:SetSize({X=26,Y=size.Y})
            aligned:SetAutoSize(false);aligned:SetZOrder(rowSlot:GetZOrder()+1)
            setText(c,c.cursors[i],">")
        end
        local tint=c.rows[1].ColorAndOpacity.SpecifiedColor -- 保留补给页自身颜色，不套机库主题。
        c.tint=color(tint.R,tint.G,tint.B,1)
        local group=assert(list.WeaponSpecifficationGroup,"补给性能组缺失")
        local video=assert(list.WeaponConceptPanel,"补给视频区域缺失")
        local typePanel=assert(list.WeaponTypePanel,"补给类型区域缺失")
        local ammoPanel=assert(list.WeaponAmmoPanel,"补给弹药区域缺失")
        local parent=group.Slot.Parent
        assert(parent:GetAddress()==video.Slot.Parent:GetAddress()and parent:GetAddress()==typePanel.Slot.Parent:GetAddress(),"补给信息区域不在同一原生容器")
        local mainSize=parent.Slot:GetSize()
        local anchors=parent.Slot:GetAnchors()
        assert(math.abs(anchors.Maximum.X-anchors.Minimum.X)<.001 and math.abs(anchors.Maximum.Y-anchors.Minimum.Y)<.001,"补给主容器尺寸模式已变化")
        local origin=video.Slot:GetPosition()
        local right=typePanel.Slot:GetPosition().X+typePanel.Slot:GetSize().X
        local gap=28 -- 两栏间距为原生逻辑单位，整体缩放仍由游戏管理。
        local width=(math.min(right,mainSize.X-20)-origin.X-gap)/2
        assert(width>250 and width<600 and mainSize.Y>450,"补给双栏可用空间不符")
        local items={} -- 直接使用已核对的 SpecName/SpecBar 原生变量，不复制 UserWidget 子树。
        for i=1,6 do
            local item=group["SpecItem"..i]
            assert(valid(item)and valid(item.SpecName)and valid(item.SpecBar)and valid(item.SpecBar.BarA),"补给性能项模板缺失")
            items[i]=item
        end
        local pitch=items[2].Slot:GetPosition().Y-items[1].Slot:GetPosition().Y
        assert(pitch>20 and pitch<60,"补给性能行距不符")
        informationColumns(c,parent,origin,width,pitch,mainSize.Y-origin.Y-20,
            {name=list.WeaponAmmo_Text,heading=assert(group.SpecGroupName),items=items,
             typeHeading=list.WeaponType_Label,type=list.WeaponType_Text,
             ammoHeading=list.WeaponAmmo_Label,ammo=list.WeaponAmmo_Text},"补给弹药（预计）")
        -- 说明仅占左侧列表下方，不再延伸至新的两栏。
        c.weaponDescription=clone(c,assert(list.WeaponDescriptionText),false)
        assert(c.weaponDescription.Slot.Parent:RemoveChild(c.weaponDescription),"无法调整补给说明位置")
        local descriptionSlot=parent:AddChild(c.weaponDescription)
        assert(valid(descriptionSlot),"无法接入补给说明")
        descriptionSlot:SetAnchors({Minimum={X=0,Y=0},Maximum={X=0,Y=0}});descriptionSlot:SetAlignment({X=0,Y=0})
        descriptionSlot:SetPosition({X=20,Y=mainSize.Y-150});descriptionSlot:SetSize({X=origin.X-40,Y=130})
        descriptionSlot:SetAutoSize(false);descriptionSlot:SetZOrder(10)
        mask(c,group);mask(c,video);mask(c,typePanel);mask(c,ammoPanel)
        setControl(owner,false)
        c.previous=inputs(c);c.repeatAt=0
        self.status="sp1";self.error=nil
        note(string.format("NATIVE_RESUPPLY_ATTACHED %s columns=%.1f pitch=%.1f",owner:GetFullName(),width,pitch))
        return c
    end
    -- 悬停更新当前栏，另一栏保留已选兵装；确认页保留完整双栏。
    local function paintSupply(c)
        local flow=c.flow;local rows=flow:rows();local stage=flow.stage
        local signature=stage..":"..flow.cursor..":"..flow.draft.a..":"..flow.draft.b..":"..tostring(flow.message)
        if signature==c.lastPaint then return end
        c.lastPaint=signature
        setText(c,c.title,stage=="sp1"and "选择 SP1 / SELECT SP WEAPON 1"or stage=="sp2"and "选择 SP2 / SELECT SP WEAPON 2"or "起飞 / TAKE OFF")
        for i=1,3 do
            local row=rows[i]
            c.rows[i]:SetVisibility(row and VISIBLE or COLLAPSED)
            c.cursors[i]:SetVisibility(row and i==flow.cursor and VISIBLE or 2)
            if row then
                setText(c,c.rows[i],row.code..(row.enabled and ""or "  未购买"))
                local tint=c.tint.SpecifiedColor
                c.rows[i]:SetColorAndOpacity(row.enabled and c.tint or color(tint.R*.45,tint.G*.45,tint.B*.45,1))
                -- 光标是新建文字控件，显式复用当前列表颜色，避免移动父 Canvas 后继承透明样式。
                c.cursors[i]:SetColorAndOpacity(c.tint)
            end
        end
        local a,b=paintColumns(c)
        local option=nil -- 最终页按 YES/NO 给说明；选择页才显示当前候选说明。
        if stage~="review"then option=flow.options[(stage=="sp1"and a or b)+1]end
        local description=stage=="review"and (flow.cursor==1 and "决定：按本次方案起飞。返回：重新选择 SP2。"or "决定：返回 SP1，重新选择特殊兵装。")
            or stage=="sp1"and "决定：选择 SP1。"or "决定：选择 SP2。返回：重选 SP1。"
        if flow.message then description=flow.message.."。"..description end
        setText(c,c.description,description)
        setText(c,c.weaponDescription,stage=="review"and (flow.cursor==2 and "变更特殊兵装 / CHANGE SPECIAL WEAPONS"or "按已选 SP1、SP2 重新起飞。")
            or option and option.description or "不搭载 SP2，SP1 保留完整载弹量。")
        self.status=stage;note("NATIVE_RESUPPLY_STAGE "..signature)
    end

    local function paint(c)
        local flow=c.flow;local rows=flow:rows();local stage=flow.stage
        local signature=stage..":"..flow.cursor..":"..flow.draft.a..":"..flow.draft.b..":"..tostring(flow.message)..":"..tostring(c.continueRequested)..":"..tostring(c.exitRequested)..":"..tostring(c.handedOff)
        if signature==c.lastPaint then return end
        c.lastPaint=signature
        local blue=color(.401978,.651406,1,1);local selected=color(1,1,1,1)
        setText(c,c.title,c.exitRequested and "取消准备 / CANCEL LOADOUT"or c.continueRequested and "原生菜单继续 / CONTINUE"
            or stage=="sp1"and "选择 SP WEAPON 1"or stage=="sp2"and "选择 SP WEAPON 2"or "确认特殊兵装 / CONFIRM LOADOUT")
        -- 交接后列表只显示操作提示，不再把已停用的编辑选项画成可选择按钮。
        if c.continueRequested or c.exitRequested then
            rows={{code=c.exitRequested and "再次返回：离开"or "再次决定：继续",enabled=true}}
        end
        for i=1,3 do
            local row=rows[i]
            c.rows[i]:SetVisibility(row and VISIBLE or COLLAPSED)
            if row then
                setText(c,c.rows[i],row.code..(row.enabled and ""or "  未购买"))
                c.rows[i]:SetColorAndOpacity(not row.enabled and color(.3,.36,.43,1)or (i==flow.cursor and selected or blue))
            end
        end
        local rowSlot=canvasSlot(c.rows[flow.cursor]);local position=rowSlot:GetPosition();local size=rowSlot:GetSize()
        local left=position.X-20 -- 底色从文字左侧留少量边距，不再依赖状态图标。
        c.highlight:SetVisibility((c.continueRequested or c.exitRequested)and COLLAPSED or VISIBLE)
        c.highlight.Slot:SetPosition({X=left-8,Y=position.Y-3});c.highlight.Slot:SetSize({X=position.X+size.X-left+16,Y=size.Y+6})
        paintColumns(c)
        local description -- 原生交接步骤明确显示，不能把编辑确认误当成出击提交。
        if c.exitRequested then description="本次草稿已取消。松开按键后，再按原生返回键离开。"
        elseif c.continueRequested then description="松开按键后，再按原生决定键继续；返回键离开本次准备。"
        elseif stage=="review"then description="上下选择修改 SP1/SP2 或继续。返回：重选 SP2。"
        elseif stage=="sp1"then description="决定：选择 SP1。返回：取消草稿，并交给原生菜单退出。"
        else description="决定：选择 SP2。返回：重选 SP1。无兵装时 SP1 保留完整载弹量。"end
        if flow.message then description=flow.message.."。"..description end
        setText(c,c.description,description);c.description:SetColorAndOpacity(blue)
        self.status=stage
        note("NATIVE_LOADOUT_STAGE "..signature)
    end
    local function pump(instance,planeId,profile,options,now,inFlight,panelOpen)
        -- F8 只暂停补给页读取，不能释放双选控制让原生单 SP 直接起飞。
        if supply and panelOpen then return end
        if (inFlight and not supply)or panelOpen then if self.cache then restore(not inFlight)end;return end
        local c=self.cache
        -- 原生 YES 已发出后，页面隐藏/停用属于正常关闭过程；保留草稿到
        -- 任务成功事件。不能因 IsVisible 变为 false 就丢弃确认事务或重新接入。
        if supply and c and c.handedOff then self.status="awaiting_success";return end
        -- 实机原生 C++ 确认不经过反射提交钩子；真实决定和旧页关闭共同确认。
        -- 返回、F8、换机体/世界或仅进入确认页都不能保存草稿。
        if not supply and c and valid(c.owner)and valid(c.controller)and c.handedOff and not c.exitRequested
            and c.planeId==planeId and instance:GetWorld():GetAddress()==c.world
            and(not c.owner:IsVisible()or not c.owner:IsEnableControl())
            and(mappedDown(c,c.accept)or(c.commitArmedUntil and now<=c.commitArmedUntil))then
            local prepared=c.flow:profile() -- 通过原生决定离开后才保存双选结果。
            self.applying=true
            local ok,err=pcall(function()instance:SetPlaneAndSWP(c.planeId,prepared.a)end)
            self.applying=false;assert(ok,err)
            assert(instance:GetSelectedPlaneSWP()==prepared.a,"原生准备 SP1 未按确认方案保存")
            commit(c.planeId,copyProfile(prepared))
            self.ignored=c.owner:GetAddress();self.ignoredReason="committed";restore(false);self.status="committed"
            note("NATIVE_LOADOUT_COMMITTED_NATIVE_CLOSE "..c.planeId)
            return
        end
        if c and (not valid(c.owner)or not valid(c.controller)or not c.owner:IsVisible()or c.planeId~=planeId or instance:GetWorld():GetAddress()~=c.world)then
            restore(false);c=nil
        end
        if not c then
            if now-self.lastScan<.25 then return end
            self.lastScan=now
            local candidate=nil
            for _,owner in ipairs(FindAllOf(supply and "MissionResupplyWidget"or "CampaignHangarWeaponWidget")or{})do
                if valid(owner)and owner:IsVisible()and owner:IsEnableControl()and owner:GetWorld():GetAddress()==instance:GetWorld():GetAddress()
                    and not owner:GetFullName():find("WidgetArchetype",1,true)and not owner:GetFullName():find("Default__",1,true)then candidate=owner;break end
            end
            if not candidate then self.ignored=nil;self.ignoredReason=nil;if not self.error then self.status="waiting"end;return end
            if candidate:GetAddress()==self.ignored then return end
            local choices=options();if #choices~=3 then return end
            self.ignored=candidate:GetAddress();self.ignoredReason="error" -- 接入失败只记录一次，不在每次轮询重试。
            if not panelClass then api()end
            c=(supply and attachSupply or attach)(candidate,instance,planeId,profile(),choices)
            self.ignored=nil;self.ignoredReason=nil
        end
        local input=inputs(c)
        if supply then
            -- 等待决定键释放，再执行原生页面 YES；原生阶段结束通知只作为观察事件。
            if c.takeoffPending then
                if input.accept or input.cancel or input.up or input.down then
                    c.previous=input;self.status="awaiting_native";return
                end
                assert(valid(self.phaseObject)and self.phaseObject:GetAddress()==c.phase,"原生补给阶段已变化")
                c.handedOff=true;c.takeoffPending=false
                -- 在调用前暂存，覆盖原生同步回调与异步关闭两种时序；只有任务成功才保存。
                self.transaction={planeId=c.planeId,profile=c.flow:profile(),phase=c.phase}
                self.status="awaiting_success"
                note("NATIVE_RESUPPLY_TAKEOFF_DISPATCHED "..c.planeId)
                restoreVisual(c)
                note("NATIVE_RESUPPLY_NATIVE_VISUAL_RESTORED "..c.planeId)
                -- 不重新开放原生输入，否则原生单 SP 页会消费随后按键并改变挂载。
                -- 真正 YES 负责武器收尾、关闭 UI 栈，再通知 MiniGamePhase。
                takeoff(c.owner)
                note("NATIVE_RESUPPLY_YES_RETURNED "..c.planeId)
                return
            end
            local direction=input.up and not input.down and -1 or input.down and not input.up and 1 or 0
            local oldDirection=c.previous.up and not c.previous.down and -1 or c.previous.down and not c.previous.up and 1 or 0
            if direction~=0 and(direction~=oldDirection or now>=c.repeatAt)then
                c.flow:move(direction);c.repeatAt=now+(direction~=oldDirection and .35 or .12)
            elseif input.cancel and not c.previous.cancel then
                if c.flow.stage=="sp1"then c.flow.message="请完成 SP1、SP2 选择后确认起飞"
                else c.flow:back()end
            elseif input.accept and not c.previous.accept then
                local result=c.flow:confirm()
                if result=="takeoff"then
                    -- 只有本事务的真实 YES 输入可以请求桥接；NO 和返回继续编辑。
                    assert(valid(self.phaseObject)and self.phaseObject:GetAddress()==c.phase,"原生补给阶段已变化")
                    c.takeoffPending=true
                    note("NATIVE_RESUPPLY_TAKEOFF_REQUESTED "..c.planeId)
                    -- 保持旧菜单禁用，避免同一个按键再被原生单 SP 处理器消费。
                    c.previous=input;self.status="awaiting_native";return
                end
            end
            c.previous=input;paintSupply(c);return
        end
        if c.continueRequested or c.exitRequested then
            -- 决定/返回必须释放后才能交接，避免同一个按键被两套处理器重复读取。
            if not c.handedOff and not input.accept and not input.cancel and not input.up and not input.down then
                c.handedOff=true;setControl(c.owner,true)
                note(c.exitRequested and "NATIVE_LOADOUT_NATIVE_EXIT_READY"or "NATIVE_LOADOUT_NATIVE_CONTINUATION_READY")
            elseif c.handedOff then
                if not c.exitRequested and input.accept and not c.previous.accept then c.commitArmedUntil=now+1 end
                if not c.owner:IsEnableControl()then
                    self.ignored=c.owner:GetAddress();self.ignoredReason="closed"
                    restore(false);self.status="waiting";return
                end
            end
        else
            local direction=input.up and not input.down and -1 or input.down and not input.up and 1 or 0
            local oldDirection=c.previous.up and not c.previous.down and -1 or c.previous.down and not c.previous.up and 1 or 0
            if direction~=0 and(direction~=oldDirection or now>=c.repeatAt)then
                c.flow:move(direction);c.repeatAt=now+(direction~=oldDirection and .35 or .12)
            elseif input.cancel and not c.previous.cancel then
                if c.flow.stage=="sp1"then
                    c.exitRequested=true -- 保留双栏直到原生退出，取消草稿不暴露旧单 SP 显示。
                    note("NATIVE_LOADOUT_DRAFT_CANCELLED "..c.planeId)
                else c.flow:back()end
            elseif input.accept and not c.previous.accept then
                c.continueRequested=c.flow:confirm()=="continue"or nil
            end
        end
        c.previous=input;paint(c)
    end
    function self:pump(...)
        local args={...};local ok,err=xpcall(function()pump(table.unpack(args))end,debug.traceback)
        if not ok then fail(err)end
    end
    function self:reset(clearError,releaseInput)
        local ok,err=pcall(function()restore(releaseInput)end)
        if not ok then
            note((supply and "NATIVE_RESUPPLY_RESTORE_ERROR "or "NATIVE_LOADOUT_RESTORE_ERROR ")..tostring(err))
            if supply then error(err)end
        end
        self.ignored=nil;self.ignoredReason=nil;if clearError~=false then self.error=nil end
        self.phase=nil;self.phaseObject=nil;self.phaseWorld=nil
        self.status=self.error and "error"or "waiting";self.lastScan=-1
    end
    -- 原生重新启用同一菜单对象时清除关闭标记；模组自己的输入操作不算重进。
    function self:onControlChanged(ctx,enabled)
        if supply or self.controlDepth>0 then return end
        local owner=ctx:get();local address=owner:GetAddress();local c=self.cache
        if (not c or c.owner:GetAddress()~=address)and address~=self.ignored then return end
        local ok,err=xpcall(function()
            if self.transaction then return end -- 原生提交的前后回调之间不能丢掉待保存事务。
            if c and c.owner:GetAddress()==address then
                -- 原生先停用旧页再提交选择；真实决定期间保留事务来源，不能提前清掉草稿。
                if c.handedOff and not c.exitRequested and not enabled:get()and mappedDown(c,c.accept)then
                    c.nativeClosing=true;note("NATIVE_LOADOUT_NATIVE_DECIDE_CLOSING");return
                end
                restore(false);self.ignored=address;self.ignoredReason="closed"
            end
            if enabled:get()and self.ignoredReason~="error"then
                self.ignored=nil;self.ignoredReason=nil;self.lastScan=-1;self.status="waiting"
                note("NATIVE_LOADOUT_REACTIVATED "..owner:GetFullName())
            end
        end,debug.traceback)
        if not ok then fail(err)end
    end
    function self:beforeNativeCommit(ctx,selectedPlane,selectedWeapon)
        if self.applying then return end -- 自己按已确认方案写入 SP1，不建立重复事务。
        local c=self.cache
        if not c or not c.handedOff or ctx:get():GetAddress()~=c.instance:GetAddress()or selectedPlane:get()~=c.planeId then return end
        if c.exitRequested then
            -- 交回原生退出后误按决定，也不能将未确认的草稿写入原生 SP1 或模组配置。
            selectedWeapon:set(c.originalWeapon);note("NATIVE_LOADOUT_CANCELLED_NATIVE_DECIDE "..c.planeId);return
        end
        if c.flow.stage~="review"then return end
        local ok,err=xpcall(function()
            assert(IsInGameThread(),"Native preparation commit requires the game thread")
            -- SetPlaneAndSWP is a shared native API, not proof of a button
            -- confirmation. Ignore preview/hover writes unless the user has
            -- pressed the native decide binding. Allow a short native delay
            -- after an observed edge; never save merely for entering review.
            if not mappedDown(c,c.accept)and not(c.commitArmedUntil and os.clock()<=c.commitArmedUntil)then return end
            local profile=c.flow:profile()
            selectedWeapon:set(profile.a) -- Never let the stock one-SP choice overwrite SP1.
            -- 补给可能共用此单 SP 接口，只对齐 SP1；提交仍等待原生结束及成功事件。
            if supply then return end
            -- 共享 setter 只对齐 SP1；保存由原生决定后关闭的协议负责。
        end,debug.traceback)
        if not ok then fail(err)end
    end
    -- 由已核对的原生补给开始事件绑定阶段，不靠飞行输入是否关闭来猜测。
    function self:beginSupplyMenu(phase)
        assert(supply and valid(phase),"无效的补给菜单阶段")
        assert(not self.transaction,"上一轮补给确认事务尚未完成")
        local phaseWorld=phase:GetWorld() -- 本机加载器的 Actor 查询可能返回返航子关卡世界。
        assert(valid(phaseWorld),"补给菜单阶段没有有效关卡世界")
        self:reset();self.phase=phase:GetAddress();self.phaseObject=phase
        self.phaseWorld=phaseWorld:GetAddress()
        note("NATIVE_RESUPPLY_PHASE_BOUND "..phase:GetFullName().." level_world="..phaseWorld:GetFullName())
    end
    -- 开始/结束必须来自同一阶段实例，不能在结束回调里改绑到任意新对象。
    -- 阶段的子关卡世界与 GameInstance 的任务世界分别验证，不要求两者地址相等。
    local function validateSupplyPhase(c,phase)
        assert(supply and c and valid(phase),"补给确认缺少原生阶段对象")
        local address=phase:GetAddress() -- 只接受本轮菜单开始事件绑定的阶段。
        assert(address==c.phase and address==self.phase and valid(self.phaseObject),"补给结束事件不是本轮菜单的阶段")
        local phaseWorld=phase:GetWorld() -- 与自身开始时的关卡归属比较。
        assert(valid(phaseWorld)and phaseWorld:GetAddress()==c.phaseWorld,"补给阶段关卡已变化")
        assert(valid(c.instance),"补给所属游戏实例已销毁")
        local taskWorld=c.instance:GetWorld() -- 任务身份仍严格保持，不放行跨世界事务。
        assert(valid(taskWorld)and taskWorld:GetAddress()==c.world,"补给期间运行任务世界已变化")
        return address
    end
    -- 只捕获真实原生确认事件中的草稿；悬停、视频预览和页面关闭不能提交。
    function self:beforeSupplyFinish(ctx)
        local c=self.cache
        local phase=ctx:get()
        assert(c and c.handedOff and self.transaction,"原生结束通知早于双 SP 的 YES 请求")
        assert(validateSupplyPhase(c,phase)==self.transaction.phase,"补给结束事件与 YES 事务不匹配")
        assert(c.flow.stage=="review","补给未完成 SP1/SP2 选择")
        assert(c.flow.cursor==1,"只有起飞 YES 才能提交补给方案")
        -- 结束通知可能在动画后到达，不要求玩家继续按住决定键，也不设置一秒的提交窗口。
        note("NATIVE_RESUPPLY_FINISH_VALIDATED "..c.planeId)
    end
    -- 原生结束入口返回后只保留事务和控件，不能在原生菜单收尾期间移除控件或改写武器。
    function self:afterSupplyFinish(ctx)
        local transaction=self.transaction
        local phase=ctx:get()
        assert(transaction and validateSupplyPhase(self.cache,phase)==transaction.phase,"补给确认事务丢失")
        transaction.finished=true -- 结束函数已返回；后续任务成功事件才能提交草稿。
        self.status="awaiting_success"
        note("NATIVE_RESUPPLY_FINISH_ACCEPTED "..transaction.planeId)
    end
    -- 只有任务报告补给成功后才提交配置并释放临时控件，避免原生收尾阶段访问已移除的控件。
    function self:completeSupply()
        local transaction=self.transaction;local c=self.cache
        assert(supply and transaction and transaction.finished and c,"补给成功时没有完成原生结束通知的 YES 事务")
        local profile=copyProfile(transaction.profile)
        commit(transaction.planeId,profile)
        if valid(c.owner)then self.ignored=c.owner:GetAddress()end
        restore(false);self.status="committed"
        note("NATIVE_RESUPPLY_CONFIRMED "..transaction.planeId)
        return profile
    end
    -- 管理界面和日志可据此区分普通补给编辑与原生起飞交接阶段。
    function self:isSupplyHandoffPending()
        local c=self.cache
        return supply and c and (c.takeoffPending or c.handedOff or self.transaction~=nil) or false
    end
    return self
end
return NativeLoadout
