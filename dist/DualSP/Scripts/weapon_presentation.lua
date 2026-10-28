-- 管理中央简称的常驻或切换后短显；右下角就绪显示由 native_hud 管理。
local path=debug.getinfo(1,"S").source:gsub("^@",""):gsub("\\","/")
local names=dofile(assert(path:match("^(.*)/[^/]+$")).."/weapon_names.lua")
local Presentation={}
-- 中央文本不接受鼠标输入；右下角名称状态由 native_hud 处理。
local TRANSIENT,COLLAPSED,HIT_TEST_INVISIBLE=0x40,1,3
local SLOT_FIELDS={"a","b","c"}
local function valid(widget)return widget and widget:IsValid()end
local function vec(value)return {X=value.X,Y=value.Y}end

-- 每个任务 HUD 独立缓存控件；禁用和世界变化时完整恢复原提示。
function Presentation.new(note)
    -- 状态与扫描计时只属于当前页面，不跨任务复用。
    local self={cache=nil,error=nil,status="waiting",lastScan=-1,waitingSince=nil,serial=0}
    local panelClass,userClass,canvasClass

    -- 进入已有子 UserWidget 的树，MSL 文字位于嵌套的 CombinedStatsWidget。
    local function walk(widget,found)
        if not valid(widget)then return end
        found[widget:GetFName():ToString()]=widget
        if widget:IsA(panelClass)then
            widget.Slots:ForEach(function(_,slot)walk(slot:get().Content,found)end)
        end
        if widget:IsA(userClass)and valid(widget.WidgetTree)then walk(widget.WidgetTree.RootWidget,found)end
    end

    -- 在改控件前核对全部必需 API，不静默使用另一种界面。
    local function preflight()
        for _,api in ipairs({"/Script/UMG.Widget:SetVisibility",
            "/Script/UMG.PanelWidget:RemoveChild","/Script/UMG.CanvasPanel:AddChildToCanvas",
            "/Script/UMG.CanvasPanelSlot:GetAnchors","/Script/UMG.CanvasPanelSlot:GetAlignment",
            "/Script/UMG.CanvasPanelSlot:GetPosition","/Script/UMG.CanvasPanelSlot:GetSize","/Script/UMG.CanvasPanelSlot:GetZOrder",
            "/Script/UMG.CanvasPanelSlot:SetAnchors","/Script/UMG.CanvasPanelSlot:SetAlignment",
            "/Script/UMG.CanvasPanelSlot:SetPosition","/Script/UMG.CanvasPanelSlot:SetSize","/Script/UMG.CanvasPanelSlot:SetZOrder",
            "/Script/UMG.TextBlock:SetText","/Script/UMG.TextBlock:SetFont","/Script/UMG.TextBlock:SetColorAndOpacity",
            "/Script/UMG.TextBlock:SetGlowColor","/Script/UMG.TextBlock:SetUseGlobalGlowColor",
            "/Script/UMG.TextBlock:SetShadowColorAndOpacity"})do
            assert(valid(StaticFindObject(api)),"武器提示 API 不可用："..api)
        end
        panelClass=StaticFindObject("/Script/UMG.PanelWidget")
        userClass=StaticFindObject("/Script/UMG.UserWidget")
        canvasClass=StaticFindObject("/Script/UMG.CanvasPanel")
        assert(valid(panelClass)and valid(userClass)and valid(canvasClass),"武器提示容器类缺失")
    end

    -- 原面板位置按实时 Canvas 槽复制，保留分辨率缩放和居中锚点。
    local function layout(slot)
        local anchors=slot:GetAnchors()
        return {anchors={Minimum=vec(anchors.Minimum),Maximum=vec(anchors.Maximum)},
            alignment=vec(slot:GetAlignment()),position=vec(slot:GetPosition()),size=vec(slot:GetSize()),z=slot:GetZOrder()}
    end
    local function applyLayout(slot,style)
        slot:SetAnchors(style.anchors);slot:SetAlignment(style.alignment)
        slot:SetPosition(style.position);slot:SetSize(style.size);slot:SetZOrder(style.z)
    end

    -- 从树移除本模块创建的控件，不销毁游戏拥有的源控件。
    local function release(widget)
        if valid(widget)and valid(widget.Slot)and valid(widget.Slot.Parent)then
            assert(widget.Slot.Parent:RemoveChild(widget),"无法释放武器提示控件")
        end
    end

    -- 恢复原面板的父容器和位置，关闭时不残留常驻文本。
    local function restore()
        local c=self.cache
        if not c then return end
        if c.sourceMoved and valid(c.sourcePanel)and valid(c.sourceParent)then
            if not valid(c.sourcePanel.Slot)or c.sourcePanel.Slot.Parent:GetAddress()~=c.sourceParent:GetAddress()then
                release(c.sourcePanel)
                local slot=c.sourceParent:AddChildToCanvas(c.sourcePanel)
                assert(valid(slot),"无法恢复原生武器提示面板")
                applyLayout(slot,c.sourceLayout)
            end
            c.sourceMoved=false
        end
        for _,widget in ipairs(c.added)do release(widget)end
        self.cache=nil
    end

    -- 构造任务内唯一的常驻文字，原瞬时面板保留在折叠的独立父容器中。
    local function attach(owner,widgets)
        local c={owner=owner,added={},sourceMoved=false,lastLabel=nil}
        self.cache=c
        c.sourcePanel=widgets.WeaponMessagePanel
        c.sourceText=widgets.WeaponMessageText
        c.sourceParent=c.sourcePanel.Slot.Parent
        assert(c.sourceParent:IsA(canvasClass),"原生武器提示父容器改变")
        c.sourceLayout=layout(c.sourcePanel.Slot)
        -- 创建空容器而非复制带 Slots 的面板，避免复用原生槽指针。
        self.serial=self.serial+1
        c.holder=StaticConstructObject(canvasClass,owner.WidgetTree,FName("DualSPWeaponMessageSource_"..self.serial),TRANSIENT,0,false,false)
        assert(valid(c.holder),"无法建立原生武器提示源容器")
        c.added[#c.added+1]=c.holder
        c.holder:SetVisibility(COLLAPSED)
        assert(valid(c.sourceParent:AddChildToCanvas(c.holder)),"无法保留原生武器提示源容器")

        -- 新建干净文字，只复制所需样式，避免继承原提示的动态颜色绑定和隐藏状态。
        c.label=StaticConstructObject(c.sourceText:GetClass(),owner.WidgetTree,FName("DualSPCurrentWeapon_"..self.serial),TRANSIENT,0,false,false)
        assert(valid(c.label),"无法建立中央武器简称")
        c.label.Justification=c.sourceText.Justification
        c.label.ShadowOffset=vec(c.sourceText.ShadowOffset)
        c.label.bIsTextHidingEnable=false
        c.label:SetUseGlobalGlowColor(false)
        c.label:SetGlowColor({SpecifiedColor={R=.08,G=1,B=.05,A=1},ColorUseRule=0})
        c.label.Slot=nil;c.added[#c.added+1]=c.label
        local labelSlot=c.sourceParent:AddChildToCanvas(c.label)
        assert(valid(labelSlot),"无法接入中央武器简称")
        applyLayout(labelSlot,c.sourceLayout)
        c.label:SetVisibility(COLLAPSED);c.visible=false -- 首次更新决定显隐，短显模式不闪出初始 MSL。
        -- 原提示可能已进入淡出，不能继承那一瞬间的文字 Alpha。
        -- 正常 Alpha 从原生 0.5 提高到 0.7；就绪系数在更新时叠加，不继承原提示的淡出。
        c.tint={R=.08,G=1,B=.05,A=.7} -- 保留原生绿色，提高文字不透明度以改善清晰度。
        local shadow=c.sourceText.ShadowColorAndOpacity -- 阴影与文字使用相同就绪系数。
        c.shadow={R=shadow.R,G=shadow.G,B=shadow.B,A=shadow.A}
        c.fontSize=nil -- 首次更新也应用放大字号；只修改副本自己的字体结构。

        -- 游戏可以继续写入原面板，折叠父容器保证其不会与简称重复显示。
        c.sourceMoved=true
        assert(c.sourceParent:RemoveChild(c.sourcePanel),"无法隔离原生瞬时武器提示")
        local sourceSlot=c.holder:AddChildToCanvas(c.sourcePanel)
        assert(valid(sourceSlot),"无法保留原生瞬时武器提示")
        applyLayout(sourceSlot,c.sourceLayout)
        note("WEAPON_PRESENTATION_ATTACHED "..owner:GetFullName())
        return c
    end

    -- 只在获取新任务 HUD 时扫描；正常飞行使用缓存。
    local function locate(plane,now)
        -- 任务 HUD 生成晚于机体；等待期间短扫描，避免原生短提示先闪现。
        if now-self.lastScan<.05 then return nil end
        self.lastScan=now
        if not panelClass then preflight()end
        local world=plane:GetWorld():GetAddress()
        for _,owner in ipairs(FindAllOf("AceMainFlightHUD_C")or{})do
            if valid(owner)and valid(owner.WidgetTree)and valid(owner:GetWorld())and owner:GetWorld():GetAddress()==world then
                local fullName=owner:GetFullName()
                if not fullName:find("WidgetArchetype",1,true)and not fullName:find("Default__",1,true)then
                    local widgets={};walk(owner.WidgetTree.RootWidget,widgets)
                    local complete=true
                    for _,name in ipairs({"WeaponMessagePanel","WeaponMessageText"})do
                        if not valid(widgets[name])then complete=false end
                    end
                    if complete then return attach(owner,widgets)end
                end
            end
        end
        return nil
    end

    -- 沿用 HUD 周期检查截止时间，不增加逐帧钩子或延迟回调。
    local function update(plane,profile,options,state,enabled,now,mode,gameTime)
        if not enabled then restore();self.status="stock";self.waitingSince=nil;return end
        if self.error then return end
        -- 过场期间 HUD 可以尚未生成，正式接管后才启动缺失诊断。
        if state.pending then self.waitingSince=nil
        elseif not self.waitingSince then self.waitingSince=now end
        local c=self.cache
        if c and not valid(c.owner)then self.cache=nil;c=nil;self.waitingSince=now;self.lastScan=-1 end
        if not c then c=locate(plane,now)end
        if not c then
            self.status="waiting"
            assert(not self.waitingSince or now-self.waitingSince<=5,"无法定位当前任务的中央武器提示")
            return
        end
        assert(valid(c.label)and valid(c.sourcePanel),"中央武器提示已销毁")
        assert(mode=="always"or mode=="switch_1s","中央武器简称显示方式无效")
        assert(type(gameTime)=="number"and gameTime>=0,"中央武器提示缺少游戏时间")
        local selection=state.armed and state.active+2 or 1
        -- 区分最终逻辑槽和配置；MSL 下预载其他 SP 不算切换，失败或重复选择也不重计时。
        local weaponConfig=state.armed and profile[SLOT_FIELDS[state.active+1]]or -1
        if c.mode~=mode then c.hideAt=nil end -- 保存短显模式先清旧计时，保留选择基线以识别同次更新的真实切换。
        if c.lastGameTime and gameTime<c.lastGameTime then
            c.hideAt=nil;c.lastSelection=nil;c.lastConfig=nil -- 时间回退只建立新基线，不伪造切换。
        end
        c.mode=mode;c.lastGameTime=gameTime
        if state.pending then
            c.hideAt=nil;c.lastSelection=nil;c.lastConfig=nil -- 预热的 MSL 占位不是实际切换。
        else
            if mode=="switch_1s"and c.lastSelection and (selection~=c.lastSelection or weaponConfig~=c.lastConfig)then c.hideAt=gameTime+1 end
            c.lastSelection=selection;c.lastConfig=weaponConfig
        end
        local visible=mode=="always"or (not state.pending and c.hideAt~=nil and gameTime<c.hideAt)
        -- 隐藏时停止文字、颜色和字体同步；原生短提示仍被折叠，不会重新冒出。
        if not visible then
            if c.visible then c.label:SetVisibility(COLLAPSED);c.visible=false end
            self.status="native";return
        end
        local label=names.current(profile,options,state)
        if label~=c.lastLabel then c.label:SetText(FText(label));c.lastLabel=label end
        -- 与右下角共用核心就绪结果；至少一发可用即正常，未锁定目标不属于装填状态。
        assert(state.ready and type(state.ready[selection])=="boolean","中央武器提示缺少当前武器就绪状态")
        local factor=state.ready[selection]and 1 or .35 -- 耗尽和全部装填中仅保留正常亮度的35%。
        if factor~=c.lastFactor then
            c.label:SetColorAndOpacity({SpecifiedColor={R=c.tint.R,G=c.tint.G,B=c.tint.B,A=c.tint.A*factor},ColorUseRule=0})
            c.label:SetShadowColorAndOpacity({R=c.shadow.R,G=c.shadow.G,B=c.shadow.B,A=c.shadow.A*factor})
            c.lastFactor=factor
        end
        if c.sourceText.Font.Size~=c.fontSize then
            c.fontSize=c.sourceText.Font.Size
            c.label:SetFont(c.sourceText.Font)
            local font=c.label.Font -- 原生 18 放大为 22，比此前 20 略大；随原生字号保持相同比例。
            font.Size=math.floor(c.fontSize*11/9+.5);c.label:SetFont(font)
            -- 相对接入时保存的原位置上移半个标称字高；反复刷新或字号变化不会累计偏移。
            local origin=c.sourceLayout.position -- 只挪本模块文字，保留原提示的布局供退出时恢复。
            c.label.Slot:SetPosition({X=origin.X,Y=origin.Y-font.Size*.5})
        end
        -- 先同步内容再显示，避免新一次切换短暂露出旧简称或旧装填亮度。
        if not c.visible then c.label:SetVisibility(HIT_TEST_INVISIBLE);c.visible=true end
        self.status="native"
    end

    -- HUD 错误只停用本模块并恢复原提示，错误明确交付 F8，不冻结武器核心。
    function self:fail(err)
        self.error=tostring(err);self.status="error";note("WEAPON_PRESENTATION_ERROR "..self.error)
        local restored,restoreErr=pcall(restore)
        if not restored then note("WEAPON_PRESENTATION_RESTORE_ERROR "..tostring(restoreErr))end
    end
    function self:update(plane,profile,options,state,enabled,now,mode,gameTime)
        local ok,err=xpcall(function()update(plane,profile,options,state,enabled,now,mode,gameTime)end,debug.traceback)
        if not ok then self:fail(err)end
    end
    -- 补给、检查点和世界变化重新绑定页面，绝不复用旧任务控件。
    function self:reset(strict)
        local ok,err=pcall(restore)
        if strict and not ok then error(err)end
        self.error=nil;self.status="waiting";self.waitingSince=nil;self.lastScan=-1
        if not ok then self:fail("重置武器提示失败："..tostring(err))end
    end
    return self
end
return Presentation
