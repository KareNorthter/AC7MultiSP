-- Mock interfaces and native control shapes reduced from the owned Steam
-- assets. This models UMG ownership/layout and input, not actual rendering.
local serial=0
function FName(s)return {ToString=function()return s end}end
function FText(s)return FName(s)end
local function object(label,class)
    serial=serial+1
    local w={id=serial,label=label,class=class,alive=true,RenderOpacity=.9,visibility=0,Font={Size=22},Brush={ResourceObject=label},
        ColorAndOpacity={SpecifiedColor={R=.4,G=.65,B=1,A=1}},Slots={items={}},textCalls=0}
    function w:IsValid()return self.alive end
    function w:GetAddress()return self.id end
    function w:GetFName()return FName(self.label)end
    function w:GetFullName()return self.class..' /Engine/Transient.Mock.'..self.label end
    function w:GetClass()return self.class end
    function w:GetWorld()return world end
    function w:IsA(cls)
        return (cls.path=='/Script/UMG.PanelWidget'and(self.class=='CanvasPanel'or self.class=='HorizontalBox'))
            or(cls.path=='/Script/UMG.UserWidget'and self.class=='UserWidget')
    end
    function w:SetRenderOpacity(v)self.RenderOpacity=v end
    function w:SetVisibility(v)self.visibility=v end
    function w:SetText(v)self.text=v:ToString();self.textCalls=self.textCalls+1 end
    function w:SetColorAndOpacity(v)self.ColorAndOpacity=v end
    function w:SetBrush(v)self.Brush=v end
    function w:SetBrushTintColor(v)self.brushTint=v end
    function w.Slots:ForEach(fn)for i,s in ipairs(self.items)do fn(i,{get=function()return s end})end end
    function w:AddChild(child)
        assert(child.Slot==nil,'A cloned slot must not detach its source')
        local s={Parent=self,Content=child,pos={X=0,Y=0},size={X=312,Y=37},anchors={Minimum={X=.5,Y=.5},Maximum={X=.5,Y=.5}},alignment={X=0,Y=0},z=0,
            Padding={Left=0,Top=0,Right=0,Bottom=0},Size={Value=1,SizeRule=0},HorizontalAlignment=0,VerticalAlignment=0}
        function s:IsValid()return true end
        function s:IsA(cls)return cls.path==(self.Parent.class=='HorizontalBox'and'/Script/UMG.HorizontalBoxSlot'or'/Script/UMG.CanvasPanelSlot')end
        function s:GetPosition()return self.pos end
        function s:GetSize()return self.size end
        function s:GetAnchors()return self.anchors end
        function s:GetAlignment()return self.alignment end
        function s:GetZOrder()return self.z end
        function s:SetPosition(v)self.pos={X=v.X,Y=v.Y}end
        function s:SetSize(v)if v.X then self.size={X=v.X,Y=v.Y}else self.Size=v end end
        function s:SetAnchors(v)self.anchors=v end
        function s:SetAlignment(v)self.alignment=v end
        function s:SetZOrder(v)self.z=v end
        function s:SetPadding(v)self.Padding=v end
        function s:SetHorizontalAlignment(v)self.HorizontalAlignment=v end
        function s:SetVerticalAlignment(v)self.VerticalAlignment=v end
        self.Slots.items[#self.Slots.items+1]=s;child.Slot=s;return s
    end
    function w:RemoveChild(child)
        for i,s in ipairs(self.Slots.items)do if s.Content==child then table.remove(self.Slots.items,i);child.Slot=nil;return true end end
        return false
    end
    return w
end
world=object('world','World')
instance=object('instance','Instance');instance.selected=0
function instance:GetSelectedPlaneSWP()return self.selected end
controller=object('controller','Controller');pressed={};axis=0;inputCalls=0
function controller:IsInputKeyDown(k)inputCalls=inputCalls+1;return pressed[k.KeyName:ToString()]==true end
function controller:GetInputAnalogKeyState()inputCalls=inputCalls+1;return axis end
local function user(label)
    local u=object(label,'UserWidget');u.WidgetTree={RootWidget=object(label..'_root','CanvasPanel'),IsValid=function()return true end}
    return u
end
local function child(parent,label,class,x,y,width,height)
    local w=object(label,class);local s=parent:AddChild(w)
    s.pos={X=x or 0,Y=y or 0};s.size={X=width or 312,Y=height or 37};return w
end
owner=user('CampaignHangarWeapon');owner.enabled=true
function owner:IsEnableControl()return self.enabled end
function owner:SetIsEnableControl(v)self.enabled=v end
base=user('LightMenuBaseWidget');owner.LightMenuBaseWidget=base;owner.WidgetTree.RootWidget:AddChild(base)
originals={}
for _,label in ipairs({'TitleText','DescriptionText','ItemText0','ItemText1','ItemText2'})do
    originals[label]=child(base.WidgetTree.RootWidget,label,'TextBlock',-700.5,-328+40*(tonumber(label:match('%d'))or 0))
    originals[label].text=label;originals[label].Font.Size=label=='TitleText'and 31 or label=='DescriptionText'and 20 or 22
end
info=user('WeaponInformation');owner.WeaponInformation=info;owner.WidgetTree.RootWidget:AddChild(info)
info.HangarAircraftWeaponNameLong=user('HangarAircraftWeaponNameLong')
local nameBox=child(info.HangarAircraftWeaponNameLong.WidgetTree.RootWidget,'NameBox','HorizontalBox')
originals.name=child(nameBox,'Name','TextBlock')
info.WidgetTree.RootWidget:AddChild(info.HangarAircraftWeaponNameLong)
info.HangarSpecGroupR=user('HangarSpecGroupR');info.WidgetTree.RootWidget:AddChild(info.HangarSpecGroupR)
for i=1,6 do
    local item=user('SpecItem'..i);item.SpecBar=user('SpecBar')
    item.WidgetTree.RootWidget:AddChild(item.SpecBar);info.HangarSpecGroupR.WidgetTree.RootWidget:AddChild(item)
    for _,name in ipairs({'BarA','BarB','BarC','BaseImage3','Transition'})do
        item.SpecBar[name]=child(item.SpecBar.WidgetTree.RootWidget,name,'Image',name=='BaseImage3'and 160 or 0,0,160,17)
    end
end
info.HangarAircraftAllWeaponsAmmo=user('Ammo');info.WidgetTree.RootWidget:AddChild(info.HangarAircraftAllWeaponsAmmo)
for i=1,3 do
    local cell=user('WeaponsSP'..i);info.HangarAircraftAllWeaponsAmmo['WeaponsSP'..i]=cell
    info.HangarAircraftAllWeaponsAmmo.WidgetTree.RootWidget:AddChild(cell)
    child(cell.WidgetTree.RootWidget,'WeaponsName','TextBlock');child(cell.WidgetTree.RootWidget,'WeaponsAmmo','TextBlock')
end
for _,name in ipairs({'HangarAircraftWeaponProperties','HangarAircraftWeaponType'})do
    info[name]=user(name);info.WidgetTree.RootWidget:AddChild(info[name])
end
owner.LightMenuItemIconCategoryWidget=user('Category')
for i=0,2 do child(owner.LightMenuItemIconCategoryWidget.WidgetTree.RootWidget,'ItemIconL'..i,'Image',-758.5,-328+i*40,20,20)end
owner.WidgetTree.RootWidget:AddChild(owner.LightMenuItemIconCategoryWidget)
owner.LightMenuItemIconStatusWidget=user('Status');owner.WidgetTree.RootWidget:AddChild(owner.LightMenuItemIconStatusWidget)
marker=user('Marker')
settings=object('InputSettings','Settings');settings.ActionMappings={}
mappings={{ActionName=FName('INP_MenuAccept'),Key={KeyName=FName('Enter')}},
    {ActionName=FName('INP_MenuAccept'),Key={KeyName=FName('Gamepad_FaceButton_Bottom')}},
    {ActionName=FName('INP_MenuCancel'),Key={KeyName=FName('Escape')}},
    {ActionName=FName('INP_MenuCancel'),Key={KeyName=FName('Gamepad_FaceButton_Right')}}}
function settings.ActionMappings:ForEach(fn)for i,m in ipairs(mappings)do fn(i,{get=function()return m end})end end
function StaticFindObject(path)
    if path=='/Script/Engine.Default__InputSettings'then return settings end
    return {path=path,IsValid=function()return true end}
end
function StaticConstructObject(class,tree,name,flags,internal,copy,archetype,source)
    if type(class)=='table'and class.path=='/Script/UMG.VerticalBox'then
        assert(not source);return object(name:ToString(),'VerticalBox')
    end
    assert(flags==0x40 and source)
    local w=object(name:ToString(),class);w.Font={Size=source.Font.Size};w.Brush=source.Brush
    w.Slot=source.Slot;return w
end
function FindFirstOf(name)assert(name=='NimbusPlayerController');return controller end
scans=0
function FindAllOf(name)
    scans=scans+1
    if name=='CampaignHangarWeaponWidget'then return {owner}end
    if name=='LightMenuItemMarkerWidget'then return {marker}end
    error('Unexpected registry query: '..name)
end
function IsInGameThread()return true end
options={{code='qaam',description='高机动导弹',max=26,stats={80,0,90,60,40,0},owned=true,available=true},
    {code='xsdb',description='小直径炸弹',max=48,stats={0,80,0,60,30,50},owned=true,available=true},
    {code='4aam',description='四目标导弹',max=40,stats={75,0,60,40,65,0},owned=true,available=true}}
profile={a=0,b=1,ratio=.5};notes={};commits={}
adapter=dofile(adapterPath).new(function(v)notes[#notes+1]=v end,function(id,p)commits[#commits+1]={id=id,profile=p}end)
clock=0
os.clock=function()return clock end
function step(dt)
    clock=clock+(dt or .02)
    adapter:pump(instance,110,function()return profile end,function()return options end,clock,false,false)
end
function press(k)
    pressed[k]=true;step();pressed[k]=false;step()
end
function remote(value)return {value=value,get=function(self)return self.value end,set=function(self,v)self.value=v end}end
