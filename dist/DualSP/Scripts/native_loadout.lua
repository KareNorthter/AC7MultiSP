-- 在 CampaignHangarWeapon 和 MissionResupply 各自的原生页面内改造多槽选择。
-- 不创建外部窗口、不模拟按键；页面跳转继续交给 AC7 原生菜单。
local path=debug.getinfo(1,"S").source:gsub("^@",""):gsub("\\","/")
local Flow=dofile(assert(path:match("^(.*)/[^/]+$")).."/loadout_flow.lua")
local Profile=dofile(assert(path:match("^(.*)/[^/]+$")).."/loadout_profile.lua") -- 预览与核心共用百分比及向上取整规则。
local NativeLoadout={}
local TRANSIENT,VISIBLE,COLLAPSED=0x40,0,1
local function valid(o)return o and o:IsValid()end
local function vec(v)return {X=v.X,Y=v.Y}end
local function key(name)return {KeyName=FName(name)}end
local function color(r,g,b,a)return {SpecifiedColor={R=r,G=g,B=b,A=a},ColorUseRule=0}end
local copyProfile=Profile.copy -- 交接快照包含第三槽。

-- 类型对应原生 WeaponConceptText 的键；不再把兵装全名放进 TYPE。
local weaponTypes={Atalom="AIR TO AIR LOCK-ON MISSILE",Atglom="AIR TO GROUND LOCK-ON MISSILE",
    Atglob="AIR TO GROUND LOCK-ON BOMB",Atglor="AIR TO GROUND LOCK-ON ROCKET",
    Ataglom="AIR TO AIR/GROUND LOCK-ON MISSILE",Ataglou="AIR TO AIR/GROUND LOCK-ON UAV",
    Rocket="ROCKET",Bomb="BOMB",Laser="LASER",Other="OTHER"}

