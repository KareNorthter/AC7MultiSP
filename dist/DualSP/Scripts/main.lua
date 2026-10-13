-- DualSP 0.1.7. All recurring and delayed Lua callbacks run on the game thread.
-- Flight HUD uses native UMG controls. The loadout menu still uses DX11.
local source=debug.getinfo(1,"S").source:gsub("^@",""):gsub("\\","/")
local root=assert(source:match("^(.*)/Scripts/[^/]+$"),"Cannot locate DualSP directory").."/"
local json=dofile(root.."Scripts/json.lua")
local weaponCycle=dofile(root.."Scripts/weapon_cycle.lua")
local log=assert(io.open(root.."mod.log","a"))
local function note(message)log:write(os.date("!%Y-%m-%dT%H:%M:%SZ ")..tostring(message).."\n");log:flush()end
local nativeHud=dofile(root.."Scripts/native_hud.lua").new(note)
local nativeLoadout=nil
local function readJson(path)
    local file=io.open(path,"r");if not file then return nil end
    local text=file:read("*a");file:close();return json.decode(text)
end
local function writeJson(path,value)
    local file=assert(io.open(path,"w"));file:write(json.encode(value));file:close()
end
local function loadExport(name,dll)
    local fn,err=package.loadlib(root.."bin/"..(dll or "DualSP.dll"),name);assert(fn,err);return fn
end
-- The validated weapon core and the rebuilt menu renderer are separate modules.
local bridge=loadExport("lab_native_execute")
local catalogExport=loadExport("dualsp_catalogue")
local publish=loadExport("dualsp_publish","DualSPUI.dll")
local drain=loadExport("dualsp_drain_actions","DualSPUI.dll")
local overlay=loadExport("dualsp_overlay_start","DualSPUI.dll")
local config=readJson(root.."profiles.json") or {schema=1,hud=true,hud_scale=1,ignore_unlocks=false,profiles={}}
assert(config.schema==1 and type(config.profiles)=="table","Unsupported DualSP configuration schema")
local function saveConfig()
    -- Keep the previous settings recoverable before replacing the small mod file.
    local old=io.open(root.."profiles.json","r")
    if old then local text=old:read("*a");old:close();local backup=assert(io.open(root.."profiles.previous.json","w"));backup:write(text);backup:close()end
    writeJson(root.."profiles.json",config)
end
local catalogue=nil
local game=nil
local plane=nil
local activator=nil
local planeAddress=nil
local instance=nil
local planeId=0
local currentProfile=nil
local configured=false
local dual=false
local lastArmed=nil
local active=0
local internal=false
local panelOpen=false
local pausedBefore=false
local gatedWidgets={}
local cursorController=nil
local previousShowCursor=nil
local requestOpen=0
local requestClose=0
local errorText=nil
local nativeState=nil
local publishSequence=0
local checkpointPending=false
local restorePending=false
local resupplyPending=false
local nativeRequest=0
local lastPublish=0
local lastActions=-1
local lastHUD=-1
local lastUIKey=nil
local flightOptions=nil
local currentWorld=""
local isFlight=false
local lastSelected=-1
local lastPlaneScan=-1
local cachedPlane=nil
local cachedWorld=""
local lastSaveScan=-1
local saveObjects={}
local descriptions={
    qaam="高机动空对空导弹",sasm="小型空爆导弹",hcaa="高容量空对空导弹",hpaa="高威力空对空导弹",hvaa="高速空对空导弹",laam="远程空对空导弹",saam="半主动雷达制导导弹",
    ["4aam"]="四目标空对空导弹",["6aam"]="六目标空对空导弹",["8aam"]="八目标空对空导弹",
    lasm="远程反舰导弹",lagm="远程空对地导弹",lacm="远程巡航导弹",["4agm"]="四目标空对地导弹",["8agm"]="八目标空对地导弹",
    ugb="无制导炸弹",gpb="精确制导炸弹",xsdb="小直径炸弹",sffs="自锻破片子母弹",sod="远距撒布弹药",faeb="燃料空气炸弹",
    eml="电磁轨道炮",plsl="脉冲激光炮",tls="战术激光系统",rkt="火箭弹",grkt="制导火箭弹",mgp="机炮吊舱",mpbm="多用途爆炸导弹",uav="无人机",ecm="电子干扰",mstm="多目标导弹",admm="全向多用途导弹"}
