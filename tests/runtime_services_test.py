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
config.profiles['10']={a=2,b=1,c=0,ratio=1/3};currentStore:save(config)
local original=readText(fixture..'current-profiles.json')
local action={request=1,management_protocol=3,language='en',central_hint=false,keyboard_keys={49,50,51,52}}
local nextConfig=currentStore:applySettings(config,action)
assert(config.language=='auto' and nextConfig.language=='en' and nextConfig.profiles['10'].a==2)
assert(readText(fixture..'current-profiles.previous.json')==original)
action.keyboard_keys={49,49,51,52}
local saved=readText(fixture..'current-profiles.json')
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
local old={schema=1,profiles={['9']={a=0,b=1,ratio=.75}},keyboard_keys={49,50,51},management_key=119}
writeJson(fixture..'migration-profiles.json',old)
local oldBytes=readText(fixture..'migration-profiles.json')
local migrated=store('migration-'):load()
assert(migrated.schema==2 and migrated.profiles['9'].c==-1 and migrated.profiles['9'].ratio==.5)
assert(migrated.management_key==119 and migrated.keyboard_keys[4]==52)
assert(readText(fixture..'migration-profiles.before-three-sp.json')==oldBytes)
old.keyboard_keys={49,50,52};writeJson(fixture..'conflict-profiles.json',old)
local conflict=readText(fixture..'conflict-profiles.json')
rejects(function()store('conflict-'):load()end,'旧键位已占用 4')
assert(readText(fixture..'conflict-profiles.json')==conflict)
assert(not readText(fixture..'conflict-profiles.before-three-sp.json'))
-- 旧响应、原生拒绝均不能被当作成功；失败后请求号继续递增。
local replyMode='ok';local requestCount=0
local client=Bridge.new{root=fixture,profile=Profile,readJson=readJson,execute=function()
    requestCount=requestCount+1
    local command=readText(fixture..'native-command.txt')
    assert(command=='switch 10 20 1 1.250000000 3 3 2 0 1 '..requestCount,command)
    writeJson(fixture..'native-status.json',{request=replyMode=='stale' and 0 or requestCount,
        ok=replyMode~='rejected',error='原生拒绝',active=1})
end}
local command={op='switch',plane=16,activator=32,target=1,time=1.25,profile={a=2,b=0,c=1}}
assert(client:request(command).active==1)
replyMode='stale';rejects(function()client:request(command)end,'does not match')
replyMode='rejected';rejects(function()client:request(command)end,'原生拒绝')
replyMode='ok';assert(client:request(command).request==4)
-- 页面缺依赖立即拒绝；关闭/重置保留错误的语义不变。
local function deps(mode)
    return {mode=mode,note=function()end,translate=function(zh)return zh end,keyboardNavigation=function()end,
        commit=function()end,takeoff=function()end,preparationAction=function()end,
        confirmSound=function()end,weaponIcon=function()end,partsAmmoPreview=function()end}
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
''')
print('配置保存/迁移/失败隔离、协议请求匹配、四页面具名接口通过')
