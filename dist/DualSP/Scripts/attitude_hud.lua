-- 微型姿态图形只在启用时构造；Lua维护生命周期和整数读数，原生镜头帧更新两个变换。
local Attitude={}
local TRANSIENT,COLLAPSED,HIT_TEST_INVISIBLE=0x40,1,3
local function valid(widget)return widget and widget:IsValid()end
local function vec(value)return {X=value.X,Y=value.Y}end

function Attitude.new(note,invoke)
    local self={cache=nil,error=nil,available=false,released=true,lastScan=-1,serial=0,player=nil}
    local panelClass,canvasClass,imageClass,angleFunction,moveFunction,visibilityFunction
    local function stop()
        if self.released then return end
        local ok,_,_,_,reason=invoke(0,0,0,0,0,0,0,24)
        assert(ok,reason or "Attitude: release failed")
        self.released=true
    end
    function self:release(strict)
        if self.released then self.available=false;return end
        local ok,err=pcall(stop)
        local c=self.cache
        if c and valid(c.root)then c.root:SetVisibility(COLLAPSED)end
        self.available=false
        if not ok then
            self.error=tostring(err);note("ATTITUDE_RELEASE_ERROR "..self.error)
            if strict then error(self.error)end
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
            "/Script/UMG.Widget:SetRenderAngle","/Script/UMG.Widget:SetRenderTranslation","/Script/UMG.Widget:SetRenderTransformPivot",
            "/Script/UMG.Widget:SetVisibility","/Script/UMG.Image:SetBrush","/Script/UMG.Image:SetColorAndOpacity",
            "/Script/UMG.Image:SetGlowColor","/Script/UMG.Image:SetUseGlobalGlowColor",
            "/Script/UMG.TextBlock:SetGlowColor","/Script/UMG.TextBlock:SetUseGlobalGlowColor"})do
            assert(valid(StaticFindObject(api)),"Attitude HUD API unavailable: "..api)
        end
        panelClass=StaticFindObject("/Script/UMG.PanelWidget");canvasClass=StaticFindObject("/Script/UMG.CanvasPanel")
        imageClass=StaticFindObject("/Script/UMG.Image")
        angleFunction=StaticFindObject("/Script/UMG.Widget:SetRenderAngle")
        moveFunction=StaticFindObject("/Script/UMG.Widget:SetRenderTranslation")
        visibilityFunction=StaticFindObject("/Script/UMG.Widget:SetVisibility")
        assert(valid(panelClass)and valid(canvasClass)and valid(imageClass),"Attitude HUD classes unavailable")
    end
    local function walk(widget,result)
        if not valid(widget)then return end
        result[widget:GetFName():ToString()]=widget
        if widget:IsA(panelClass)then widget.Slots:ForEach(function(_,slot)walk(slot:get().Content,result)end)end
    end
    local function construct(class,tree,name)
        self.serial=self.serial+1
        local object=StaticConstructObject(class,tree,FName("DualSPAttitude"..name.."_"..self.serial),TRANSIENT,0,false,false)
        assert(valid(object),"Attitude: cannot construct "..name);return object
    end
    local function place(slot,x,y,w,h)
        slot:SetPosition({X=x,Y=y});slot:SetSize({X=w,Y=h});slot:SetAutoSize(false)
    end
    local function attach(owner,widgets)
        local source=widgets.SpeedText
        local panel=widgets.SpeedMeterPanel
        assert(source.Slot.Parent:GetAddress()==panel:GetAddress()and panel:IsA(canvasClass),"Attitude: speed label layout changed")
        local c={owner=owner,source=source,panel=panel,lines={},texts={}}
        self.cache=c -- 先登记所有权，部分构造错误也可以隐藏/归还。
        c.root=construct(canvasClass,owner.WidgetTree,"Root");c.root:SetVisibility(COLLAPSED)
        c.slot=panel:AddChildToCanvas(c.root);assert(valid(c.slot),"Attitude: cannot attach root")
        local size=source.Font.Size/20
        c.scale=size
        local function canvas(parent,name,x,y,w,h)
            local object=construct(canvasClass,owner.WidgetTree,name)
            local slot=parent:AddChildToCanvas(object);assert(valid(slot),"Attitude: cannot attach canvas")
            place(slot,x*size,y*size,w*size,h*size);object:SetVisibility(HIT_TEST_INVISIBLE)
            return object
        end
        local function line(parent,x,y,w,h,angle,alpha)
            local object=construct(imageClass,owner.WidgetTree,"Line")
            local brush=object.Brush;brush.DrawAs=1;object:SetBrush(brush);object.bIsImageHidingEnable=false
            object:SetGlowColor(source.GlowColor);object:SetUseGlobalGlowColor(source.bUseGlobalGlowColor)
            local slot=parent:AddChildToCanvas(object);assert(valid(slot),"Attitude: cannot attach line")
            place(slot,x*size,y*size,w*size,h*size)
            object:SetRenderTransformPivot({X=.5,Y=.5});object:SetRenderAngle(angle or 0)
            object:SetVisibility(HIT_TEST_INVISIBLE)
            c.lines[#c.lines+1]={widget=object,alpha=alpha or 1}
        end
        local function text(y,value)
            local object=construct(source:GetClass(),owner.WidgetTree,"Text")
            object.Justification=1;object.Margin={Left=0,Top=0,Right=0,Bottom=0};object.bIsTextHidingEnable=false
            object.ShadowOffset=vec(source.ShadowOffset);object:SetFont(source.Font)
            local font=object.Font;font.Size=math.max(1,math.floor(source.Font.Size*.7+.5));object:SetFont(font)
            object:SetGlowColor(source.GlowColor);object:SetUseGlobalGlowColor(source.bUseGlobalGlowColor)
            object:SetText(FText(value));object:SetVisibility(HIT_TEST_INVISIBLE)
            local slot=c.root:AddChildToCanvas(object);assert(valid(slot),"Attitude: cannot attach text")
            place(slot,0,y*size,110*size,18*size);c.texts[#c.texts+1]=object;return object
        end
        c.pitch=text(0,"P ---");c.bank=text(70,"B ---")
        -- 圆环与水平基准固定；非对称尾翼让180°倒飞与平飞明显不同。
        local segments=32
        for index=0,segments-1 do
            local a,b=index*2*math.pi/segments,(index+1)*2*math.pi/segments
            local x1,y1=60+24*math.cos(a),44+24*math.sin(a)
            local x2,y2=60+24*math.cos(b),44+24*math.sin(b)
            local length=math.sqrt((x2-x1)^2+(y2-y1)^2)
            line(c.root,(x1+x2)/2-length/2,(y1+y2)/2-.4,length,.8,(a+b)/2*180/math.pi+90,.7)
        end
        line(c.root,34,43.6,52,.8,0,.3)
        for _,point in ipairs({{60,20},{84,44},{60,68},{36,44}})do
            if point[1]==60 then line(c.root,point[1]-.5,point[2]-2.5,1,5,0,.8)
            else line(c.root,point[1]-2.5,point[2]-.5,5,1,0,.8)end
        end
        line(c.root,21.5,20,1,48,0,.7)
        for _,y in ipairs({20,44,68})do line(c.root,18,y-.5,8,1,0,.8)end
        c.cursor=canvas(c.root,"Cursor",23,41,7,6)
        line(c.cursor,0,1,6,1,30,1);line(c.cursor,0,4,6,1,-30,1)
        c.wings=canvas(c.root,"Wings",36,20,48,48)
        c.wings:SetRenderTransformPivot({X=.5,Y=.5})
        line(c.wings,10,23.25,28,1.5,0,1)
        line(c.wings,23.25,12,1.5,12,0,1)
        line(c.wings,22.5,22.5,3,3,0,1)
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
    local function locate(plane,now)
        if now-self.lastScan<.05 then return nil end
        self.lastScan=now
        if not panelClass then preflight()end
        local world=plane:GetWorld():GetAddress()
        for _,owner in ipairs(FindAllOf("AceMainFlightHUD_C")or{})do
            if valid(owner)and valid(owner.WidgetTree)and valid(owner:GetWorld())and owner:GetWorld():GetAddress()==world then
                local widgets={};walk(owner.WidgetTree.RootWidget,widgets)
                if valid(widgets.SpeedText)and valid(widgets.SpeedMeterPanel)then return attach(owner,widgets)end
            end
        end
    end
    local function update(plane,enabled,now)
        if not enabled then self:release();self.waitingSince=nil;return end
        local address=plane:GetAddress()
        if self.player~=address then self:reset(true);self.player=address end
        if self.error then return end
        local c=self.cache
        if c and (not valid(c.owner)or not valid(c.root)or not valid(c.source)or not valid(c.wings)or not valid(c.cursor))then
            self:reset(true);self.player=address;c=nil
        end
        -- 字号变化时重新构造少量静态线段，正常帧只调用一次轻量控制接口。
        if c and c.scale~=c.source.Font.Size/20 then self:reset(true);self.player=address;c=nil end
        if not c then
            c=locate(plane,now)
            if not c then
                self.waitingSince=self.waitingSince or now
                assert(now-self.waitingSince<=5,"Attitude: cannot locate native speed HUD");return
            end
        end
        local color=c.source.ColorAndOpacity.SpecifiedColor
        local signature=table.concat({color.R,color.G,color.B,color.A},":")
        if c.style~=signature then
            for _,entry in ipairs(c.lines)do entry.widget:SetColorAndOpacity({R=color.R,G=color.G,B=color.B,A=color.A*entry.alpha})end
            for _,object in ipairs(c.texts)do object:SetColorAndOpacity(c.source.ColorAndOpacity);object:SetShadowColorAndOpacity(c.source.ShadowColorAndOpacity)end
            c.style=signature
        end
        self.released=false -- 跨接口失败也必须尝试停止原生观察责任。
        local ok,available,pitch,bank,reason=invoke(address,c.root:GetAddress(),c.wings:GetAddress(),c.cursor:GetAddress(),
            angleFunction:GetAddress(),moveFunction:GetAddress(),visibilityFunction:GetAddress(),24*c.scale)
        assert(ok,reason or "Attitude: control failed")
        self.available=available
        if pitch~=""then c.pitch:SetText(FText(pitch))end
        if bank~=""then c.bank:SetText(FText(bank))end
    end
    function self:update(plane,enabled,now)
        local ok,err=xpcall(function()update(plane,enabled,now)end,debug.traceback)
        if not ok then
            self.error=tostring(err);note("ATTITUDE_HUD_ERROR "..self.error)
            self:release()
        end
    end
    return self
end
return Attitude