-- 补给适配器复用原生控件操作和输入读取，保持两种页面各自的布局。
function NativeLoadout.new(dependencies)
    local note,commit,mode=dependencies.note,dependencies.commit,dependencies.mode
    local keyboardNavigation,takeoff=dependencies.keyboardNavigation,dependencies.takeoff
    local preparationAction,confirmSound=dependencies.preparationAction,dependencies.confirmSound
    local weaponIcon,translate,partsAmmoPreview=dependencies.weaponIcon,dependencies.translate,dependencies.partsAmmoPreview
    local ammoSettings=assert(dependencies.ammoSettings,"缺少弹药比例设置接口")
    local mountPreview,releaseMountPreview=dependencies.mountPreview,dependencies.releaseMountPreview
    assert(mode=="preparation"or mode=="resupply"or mode=="standby"or mode=="parts","未知原生页面模式")
    if mode=="preparation"or mode=="resupply"then assert(type(commit)=="function","交互页面缺少提交接口")end
    local tr=assert(translate,"缺少原生页面语言提供器") -- 页面语言由游戏文本文化决定。
    -- 每个适配器独立保存草稿、控件与原生确认事务。
    local supply=mode=="resupply"
    local parts=mode=="parts" -- PART 页复用右栏只读展示，不接管部件选择。
    if parts then assert(type(partsAmmoPreview)=="function","PART 缺少只读弹药预览接口")end
    local standby=mode=="standby" or parts -- 两个只读页面均展示已保存方案。
    assert(type(keyboardNavigation)=="function","缺少原生页面键盘导航接口")
    if supply then assert(type(takeoff)=="function","缺少原生补给 YES 桥接")end
    if not supply and not standby then assert(type(preparationAction)=="function","缺少机库一步确认/返回桥接")end
    if not supply and not standby then assert(type(weaponIcon)=="function","缺少机库分类图标桥接")end
    if not standby then assert(type(confirmSound)=="function","缺少原生菜单确认音接口")end
    local self={cache=nil,status="waiting",error=nil,lastScan=-1,ignored=nil,ignoredReason=nil,controlDepth=0,serial=0,transaction=nil,phase=nil,phaseObject=nil,
        phaseWorld=nil,owners={},ownerWorld=nil,lastDiscovery=-1} -- 页面候选与阶段身份分别缓存。
    local function releasePreview(c)
        if not c or not c.mountOwner then return end
        local ownerAddress=c.mountOwner;c.mountOwner=nil
        if releaseMountPreview then
            local ok,err=pcall(releaseMountPreview,ownerAddress)
            if not ok then note("NATIVE_MOUNT_PREVIEW_RELEASE_WARNING "..tostring(err))end
        end
    end
    local function refreshPreview(context)
        local c=self.cache
        if not c or not mountPreview then return end
        if context.panelOpen then return end -- 管理页暂借控制，不视为页面退出。
        -- 两个只读页面可能同时可见，只有当前开放控制的一页可以取得预览。
        -- YES 后原生隐藏页面属于起飞过渡，仍保留已经确认的模型。
        local held=supply and c.handedOff
        local called,err=pcall(function()
            if not held and (not valid(c.owner)or not c.owner:IsVisible()
                or standby and not c.owner:IsEnableControl())then releasePreview(c);return end
            c.mountOwner=c.owner:GetAddress()
            mountPreview{owner=c.owner,ownerAddress=c.mountOwner,instance=c.instance,planeId=c.planeId,mode=mode,
                profile=held and self.transaction and copyProfile(self.transaction.profile)or c.flow:preview(),options=c.flow.options}
        end)
        if not called then
            releasePreview(c)
            local warning=tostring(err)
            if warning~=self.mountWarning then note("NATIVE_MOUNT_PREVIEW_WARNING "..warning)end
            self.mountWarning=warning -- 不进入页面 fail；原生文字、输入和提交继续运行。
        else self.mountWarning=nil end
    end
    -- 对象发现与可见性检查分开：平时只检查少量页面，不每 50 ms 扫描全局对象表。
    local function owners(instance,now)
        local world=instance:GetWorld():GetAddress() -- 换世界后立即重新发现，不能复用旧页面。
        if self.ownerWorld~=world then
            self.owners={};self.ownerWorld=world;self.lastDiscovery=-1
        end
        if self.eventOwner then return {self.eventOwner}end -- 激活事件已经给出准确对象，无需查找。
        if self.modelWait then return {self.modelWait.owner}end -- 等待已绑定模型期间不每 500 ms 重新扫描页面；pump 已核对身份。
        local interval=supply and .02 or .5 -- 补给仅在短暂等待页面时快速发现，飞行中不调用。
        if self.lastDiscovery<0 or now-self.lastDiscovery>=interval then
            local className=supply and "MissionResupplyWidget"or parts and "CampaignHangarPartsWidget"
                or standby and "CampaignHangarReviewWidget"or "CampaignHangarWeaponWidget"
            self.owners=FindAllOf(className)or {};self.lastDiscovery=now
        end
        return self.owners
    end
    -- 选装页交接时暂存私有根的显隐；只读 STANDBY/PART 不接管整页显隐。
    local function releaseClosedRoot()
        local entry=self.closedRoot
        if entry and valid(entry.root)then entry.root:SetVisibility(entry.visibility)end
        self.closedRoot=nil
    end
    -- 退出动画尚在绘制时，先隐藏此页私有根，才允许还原原生信息。
    local function hideClosedRoot(c)
        releaseClosedRoot()
        if valid(c.owner)and valid(c.owner.WidgetTree)and valid(c.owner.WidgetTree.RootWidget)then
            local root=c.owner.WidgetTree.RootWidget -- 不影响游戏共享菜单前景。
            self.closedRoot={owner=c.owner:GetAddress(),root=root,visibility=root:GetVisibility()} -- 选装页重进需核对原页面身份。
            root:SetVisibility(COLLAPSED)
        end
    end
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
            "/Script/Nimbus.UIBaseWidget:GetUIManagerActor",
            "/Script/UMG.Widget:SetVisibility","/Script/UMG.Widget:GetVisibility","/Script/UMG.Widget:GetOwningPlayer","/Script/UMG.Widget:IsVisible","/Script/UMG.Image:SetBrush",
            "/Script/UMG.Image:SetColorAndOpacity","/Script/UMG.TextBlock:SetText","/Script/UMG.TextBlock:SetFont","/Script/UMG.UserWidget:SetColorAndOpacity",
            "/Script/UMG.Image:SetGlowColor","/Script/UMG.Image:SetUseGlobalGlowColor","/Script/UMG.Image:SetBrushFromMaterial",
            "/Script/UMG.TextBlock:SetGlowColor","/Script/UMG.TextBlock:SetUseGlobalGlowColor",
            "/Script/Nimbus.LocalizationManager:HasString","/Script/Nimbus.LocalizationManager:LocalizeString",
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
    local function preparationTitle(stage)
        return "SELECT SP WEAPON "..assert(stage:match("^sp([123])$"),"准备页兵装标题阶段无效")
    end
    -- 原生文字模板含隐藏动画的内部状态，新增文字从干净对象初始化，只复制显示样式。
    local function initializeLeaf(widget,source,isImage,initialText)
        -- AC7 自定义的局部泛光不能只靠字体颜色继承。
        widget:SetGlowColor(source.GlowColor);widget:SetUseGlobalGlowColor(source.bUseGlobalGlowColor)
        if isImage then
            widget.bIsImageHidingEnable=false;widget:SetBrush(source.Brush)
            widget.bBlendUsePreMultiplyAlpha=source.bBlendUsePreMultiplyAlpha
        else
            local font=source.Font
            assert(font.Size>0,"准备页原生字体尚未初始化："..source:GetFullName())
            widget.bIsTextHidingEnable=false
            assert(valid(font.FontObject),"准备页字体资源缺失："..source:GetFullName())
            widget:SetFont(font)
            widget.bAutoWrapText=source.bAutoWrapText;widget.WrapTextAt=source.WrapTextAt
            widget.Justification=source.Justification
            widget.LineHeightPercentage=source.LineHeightPercentage
            widget:SetText(initialText~=nil and FText(initialText)or source.Text)
        end
    end
    local function clone(c,source,isImage,retainSource,wrapWidth,initialText)
        assert(valid(source)and valid(source.Slot),"Native preparation source is missing")
        local oldSlot=source.Slot;local parent=oldSlot.Parent
        local layout=readLayout(oldSlot,source)
        self.serial=self.serial+1
        local widget=StaticConstructObject(source:GetClass(),c.owner.WidgetTree,FName("DualSPPreparation_"..self.serial),TRANSIENT,0,false,false)
        assert(valid(widget),"Cannot construct native preparation control")
        widget.Slot=nil
        c.added[#c.added+1]=widget -- Rollback also owns a partially attached clone.
        initializeLeaf(widget,source,isImage,initialText)
        if not isImage and initialText~=nil then c.texts[widget:GetAddress()]=initialText end
        if wrapWidth then
            -- 必须在首次 AddChild 构建 Slate 前写入换行宽度，单改已挂载控件字段不会同步。
            widget.bAutoWrapText=true;widget.WrapTextAt=wrapWidth;widget.Justification=0
        end
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
        for _,entry in ipairs(c.repositioned or {})do
            if valid(entry.widget)and valid(entry.widget.Slot)then applyLayout(entry.widget.Slot,entry.layout,entry.widget)end
        end
        for _,widget in ipairs(c.added)do
            if valid(widget)and valid(widget.Slot)and valid(widget.Slot.Parent)then
                assert(widget.Slot.Parent:RemoveChild(widget),"无法释放原生页面克隆控件")
            end
        end
        c.visualRestored=true
    end
    local function showModelWait(pending)
        -- 过渡期间只展示等待状态，不能先露出原生单 SP 列表/参数再跳成三栏。
        if standby and self.cache then return end -- 已存在的三栏维持到资料就绪，不退回原生单栏。
        if not panelClass then api()end
        local view={owner=pending.owner,added={},masked={},moved={},texts={}} -- 复用现有控件所有权与归还流程。
        pending.view=view
        local info -- 各页面仅遮蔽兵装区域，机体背景、导航和整页根节点仍由游戏管理。
        if standby then
            if parts then info=pending.owner.AircraftWeaponInformation else info=pending.owner.ReviewInformation end
            assert(valid(info),"等待中的只读兵装信息区缺失")
            local ammo=info.HangarAircraftBoardWeaponsAmmo -- 保留 GUN/MSL 等其他行，只替换原 SP 行的文字。
            assert(valid(ammo)and valid(ammo.WeaponsSP1),"等待中的只读弹药模板缺失")
            local row=controls(ammo.WeaponsSP1) -- 不显示尚未确认的名称或弹数。
            setText(view,clone(view,assert(row.WeaponsName),false),"SP1 / SP2 / SP3")
            setText(view,clone(view,assert(row.WeaponsAmmo),false),"…")
            mask(view,info.HangarSpecGroupR)
            mask(view,info.HangarAircraftWeaponNameShortR)
            mask(view,info.HangarAircraftWeaponType)
            return
        end
        local base=pending.owner.LightMenuBaseWidget -- 列表标题、说明与三条原生兵装行。
        assert(valid(base),"等待中的原生兵装列表缺失")
        local list=controls(base)
        -- 等待与正式选装使用同一个SP1标题，挂入页面前就写好，不额外显示三槽占位标题。
        clone(view,assert(list.TitleText),false,nil,nil,preparationTitle("sp1"))
        setText(view,clone(view,assert(list.DescriptionText),false),tr("正在加载兵装，请稍候……","Loading weapons. Please wait..."))
        for index=0,2 do mask(view,assert(list["ItemText"..index]))end
        info=pending.owner.WeaponInformation
        assert(valid(info)and valid(info.HangarAircraftAllWeaponsAmmo),"等待中的兵装信息区缺失")
        local ammo=info.HangarAircraftAllWeaponsAmmo -- 两条 SP 占位，不展示原单 SP 内容或猜测载量。
        for index=1,3 do
            local row=controls(ammo["WeaponsSP"..index])
            setText(view,clone(view,assert(row.WeaponsName),false),"SP"..index)
            setText(view,clone(view,assert(row.WeaponsAmmo),false),"…")
        end
        mask(view,info.HangarSpecGroupR)
        mask(view,info.HangarAircraftWeaponNameLong)
        mask(view,info.HangarAircraftWeaponProperties)
        mask(view,info.HangarAircraftWeaponType)
        mask(view,pending.owner.LightMenuItemIconStatusWidget)
        mask(view,pending.owner.LightMenuItemIconCategoryWidget)
        local manager=pending.owner:GetUIManagerActor() -- 原金色选择光标不能停留在已遮蔽的单选列表上。
        assert(valid(manager)and valid(manager.MenuForegroundWidget),"等待中的原生前景选择器缺失")
        mask(view,manager.MenuForegroundWidget.ScrollCanvas)
    end
    local function clearModelWait(releaseInput)
        -- 就绪时在同一回调内归还模板并接入三栏；只有离页或明确失败才恢复原生显示。
        local pending=self.modelWait -- 等待记录绑定具体页面、机体及世界。
        if not pending then return end
        self.modelWait=nil
        restoreVisual(pending.view)
        if pending.inputOwned and releaseInput~=false and valid(pending.owner)and pending.owner:IsVisible()then
            setControl(pending.owner,pending.originalControl)
        end
    end
    local function modelChoices(owner,instance,planeId,options,now)
        -- 未就绪是有限等待状态；只在模型身份一致后建立多槽选择，真实组件/资源异常仍由原错误路径处理。
        local started=os.clock() -- 仅测本次 Lua 同步调用；不当作整帧或模型资源加载耗时。
        local choices,reason=options() -- 第二返回值仅表示原生预览尚未跟上当前选择。
        self.optionsCost=(os.clock()-started)*1000 -- 最后一次选项查询耗时，接入成功时汇总一次。
        if not reason then
            if self.modelWait then
                note(string.format("NATIVE_MODEL_READY plane=%d lua_clock_wait_ms=%.3f",planeId,(now-self.modelWait.started)*1000))
                clearModelWait(true)
            end
            return choices
        end
        assert(not supply,"补给不允许等待机库模型："..reason)
        if not self.modelWait then
            local pending={owner=owner,planeId=planeId,world=instance:GetWorld():GetAddress(),started=now} -- 当前等待上下文。
            self.modelWait=pending
            if not standby then
                pending.originalControl=owner:IsEnableControl();pending.inputOwned=true
                setControl(owner,false) -- 等待期间不允许原生单 SP 消费确认键。
            end
            showModelWait(pending)
            note("NATIVE_MODEL_WAIT plane="..planeId.." "..reason)
        end
        self.status="waiting_model"
        assert(now-self.modelWait.started<10,"机体兵装等待超过 10 秒，请检查模型并重新选机："..reason)
        return nil
    end
    -- 一次接入只写一条分段记录，便于把选项计算和控件集中创建分开观察。
    local function measuredAttach(create,owner,instance,planeId,profile,choices)
        local started=os.clock() -- 与既有 PERF_TICK 相同的 Lua 时钟，仅代表同步调用范围。
        local c=create(owner,instance,planeId,profile,choices) -- 保留原接入顺序与错误传播。
        c.mountOwner=owner:GetAddress()
        note(string.format("NATIVE_UI_ATTACH_COST mode=%s plane=%d options_ms=%.3f build_ms=%.3f",mode or "preparation",
            planeId,self.optionsCost or 0,(os.clock()-started)*1000))
        return c
    end
    local function restore(releaseInput,keepPreview)
        clearModelWait(releaseInput)
        local c=self.cache;if not c then return end
        if not keepPreview then releasePreview(c)end
        if c.actionQueued then
            local ok,_,err=preparationAction(c.actionWidget,0,-1)
            assert(ok,err);c.actionQueued=false
        end
        restoreVisual(c)
        -- Once handed back, the native menu owns its enable/disable lifecycle.
        -- In particular, never reactivate a menu that is leaving the screen.
        if not standby and releaseInput~=false and not c.handedOff and valid(c.owner)then setControl(c.owner,c.originalControl)end
        self.cache=nil;self.transaction=nil
    end
    local function fail(err)
        local c=self.cache;self.ignored=c and c.owner:GetAddress()or self.ignored
        self.ignoredReason="error"
        self.error=tostring(err);self.status="error";note((supply and "NATIVE_RESUPPLY_ERROR "or "NATIVE_LOADOUT_ERROR ")..self.error)
        local ok,why=pcall(restore);if not ok then note("NATIVE_LOADOUT_RESTORE_ERROR "..tostring(why))end
        local released,releaseErr=pcall(releaseClosedRoot)
        if not released then note("NATIVE_LOADOUT_ROOT_RESTORE_ERROR "..tostring(releaseErr))end
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
    -- 三栏采用原生文字/图片模板的叶控件，空 Canvas 不复制任何 Slots 或子树。
    local function infoWidget(c,source,parent,position,size,isImage,wrap)
        assert(valid(source),"兵装信息模板缺失")
        self.serial=self.serial+1
        local widget=StaticConstructObject(source:GetClass(),c.owner.WidgetTree,FName("DualSPWeaponInfo_"..self.serial),TRANSIENT,0,false,false)
        assert(valid(widget),"无法创建兵装信息控件")
        widget.Slot=nil;c.added[#c.added+1]=widget
        initializeLeaf(widget,source,isImage)
        if wrap then
            -- 新建全名、类型先设左对齐和栏宽，再交给原生控件建立实际排版。
            widget.Justification=0;widget.bAutoWrapText=true;widget.WrapTextAt=size.X
        end
        if c.glow then widget:SetGlowColor(c.glow);widget:SetUseGlobalGlowColor(false)end
        local slot=parent:AddChild(widget);assert(valid(slot),"无法接入兵装信息控件")
        slot:SetAnchors({Minimum={X=0,Y=0},Maximum={X=0,Y=0}});slot:SetAlignment({X=0,Y=0})
        slot:SetPosition(position);slot:SetSize(size);slot:SetAutoSize(false);slot:SetZOrder(isImage and 0 or 1)
        widget:SetVisibility(VISIBLE)
        if isImage then
            widget.bIsImageHidingEnable=false
        else
            widget:SetColorAndOpacity(c.tint);widget.bIsTextHidingEnable=false
            local font=widget.Font -- 三槽信息字体，字号不随原生候选动画漂移。
            font.Size=math.min(font.Size,18);widget:SetFont(font)
        end
        return widget
    end
    -- 三类页面共用较大的槽位标题；向上留出字高，避免挤压原有的全名或第一行性能。
    local function informationTitle(c,source,parent,width)
        local title=infoWidget(c,source,parent,{X=0,Y=-8},{X=width,Y=38},false) -- 底边仍为 30，正文位置不变。
        local font=title.Font -- 保留原生字体、颜色及泛光，只把标题从最多 18 放大到 24。
        font.Size=24;title:SetFont(font)
        return title
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
    -- 选装弹药行与原生武器索引脱钩：复制几何到同级干净 Canvas，不继承原生行的选中颜色。
    local function preparationAmmoRow(c,source)
        assert(valid(source)and valid(source.WidgetTree),"准备页弹药行缺失")
        local root=source.WidgetTree.RootWidget -- 原生行根 Canvas 内还有承载名称/数量的 CanvasPanel_2。
        assert(valid(root)and root:IsA(canvasPanelClass),"准备页弹药行根布局已变化")
        local rowSlot=canvasSlot(source) -- 行在弹药总表中的位置，保留原生缩放、锚点和行距。
        local rowLayout=readLayout(rowSlot,source)
        local panel=infoCanvas(c,rowSlot.Parent,rowLayout.position,rowLayout.size) -- 自有容器不受原生逐行高亮驱动。
        applyLayout(panel.Slot,rowLayout,panel,10)
        local panels={[root:GetAddress()]=panel} -- 两个文字共用原生内层 Canvas，不能把局部坐标直接当行根坐标。
        local function parentCanvas(original,depth)
            assert(valid(original)and depth<=16,"准备页弹药文字父层级无效或过深")
            local address=original:GetAddress()
            if panels[address]then return panels[address]end
            assert(original:IsA(canvasPanelClass),"准备页弹药文字中间容器不是 Canvas："..original:GetFullName())
            local slot=original.Slot
            assert(valid(slot)and valid(slot.Parent),"准备页弹药文字不属于行根："..original:GetFullName())
            assert(slotKind(slot,original)=="canvas","准备页弹药文字中间 Canvas 定位已变化")
            local parent=parentCanvas(slot.Parent,depth+1) -- 先确认能追溯到本行根，再重建空容器。
            local layout=readLayout(slot,original)
            local copy=infoCanvas(c,parent,layout.position,layout.size)
            applyLayout(copy.Slot,layout,copy)
            panels[address]=copy
            return copy
        end
        local widgets=controls(source) -- 仅用源叶控件的字体和几何，不复制颜色状态或动画。
        local result={} -- 同时持有名称和数量，后续一起更新高亮。
        for field,name in pairs({name="WeaponsName",count="WeaponsAmmo"})do
            local template=assert(widgets[name],"准备页弹药文字缺失："..name)
            local leafSlot=canvasSlot(template) -- 运行时文字包装层的外侧 Canvas 槽。
            local parent=parentCanvas(leafSlot.Parent,1) -- 沿实际中间 Canvas 保留各层布局。
            local leafLayout=readLayout(leafSlot,template) -- 克隆到新 Canvas 后仍使用相同的局部坐标。
            local text=infoWidget(c,template,parent,leafLayout.position,leafLayout.size,false)
            applyLayout(text.Slot,leafLayout,text,10)
            text:SetFont(template.Font) -- 弹药保持原生 20 号，不沿用性能信息的 18 号限制。
            result[field]=text
        end
        mask(c,source) -- 原生仍可更新原行，但其颜色只影响保留在折叠容器内的源对象。
        return result
    end
    -- 复用原生基础条与三条细边框；空白部分保持透明，不铺满宽度的底色。
    local function performanceBar(c,source,parent,position,width)
        assert(valid(source)and valid(source.BarA),"原生性能基础条缺失")
        local bottom,left,right=source.BaseImage1,source.BaseImage2,source.BaseImage3 -- 原资源分别为底、左、右边框。
        assert(valid(bottom)and valid(left)and valid(right),"原生性能边框缺失")
        -- 运行时会给图片增加 Overlay；资源中的直接 Canvas 槽不能代表现场层级。
        -- 与原生文字定位共用外层 Canvas 查找，明确核对类型后再读取几何。
        local bottomSlot,leftSlot,rightSlot=canvasSlot(bottom),canvasSlot(left),canvasSlot(right)
        local fillHeight=bottomSlot:GetPosition().Y -- 机库 17、补给 16；保留各页原生高度和线宽。
        local frameHeight=rightSlot:GetSize().Y -- 两种原生边框总高均为 18。
        local leftWidth,rightWidth=leftSlot:GetSize().X,rightSlot:GetSize().X -- 机库 1、补给 2。
        local bottomHeight=bottomSlot:GetSize().Y -- 底边线厚度，不跟随横向栏宽拉伸。
        local fillWidth=width-rightWidth -- 右边框占用的空间不计入性能值范围。
        assert(fillHeight>0 and fillHeight<frameHeight and frameHeight<=24
            and leftWidth>0 and rightWidth>0 and bottomHeight>0 and fillWidth>0,"原生性能边框尺寸不符")
        -- 图片同时保留画刷、颜色透明度与局部泛光，避免重复乘蓝色或改成实心色块。
        local function image(template,x,y,w,h,z)
            local widget=infoWidget(c,template,parent,{X=position.X+x,Y=position.Y+y},{X=w,Y=h},true) -- 新叶控件，不复制原生动画状态。
            widget:SetColorAndOpacity(template.ColorAndOpacity)
            widget:SetGlowColor(template.GlowColor);widget:SetUseGlobalGlowColor(template.bUseGlobalGlowColor)
            widget.Slot:SetZOrder(z)
            return widget
        end
        local fill=image(source.BarA,0,0,fillWidth,fillHeight,0) -- 仅绘制目录中可核实的基础值。
        image(bottom,0,fillHeight,width,bottomHeight,5)
        image(left,0,0,leftWidth,frameHeight,5)
        image(right,fillWidth,0,rightWidth,frameHeight,5)
        -- 不复制绿色/黄色图层：未接通部件图表增量前，不能用装饰条冒充真实加成。
        return {fill=fill,width=fillWidth,height=fillHeight}
    end
    -- 全名位于性能上方；准备页弹药仅留在原右下汇总，补给保留独立数量。
    local function informationColumns(c,parent,origin,width,pitch,height,templates,ammoLabel,gap)
        c.columns={}
        local detailsOffset=1.5*pitch -- 选装和补给均为三行全名及半行留白预留高度。
        -- 调用方传入每栏实际宽度，各页面按自己的可用区域排版。
        assert(width>=180,"三 SP 信息区宽度不足")
        for index=1,3 do
            local panel=infoCanvas(c,parent,{X=origin.X+(index-1)*(width+gap),Y=origin.Y},{X=width,Y=height})
            local column={bars={},barWidth=width*.40}
            c.columns[index]=column
            column.name=informationTitle(c,templates.name,panel,width)
            local inset=templates.icon and 36 or 0 -- 只有机库使用彩色类别图标，补给仍全绿色。
            column.fullName=infoWidget(c,templates.fullName or templates.type,panel,{X=inset,Y=pitch},{X=width-inset,Y=3*pitch},false,true)
            if templates.icon then
                column.icon=infoWidget(c,templates.icon,panel,{X=0,Y=pitch+3},{X=20,Y=20},true)
                local material=StaticFindObject("/Game/UI/Menu/LightMenu/Materials/ItemIconCategoryMaterial_Inst0.ItemIconCategoryMaterial_Inst0")
                assert(valid(material),"原生兵装分类材质未加载")
                -- 先设置常量父材质，原生绘制时再生成各栏自己的动态实例，避免两栏串色。
                column.icon:SetBrushFromMaterial(material)
            end
            column.heading=infoWidget(c,templates.heading,panel,{X=0,Y=3*pitch+detailsOffset},{X=width,Y=24},false)
            setText(c,column.heading,"[WEAPON SPECIFICATIONS]")
            for i,item in ipairs(templates.items)do
                local y=(i+3)*pitch+detailsOffset -- 性能和类型整体下移，不挤压名称或内部行距。
                local label=infoWidget(c,item.SpecName,panel,{X=0,Y=y},{X=width*.58,Y=pitch},false) -- 标签与条形区分开。
                local font=label.Font;font.Size=16;label:SetFont(font) -- 三栏缩短标签，保留全部性能项。
                column.bars[i]=performanceBar(c,item.SpecBar,panel,{X=width*.60,Y=y+4},column.barWidth)
            end
            column.typeHeading=infoWidget(c,templates.typeHeading,panel,{X=0,Y=10.2*pitch+detailsOffset},{X=width,Y=24},false)
            column.type=infoWidget(c,templates.type,panel,{X=0,Y=11*pitch+detailsOffset},{X=width,Y=2*pitch},false,true)
            if supply then
                column.ammoHeading=infoWidget(c,templates.ammoHeading,panel,{X=0,Y=13.2*pitch+detailsOffset},{X=width,Y=24},false)
                column.ammo=infoWidget(c,templates.ammo,panel,{X=0,Y=14*pitch+detailsOffset},{X=width,Y=32},false)
                setText(c,column.ammoHeading,ammoLabel)
            end
            setText(c,column.typeHeading,"[TYPE]")
        end
    end
    -- 兵装全名来自当前原生名称表，支持模组新增或替换的名称；缺项明确报错。
    local function weaponFullName(c,option)
        c.weaponFullNames=c.weaponFullNames or {} -- 缓存只属于当前页面，重新进入即可反映语言变化。
        local code=option.code:lower() -- 采用目录里的真实兵装代码，不自行映射为原版武器。
        if not c.weaponFullNames[code]then
            local localizer=StaticFindObject("/Script/Nimbus.Default__LocalizationManager") -- 游戏已合并加载的名称源。
            local nameKey="Weapon_Name_"..code -- 与原生机库兵装名称使用同一组键。
            assert(valid(localizer),"原生兵装名称接口缺失")
            assert(localizer:HasString(nameKey),"原生兵装名称不存在："..nameKey)
            local name=localizer:LocalizeString(nameKey,true,""):ToString() -- 跟随原生语言，不改写全局语言。
            assert(#name>0,"原生兵装名称为空："..nameKey)
            c.weaponFullNames[code]=name
        end
        return c.weaponFullNames[code]
    end
    -- 原生上下文与三 SP 数字独立刷新；不将悬停结果写入 Flow 或 options.max。
    local function refreshPartsPreview(c,now)
        if not parts then return end
        c.partsPreview=partsAmmoPreview(c.owner,c.instance,c.planeId,c.flow.options)
        c.partsPreviewAt=now -- 正常轮询只访问当前已绑定页面，不扫描全局对象。
        if c.partsPreview.pending then
            if not c.partsWaitAt then
                c.partsWaitAt=now;note("PARTS_AMMO_WAIT plane="..c.planeId)
            end
            assert(now-c.partsWaitAt<3,"PART 原生弹药上下文超过 3 秒未同步到当前机体")
        elseif c.partsWaitAt then
            c.partsWaitAt=nil;note("PARTS_AMMO_READY plane="..c.planeId)
        end
    end
    -- SP1 阶段尚未选择 SP2，始终显示 EMPTY；SP2 阶段才展示其当前候选。
    local function paintColumns(c)
        local flow=c.flow;local stage=flow.stage;local row=flow:rows()[flow.cursor]
        local focusedSlot=tonumber(stage:match("^sp([123])$")) -- 列表光标预览的是当前逻辑槽，不是原生武器序号。
        local preview=flow:preview() -- 当前光标与已经确认的前置槽组成展示方案。
        local count=Profile.count(preview) -- 候选变化后立即重算所有可见载量。
        local a,b=preview.a,preview.b
        local statOrder={1,2,3,5,6,4} -- 目录与原生行顺序：后三项对应范围、伤害范围、装填。
        for i,column in ipairs(c.columns)do
            local weapon=preview[Profile.fields[i]]
            local option=weapon>=0 and flow.options[weapon+1]or nil -- 空栏不借用历史兵装的性能或弹药。
            local pending=parts and c.partsPreview.pending -- 等待只清空模组数字，不阻止原生 PART 导航。
            local entry=parts and option and not pending and c.partsPreview.amounts[weapon+1]or nil -- 当前实际 WeaponID 对应结果。
            local amount=option and not pending and Profile.ammo(entry and entry.preview or option.max,count,preview.ratio)or 0
            local affects=entry and entry.affects or false -- 不能用“整数变大”替代原生候选影响标记。
            local ammoText=option and not pending and tostring(amount)or "---" -- EMPTY 与未就绪均不展示猜测数量。
            local ammoSignature=ammoText..":"..tostring(affects)..":"..tostring(pending) -- 数字相同也能立即切换黄色。
            if c.ammo and not supply and not standby then
                local focused=i==focusedSlot -- 槽位变更也要重画颜色，即使简称和弹数恰好相同。
                if column.ammoFocused~=focused then
                    local base=c.tint.SpecifiedColor -- 非当前槽保留信息蓝，仅调整透明度。
                    local tint=focused and color(1,1,1,1)or color(base.R,base.G,base.B,.5) -- 当前槽完全高亮，其余两行半透明。
                    c.ammo[i].name:SetColorAndOpacity(tint);c.ammo[i].count:SetColorAndOpacity(tint)
                    column.ammoFocused=focused
                end
            end
            if column.lastAmmo~=ammoSignature then
                if column.ammo then
                    setText(c,column.ammo,ammoText)
                    if parts then
                        column.ammo:SetColorAndOpacity(affects and c.ammoCandidate or c.ammoStandard)
                        column.ammo:SetUseGlobalGlowColor(not affects)
                        if affects then column.ammo:SetGlowColor(c.ammoCandidateGlow)end -- 标准状态交还全局泛光，与原生一致。
                    end
                end
                if c.ammo then setText(c,c.ammo[i].count,ammoText)end
                column.lastAmmo=ammoSignature
            end
            -- 悬停数量与颜色独立更新，不反复重画未变的名称和 18 个基础性能条。
            local signature=tostring(weapon)
            if column.lastContent==signature then goto nextColumn end
            setText(c,column.name,"SP"..i.."  "..(option and option.code:upper()or "EMPTY"))
            local fullName=option and weaponFullName(c,option)or "NO WEAPON" -- 空挂载不查询不存在的兵装键。
            local typeText=option and assert(weaponTypes[option.type_key:gsub("^HangarWeaponType_Name_","")],"原生兵装类型未知："..option.type_key)or "NO WEAPON"
            if column.fullName then setText(c,column.fullName,fullName)else typeText=fullName end -- STANDBY 紧凑栏保留原有全名位置。
            setText(c,column.type,typeText)
            if column.icon then
                column.icon:SetVisibility(option and VISIBLE or COLLAPSED)
                if option and column.category~=option.category then
                    local ok,err=weaponIcon(column.icon:GetAddress(),c.owner:GetAddress(),option.category)
                    assert(ok,err);column.category=option.category
                end
            end
            for stat,bar in ipairs(column.bars)do
                local percent=option and option.stats[statOrder[stat]]or 0
                bar.fill.Slot:SetSize({X=bar.width*math.max(0,math.min(1,percent/100)),Y=bar.height})
                bar.fill:SetVisibility(percent>0 and VISIBLE or COLLAPSED) -- EMPTY 保留边框，不留下零宽度亮线。
            end
            -- 准备页弹药汇总的三个逻辑槽与上方三栏保持一致。
            if c.ammo then
                setText(c,c.ammo[i].name,option and option.code:upper()or "EMPTY")
            end
            column.lastContent=signature
            ::nextColumn::
        end
        return a,b,preview.c
    end
    local function attach(owner,instance,planeId,profile,options)
        local c={owner=owner,instance=instance,planeId=planeId,added={},masked={},moved={},texts={},lastPaint=nil,originalControl=owner:IsEnableControl(),originalWeapon=instance:GetSelectedPlaneSWP(),flow=Flow.new(profile,options,nil,nil,tr,ammoSettings)}
        self.cache=c
        c.controller=owner:GetOwningPlayer();assert(valid(c.controller),"准备页所属输入控制器缺失")
        c.world=instance:GetWorld():GetAddress();bindings(c)
        local base=owner.LightMenuBaseWidget;assert(valid(base),"Campaign native list is missing")
        local list=controls(base)
        for _,name in ipairs({"TitleText","DescriptionText","ItemText0","ItemText1","ItemText2"})do assert(valid(list[name]),"Native list control is missing: "..name)end
        -- Preserve the game's own footer glyphs and device-dependent labels.
        c.title=clone(c,list.TitleText,false,nil,nil,preparationTitle(c.flow.stage));c.description=clone(c,list.DescriptionText,false)
        c.rows={};for i=1,3 do c.rows[i]=clone(c,list["ItemText"..(i-1)],false)end
        -- 采用资源核对的机库信息蓝；列表第一项可能已被原生高亮改成深色。
        c.tint=color(.401978,.651406,1,1)
        local info=owner.WeaponInformation;assert(valid(info),"原生兵装信息区缺失")
        local wrapper=info.HangarSpecGroupR;assert(valid(wrapper)and valid(wrapper.SpecGroup),"准备性能组缺失")
        local group=wrapper.SpecGroup -- 外层 R 只是定位容器；性能项属于内层 SpecGroup。
        local ammo=info.HangarAircraftAllWeaponsAmmo;assert(valid(ammo),"原生弹药汇总缺失")
        local ammoTemplate=controls(ammo.WeaponsSP1)
        local typeTemplate=controls(info.HangarAircraftWeaponType)
        local fullNameTemplate=controls(info.HangarAircraftWeaponNameLong) -- 原生彩色分类图标和全名字体。
        local items={}
        for i=1,6 do
            local item=group["SpecItem"..i]
            assert(valid(item)and valid(item.SpecName)and valid(item.SpecBar)and valid(item.SpecBar.BarA),"准备性能项模板缺失")
            items[i]=item
        end
        local origin=vec(group.Slot:GetPosition()) -- 以原生性能区右边界为基准，向左展开三栏。
        local width=canvasSlot(group.SpecGroupName):GetSize().X+items[1].Slot:GetPosition().X -- 原生性能区宽度。
        local pitch=items[2].Slot:GetPosition().Y-items[1].Slot:GetPosition().Y -- 沿用原生性能行距。
        assert(width>300 and width<450 and pitch>20 and pitch<60,"准备三栏尺寸不符")
        local columnWidth,gap=320,36 -- 原生逻辑坐标；加宽名称和性能区，栏间仍留出明确间隔。
        origin.X=origin.X+width-3*columnWidth-2*gap;origin.Y=origin.Y-1.5*pitch
        assert(origin.X>700 and origin.Y>100,"准备三栏位置不符，停止接入")
        -- 新三栏挂信息区的根 Canvas，不能挂进随后折叠的原性能组件子树。
        local infoRoot=info.WidgetTree.RootWidget
        assert(valid(infoRoot)and infoRoot:GetClass():GetFullName()=="Class /Script/UMG.CanvasPanel","准备信息区根 Canvas 不符")
        informationColumns(c,infoRoot,origin,columnWidth,pitch,14.5*pitch,
            {name=assert(ammoTemplate.WeaponsName),fullName=assert(fullNameTemplate.Name),icon=assert(fullNameTemplate.Icon),
             heading=assert(group.SpecGroupName),items=items,
             typeHeading=assert(typeTemplate.WeaponTypeLabel),type=assert(typeTemplate.WeaponTypeText),
             ammoHeading=assert(typeTemplate.WeaponTypeLabel),ammo=assert(ammoTemplate.WeaponsAmmo)},"[WEAPONS AMMO]",gap)
        c.ammo={}
        for i=1,3 do
            c.ammo[i]=preparationAmmoRow(c,ammo["WeaponsSP"..i])
        end
        mask(c,wrapper);mask(c,info.HangarAircraftWeaponNameLong)
        mask(c,info.HangarAircraftWeaponProperties);mask(c,info.HangarAircraftWeaponType)
        -- 原状态图标是未读标记，不能当作兵装类别；新列表以简称和选中底色识别。
        mask(c,owner.LightMenuItemIconStatusWidget)
        mask(c,owner.LightMenuItemIconCategoryWidget) -- 右侧原生单兵装类别图标不能混入新的三栏。
        -- 现场确认金色光标属于共享前景的 ScrollCanvas，兵装页上的同名 Marker 不是实际对象。
        local manager=owner:GetUIManagerActor() -- 只接管本页所属管理器，离开时恢复给 STANDBY。
        assert(valid(manager)and valid(manager.MenuForegroundWidget),"准备页原生前景选择器缺失")
        local selector=manager.MenuForegroundWidget
        assert(selector:GetClass():GetFName():ToString()=="MenuBaseSelector_C"and valid(selector.ScrollCanvas),"准备页金色光标结构已变化")
        -- 按完整 FName 读取实际子树；光晕的数字后缀属于名称，不可省略。
        local goldSources=controls(selector.ScrollCanvas)
        mask(c,selector.ScrollCanvas)
        -- 从原生金色标记取相对布局，四个叶控件使用本页副本，不移动共享前景。
        local header=assert(goldSources.ScrollHeaderImage,"准备页金色标记缺失")
        local headerPosition=header.Slot:GetPosition()
        local headerSize=header.Slot:GetSize();local headerAlignment=header.Slot:GetAlignment()
        local headerCenter=headerPosition.Y+headerSize.Y*(.5-headerAlignment.Y)
        local rowSlot=canvasSlot(c.rows[1]) -- 副本挂到列表可见的外层 Canvas，随原生页面缩放。
        c.goldCursor={}
        for _,name in ipairs({"BG_Glow_2","ScrollHighlightImage","ScrollUnderlineImage","ScrollHeaderImage"})do
            local source=goldSources[name];assert(valid(source),"准备页金色光标素材缺失："..name)
            local layout=readLayout(source.Slot,source)
            assert(layout.kind=="canvas","准备页金色光标槽类型改变")
            local widget=clone(c,source,true,false)
            -- AC7 图片还有专用光晕和预乘透明属性，必须随原画刷保留。
            widget.GlowColor=source.GlowColor;widget.bUseGlobalGlowColor=source.bUseGlobalGlowColor
            widget.bBlendUsePreMultiplyAlpha=source.bBlendUsePreMultiplyAlpha
            local tint=source.ColorAndOpacity -- 渐变原生 Alpha 为 0.2，不能被通用克隆提高到 1。
            widget:SetColorAndOpacity({R=tint.R,G=tint.G,B=tint.B,A=tint.A})
            assert(widget.Slot.Parent:RemoveChild(widget),"无法移动金色光标副本")
            local target=rowSlot.Parent:AddChild(widget);assert(valid(target),"无法接入金色光标副本")
            -- 列表文字以页面中心为横向锚点，金色素材以左边缘为锚点；不能混用。
            local rowAnchors=rowSlot:GetAnchors()
            target:SetAnchors({Minimum={X=layout.anchors.Minimum.X,Y=rowAnchors.Minimum.Y},
                Maximum={X=layout.anchors.Maximum.X,Y=rowAnchors.Maximum.Y}})
            target:SetAlignment(layout.alignment)
            target:SetAutoSize(false);target:SetSize(layout.size);target:SetZOrder(rowSlot:GetZOrder()-1)
            c.goldCursor[#c.goldCursor+1]={widget=widget,x=layout.position.X,y=layout.position.Y-headerCenter}
        end
        setControl(owner,false)
        c.previous=inputs(c);c.repeatAt=0
        note("NATIVE_LOADOUT_ATTACHED "..owner:GetFullName())
        self.status="sp1";self.error=nil
        return c
    end
    -- STANDBY/PART 左栏展示机体、部件和弹药，右栏用完整高度排列三个 SP。
    local function attachStandby(owner,instance,planeId,profile,options)
        local c={owner=owner,instance=instance,planeId=planeId,world=instance:GetWorld():GetAddress(),
            added={},masked={},moved={},texts={},repositioned={},tint=color(.401978,.651406,1,1),columns={},ammo={},
            controlEnabled=owner:IsEnableControl()} -- 页面复用时用控制重新开放识别返回。
        self.cache=c
        c.flow=Flow.new(profile,options,nil,nil,tr,ammoSettings);c.flow.stage="review" -- 只读快照复用同一套名称、性能和配弹算法。
        local info -- 两套资源共用已核对的右栏结构，字段缺失时明确报错。
        if parts then info=owner.AircraftWeaponInformation else info=owner.ReviewInformation end
        assert(valid(info)and valid(info.WidgetTree),"只读兵装信息区缺失："..mode)
        local root=info.WidgetTree.RootWidget
        local wrapper=info.HangarSpecGroupR;local group=valid(wrapper)and wrapper.SpecGroup
        local leftWrapper=info.HangarSpecGroupL -- 左侧机体性能栏是弹药区的横向对齐基准。
        local leftGroup=valid(leftWrapper)and leftWrapper.SpecGroup -- 两页均使用原生 SpecGroup 定位。
        local ammo=info.HangarAircraftBoardWeaponsAmmo
        assert(valid(root)and valid(group)and valid(leftGroup)and valid(ammo)and valid(ammo.WeaponsSP1),"STANDBY 性能或弹药模板缺失")
        local weaponTemplate=controls(ammo.WeaponsSP1)
        local typeTemplate=controls(info.HangarAircraftWeaponType)
        local origin=vec(group.Slot:GetPosition()) -- 原右栏在 1920 逻辑坐标中的 X=1442。
        local cardX=origin.X+32 -- 只将三个 SP 卡片右移；弹药区仍使用未经平移的原生基准。
        local leftOrigin=vec(leftGroup.Slot:GetPosition()) -- 同一逻辑坐标系中的左栏 X=1039。
        local width=canvasSlot(group.SpecGroupName):GetSize().X+group.SpecItem1.Slot:GetPosition().X
        assert(width>300 and width<450 and origin.X>1400 and origin.Y>100,"STANDBY 右栏布局已变化")
        assert(leftOrigin.X>900 and leftOrigin.X+width<=origin.X,"STANDBY 左右栏对齐基准已变化")
        assert(cardX+width<=1880,"STANDBY 三 SP 卡片超出右侧安全边距")
        if parts then
            local manager=owner:GetUIManagerActor() -- 黄字样式与当前原生机库主题一致。
            assert(valid(manager),"PART 原生颜色管理器缺失")
            local style=manager.HangarColorParameters -- 原生用 OnCursor，而非 ValueOver。
            local function textColor(value)return color(value.R,value.G,value.B,value.A)end -- 复制线性色，避免保存结构引用。
            c.ammoStandard=textColor(style.TextColorStandard)
            c.ammoCandidate=textColor(style.TextColorOnCursor)
            c.ammoCandidateGlow=textColor(style.TextGlowOnCursor)
        end
        origin.Y=origin.Y-39 -- 对齐原右侧兵装名称，不侵占上方标题。
        local cardPitch,cardHeight=252,240 -- 移走弹药区后增加卡片高度，三组底部仍避开原生操作栏。
        local statTop,statPitch=34,26 -- 性能行从 18 增至 26，文字与条形不再紧贴。
        for index=1,3 do
            local panel=infoCanvas(c,root,{X=cardX,Y=origin.Y+(index-1)*cardPitch},{X=width,Y=cardHeight})
            local column={bars={},barWidth=width*.40};c.columns[index]=column
            column.name=informationTitle(c,assert(weaponTemplate.WeaponsName),panel,width)
            for stat=1,6 do
                local item=group["SpecItem"..stat] -- 性能名称和画刷取自原生右侧兵装组。
                assert(valid(item)and valid(item.SpecName)and valid(item.SpecBar.BarA),"STANDBY 性能模板缺失")
                local y=statTop+(stat-1)*statPitch -- 六行性能共用固定节奏，随原生父容器缩放。
                infoWidget(c,item.SpecName,panel,{X=0,Y=y},{X=width*.52,Y=statPitch},false)
                column.bars[stat]=performanceBar(c,item.SpecBar,panel,{X=width*.60,Y=y+4},column.barWidth)
            end
            column.type=infoWidget(c,assert(typeTemplate.WeaponTypeText),panel,{X=0,Y=194},{X=width,Y=46},false,true)
            -- 部件加成及比例配弹后的载量统一放在左下弹药区，不占用右侧卡片。
            local ammoParent=ammo.WeaponsSP1.Slot.Parent
            c.ammo[index]={name=infoWidget(c,weaponTemplate.WeaponsName,ammoParent,{X=217,Y=26+(index-1)*34},{X=108,Y=32},false),
                count=infoWidget(c,assert(weaponTemplate.WeaponsAmmo),ammoParent,{X=325,Y=26+(index-1)*34},{X=56,Y=32},false)}
            column.ammo=c.ammo[index].count
        end
        -- 整块移动原弹药 Canvas，保留 GUN/MSL/FLR、三个 SP 及标题的相对排版。
        local ammoCanvas=ammo.WeaponsSP1.Slot.Parent
        c.repositioned[1]={widget=ammoCanvas,layout=readLayout(ammoCanvas.Slot,ammoCanvas)}
        assert(c.repositioned[1].layout.kind=="canvas","STANDBY 弹药区不是 Canvas 布局")
        local ammoPosition=ammoCanvas.Slot:GetPosition() -- 原弹药块比右侧性能基准多 5 个逻辑像素。
        local ammoTop=leftOrigin.Y+(parts and 560 or 460) -- PART 多出三类容量条，弹药整体下移 100；STANDBY 保持原位置。
        ammoCanvas.Slot:SetPosition({X=ammoPosition.X+leftOrigin.X-origin.X,Y=ammoTop})
        mask(c,ammo.WeaponsSP1);mask(c,wrapper)
        mask(c,info.HangarAircraftWeaponNameShortR);mask(c,info.HangarAircraftWeaponType)
        refreshPartsPreview(c,os.clock())
        paintColumns(c);self.status=mode;note("NATIVE_"..mode:upper().."_ATTACHED "..owner:GetFullName())
        return c
    end
    -- 待命页只在有效方案、同一机体/世界和原生页面激活时展示；重进重新读取已保存配置。
    local function pumpStandby(instance,planeId,profile,options,now,inFlight,panelOpen)
        if panelOpen then return end -- F6 只暂停刷新，不能把临时输入门当成离页。
        local saved=profile() -- 调用方按下一次出击设置计算只读载量，不改当前任务快照。
        local c=self.cache
        if c and (inFlight or not saved or not valid(c.owner)or not c.owner:IsVisible()
            or c.planeId~=planeId or c.world~=instance:GetWorld():GetAddress()
            or c.flow.draft.a~=saved.a or c.flow.draft.b~=saved.b or c.flow.draft.c~=saved.c or c.flow.draft.ratio~=saved.ratio)then
            -- 只撤回自己修改的兵装区域。原生会复用此页，隐藏整个根会越过本适配器的生命周期。
            -- 页面退出已由原生显隐控制，不能再折叠根并等待另一次启用事件来补救。
            note("NATIVE_"..mode:upper().."_DETACHED plane="..c.planeId.." next="..planeId)
            restore(false);c=nil
        end
        -- 输入停用是退出动画的起点，此时保留多 SP 画面直到页面实际隐藏。
        if c then
            -- STANDBY 只在返回时刷新，PART 继续观察已安装部件，避免复用页面留下旧载量。
            local enabled=c.owner:IsEnableControl()
            if enabled and (not c.controlEnabled or (parts and now-(c.ammoRefreshedAt or -1)>.25))then
                local choices=modelChoices(c.owner,instance,planeId,options,now) -- 部件页也不能拿上一架预览计算载量。
                if not choices then return end
                c.ammoRefreshedAt=now;c.flow.options=choices
                refreshPartsPreview(c,now);paintColumns(c);self.status=mode
            elseif enabled and parts and now-c.partsPreviewAt>=.05 then
                refreshPartsPreview(c,now);paintColumns(c) -- 50 ms 只读候选刷新，不重建控件或重新查询机体选项。
            end
            c.controlEnabled=enabled
            return
        end
        if inFlight or panelOpen or not saved or self.error or now-self.lastScan<.05 then return end
        self.lastScan=now
        for _,owner in ipairs(owners(instance,now))do
            if valid(owner)and owner:IsVisible()and owner:IsEnableControl()and valid(owner:GetWorld())
                and owner:GetWorld():GetAddress()==instance:GetWorld():GetAddress()
                and not owner:GetFullName():find("WidgetArchetype",1,true)and not owner:GetFullName():find("Default__",1,true)then
                if not panelClass then api()end
                local choices=modelChoices(owner,instance,planeId,options,now) -- 等待只影响本模组显示，不接管只读页输入。
                if choices then measuredAttach(attachStandby,owner,instance,planeId,saved,choices)end
                return
            end
        end
        self.status="waiting"
    end
    -- 扩大原生补给容器及背景边框；离页统一归还原布局，避免复用页面时越变越大。
    local function expandSupplyPanel(c,parent,list)
        local original=readLayout(parent.Slot,parent) -- 已核对资源采用左上锚点、固定尺寸。
        assert(original.kind=="canvas"and original.anchors.Minimum.X==0 and original.anchors.Minimum.Y==0
            and original.anchors.Maximum.X==0 and original.anchors.Maximum.Y==0
            and original.alignment.X==0 and original.alignment.Y==0,"补给主容器定位方式已变化")
        assert(original.size.X>1200 and original.size.X<1400 and original.size.Y>500 and original.size.Y<650,"补给原面板尺寸已变化")
        local size={X=1680,Y=680} -- 原生 1920×1080 逻辑坐标，左右仍保留约 120 的屏幕边距。
        -- 每个原控件先记录再修改，成功退出和接入报错均走同一还原流程。
        local function resize(widget,position,dimensions)
            assert(valid(widget)and valid(widget.Slot),"补给背景或边框缺失")
            local slot=canvasSlot(widget) -- 背景/边框图片同样可能有运行时 Overlay 包装。
            local positioned=slot.Content -- 几何归属外层容器，还原时也必须归还这一层。
            assert(valid(positioned),"补给定位槽内容缺失")
            assert(positioned:GetAddress()==parent:GetAddress()or slot.Parent:GetAddress()==parent:GetAddress(),"补给背景或边框不属于主容器")
            local layout=readLayout(slot,positioned) -- 不保存可被随后 SetSize 改写的结构引用。
            assert(layout.kind=="canvas"and layout.anchors.Minimum.X==0 and layout.anchors.Minimum.Y==0
                and layout.anchors.Maximum.X==0 and layout.anchors.Maximum.Y==0,"补给背景或边框定位方式已变化")
            c.repositioned[#c.repositioned+1]={widget=positioned,layout=layout}
            slot:SetPosition(position);slot:SetSize(dimensions);slot:SetAutoSize(false)
        end
        resize(parent,{X=original.position.X-(size.X-original.size.X)/2,Y=original.position.Y-(size.Y-original.size.Y)/2},size)
        local backgroundLeft,backgroundRight=list.Background_Left,list.Background_Right -- 原生左右渐变背景分别延展。
        assert(valid(backgroundLeft)and valid(backgroundRight),"补给左右背景缺失")
        local split=canvasSlot(backgroundLeft):GetSize().X/original.size.X*size.X -- 保留原左右背景的宽度比例。
        assert(split>0 and split<size.X,"补给背景分界不符")
        resize(backgroundLeft,{X=0,Y=0},{X=split,Y=size.Y})
        resize(backgroundRight,{X=split,Y=0},{X=size.X-split,Y=size.Y})
        for _,name in ipairs({"Border_TOP","Border_BTM","Border_Left","Border_Right"})do
            local border=assert(list[name],"补给边框缺失："..name) -- 四条边框只延长跨度，保留原线厚。
            local oldSize=canvasSlot(border):GetSize() -- 水平边取高度，垂直边取宽度。
            local horizontal=name=="Border_TOP"or name=="Border_BTM"
            local thickness=horizontal and oldSize.Y or oldSize.X -- 原生 2 个逻辑像素。
            assert(thickness>0 and thickness<=4,"补给边框线厚异常")
            resize(border,{X=name=="Border_Right"and size.X-thickness or 0,Y=name=="Border_BTM"and size.Y-thickness or 0},
                {X=horizontal and size.X or thickness,Y=horizontal and thickness or size.Y})
        end
        return size
    end
    -- 在扩大的原生补给背景内多槽选择，并列展示三个 SP；不改变原生确认事务。
    local function attachSupply(owner,instance,planeId,profile,options)
        assert(self.phase and valid(self.phaseObject),"补给菜单缺少原生阶段绑定")
        local c={owner=owner,instance=instance,planeId=planeId,added={},masked={},moved={},texts={},repositioned={},lastPaint=nil,
            originalControl=owner:IsEnableControl(),flow=Flow.new(profile,options,nil,"resupply",tr,ammoSettings),phase=self.phase,phaseWorld=self.phaseWorld,
            takeoffPending=false,visualRestored=false}
        self.cache=c
        c.controller=owner:GetOwningPlayer();assert(valid(c.controller),"补给页所属输入控制器缺失")
        c.world=instance:GetWorld():GetAddress();bindings(c)
        local list=controls(owner) -- 保留原生列表、光标和设备决定/返回提示。
        local nativeTint=list.TitleText.ColorAndOpacity.SpecifiedColor -- 原生白色字面配局部绿色泛光。
        c.tint=color(nativeTint.R,nativeTint.G,nativeTint.B,1);c.glow=list.TitleText.GlowColor
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
        c.title:SetColorAndOpacity(c.tint);c.description:SetColorAndOpacity(c.tint)
        -- 在接入时读取一次原生完整武器说明；不拿简称表冒充说明，也不切换全局语言。
        local localizer=StaticFindObject("/Script/Nimbus.Default__LocalizationManager")
        assert(valid(localizer),"原生武器说明翻译接口缺失")
        c.weaponDescriptions={}
        for _,option in ipairs(options)do
            local descriptionKey="Weapon_Description_"..option.code:lower()
            assert(localizer:HasString(descriptionKey),"原生武器说明不存在："..descriptionKey)
            local description=localizer:LocalizeString(descriptionKey,true,""):ToString()
            assert(#description>0,"原生武器说明为空："..descriptionKey)
            -- 原生介绍自带针对宽面板的硬换行；本栏重新排版，保留空行划分的段落。
            description=description:gsub("\r\n","\n"):gsub("\r","\n")
            -- 英文换行转为空格，避免两个单词粘连；中文仍去除排版用硬换行。
            description=description:gsub("\n[ \t]*\n+","\1"):gsub("[ \t]*\n[ \t]*",tr(""," ")):gsub("\1","\n\n")
            c.weaponDescriptions[option.code]=description
        end
        local group=assert(list.WeaponSpecifficationGroup,"补给性能组缺失")
        local video=assert(list.WeaponConceptPanel,"补给视频区域缺失")
        local typePanel=assert(list.WeaponTypePanel,"补给类型区域缺失")
        local ammoPanel=assert(list.WeaponAmmoPanel,"补给弹药区域缺失")
        local parent=group.Slot.Parent
        assert(parent:GetAddress()==video.Slot.Parent:GetAddress()and parent:GetAddress()==typePanel.Slot.Parent:GetAddress(),"补给信息区域不在同一原生容器")
        local mainSize=expandSupplyPanel(c,parent,list) -- 同时扩大背景和四边框，并保持原面板中心。
        local origin={X=560,Y=48} -- 左侧保留列表、操作提示及 458 宽的武器说明。
        local columnWidth,gap=320,40 -- 三栏宽度与机库选装一致，标题及性能标签不再挤在一起。
        assert(origin.X+3*columnWidth+2*gap<=mainSize.X-60,"补给三栏超出面板安全边距")
        local items={} -- 直接使用已核对的 SpecName/SpecBar 原生变量，不复制 UserWidget 子树。
        for i=1,6 do
            local item=group["SpecItem"..i]
            assert(valid(item)and valid(item.SpecName)and valid(item.SpecBar)and valid(item.SpecBar.BarA),"补给性能项模板缺失")
            items[i]=item
        end
        local pitch=items[2].Slot:GetPosition().Y-items[1].Slot:GetPosition().Y
        assert(pitch>20 and pitch<60,"补给性能行距不符")
        pitch=34 -- 加高面板后放宽行距；三行全名、类型和弹药各自保留空间。
        assert(origin.Y+15.5*pitch+32<=mainSize.Y-40,"补给弹药信息超出面板安全边距")
        informationColumns(c,parent,origin,columnWidth,pitch,mainSize.Y-origin.Y-20,
            {name=list.WeaponAmmo_Text,heading=assert(group.SpecGroupName),items=items,
             typeHeading=list.WeaponType_Label,type=list.WeaponType_Text,
             ammoHeading=list.WeaponAmmo_Label,ammo=list.WeaponAmmo_Text},"[WEAPONS AMMO]",gap)
        -- 说明仅占左侧列表下方，与右侧三栏保留明确间隔。
        local descriptionX=72 -- 与原生左侧列表标题的逻辑基线对齐。
        local descriptionWidth=origin.X-descriptionX-30 -- 与右侧信息栏保留间隙。
        c.weaponDescription=clone(c,assert(list.WeaponDescriptionText),false,true,descriptionWidth)
        c.weaponDescription:SetColorAndOpacity(c.tint)
        local descriptionFont=c.weaponDescription.Font;descriptionFont.Size=20;c.weaponDescription:SetFont(descriptionFont)
        assert(c.weaponDescription.Slot.Parent:RemoveChild(c.weaponDescription),"无法调整补给说明位置")
        local descriptionSlot=parent:AddChild(c.weaponDescription)
        assert(valid(descriptionSlot),"无法接入补给说明")
        descriptionSlot:SetAnchors({Minimum={X=0,Y=0},Maximum={X=0,Y=0}});descriptionSlot:SetAlignment({X=0,Y=0})
        -- 留出左栏下半部显示原生多行介绍，避免长说明被原来的 130 高度裁掉。
        descriptionSlot:SetPosition({X=descriptionX,Y=mainSize.Y*.52});descriptionSlot:SetSize({X=descriptionWidth,Y=mainSize.Y*.48-30})
        descriptionSlot:SetAutoSize(false);descriptionSlot:SetZOrder(10)
        mask(c,group);mask(c,video);mask(c,typePanel);mask(c,ammoPanel)
        setControl(owner,false)
        c.previous=inputs(c);c.repeatAt=0
        self.status="sp1";self.error=nil
        note(string.format("NATIVE_RESUPPLY_ATTACHED %s panel=%.0fx%.0f columns=%.1f pitch=%.1f",owner:GetFullName(),mainSize.X,mainSize.Y,columnWidth,pitch))
        return c
    end
    -- 悬停更新当前栏，另一栏保留已选兵装；确认页保留完整三栏。
    local function paintSupply(c)
        local flow=c.flow;local rows=flow:rows();local stage=flow.stage
        local signature=stage..":"..flow.cursor..":"..flow.draft.a..":"..flow.draft.b..":"..flow.draft.c..":"..tostring(flow.message)
        signature=signature..":"..flow:preview().ratio
        if signature==c.lastPaint then return end
        c.lastPaint=signature
        setText(c,c.title,stage=="sp1"and "[SELECT SP WEAPON 1]"or stage=="sp2"and "[SELECT SP WEAPON 2]"or stage=="sp3"and "[SELECT SP WEAPON 3]"or "[TAKE OFF]")
        for i=1,3 do
            local row=rows[i]
            c.rows[i]:SetVisibility(row and VISIBLE or COLLAPSED)
            c.cursors[i]:SetVisibility(row and i==flow.cursor and VISIBLE or 2)
            if row then
                local label=row.action=="takeoff"and "YES"or row.action=="reselect"and "NO"
                    or row.weapon<0 and "NO WEAPON"or row.code
                setText(c,c.rows[i],label..(row.enabled and ""or "  NOT OWNED"))
                local tint=c.tint.SpecifiedColor
                c.rows[i]:SetColorAndOpacity(row.enabled and c.tint or color(tint.R*.45,tint.G*.45,tint.B*.45,1))
                -- 光标是新建文字控件，显式复用当前列表颜色，避免移动父 Canvas 后继承透明样式。
                c.cursors[i]:SetColorAndOpacity(c.tint)
            end
        end
        local a,b,third=paintColumns(c)
        local option=nil -- 最终页按 YES/NO 给说明；选择页才显示当前候选说明。
        if stage~="review"then option=flow.options[(stage=="sp1"and a or stage=="sp2"and b or third)+1]end
        local description=stage=="review"and (flow.cursor==1 and tr("决定：按本次方案起飞。返回：修改最后一个选择。","Confirm: take off. Back: edit the last choice.")or tr("决定：返回 SP1，重新选择特殊兵装。","Confirm: reselect special weapons from SP1."))
            or stage=="sp1"and tr("决定：选择 SP1。","Confirm: select SP1.")or tr("决定：选择当前 SP，或无兵装结束。返回：修改上一槽。","Confirm this SP, or NO WEAPON to finish. Back: edit the previous slot.")
        if flow.message then description=flow.message..tr("。",". ")..description end
        setText(c,c.description,description)
        setText(c,c.weaponDescription,stage=="review"and (flow.cursor==2 and tr("变更特殊兵装。","Change special weapons.")or tr("按已选的 1 至 3 个 SP 重新起飞。","Take off with the selected one to three SP weapons."))
            or option and assert(c.weaponDescriptions[option.code])or tr("不搭载当前及后续槽，按管理页比例配弹。","Leave this and later slots empty. Ammunition follows management percentages."))
        self.status=stage;note("NATIVE_RESUPPLY_STAGE "..signature)
    end

    local function paint(c)
        local flow=c.flow;local rows=flow:rows();local stage=flow.stage
        local signature=stage..":"..flow.cursor..":"..flow.draft.a..":"..flow.draft.b..":"..flow.draft.c..":"..tostring(flow.message)..":"..tostring(c.continueRequested)..":"..tostring(c.exitRequested)..":"..tostring(c.handedOff)
        signature=signature..":"..flow:preview().ratio
        if signature==c.lastPaint then return end
        c.lastPaint=signature
        local blue=color(.401978,.651406,1,1);local selected=color(1,1,1,1)
        -- 原生预览行和模组光标同步；EMPTY 没有模型，保留已选 SP1 的特写。
        local preview=rows[flow.cursor].weapon
        if preview<0 then preview=flow.draft.a end
        if c.previewWeapon~=preview then
            local ok,_,err=preparationAction(c.owner:GetAddress(),preview,3)
            assert(ok,err);c.previewWeapon=preview
        end
        setText(c,c.title,preparationTitle(stage))
        for i=1,3 do
            local row=rows[i]
            c.rows[i]:SetVisibility(row and VISIBLE or COLLAPSED)
            if row then
                setText(c,c.rows[i],(row.weapon<0 and "NO WEAPON"or row.code)..(row.enabled and ""or "  NOT OWNED"))
                c.rows[i]:SetColorAndOpacity(not row.enabled and color(.3,.36,.43,1)or (i==flow.cursor and selected or blue))
            end
        end
        local rowSlot=canvasSlot(c.rows[flow.cursor]);local position=rowSlot:GetPosition();local size=rowSlot:GetSize()
        -- 两个 Canvas 都沿用整页逻辑坐标；横向保留原生竖线位置，纵向跟随当前行。
        for _,part in ipairs(c.goldCursor)do
            part.widget:SetVisibility((c.continueRequested or c.exitRequested)and COLLAPSED or VISIBLE)
            part.widget.Slot:SetPosition({X=part.x,Y=position.Y+size.Y*.5+part.y})
        end
        paintColumns(c)
        local description=stage=="sp1"and tr("请选择第一种特殊兵装。","Select your first special weapon.")
            or tr("请选择当前槽兵装；无兵装结束选装。弹药按管理页比例向上取整。","Select this SP, or NO WEAPON to finish. Ammunition follows management percentages, rounded up.")
        if flow.message then description=flow.message..tr("。",". ")..description end
        setText(c,c.description,description);c.description:SetColorAndOpacity(blue)
        self.status=stage
        note("NATIVE_LOADOUT_STAGE "..signature)
    end
    local function pump(instance,planeId,profile,options,now,inFlight,panelOpen)
        -- F8 只暂停补给页读取，不能释放多槽选择控制让原生单 SP 直接起飞。
        if supply and panelOpen then return end
        if (inFlight and not supply)or panelOpen then if self.cache then restore(not inFlight)end;return end
        local c=self.cache
        -- 原生 YES 已发出后，页面隐藏/停用属于正常关闭过程；保留草稿到
        -- 任务成功事件。不能因 IsVisible 变为 false 就丢弃确认事务或重新接入。
        if supply and c and c.handedOff then self.status="awaiting_success";return end
        -- 原生输入桥接报告已接收后才保存；动画期间保留三栏，隐藏后再恢复源控件。
        if not supply and c and c.actionQueued then
            assert(c.planeId==planeId and instance:GetWorld():GetAddress()==c.world,"机库交接期间机体或世界已变化")
            local ok,accepted,err=preparationAction(c.actionWidget,0,0) -- 回执不依赖旧页面继续存活。
            assert(ok,err)
            if accepted then
                c.actionQueued=false;c.actionAccepted=true
                if not c.exitRequested then
                    local prepared=c.flow:profile()
                    assert(instance:GetSelectedPlaneSWP()==prepared.a,"原生准备 SP1 未按确认方案保存")
                    commit(c.planeId,copyProfile(prepared));self.status="committed"
                    note("NATIVE_LOADOUT_COMMITTED_NATIVE_ACTION "..c.planeId)
                end
            else assert(now-c.actionAt<3,"原生机库未接收决定/返回，已停止交接")end
        end
        if not supply and c and c.actionAccepted then
            if not valid(c.owner)or not c.owner:IsVisible()or not c.owner:IsEnableControl()then
                hideClosedRoot(c)
                self.ignored=c.actionWidget;self.ignoredReason="closed";restore(false)
            end
            return
        end
        if c and (not valid(c.owner)or not valid(c.controller)or not c.owner:IsVisible()or c.planeId~=planeId or instance:GetWorld():GetAddress()~=c.world)then
            restore(false);c=nil
        end
        if not c then
            if self.error or now-self.lastScan<(supply and .02 or .05)then return end
            self.lastScan=now
            local candidate=nil
            for _,owner in ipairs(owners(instance,now))do
                -- 补给输入已在任务事件中隔离；无需等原生入场动画开放控制才替换单 SP 页面。
                if valid(owner)and owner:IsVisible()and (supply or owner:IsEnableControl()
                    or(self.modelWait and self.modelWait.owner:GetAddress()==owner:GetAddress()))
                    and valid(owner.WidgetTree)and valid(owner.WidgetTree.RootWidget)
                    and valid(owner:GetWorld())and owner:GetWorld():GetAddress()==instance:GetWorld():GetAddress()
                    and not owner:GetFullName():find("WidgetArchetype",1,true)and not owner:GetFullName():find("Default__",1,true)then candidate=owner;break end
            end
            if not candidate then self.ignored=nil;self.ignoredReason=nil;if not self.error then self.status="waiting"end;return end
            if self.closedRoot and self.closedRoot.owner==candidate:GetAddress()then
                releaseClosedRoot();self.ignored=nil;self.ignoredReason=nil
            end
            if candidate:GetAddress()==self.ignored then return end
            local choices=modelChoices(candidate,instance,planeId,options,now);if not choices or #choices~=3 then return end
            self.ignored=candidate:GetAddress();self.ignoredReason="error" -- 接入失败只记录一次，不在每次轮询重试。
            if not panelClass then api()end
            c=measuredAttach(supply and attachSupply or attach,candidate,instance,planeId,profile(),choices)
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
                if c.flow.stage=="sp1"then c.flow.message=tr("请完成所需 SP 选择后确认起飞","Finish selecting your SP loadout before confirming takeoff")
                else c.flow:back()end
            elseif input.accept and not c.previous.accept then
                local result=c.flow:confirm()
                -- SP1、SP2 以及 NO 重选由模组切页，需各补一次原生确认音；YES 自带起飞音。
                if result~="blocked"and result~="takeoff"then confirmSound(c.owner)end
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
            -- 松开这次按键后自动交接，无需第二次按键；原生 Tick 执行实际跳转。
            if not c.handedOff and not input.accept and not input.cancel and not input.up and not input.down then
                c.actionWidget=c.owner:GetAddress() -- 留作取消与读取回执的身份，不再解引用已关闭页面。
                local ok,_,err=preparationAction(c.actionWidget,c.flow.draft.a,c.exitRequested and 2 or 1)
                assert(ok,err)
                c.handedOff=true;c.actionQueued=true;c.actionAt=now;setControl(c.owner,true)
                note(c.exitRequested and "NATIVE_LOADOUT_NATIVE_BACK_QUEUED"or "NATIVE_LOADOUT_NATIVE_DECIDE_QUEUED")
            end
            c.previous=input;return -- 保留刚才的 SP1/SP2 画面，不绘制额外确认页。
        else
            local direction=input.up and not input.down and -1 or input.down and not input.up and 1 or 0
            local oldDirection=c.previous.up and not c.previous.down and -1 or c.previous.down and not c.previous.up and 1 or 0
            if direction~=0 and(direction~=oldDirection or now>=c.repeatAt)then
                c.flow:move(direction);c.repeatAt=now+(direction~=oldDirection and .35 or .12)
            elseif input.cancel and not c.previous.cancel then
                if c.flow.stage=="sp1"then
                    c.exitRequested=true -- 保留三栏直到原生退出，取消草稿不暴露旧单 SP 显示。
                    note("NATIVE_LOADOUT_DRAFT_CANCELLED "..c.planeId)
                else c.flow:back()end
            elseif input.accept and not c.previous.accept then
                local result=c.flow:confirm() -- SP2 的最终决定由原生交接播放，不能重复叠音。
                if result=="sp2"or result=="sp3"then confirmSound(c.owner)end
                c.continueRequested=result=="continue"or nil
            end
        end
        c.previous=input
        if not c.continueRequested and not c.exitRequested then paint(c)end
    end
    function self:pump(context)
        local ok,err=xpcall(function()
            assert(type(context)=="table"and type(context.getProfile)=="function"and type(context.getOptions)=="function","页面更新缺少具名上下文")
            assert(type(context.inFlight)=="boolean"and type(context.panelOpen)=="boolean","页面更新缺少飞行/管理状态")
            local pending=self.modelWait -- 换机、换世界、离页或打开管理界面时结束本次等待，不能遗留输入锁。
            if pending and (not valid(pending.owner)or not pending.owner:IsVisible()or context.planeId~=pending.planeId
                or context.instance:GetWorld():GetAddress()~=pending.world or context.inFlight or context.panelOpen)then
                note("NATIVE_MODEL_WAIT_CANCELLED plane="..pending.planeId)
                clearModelWait(not context.inFlight)
            end
            (standby and pumpStandby or pump)(context.instance,context.planeId,context.getProfile,context.getOptions,context.now,context.inFlight,context.panelOpen)
            refreshPreview(context) -- 每轮核对模型身份和设置，不能被文字绘制签名短路。
        end,debug.traceback)
        if not ok then fail(err)end
    end
    -- 页面激活或补给打开事件中同步接入，不再等待 250 ms 的常规扫描。
    function self:refreshNow(context)
        self.lastScan=-1;self:pump(context)
    end
    -- 直接接入原生激活事件的页面；下一次轮询仍按世界和可见性核对生命周期。
    function self:activate(owner,context)
        if self.modelWait and (not valid(self.modelWait.owner)or self.modelWait.owner:GetAddress()~=owner:GetAddress())then
            note("NATIVE_MODEL_WAIT_CANCELLED reason=page_replaced")
            clearModelWait(true) -- 新页面代次不能继续使用旧页面的等待身份或输入锁。
        end
        -- 保留原生激活事件给出的准确页面，后续无需立即全局发现一次相同对象。
        self.owners={owner};self.ownerWorld=owner:GetWorld():GetAddress();self.lastDiscovery=os.clock()
        self.eventOwner=owner
        self:refreshNow(context)
        self.eventOwner=nil
    end
    -- F8 不能打断已经交给原生处理的一步决定，避免保存事务丢失。
    function self:isPreparationHandoffPending()
        local c=self.cache
        return not supply and not standby and c and (c.continueRequested or c.exitRequested)or false
    end
    function self:reset(options)
        options=options or {}
        local clearError,releaseInput=options.clearError,options.releaseInput
        local ok,err=pcall(function()restore(releaseInput);releaseClosedRoot()end)
        if not ok then
            note((supply and "NATIVE_RESUPPLY_RESTORE_ERROR "or "NATIVE_LOADOUT_RESTORE_ERROR ")..tostring(err))
            if supply then error(err)end
        end
        self.ignored=nil;self.ignoredReason=nil;if clearError~=false then self.error=nil end
        self.phase=nil;self.phaseObject=nil;self.phaseWorld=nil
        self.owners={};self.ownerWorld=nil;self.lastDiscovery=-1;self.eventOwner=nil
        self.status=self.error and "error"or "waiting";self.lastScan=-1
    end
    -- 原生重新启用同一菜单对象时清除关闭标记；模组自己的输入操作不算重进。
    function self:onControlChanged(ctx,enabled)
        if supply or standby or self.controlDepth>0 then return end
        local owner=ctx:get();local address=owner:GetAddress();local c=self.cache
        if (not c or c.owner:GetAddress()~=address)and address~=self.ignored then return end
        local ok,err=xpcall(function()
            if self.transaction then return end -- 原生提交的前后回调之间不能丢掉待保存事务。
            if c and c.owner:GetAddress()==address then
                -- 原生先停用旧页再提交选择；真实决定期间保留事务来源，不能提前清掉草稿。
                if c.handedOff then return end -- 原生关闭期间保留三栏和待确认事务。
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
            if not c.actionQueued and not c.actionAccepted then return end
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
        assert(c and c.handedOff and self.transaction,"原生结束通知早于多 SP 的 YES 请求")
        assert(validateSupplyPhase(c,phase)==self.transaction.phase,"补给结束事件与 YES 事务不匹配")
        assert(c.flow.stage=="review","补给尚未完成兵装选择")
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
        restore(false,true);self.status="committed" -- 已确认模型由 main 在 configureFlight 开始时归还。
        note("NATIVE_RESUPPLY_CONFIRMED "..transaction.planeId)
        return profile
    end
    -- 只读已交接的草稿供起飞过场预建 HUD；此处不保存、不提交、不重建兵装。
    function self:previewSupply()
        assert(supply and self.transaction and self.transaction.finished,"Supply HUD preview requires a finished handoff")
        return copyProfile(self.transaction.profile)
    end
    -- 操作提示可在下一次绘制更新；原生长说明在下次建立补给页面时重新本地化。
    function self:languageChanged()
        if self.cache then self.cache.lastPaint=nil;self.cache.flow.message=nil end
    end
    -- 管理界面和日志可据此区分普通补给编辑与原生起飞交接阶段。
    function self:isSupplyHandoffPending()
        local c=self.cache
        return supply and c and (c.takeoffPending or c.handedOff or self.transaction~=nil) or false
    end
    return self
end
return NativeLoadout
