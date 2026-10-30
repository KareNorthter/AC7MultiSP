-- 可选PW武器布局：复用任务原生HUD的字体、颜色、泛光和祖先缩放，不读取游戏对象。
local path=debug.getinfo(1,"S").source:gsub("^@",""):gsub("\\","/")
local root=assert(path:match("^(.*)/[^/]+$"))
local Names=dofile(root.."/weapon_names.lua")
local Profile=dofile(root.."/loadout_profile.lua")
local PW={}
local TRANSIENT,COLLAPSED,HIT_TEST_INVISIBLE=0x40,1,3
local serial=0
local outline={{1,0},{2,0},{2,1},{3,1},{3,2},{2,2},{2,3},{1,3},{1,2},{0,2},{0,1},{1,1},{1,0}}
local directions={{.5,1.5},{1.5,.5},{2.5,1.5},{1.5,2.5}}
local function valid(widget)return widget and widget:IsValid()end
local function vec(value)return {X=value.X,Y=value.Y}end
local function identity(prefix)
    serial=serial+1;return FName(prefix.."_"..serial)
end
local function rect(slot,x,y,w,h)
    slot:SetPosition({X=x,Y=y});slot:SetSize({X=w,Y=h})
end
function PW.unit(c)
    -- 列表原字体有负边距，十字布局不能照搬偏小的行高；为数量与名称留足字高。
    local size=math.max(c.mainFontSize,c.nameFontSize,c.countFontSize,c.widgets.MainWeaponCountTextBP.Font.Size,
        c.widgets.GunTextBP.Font.Size,c.widgets.GunCountTextBP.Font.Size)
    return math.max(c.row,math.ceil(size*1.5))
end
function PW.wheelRows(c)return c.countSP==3 and 6.8 or 4.5 end
function PW.reserveHeight(c)return (PW.wheelRows(c)+.35)*PW.unit(c)end
function PW.compactStats(c)
    local gap=c.row*.12
    for _,column in ipairs(c.columns)do
        for _,index in ipairs({1,4,5})do
            local entry=column.original[index]
            local padding=entry.style.padding
            entry.widget.Slot:SetPadding({Left=padding.Left,Top=padding.Top,Right=padding.Right,
                Bottom=padding.Bottom-(index==5 and 0 or gap)})
        end
    end
    return gap*2 -- 两个行间隙缩短的总高度，由父布局补偿以维持DMG底部基准。
end

-- 只在首次启用PW时核对专用接口；默认列表不依赖这些控件。
local function classes()
    for _,api in ipairs({"/Script/UMG.Image:SetBrush","/Script/UMG.Image:SetColorAndOpacity",
        "/Script/UMG.Image:SetGlowColor","/Script/UMG.Image:SetUseGlobalGlowColor",
        "/Script/UMG.TextBlock:SetGlowColor","/Script/UMG.TextBlock:SetUseGlobalGlowColor",
        "/Script/UMG.CanvasPanelSlot:SetAutoSize"})do
        assert(valid(StaticFindObject(api)),"PW HUD API unavailable: "..api)
    end
    local canvas,image=StaticFindObject("/Script/UMG.CanvasPanel"),StaticFindObject("/Script/UMG.Image")
    assert(valid(canvas)and valid(image),"PW HUD widget classes unavailable")
    return canvas,image
end

-- 透明度系数仅影响本模块的副本；同色同亮度不重复写控件。
local function tintText(entry,source,factor)
    local color,shadow=source.ColorAndOpacity.SpecifiedColor,source.ShadowColorAndOpacity
    local signature=table.concat({color.R,color.G,color.B,color.A*factor,shadow.R,shadow.G,shadow.B,shadow.A*factor},":")
    if entry.style==signature then return end
    entry.widget:SetColorAndOpacity({SpecifiedColor={R=color.R,G=color.G,B=color.B,A=color.A*factor},ColorUseRule=0})
    entry.widget:SetShadowColorAndOpacity({R=shadow.R,G=shadow.G,B=shadow.B,A=shadow.A*factor})
    entry.style=signature
end

