"""配置持久化、原生命令边界和页面具名接口回归；文件仅写 Temp。"""
from pathlib import Path
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools/test-python'))
from lupa.lua54 import LuaRuntime

workspace = Path(tempfile.mkdtemp(prefix='runtime-services-', dir=ROOT / 'Temp'))
lua = LuaRuntime(unpack_returned_tuples=True)
lua.globals().script_dir = (ROOT / 'dist/DualSP/Scripts').as_posix()
lua.globals().fixture = workspace.as_posix() + '/'
lua.execute(r'''
local json=dofile(script_dir..'/json.lua')
local Profile=dofile(script_dir..'/loadout_profile.lua')
local Config=dofile(script_dir..'/config_store.lua')
local Bridge=dofile(script_dir..'/native_bridge.lua')
local Page=dofile(script_dir..'/native_loadout.lua')
local function readText(path)
    local f=io.open(path,'r');if not f then return nil end
    local text=f:read('*a');f:close();return text
end
local function readJson(path)local text=readText(path);return text and json.decode(text)or nil end
local function writeJson(path,value)
    Config.writeJson(path,value,json)
end
local function rejects(fn,expected)
    local ok,err=pcall(fn);assert(not ok,'应拒绝的操作被接受')
    if expected then assert(tostring(err):find(expected,1,true),tostring(err))end
end
local writeFail=false
local function store(prefix)
    return Config.new{root=fixture..prefix,note=function()end,json=json,profile=Profile,readJson=readJson,
        writeJson=function(path,value)if writeFail then error('模拟写盘失败')end;writeJson(path,value)end}
end
local currentStore=store('current-')
local config=currentStore:load()
assert(config.schema==2 and config.management_key==117 and #config.keyboard_keys==4)
assert(config.management_revision==14 and config.mod_enabled and config.fixed_mount_enabled)
assert(config.ammo_percent_two==50 and config.ammo_percent_three==33 and config.central_hint==nil)
assert(not config.target_gaze_pad_l3 and not config.camera_hold_enabled and config.camera_r3_enabled and config.central_weapon_display=="always")
assert(config.camera_auto_center_degrees==30)
config.profiles['10']={a=2,b=1,c=0,ratio=.82};currentStore:save(config)
local original=readText(fixture..'current-profiles.json')
local action={request=1,management_protocol=15,language='en',mod_enabled=false,fixed_mount_enabled=false,gun_salvo_enabled=false,
    central_weapon_display="always",target_gaze_pad_l3=false,target_gaze_key=0,camera_hold_enabled=false,camera_auto_center_degrees=30,camera_r3_enabled=false,camera_center_key=4,camera_sensitivity=100,
    ammo_percent_two=75,ammo_percent_three=66,keyboard_keys={49,50,51,52},shortcuts=json.decode(json.encode(config.shortcuts))}
local nextConfig=currentStore:applySettings(config,action)
assert(config.language=='auto' and nextConfig.language=='en' and nextConfig.profiles['10'].a==2)
assert(not nextConfig.mod_enabled and not nextConfig.fixed_mount_enabled and nextConfig.central_hint==nil)
assert(nextConfig.ammo_percent_two==75 and nextConfig.ammo_percent_three==66 and nextConfig.profiles['10'].ratio==.82)
assert(readText(fixture..'current-profiles.previous.json')==original)
local saved=readText(fixture..'current-profiles.json')
-- 无效协议、废弃字段和百分比不能部分写盘，也不能改写当前配置。
for _,bad in ipairs({{'management_protocol',3},{'central_hint',false},{'mod_enabled',1},
    {'camera_auto_center_degrees',-1},{'camera_auto_center_degrees',46},{'camera_auto_center_degrees',30.5},{'fixed_mount_enabled',1},{'ammo_percent_two',49},{'ammo_percent_two',101},
    {'ammo_percent_three',32},{'ammo_percent_three',101},{'ammo_percent_three',33.5}})do
    local previous=action[bad[1]];action[bad[1]]=bad[2]
    rejects(function()currentStore:applySettings(nextConfig,action)end)
    assert(readText(fixture..'current-profiles.json')==saved and nextConfig.profiles['10'].ratio==.82)
    action[bad[1]]=previous
end
for _,values in ipairs({{50,33},{100,100}})do
    action.ammo_percent_two=values[1];action.ammo_percent_three=values[2]
    nextConfig=currentStore:applySettings(nextConfig,action)
    assert(nextConfig.ammo_percent_two==values[1] and nextConfig.ammo_percent_three==values[2])
    assert(nextConfig.profiles['10'].ratio==.82)
end
action.ammo_percent_two=75;action.ammo_percent_three=66
saved=readText(fixture..'current-profiles.json')
action.keyboard_keys={49,49,51,52}
rejects(function()currentStore:applySettings(nextConfig,action)end,'不能重复')
assert(readText(fixture..'current-profiles.json')==saved)
action.keyboard_keys={49,50,51,52};action.language='zh';writeFail=true
rejects(function()currentStore:applySettings(nextConfig,action)end,'模拟写盘失败')
assert(nextConfig.language=='en' and readText(fixture..'current-profiles.json')==saved)
writeFail=false
-- 编码失败不能截断已有文件；底层 write/close 返回失败也必须上报。
local cyclic={};cyclic.self=cyclic
rejects(function()Config.writeJson(fixture..'current-profiles.json',cyclic,json)end)
assert(readText(fixture..'current-profiles.json')==saved)
local realOpen=io.open
for _,failure in ipairs({'write','close'})do
    local closed=false
    io.open=function(path,mode)
        if mode~='w' then return realOpen(path,mode)end
        return {write=function(self)
            if failure=='write' then return nil,'模拟底层写入失败'end
            return self
        end,close=function()
            closed=true
            if failure=='close' then return nil,'模拟底层关闭失败'end
            return true
        end}
    end
    rejects(function()Config.writeJson(fixture..'current-profiles.json',{},json)end,'模拟底层')
    rejects(function()currentStore:applySettings(nextConfig,action)end,'模拟底层')
    assert(closed and nextConfig.language=='en')
    local executed=false
    local failedClient=Bridge.new{root=fixture,profile=Profile,readJson=readJson,execute=function()executed=true end}
    rejects(function()failedClient:request{op='setup',plane=16,activator=32,target=0,time=0,profile={a=0,b=-1,c=-1}}end,'模拟底层')
    assert(not executed,'写入失败后不能调用核心')
    io.open=realOpen
end
assert(readText(fixture..'current-profiles.json')==saved)
-- 四个自定义快捷键：保留旧管理键、组合键与单键区分、重复和武器选择冲突拒绝。
local oldShortcuts=nextConfig.shortcuts
local candidate=json.decode(json.encode(action))
candidate.shortcuts={management={key=120,mods=5},cycle={key=121,mods=0},mount={key=122,mods=7},salvo={key=116,mods=3}}
local custom=currentStore:applySettings(nextConfig,candidate)
assert(custom.management_key==120 and custom.shortcuts.management.mods==5)
assert(custom.shortcuts.cycle.key==121 and custom.shortcuts.mount.mods==7)
for _,bad in ipairs({
    {management={key=117,mods=0},cycle={key=117,mods=0},mount={key=118,mods=3}},
    {management={key=49,mods=0},cycle={key=117,mods=3},mount={key=118,mods=3}},
    {management={key=27,mods=1},cycle={key=117,mods=3},mount={key=118,mods=3}},
    {management={key=117,mods=8},cycle={key=117,mods=3},mount={key=118,mods=3}},
    {management={key=115,mods=2},cycle={key=117,mods=3},mount={key=118,mods=3}}
})do
    bad.salvo=json.decode(json.encode(config.shortcuts.salvo));candidate.shortcuts=bad;local bytes=readText(fixture..'current-profiles.json')
    rejects(function()currentStore:applySettings(custom,candidate)end)
    assert(readText(fixture..'current-profiles.json')==bytes)
end
local legacy5=json.decode(json.encode(config));legacy5.camera_auto_center_degrees=nil;legacy5.management_revision=5;legacy5.shortcuts=nil;legacy5.gun_salvo_enabled=nil;legacy5.management_key=119
legacy5.target_gaze_pad_x=nil;legacy5.target_gaze_pad_l3=nil;legacy5.central_weapon_display=nil;legacy5.target_gaze_key=nil;legacy5.camera_hold_enabled=nil;legacy5.camera_r3_enabled=nil;legacy5.camera_center_key=nil;legacy5.camera_sensitivity=nil
writeJson(fixture..'shortcuts-profiles.json',legacy5)
local legacyBytes=readText(fixture..'shortcuts-profiles.json')
local shortcutConfig=store('shortcuts-'):load()
assert(shortcutConfig.management_revision==14 and shortcutConfig.shortcuts.management.key==119)
assert(shortcutConfig.shortcuts.cycle.mods==3 and shortcutConfig.shortcuts.mount.key==118)
assert(readText(fixture..'shortcuts-profiles.before-custom-shortcuts.json')==legacyBytes)
assert(store('shortcuts-'):load().shortcuts.management.key==119)
assert(readText(fixture..'shortcuts-profiles.before-custom-shortcuts.json')==legacyBytes)
-- 整合后的旧配置10须连续迁移两项设置；两份永久备份均保留原始字节。
local legacy10=json.decode(json.encode(config));legacy10.camera_auto_center_degrees=nil;legacy10.management_revision=10;legacy10.gun_salvo_enabled=nil;legacy10.shortcuts.salvo=nil
legacy10.target_gaze_pad_l3=nil;legacy10.target_gaze_pad_x=true;legacy10.central_weapon_display=nil
legacy10.camera_hold_enabled=false;legacy10.camera_sensitivity=73
writeJson(fixture..'combined-profiles.json',legacy10)
local combinedBytes=readText(fixture..'combined-profiles.json')
local combined=store('combined-'):load()
assert(combined.management_revision==14 and not combined.target_gaze_pad_l3 and combined.camera_r3_enabled)
assert(not combined.camera_hold_enabled and combined.camera_sensitivity==73 and combined.central_weapon_display=='always')
assert(combined.target_gaze_pad_x==nil and combined.profiles['10'].ratio==.82)
assert(readText(fixture..'combined-profiles.before-controller-clicks.json')==combinedBytes)
assert(readText(fixture..'combined-profiles.before-central-weapon-display.json')==combinedBytes)
assert(store('combined-'):load().camera_sensitivity==73)
-- 真实旧配置12保留开关/方案；追加齐射默认关闭，备份原字节且重复加载不覆写。
local legacy12=json.decode(json.encode(config));legacy12.camera_auto_center_degrees=nil;legacy12.management_revision=12
legacy12.gun_salvo_enabled=nil;legacy12.shortcuts.salvo=nil
legacy12.target_gaze_pad_l3=true;legacy12.camera_hold_enabled=true
legacy12.shortcuts.mount={key=116,mods=3};legacy12.camera_center_key=116
writeJson(fixture..'salvo-profiles.json',legacy12)
local salvoBytes=readText(fixture..'salvo-profiles.json')
local salvoConfig=store('salvo-'):load()
assert(salvoConfig.management_revision==14 and not salvoConfig.gun_salvo_enabled)
assert(salvoConfig.target_gaze_pad_l3 and salvoConfig.camera_hold_enabled and salvoConfig.profiles['10'].ratio==.82)
assert(salvoConfig.shortcuts.salvo.key==0 and salvoConfig.shortcuts.mount.key==116)
assert(readText(fixture..'salvo-profiles.before-gun-salvo.json')==salvoBytes)
assert(store('salvo-'):load().shortcuts.salvo.key==0)
salvoConfig.shortcuts.salvo={key=116,mods=5}
assert(Config.centerShortcutMask(salvoConfig)==((1<<3)|(1<<5)))
-- 真实配置13升级：永久备份原字节、保留开关和方案；阈值端点可保存。
local legacy13=json.decode(json.encode(config));legacy13.management_revision=13;legacy13.camera_auto_center_degrees=nil
legacy13.camera_hold_enabled=true
writeJson(fixture..'camera-profiles.json',legacy13)
local cameraBytes=readText(fixture..'camera-profiles.json')
local cameraStore=store('camera-');local cameraConfig=cameraStore:load()
assert(cameraConfig.management_revision==14 and cameraConfig.camera_auto_center_degrees==30 and cameraConfig.camera_hold_enabled)
assert(cameraConfig.profiles['10'].ratio==.82 and readText(fixture..'camera-profiles.before-camera-auto-center.json')==cameraBytes)
for _,degrees in ipairs({0,45})do
    local update=json.decode(json.encode(action));update.camera_auto_center_degrees=degrees
    cameraConfig=cameraStore:applySettings(cameraConfig,update)
    assert(cameraStore:load().camera_auto_center_degrees==degrees)
end
local toggled=cameraStore:setCameraHold(cameraConfig,true)
assert(toggled.camera_hold_enabled and cameraStore:load().camera_hold_enabled)
assert(not cameraConfig.camera_hold_enabled and toggled.camera_auto_center_degrees==45)
writeFail=true
rejects(function()cameraStore:setCameraHold(toggled,false)end,'模拟写盘失败')
assert(toggled.camera_hold_enabled and cameraStore:load().camera_hold_enabled)
writeFail=false
writeJson(fixture..'blocked-camera-profiles.json',legacy13)
writeJson(fixture..'blocked-camera-profiles.before-camera-auto-center.json',{})
local blockedBytes=readText(fixture..'blocked-camera-profiles.json')
rejects(function()store('blocked-camera-'):load()end,'迁移备份已存在')
assert(readText(fixture..'blocked-camera-profiles.json')==blockedBytes)
local old={schema=1,profiles={['9']={a=0,b=1,ratio=.75}},keyboard_keys={49,50,51},management_key=119}
writeJson(fixture..'migration-profiles.json',old)
local oldBytes=readText(fixture..'migration-profiles.json')
local migrated=store('migration-'):load()
assert(migrated.schema==2 and migrated.profiles['9'].c==-1 and migrated.profiles['9'].ratio==.5)
assert(migrated.management_key==119 and migrated.keyboard_keys[4]==52)
assert(readText(fixture..'migration-profiles.before-three-sp.json')==oldBytes)
assert(readText(fixture..'migration-profiles.before-management-options.json')==oldBytes)
-- 管理第 3/4 版的旧三挂比例迁移为 33%，分别保留原始字节。
for _,revision in ipairs({3,4})do
    local prefix='revision'..revision..'-'
    local legacy={schema=2,management_revision=revision,profiles={['10']={a=2,b=0,c=1,ratio=1/3}},
        language='en',central_hint=false,keyboard_keys={49,50,51,52},management_key=117}
    writeJson(fixture..prefix..'profiles.json',legacy)
    local bytes=readText(fixture..prefix..'profiles.json')
    local upgraded=store(prefix):load()
    assert(upgraded.management_revision==14 and upgraded.mod_enabled and upgraded.fixed_mount_enabled)
    assert(upgraded.ammo_percent_two==50 and upgraded.ammo_percent_three==33 and upgraded.central_hint==nil)
    assert(upgraded.profiles['10'].ratio==.33 and upgraded.language=='en')
    assert(readText(fixture..prefix..'profiles.before-management-options.json')==bytes)
    assert(not readText(fixture..prefix..'profiles.before-three-sp.json'))
    assert(store(prefix):load().profiles['10'].ratio==.33) -- 再加载不重复迁移或覆盖永久备份。
    assert(readText(fixture..prefix..'profiles.before-management-options.json')==bytes)
end
old.keyboard_keys={49,50,52};writeJson(fixture..'conflict-profiles.json',old)
local conflict=readText(fixture..'conflict-profiles.json')
rejects(function()store('conflict-'):load()end,'旧键位已占用 4')
assert(readText(fixture..'conflict-profiles.json')==conflict)
assert(not readText(fixture..'conflict-profiles.before-three-sp.json'))
assert(not readText(fixture..'conflict-profiles.before-management-options.json'))
-- 旧响应、原生拒绝均不能被当作成功；失败后请求号继续递增。
local replyMode='ok';local requestCount=0
local client=Bridge.new{root=fixture,profile=Profile,readJson=readJson,execute=function()
    requestCount=requestCount+1
    local command=readText(fixture..'native-command.txt')
    assert(command=='switch 10 20 1 1.250000000 4 3 2 0 1 '..requestCount..' 0.67',command)
    writeJson(fixture..'native-status.json',{request=replyMode=='stale' and 0 or requestCount,
        ok=replyMode~='rejected',error='原生拒绝',active=1})
end}
local command={op='switch',plane=16,activator=32,target=1,time=1.25,profile={a=2,b=0,c=1,ratio=.67}}
assert(client:request(command).active==1)
replyMode='stale';rejects(function()client:request(command)end,'does not match')
replyMode='rejected';rejects(function()client:request(command)end,'原生拒绝')
replyMode='ok';assert(client:request(command).request==4)
-- 页面缺依赖立即拒绝；关闭/重置保留错误的语义不变。
local function deps(mode)
    return {mode=mode,note=function()end,translate=function(zh)return zh end,keyboardNavigation=function()end,
        commit=function()end,takeoff=function()end,preparationAction=function()end,
        confirmSound=function()end,weaponIcon=function()end,partsAmmoPreview=function()end,
        ammoSettings=function()return {ammo_percent_two=50,ammo_percent_three=33}end}
end
for _,mode in ipairs({'preparation','resupply','standby','parts'})do
    local page=Page.new(deps(mode))
    page.error='原有错误';page:reset{clearError=false,releaseInput=false};assert(page.error=='原有错误')
    page:reset{clearError=true,releaseInput=false};assert(not page.error and page.status=='waiting')
    page:pump{getProfile=function()return nil end,getOptions=function()return {}end,now=0,inFlight=true,panelOpen=true}
    assert(not page.error,page.error) -- 禁用上下文不访问引擎对象。
end
local missing=deps('resupply');missing.takeoff=nil
rejects(function()Page.new(missing)end,'缺少原生补给')
missing=deps('parts');missing.partsAmmoPreview=nil
rejects(function()Page.new(missing)end,'缺少只读弹药')
missing=deps('preparation');missing.mode='unknown'
rejects(function()Page.new(missing)end,'未知原生页面模式')
missing=deps('preparation');missing.ammoSettings=nil
rejects(function()Page.new(missing)end,'缺少弹药比例设置接口')
-- 只构造原生 YES 已交接的缓存，检查页面生命周期；不模拟起飞成功或引擎挂载。
local Flow=dofile(script_dir..'/loadout_flow.lua')
local options={{code='6aam',owned=true,available=true,max=30},
    {code='sffs',owned=true,available=true,max=47},{code='tls',owned=true,available=true,max=11}}
local settings={ammo_percent_two=50,ammo_percent_three=33}
local confirmed={a=2,b=0,c=1,ratio=.33}
local world={IsValid=function()return true end,GetAddress=function()return 900 end}
local instance={IsValid=function()return true end,GetWorld=function()return world end}
local owner={enabled=false,visible=false,IsValid=function()return true end,GetAddress=function()return 321 end,
    IsVisible=function(self)return self.visible end,IsEnableControl=function(self)return self.enabled end}
local releases,previews,committed={}, {}, {}
local previewFails,releaseFails=false,false
local function previewDeps(mode)
    local result=deps(mode)
    result.ammoSettings=function()return settings end
    result.commit=function(plane,profile)committed[#committed+1]={plane=plane,profile=profile}end
    result.mountPreview=function(context)
        assert(context.owner==owner and context.instance==instance and context.planeId==10 and context.mode==mode)
        assert(context.ownerAddress==321,'预览回调必须携带可供异常/禁用归还的 owner 地址')
        assert(context.options==options)
        previews[#previews+1]=Profile.copy(context.profile)
        if previewFails then error('模拟模型预览失败')end
    end
    result.releaseMountPreview=function(address)
        releases[#releases+1]=address
        assert(address==321,'页面只能释放自己取得的预览，不能全局释放其他页面')
        if releaseFails then error('模拟模型归还失败')end
    end
    return result
end
local function previewCache()
    local flow=Flow.new(confirmed,options,nil,'resupply',function(zh)return zh end,function()return settings end)
    flow.stage='review';flow.cursor=1
    return {owner=owner,instance=instance,planeId=10,world=900,flow=flow,mountOwner=321,
        handedOff=true,moved={},masked={},added={},texts={}}
end
local context={instance=instance,planeId=10,getProfile=function()return confirmed end,
    getOptions=function()return options end,now=1,inFlight=true,panelOpen=false}
local supplyPage=Page.new(previewDeps('resupply'))
local supplyCache=previewCache();supplyPage.cache=supplyCache
local transaction={planeId=10,profile=Profile.copy(confirmed),phase=901,finished=true}
supplyPage.transaction=transaction
-- 确认后的比例冻结；隐藏页面和之后修改管理设置均不能闪回旧任务方案。
settings.ammo_percent_three=66
supplyPage:pump(context)
assert(supplyPage.status=='awaiting_success'and supplyPage.cache==supplyCache and supplyPage.transaction==transaction)
assert(#previews==1 and previews[1].ratio==.33 and #releases==0 and #committed==0)
local hudPreview=supplyPage:previewSupply();hudPreview.a=0
assert(supplyPage.transaction.profile.a==2 and #committed==0)
-- 单独的模型错误只归还模型；文字/输入/补给确认事务继续等待真实成功事件。
previewFails=true;supplyPage:pump(context)
assert(not supplyPage.error and supplyPage.status=='awaiting_success'and supplyPage.transaction==transaction)
assert(supplyPage.cache==supplyCache and #releases==1 and #committed==0)
previewFails=false;supplyPage:pump(context)
assert(#previews==3 and supplyCache.mountOwner==321)
local releaseCount=#releases
local final=supplyPage:completeSupply()
assert(final.a==2 and final.b==0 and final.c==1 and final.ratio==.33)
assert(#committed==1 and committed[1].plane==10 and committed[1].profile.ratio==.33)
assert(not supplyPage.cache and not supplyPage.transaction and supplyPage.status=='committed')
assert(#releases==releaseCount,'成功交接后的模型应保留到 main 即将重建兵装时释放')
-- reset 即使归还接口失败，也必须清空页面缓存与事务，并保持 owner 限定。
supplyPage.cache=previewCache();supplyPage.transaction=transaction;releaseFails=true
supplyPage:reset{clearError=false,releaseInput=false}
assert(not supplyPage.cache and not supplyPage.transaction and supplyPage.status=='waiting')
assert(#releases==releaseCount+1 and releases[#releases]==321)
releaseFails=false;settings.ammo_percent_three=33
-- 只读页停用时归还本页，F6 暂借输入时继续保留，避免覆盖当前活动的选装页。
for _,mode in ipairs({'standby','parts'})do
    local readonly=Page.new(previewDeps(mode));local c=previewCache()
    c.handedOff=false;c.controlEnabled=false;owner.visible=true;readonly.cache=c
    context.inFlight=false;context.panelOpen=true
    local beforeRelease=#releases;local beforePreview=#previews
    readonly:pump(context)
    assert(#releases==beforeRelease and #previews==beforePreview and readonly.cache==c)
    context.panelOpen=false;readonly:pump(context)
    assert(not readonly.error and readonly.cache==c and #releases==beforeRelease+1 and #previews==beforePreview)
    readonly:reset{releaseInput=false}
end
-- 弹药行真实层级：根 Canvas -> 内 Canvas -> HorizontalBox -> Overlay -> 文字。
-- 只提取实际页面的内部构建/归还函数，不增加产品接口，也不模拟整场原生选择流程。
local function closure(fn,target,seen)
    seen=seen or {};if seen[fn]then return end;seen[fn]=true
    for i=1,100 do
        local name,value=debug.getupvalue(fn,i);if not name then break end
        if name==target then return value end
        if type(value)=='function' then local found=closure(value,target,seen);if found then return found end end
    end
end
local function copy(value)
    if type(value)~='table' then return value end
    local result={};for key,item in pairs(value)do result[key]=copy(item)end;return result
end
local classes={}
local function class(name)
    if not classes[name]then classes[name]={name=name,IsValid=function()return true end,
        GetFullName=function(self)return 'Class /Script/UMG.'..self.name end}end
    return classes[name]
end
StaticFindObject=function(path)return class(path:match('%.([^.:]+)$')or path)end
FName=function(value)return value end
local serial=0
local function widget(kind,name)
    serial=serial+1
    local result={kind=kind,name=name,address=serial,visibility=0,Slots={items={}},
        Font={Size=20,FontObject=class('Font')},Text=name,bAutoWrapText=false,WrapTextAt=0,
        Justification=0,LineHeightPercentage=1,GlowColor={},bUseGlobalGlowColor=false}
    function result:IsValid()return true end
    function result:GetAddress()return self.address end
    function result:GetClass()return class(self.kind)end
    function result:GetFullName()return self.kind..' '..self.name end
    function result:GetFName()return {ToString=function()return self.name end}end
    function result:IsA(expected)
        return expected.name==self.kind or expected.name=='PanelWidget'and
            (self.kind=='CanvasPanel'or self.kind=='HorizontalBox'or self.kind=='Overlay')
    end
    function result.Slots:ForEach(callback)
        for index,slot in ipairs(self.items)do callback(index,{get=function()return slot end})end
    end
    function result:AddChild(child)
        local slot={Parent=self,Content=child,position={X=0,Y=0},size={X=100,Y=32},
            anchors={Minimum={X=0,Y=0},Maximum={X=0,Y=0}},alignment={X=0,Y=0},z=0,bAutoSize=false,
            Padding={Left=0,Top=0,Right=0,Bottom=0},HorizontalAlignment=0,VerticalAlignment=0,
            Size={Value=1,SizeRule=0}}
        function slot:IsValid()return true end
        function slot:GetClass()return class(self.Parent.kind=='CanvasPanel'and 'CanvasPanelSlot'or self.Parent.kind..'Slot')end
        function slot:GetFullName()return self:GetClass():GetFullName()end
        for _,field in ipairs({'Position','Size','Anchors','Alignment','ZOrder','AutoSize'})do
            local stored=({Position='position',Size='size',Anchors='anchors',Alignment='alignment',ZOrder='z',AutoSize='bAutoSize'})[field]
            slot['Get'..field]=function(self)return copy(self[stored])end
            slot['Set'..field]=function(self,value)self[stored]=copy(value)end
        end
        child.Slot=slot;self.Slots.items[#self.Slots.items+1]=slot;return slot
    end
    function result:RemoveChild(child)
        for index,slot in ipairs(self.Slots.items)do
            if slot.Content==child then table.remove(self.Slots.items,index);child.Slot=nil;return true end
        end
        return false
    end
    function result:SetVisibility(value)self.visibility=value end
    function result:GetVisibility()return self.visibility end
    function result:SetFont(value)self.Font=copy(value)end
    function result:SetGlowColor(value)self.GlowColor=value end
    function result:SetUseGlobalGlowColor(value)self.bUseGlobalGlowColor=value end
    function result:SetText(value)self.Text=value end
    function result:SetColorAndOpacity(value)self.ColorAndOpacity=value end
    return result
end
StaticConstructObject=function(kind,tree,name)return widget(kind.name,name)end
local page=Page.new(deps('preparation'))
assert(closure(page.pump,'api'))()
local buildRow=assert(closure(page.pump,'preparationAmmoRow'))
local restoreVisual=assert(closure(page.reset,'restoreVisual'))
local outside=widget('CanvasPanel','AmmoTable')
local source=widget('UserWidget','WeaponsSP1')
local root=widget('CanvasPanel','CanvasPanel_0')
source.WidgetTree={RootWidget=root,IsValid=function()return true end}
local nativeInner=widget('CanvasPanel','CanvasPanel_2')
root:AddChild(nativeInner):SetSize({X=158.258255,Y=38})
local originals={}
for _,name in ipairs({'WeaponsName','WeaponsAmmo'})do
    local horizontal=widget('HorizontalBox',name..'Horizontal')
    local outerSlot=nativeInner:AddChild(horizontal)
    outerSlot:SetPosition({X=name=='WeaponsName'and 0 or 58,Y=0})
    local overlay=widget('Overlay',name..'Overlay');horizontal:AddChild(overlay)
    local text=widget('TextBlock',name);overlay:AddChild(text);originals[name]=text
end
local originalSlot=outside:AddChild(source)
originalSlot:SetPosition({X=217,Y=60});originalSlot:SetSize({X=164,Y=38})
local function context()
    return {owner={WidgetTree={RootWidget=outside}},added={},moved={},masked={},texts={},
        tint={SpecifiedColor={R=.4,G=.65,B=1,A=1},ColorUseRule=0}}
end
local c=context();local row=buildRow(c,source)
local inner=row.name.Slot.Parent;local panel=inner.Slot.Parent
assert(inner==row.count.Slot.Parent and inner~=nativeInner,'名称/数量必须共用独立的内层 Canvas')
assert(inner.Slot:GetSize().X==158.258255 and inner.Slot:GetSize().Y==38)
assert(panel.Slot.Parent==outside and panel.Slot:GetPosition().X==217 and panel.Slot:GetPosition().Y==60)
assert(row.name.Slot:GetPosition().X==0 and row.count.Slot:GetPosition().X==58)
assert(row.name.Slot:GetSize().X==100 and row.count.Slot:GetSize().Y==32 and not row.name.Slot.bAutoSize)
assert(row.name.Font.Size==20 and row.count.Font.Size==20,'弹药行保留原生字号')
assert(source.Slot.Parent~=outside and source.Slot.Parent.visibility==1,'原行须隔离原生高亮')
assert(originals.WeaponsName.Slot.Parent.kind=='Overlay','不能移动文字模板或破坏包装层')
restoreVisual(c)
assert(source.Slot.Parent==outside and source.Slot:GetPosition().Y==60 and #outside.Slots.items==1)
-- 无关 Canvas 不能冒充行根；失败后同样归还已创建控件。
local savedParent=nativeInner.Slot.Parent;nativeInner.Slot.Parent=outside
c=context();rejects(function()buildRow(c,source)end,'不属于行根')
restoreVisual(c);nativeInner.Slot.Parent=savedParent
assert(source.Slot.Parent==outside and #outside.Slots.items==1)
''')
main_source = (ROOT / 'dist/DualSP/Scripts/main.lua').read_text(encoding='utf-8')
lua.globals().scene_preview_source = main_source[main_source.index('local scenePreviewRequested='):main_source.index('-- 补给开始先接管原生输入')]
lua.execute(r'''
-- 执行 main 的真实场景协调代码，复现页面退出、SKIN 无适配器以及补给页销毁。
local Profile=dofile(script_dir..'/loadout_profile.lua')
local function object(address)
    return {IsValid=function()return true end,GetAddress=function()return address end}
end
local world=object(900)
local function inWorld(address)local result=object(address);result.GetWorld=function()return world end;return result end
local model=inWorld(200);model.PlayerWeaponActivator=object(201)
local instance=inWorld(100);instance.HangarManager=object(101);instance.HangarManager.LoadedPlayerPlane=model
instance.GetSelectedPlaneID=function()return 128 end
local page=inWorld(321)
local options={{max=30},{max=47},{max=11}}
local profile={a=0,b=1,c=-1,ratio=.5}
local calls,lease,fail={},nil,false
local env=setmetatable({runtimeAllowed=true,runtimeModEnabled=true,runtimeFixedMountEnabled=true,
    expandedMountAssets={},runtimeContext={hangarVisible=true},
    instance=instance,planeId=128,plane=model,activator=model.PlayerWeaponActivator,catalogue={['128']={code='su30sm'}},Profile=Profile,
    valid=function(o)return o and o:IsValid()end,isSelectedPlane=function()return true end,
    profileFor=function()return profile end,optionsFor=function()return options end,
    previewWarning=function()end,recordStage=function()end,nativeMountPreview=function(owner,plane,activator,mode,a,b,c,aa,ab,ac)
        if fail then error('模拟模型失败')end
        calls[#calls+1]={owner=owner,plane=plane,mode=mode,a=a,aa=aa,ab=ab}
        if mode==0 then if owner==0 or owner==lease then lease=nil end
        elseif mode~=4 then lease=owner end
        return true,''
    end}, {__index=_G})
local coordinator=assert(load(scene_preview_source..[[
return {preview=mountPreview,release=releaseMountPreview,scene=maintainScenePreview,supply=maintainSupplyPreview,
beginTick=function()scenePreviewRequested=false end}
]],'场景预览回归','t',env))()
coordinator.preview{owner=page,ownerAddress=321,instance=instance,planeId=128,mode='preparation',profile=profile,options=options}
assert(lease==100 and calls[#calls].aa==15 and calls[#calls].ab==24)
coordinator.release(321);assert(lease==100,'退出页面不能释放机库展示机')
local before=#calls
coordinator.scene();assert(#calls==before,'本轮活动页候选必须优先')
coordinator.beginTick();coordinator.scene()
assert(lease==100 and calls[#calls].mode==1,'SKIN等无页面适配器时仍显示已存方案')
-- 换涂装生成新展示机时使用当前LoadedPlayerPlane，不能沿用旧地址。
local nextModel=inWorld(202);nextModel.PlayerWeaponActivator=object(203)
instance.HangarManager.LoadedPlayerPlane=nextModel
coordinator.beginTick();coordinator.scene();assert(calls[#calls].plane==202)
coordinator.preview{owner=page,instance=instance,planeId=128,mode='resupply',profile=profile,options=options}
page.IsValid=function()return false end
coordinator.supply();assert(calls[#calls].mode==4 and calls[#calls].owner==0 and calls[#calls].plane==200,
    '起飞过渡只能刷新已校验的任务实例，不再解引用补给页')
coordinator.release(0);before=#calls;coordinator.supply();assert(#calls==before,'重建后不能继续刷新旧预览')
coordinator.preview{instance=instance,planeId=128,mode='flight',profile=profile,options=options}
assert(calls[#calls].mode==3 and calls[#calls].plane==200,'开场只提供视觉方案')
fail=true;coordinator.beginTick();coordinator.scene();assert(not env.errorText,'视觉异常不能变为兵装错误')
fail=false;env.runtimeFixedMountEnabled=false
coordinator.preview{instance=instance,planeId=128,mode='flight',profile=profile,options=options}
assert(not lease and calls[#calls].mode==0,'禁用必须归还整个场景')
env.runtimeFixedMountEnabled=true
coordinator.preview{instance=instance,planeId=128,mode='scene',profile=profile,options=options}
env.runtimeContext.hangarVisible=false
coordinator.beginTick();coordinator.scene();assert(not lease,'评分页即使保留机库飞机也必须归还预览')
before=#calls;coordinator.beginTick();coordinator.scene();assert(#calls==before,'评分页不能持续调用原生预览')
env.runtimeContext.hangarVisible=true
coordinator.beginTick();coordinator.scene();assert(lease==100,'返回机库必须恢复预览')
''')
lua.globals().locate_plane_source = main_source[main_source.index('local function locatePlane()'):main_source.index('local function publishState()')]
lua.execute(r'''
-- 玩家已生成时不再等待250ms全局扫描；未持有玩家时仍限制扫描频率。
local scans=0
local world={IsValid=function()return true end,GetAddress=function()return 900 end,
    GetFullName=function()return 'World /Game/Maps/Mission/ms06.ms06' end}
local player={IsValid=function()return true end,IsA=function()return true end,GetWorld=function()return world end}
local env=setmetatable({instance={GetWorld=function()return world end},lastPlaneScan=1,
    valid=function(o)return o and o:IsValid()end,game={GetPlayerPawn=function()return player end},
    StaticFindObject=function()return {}end,FindAllOf=function()scans=scans+1;return {}end,
    os={clock=function()return 1.01 end}}, {__index=_G})
local locate=assert(load(locate_plane_source..'return locatePlane','开场玩家定位','t',env))()
assert(locate()==player and scans==0,'直接玩家路径不受全局扫描节流限制')
env.game.GetPlayerPawn=function()return nil end
assert(locate()==nil and scans==0,'构造阶段不能每帧全局扫描')
env.os.clock=function()return 1.5 end
assert(locate()==nil and scans==1 and env.lastPlaneScan==1.5)
world.GetFullName=function()return 'World /Game/Maps/Menu/Hangar.Hangar' end
assert(locate()==nil and scans==1,'菜单不查找任务玩家')
''')
lua.globals().hotkey_source = main_source[main_source.index('local function applyDebugHotkey('):main_source.index('local function applyAction(')]
lua.execute(r'''
-- 执行真实调试快捷操作：拒绝过期/忙碌请求；模型开关不能调用补弹换装。
local json=dofile(script_dir..'/json.lua')
local Profile=dofile(script_dir..'/loadout_profile.lua')
local world={GetAddress=function()return 900 end}
local options={{owned=true,available=true,code='a'},{owned=true,available=true,code='b'},{owned=true,available=true,code='c'}}
local configured,toggled,saved,globalSetting=0,0,0,0
local env=setmetatable({hotkeyResponse=0,runtimeAllowed=true,runtimeModEnabled=true,isFlight=true,configured=true,debugSession=1,weaponRevision=5,
    config={fixed_mount_enabled=true,ammo_percent_two=50,ammo_percent_three=33},runtimeFixedMountEnabled=true,
    currentProfile={a=0,b=-1,c=-1},Profile=Profile,json=json,planeId=128,flightOptions=options,
    instance={GetWorld=function()return world end,GetSelectedPlaneID=function()return 128 end},
    plane={GetAddress=function()return 200 end,GetWorld=function()return world end},activator={GetAddress=function()return 201 end},
    game={GetTimeSeconds=function()return 123 end},canDebugLoadout=function(hotkey)assert(hotkey);return true end,
    debugLoadoutReady=function()return true end,optionsFor=function()return options end,note=function()end,
    tr=function(zh)return zh end,saveConfig=function()saved=saved+1 end,valid=function(o)return o~=nil end,
    setFixedMountEnabled=function()return true end,
    debugFixedMount=function(p,a,t,enabled)assert(p==200 and a==201 and t==123);toggled=toggled+1;return true,'' end}, {__index=_G})
env.applyFixedMountSetting=function()globalSetting=globalSetting+1;env.runtimeFixedMountEnabled=env.config.fixed_mount_enabled end
env.configureFlight=function(profile,reason)assert(reason=='debug_hotkey');configured=configured+1;env.currentProfile=profile end
local apply=assert(load(hotkey_source..'return applyDebugHotkey','调试快捷操作','t',env))()
local function action(request,command,revision)return {request=request,command=command,revision=revision or 5,session=1,hotkey_protocol=1}end
apply(action(1,'cycle'));assert(configured==1 and env.currentProfile.a==1 and toggled==0)
apply(action(1,'cycle'));assert(configured==1,'同一请求不能补弹两次')
apply(action(2,'cycle',4));assert(configured==1,'旧兵装请求必须拒绝')
env.canDebugLoadout=function()return false,'正在补给' end
apply(action(3,'cycle'));assert(toggled==0 and saved==0)
env.canDebugLoadout=function()return true end
apply(action(4,'mount'));assert(toggled==1 and saved==1 and not env.runtimeFixedMountEnabled and not env.config.fixed_mount_enabled)
assert(configured==1,'模型开关不得调用补弹或改组合')
env.saveConfig=function()error('模拟保存失败')end
apply(action(5,'mount'));assert(toggled==1 and not env.runtimeFixedMountEnabled and not env.config.fixed_mount_enabled)
env.debugLoadoutReady=function()return false,'有在途武器' end
apply(action(6,'cycle'));assert(configured==1 and env.hotkeyNotice:find('有在途武器',1,true))
env.saveConfig=function()saved=saved+1 end
env.canDebugLoadout=function()error('模型开关不得调用飞行/暂停/换装守卫')end
apply(action(7,'mount',0));assert(toggled==2 and env.runtimeFixedMountEnabled,'模型开关允许暂停、在途武器和兵装版本变化')
env.isFlight=false;env.plane=nil;env.activator=nil
env.lastMountPreviewContext={mode='scene'}
local previewed=0;env.mountPreview=function(c)assert(c==env.lastMountPreviewContext);previewed=previewed+1 end
apply(action(8,'mount'));assert(globalSetting==1 and not env.runtimeFixedMountEnabled)
apply(action(9,'mount'));assert(globalSetting==2 and previewed==1 and env.runtimeFixedMountEnabled,'机库立即重建最后候选')
env.isFlight=true;env.supplySession={menuClosed=true}
local supplied=0;env.maintainSupplyPreview=function()supplied=supplied+1 end
apply(action(10,'mount'));assert(globalSetting==3 and supplied==1,'补给过渡不依赖已销毁页面')
env.runtimeModEnabled=false
apply(action(11,'mount'));assert(globalSetting==4 and env.config.fixed_mount_enabled and env.hotkeyNotice:find('仅保存设置',1,true))
env.runtimeModEnabled=true;env.supplySession=nil;env.isFlight=false
env.applyFixedMountSetting=function()error('模拟外观绑定失败')end
apply(action(12,'mount'));assert(not env.runtimeFixedMountEnabled and env.mountWarning:find('回退',1,true))
assert(configured==1,'任何外观开关操作均不得补弹或改组合')
''')
print('配置/协议/页面、场景预览、开场定位、调试快捷操作及失败隔离检查通过')

