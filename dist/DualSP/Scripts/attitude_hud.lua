-- 微型姿态图形只在启用时构造；Lua维护生命周期和整数读数，原生镜头帧更新两个变换。
local Attitude={}
local TRANSIENT,COLLAPSED,HIT_TEST_INVISIBLE=0x40,1,3
local SCALE=1.3*1.1 -- 在beta4的1.3倍基础上再放大10%。
local function valid(widget)return widget and widget:IsValid()end
local function vec(value)return {X=value.X,Y=value.Y}end
local function layout(slot)
    local a=slot:GetAnchors()
    return {anchors={Minimum=vec(a.Minimum),Maximum=vec(a.Maximum)},alignment=vec(slot:GetAlignment()),
        position=vec(slot:GetPosition()),size=vec(slot:GetSize()),z=slot:GetZOrder(),auto=slot.bAutoSize}
end
local function applyLayout(slot,value)
    slot:SetAnchors(value.anchors);slot:SetAlignment(value.alignment);slot:SetPosition(value.position)
    slot:SetSize(value.size);slot:SetZOrder(value.z);slot:SetAutoSize(value.auto)
end

function Attitude.new(note,invoke,fatal)
    local self={cache=nil,error=nil,available=false,released=true,lastScan=-1,serial=0,player=nil}
    local panelClass,userClass,canvasClass,imageClass,angleFunction,moveFunction,visibilityFunction
    local function stop()
        if self.released then return end
        local ok,_,_,_,reason=invoke(0,0,0,0,0,0,0,24,0,0)
        assert(ok,reason or "Attitude: release failed")
        self.released=true
    end
    local function restoreStock()
        local c=self.cache
        if not c then return end
        if c.stockMoved and valid(c.stockPanel)then
            assert(valid(c.stockParent),"Attitude: native pitch parent expired")
            local alreadyRestored=false
            if valid(c.stockPanel.Slot)and valid(c.stockPanel.Slot.Parent)then
                local parent=c.stockPanel.Slot.Parent
                alreadyRestored=parent:GetAddress()==c.stockParent:GetAddress()
                if not alreadyRestored then
                    assert(valid(c.stockHolder)and parent:GetAddress()==c.stockHolder:GetAddress(),"Attitude: native pitch parent changed")
                    assert(parent:RemoveChild(c.stockPanel),"Attitude: cannot detach native pitch panel")
                end
            end
            if not alreadyRestored then
                local slot=c.stockParent:AddChildToCanvas(c.stockPanel)
                assert(valid(slot),"Attitude: cannot restore native pitch panel");applyLayout(slot,c.stockLayout)
            end
        end
        c.stockMoved=false
        if valid(c.stockHolder)and valid(c.stockHolder.Slot)and valid(c.stockHolder.Slot.Parent)then
            assert(c.stockHolder.Slot.Parent:RemoveChild(c.stockHolder),"Attitude: cannot release native pitch holder")
        end
        c.stockHolder=nil
    end
    function self:release(strict)
        if self.released and not(self.cache and self.cache.stockHolder)then self.available=false;return end
        local ok,err=pcall(stop)
        local c=self.cache
        local restored,reason=pcall(restoreStock)
        local hidden,hideError=pcall(function()if c and valid(c.root)then c.root:SetVisibility(COLLAPSED)end end)
        if not restored then ok=false;err=reason end
        if not hidden then ok=false;err=hideError end
        self.available=false
        if not ok then
            self.error=tostring(err);note("ATTITUDE_RELEASE_ERROR "..self.error)
            if strict then error(self.error)end
            if not self.cleanupReported then
                self.cleanupReported=true
                if fatal then fatal(self.error)end
            end
        end
    end
    function self:reset(strict)
        local failure
        local ok,err=pcall(function()self:release(true)end)
        if not ok then failure=tostring(err)end
        local c=self.cache
        if c and valid(c.root)and valid(c.root.Slot)and valid(c.root.Slot.Parent)then
            local removed,reason=pcall(function()assert(c.root.Slot.Parent:RemoveChild(c.root),"Attitude: cannot release root")end)
            if not removed then failure=failure or tostring(reason)end
        end
        self.cache=nil;self.lastScan=-1;self.waitingSince=nil;self.player=nil;self.available=false
        if failure then self.error=failure;note("ATTITUDE_RESET_ERROR "..failure);if strict then error(failure)end
        else self.error=nil end
    end
    local function preflight()
        for _,api in ipairs({"/Script/UMG.CanvasPanel:AddChildToCanvas","/Script/UMG.CanvasPanelSlot:SetAutoSize",
            "/Script/UMG.Widget:SetRenderAngle","/Script/UMG.Widget:SetRenderTranslation","/Script/UMG.Widget:SetRenderTransformPivot","/Script/UMG.Widget:SetRenderScale",
            "/Script/UMG.Widget:SetVisibility","/Script/UMG.Image:SetBrush","/Script/UMG.Image:SetColorAndOpacity",
            "/Script/UMG.Image:SetGlowColor","/Script/UMG.Image:SetUseGlobalGlowColor",
            "/Script/UMG.TextBlock:SetGlowColor","/Script/UMG.TextBlock:SetUseGlobalGlowColor"})do
            assert(valid(StaticFindObject(api)),"Attitude HUD API unavailable: "..api)
        end
        panelClass=StaticFindObject("/Script/UMG.PanelWidget");userClass=StaticFindObject("/Script/UMG.UserWidget")
        canvasClass=StaticFindObject("/Script/UMG.CanvasPanel")
        imageClass=StaticFindObject("/Script/UMG.Image")
        angleFunction=StaticFindObject("/Script/UMG.Widget:SetRenderAngle")
        moveFunction=StaticFindObject("/Script/UMG.Widget:SetRenderTranslation")
        visibilityFunction=StaticFindObject("/Script/UMG.Widget:SetVisibility")
        assert(valid(panelClass)and valid(userClass)and valid(canvasClass)and valid(imageClass),"Attitude HUD classes unavailable")
    end
    local function walk(widget,result,tintParents)
        if not valid(widget)then return end
        local name=widget:GetFName():ToString()
        result[name]=widget
        if name=="SpeedText"then result.attitudeTintParents=tintParents end
        if widget:IsA(panelClass)then widget.Slots:ForEach(function(_,slot)walk(slot:get().Content,result,tintParents)end)end
        -- SPEED位于PersistentWidget子界面；UserWidget自身没有PanelWidget的Slots。
        if widget:IsA(userClass)and valid(widget.WidgetTree)then
            local nested={};for index,parent in ipairs(tintParents)do nested[index]=parent end
            nested[#nested+1]=widget
            walk(widget.WidgetTree.RootWidget,result,nested)
        end
    end
    local function construct(class,tree,name)
        self.serial=self.serial+1
        local object=StaticConstructObject(class,tree,FName("DualSPAttitude"..name.."_"..self.serial),TRANSIENT,0,false,false)
        assert(valid(object),"Attitude: cannot construct "..name);return object
    end
    local function place(slot,x,y,w,h)
        slot:SetPosition({X=x,Y=y});slot:SetSize({X=w,Y=h});slot:SetAutoSize(false)
    end
    local function retainStock(c)
        if c.mask%4==0 or c.stockMoved then return end
        assert(valid(c.stockPanel)and c.stockPanel:IsA(canvasClass)and valid(c.stockPanel.Slot),"Attitude: native pitch panel unavailable")
        c.stockParent=c.stockPanel.Slot.Parent
        assert(valid(c.stockParent)and c.stockParent:IsA(canvasClass),"Attitude: native pitch parent layout changed")
        assert(c.stockParent:GetAddress()==c.panel.Slot.Parent:GetAddress(),"Attitude: native pitch panel left the speed HUD group")
        c.stockLayout=layout(c.stockPanel.Slot)
        c.stockHolder=construct(canvasClass,c.owner.WidgetTree,"NativeHolder")
        c.stockHolder:SetVisibility(HIT_TEST_INVISIBLE)
        local slot=c.stockParent:AddChildToCanvas(c.stockHolder)
        assert(valid(slot),"Attitude: cannot attach native pitch holder")
        -- 包装器铺满原父容器，原控件沿用同一套锚点/偏移；只切换包装器的显隐。
        slot:SetAnchors({Minimum={X=0,Y=0},Maximum={X=1,Y=1}});slot:SetAlignment({X=0,Y=0})
        slot:SetPosition({X=0,Y=0});slot:SetSize({X=0,Y=0});slot:SetAutoSize(false);slot:SetZOrder(c.stockLayout.z)
        c.stockMoved=true -- 分离之后的任何错误都必须尝试归还原层级。
        assert(c.stockParent:RemoveChild(c.stockPanel),"Attitude: cannot retain native pitch panel")
        slot=c.stockHolder:AddChildToCanvas(c.stockPanel)
        assert(valid(slot),"Attitude: cannot keep native pitch panel");applyLayout(slot,c.stockLayout)
    end
    local function attach(owner,widgets,mask)
        local source=widgets.SpeedText
        local panel=widgets.SpeedMeterPanel
        assert(source.Slot.Parent:GetAddress()==panel:GetAddress()and panel:IsA(canvasClass),"Attitude: speed label layout changed")
        local c={owner=owner,source=source,panel=panel,lines={},texts={},stockPanel=widgets.PitchMeterPanel,mask=mask,
            tintParents=assert(widgets.attitudeTintParents,"Attitude: speed HUD tint ancestry unavailable")}
        self.cache=c -- 先登记所有权，部分构造错误也可以隐藏/归还。
        c.root=construct(canvasClass,owner.WidgetTree,"Root");c.root:SetVisibility(COLLAPSED)
        c.slot=panel:AddChildToCanvas(c.root);assert(valid(c.slot),"Attitude: cannot attach root")
        local size=source.Font.Size/20*SCALE
        c.scale=size
        local function canvas(parent,name,x,y,w,h)
            local object=construct(canvasClass,owner.WidgetTree,name)
            local slot=parent:AddChildToCanvas(object);assert(valid(slot),"Attitude: cannot attach canvas")
            place(slot,x*size,y*size,w*size,h*size);object:SetVisibility(HIT_TEST_INVISIBLE)
            return object
        end
        local function patch(parent,x,y,w,h,angle,alpha,absoluteAlpha)
            local object=construct(imageClass,owner.WidgetTree,"Line")
            local brush=object.Brush;brush.DrawAs=1;object:SetBrush(brush);object.bIsImageHidingEnable=false
            object:SetGlowColor(source.GlowColor);object:SetUseGlobalGlowColor(source.bUseGlobalGlowColor)
            local slot=parent:AddChildToCanvas(object);assert(valid(slot),"Attitude: cannot attach line")
            place(slot,x*size,y*size,w*size,h*size)
            object:SetRenderTransformPivot({X=.5,Y=.5});object:SetRenderAngle(angle or 0)
            object:SetVisibility(HIT_TEST_INVISIBLE)
            c.lines[#c.lines+1]={widget=object,alpha=alpha or 1,absoluteAlpha=absoluteAlpha}
        end
        local function line(parent,x,y,w,h,angle,alpha,absoluteAlpha)
            -- 两层低透明度边缘包住实体线；沿整条边羽化，减轻旋转时的硬台阶。
            alpha=alpha or 1
            patch(parent,x-.6,y-.6,w+1.2,h+1.2,angle,alpha*.16,absoluteAlpha)
            patch(parent,x-.3,y-.3,w+.6,h+.6,angle,alpha*.32,absoluteAlpha)
            patch(parent,x,y,w,h,angle,alpha,absoluteAlpha)
        end
        local function text(x,y,w,value,justification)
            local object=construct(source:GetClass(),owner.WidgetTree,"Text")
            object.Justification=justification;object.Margin={Left=0,Top=0,Right=0,Bottom=0};object.bIsTextHidingEnable=false
            object.ShadowOffset=vec(source.ShadowOffset);object:SetFont(source.Font)
            local font=object.Font;font.Size=math.max(1,math.floor(source.Font.Size*.7*SCALE+.5));object:SetFont(font)
            object:SetGlowColor(source.GlowColor);object:SetUseGlobalGlowColor(source.bUseGlobalGlowColor)
            object:SetText(FText(value));object:SetVisibility(HIT_TEST_INVISIBLE)
            local slot=c.root:AddChildToCanvas(object);assert(valid(slot),"Attitude: cannot attach text")
            place(slot,x*size,y*size,w*size,18*size);c.texts[#c.texts+1]=object;return object
        end
        text(14,0,22,"P",2);c.pitch=text(39,0,65,"---",0)
        text(14,70,22,"B",2);c.bank=text(39,70,65,"---",0)
        -- 地平圈整体反向滚转；中央倒T另建固定容器，不参与旋转。
        c.horizon=canvas(c.root,"Horizon",36,20,48,48)
        c.horizon:SetRenderTransformPivot({X=.5,Y=.5})
        -- 下半圆用连续横带填满，外缘藏在圆环实体线下；只在构造时生成。
        local radius,bands=23.2,32
        local step=radius/bands
        for index=0,bands-1 do
            local y=(index+.5)*step
            local halfWidth=math.sqrt(radius*radius-y*y)
            patch(c.horizon,24-halfWidth,24+index*step,halfWidth*2,step,0,.18,true)
        end
        local segments=64
        for index=0,segments-1 do
            local a,b=index*2*math.pi/segments,(index+1)*2*math.pi/segments
            local x1,y1=24+24*math.cos(a),24+24*math.sin(a)
            local x2,y2=24+24*math.cos(b),24+24*math.sin(b)
            local length=math.sqrt((x2-x1)^2+(y2-y1)^2)
            line(c.horizon,(x1+x2)/2-length/2-.15,(y1+y2)/2-.8,length+.3,1.6,(a+b)/2*180/math.pi+90,.7)
        end
        -- 天地仅由填色边界区分，不再额外绘制贯穿圆圈的细水平线。
        for _,point in ipairs({{24,0},{48,24},{24,48},{0,24}})do
            if point[1]==24 then line(c.horizon,point[1]-.8,point[2]-2.5,1.6,5,0,.8)
            else line(c.horizon,point[1]-2.5,point[2]-.8,5,1.6,0,.8)end
        end
        line(c.root,21.2,20,1.6,48,0,.7)
        for _,y in ipairs({20,44,68})do line(c.root,18,y-.8,8,1.6,0,.8)end
        c.cursor=canvas(c.root,"Cursor",23,41,7,6)
        -- 菱形截取右半部后横向拉伸，形成真正填满的右指三角形，无逐行拼接缝。
        local triangle=canvas(c.cursor,"TriangleClip",0,0,3,6)
        triangle.Clipping=1;triangle:SetRenderTransformPivot({X=0,Y=0});triangle:SetRenderScale({X=2,Y=1})
        local side=6/math.sqrt(2)
        line(triangle,-side/2,3-side/2,side,side,45,1)
        local aircraft=canvas(c.root,"Aircraft",36,20,48,48)
        aircraft.Slot:SetZOrder(1) -- 固定机体符号盖在地面填色上。
        line(aircraft,10,22.9,28,2.2,0,1)
        line(aircraft,22.9,12,2.2,12,0,1)
        line(aircraft,22.3,22.3,3.4,3.4,0,1)
        -- 直接以原生速度面板为父级，在整个数值区底边下留12个逻辑像素。
        local anchors=source.Slot:GetAnchors()
        assert(math.abs(anchors.Maximum.X-anchors.Minimum.X)<.001,"Attitude: speed label horizontal anchors changed")
        c.slot:SetAnchors({Minimum={X=anchors.Minimum.X,Y=1},Maximum={X=anchors.Minimum.X,Y=1}})
        c.slot:SetAlignment({X=.5,Y=0})
        local origin,alignment,dimensions=source.Slot:GetPosition(),source.Slot:GetAlignment(),source.Slot:GetSize()
        local width=dimensions.X
        if width<=0 then source:ForceLayoutPrepass();width=source:GetDesiredSize().X end
        place(c.slot,origin.X+(.5-alignment.X)*width,12*size,110*size,88*size)
        c.slot:SetZOrder(source.Slot:GetZOrder()+1)
        note("ATTITUDE_HUD_ATTACHED "..owner:GetFullName())
        return c
    end
    local function locate(plane,now,mask)
        if now-self.lastScan<.05 then return nil end
        self.lastScan=now
        if not panelClass then preflight()end
        local world=plane:GetWorld():GetAddress()
        for _,owner in ipairs(FindAllOf("AceMainFlightHUD_C")or{})do
            if valid(owner)and valid(owner.WidgetTree)and valid(owner:GetWorld())and owner:GetWorld():GetAddress()==world then
                local name=owner:GetFullName()
                if not name:find("WidgetArchetype",1,true)and not name:find("Default__",1,true)then
                    local widgets={};walk(owner.WidgetTree.RootWidget,widgets,{owner})
                    if valid(widgets.SpeedText)and valid(widgets.SpeedMeterPanel)then return attach(owner,widgets,mask)end
                end
            end
        end
    end
    local function tintParentsValid(c)
        for _,parent in ipairs(c.tintParents)do if not valid(parent)then return false end end
        return true
    end
    local function update(plane,mask,now)
        if mask==0 then self:release();self.waitingSince=nil;return end
        local address=plane:GetAddress()
        if self.player~=address then self:reset(true);self.player=address end
        if self.error then return end
        local c=self.cache
        if c and (not valid(c.owner)or not valid(c.root)or not valid(c.source)or not valid(c.horizon)or not valid(c.cursor)or not tintParentsValid(c)
            or (mask%4~=0 and not valid(c.stockPanel)))then
            self:reset(true);self.player=address;c=nil
        end
        -- 字号变化时重新构造少量静态线段，正常帧只调用一次轻量控制接口。
        if c and (c.scale~=c.source.Font.Size/20*SCALE or c.mask~=mask)then self:reset(true);self.player=address;c=nil end
        if not c then
            c=locate(plane,now,mask)
            if not c then
                self.waitingSince=self.waitingSince or now
                assert(now-self.waitingSince<=5,"Attitude: cannot locate native speed HUD");return
            end
        end
        local color=c.source.ColorAndOpacity.SpecifiedColor
        -- 原生PersistentWidget会把所有后代乘上绿色。只补偿自有警示数字，保留整棵原生HUD的颜色。
        local tintR,tintG,tintB=1,1,1
        for _,parent in ipairs(c.tintParents)do
            assert(valid(parent),"Attitude: HUD tint ancestor expired")
            local tint=parent.ColorAndOpacity
            tintR=tintR*tint.R;tintG=tintG*tint.G;tintB=tintB*tint.B
        end
        local glow=c.source.GlowColor.SpecifiedColor
        local signature=table.concat({color.R,color.G,color.B,color.A,tintR,tintG,tintB,glow.R,glow.G,glow.B,glow.A,
            tostring(c.source.bUseGlobalGlowColor)},":")
        local styleChanged=c.style~=signature
        if styleChanged then
            for _,entry in ipairs(c.lines)do
                entry.widget:SetColorAndOpacity({R=color.R,G=color.G,B=color.B,A=(entry.absoluteAlpha and 1 or color.A)*entry.alpha})
            end
            for _,object in ipairs(c.texts)do object:SetColorAndOpacity(c.source.ColorAndOpacity);object:SetShadowColorAndOpacity(c.source.ShadowColorAndOpacity)end
            c.style=signature
        end
        self.released=false -- 跨接口失败也必须尝试停止原生观察责任。
        retainStock(c)
        local ok,available,pitch,bank,reason,pitchWarning,bankWarning=invoke(address,c.root:GetAddress(),c.horizon:GetAddress(),c.cursor:GetAddress(),
            angleFunction:GetAddress(),moveFunction:GetAddress(),visibilityFunction:GetAddress(),24*c.scale,mask,c.stockHolder and c.stockHolder:GetAddress()or 0)
        assert(ok,reason or "Attitude: control failed")
        self.available=available
        if pitch~=""then c.pitch:SetText(FText(pitch))end
        if bank~=""then c.bank:SetText(FText(bank))end
        local function numberStyle(object,warning,previous)
            if not styleChanged and warning==previous then return end
            -- 使用线性色补偿，允许本地分量大于1；父级相乘后才得到目标红色，不额外降低Alpha。
            local tint=warning and {SpecifiedColor={R=1/math.max(tintR,.001),G=.12/math.max(tintG,.001),
                B=.08/math.max(tintB,.001),A=color.A},ColorUseRule=0}or c.source.ColorAndOpacity
            object:SetColorAndOpacity(tint)
            object:SetUseGlobalGlowColor(not warning and c.source.bUseGlobalGlowColor)
            if warning then
                local red=tint.SpecifiedColor
                object:SetGlowColor({SpecifiedColor={R=red.R,G=red.G,B=red.B,A=glow.A},ColorUseRule=0})
            else object:SetGlowColor(c.source.GlowColor)end
        end
        numberStyle(c.pitch,pitchWarning,c.pitchWarning);numberStyle(c.bank,bankWarning,c.bankWarning)
        c.pitchWarning=pitchWarning;c.bankWarning=bankWarning
    end
    function self:update(plane,mask,now)
        local ok,err=xpcall(function()update(plane,mask,now)end,debug.traceback)
        if not ok then
            self.error=tostring(err);note("ATTITUDE_HUD_ERROR "..self.error)
            self:release()
        end
    end
    return self
end
return Attitude