local unsupportedDual={uav=true,ecm=true}
local names={f04e="F-4E Phantom II",f15c="F-15C Eagle",f15e="F-15E Strike Eagle",f18f="F/A-18F Super Hornet",f16c="F-16C Fighting Falcon",f22a="F-22A Raptor",f35c="F-35C Lightning II",a10a="A-10C Thunderbolt II",f14d="F-14D Super Tomcat",f02a="F-2A",f15j="F-15J",j39e="Gripen E",m21b="MiG-21bis",m29a="MiG-29A",m31b="MiG-31B",mr2k="Mirage 2000-5",rflm="Rafale M",typn="Typhoon",pkfa="Su-57",su30="Su-30M2",su30sm="Su-30SM",su33="Su-33",su34="Su-34",su35="Su-35S",su37="Su-37",su47="Su-47",x02s="X-02S Strike Wyvern",adf11f="ADF-11F Raven",zoef="ADF-01 FALKEN",mrgn="ADFX-01 Morgan",dark="DarkStar",f18e="F/A-18E",f18etg="F/A-18E Top Gun",f14a="F-14A",f14atg="F-14A Top Gun",su57tg="Su-57 Top Gun",f104="F-104C",asfx="ASF-X Shinden II",fa27="XFA-27",fa44="CFA-44 Nosferatu",f15m="F-15 S/MTD",f16x="F-16XL",fb22="FB-22",f18x="F/A-18F Block III",f02x="F-2A Super Kai",m35d="MiG-35D",yf23="YF-23"}

local function valid(object)return object and object:IsValid()end
local function native(op,target)
    assert(IsInGameThread(),"Native weapon access requires the game thread")
    assert(valid(plane) and valid(activator),"Player plane is unavailable")
    local time=game:GetTimeSeconds(plane)
    local f=assert(io.open(root.."native-command.txt","w"))
    nativeRequest=nativeRequest+1
    f:write(string.format("%s %X %X %d %.9f %d %d %.6f %d",op,plane:GetAddress(),activator:GetAddress(),target or 0,time,currentProfile.b,currentProfile.a,currentProfile.ratio,nativeRequest));f:close()
    bridge();local result=assert(readJson(root.."native-status.json"))
    assert(result.request==nativeRequest,"Native response does not match the current request")
    assert(result.ok,result.error)
    active=result.active;nativeState=result
    -- Commit the presentation in the same game-thread transaction. Fixed SP
    -- rows cannot exchange identities, and the arrow need not wait for the
    -- next 100 ms ammo refresh after a switch/checkpoint/resupply.
    if configured and dual and flightOptions then
        nativeHud:update(plane,currentProfile,flightOptions,nativeState,config.hud and not errorText,os.clock())
    end
    return result
end
local function ownedWeapons()
    local set={}
    if os.clock()-lastSaveScan>1 then saveObjects=FindAllOf("SigmaSaveGame")or{};lastSaveScan=os.clock()end
    for _,save in ipairs(saveObjects)do
        if valid(save)then save.CommonData.PurchasedWeapons:ForEach(function(_,value)set[value:get()]=true end)end
    end
    return set
end
local function optionsFor(id)
    -- Purchases cannot change during a sortie. Do not scan the global object
    -- registry or enumerate PurchasedWeapons for every HUD refresh.
    if configured and isFlight and id==planeId and flightOptions then return flightOptions end
    local info=catalogue and catalogue[tostring(id)]
    if not info or not info.supported then return {} end
    local owned=ownedWeapons();local options={}
    for i,item in ipairs(info.weapons)do
        options[i]={index=item.index,code=item.code,max=item.max,stats=item.stats,description=descriptions[item.code]or item.code:upper(),
            owned=config.ignore_unlocks or item.index==0 or owned[id*10+item.index]==true,
            available=not unsupportedDual[item.code]}
    end
    return options
end
local function profileFor(id)
    local saved=config.profiles[tostring(id)]
    if saved then return {a=saved.a,b=saved.b,ratio=saved.ratio}end
    local selected=valid(instance) and instance:GetSelectedPlaneSWP()or 0
    return {a=math.max(0,math.min(2,selected)),b=-1,ratio=.5}