# 执行实际安全菜单判定和总开关交接函数，覆盖管理面板借走输入、任务拒绝及清理失败。
context_source = (ROOT / 'dist/DualSP/Scripts/runtime_context.lua').read_text(encoding='utf-8')
lua.globals().mission_gate_source = context_source[context_source.index('function Context.allowed('):context_source.index('-- 管理面板暂借了原生输入')]
lua.execute(r'''
-- 执行真实许可判断：仅113任务/机库旁路，残留ID、213自由飞行及多人边界不能混淆。
local state={id=113,name='World /Game/Maps/Mission/ms13.ms13',shared=false,network=false,multi=false,kind='hangar'}
local function valid(o)return o and o:IsValid()end
local world={IsValid=function()return true end,GetAddress=function()return 900 end,GetFullName=function()return state.name end}
local mode={IsValid=function()return true end,IsA=function(_,class)return class=='ui'and state.shared or class=='multi'and(state.shared or state.multi)end}
local widget={IsValid=function()return true end,GetWorld=function()return world end,IsVisible=function()return true end,
    IsEnableControl=function()return true end,IsA=function(_,class)return class=='base'or class==state.kind end,
    GetClass=function()return {GetFullName=function()return state.kind=='hangar'and'/Game/UI/Menu/Hangar/Weapon'or'/Game/UI/Menu/MainMenu'end}end}
local manager={IsValid=function()return true end,GetWorld=function()return world end,
    MenuWidgetArray={ForEach=function(_,fn)fn(nil,{get=function()return widget end})end}}
local context={isMultiplayerWidget=function()return state.multiWidget end}
local env=setmetatable({Context=context,valid=valid,initialize=function()end,
    system={IsStandalone=function()return not state.network end},game={GetGameMode=function()return mode end},
    ui={GetCurrentUIManagerActor=function()return manager end},modeClasses={UIGameMode='ui',MultiPlayerGameMode='multi'},
    menuBaseClass='base',singleEntryClasses={'main'},lastHangarTime=-1},{__index=_G})
local allowed=assert(load(mission_gate_source..'return Context.allowed','第13关上下文旁路','t',env))()
local instance={IsValid=function()return true end,GetWorld=function()return world end,GetMissionID=function()return state.id end}
local ok,reason=allowed(instance);assert(ok and reason=='mission13_fixed_sp')
state.id=114;ok,reason=allowed(instance);assert(ok and reason=='singleplayer')
state.id=213;state.name='World /Game/Maps/FreeFlight/ms13.ms13';ok,reason=allowed(instance);assert(ok and reason=='singleplayer')
state.id=113;state.shared=true;state.name='World /Game/UI/Menu/Menu.Menu';ok,reason=allowed(instance);assert(ok and reason=='mission13_fixed_sp')
state.kind='main';ok,reason=allowed(instance);assert(ok and reason=='singleplayer_menu','主菜单不能因残留113编号停用')
state.kind='hangar';state.network=true;ok,reason=allowed(instance);assert(not ok and reason=='network_world')
state.network=false;state.shared=false;state.multi=true;state.name='World /Game/Maps/Mission/ms13.ms13'
ok,reason=allowed(instance);assert(not ok and reason=='multiplayer_mode')
state.shared=true;state.multi=false;state.multiWidget=true;state.name='World /Game/UI/Menu/Menu.Menu'
ok,reason=allowed(instance);assert(not ok and reason=='multiplayer_menu')
''')
print('第13关113旁路、213自由飞行/离关恢复、主菜单残留ID及多人优先边界检查通过')
lua.globals().master_menu_source = context_source[context_source.index('function Context.canToggleMaster('):context_source.index('-- 仅在许可变化时记录上下文')]
lua.globals().master_apply_source = main_source[main_source.index('tryApplyMasterSwitch=function(force)'):main_source.index('local function tick()')]
lua.execute(r"""
local world={IsValid=function()return true end,GetAddress=function()return 900 end,GetFullName=function()return 'World /Game/UI/Menu/Menu.Menu' end}
local otherWorld={IsValid=function()return true end,GetAddress=function()return 901 end}
local visible,controlled,permitted=true,false,true
local kind='main'
local widget={IsValid=function()return true end,GetWorld=function()return world end,
    IsVisible=function()return visible end,IsEnableControl=function()return controlled end,
    IsA=function(_,class)return class=='base' or class==kind end}
local manager={IsValid=function()return true end,GetWorld=function()return world end,
    MenuWidgetArray={ForEach=function(_,fn)fn(nil,{get=function()return widget end})end}}
local env=setmetatable({Context={allowed=function()return permitted end},toggleMenuClasses={'main','title','campaign','missions'},
    menuBaseClass='base',valid=function(o)return o and o:IsValid()end,
    ui={GetCurrentUIManagerActor=function()return manager end}}, {__index=_G})
local canToggle=assert(load(master_menu_source..'return Context.canToggleMaster','总开关安全菜单','t',env))()
local instance={GetWorld=function()return world end}
assert(not canToggle(instance,{}),'不能使用仍可见但未开放输入的底层主菜单')
assert(canToggle(instance,{widget}),'管理面板实际持有的主菜单可以安全切换')
controlled=true;assert(canToggle(instance,{}))
visible=false;assert(not canToggle(instance,{widget}));visible=true
kind='hangar';assert(not canToggle(instance,{widget}),'机库草稿期间不能切换');kind='main'
world.GetFullName=function()return 'World /Game/Maps/Mission/ms19.ms19' end
assert(not canToggle(instance,{widget}),'任务世界不能用缓存主菜单绕过限制')
world.GetFullName=function()return 'World /Game/UI/Menu/Menu.Menu' end
widget.GetWorld=function()return otherWorld end;assert(not canToggle(instance,{widget}))
widget.GetWorld=function()return world end;permitted=false;assert(not canToggle(instance,{widget}));permitted=true
local resets,gates,fail=0,{},false
local adapter={isPreparationHandoffPending=function()return false end,reset=function()resets=resets+1;if fail then error('模拟控件归还失败')end end}
local applyEnv=setmetatable({config={mod_enabled=true},runtimeModEnabled=false,runtimeAllowed=true,runtimeChanging=false,isFlight=false,
    masterSwitch={lastCheck=-1,failed=false,response=0},instance=instance,gatedWidgets={widget},nativeLoadout=adapter,
    nativeSupply=adapter,nativeStandby=adapter,nativeParts=adapter,nativeHud=adapter,weaponPresentation=adapter,
    runtimeContext={canToggleMaster=function(i,held)return canToggle(i,held)end},
    nativeRuntimeGate=function(n)gates[#gates+1]=n;return true end,
    releaseMountPreview=function()return true end,setFixedMountEnabled=function()return true end,
    resetCheckpointState=function()end,resetFlightTracking=function()end,applyFixedMountSetting=function()end,
    tr=function(zh,en)return zh end,note=function()end}, {__index=_G})
local apply=assert(load(master_apply_source..'return tryApplyMasterSwitch','总开关完整交接','t',applyEnv))()
applyEnv.isFlight=true;apply(true);assert(#gates==0 and resets==0 and not applyEnv.runtimeModEnabled)
applyEnv.isFlight=false;apply(true)
assert(applyEnv.runtimeModEnabled and resets==6 and gates[1]==0 and gates[2]==1)
assert(applyEnv.masterSwitch.response==1 and not applyEnv.runtimeChanging)
applyEnv.config.mod_enabled=false;apply(true)
assert(not applyEnv.runtimeModEnabled and resets==12 and gates[#gates]==0)
applyEnv.config.mod_enabled=true;fail=true;apply(true)
assert(not applyEnv.runtimeModEnabled and applyEnv.masterSwitch.failed and applyEnv.masterSwitch.error:find('归还失败'))
local count=#gates;apply(true);assert(#gates==count,'清理失败不能在每帧自动重试')
fail=false;applyEnv.masterSwitch.failed=false;apply(true);assert(applyEnv.runtimeModEnabled)
""")
print('总开关安全入口、同世界/输入所有权、禁止任务内切换及失败停用检查通过')