function PW.attach(c)
    local self={cells={},edges={},frame={},cache=c,visible=false}
    c.pw=self -- 部分构造失败也由native_hud统一隐藏/释放，不遗留半幅UI。
    function self:hide()
        if valid(self.root)then self.root:SetVisibility(COLLAPSED)end
        self.visible=false;self.layoutKey=nil;self.frameKey=nil
    end
    function self:release()
        if valid(self.root)and valid(self.root.Slot)and valid(self.root.Slot.Parent)then
            assert(self.root.Slot.Parent:RemoveChild(self.root),"Cannot release PW HUD root")
        end
    end
    local canvasClass,imageClass=classes()
    local tree=c.owner.WidgetTree
    local function canvas(parent,prefix)
        local widget=StaticConstructObject(canvasClass,tree,identity(prefix),TRANSIENT,0,false,false)
        assert(valid(widget),"Cannot construct PW HUD canvas")
        local slot=parent:AddChildToCanvas(widget)
        assert(valid(slot),"Cannot attach PW HUD canvas")
        widget:SetVisibility(HIT_TEST_INVISIBLE)
        return widget,slot
    end
    -- 根容器与原生统计HBox共父容器、共布局，选择区和统计栏对齐原生右边界。
    self.root=StaticConstructObject(canvasClass,tree,identity("DualSPPwRoot"),TRANSIENT,0,false,false)
    assert(valid(self.root),"Cannot construct PW HUD root")
    self.root:SetVisibility(COLLAPSED)
    assert(c.hboxSlot.Parent:IsA(canvasClass),"PW HUD stats parent must be a canvas")
    self.rootSlot=c.hboxSlot.Parent:AddChildToCanvas(self.root)
    assert(valid(self.rootSlot),"Cannot attach PW HUD root")
    self.rootSlot:SetAutoSize(false)
    self.wheel,self.wheelSlot=canvas(self.root,"DualSPPwWheel")
    self.wheelSlot:SetAnchors({Minimum={X=1,Y=0},Maximum={X=1,Y=0}})
    self.wheelSlot:SetAlignment({X=1,Y=1});self.wheelSlot:SetAutoSize(false)
    local function text(parent,source,prefix)
        -- 整份克隆原生GUN模板，保留字体材质、字面样式和辉光参数，避免干净TextBlock漏掉效果。
        local widget=StaticConstructObject(source:GetClass(),tree,identity(prefix),TRANSIENT,0,false,false,source)
        assert(valid(widget),"Cannot construct PW HUD text")
        widget.Slot=nil;widget:SetRenderTranslation({X=0,Y=0})
        widget.Justification=1;widget.Margin={Left=0,Top=0,Right=0,Bottom=0}
        widget.ShadowOffset=vec(source.ShadowOffset);widget.bIsTextHidingEnable=false
        widget:SetFont(source.Font)
        widget:SetGlowColor(source.GlowColor);widget:SetUseGlobalGlowColor(source.bUseGlobalGlowColor)
        local slot=parent:AddChildToCanvas(widget)
        assert(valid(slot),"Cannot attach PW HUD text")
        slot:SetAutoSize(false);slot:SetZOrder(1);widget:SetVisibility(HIT_TEST_INVISIBLE)
        return {widget=widget,slot=slot,source=source}
    end
    local function line()
        local widget=StaticConstructObject(imageClass,tree,identity("DualSPPwLine"),TRANSIENT,0,false,false)
        assert(valid(widget),"Cannot construct PW HUD line")
        local slot=self.wheel:AddChildToCanvas(widget)
        assert(valid(slot),"Cannot attach PW HUD line")
        local brush=widget.Brush;brush.DrawAs=1 -- 无外部贴图的纯色Box画刷。
        widget:SetBrush(brush);widget.bIsImageHidingEnable=false
        widget:SetGlowColor(c.widgets.GunTextBP.GlowColor);widget:SetUseGlobalGlowColor(c.widgets.GunTextBP.bUseGlobalGlowColor)
        widget:SetVisibility(HIT_TEST_INVISIBLE);slot:SetAutoSize(false)
        return {widget=widget,slot=slot}
    end
    for index=1,4 do
        local box,slot=canvas(self.wheel,"DualSPPwWeapon")
        local nameSource=c.widgets.GunTextBP
        local countSource=c.widgets.GunCountTextBP
        local cell={widget=box,slot=slot,name=text(box,nameSource,"DualSPPwName"),count=text(box,countSource,"DualSPPwCount"),
            number=text(self.wheel,nameSource,"DualSPPwSlot")}
        cell.number.widget:SetText(FText(tostring(index)));self.cells[index]=cell
    end
    for index=1,#outline-1 do self.edges[index]=line()end
    for index=1,4 do self.frame[index]=line()end
    self.separator=line()

    -- 布局只随行高、字号、简称、槽数或重新显示变化，不随每发弹药重新测量。
    local function layout(labels)
        local r=PW.unit(c)
        local cellWidth=2.75*r
        local frameWidth=0
        for index,cell in ipairs(self.cells)do
            -- 名称与数量直接采用原生字号；槽号仍是辅助小字，不通过缩小主字体压缩布局。
            cell.name.widget:SetFont(cell.name.source.Font)
            cell.count.widget:SetFont(cell.count.source.Font)
            local font=cell.number.widget.Font
            font.Size=math.max(1,math.floor(cell.name.source.Font.Size*.55+.5));cell.number.widget:SetFont(font)
            if labels[index]then
                cell.name.widget:ForceLayoutPrepass()
                cell.nameWidth=cell.name.widget:GetDesiredSize().X
                frameWidth=math.max(frameWidth,cell.nameWidth+.35*r)
            end
        end
        cellWidth=math.max(cellWidth,frameWidth)
        local cross,gap=1.75*r,.18*r
        local width=cellWidth*2+cross+gap*2
        local height=PW.wheelRows(c)*r
        local center=width/2
        local positions={{0,2.05*r},{center-cellWidth/2,0},{width-cellWidth,2.05*r},{center-cellWidth/2,4.4*r}}
        local crossX,crossY=center-cross/2,2.25*r
        local source=c.hboxSlot
        local anchors=source:GetAnchors()
        self.rootSlot:SetAnchors({Minimum=vec(anchors.Minimum),Maximum=vec(anchors.Maximum)})
        self.rootSlot:SetAlignment(vec(source:GetAlignment()));self.rootSlot:SetPosition(vec(source:GetPosition()))
        local size=source.bAutoSize and vec(c.hbox:GetDesiredSize())or vec(source:GetSize())
        self.rootSlot:SetSize(size);self.rootSlot:SetZOrder(source:GetZOrder()+1)
        -- 撤去旧版共同左移：统计栏回到原生位置，选择区从同一右边界向左展开。
        local nativeWidth=size.X
        if anchors.Minimum.X~=anchors.Maximum.X or nativeWidth<=0 then
            c.hbox:ForceLayoutPrepass();nativeWidth=c.hbox:GetDesiredSize().X
        end
        assert(nativeWidth>0,"PW HUD stats width is invalid")
        local offset=vec(c.hboxTranslation)
        self.root:SetRenderTranslation(offset);c.hbox:SetRenderTranslation(offset)
        rect(self.wheelSlot,0,-.35*r,width,height)
        for index,cell in ipairs(self.cells)do
            local position=positions[index]
            rect(cell.slot,position[1],position[2],cellWidth,2.15*r)
            local inset=index==3 and cellWidth-frameWidth or (cellWidth-frameWidth)/2
            -- 数量保留完整列宽以容纳六位库存；右侧数量沿边界内缩少量后右对齐。
            cell.count.widget.Justification=index==3 and 2 or 1
            rect(cell.count.slot,0,0,cellWidth-(index==3 and .175*r or 0),1.05*r)
            rect(cell.name.slot,inset,1.05*r,frameWidth,1.1*r)
            cell.frame={x=position[1]+inset,y=position[2]+1.01*r,w=frameWidth,h=1.14*r}
            local direction=directions[index]
            rect(cell.number.slot,crossX+(direction[1]-.5)*cross/3,crossY+(direction[2]-.5)*cross/3+.04*r,cross/3,cross/3)
        end
        local stroke=math.max(2,math.floor(.06*r+.5)) -- 十字、选中框和分隔线共用等粗线宽。
        for index,entry in ipairs(self.edges)do
            local a,b=outline[index],outline[index+1]
            rect(entry.slot,crossX+math.min(a[1],b[1])*cross/3-stroke/2,crossY+math.min(a[2],b[2])*cross/3-stroke/2,
                math.max(math.abs(a[1]-b[1])*cross/3,stroke),math.max(math.abs(a[2]-b[2])*cross/3,stroke))
        end
        rect(self.separator.slot,math.max(0,width-nativeWidth),height-stroke,math.min(width,nativeWidth),stroke)
        self.stroke=stroke;self.frameKey=nil
    end
    function self:update(profile,options,state)
        assert(valid(self.root)and valid(self.wheel),"PW HUD widgets were destroyed")
        local labels={"MSL"}
        for index=1,c.countSP do labels[index+1]=Names.special(options,profile[Profile.fields[index]])end
        local position,size,anchors,alignment=c.hboxSlot:GetPosition(),c.hboxSlot:GetSize(),c.hboxSlot:GetAnchors(),c.hboxSlot:GetAlignment()
        local desired=c.hboxSlot.bAutoSize and c.hbox:GetDesiredSize()or {X=0,Y=0}
        local layoutKey=table.concat({c.row,c.mainFontSize,c.nameFontSize,c.countFontSize,c.widgets.MainWeaponCountTextBP.Font.Size,
            c.widgets.GunTextBP.Font.Size,c.widgets.GunCountTextBP.Font.Size,
            c.countSP,labels[1],labels[2]or "",labels[3]or "",labels[4]or "",position.X,position.Y,size.X,size.Y,
            anchors.Minimum.X,anchors.Minimum.Y,anchors.Maximum.X,anchors.Maximum.Y,alignment.X,alignment.Y,
            desired.X,desired.Y,c.hboxSlot:GetZOrder()},":")
        for index,cell in ipairs(self.cells)do
            local present=labels[index]~=nil
            if cell.present~=present then
                cell.widget:SetVisibility(present and HIT_TEST_INVISIBLE or COLLAPSED)
                cell.number.widget:SetVisibility(present and HIT_TEST_INVISIBLE or COLLAPSED);cell.present=present
            end
            if present then
                assert(valid(cell.name.widget)and valid(cell.count.widget)and valid(cell.number.widget),"PW HUD text was destroyed")
                local label=labels[index]
                local ammo=state.main_ammo
                if index>1 then ammo=state.cached_sp[index-1]end
                local count=state.pending and "—"or Profile.ammoText(ammo)
                if cell.label~=label then cell.name.widget:SetText(FText(label));cell.label=label end
                if cell.ammo~=count then cell.count.widget:SetText(FText(count));cell.ammo=count end
                assert(state.ready and type(state.ready[index])=="boolean","PW HUD readiness is missing")
                tintText(cell.name,cell.name.source,Names.brightness(label,state.ready[index],ammo))
                tintText(cell.count,cell.count.source,state.pending and 1 or (Profile.hasAmmo(ammo)and 1 or .35))
                tintText(cell.number,cell.number.source,1)
            end
        end
        if self.layoutKey~=layoutKey then layout(labels);self.layoutKey=layoutKey end
        local selected=state.armed and state.active+2 or 1
        assert(labels[selected],"PW HUD selection does not match a carried weapon")
        if self.frameKey~=selected then
            local box=self.cells[selected].frame
            local x,y,w,h,t=box.x,box.y,box.w,box.h,self.stroke
            rect(self.frame[1].slot,x,y,w,t);rect(self.frame[2].slot,x,y+h-t,w,t)
            rect(self.frame[3].slot,x,y,t,h);rect(self.frame[4].slot,x+w-t,y,t,h)
            self.frameKey=selected
        end
        local color=c.widgets.GunTextBP.ColorAndOpacity.SpecifiedColor
        local signature=table.concat({color.R,color.G,color.B,color.A},":")
        if self.lineStyle~=signature then
            for _,group in ipairs({self.edges,self.frame,{self.separator}})do
                for _,entry in ipairs(group)do entry.widget:SetColorAndOpacity({R=color.R,G=color.G,B=color.B,A=color.A})end
            end
            self.lineStyle=signature
        end
        if not self.visible then self.root:SetVisibility(HIT_TEST_INVISIBLE);self.visible=true end
    end
    return self
end
return PW