end
nativeLoadout=dofile(root.."Scripts/native_loadout.lua").new(note,function(id,profile)
    assert(not isFlight and id==planeId,"Aircraft changed before native preparation commit")
    local options=optionsFor(id)
    assert(options[profile.a+1].owned and (profile.b<0 or options[profile.b+1].owned),"Prepared SP is not purchased")
    assert(profile.b<0 or (profile.a~=profile.b and options[profile.a+1].available and options[profile.b+1].available),"Prepared SP pair is incompatible")
    currentProfile=profile;config.profiles[tostring(id)]=profile;saveConfig()
    lastUIKey=nil;note("PROFILE_SAVED_NATIVE "..id)
end)
local function setPanel(open)
    if panelOpen==open then return end
    panelOpen=open
    if open then
        -- Restore the original menu before F8 takes its normal input gate.
        -- Closing F8 restarts preparation from its newly saved profile.
        nativeLoadout:reset(false)
        -- Hide the engine's software cursor as well as the OS pointer. The
        -- overlay owns a separate pointer until this panel closes.
        cursorController=FindFirstOf("NimbusPlayerController")
        if valid(cursorController)then
            previousShowCursor=cursorController.bShowMouseCursor
            cursorController.bShowMouseCursor=false
        end
        if isFlight and valid(plane)then pausedBefore=game:IsGamePaused(plane);game:SetGamePaused(plane,true)end
        gatedWidgets={}
        for _,widget in ipairs(FindAllOf("MenuBaseWidget")or{})do
            local name=widget:GetFullName()
            if (name:find("/Engine/Transient.",1,true)or name:find("/Game/Maps/",1,true)) and not name:find("WidgetArchetype",1,true) and widget:IsEnableControl()then
                widget:SetIsEnableControl(false);gatedWidgets[#gatedWidgets+1]=widget
            end
        end
    else
        if valid(cursorController)then cursorController.bShowMouseCursor=previousShowCursor end
        cursorController=nil;previousShowCursor=nil
        for _,widget in ipairs(gatedWidgets)do if valid(widget)then widget:SetIsEnableControl(true)end end
        gatedWidgets={}
        if isFlight and valid(plane)then game:SetGamePaused(plane,pausedBefore)end
    end
end
local function ready()return valid(plane) and plane.bEnableWeaponInput and not plane:IsWeaponSystemDisabled()end
local function configureFlight(profile)
    assert(ready(),"Wait until the mission's flight controls are active")
    assert(not configured,"Loadout is locked for this sortie")
    local options=optionsFor(planeId)
    assert(#options==3 and options[profile.a+1].owned and (profile.b<0 or options[profile.b+1].owned),"Saved loadout includes an unpurchased SP")
    currentProfile=profile
    -- Initial loadout selection is performed once, before gameplay resumes.
    plane:InitializeWeaponSystem(profile.a)
    dual=profile.b>=0
    if dual then native("setup",0);native("checkpoint_save",0)else
        nativeState={active=0,armed=false,main_ammo=activator:GetMainWeaponCount(),cached_sp={activator:GetSpecialWeaponCount(),0},maximum={activator.MaxSpecialWeaponCount,0},slots={{},{}}}
    end
    configured=true;active=0;lastArmed=plane:IsSpecialWeaponArmed();note("LOADOUT "..planeId.." "..json.encode(profile))
    flightOptions=options
end
local function delay(ms,fn)
    ExecuteInGameThreadWithDelay(ms,function()
        local ok,err=xpcall(function()
            assert(IsInGameThread(),"Delayed switch callback is outside the game thread")
            fn()
        end,debug.traceback)
        if not ok then errorText=tostring(err);internal=false;note("ERROR "..errorText)end
    end)
end
local function pressSwitch(done)
    internal=true
    local switchedAddress=plane:GetAddress()
    plane:OnWeaponSwitchPressed()
    delay(80,function()
        if not valid(plane)or plane:GetAddress()~=switchedAddress then internal=false;note("SWITCH_CANCELLED_WORLD_CHANGE");return end
        plane:OnWeaponSwitchReleased();lastArmed=plane:IsSpecialWeaponArmed();internal=false
        if done then done()end
    end)
end
local function cycle()
    if not configured or panelOpen or internal or errorText or not ready()or game:IsGamePaused(plane)then return end
    if not dual then pressSwitch();return end
    local live=native("snapshot")
    local armed=plane:IsSpecialWeaponArmed()
    local desired=weaponCycle.next(armed and active+1 or 0,live.cached_sp[1],live.cached_sp[2])
    if desired==0 then
        if armed then pressSwitch()end
    else
        if active~=desired-1 then native("switch",desired-1)end
        if not armed then pressSwitch()else lastArmed=plane:IsSpecialWeaponArmed()end
    end
end
local function observeOriginalSwitch()
    if not dual or not configured or internal or panelOpen or errorText or not ready()or game:IsGamePaused(plane)then return end
    local armed=plane:IsSpecialWeaponArmed()
    -- The live SP count is authoritative; inactive SP counts are captured on
    -- each swap. A consumed last round must not wait for the 100 ms HUD update.
    local ammo={nativeState.cached_sp[1],nativeState.cached_sp[2]}
    ammo[active+1]=activator:GetSpecialWeaponCount()
    if lastArmed~=nil and armed~=lastArmed then
        local desired=weaponCycle.next(lastArmed and active+1 or 0,ammo[1],ammo[2])
        if desired>0 then
            if active~=desired-1 then native("switch",desired-1)end
            if not armed then pressSwitch()else lastArmed=plane:IsSpecialWeaponArmed()end
        else lastArmed=armed end
    else
        -- While MSL is armed, install the first nonempty SP in advance. This
        -- lets the game's original switch key work even if its prior SP is empty
        -- and no armed-category transition would otherwise occur.
        if not armed then
            local desired=weaponCycle.next(0,ammo[1],ammo[2])
            if desired>0 and active~=desired-1 then native("switch",desired-1)end
        end
        lastArmed=armed
    end
end
local function applyAction(action)
    if action.type=="open"then setPanel(true)
    elseif action.type=="close"then
        if isFlight and ready()and not configured and not errorText then configureFlight({a=currentProfile.a,b=-1,ratio=1})end
        setPanel(false)
    elseif action.type=="cycle"then cycle()
    elseif action.type=="settings"then
        if action.native_loadout~=nil then config.native_loadout=action.native_loadout==true end
        config.hud=action.hud==true;config.hud_scale=math.max(.65,math.min(1.5,action.hud_scale));config.ignore_unlocks=action.ignore_unlocks==true;saveConfig()
    elseif action.type=="apply"then
        assert(action.plane_id==planeId,"Plane selection changed; select the loadout again")
        assert(not isFlight or not configured,"This sortie's loadout is locked")
        assert(action.a>=0 and action.a<=2 and action.b>=-1 and action.b<=2 and action.a~=action.b,"Invalid SP pair")
        local options=optionsFor(planeId)
        assert(#options==3 and options[action.a+1].owned and (action.b<0 or options[action.b+1].owned),"SP is not purchased")
        assert(action.b<0 or (options[action.a+1].available and options[action.b+1].available),"UAV and ECM dual loadouts are not supported in this build")
        local profile={a=action.a,b=action.b,ratio=math.max(.1,math.min(1,action.ratio))}
        if isFlight then configureFlight(profile)else instance:SetPlaneAndSWP(planeId,profile.a);currentProfile=profile end
        config.profiles[tostring(planeId)]=profile;saveConfig();setPanel(false);requestClose=requestClose+1
        -- Acknowledge through a monotonically changing UI state field.
        note("PROFILE_SAVED "..planeId)
    else error("Unknown UI action: "..tostring(action.type))end
end
local function locatePlane()
    local worldContext=instance:GetWorld()
    if not valid(worldContext)then return nil,""end
    for _,candidate in ipairs(FindAllOf("PlayerPlane")or{})do
        local name=candidate:GetFullName()
        if name:find(":PersistentLevel.AcePlayerPawn_",1,true) then
            local candidateWorld=candidate:GetWorld()
            local world=candidateWorld:GetFullName()
            if candidateWorld:GetAddress()~=worldContext:GetAddress()then goto nextPlane end
            if world:find("/Game/Maps/Mission/",1,true)or world:find("/Game/Maps/FreeFlight/",1,true)then return candidate,world end
        end
        ::nextPlane::
    end
    return nil,""
end
local function publishState()
    publishSequence=publishSequence+1
    local options=optionsFor(planeId)
    local info=catalogue and catalogue[tostring(planeId)]
    local state={schema=1,scene=isFlight and "flight"or"menu",plane_id=planeId,plane_name=info and (names[info.code]or info.code:upper())or"请选择机体",
        profile=currentProfile or profileFor(planeId),weapons=options,allow_edit=not configured and #options==3 and (not isFlight or ready()),
        configured=configured,state=nativeState,hud=config.hud,hud_scale=config.hud_scale,ignore_unlocks=config.ignore_unlocks,request_open=requestOpen,request_close=requestClose}
    state.sequence=publishSequence
    state.native_hud=nativeHud.status
    state.native_loadout=nativeLoadout.status
    state.native_loadout_enabled=config.native_loadout~=false
    if nativeLoadout.error then state.menu_error=nativeLoadout.error end
    if nativeHud.error then state.hud_error=nativeHud.error end
    if errorText then state.error=errorText;state.state=nil end
    writeJson(root.."ui-state.pending.json",state);publish()
end
local function tick()
    if not valid(instance)then instance=FindFirstOf("NimbusGameInstance")end
    if not valid(instance)then return end
    if not game then game=StaticFindObject("/Script/Engine.Default__GameplayStatics")end
    if not catalogue then
        local info=instance.GameInformation;if not valid(info)or not valid(info.PlaneDataTable)or not valid(info.WeaponDataTable)then return end
        local f=assert(io.open(root.."catalogue-command.txt","w"));f:write(string.format("%X %X",info.PlaneDataTable:GetAddress(),info.WeaponDataTable:GetAddress()));f:close()
        catalogExport();catalogue=assert(readJson(root.."catalogue.json"));assert(not catalogue.error,catalogue.error);note("CATALOGUE_READY")
    end
    -- A live player actor remains valid for its world. Registry searches are
    -- needed while acquiring a new actor, not four times per second in flight.
    if cachedPlane then
        local worldContext=instance:GetWorld()
        if not valid(cachedPlane)or not valid(worldContext)or cachedPlane:GetWorld():GetAddress()~=worldContext:GetAddress()then cachedPlane=nil;lastPlaneScan=-1 end
    end
    if not cachedPlane and os.clock()-lastPlaneScan>.25 then
        cachedPlane,cachedWorld=locatePlane();lastPlaneScan=os.clock()
    end
    local found,world=cachedPlane,cachedWorld
    local address=found and found:GetAddress()or nil
    if address~=planeAddress then
        -- No stale actor pointers or cached loadout cross a world transition.
        if panelOpen then setPanel(false)end
        nativeHud:reset()
        nativeLoadout:reset(true,false)
        plane=found;planeAddress=address;activator=found and found.PlayerWeaponActivator or nil
        configured=false;dual=false;lastArmed=nil;nativeState=nil;errorText=nil;internal=false
        flightOptions=nil;lastUIKey=nil;lastHUD=-1
        isFlight=found~=nil;currentWorld=world;currentProfile=nil;lastSelected=-1
        checkpointPending=false;restorePending=false;resupplyPending=false
        note("WORLD_CHANGED "..world)
    end
    local selected=instance:GetSelectedPlaneID()
    if selected~=lastSelected then
        planeId=selected;lastSelected=selected;currentProfile=profileFor(planeId)
        if not isFlight then errorText=nil end
    end
    if isFlight and ready()and not configured and not errorText then
        local saved=config.profiles[tostring(planeId)]
        local options=optionsFor(planeId)
        local canUse=saved and #options==3 and options[saved.a+1]and options[saved.a+1].owned and (saved.b<0 or (options[saved.b+1]and options[saved.b+1].owned))
        if canUse then configureFlight(profileFor(planeId))
        elseif not panelOpen then requestOpen=requestOpen+1;setPanel(true)end
    end
    local now=os.clock()
    if now-lastActions>=.05 then
        lastActions=now;drain()
        local batch=readJson(root.."ui-actions.json")
        if batch then
            -- The renderer only writes nonempty batches. Remove before applying
            -- to acknowledge once, so idle ticks cannot replay an old action.
            assert(os.remove(root.."ui-actions.json"),"Cannot acknowledge UI actions")
            for _,action in ipairs(batch)do applyAction(action)end
        end
    end
    if isFlight and configured and dual and not errorText then
        if restorePending then restorePending=false;native("checkpoint_restore");lastArmed=plane:IsSpecialWeaponArmed();note("CHECKPOINT_RESTORED")end
        if resupplyPending then resupplyPending=false;native("resupply");lastArmed=plane:IsSpecialWeaponArmed();note("DUAL_RESUPPLY")end
        if checkpointPending then checkpointPending=false;native("checkpoint_save");note("CHECKPOINT_CAPTURED")end
        observeOriginalSwitch()
    end
    if now-lastHUD>=.1 then
        lastHUD=now
        if configured and valid(activator)then
            if dual and not errorText then
                -- The inactive group changes only during a native transaction.
                -- The active count has a reflected getter; a periodic native
                -- file transaction is unnecessary for an ammo-only native HUD.
                nativeState.main_ammo=activator:GetMainWeaponCount()
                nativeState.special_ammo=activator:GetSpecialWeaponCount()
                nativeState.cached_sp[active+1]=nativeState.special_ammo
                nativeState.armed=plane:IsSpecialWeaponArmed()
                nativeState.slots=nil;nativeState.reload=nil
            elseif not dual then nativeState={active=0,armed=plane:IsSpecialWeaponArmed(),main_ammo=activator:GetMainWeaponCount(),cached_sp={activator:GetSpecialWeaponCount(),0},maximum={activator.MaxSpecialWeaponCount,0},slots={{},{}}}end
        end
        if isFlight and configured and dual and valid(plane)and nativeState then
            nativeHud:update(plane,currentProfile,flightOptions,nativeState,config.hud and not errorText,now)
        end
    end
    if not isFlight then
        if config.native_loadout~=false then
            nativeLoadout:pump(instance,planeId,function()return profileFor(planeId)end,function()return optionsFor(planeId)end,now,false,panelOpen)
        elseif nativeLoadout.cache then nativeLoadout:reset()end
    end
    local uiKey=tostring(panelOpen)..":"..requestOpen..":"..requestClose..":"..planeId..":"..tostring(configured)..":"..tostring(errorText)..":"..nativeHud.status..":"..nativeLoadout.status
    if uiKey~=lastUIKey or now-lastPublish>=(panelOpen and .1 or 1)then
        lastPublish=now;lastUIKey=uiKey;publishState()
    end
end
RegisterHook("/Script/Nimbus.NimbusGameInstance:SetPlaneAndSWP",function(ctx,selectedPlane,selectedWeapon)
    nativeLoadout:beforeNativeCommit(ctx,selectedPlane,selectedWeapon)
end,function()nativeLoadout:afterNativeCommit()end)
-- These reflected entry points cover normal dynamic mission events. Their
-- checkpoint/resupply coverage remains experimental and is logged explicitly.
RegisterHook("/Script/Nimbus.Mission:OnStartNextPhaseHandler",function()end,function()
    if configured and dual then checkpointPending=true end
end)
RegisterHook("/Script/Nimbus.PlayerPlane:RestoreWeaponFromCheckpoint",function()end,function(ctx)
    if configured and dual and valid(plane)and ctx:get():GetAddress()==plane:GetAddress()then restorePending=true end
end)
RegisterHook("/Script/Nimbus.Mission:OnSuccessSupplyPhaseHandler",function()end,function()
    if configured and dual then resupplyPending=true end
end)
assert(type(LoopInGameThreadWithDelay)=="function" and type(ExecuteInGameThreadWithDelay)=="function" and type(IsInGameThread)=="function",
    "This DualSP version requires the bundled game-thread timer API")
overlay();note("DUALSP_LOADED 0.1.7 NATIVE_PREPARATION FIXED_NATIVE_SP_ROWS LOW_IDLE_IO")
-- LoopAsync + ExecuteInGameThread used to register new Lua closures from an
-- async Lua state every 20 ms while another state executed the controller.
-- The crash dump aborts in Registry::get_function_ref. Register one recurring
-- game-thread callback instead; delayed callbacks use the same scheduler.
local threadReady=false
local threadWaitReported=false
LoopInGameThreadWithDelay(20,function()
        local ok,err=xpcall(function()
            if not threadReady then
                local checked,result=pcall(IsInGameThread)
                if not checked then
                    -- The loader can schedule its first timer before the first
                    -- engine Tick has assigned a game-thread ID. Wait for that
                    -- one documented readiness condition, reject other errors.
                    if tostring(result):find("GetGameThreadId called too early",1,true)then
                        if not threadWaitReported then note("WAITING_FOR_GAME_THREAD");threadWaitReported=true end
                        return
                    end
                    error(result)
                end
                assert(result,"Controller callback is outside the game thread")
                threadReady=true
            end
            assert(IsInGameThread(),"Controller callback is outside the game thread")
            tick()
        end,debug.traceback)
        if not ok then
            errorText=tostring(err);note("ERROR "..errorText)
            -- Report once, freeze weapon mutations, and keep the UI/error visible.
            -- A user closes the panel to resume the game; no hidden loadout retry.
            local published,publishErr=pcall(publishState)
            if not published then note("UI_PUBLISH_ERROR "..tostring(publishErr))end
        end
    return false
end)