# 截取入口时同时保留真实上值，避免把固定字段名缺失误报为资源加载失败。
field_names_start = main_source.index('local weaponFieldNames=')
field_names_source = main_source[field_names_start:main_source.index('\n', field_names_start)]
lua.globals().mount_asset_source = field_names_source + '\n' + main_source[main_source.index('local mountAssetStatus={}'):main_source.index('local function mountPreview(context)')]
lua.execute(r'''
-- 执行真实加载修复：精确路径优先，保留原LoadAsset后备和工厂元数据。
local function object(address)
    return {IsValid=function()return true end,GetAddress=function()return address end}
end
local function loader(mode)
    local calls={load=0,factory=0,register=0,warnings=0,logs={}}
    local meshClass=object(80)
    local donor=object(81);donor.IsA=function(_,class)return class==meshClass end
    local rack=object(82);rack.StaticMesh=donor;rack.GetCollisionEnabled=function()return mode=='collision' and 1 or 0 end
    local plane=object(83);plane.SWP1=rack;plane.SWP2=rack;plane.SWP3=rack
    if mode=='unused_collision' then
        plane.SWP3=object(86);plane.SWP3.GetCollisionEnabled=function()return 1 end
    end
    local loaded=object(84)
    local helpers=object(85)
    helpers.CreateAssetData=function(_,source,allowBlueprint)
        assert(source==donor and allowBlueprint==false);calls.factory=calls.factory+1
        return {nativeFactorySentinel=123,ObjectPath='old',PackageName='old',TagsAndValues={keep=true}}
    end
    helpers.GetAsset=function(_,data)
        assert(data.nativeFactorySentinel==123 and data.TagsAndValues.keep)
        assert(data.ObjectPath=='/Game/AC7MultiSP/Research/MSP_case.MSP_case')
        assert(data.PackageName=='/Game/AC7MultiSP/Research/MSP_case' and data.AssetName=='MSP_case')
        assert(data.PackagePath=='/Game/AC7MultiSP/Research' and data.AssetClass=='StaticMesh')
        if mode=='path_error' then error('模拟精确路径临时不可用')end
        if mode~='failed' and mode~='returned' then return loaded end
    end
    local env=setmetatable({runtimeAllowed=true,runtimeModEnabled=true,runtimeFixedMountEnabled=true,
        expandedMountAssets={f16c={{0,'MSP_case',1}}},planeId=1,catalogue={['1']={code='f16c'}},plane=plane,
        valid=function(o)return o and o:IsValid()end,IsInGameThread=function()return true end,
        isSelectedPlane=function(o,code)return o==plane and code=='f16c' end,FName=function(s)return s end,
        previewWarning=function(message)assert(message:find('回退'));calls.warnings=calls.warnings+1 end,
        note=function(message)calls.logs[#calls.logs+1]=message end,
        StaticFindObject=function(path)
            if path=='/Script/AssetRegistry.Default__AssetRegistryHelpers' then return helpers end
            if path=='/Script/Engine.StaticMesh' then return meshClass end
        end,
        LoadAsset=function(path)
            calls.load=calls.load+1
            if mode=='returned' or mode=='path_error' then return loaded,false,true end
            return nil,false,false
        end,
        registerMountAsset=function(id,address)assert(id==0 and address==84);calls.register=calls.register+1;return true,'' end}, {__index=_G})
    return assert(load(mount_asset_source..'return ensureMountAssets','挂架加载入口','t',env))(),calls,env,loaded
end
local load,calls=loader('returned')
assert(load() and calls.load==1 and calls.factory==1 and calls.register==1,'精确路径未就绪时不得丢弃LoadAsset后备返回的对象')
assert(load() and calls.load==1,'成功资源不重复加载')
load,calls=loader('unregistered')
assert(load() and calls.load==0 and calls.factory==1 and calls.register==1,'精确路径成功不应再触发登记表搜索')
assert(calls.logs[1]:find('valid=true'))
load,calls=loader('path_error')
assert(load()and calls.load==1 and calls.register==1,'精确路径异常仍允许后备加载')
assert(calls.logs[1]:find('MOUNT_ASSET_PATH_ERROR')and calls.logs[3]:find('registered=false'))
local env,loaded
load,calls,env,loaded=loader('failed')
assert(not load() and calls.warnings==1 and calls.register==0)
assert(not load() and calls.load==1,'失败不能每帧反复加载')
env.LoadAsset=function()calls.load=calls.load+1;return loaded,false,true end
assert(load(nil,nil,true) and calls.load==2 and calls.register==1,'显式模型快捷键允许重试')
load,calls=loader('collision')
assert(not load() and calls.load==0 and calls.factory==0 and calls.register==0,'启用碰撞的原挂架不能替换为分区网格')
load,calls=loader('unused_collision')
assert(load() and calls.register==1,'未替换的原生架保原生碰撞，不能阻断其他池加载')
load,calls,env=loader('unused_collision')
env.currentProfile={a=0,b=-1,c=-1}
assert(load()and calls.load==0 and calls.register==0,'原生单挂不得为其他组合加载派生资源')
env.expandedMountAssets.f16c={{0,'MSP_case',1,1 << 3},{1,'MSP_unused',3,1 << 5}}
env.currentProfile={a=0,b=1,c=-1}
assert(load()and calls.load==0 and calls.factory==1 and calls.register==1,'仅加载当前选择集合真正使用的资源')
env.currentProfile={a=0,b=2,c=-1}
assert(not load()and calls.load==0,'新方案需要替换的碰撞组件仍须拒绝')
env.currentProfile={a=0,b=1,c=-1}
assert(load()and calls.load==0,'另一组合的资源失败不能锁死已正常加载的组合')
''')
print('挂架加载修复：直接返回、未登记备用路径、工厂元数据、失败缓存/显式重试及碰撞拒绝检查通过')

