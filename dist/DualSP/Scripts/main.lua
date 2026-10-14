-- DualSP 0.1.8：用户初步验证通过；所有调度回调仍在游戏线程运行。
-- 出击和中途补给使用各自的原生 UMG 页面，F8 只管理设置和日志。
local source=debug.getinfo(1,"S").source:gsub("^@",""):gsub("\\","/")
local root=assert(source:match("^(.*)/Scripts/[^/]+$"),"Cannot locate DualSP directory").."/"
local json=dofile(root.."Scripts/json.lua")
local weaponCycle=dofile(root.."Scripts/weapon_cycle.lua")
local log=assert(io.open(root.."mod.log","a"))
local function note(message)log:write(os.date("!%Y-%m-%dT%H:%M:%SZ ")..tostring(message).."\n");log:flush()end
local nativeHud=dofile(root.."Scripts/native_hud.lua").new(note)
-- 中央简称独立管理，右下角 MSL/单 SP/双 SP 名称由固定行模块统一处理。
local weaponPresentation=dofile(root.."Scripts/weapon_presentation.lua").new(note)
local refreshHUD=nil
local nativeLoadout=nil
-- 原生补给页与出击页分别拥有控件和草稿，不能互相提交。
local nativeSupply=nil
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
-- 候选核心直接返回就绪值，周期 HUD 不执行文件快照事务。
local readReadiness=loadExport("dualsp_hud_readiness")
local nativeTakeoff=loadExport("dualsp_resupply_takeoff") -- 候选 5：真正的原生补给 YES 入口。
local nativeSupplyControl=loadExport("dualsp_resupply_control") -- 候选 6：隔离原生补给输入入口。
local prepareWeaponRebuild=loadExport("dualsp_prepare_weapon_rebuild") -- 丢弃锁定槽前配对停止音效。
local catalogExport=loadExport("dualsp_catalogue")
local publish=loadExport("dualsp_publish","DualSPUI.dll")
local drain=loadExport("dualsp_drain_actions","DualSPUI.dll")
local overlay=loadExport("dualsp_overlay_start","DualSPUI.dll")
-- 管理版 Lua 必须搭配新版界面，防止旧 F8 再次提交兵装配置。
local managementProtocol=loadExport("dualsp_management_supported","DualSPUI.dll")
local preparationNavigation=loadExport("dualsp_preparation_navigation","DualSPUI.dll") -- 原生页面的只读键盘导航。
local config=readJson(root.."profiles.json") or {schema=1,hud=true,hud_scale=1,ignore_unlocks=false,profiles={}}
assert(config.schema==1 and type(config.profiles)=="table","Unsupported DualSP configuration schema")
-- 旧配置新增字段的默认值；原机体的配弹比例在首次保存管理设置前保留。
if config.language==nil then config.language="zh"end
if config.hud==nil then config.hud=true end
if config.ignore_unlocks==nil then config.ignore_unlocks=false end
if config.central_hint==nil then config.central_hint=true end
if config.direct_select==nil then config.direct_select=true end
if config.keyboard_keys==nil then config.keyboard_keys={49,50,51}end
config.native_loadout=nil -- 原生双选为唯一入口，不再提供旧的关闭选项。
-- 键位只接受单个普通键，F8 固定管理入口，Esc 用于取消改键。
local function supportedKey(key)
    return type(key)=="number"and key%1==0 and
        ((key>=48 and key<=57)or(key>=65 and key<=90)or(key>=96 and key<=111)
        or(key>=112 and key<=135 and key~=119)or(key>=186 and key<=192)or(key>=219 and key<=222)
        or key==8 or key==9 or key==13 or key==32 or(key>=33 and key<=40)or key==45 or key==46)
