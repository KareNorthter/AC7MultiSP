"""Run the shipped Lua controller with mocked UE4SS/game/IPC, without AC7."""
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools/test-python"))
from lupa.lua54 import LuaRuntime

lua = LuaRuntime(unpack_returned_tuples=True)
lua.globals().scriptRoot = str(ROOT / "dist/DualSP").replace("\\", "/") + "/"
lua.execute(r'''
local realDofile=dofile
hudCalls={};switchHudCommits=0;pendingHudCommit=nil
function dofile(path)
    local module=realDofile(path)
    if path==scriptRoot..'Scripts/native_hud.lua' then
        local create=module.new
        module.new=function(note)
            local adapter=create(note);local update=adapter.update
            function adapter:update(plane,profile,options,state,enabled,now)
                hudCalls[#hudCalls+1]={active=state.active,counts={state.cached_sp[1],state.cached_sp[2]}}
                if pendingHudCommit then
                    assert(state.active==pendingHudCommit,'HUD commit must use the new native selection')
                    pendingHudCommit=nil;switchHudCommits=switchHudCommits+1
                end
                return update(self,plane,profile,options,state,enabled,now)
            end
            return adapter
        end
    end
    return module
end
testJson=realDofile(scriptRoot.."Scripts/json.lua")
files={};writes={};scans={}
os.remove=function(path)assert(files[path]);files[path]=nil;return true end
files[scriptRoot.."profiles.json"]=testJson.encode({schema=1,hud=true,hud_scale=1,ignore_unlocks=true,profiles={["110"]={a=0,b=1,ratio=.5}}})
files[scriptRoot.."catalogue.json"]=testJson.encode({["110"]={code="f22a",supported=true,weapons={
    {index=0,code="qaam",max=26,stats={1,2,3,4,5,6}},
    {index=1,code="xsdb",max=48,stats={1,2,3,4,5,6}},
    {index=2,code="4aam",max=40,stats={1,2,3,4,5,6}}}}})
io.open=function(path,mode)
    if mode=="r" and not files[path]then return nil end
    local buffer=mode=="a" and (files[path]or"")or""
    return {read=function()return files[path]end,write=function(self,...)
        for _,value in ipairs({...})do buffer=buffer..tostring(value)end
    end,flush=function()files[path]=buffer end,close=function()files[path]=buffer;if mode~='r'then writes[path]=(writes[path]or 0)+1 end end}
end
clock=0;os.clock=function()return clock end
actions={};delays={};hooks={};commandCount=0;failSnapshot=false
local function valid(self)return true end
act={IsValid=valid,GetAddress=function()return 0x200 end,ammo=26,main=134,MaxSpecialWeaponCount=26}
function act:GetSpecialWeaponCount()return self.ammo end
function act:GetMainWeaponCount()return self.main end
world={IsValid=valid,GetAddress=function()return 900 end,GetFullName=function()return "World /Game/Maps/Mission/ms06/ms06.ms06"end}
plane={IsValid=valid,GetAddress=function()return 0x100 end,bEnableWeaponInput=true,PlayerWeaponActivator=act,armed=false,world=world}
function plane:GetFullName()return "PlayerPlane /Game/Maps/Mission/ms06/ms06.ms06:PersistentLevel.AcePlayerPawn_1" end
function plane:GetWorld()return self.world end
function plane:IsWeaponSystemDisabled()return false end
function plane:IsSpecialWeaponArmed()return self.armed end
function plane:InitializeWeaponSystem(config)act.ammo=({26,48,40})[config+1];self.armed=false end
function plane:OnWeaponSwitchPressed()
    assert(pendingHudCommit==nil,'Native switch HUD must commit before the next input phase, without waiting for a HUD poll')
    self.pending=true
end
function plane:OnWeaponSwitchReleased()
    if self.pending then
        if self.armed then self.armed=false elseif act.ammo>0 then self.armed=true end
    end
    self.pending=false
end
game={GetTimeSeconds=function()return clock end,IsGamePaused=function()return false end,SetGamePaused=function()end}
instance={IsValid=valid,GetWorld=function()return world end,GetSelectedPlaneID=function()return 110 end,GetSelectedPlaneSWP=function()return 0 end,
    GameInformation={IsValid=valid,PlaneDataTable={IsValid=valid,GetAddress=function()return 0x300 end},WeaponDataTable={IsValid=valid,GetAddress=function()return 0x400 end}}}
function FindFirstOf(name)scans[name]=(scans[name]or 0)+1;if name=="NimbusGameInstance"then return instance end end
function FindAllOf(name)scans[name]=(scans[name]or 0)+1;if name=="PlayerPlane"then return retainedPlane and {retainedPlane,plane}or{plane}end;return {}end
function StaticFindObject()return game end
function RegisterHook(name,pre,post)hooks[name]={pre,post}end
function ExecuteInGameThread(fn)fn()end
gameThread=true;gameThreadInitialized=false
function IsInGameThread()if not gameThreadInitialized then error('GetGameThreadId called too early, not yet initialized by UGameEngine::Tick')end;return gameThread end
function ExecuteInGameThreadWithDelay(ms,fn)delays[#delays+1]={due=clock+ms/1000,fn=fn}end
function LoopInGameThreadWithDelay(ms,fn)loop=fn end
function ExecuteWithDelay()error("Async timer must never be used")end
function LoopAsync()error("Async loop must never be used")end
controller={IsValid=valid,bShowMouseCursor=true}
local originalFindFirst=FindFirstOf
function FindFirstOf(name)if name=="NimbusPlayerController"then return controller end;return originalFindFirst(name)end
session={active=0,ammo={13,24},maximum={13,24}}
local function snapshot()
    session.ammo[session.active+1]=act.ammo
    return {ok=true,active=session.active,armed=plane.armed,main_ammo=act.main,special_ammo=act.ammo,
        cached_sp={session.ammo[1],session.ammo[2]},maximum=session.maximum,slots={{1,.2},{1,1,1,1}}}
end
package.loadlib=function(path,name)
    if name=="lab_native_execute"then return function()
        commandCount=commandCount+1
        local parts={};for token in files[scriptRoot.."native-command.txt"]:gmatch("%S+")do parts[#parts+1]=token end
        local op,target,request=parts[1],tonumber(parts[4]),tonumber(parts[9])
        if op=="snapshot" and failSnapshot then files[scriptRoot.."native-status.json"]=testJson.encode({ok=false,request=request,error="test: non-finite timer"});return end
        if op=="setup"then session.active=0;act.ammo=13;session.ammo={13,24}
        elseif op=="switch"then session.ammo[session.active+1]=act.ammo;session.active=target;act.ammo=session.ammo[target+1];pendingHudCommit=target
        elseif op=="checkpoint_save"or op=="snapshot"then
        else error("Unexpected native operation: "..op)end
        local reply=snapshot();reply.request=request
        files[scriptRoot.."native-status.json"]=testJson.encode(reply)
    end end
    if name=="dualsp_publish"then return function()files[scriptRoot.."ui-state.json"]=files[scriptRoot.."ui-state.pending.json"]end end
    if name=="dualsp_drain_actions"then return function()if #actions>0 then files[scriptRoot.."ui-actions.json"]=testJson.encode(actions);actions={}end end end
    if name=="dualsp_catalogue"or name=="dualsp_overlay_start"then return function()end end
    error("Unexpected export: "..name)
end
function step(dt)
    clock=clock+(dt or .11)
    local due={};local pending={}
    for _,job in ipairs(delays)do if job.due<=clock then due[#due+1]=job else pending[#pending+1]=job end end
    delays=pending;for _,job in ipairs(due)do job.fn()end
    loop()
end
function ui()return testJson.decode(files[scriptRoot.."ui-state.json"])end
function cycleKey()actions={{type="cycle"}};step();step()end
function originalKey()plane:OnWeaponSwitchPressed();plane:OnWeaponSwitchReleased();step();step()end
function expectSelection(logical)
    local actual=plane.armed and session.active+1 or 0
    assert(actual==logical,"Selection: wanted "..logical..", got "..actual)
end
''')
path = ROOT / "dist/DualSP/Scripts/main.lua"
lua.execute("assert(load(..., '@'..scriptRoot..'Scripts/main.lua'))()", path.read_text(encoding="utf-8"))
lua.execute(r'''
checks=0
local function check(name,fn)fn();checks=checks+1 end
check("wait for loader game-thread readiness",function()
    step();assert(not files[scriptRoot..'ui-state.json'])
    gameThreadInitialized=true;step();step()
end)
check("configured",function()assert(ui().configured and act.ammo==13 and session.ammo[2]==24)end)
check("panel hides engine cursor and restores it",function()
    actions={{type="open"}};step();assert(not controller.bShowMouseCursor)
    actions={{type="close"}};step();assert(controller.bShowMouseCursor)
end)
check("F9 MSL>A",function()cycleKey();expectSelection(1)end)
check("F9 A>B",function()act.ammo=6;act.main=131;cycleKey();expectSelection(2);assert(session.ammo[1]==6)end)
check("F9 B>MSL",function()act.ammo=14;cycleKey();expectSelection(0);assert(session.ammo[2]==14)end)
check("live HUD",function()step(1.01);assert(ui().state.main_ammo==131 and ui().state.cached_sp[1]==6 and ui().state.cached_sp[2]==14)end)
check("original MSL>A",function()originalKey();expectSelection(1)end)
check("original A>B",function()originalKey();expectSelection(2)end)
check("original B>MSL",function()originalKey();expectSelection(0)end)
check("original exhausted A reaches B",function()
    originalKey();expectSelection(1);act.ammo=0
    originalKey();expectSelection(2);assert(session.ammo[1]==0 and act.ammo==14)
    originalKey();expectSelection(0)
    originalKey();expectSelection(2)
end)
check("F9 exhausted A reaches B",function()cycleKey();expectSelection(0);cycleKey();expectSelection(2);assert(session.ammo[1]==0)end)
check("both SP empty retains MSL",function()act.ammo=0;cycleKey();expectSelection(0);cycleKey();expectSelection(0);originalKey();expectSelection(0)end)
check("empty B skipped",function()
    session.ammo[1]=3;cycleKey();expectSelection(1);cycleKey();expectSelection(0);originalKey();expectSelection(1);originalKey();expectSelection(0)
end)
check("native switch commits HUD before delayed input",function()
    assert(switchHudCommits>0 and pendingHudCommit==nil)
    assert(hudCalls[#hudCalls].active==session.active)
end)
check("idle flight avoids native transactions and global scans",function()
    local beforeCommands=commandCount;local beforeScans=0
    for _,n in pairs(scans)do beforeScans=beforeScans+n end
    local beforeWrites=writes[scriptRoot..'ui-state.pending.json']or 0
    for i=1,500 do step(.02)end
    local afterScans=0;for _,n in pairs(scans)do afterScans=afterScans+n end
    local publications=(writes[scriptRoot..'ui-state.pending.json']or 0)-beforeWrites
    assert(commandCount==beforeCommands and afterScans==beforeScans)
    assert(publications<=11 and not files[scriptRoot..'ui-actions.json'])
    perf={simulated_seconds=10,native_transactions=0,global_registry_scans=0,ui_state_writes=publications}
end)
check("world transition rejects retained valid actor",function()
    retainedPlane=plane
    world={IsValid=function()return true end,GetAddress=function()return 901 end,GetFullName=function()return 'World /Game/Maps/Mission/ms02/ms02.ms02'end}
    local fresh={};for k,v in pairs(plane)do fresh[k]=v end
    fresh.world=world;fresh.GetAddress=function()return 0x110 end;fresh.armed=false
    plane=fresh;local before=scans.PlayerPlane;step()
    assert(scans.PlayerPlane==before+1 and ui().configured and act.ammo==13,
        'transition scans='..scans.PlayerPlane..' before='..before..' ammo='..act.ammo..' ui='..testJson.encode(ui()))
    assert(retainedPlane:IsValid() and retainedPlane:GetWorld():GetAddress()==900)
    retainedPlane=nil
end)
check("error invalidates HUD",function()
    failSnapshot=true;actions={{type='cycle'}};step();local s=ui();assert(s.error:find("non%-finite") and s.state==nil)
end)
check("error stops mutations explicitly",function()
    local before=commandCount;cycleKey();assert(commandCount==before and ui().error)
end)
check("game-thread guard catches wrong scheduler",function()
    gameThread=false;step();assert(ui().error:find("outside the game thread",1,true));gameThread=true
end)
''')
cycle = lua.eval("dofile(scriptRoot..'Scripts/weapon_cycle.lua')")
matrix = 0
for a, b in ((0, 0), (0, 14), (6, 0), (6, 14)):
    for current in range(3):
        expected = (1 if a else 2 if b else 0) if current == 0 else (2 if b else 0) if current == 1 else 0
        assert cycle.next(current, a, b) == expected
        matrix += 1
result = {"controller_scenarios": lua.globals().checks, "cycle_matrix_cases": matrix, "game_started": False,
          "idle_flight":dict(lua.globals().perf.items())}
print(json.dumps(result, indent=2))
(ROOT / "evidence/0.1.7-controller-tests.json").write_text(json.dumps(result, indent=2), encoding="utf-8")