# 只抽取选择/等待函数；齐射服务的装配需完整运行上下文，不属于本夹具。
lua.globals().weapon_switch_source = (
    main_source[main_source.index('function weaponSwitch.defer'):main_source.index('weaponSwitch.salvo.service=dofile')]
    + main_source[main_source.index('-- 仅普通连射保留待切换请求'):main_source.index('-- 直接选择只在允许武器操作')]
)
lua.execute(r'''
-- 使用真实切换/等待代码；原生忙碌不能重建SP，重试不重复释放输入。
local function switchCase()
    local calls={release=0,native=0,pressed=0,released=0,audio=0,hud=0}
    local state={busy=true,time=10,armed=false,allowPress=true,paused=false}
    local plane={GetAddress=function()return 100 end,IsSpecialWeaponArmed=function()return state.armed end,
        OnWeaponSwitchPressed=function()calls.pressed=calls.pressed+1;if state.allowPress then state.armed=not state.armed end end,
        OnWeaponSwitchReleased=function()calls.released=calls.released+1 end}
    local activator={GetAddress=function()return 200 end,GetSpecialWeaponCount=function()return 20 end}
    local env=setmetatable({weaponSwitch={salvo={},read=function()return true,not state.busy,'native_busy' end,
        press=function(p,a)assert(p==100 and a==200);plane:OnWeaponSwitchPressed();plane:OnWeaponSwitchReleased();return true end},
        plane=plane,activator=activator,currentWorld={},weaponRevision=1,active=0,configured=true,isFlight=true,
        nativeState={cached_sp={20,20,20}},
        game={GetTimeSeconds=function()return state.time end,IsGamePaused=function()return state.paused end},
        valid=function(o)return o~=nil end,ready=function()return true end,IsInGameThread=function()return true end,
        note=function()end,refreshHUD=function()calls.hud=calls.hud+1 end,
        selectionAudio=function()calls.audio=calls.audio+1 end,
        releaseWeaponInput=function()calls.release=calls.release+1 end}, {__index=_G})
    env.native=function(op,index)assert(op=='switch');calls.native=calls.native+1;env.active=index;env.weaponRevision=env.weaponRevision+1 end
    local select=assert(load(weapon_switch_source..'return selectLogicalWeapon','原生忙碌等待切换','t',env))()
    return select,env,state,calls
end
local select,env,state,calls=switchCase()
assert(not select(2) and env.weaponSwitch.pending.desired==2)
assert(calls.release==1 and calls.native==0 and calls.pressed==0 and calls.audio==0)
env.weaponSwitch.pump();assert(calls.native==0)
state.busy=false;state.time=10.02;env.weaponSwitch.pump();assert(calls.native==0,'50ms内不能重试')
state.time=10.06;env.weaponSwitch.pump()
assert(env.active==1 and not env.weaponSwitch.pending and calls.release==1 and calls.native==1)
assert(calls.pressed==1 and calls.released==1 and calls.audio==1 and state.armed)
select,env,state,calls=switchCase();select(2);env.currentWorld={};env.weaponSwitch.pump()
assert(not env.weaponSwitch.pending and calls.native==0,'换世界取消旧请求')
select,env,state,calls=switchCase();select(2);env.weaponRevision=2;env.weaponSwitch.pump()
assert(not env.weaponSwitch.pending and calls.native==0,'配装版本变化取消旧请求')
select,env,state,calls=switchCase();select(2);state.time=15.01;state.busy=false;env.weaponSwitch.pump()
assert(not env.weaponSwitch.pending and calls.native==0,'超过五游戏秒取消')
select,env,state,calls=switchCase();select(2);env.nativeState.cached_sp[2]=0;state.busy=false;env.weaponSwitch.pump()
assert(not env.weaponSwitch.pending and env.inputNotice=='empty_ammo' and calls.native==0)
select,env,state,calls=switchCase();select(2);select(3)
assert(env.weaponSwitch.pending.desired==3 and calls.release==2,'新目标覆盖旧目标')
select,env,state,calls=switchCase();state.busy=false;state.allowPress=false
assert(not select(2) and calls.native==1 and calls.pressed==1 and calls.released==1 and calls.audio==0)
state.allowPress=true;env.weaponSwitch.pump()
assert(not env.weaponSwitch.pending and calls.native==1 and calls.pressed==2 and calls.released==2 and calls.audio==1)
select,env,state,calls=switchCase();select(2);state.busy=false;state.paused=true;env.weaponSwitch.pump()
assert(calls.native==0 and calls.pressed==0,'暂停期间不操作武器')
''')
print('原生连射等待、成对输入、节流/超时、上下文取消、空弹及类别重试检查通过')