end
-- 所有管理设置先完整验证，再写盘，不允许部分保存或静默替换错误键位。
local function validateSettings(settings)
    assert(settings.language=="zh"or settings.language=="en","界面语言只能为中文或英文")
    for _,field in ipairs({"hud","central_hint","direct_select","ignore_unlocks"})do
        assert(type(settings[field])=="boolean","设置必须为布尔值："..field)
    end
    assert(type(settings.keyboard_keys)=="table"and #settings.keyboard_keys==3,"需要 MSL/SP1/SP2 三个键位")
    local used={}
    for _,key in ipairs(settings.keyboard_keys)do
        assert(supportedKey(key),"不支持此键位，F8 和 Esc 保留给管理界面")
        assert(not used[key],"三个武器选择键不能重复");used[key]=true
    end
    if settings.ammo_ratio~=nil then
        assert(type(settings.ammo_ratio)=="number"and settings.ammo_ratio>=.1 and settings.ammo_ratio<=1,"配弹比例须为 10% 至 100%")
    end
end
validateSettings(config)
-- 设置保存可传入完整新配置；只有写盘成功后才替换当前配置。
local function saveConfig(nextConfig)
    -- Keep the previous settings recoverable before replacing the small mod file.
    local old=io.open(root.."profiles.json","r")
    if old then local text=old:read("*a");old:close();local backup=assert(io.open(root.."profiles.previous.json","w"));backup:write(text);backup:close()end
    writeJson(root.."profiles.json",nextConfig or config)
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
-- 原生输入边沿和核心换槽可能分布在相邻帧；短暂锁定观察器，避免
-- 同一次切换被误判成第二次切换而在 MSL/SP 间反复触发。
local switchGuardUntil=0
local panelOpen=false
local pausedBefore=false
local gatedWidgets={}
local cursorController=nil
local previousShowCursor=nil
local requestOpen=0
local requestClose=0
local errorText=nil
local nativeState=nil
-- HUD 采样错误独立于中央文字，关闭一个功能不影响另一项。
local hudSamplingError=nil
-- 管理设置及直接选择的反馈交付 F8，设置错误不冻结任务兵装。
local settingsError=nil
local settingsAck=0
local settingsResponse=0 -- 成功或失败均回复请求编号，界面不提前宣称已保存。
local inputNotice=nil
local publishSequence=0
local checkpointPending=false
local restorePending=false
-- 补给事务跨菜单关闭保存；只有成功返回任务才应用已确认方案。
local supplySession=nil
-- 检查点配置随武器快照保存，返航重选不能改写已经捕获的旧检查点。
local checkpointProfile=nil
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
    -- 核心事务只更新数据；HUD 由选择事务结束或周期调度统一提交。
    -- snapshot/重建返回时的类别可能仍是原生切换过程中的 MSL，不能立即绘制。
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
    if saved then return {a=saved.a,b=saved.b,ratio=config.ammo_ratio or saved.ratio}end
    local selected=valid(instance) and instance:GetSelectedPlaneSWP()or 0
    return {a=math.max(0,math.min(2,selected)),b=-1,ratio=config.ammo_ratio or .5}
end
-- 复制小配置，避免补给草稿与检查点引用同一张可修改的表。
local function copyProfile(profile)return {a=profile.a,b=profile.b,ratio=profile.ratio}end
-- 补给开始先接管原生输入，不能等到页面轮询发现后才禁用；解绑不解引用旧机体。
local function ownSupplyInput(enabled)
    assert(IsInGameThread(),"补给输入所有权必须在游戏线程修改")
    if enabled then assert(valid(plane)and valid(activator),"补给输入缺少玩家机体")end
    local ok,err=nativeSupplyControl(enabled and plane:GetAddress()or 0,enabled and activator:GetAddress()or 0)
    assert(ok,err or "补给输入接口未返回成功状态")
end
nativeLoadout=dofile(root.."Scripts/native_loadout.lua").new(note,function(id,profile)
    assert(not isFlight and id==planeId,"Aircraft changed before native preparation commit")
    local options=optionsFor(id)
    assert(options[profile.a+1].owned and (profile.b<0 or options[profile.b+1].owned),"Prepared SP is not purchased")
    assert(profile.b<0 or (profile.a~=profile.b and options[profile.a+1].available and options[profile.b+1].available),"Prepared SP pair is incompatible")
    currentProfile=profile;config.profiles[tostring(id)]=profile;saveConfig()
    lastUIKey=nil;note("PROFILE_SAVED_NATIVE "..id)
end,nil,preparationNavigation)
-- 原生补给菜单确认只暂存方案；此时不改武器，也不保存到配置文件。
nativeSupply=dofile(root.."Scripts/native_loadout.lua").new(note,function(id,profile)
    assert(supplySession and id==supplySession.planeId,"补给确认期间机体已变化")
    assert(not supplySession.confirmed,"本次补给方案已确认")
    supplySession.confirmed=copyProfile(profile)
    note("RESUPPLY_PROFILE_STAGED "..id.." "..json.encode(profile))
end,"resupply",preparationNavigation,function(owner)
    assert(IsInGameThread(),"补给起飞必须在游戏线程执行")
    assert(supplySession and valid(plane)and valid(activator),"补给起飞缺少当前玩家")
    assert(owner:GetWorld():GetAddress()==plane:GetWorld():GetAddress(),"补给页面与玩家不在同一世界")
    local ok,err=nativeTakeoff(owner:GetAddress(),plane:GetAddress(),activator:GetAddress()) -- 原生负责关闭页面并通知任务。
    assert(ok,err or "原生补给 YES 接口未返回成功状态")
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

-- 就绪采样与两处提示使用同一最终选择；错误只停用新 HUD，保持武器逻辑。
refreshHUD=function(now)
    assert(IsInGameThread(),"武器 HUD 必须在游戏线程更新")
    if internal then return end
    local enabled=not errorText and not supplySession and ready()and plane:GetNimbusHUDEnabled()
    if enabled and config.hud and not hudSamplingError then
        local callOK,sampleOK,mainReady,sp1Ready,sp2Ready,err=pcall(readReadiness,
            plane:GetAddress(),activator:GetAddress(),game:GetTimeSeconds(plane),dual and 1 or 0)
        nativeState.ready=nil
        if not callOK or sampleOK~=true then
            hudSamplingError=tostring(callOK and (err or "候选核心的 HUD 内存接口不可用")or sampleOK)
            note("NATIVE_HUD_SAMPLE_ERROR "..hudSamplingError)
        else
            nativeState.ready={mainReady,sp1Ready,sp2Ready}
        end
    end
    nativeState.armed=plane:IsSpecialWeaponArmed()
    -- 单 SP 出击也处理名称就绪状态，但保持原生面板高度。
    nativeHud:update(plane,currentProfile,flightOptions,nativeState,enabled and config.hud and not hudSamplingError,now)
    weaponPresentation:update(plane,currentProfile,flightOptions,nativeState,enabled and config.central_hint,now)
end
-- 由出击初始化和飞行切换共同使用的输入释放接口，在 configureFlight
-- 定义前先声明，避免 Lua 将其解析成全局变量。
local releaseWeaponInput
-- 在原生武器系统重建前先接管 HUD。否则 InitializeWeaponSystem 会先让
-- 游戏绘制一行原生 SP，下一次 Lua tick 才能插入第二行，双 SP 出击时会
-- 出现肉眼可见的 MSL+单 SP 短暂画面。预热只建立控件和最终文本，不写武器
-- 内存；真实就绪状态在核心初始化后由 refreshHUD 立即替换。
local function primeFlightHUD(profile,options)
    local seed={active=0,armed=false,main_ammo=activator:GetMainWeaponCount(),
        cached_sp={activator:GetSpecialWeaponCount(),0},maximum={activator.MaxSpecialWeaponCount,0},
        slots={{},{}} ,ready={false,false,false}}
    local visible=plane:GetNimbusHUDEnabled()
    nativeHud:update(plane,profile,options,seed,visible and config.hud,os.clock())
    weaponPresentation:update(plane,profile,options,seed,visible and config.central_hint,os.clock())
end
-- 正常出击建立检查点；补给重建只替换当前兵装，保留任务检查点。
local function configureFlight(profile,resupply)
    assert(ready(),"Wait until the mission's flight controls are active")
    assert(not configured or resupply,"Loadout is locked for this sortie")
    -- 在触碰武器前检查候选协议；正式 0.1.7 核心不能冒充支持补给重选。
    if resupply and profile.b>=0 then
        local supported=package.loadlib(root.."bin/DualSP.dll","dualsp_resupply_supported")
        assert(supported,"补给重选需要支持 resupply_setup 的独立候选核心，当前正式 DLL 不支持")
    end
    local options=optionsFor(planeId)
    assert(#options==3 and options[profile.a+1].owned and (profile.b<0 or options[profile.b+1].owned),"Saved loadout includes an unpurchased SP")
    assert(profile.b<0 or (profile.a~=profile.b and options[profile.a+1].available and options[profile.b+1].available),"双 SP 组合不受支持")
    currentProfile=profile
    flightOptions=options
    dual=profile.b>=0
    primeFlightHUD(profile,options)
    -- 仅在出击或已成功的补给事务内重建；正常切换仍由核心保存消耗与冷却。
    local audioReady,audioError=prepareWeaponRebuild(plane:GetAddress(),activator:GetAddress()) -- 保留旧槽直到停止事件完成。
    assert(audioReady,audioError or "原生锁定音收尾未完成")
    plane:InitializeWeaponSystem(profile.a)
    -- 释放发射按键只负责输入收尾；锁定参数由完整原生初始化/切换流程更新。
    releaseWeaponInput(resupply and "resupply_loadout"or "mission_loadout")
    if dual then
        native(resupply and "resupply_setup"or "setup",0)
        if not resupply then native("checkpoint_save",0)end
    else
        nativeState={active=0,armed=false,main_ammo=activator:GetMainWeaponCount(),cached_sp={activator:GetSpecialWeaponCount(),0},maximum={activator.MaxSpecialWeaponCount,0},slots={{},{}}}
    end
    configured=true;active=0;lastArmed=plane:IsSpecialWeaponArmed();note("LOADOUT "..planeId.." "..json.encode(profile))
    if not resupply then checkpointProfile=copyProfile(profile)end
    -- 核心初始化后立即提交一次真实状态，避免等待 100 ms HUD 周期。
    refreshHUD(os.clock())
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
-- 重建前释放发射输入；该接口不负责更新 seeker，也不能作为停止锁定音的修复。
releaseWeaponInput=function(reason)
    assert(valid(activator),"Player weapon activator is unavailable")
    local ok,err=pcall(function()activator:ReleaseWeaponInput()end)
    assert(ok,err)
    note("WEAPON_INPUT_RELEASED "..reason)
end
-- 原生按下/释放属于同一切换事务，完成后再向两处 HUD 提交选择。
local function pressSwitch()
    internal=true
    local switchedAddress=plane:GetAddress()
    local desiredArmed=not plane:IsSpecialWeaponArmed() -- 原生类别变化后必须与目标一致。
    -- 完整飞机入口还会更新锁定距离、seeker 与部件效果，不能换成 ToggleSpecial。
    plane:OnWeaponSwitchPressed()
    delay(80,function()
        if not valid(plane)or plane:GetAddress()~=switchedAddress then internal=false;note("SWITCH_CANCELLED_WORLD_CHANGE");return end
        plane:OnWeaponSwitchReleased()
        lastArmed=plane:IsSpecialWeaponArmed()
        assert(lastArmed==desiredArmed,"原生武器类别切换未完成")
        switchGuardUntil=os.clock()+.15;internal=false
        note("WEAPON_CATEGORY_COMMITTED "..tostring(lastArmed))
        if configured and flightOptions and not supplySession then refreshHUD(os.clock())end
    end)
end
-- MSL/SP 类别转换与 SP 重建共同完成后才刷新，避免暴露中间的 MSL 状态。
local function selectLogicalWeapon(desired)
    assert(desired>=0 and desired<=2,"无效的逻辑武器选择")
    internal=true
    switchGuardUntil=os.clock()+.15
    if desired>0 and active~=desired-1 then
        -- 核心恢复 SP 时也必须执行完整飞机切换，保持武器与锁定组件一致。
        releaseWeaponInput("special_slot_switch")
        native("switch",desired-1)
    end
    local armed=plane:IsSpecialWeaponArmed()
    if armed~=(desired>0)then
        pressSwitch()
    else
        lastArmed=armed;internal=false
        refreshHUD(os.clock())
    end
end
-- 直接选择只在允许武器操作的任务中执行；已选中目标时不重建。
local function directSelect(desired)
    assert(type(desired)=="number"and desired%1==0 and desired>=0 and desired<=2,"直接选择目标无效")
    if not config.direct_select or not configured or supplySession or panelOpen or internal or errorText or restorePending or checkpointPending
        or not isFlight or not ready()or game:IsGamePaused(plane)then return end
    inputNotice=nil
    if desired==2 and not dual then inputNotice="empty_sp2";note("DIRECT_SELECTION_REJECTED EMPTY_SP2");return end
    local armed=plane:IsSpecialWeaponArmed()
    if (armed and active+1 or 0)==desired then
        return
    end
    if desired>0 then
        -- 活动 SP 读取真实弹药，未活动 SP 使用切换时保存的核心计数。
        local ammo=desired==active+1 and activator:GetSpecialWeaponCount()or nativeState.cached_sp[desired]
        if ammo<=0 then inputNotice="empty_ammo";note("DIRECT_SELECTION_REJECTED EMPTY_AMMO "..desired);return end
    end
    note("DIRECT_SELECTION_REQUESTED "..desired)
    selectLogicalWeapon(desired)
end
local function observeOriginalSwitch()
    if not dual or not configured or supplySession or internal or panelOpen or errorText or not ready()or game:IsGamePaused(plane)then return end
    if os.clock()<switchGuardUntil then return end
    local armed=plane:IsSpecialWeaponArmed()
    -- The live SP count is authoritative; inactive SP counts are captured on
    -- each swap. A consumed last round must not wait for the 100 ms HUD update.
    local ammo={nativeState.cached_sp[1],nativeState.cached_sp[2]}
    ammo[active+1]=activator:GetSpecialWeaponCount()
    if lastArmed~=nil and armed~=lastArmed then
        local desired=weaponCycle.next(lastArmed and active+1 or 0,ammo[1],ammo[2])
        selectLogicalWeapon(desired)
    else
        -- While MSL is armed, install the first nonempty SP in advance. This
        -- lets the game's original switch key work even if its prior SP is empty
        -- and no armed-category transition would otherwise occur.
        if not armed then
            local desired=weaponCycle.next(0,ammo[1],ammo[2])
            if desired>0 and active~=desired-1 then
                releaseWeaponInput("preload_special_slot")
                native("switch",desired-1)
                switchGuardUntil=os.clock()+.15
            end
        end
        lastArmed=armed
    end
end
local function applyAction(action)
    if action.type=="open"then setPanel(true)
    elseif action.type=="close"then setPanel(false)
    elseif action.type=="select_weapon"then directSelect(action.target)
    elseif action.type=="settings"then
        -- 管理界面没有选兵装的权限；构造完整新设置后验证并保存。
        local ok,err=xpcall(function()
            assert(type(action.request)=="number"and action.request%1==0 and action.request>0,"管理保存请求编号无效")
            local nextConfig=json.decode(json.encode(config))
            for _,field in ipairs({"language","hud","central_hint","direct_select","ignore_unlocks","keyboard_keys","ammo_ratio"})do
                assert(action[field]~=nil,"管理设置缺少字段："..field);nextConfig[field]=action[field]
            end
            validateSettings(nextConfig);saveConfig(nextConfig);config=nextConfig
            settingsAck=action.request;settingsError=nil;inputNotice=nil
            lastUIKey=nil;note("MANAGEMENT_SETTINGS_SAVED "..settingsAck)
        end,debug.traceback)
        settingsResponse=action.request
        if not ok then settingsError=tostring(err);lastUIKey=nil;note("MANAGEMENT_SETTINGS_ERROR "..settingsError)end
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
    local info=catalogue and catalogue[tostring(planeId)]
    -- 仅发布管理参数和只读状态，不给界面提供兵装编辑事务。
    local state={schema=1,scene=isFlight and "flight"or"menu",plane_id=planeId,plane_name=info and (names[info.code]or info.code:upper())or"请选择机体",
        configured=configured,state=nativeState,hud=config.hud,ignore_unlocks=config.ignore_unlocks,request_open=requestOpen,request_close=requestClose,
        management_protocol=1,language=config.language,central_hint=config.central_hint,direct_select=config.direct_select,
        keyboard_keys=config.keyboard_keys,ammo_ratio=config.ammo_ratio or (currentProfile and currentProfile.ratio)or .5,
        settings_ack=settingsAck,settings_response=settingsResponse,settings_error=settingsError,input_notice=inputNotice}
    state.direct_selection_available=isFlight and configured and not supplySession and not panelOpen and not internal and not errorText
        and not restorePending and not checkpointPending
        and ready()and not game:IsGamePaused(plane)
    state.sequence=publishSequence
    state.native_hud=nativeHud.status
    state.weapon_presentation=weaponPresentation.status
    state.native_loadout=nativeLoadout.status
    state.native_resupply=nativeSupply.status
    if nativeLoadout.error then state.menu_error=nativeLoadout.error end
    if nativeSupply.error then state.menu_error=nativeSupply.error end
    if nativeHud.error or hudSamplingError then state.hud_error=nativeHud.error or hudSamplingError end
    if weaponPresentation.error then state.central_error=weaponPresentation.error end
    if errorText then state.error=errorText;state.state=nil end
    writeJson(root.."ui-state.pending.json",state);publish()
end
local function tick()
    if not valid(instance)then instance=FindFirstOf("NimbusGameInstance")end
    if not valid(instance)then return end
    -- 返回主菜单明确结束本轮补给，防止旧事务阻止下一次任务配置。
    if supplySession then
        local worldContext=instance:GetWorld()
        if valid(worldContext)and worldContext:GetFullName():find("/Game/Maps/Menu/",1,true)then
            nativeSupply:reset(true,false);ownSupplyInput(false);supplySession=nil;checkpointProfile=nil
            note("RESUPPLY_CANCELLED_LEAVE_MISSION")
        end
    end
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
        weaponPresentation:reset()
        nativeLoadout:reset(true,false)
        plane=found;planeAddress=address;activator=found and found.PlayerWeaponActivator or nil
        configured=false;dual=false;lastArmed=nil;nativeState=nil;internal=false;hudSamplingError=nil;inputNotice=nil
        if not supplySession then errorText=nil end
        flightOptions=nil;lastUIKey=nil;lastHUD=-1
        isFlight=found~=nil;currentWorld=world;currentProfile=nil;lastSelected=-1
        -- 补给可能暂时移走玩家机体；保留确认事务和检查点，等待原生成功事件。
        if not supplySession then checkpointPending=false;restorePending=false;checkpointProfile=nil end
        note("WORLD_CHANGED "..world)
    end
    local selected=instance:GetSelectedPlaneID()
    if selected~=lastSelected then
        planeId=selected;lastSelected=selected;currentProfile=profileFor(planeId)
        if not isFlight and not supplySession then errorText=nil end
    end
    -- 成功事件只排队：原生阶段先完成自己的收尾，随后才释放补给控件并重建武器。
    if supplySession and supplySession.success and isFlight and ready()and not errorText then
        assert(not nativeSupply.error,nativeSupply.error)
        assert(planeId==supplySession.planeId and currentWorld==supplySession.world,"补给返回了不同机体或任务世界")
        -- 事务在 OnFinishdSupplyMenuHandler 后仍保留；此处收到成功事件后才提交并清理页面。
        local profile=nativeSupply:completeSupply()
        assert(supplySession.confirmed,"原生补给成功，但未收到双 SP 的起飞确认")
        nativeHud:reset()
        weaponPresentation:reset()
        -- 原生单 SP 选择始终与新方案 SP1 对齐；完整补给由初始化和核心重建完成。
        instance:SetPlaneAndSWP(planeId,profile.a)
        assert(instance:GetSelectedPlaneSWP()==profile.a,"补给 SP1 原生选择未生效")
        configureFlight(profile,true)
        config.profiles[tostring(planeId)]=copyProfile(profile);saveConfig()
        nativeSupply:reset(false,false);ownSupplyInput(false);supplySession=nil;lastUIKey=nil
        note("RESUPPLY_LOADOUT_APPLIED "..planeId.." "..json.encode(profile))
    end
    if isFlight and ready()and not configured and not supplySession and not errorText then
        local saved=config.profiles[tostring(planeId)]
        local options=optionsFor(planeId)
        local canUse=saved and #options==3 and options[saved.a+1]and options[saved.a+1].owned and (saved.b<0 or (options[saved.b+1]and options[saved.b+1].owned))
        assert(canUse,"没有已确认且可用的原生双 SP 配置，请返回机库通过原生兵装页确认后出击；F8 不提供兵装选择")
        configureFlight(profileFor(planeId))
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
    if isFlight and configured and not supplySession and not errorText then
        if restorePending then
            restorePending=false;assert(checkpointProfile,"检查点缺少对应的双 SP 配置")
            currentProfile=copyProfile(checkpointProfile);dual=currentProfile.b>=0
            nativeHud:reset()
            weaponPresentation:reset()
            if dual then native("checkpoint_restore")else
                -- 原生恢复已完成单 SP 的弹药恢复，不再次初始化导致补满。
                active=0;nativeState={active=0,armed=plane:IsSpecialWeaponArmed(),main_ammo=activator:GetMainWeaponCount(),
                    cached_sp={activator:GetSpecialWeaponCount(),0},maximum={activator.MaxSpecialWeaponCount,0}}
            end
            instance:SetPlaneAndSWP(planeId,currentProfile.a)
            config.profiles[tostring(planeId)]=copyProfile(currentProfile);saveConfig()
            lastArmed=plane:IsSpecialWeaponArmed();note("CHECKPOINT_RESTORED")
        end
        if checkpointPending then
            checkpointPending=false
            if dual then native("checkpoint_save")end
            checkpointProfile=copyProfile(currentProfile);note("CHECKPOINT_CAPTURED")
        end
        if dual then observeOriginalSwitch()end
    end
    if now-lastHUD>=.1 then
        lastHUD=now
        if configured and not supplySession and valid(activator)then
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
        if isFlight and configured and not supplySession and valid(plane)and nativeState and flightOptions then
            refreshHUD(now)
        end
    end
    if supplySession and supplySession.menuActive and not supplySession.menuClosed and not errorText then
        -- 只在实际补给菜单打开期间扫描控件，不在正常飞行中扫描补给页。
        nativeSupply:pump(instance,supplySession.planeId,function()
                -- 新参数只用于新开的原生选择草稿，不改返航前方案及取消恢复。
                local profile=copyProfile(supplySession.profile)
                profile.ratio=config.ammo_ratio or profile.ratio;return profile
            end,
            function()return supplySession.options end,now,isFlight,panelOpen)
        if nativeSupply.error then error(nativeSupply.error)end
    elseif not isFlight and not supplySession then
        nativeLoadout:pump(instance,planeId,function()return profileFor(planeId)end,function()return optionsFor(planeId)end,now,false,panelOpen)
    end
    local uiKey=tostring(panelOpen)..":"..requestOpen..":"..requestClose..":"..planeId..":"..tostring(configured)..":"..tostring(errorText)..":"..nativeHud.status..":"..weaponPresentation.status..":"..nativeLoadout.status..":"..nativeSupply.status..":"..tostring(internal)..":"..tostring(inputNotice)
    if uiKey~=lastUIKey or now-lastPublish>=(panelOpen and .1 or 1)then
        lastPublish=now;lastUIKey=uiKey;publishState()
    end
end
RegisterHook("/Script/Nimbus.NimbusGameInstance:SetPlaneAndSWP",function(ctx,selectedPlane,selectedWeapon)
    nativeLoadout:beforeNativeCommit(ctx,selectedPlane,selectedWeapon)
    nativeSupply:beforeNativeCommit(ctx,selectedPlane,selectedWeapon)
end,function()end)
-- 原生页面可能复用同一对象；真实输入激活事件作为新一轮准备的边界。
RegisterHook("/Script/Nimbus.MenuBaseWidget:SetIsEnableControl",function()end,function(ctx,enabled)
    nativeLoadout:onControlChanged(ctx,enabled)
end)
-- These reflected entry points cover normal dynamic mission events. Their
-- checkpoint/resupply coverage remains experimental and is logged explicitly.
RegisterHook("/Script/Nimbus.Mission:OnStartNextPhaseHandler",function()end,function()
    if configured then checkpointPending=true end
end)
RegisterHook("/Script/Nimbus.PlayerPlane:RestoreWeaponFromCheckpoint",function()end,function(ctx)
    if configured and valid(plane)and ctx:get():GetAddress()==plane:GetAddress()then
        -- 检查点重试中止尚未完成的返航草稿，恢复已捕获方案。
        if supplySession then nativeSupply:reset(false,false);ownSupplyInput(false);supplySession=nil;note("RESUPPLY_CANCELLED_CHECKPOINT")end
        restorePending=true
    end
end)
-- 生命周期失败明确停用武器修改；不能忽略异常后继续应用旧方案。
local function supplyHook(callback)
    return function(...)
        if errorText then return end
        local args={...}
        local ok,err=xpcall(function()
            assert(IsInGameThread(),"补给事件不在游戏线程")
            callback(table.unpack(args))
        end,debug.traceback)
        if not ok then errorText=tostring(err);note("NATIVE_RESUPPLY_ERROR "..errorText)end
    end
end
RegisterHook("/Script/Nimbus.Mission:OnStartSupplyPhaseHandler",function()end,supplyHook(function(ctx)
    assert(configured and currentProfile,"补给开始时模组尚未完成出击配置")
    assert(not supplySession,"上一轮补给事务尚未结束")
    ownSupplyInput(true) -- 页面生成前已隔离两个原生输入入口，避免第一帧穿透。
    -- 任务和原机体绑定用于排除其他任务、菜单或机体触发的事件。
    supplySession={mission=ctx:get():GetAddress(),world=currentWorld,planeId=planeId,profile=copyProfile(currentProfile),
        options=optionsFor(planeId),menuActive=false,menuClosed=false,confirmed=nil,success=false}
    nativeHud:reset();weaponPresentation:reset();note("RESUPPLY_STARTED "..planeId)
end))
RegisterHook("/Script/Nimbus.MiniGamePhase:OnStartSupplyMenuHandler",function()end,supplyHook(function(ctx)
    assert(supplySession,"补给菜单打开前未收到补给开始事件")
    assert(not supplySession.menuActive,"本轮补给菜单重复开始")
    assert(not supplySession.confirmed,"已经确认的补给菜单被重新打开")
    supplySession.menuPhase=ctx:get():GetAddress();supplySession.menuActive=true;supplySession.menuClosed=false
    nativeSupply:beginSupplyMenu(ctx:get());note("RESUPPLY_MENU_OPENED")
end))
RegisterHook("/Script/Nimbus.MiniGamePhase:OnFinishdSupplyMenuHandler",supplyHook(function(ctx)
    -- 这里只观察已发出的 YES；异步关闭期间打开 F8 不应使合法结束通知失败。
    assert(supplySession and supplySession.menuActive,"补给结束阶段没有活动菜单")
    -- 严格匹配菜单开始时的阶段，不因关卡世界不同而重绑对象或误判跨任务。
    assert(supplySession.menuPhase==ctx:get():GetAddress(),"补给结束事件与菜单开始阶段不匹配")
    nativeSupply:beforeSupplyFinish(ctx)
end),supplyHook(function(ctx)
    nativeSupply:afterSupplyFinish(ctx)
end))
RegisterHook("/Script/Nimbus.MiniGamePhase:OnCloseSupplyMenu",function()end,supplyHook(function(ctx)
    if supplySession and supplySession.menuPhase==ctx:get():GetAddress()then
        -- 关闭不能单独提交草稿；结束回调可能尚未返回，保留控件事务。
        assert(nativeSupply:isSupplyHandoffPending(),"原生补给菜单在 YES 请求前关闭")
        supplySession.menuClosed=true;note("RESUPPLY_MENU_CLOSED")
    end
end))
RegisterHook("/Script/Nimbus.Mission:OnSuccessSupplyPhaseHandler",function()end,supplyHook(function(ctx)
    assert(supplySession and supplySession.mission==ctx:get():GetAddress(),"补给成功事件与当前任务不匹配")
    assert(not supplySession.success,"同一轮补给重复成功")
    supplySession.success=true;note("RESUPPLY_SUCCESS_PENDING")
end))
assert(type(LoopInGameThreadWithDelay)=="function" and type(ExecuteInGameThreadWithDelay)=="function" and type(IsInGameThread)=="function",
    "This DualSP version requires the bundled game-thread timer API")
overlay();note("DUALSP_LOADED 0.1.8 MANAGEMENT_UI")
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