lua.execute(r'''
-- 执行真实换机事务：选择回读不等于新玩家生成，旧检查点也不能完成换机回执。
local Aircraft=dofile(script_dir..'/debug_aircraft.lua')
local json=dofile(script_dir..'/json.lua')
local Profile=dofile(script_dir..'/loadout_profile.lua')
local originalFind=StaticFindObject
StaticFindObject=function(path)return {IsValid=function()return true end}end
local function object(id)
    return {IsValid=function()return true end,GetAddress=function()return id end,IsA=function()return true end}
end
local function scenario()
    local state={restart=0,prepared=0,blocked=0,time=0,allowed=true,dlc=false,selected=110,weapon=2,skin=3,emblem=4}
    local function world(id)
        local value=object(id);value.GetFullName=function()return 'World /Game/Maps/Mission/ms06.ms06' end;return value
    end
    local currentWorld=world(900)
    local instance=object(100)
    instance.GetWorld=function()return currentWorld end
    instance.GetSelectedPlaneID=function()return state.selected end
    instance.GetSelectedPlaneSWP=function()return state.weapon end
    instance.GetSelectedPlaneSkinNo=function()return state.skin end
    instance.GetSelectedPlaneEmblemID=function()return state.emblem end
    instance.SetPlaneAndSWP=function(_,id,weapon)if not state.refuseSelection then state.selected=id;state.weapon=weapon end end
    instance.SetSelectedPlaneSkinNo=function(_,v)state.skin=v end
    instance.SetSelectedPlaneEmblemID=function(_,v)state.emblem=v end
    instance.DLCInformation=object(101);instance.DLCInformation.IsDLCOwned=function()return state.dlc end
    local owner=object(300)
    owner.GetWorld=function()return currentWorld end
    owner.IsLocalController=function()return true end;owner.IsLocalPlayerController=function()return true end
    owner.RestartLevel=function()state.restart=state.restart+1;if state.throwRestart then error('模拟重开失败')end end
    local function pawn(id,code)
        local value=object(id);value.code=code;value.bEnableWeaponInput=true
        value.GetWorld=function()return currentWorld end;value.GetController=function()return owner end
        value.IsWeaponSystemDisabled=function()return false end;return value
    end
    local held=pawn(200,'f22a')
    owner.K2_GetPawn=function()return held end
    local context={instance=instance,plane=held,id=110,profile={a=2,b=0,c=-1,ratio=.5},session=7,revision=8}
    local catalog={['110']={code='f22a',supported=true,dlc_id='Base',weapons={{},{},{}}},
        ['120']={code='su35',supported=true,dlc_id='Base',weapons={{},{},{}}},
        ['130']={code='zoef',supported=true,dlc_id='DLC',weapons={{},{},{}}},
        ['444']={code='su35',supported=true,dlc_id='Base',weapons={{},{},{}}}}
    local game={GetPlayerPawn=function()return held end,GetGameInstance=function()return instance end}
    local deps={json=json,copy=Profile.copy,names={f22a='F-22A',su35='Su-35S',zoef='ADF-01'},note=function()end,changed=function()end,
        context=function()return context end,game=function()return game end,catalogue=function()return catalog end,
        owned=function()return true,'[110,120]'end,allowed=function()return state.allowed end,
        canChange=function()return state.allowed,'当前不可换机' end,
        prepare=function()state.prepared=state.prepared+1;assert(state.module:busy())end,
        matches=function(value,code)return value.code==code end,
        bind=function(value)context.plane=value;context.id=state.selected end,
        failFlight=function()state.blocked=state.blocked+1 end}
    state.module=Aircraft.new(deps)
    state.context=context;state.instance=instance;state.deps=deps
    function state:request(number,id,revision)
        self.module:request{debug_aircraft_protocol=2,request=number,session=7,revision=revision or 8,target_plane_id=id or 120}
    end
    function state:spawn(code)
        currentWorld=world(901);held=pawn(201,code or 'su35');return held
    end
    function state:menu()
        currentWorld=world(902);currentWorld.GetFullName=function()return 'World /Game/Maps/Menu/Main.Main' end
    end
    function state:noPawn()currentWorld=world(901);held=nil end
    return state
end
local s=scenario();local view=s.module:state(7,8)
assert(view.available and #view.options==4,'须包含官方无DLC许可项及目录完整的Mod机体')
local modFound=false;for _,option in ipairs(view.options)do if option.plane_id==444 then modFound=option.mod and option.code=='su35' end end;assert(modFound,'Mod须保留编号及来源身份')
local unlicensed=false;for _,option in ipairs(view.options)do if option.plane_id==130 then unlicensed=option.label:find('无DLC许可',1,true)~=nil end end
assert(unlicensed,'缺少DLC只能标注，不能从调试列表移除')
s:request(1,999);assert(s.restart==0 and s.module.status=='failed' and s.module.ack==0)
s:request(2,120,7);assert(s.restart==0,'旧兵装代次不能重开')
s:request(3);assert(s.restart==1 and s.prepared==1 and s.selected==120 and s.weapon==0 and s.skin==0 and s.emblem==4)
assert(s.module:busy()and s.module.status=='restarting' and s.module.ack==3)
assert(s.module:pump('singleplayer')and not s.module:profile(120),'GI选择回读成功但旧Pawn还在，不得反馈生效')
s:request(3);assert(s.restart==1,'请求不能重复重开')
s.allowed=false;assert(s.module:pump('waiting_world')and s.module:busy(),'载入过渡必须保留事务')
s.allowed=true;s:spawn();assert(not s.module:pump('singleplayer'))
assert(s.module:busy()and s.module.status=='restarting','玩家生成仍需等待兵装初始化')
local temporary=s.module:profile(120);assert(temporary.a==0 and temporary.b==-1 and temporary.ratio==1)
s.module:configured(120,temporary);assert(not s.module:busy()and s.module.status=='applied' and s.module.notice:find('Su-35S',1,true))
s.module:configured(120,{a=0,b=1,c=-1,ratio=.5});assert(s.module:profile(120).b==1,'当前任务临时方案须跟随调试换装')
temporary.a=2;assert(s.module:profile(120).a==0,'外部副本不能改写任务临时方案')
s:menu();s.module:pump('singleplayer_menu');assert(not s.module:profile(120),'返回菜单不保留临时换机方案')

s=scenario();s.throwRestart=true;s:request(1)
assert(s.restart==1 and not s.module:busy()and s.module.status=='failed' and s.selected==110 and s.weapon==2)
assert(s.skin==3 and s.emblem==4 and s.module:profile(110).a==2 and s.blocked==0,'同一原世界失败须恢复选择及临时原方案')
s=scenario();s.refuseSelection=true;s:request(1);assert(s.restart==0 and s.module.status=='failed')
s=scenario();s:request(1);s.allowed=false;s.module:pump('multiplayer_mode')
assert(not s.module:busy()and s.module.status=='failed' and s.selected==120,'进入多人只取消，不写回旧选择')
s=scenario();s:request(1);s:menu();s.module:pump('singleplayer_menu');assert(not s.module:busy())
s=scenario();s:request(1);s:spawn('f22a');s.module:pump('singleplayer')
assert(s.module.status=='failed' and s.blocked==1 and s.module.blocked,'原生仍生成旧机时必须报失败，不能伪报换机成功')
s=scenario();s.deps.owned=function()return false,'当前存档身份不符'end
view=s.module:state(7,8);assert(view.available and #view.options==4,'购买信息读取失败不隐藏用户要求的调试候选')
s=scenario();s:request(1,130);assert(s.restart==1 and s.selected==130,'无DLC许可项必须允许尝试重开，不改购买或许可')
local originalClock=os.clock;local now=0;os.clock=function()return now end
s=scenario();s:request(1);now=16;s.module:pump('singleplayer')
assert(not s.module:busy()and s.module.status=='failed' and s.selected==110 and s.blocked==0,'原世界15秒无反应须恢复原机选择')
now=0;s=scenario();s:request(1);s:noPawn();now=16
assert(s.module:pump('singleplayer')and s.module:busy(),'新世界仍在生成玩家时应继续等待')
now=181;s.module:pump('singleplayer')
assert(not s.module:busy()and s.module.blocked and s.blocked==1,'新世界超时不能恢复旧选择或继续普通接管')
os.clock=originalClock
StaticFindObject=originalFind
''')
print('原生重开换机：官方编号/许可标记、未持有DLC可尝试、过期拒绝、代次等待、实际玩家确认、临时方案与失败/退出隔离检查通过')
