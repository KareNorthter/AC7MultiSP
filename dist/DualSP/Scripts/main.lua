-- AC7MultiSP 2.23.0正式版：内存交接、装填队列修正及管理页调整；补给保持已购筛选。
-- 出击和中途补给使用原生 UMG 页面，管理快捷键默认 F6，可配置以避开其他模组。
local source=debug.getinfo(1,"S").source:gsub("^@",""):gsub("\\","/")
local root=assert(source:match("^(.*)/Scripts/[^/]+$"),"Cannot locate DualSP directory").."/"
-- 与页面字段一样，反射查询的短字符串须由回调持续持有，不能临时拼接后交给加载器缓存。
local weaponFieldNames={config={"SpecialWeaponConfig1","SpecialWeaponConfig2","SpecialWeaponConfig3"},pool={"SWP1","SWP2","SWP3"}}
local json=dofile(root.."Scripts/json.lua")
local ConfigStore=dofile(root.."Scripts/config_store.lua")
local Profile=dofile(root.."Scripts/loadout_profile.lua") -- 三槽结构、购买校验和百分比配弹。
local weaponCycle=dofile(root.."Scripts/weapon_cycle.lua")
local runtimeContext=dofile(root.."Scripts/runtime_context.lua") -- 模式与游戏文本语言共用只读来源。
local tr=runtimeContext.text -- 原生页面文字跟随游戏，不跟随 F8 手动语言。
local runtimeAllowed=false -- 未确认单人状态前，所有模组功能均禁止。
local runtimeReason="initializing" -- 禁用原因写日志，不在多人界面绘制提示。
local runtimeChanging=false -- 模式清理期间阻止反射钩子重入。
local runtimeFailed=false -- 模式识别出错后明确停用到重启，不不断重试或沿用旧许可。
local syncRuntime=nil -- 完整状态清理函数在各模块定义之后建立。
local note
do -- 日志私有状态限于闭包，避免三线程整合后超出Lua入口局部变量上限。
    local nativeNote=assert(package.loadlib(root.."bin/DualSP.dll","dualsp_log"))
    local logUnavailable=false
    note=function(message)
        if nativeNote(tostring(message))==false and not logUnavailable then
            logUnavailable=true;print("[AC7MultiSP] Background logging unavailable; mission control remains active.")
        end
    end
end
local nativeHud=nil -- 核心剪影刷新接口加载后再建立 HUD 模块。
-- 中央简称独立管理，右下角 MSL/单 SP/多 SP 名称由固定行模块统一处理。
local weaponPresentation=dofile(root.."Scripts/weapon_presentation.lua").new(note)
local refreshHUD=nil
local nativeLoadout=nil
-- 原生补给页与出击页分别拥有控件和草稿，不能互相提交。
local nativeSupply=nil
local nativeStandby=nil -- STANDBY 独立展示已保存方案，不改变原生机库导航。
local nativeParts=nil -- PART 页只读展示已保存的多 SP 和原生悬停载量，不提交候选部件。
local function readJson(path)
    local file=io.open(path,"r");if not file then return nil end
    local text=file:read("*a");file:close();return json.decode(text)
end
local function loadExport(name,dll)
    local fn,err=package.loadlib(root.."bin/"..(dll or "DualSP.dll"),name);assert(fn,err);return fn
end
-- The validated weapon core and the rebuilt menu renderer are separate modules.
local bridge=loadExport("dualsp_execute_memory_v1")
loadExport("dualsp_cfa_startup_prewarm_supported") -- 预热、Lua开场和提示界面须成套。
local mountStartup={serial=0,invoke=loadExport("dualsp_mount_loading","DualSPUI.dll")}
loadExport("dualsp_three_sp_supported") -- 在读取或迁移配置前拒绝旧核心。
loadExport("dualsp_ammo_ratio_supported") -- 比例命令必须与新核心成套部署。
loadExport("dualsp_infinite_ammo_supported") -- 负弹药数按无限处理，拒绝旧预览/核心混装。
loadExport("dualsp_management_options_supported","DualSPUI.dll")
loadExport("dualsp_async_logging_supported","DualSPUI.dll")
loadExport("dualsp_flat_management_supported","DualSPUI.dll")
local setFixedMountEnabled=loadExport("dualsp_set_fixed_mount_enabled")
local registerMountAsset=loadExport("dualsp_mount_asset")
local expandedMountAssets=dofile(root.."Scripts/mount_assets.lua")
local lightMountAircraft={f22a=true,pkfa=true,dark=true,zoef=true,adf11f=true,fa44=true}
local ensureMountAssets=nil
local nativeMountPreview=loadExport("dualsp_mount_preview") -- 独立视觉上下文，不参与兵装事务。
loadExport("dualsp_three_sp_supported","DualSPUI.dll") -- 四键管理协议必须成套安装。
local nativeRuntimeGate=loadExport("dualsp_runtime_gate") -- 原生核心独立检查功能许可。
local uiRuntimeGate=loadExport("dualsp_runtime_gate","DualSPUI.dll") -- F8、窗口消息与手柄同时禁用。
-- 候选核心直接返回就绪值，周期 HUD 不执行文件快照事务。
local readReadiness=loadExport("dualsp_hud_readiness")
local refreshNativeIcons=loadExport("dualsp_refresh_weapon_icons") -- 最终兵装提交后刷新原生剪影和显隐。
nativeHud=dofile(root.."Scripts/native_hud.lua").new(note,refreshNativeIcons)
local nativeTakeoff=loadExport("dualsp_resupply_takeoff") -- 候选 5：真正的原生补给 YES 入口。
local nativeSupplyControl=loadExport("dualsp_resupply_control") -- 候选 6：隔离原生补给输入入口。
local nativePreparationAction=loadExport("dualsp_preparation_action") -- 体验候选：原生机库一步确认/返回。
local nativeSelectionAudio=loadExport("dualsp_selection_audio") -- 最终逻辑选择统一提交 A/B/C 音效。
local nativeCustomAudioUpdate=loadExport("dualsp_custom_audio_update") -- 自定义 WAV 跟随游戏音量与暂停。
local updateFixedMount=loadExport("dualsp_fixed_mount_update") -- 独立 Su-30SM 原型必须与候选核心成套部署。
note("MANAGEMENT_OPTIONS_DEVELOPMENT base=2.0.0 core_memory_protocol=1 management_protocol=21") -- 新源码须与新核心及 UI 成套构建。
local takeEmptySwitch=loadExport("dualsp_take_empty_switch") -- 按下原生切换键才处理耗尽挂载，不预先换模型。
loadExport("dualsp_raven_sp_supported") -- UAV方案须与新核心成套部署。
loadExport("dualsp_mod_electronic_supported") -- 通用电子战资格和核心保有流程须成套，拒绝只替换Lua。
loadExport("dualsp_mod_uav_supported") -- UAV/HPM按真实类型与原生契约；选装新增第五返回值。
local continuousSPOption=loadExport("dualsp_continuous_sp_option") -- 按真实类检查电子战/UAV，TLS保留原范围。
local finishTLS=loadExport("dualsp_finish_tls") -- 在缓存冷却前核对持续策略，已登记原生TLS交给保有事务。
local nativeMenuConfirm=loadExport("dualsp_menu_confirm") -- 双选内部步骤复用当前页面的原生确认音。
local nativeLoadoutAmmo=loadExport("dualsp_loadout_ammo") -- 三页统一读取部件加成后的完整载量。
local nativePartsAmmo=loadExport("dualsp_parts_ammo_preview") -- PART 只读候选快照；旧核心缺少接口时明确拒绝混装。
local nativeWeaponIcon=loadExport("dualsp_weapon_icon") -- 机库全名前复用原生分类图标。
local prepareWeaponRebuild=loadExport("dualsp_prepare_weapon_rebuild") -- 丢弃锁定槽前解除旧目标登记及音效。
local weaponSwitch={read=loadExport("dualsp_switch_ready"),takeCycle=loadExport("dualsp_take_cycle_switch"),
    press=loadExport("dualsp_controlled_weapon_switch"),prepareCfa=loadExport("dualsp_prepare_cfa_switch")} -- 新Lua与核心须成套。
weaponSwitch.gaze={invoke=loadExport("dualsp_target_gaze"),active=false,available=false,generation=0,response=0}
loadExport("dualsp_gaze_inputs_supported")
loadExport("dualsp_gaze_inputs_supported","DualSPUI.dll") -- 防止旧管理页丢失新设置。
weaponSwitch.camera={invoke=loadExport("dualsp_camera_hold"),available=false,released=true}
weaponSwitch.view={invoke=loadExport("dualsp_view_target"),marker=loadExport("dualsp_view_center_marker","DualSPUI.dll"),available=false,released=true}
loadExport("dualsp_view_target_modes_supported")
loadExport("dualsp_view_target_modes_supported","DualSPUI.dll") -- 双模式设置与四参数/四返回值白点接口须成套。
loadExport("dualsp_camera_hold_supported","DualSPUI.dll")
loadExport("dualsp_camera_controls_v2_supported")
loadExport("dualsp_camera_controls_v2_supported","DualSPUI.dll") -- 七参数/四返回值镜头接口，L3回正/R3短按视角/长按保持须成套。
loadExport("dualsp_camera_controls_supported")
loadExport("dualsp_camera_controls_supported","DualSPUI.dll") -- 新回正键/灵敏度要求核心及管理页一起升级。
loadExport("dualsp_camera_input_options_supported")
loadExport("dualsp_camera_input_options_supported","DualSPUI.dll")
loadExport("dualsp_keyboard_modifier_routing_supported")
loadExport("dualsp_keyboard_modifier_routing_supported","DualSPUI.dll") -- 修饰键分流与回中接口须成套。
loadExport("dualsp_controller_clicks_supported")
loadExport("dualsp_controller_clicks_supported","DualSPUI.dll")
loadExport("dualsp_central_weapon_display_supported","DualSPUI.dll") -- 旧界面不能保存新增的中央简称显示方式。
loadExport("dualsp_gun_salvo_supported")
loadExport("dualsp_gun_salvo_supported","DualSPUI.dll")
weaponSwitch.salvo={invoke=loadExport("dualsp_gun_salvo")}
local catalogExport=loadExport("dualsp_catalogue_memory_v1")
loadExport("dualsp_mod_aircraft_catalogue_supported") -- 新Lua依赖真实机体/涂装目录，拒绝旧核心混装。
local modAircraft=dofile(root.."Scripts/mod_aircraft.lua").new(note)
local publish=loadExport("dualsp_publish_memory_v1","DualSPUI.dll")
local drain=loadExport("dualsp_drain_actions_memory_v1","DualSPUI.dll")
local overlay=loadExport("dualsp_overlay_start","DualSPUI.dll")
-- 管理版 Lua 必须搭配新版界面，防止旧 F8 再次提交兵装配置。
local managementProtocol=loadExport("dualsp_management_supported","DualSPUI.dll")
local shortcutProtocol=loadExport("dualsp_shortcuts_supported","DualSPUI.dll") -- 可配置组合键须搭配新UI。
local debugLoadoutProtocol=loadExport("dualsp_debug_loadout_supported","DualSPUI.dll") -- 调试扩展须与界面成套安装。
loadExport("dualsp_debug_aircraft_supported","DualSPUI.dll") -- 换机独立子协议，不改变现有管理设置。
local debugLoadoutReady=loadExport("dualsp_debug_loadout_ready") -- 重建前只读检查正在飞行的武器。
local debugFixedMount=loadExport("dualsp_debug_fixed_mount")
local hotkeyResponse=0
local hotkeyNotice=nil
local preparationNavigation=loadExport("dualsp_preparation_navigation","DualSPUI.dll") -- 原生页面的只读键盘导航。
local configStore=ConfigStore.new{
    root=root,note=note,json=json,profile=Profile,readJson=readJson}
local config=configStore:load()
-- 总开关在明确的安全菜单切换；任务内保存先等待，不因切换设置而补弹。
local runtimeModEnabled=config.mod_enabled
local masterSwitch={lastCheck=-1,failed=false,error=nil,response=0,notice=nil}
local tryApplyMasterSwitch=nil -- 页面和原生接口都建立后，再定义安全菜单中的完整清理/启用。
local runtimeFixedMountEnabled=config.task_mount_enabled -- 核心共用入口当前实际使用的场景开关，不再作为模型总开关。
local mountOptionsApplied=false
local mountWarning=nil
local previewMountWarning=nil -- 预览警告独立保存，任务周期刷新不能覆盖。
local function applyFixedMountSetting(enabled)
    if enabled==nil then enabled=config.task_mount_enabled end -- 任务/补给重建默认用任务开关；机库调用方显式传机库开关。
    if mountOptionsApplied and runtimeFixedMountEnabled==enabled then return end
    local ok,err=setFixedMountEnabled(runtimeModEnabled and enabled and 1 or 0)
    assert(ok,err)
    runtimeFixedMountEnabled=enabled
    mountOptionsApplied=true
    mountWarning=err and err~=""and err or nil
    note("FIXED_MOUNT_SETTING_APPLIED "..tostring(runtimeFixedMountEnabled))
end
local function ammoSettings()
    return {ammo_percent_two=config.ammo_percent_two,ammo_percent_three=config.ammo_percent_three}
end
local function saveConfig(nextConfig)configStore:save(nextConfig or config)end
local catalogue=nil
local game=nil
local plane=nil
local activator=nil
local planeAddress=nil
local instance=nil
local planeId=0
local currentProfile=nil
local configured=false
local lastArmed=nil
local active=0
local internal=false
local panelOpen=false
local managementPause=nil -- 只保存当前面板实际取得的暂停，不把临时的 IsGamePaused 状态当作恢复目标。
local managementPauseError=nil -- 暂停交接失败单独展示；禁止继续调试换装，允许用户明确重试关闭。
local gatedWidgets={}
local cursorController=nil
local previousShowCursor=nil
local requestOpen=0
local requestClose=0
local errorText=nil
local nativeState=nil
-- HUD 采样错误由两处提示分别展示，不改变兵装状态。
local hudSamplingError=nil
-- 管理设置及直接选择的反馈交付 F8，设置错误不冻结任务兵装。
local settingsError=nil
local settingsAck=0
local settingsResponse=0 -- 成功或失败均回复请求编号，界面不提前宣称已保存。
local debugSession=0 -- 玩家实例换代后递增，拒绝上一任务排队的调试请求。
local debugResponse=0 -- 已处理请求编号，包含正常拒绝。
local debugAck=0 -- 仅完成核心与 HUD 交接后确认成功。
local debugError=nil -- 调试请求错误单独展示，不把输入拒绝当成控制器故障。
local debugWeapons={infinite=false,fast=false,response=0,ack=0,invoke=loadExport("dualsp_debug_weapons")}
local debugProfileActive=false -- 调试方案只属于本次任务，检查点恢复也不能写入个人配置。
local aircraftDebug=nil -- 换机事务独立于兵装，允许跨整关载入等待新玩家实例。
local checkpointDebugProfile=false -- 真实检查点是否捕获了调试方案。
local inputNotice=nil
local publishSequence=0
local checkpointPending=false
local restorePending=false
-- 补给事务跨菜单关闭保存；只有成功返回任务才应用已确认方案。
local supplySession=nil
-- 检查点配置随武器快照保存，返航重选不能改写已经捕获的旧检查点。
local checkpointProfile=nil
local weaponRevision=0 -- 只在兵装重建成功后递增；同槽补给也必须刷新剪影。
local lastPublish=0
local lastActions=-1
local lastHUD=-1
local previewOptions=nil -- 开场只缓存当前机体的兵装资料，不反复查询解锁和性能数据。
local lastPreviewHUD=-1 -- 预接管与正式 HUD 使用相同的短周期。
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
-- 菜单性能诊断候选：只累计耗时，每15秒随既有统计输出，不逐次写日志。
local performanceStages={}
-- 仅开场分帧等一次真实呈现，不在游戏线程Sleep或等待渲染锁。
function mountStartup.cancel()
    if mountStartup.token then
        local called,ok,_,err=pcall(mountStartup.invoke,3,mountStartup.token)
        if not called or not ok then note("MOUNT_LOADING_CANCEL_ERROR "..tostring(called and err or ok))end
    end
    mountStartup.token=nil;mountStartup.key=nil;mountStartup.ready=false;mountStartup.finished=false
end
function mountStartup.prepare(key)
    if key~=mountStartup.key then
        mountStartup.cancel();mountStartup.serial=mountStartup.serial+1
        mountStartup.token=mountStartup.serial;mountStartup.key=key
        local ok,_,err=mountStartup.invoke(1,mountStartup.token);assert(ok,err)
        return false -- 交还游戏循环，让渲染线程先把提示送上屏幕。
    end
    if mountStartup.ready then return true end
    local ok,ready,err=mountStartup.invoke(0,mountStartup.token);assert(ok,err)
    if ready then
        mountStartup.ready=true
        if err~=""then note("MOUNT_LOADING_PRESENT_FALLBACK "..err)end
    end
    return ready
end
function mountStartup.finish()
    if not mountStartup.token or mountStartup.finished then return end
    local ok,_,err=mountStartup.invoke(2,mountStartup.token);assert(ok,err)
    mountStartup.finished=true -- 界面按实际首次呈现时间补足一秒，不延迟武器就绪。
end
local function recordStage(name,started)
    local row=performanceStages[name]
    if not row then row={count=0,total=0,peak=0};performanceStages[name]=row end
    local elapsed=(os.clock()-started)*1000
    row.count=row.count+1;row.total=row.total+elapsed;row.peak=math.max(row.peak,elapsed)
end
-- 只清理本次玩家的运行缓存，不触碰页面、原生输入或补给确认事务。
local function resetFlightTracking()
    mountStartup.cancel()
    configured=false;lastArmed=nil;nativeState=nil;internal=false;hudSamplingError=nil;inputNotice=nil
    weaponSwitch.pending=nil
    flightOptions=nil;lastUIKey=nil;lastHUD=-1;previewOptions=nil;lastPreviewHUD=-1
    currentProfile=nil;lastSelected=-1
end
local function resetCheckpointState()
    checkpointPending=false;restorePending=false;checkpointProfile=nil
    debugProfileActive=false;checkpointDebugProfile=false
end
local descriptions={
    qaam="高机动空对空导弹",sasm="小型空爆导弹",hcaa="高容量空对空导弹",hpaa="高威力空对空导弹",hvaa="高速空对空导弹",laam="远程空对空导弹",saam="半主动雷达制导导弹",
    ["4aam"]="四目标空对空导弹",["6aam"]="六目标空对空导弹",["8aam"]="八目标空对空导弹",
    lasm="远程反舰导弹",lagm="远程空对地导弹",lacm="远程巡航导弹",["4agm"]="四目标空对地导弹",["8agm"]="八目标空对地导弹",
    ugb="无制导炸弹",gpb="精确制导炸弹",xsdb="小直径炸弹",sffs="自锻破片子母弹",sod="远距撒布弹药",faeb="燃料空气炸弹",
    eml="电磁轨道炮",plsl="脉冲激光炮",tls="战术激光系统",rkt="火箭弹",grkt="制导火箭弹",mgp="机炮吊舱",mpbm="多用途爆炸导弹",uav="无人机",ecm="电子干扰",mstm="多目标导弹",admm="全向多用途导弹"}
local unsupportedDual={[26]=true,[31]=true} -- 未知31/26仍拒绝；电子战/UAV真实类及生命周期由核心核对。
local electronicDecisionSeen={} -- 仅去重诊断日志，不缓存资格；换涂装后仍读取当前真实类。
local names={f04e="F-4E Phantom II",f15c="F-15C Eagle",f15e="F-15E Strike Eagle",f18f="F/A-18F Super Hornet",f16c="F-16C Fighting Falcon",f22a="F-22A Raptor",f35c="F-35C Lightning II",a10a="A-10C Thunderbolt II",f14d="F-14D Super Tomcat",f02a="F-2A",f15j="F-15J",j39e="Gripen E",m21b="MiG-21bis",m29a="MiG-29A",m31b="MiG-31B",mr2k="Mirage 2000-5",rflm="Rafale M",typn="Typhoon",pkfa="Su-57",su30="Su-30M2",su30sm="Su-30SM",su33="Su-33",su34="Su-34",su35="Su-35S",su37="Su-37",su47="Su-47",x02s="X-02S Strike Wyvern",adf11f="ADF-11F Raven",zoef="ADF-01 FALKEN",mrgn="ADFX-01 Morgan",dark="DarkStar",f18e="F/A-18E",f18etg="F/A-18E Top Gun",f14a="F-14A",f14atg="F-14A Top Gun",su57tg="Su-57 Top Gun",f104="F-104C",asfx="ASF-X Shinden II",fa27="XFA-27",fa44="CFA-44 Nosferatu",f15m="F-15 S/MTD",f16x="F-16XL",fb22="FB-22",f18x="F/A-18F Block III",f02x="F-2A Super Kai",m35d="MiG-35D",yf23="YF-23"}

local function valid(object)return object and object:IsValid()end
local nativeClient=dofile(root.."Scripts/native_bridge.lua").new{
    execute=bridge,json=json,profile=Profile}
local function native(op,target)
    if weaponSwitch.salvo.service and op~="switch"and op~="snapshot"then weaponSwitch.salvo.service:release()end
    if ensureMountAssets and runtimeFixedMountEnabled then
        -- 新玩家、重建或检查点是明确的新尝试边界；普通轮切不重复重试失败资源。
        local retryAssets=op=="setup"or op=="resupply_setup"or op=="debug_setup"or op=="checkpoint_restore"
        ensureMountAssets(nil,nil,retryAssets,currentProfile)
    end
    assert(IsInGameThread(),"Native weapon access requires the game thread")
    assert(valid(plane) and valid(activator),"Player plane is unavailable")
    local time=game:GetTimeSeconds(plane)
    local result=nativeClient:request{op=op,plane=plane:GetAddress(),activator=activator:GetAddress(),
        target=target or 0,time=time,profile=currentProfile}
    if op=="setup"or op=="resupply_setup"or op=="debug_setup"or op=="switch"or op=="checkpoint_restore"or op=="resupply"then
        weaponRevision=weaponRevision+1
    end
    result.weapon_revision=weaponRevision
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
local function isSelectedPlane(sourcePlane,code)
    local info=catalogue and catalogue[tostring(instance:GetSelectedPlaneID())]
    if not info or not info.code or info.code:lower()~=code:lower()then return false end
    return modAircraft:matches(sourcePlane,info,instance)
end
local function readOptionsFor(id)
    -- Purchases cannot change during a sortie. Do not scan the global object
    -- registry or enumerate PurchasedWeapons for every HUD refresh.
    if configured and isFlight and id==planeId and flightOptions then return flightOptions end
    local info=catalogue and catalogue[tostring(id)]
    if not info or not info.supported then return {} end
    if modAircraft:nativeOnlyReason(id,info)then return {},"native_only"end
    assert(IsInGameThread()and valid(instance),"弹药预览需要当前游戏线程与机体选择")
    -- 菜单直接读取机库当前展示的实例，任务读取玩家实例；不假定蓝图默认对象已具备完整组件。
    local sourcePlane=plane -- 任务实例已由现有世界/生命周期检查确定。
    if not isFlight then
        local hangar=instance.HangarManager -- 原生机库管理器持有实际展示对象，无需全局猜测或另行生成机体。
        assert(valid(hangar),"当前机库管理器未就绪")
        sourcePlane=hangar.LoadedPlayerPlane
        -- 选择编号先改变、预览模型后加载属于原生异步换机；返回显式等待原因，不能记成永久接入错误。
        if not modAircraft:readyObject(sourcePlane)then return {},"preview_missing_or_loading selected="..info.code end
        if instance:GetSelectedPlaneID()~=id or not isSelectedPlane(sourcePlane,info.code)then
            return {},"preview_changing selected="..info.code.." expected="..modAircraft:reference(info,instance).." actual="..sourcePlane:GetClass():GetFullName()
        end
    end
    assert(valid(sourcePlane),"当前机体实例未加载，请确认模型已安装："..info.code)
    assert(valid(sourcePlane:GetWorld())and sourcePlane:GetWorld():GetAddress()==instance:GetWorld():GetAddress(),"机体兵装来源不属于当前世界")
    -- 机库预览实例的 PlaneTypeID 可为 -1，以实际类继承链核对当前选择，保留完整路径约束。
    -- 菜单分支刚核对过同一实例的类链，避免重复遍历；任务实例仍在这里独立核对。
    if isFlight then assert(isSelectedPlane(sourcePlane,info.code),"任务机体与当前选择不符："..info.code)end
    local sourceActivator=sourcePlane.PlayerWeaponActivator -- 使用原生构造完成并应用蓝图覆盖的组件。
    if not modAircraft:readyObject(sourceActivator)then
        if not isFlight then return {},"weapon_activator_loading selected="..info.code end
        error("当前机体实例兵装配置未就绪："..info.code)
    end
    local continuousOptions={}
    local salvoOptions={}
    local presentation={}
    local baseWeapons=modAircraft:baseWeapons(info,sourcePlane) -- 目录软引用缺失时只沿实际父类补齐展示来源。
    local weaponIds={} -- 菜单 PWDT 编号与运行时 WeaponID 可以不同；部件计算必须用后者。
    for i=1,3 do
        local weaponClass=sourceActivator[weaponFieldNames.config[i]].WeaponClass -- 每槽可被替换模组独立改写。
        assert(valid(weaponClass),"当前机体兵装类缺失："..info.code.."/"..i)
        local weaponDefault=modAircraft:existingDefault(weaponClass)
        if not weaponDefault then
            if not isFlight then return {},"weapon_default_loading selected="..info.code.." config="..i end
            error("当前机体兵装默认对象未就绪："..info.code.."/"..i)
        end
        weaponIds[i]=weaponDefault.WeaponID
        local name=weaponDefault:GetFullName()
        salvoOptions[i]=weaponDefault.bAutoFireOnHold and
            ((weaponIds[i]==17 and name:match("Default__plwp_mgp_r0_C$")~=nil)or
             (weaponIds[i]==22 and name:match("Default__plwp_plsl_x0_C$")~=nil))
        assert(type(weaponIds[i])=="number","原生兵装编号类型异常："..info.code.."/"..i)
        assert(weaponIds[i]%1==0 and weaponIds[i]>=0 and weaponIds[i]<102,"当前兵装超出核心支持范围："..info.code.."/"..i)
        presentation[i]=modAircraft:weapon(catalogue,info,i,weaponClass,weaponIds[i],baseWeapons)
        local ok,supported,electronic,reason,uav=continuousSPOption(sourcePlane:GetAddress(),weaponDefault:GetAddress())
        assert(ok,supported)
        assert(type(supported)=="boolean"and type(electronic)=="boolean"and type(reason)=="string"and type(uav)=="boolean","持续兵装资格回执不完整")
        if electronic or uav or unsupportedDual[weaponIds[i]]then continuousOptions[i]=supported
        else continuousOptions[i]=true end
        if electronic or uav or unsupportedDual[weaponIds[i]]then
            local key=info.code.."/"..name.."/"..reason
            if not electronicDecisionSeen[key]then
                electronicDecisionSeen[key]=true
                note((uav and (supported and "MOD_UAV_SUPPORTED"or "MOD_UAV_UNAVAILABLE")or
                    (supported and "MOD_ELECTRONIC_SUPPORTED"or "MOD_ELECTRONIC_UNAVAILABLE"))..
                    " plane="..info.code.." config="..i.." weapon_id="..weaponIds[i].." class="..name.." reason="..reason)
            end
        end
    end
    local w=info.weapons -- 原生部件先作用于完整载量；草稿随后仍使用原配弹比例。
    local ok,encoded=nativeLoadoutAmmo(instance:GetAddress(),id,weaponIds[1],w[1].max,weaponIds[2],w[2].max,weaponIds[3],w[3].max)
    assert(ok,encoded);local amounts=json.decode(encoded)
    assert(type(amounts)=="table"and #amounts==3,"原生弹药预览结果不完整")
    local owned=ownedWeapons();local options={}
    for i,item in ipairs(info.weapons)do
        local display=presentation[i]
        options[i]={index=item.index,code=display.code,runtime_weapon_id=weaponIds[i],salvo_supported=salvoOptions[i],max=amounts[i],base_max=item.max,category=display.category,type_key=display.type_key,generic=display.generic,
            stats=display.stats,description=descriptions[display.code]or display.code:upper(),
            owned=item.index==0 or owned[id*10+item.index]==true, -- 不再绕过游戏购买记录。
            available=continuousOptions[i]==true}
    end
    return options
end
local function optionsFor(id)
    local started=os.clock()
    local options,waiting=readOptionsFor(id)
    recordStage("options",started)
    return options,waiting
end
local function profileFor(id)
    local temporary=aircraftDebug and aircraftDebug:profile(id)
    if temporary then return temporary end
    local saved=config.profiles[tostring(id)]
    if saved then return Profile.withSettings(saved,config)end
    local selected=valid(instance) and instance:GetSelectedPlaneSWP()or 0
    return {a=math.max(0,math.min(2,selected)),b=-1,c=-1,ratio=1}
end
-- 复制小配置，避免补给草稿与检查点引用同一张可修改的表。
local copyProfile=Profile.copy -- 检查点、补给和预览使用同一复制规则。
local function previewWarning(warning)
    warning=warning and warning~=""and tostring(warning)or nil
    if warning~=previewMountWarning then
        previewMountWarning=warning;lastUIKey=nil
        if warning then note("MOUNT_PREVIEW_WARNING "..warning)end
    end
end
local scenePreviewRequested=false
local scenePreviewOptions=nil
local scenePreviewPlane=nil
local scenePreviewTime=-1
local supplyPreviewActive=false
local lastMountPreviewContext=nil -- 开关重启预览时复用最后候选，原生接口仍须重新核对身份。
local function releaseMountPreview(ownerAddress)
    if not ownerAddress or ownerAddress==0 then
        scenePreviewOptions=nil;scenePreviewPlane=nil;supplyPreviewActive=false
    end
    -- 机库由游戏实例持有，旧页面的归还请求不会拆掉整个机库的显示。
    local called,ok,warning=pcall(nativeMountPreview,ownerAddress or 0,0,0,0,0,0,0,0,0,0)
    if not called then previewWarning(ok);return false,tostring(ok)end
    if not ok or warning and warning~=""then
        previewWarning(warning and warning~=""and warning or "挂载预览归还失败")
    end
    return ok,not ok and previewMountWarning or warning
end
local mountAssetStatus={} -- 按实际资源记成功/失败；组合变化只加载新方案需要的副本。
local mountAssetFailureOwner={} -- 失败只属于当时的机体实例，不阻断后来换机产生的新实例。
ensureMountAssets=function(code,sourcePlane,retry,profile)
    if not(runtimeAllowed and runtimeModEnabled and (runtimeFixedMountEnabled or retry))then return true end
    if not isFlight and not supplySession and not config.hangar_mount_enabled then return true end -- 机库关闭时不预加载模型，任务/补给照旧。
    local info=catalogue and catalogue[tostring(planeId)]
    code=code or info and info.code
    local entries=code and expandedMountAssets[code]
    if not entries or #entries==0 then return true end
    sourcePlane=sourcePlane or plane
    if not valid(sourcePlane)then
        local hangar=valid(instance)and instance.HangarManager or nil
        sourcePlane=valid(hangar)and hangar.LoadedPlayerPlane or nil
    end
    local owner=valid(sourcePlane)and sourcePlane:GetAddress()or 0
    profile=profile or currentProfile
    local mask=nil
    if profile then
        mask=0
        for _,field in ipairs({"a","b","c"})do if profile[field]>=0 then mask=mask | (1 << profile[field])end end
        if mask&(mask-1)==0 then return true end -- 原生单挂不替换派生网格，也不预加载其他组合。
    end
    local status=mountAssetStatus[code]or {};mountAssetStatus[code]=status
    if mountAssetFailureOwner[code]~=owner then
        for index,value in pairs(status)do if value~=true then status[index]=nil end end
        mountAssetFailureOwner[code]=owner -- 已成功保有的纯资源仍可跨机体复用。
    end
    local needed={}
    for _,entry in ipairs(entries)do
        if not mask or not entry[4]or entry[4]&(1 << mask)~=0 then
            if retry and status[entry[1]]~=true then status[entry[1]]=nil end
            if status[entry[1]]~=nil and status[entry[1]]~=true then return false end
            if status[entry[1]]~=true then needed[#needed+1]=entry end
        end
    end
    if #needed==0 then return true end
    local loadingIndex=needed[1][1]
    local ok,err=pcall(function()
        assert(IsInGameThread(),"扩展挂架只能在游戏线程加载")
        assert(type(LoadAsset)=="function","当前加载器没有独立挂架加载接口")
        sourcePlane=sourcePlane or plane
        if not valid(sourcePlane)then
            local hangar=valid(instance)and instance.HangarManager or nil
            sourcePlane=valid(hangar)and hangar.LoadedPlayerPlane or nil
        end
        assert(valid(sourcePlane)and isSelectedPlane(sourcePlane,code),"扩展挂架缺少当前机体")
        for _,entry in ipairs(needed)do
            loadingIndex=entry[1]
            local pool=entry[3]
            assert(type(pool)=="number"and pool>=1 and pool<=3,"扩展挂架缺少目标组件编号")
            -- 这里只加载和保有纯资源；碰撞资格属于实际绑定阶段，不能据未使用的挂架阻止预加载。
        end
        for _,entry in ipairs(needed)do
            loadingIndex=entry[1]
            local started=os.clock()
            local pathElapsed,registryElapsed=0,0
            local package="/Game/AC7MultiSP/Research/"..entry[2]
            local objectPath=package.."."..entry[2]
            local object=StaticFindObject(objectPath)
            if not valid(object)then
                note("MOUNT_ASSET_PATH_BEGIN aircraft="..code.." mask="..tostring(mask).." path="..objectPath)
                -- 本PAK的自有资源在登记表中未命中；先用已验证的原生精确路径，避免逐份重复失败搜索。
                -- 原生工厂建立完整AssetData；来源或接口未就绪时仍可尝试原加载器入口。
                local pathStarted=os.clock()
                local pathOK,pathObject=pcall(function()
                    local helpers=StaticFindObject("/Script/AssetRegistry.Default__AssetRegistryHelpers")
                    assert(valid(helpers),"独立挂架缺少原生AssetRegistryHelpers")
                    local donor=sourcePlane.SWP1.StaticMesh
                    local meshClass=StaticFindObject("/Script/Engine.StaticMesh")
                    assert(valid(donor)and valid(meshClass)and donor:IsA(meshClass),"独立挂架缺少StaticMesh来源对象")
                    local data=helpers:CreateAssetData(donor,false)
                    data.ObjectPath=FName(objectPath)
                    data.PackageName=FName(package)
                    data.PackagePath=FName("/Game/AC7MultiSP/Research")
                    data.AssetName=FName(entry[2])
                    data.AssetClass=FName("StaticMesh")
                    return helpers:GetAsset(data)
                end)
                pathElapsed=(os.clock()-pathStarted)*1000
                object=pathOK and valid(pathObject)and pathObject or StaticFindObject(objectPath)
                if not pathOK then note("MOUNT_ASSET_PATH_ERROR path="..objectPath.." error="..tostring(pathObject))end
                note("MOUNT_ASSET_PATH_RESULT path="..objectPath.." valid="..tostring(valid(object)))
                if not valid(object)then
                    local registryStarted=os.clock()
                    note("MOUNT_ASSET_REGISTRY_BEGIN path="..objectPath)
                    local loaded,registered,didLoad=LoadAsset(objectPath)
                    registryElapsed=(os.clock()-registryStarted)*1000
                    object=valid(loaded)and loaded or StaticFindObject(objectPath)
                    note("MOUNT_ASSET_REGISTRY_RESULT path="..objectPath.." registered="..tostring(registered).." loaded="..tostring(didLoad))
                end
            end
            assert(valid(object),"独立挂架加载失败："..entry[2])
            local accepted,message=registerMountAsset(entry[1],object:GetAddress())
            assert(accepted,message)
            status[entry[1]]=true
            note(string.format("MOUNT_ASSET_LOAD_COST asset=%s elapsed_ms=%.3f path_ms=%.3f registry_ms=%.3f",
                entry[2],(os.clock()-started)*1000,pathElapsed,registryElapsed))
        end
    end)
    if not ok then status[loadingIndex]=tostring(err)end
    if not ok then previewWarning("多 SP 挂载已回退："..tostring(err));note("MOUNT_ASSET_ERROR "..tostring(err))end
    return ok
end
local function mountPreview(context)
    -- 视觉失败只返回警告，不能冒泡为页面或兵装事务错误。
    local called,ok,warning=pcall(function()
        local supply=context.mode=="resupply"or context.mode=="resupply_transition"
        local flight=context.mode=="flight"
        if not supply and not flight then scenePreviewRequested=true end
        if not supply and not flight and not config.hangar_mount_enabled then
            if scenePreviewPlane or lastMountPreviewContext then
                lastMountPreviewContext=nil;return releaseMountPreview(0)
            end
            return true,"" -- 已归还后不逐帧重复调用核心，也不建立机库候选缓存。
        end
        lastMountPreviewContext=context
        local owner=supply and context.owner or context.instance
        if not(runtimeAllowed and runtimeModEnabled)or errorText then
            return releaseMountPreview(0)
        end
        if supply or flight then applyFixedMountSetting()else applyFixedMountSetting(config.hangar_mount_enabled)end
        if not runtimeFixedMountEnabled then return releaseMountPreview(0)end
        if not valid(owner)or not valid(context.instance)then return false,""end
        local sourcePlane=plane
        if not supply and not flight then
            local hangar=context.instance.HangarManager
            sourcePlane=valid(hangar)and hangar.LoadedPlayerPlane or nil
        end
        local info=catalogue and catalogue[tostring(context.planeId)]
        -- CFA任务外仅用当前原生模型；在资源加载、反射绑定和多挂维护之前退出。
        if info and info.code=="fa44"and not supply and not flight then
            return releaseMountPreview(0)
        end
        if info and info.code~="f15e"and info.code~="f35c"and info.code~="su30sm"and not expandedMountAssets[info.code]and not lightMountAircraft[info.code]then return releaseMountPreview(0)end
        -- 异步换机属于等待：先归还上一展示对象，下次 pump 再核对完整身份。
        if not valid(sourcePlane)or not info or not isSelectedPlane(sourcePlane,info.code)
            or context.instance:GetSelectedPlaneID()~=context.planeId then
            releaseMountPreview(owner:GetAddress());return false,""
        end
        if not ensureMountAssets(info.code,sourcePlane,false,context.profile)then
            releaseMountPreview(owner:GetAddress());return false,previewMountWarning or "多 SP 挂架资源未就绪"
        end
        local sourceActivator=sourcePlane.PlayerWeaponActivator
        if not valid(sourceActivator)then releaseMountPreview(owner:GetAddress());return false,""end
        assert(valid(sourcePlane:GetWorld())and valid(owner:GetWorld())
            and sourcePlane:GetWorld():GetAddress()==owner:GetWorld():GetAddress()
            and sourcePlane:GetWorld():GetAddress()==context.instance:GetWorld():GetAddress(),"预览页面、机体或世界已变化")
        local profile=context.profile;local count=Profile.count(profile);local amounts={}
        for i,field in ipairs(Profile.fields)do
            local weapon=profile[field];local option=weapon>=0 and context.options[weapon+1]or nil
            amounts[i]=option and Profile.ammo(option.max,count,profile.ratio)or 0
        end
        local started=os.clock()
        local accepted,message=nativeMountPreview(owner:GetAddress(),sourcePlane:GetAddress(),sourceActivator:GetAddress(),
            context.mode=="resupply_transition"and 5 or supply and 2 or flight and 3 or 1,profile.a,profile.b,profile.c,amounts[1],amounts[2],amounts[3])
        recordStage("preview_native",started)
        if supply then supplyPreviewActive=true end
        return accepted,message
    end)
    if not called then
        if type(context.ownerAddress)=="number"then releaseMountPreview(context.ownerAddress)end
        previewWarning(ok);return false
    end
    if warning and warning~=""then previewWarning(warning)
    elseif ok then previewWarning(nil)end
    return ok,warning
end
-- 所有机库页面共用这一层；武器页当轮提供草稿时优先，其他页面展示已保存方案。
local function maintainScenePreview()
    if scenePreviewRequested or isFlight or supplySession or panelOpen or errorText then return end
    if not config.hangar_mount_enabled then
        if scenePreviewPlane or lastMountPreviewContext then releaseMountPreview(0);lastMountPreviewContext=nil end
        return
    end
    if not(runtimeAllowed and runtimeModEnabled)then return end -- 机库是否启用只读机库开关，不受任务开关影响。
    if not runtimeContext.hangarVisible then
        if scenePreviewPlane or lastMountPreviewContext then releaseMountPreview(0);lastMountPreviewContext=nil end
        return -- 评分/任务选择页即使保留了隐藏机体，也不刷新它的挂载、部件与配弹。
    end
    local called,err=pcall(function()
        local hangar=valid(instance)and instance.HangarManager or nil
        local model=valid(hangar)and hangar.LoadedPlayerPlane or nil
        if not valid(model)then releaseMountPreview(0);return end
        local info=catalogue and catalogue[tostring(planeId)]
        if info and info.code=="fa44"then releaseMountPreview(0);return end
        if not info or info.code~="f15e"and info.code~="f35c"and info.code~="su30sm"and not expandedMountAssets[info.code]then return end
        local address=model:GetAddress()
        if address~=scenePreviewPlane or os.clock()-scenePreviewTime>=.5 then
            local options,waiting=optionsFor(planeId)
            if waiting then releaseMountPreview(0);return end
            scenePreviewOptions=options;scenePreviewPlane=address;scenePreviewTime=os.clock()
        end
        mountPreview{owner=instance,instance=instance,planeId=planeId,mode="scene",
            profile=profileFor(planeId),options=scenePreviewOptions}
    end)
    if not called then releaseMountPreview(0);previewWarning(err)end
end
local function maintainSupplyPreview()
    if not valid(plane)or not valid(activator)then return end
    if not(runtimeAllowed and runtimeModEnabled and runtimeFixedMountEnabled)then releaseMountPreview(0);return end
    if not supplyPreviewActive then
        if supplySession and supplySession.menuClosed and supplySession.preview then
            mountPreview{owner=plane,instance=instance,planeId=planeId,mode="resupply_transition",
                profile=supplySession.preview,options=supplySession.options}
        end
        return
    end
    local called,ok,warning=pcall(nativeMountPreview,0,plane:GetAddress(),activator:GetAddress(),4,0,0,0,0,0,0)
    if not called then previewWarning(ok)
    elseif warning and warning~=""then previewWarning(warning)end
end
-- 补给开始先接管原生输入，不能等到页面轮询发现后才禁用；解绑不解引用旧机体。
local function ownSupplyInput(enabled)
    assert(IsInGameThread(),"补给输入所有权必须在游戏线程修改")
    if enabled then assert(valid(plane)and valid(activator),"补给输入缺少玩家机体")end
    local ok,err=nativeSupplyControl(enabled and plane:GetAddress()or 0,enabled and activator:GetAddress()or 0)
    assert(ok,err or "补给输入接口未返回成功状态")
end
-- 只播放确认声音，不执行原生单 SP 提交或页面跳转。
local function menuConfirm(owner)
    assert(IsInGameThread()and valid(owner),"菜单确认音缺少游戏线程或有效页面")
    local ok,err=nativeMenuConfirm(owner:GetAddress()) -- 页身份和原生入口由核心再次核对。
    assert(ok,err or "菜单确认音接口未返回成功状态")
end
local NativeLoadout=dofile(root.."Scripts/native_loadout.lua")
nativeLoadout=NativeLoadout.new{note=note,mode="preparation",commit=function(id,profile)
    assert(not isFlight and id==planeId,"Aircraft changed before native preparation commit")
    local options,waiting=optionsFor(id) -- 提交不能等待或使用前一机体的兵装。
    assert(not waiting,"确认时机库预览尚未就绪："..tostring(waiting))
    Profile.validate(profile,options)
    currentProfile=profile;config.profiles[tostring(id)]=profile;saveConfig()
    lastUIKey=nil;note("PROFILE_SAVED_NATIVE "..id)
end,keyboardNavigation=preparationNavigation,preparationAction=nativePreparationAction,
    confirmSound=menuConfirm,weaponIcon=nativeWeaponIcon,translate=tr,ammoSettings=ammoSettings,mountPreview=mountPreview,releaseMountPreview=releaseMountPreview}
nativeStandby=NativeLoadout.new{note=note,mode="standby",keyboardNavigation=preparationNavigation,translate=tr,ammoSettings=ammoSettings,
    mountPreview=mountPreview,releaseMountPreview=releaseMountPreview}
-- 候选只进入展示快照，不改 optionsFor 的已装结果或保存的方案。
local function partsAmmoPreview(owner,selectedInstance,id,options)
    assert(IsInGameThread()and valid(owner)and valid(selectedInstance),"PART 预览需要当前游戏线程及有效页面")
    assert(owner:IsVisible()and owner:GetWorld():GetAddress()==selectedInstance:GetWorld():GetAddress(),"PART 预览页面或世界已变化")
    assert(owner:IsA(StaticFindObject("/Script/Nimbus.CampaignHangarPartsWidget")),"PART 预览页面类型不符")
    assert(selectedInstance:GetSelectedPlaneID()==id and #options==3,"PART 预览机体或兵装选项不符")
    local status=owner.HangarStatus -- 原生已处理当前光标、页签及部件列表的状态对象。
    assert(valid(status)and valid(status.BoardWeaponsAmmo),"PART 原生弹药上下文缺失")
    local board=status.BoardWeaponsAmmo -- 每次从当前页面读取，不跨代次保存对象。
    local ok,encoded=nativePartsAmmo(owner:GetAddress(),status:GetAddress(),board:GetAddress(),selectedInstance:GetAddress(),id,
        options[1].runtime_weapon_id,options[1].base_max,options[2].runtime_weapon_id,options[2].base_max,
        options[3].runtime_weapon_id,options[3].base_max)
    assert(ok,encoded)
    local result=json.decode(encoded) -- pending 与真实数量严格区分，不把上架机体数量拿来占位。
    assert(type(result)=="table"and type(result.pending)=="boolean","PART 预览结果缺少状态")
    if not result.pending then
        assert(result.plane==id and type(result.amounts)=="table"and #result.amounts==3,"PART 预览结果不完整")
        for _,entry in ipairs(result.amounts)do
            assert(type(entry.affects)=="boolean"and type(entry.preview)=="number"and type(entry.current)=="number","PART 预览数量或影响标记缺失")
        end
    end
    return result
end
nativeParts=NativeLoadout.new{note=note,mode="parts",keyboardNavigation=preparationNavigation,translate=tr,partsAmmoPreview=partsAmmoPreview,ammoSettings=ammoSettings,
    mountPreview=mountPreview,releaseMountPreview=releaseMountPreview}
-- 原生补给菜单确认只暂存方案；此时不改武器，也不保存到配置文件。
nativeSupply=NativeLoadout.new{note=note,mode="resupply",commit=function(id,profile)
    assert(supplySession and id==supplySession.planeId,"补给确认期间机体已变化")
    assert(not supplySession.confirmed,"本次补给方案已确认")
    supplySession.confirmed=copyProfile(profile)
    note("RESUPPLY_PROFILE_STAGED "..id.." "..json.encode(profile))
end,keyboardNavigation=preparationNavigation,takeoff=function(owner)
    assert(IsInGameThread(),"补给起飞必须在游戏线程执行")
    assert(supplySession and valid(plane)and valid(activator),"补给起飞缺少当前玩家")
    assert(owner:GetWorld():GetAddress()==plane:GetWorld():GetAddress(),"补给页面与玩家不在同一世界")
    local ok,err=nativeTakeoff(owner:GetAddress(),plane:GetAddress(),activator:GetAddress()) -- 原生负责关闭页面并通知任务。
    assert(ok,err or "原生补给 YES 接口未返回成功状态")
end,confirmSound=menuConfirm,translate=tr,ammoSettings=ammoSettings,mountPreview=mountPreview,releaseMountPreview=releaseMountPreview}
for _,adapter in ipairs({nativeLoadout,nativeStandby,nativeParts})do
    adapter.onFailure=function(id,message)
        if not isFlight and not supplySession and not configured then modAircraft:quarantine(id,message)end
    end
end
local function pageContext(context)
    context.instance=instance
    if context.mode=="resupply"then
        context.planeId=supplySession.planeId
        context.getProfile=function()return copyProfile(supplySession.profile)end
        context.getOptions=function()return supplySession.options end
    else
        assert(context.mode=="preparation"or context.mode=="saved","未知页面方案来源")
        context.planeId=planeId
        context.getProfile=function()
            if context.mode=="preparation"then return profileFor(planeId)end
            local saved=config.profiles[tostring(planeId)];return saved and Profile.withSettings(saved,config)or nil
        end
        context.getOptions=function()return optionsFor(planeId)end
        context.getVariantToken=function()return modAircraft:token(instance)end
        context.getSelectionToken=function()return modAircraft:selection(instance)end
    end
    return context
end
-- 按当前任务世界定位原生暂停拥有者；只在开关管理页时读取，不参与每帧扫描。
local function acquireManagementPause()
    if not isFlight or not valid(plane)then return end
    local world=plane:GetWorld() -- 暂停属于世界，不能沿用另一任务的记录。
    assert(valid(world),"当前任务世界未就绪")
    local settings=nil -- 当前世界唯一的 WorldSettings。
    for _,candidate in ipairs(FindAllOf("WorldSettings")or{})do
        local candidateWorld=valid(candidate)and candidate:GetWorld()or nil -- 排除模板及已离开世界的对象。
        if valid(candidateWorld)and candidateWorld:GetAddress()==world:GetAddress()then
            assert(not settings,"当前世界出现多个 WorldSettings，停止管理暂停")
            settings=candidate
        end
    end
    assert(valid(settings),"当前世界的暂停状态未就绪")
    local pauser=settings.Pauser -- 反射现场核对：WorldSettings.Pauser 是当前真实暂停拥有者。
    managementPause={world=world:GetAddress(),settings=settings,owned=false,
        effective_before=game:IsGamePaused(plane),existing=valid(pauser),pauser=valid(pauser)and pauser:GetAddress()or nil}
    note("MANAGEMENT_PAUSE_BEGIN effective="..tostring(managementPause.effective_before).." existing="..tostring(managementPause.existing))
    if not managementPause.existing then
        local accepted=game:SetGamePaused(plane,true) -- 即使临时有效暂停已生效，也明确取得自己可归还的原生暂停。
        pauser=settings.Pauser
        managementPause.owned=valid(pauser) -- 发生部分成功时保留实际所有权，不能丢掉暂停记录。
        managementPause.pauser=valid(pauser)and pauser:GetAddress()or nil
        assert(accepted==true and managementPause.owned,"管理界面未能取得原生暂停")
    end
    note("MANAGEMENT_PAUSE_ACQUIRED owned="..tostring(managementPause.owned).." effective="..tostring(game:IsGamePaused(plane)))
end
-- 只核对打开F6前的同一原生暂停；普通关闭保留，明确换机另行交接。
local function existingManagementPauseValid(lease)
    return lease and lease.existing and not lease.owned and lease.effective_before
        and valid(plane)and valid(lease.settings)and valid(lease.settings:GetWorld())
        and lease.settings:GetWorld():GetAddress()==lease.world
        and valid(plane:GetWorld())and plane:GetWorld():GetAddress()==lease.world
        and valid(lease.settings.Pauser)and lease.settings.Pauser:GetAddress()==lease.pauser
        and game:IsGamePaused(plane)
end
-- 只释放本面板创建且仍属于同一世界/拥有者的暂停；其他暂停不重写、不强制清零。
local function releaseManagementPause()
    if not managementPause then return end
    local lease=managementPause -- 完成核验前保留会话，失败由控制器明确报告。
    if not valid(lease.settings)then
        note("MANAGEMENT_PAUSE_WORLD_DESTROYED") -- 世界已销毁，无须向新世界发送解除请求。
        managementPause=nil;return
    end
    if lease.owned then
        local world=lease.settings:GetWorld() -- 退出任务时旧玩家可能先销毁，使用暂停所属世界作为上下文。
        assert(valid(world)and world:GetAddress()==lease.world,
            "管理暂停的任务世界已经改变")
        local pauser=lease.settings.Pauser -- 其他系统若已经解除暂停，不重复调用解除接口。
        if valid(pauser)then
            assert(pauser:GetAddress()==lease.pauser,"暂停拥有者已经改变，不能代替其他系统解除")
            local accepted=game:SetGamePaused(lease.settings,false) -- 走游戏原生解除流程，不写 Pauser 字段。
            assert(accepted==true and not valid(lease.settings.Pauser),"管理暂停未能释放，请查看暂停日志")
        end
    end
    note("MANAGEMENT_PAUSE_RELEASED owned="..tostring(lease.owned).." remaining="..tostring(valid(lease.settings)and valid(lease.settings.Pauser)))
    managementPause=nil
end
local function setPanel(open)
    if open and not runtimeAllowed then return end
    if panelOpen==open then return end
    if open and nativeLoadout:isPreparationHandoffPending()then
        note("MANAGEMENT_WAITING_FOR_PREPARATION_CLOSE");return
    end
    if open then
        local gazeOK,_,gazeError=weaponSwitch.gaze.invoke(0,0,0,0)
        assert(gazeOK,gazeError);weaponSwitch.gaze.active=false;weaponSwitch.gaze.available=false
        weaponSwitch.camera.release()
        weaponSwitch.view.release()
        local ok,err=pcall(acquireManagementPause) -- 暂停失败必须在面板可见，不能变成无菜单的静止画面。
        managementPauseError=not ok and tostring(err)or nil
        if not ok then note("MANAGEMENT_PAUSE_ERROR "..managementPauseError)end
        panelOpen=true
        -- Restore the original menu before F8 takes its normal input gate.
        -- Closing F8 restarts preparation from its newly saved profile.
        nativeLoadout:reset{clearError=false}
        -- Hide the engine's software cursor as well as the OS pointer. The
        -- overlay owns a separate pointer until this panel closes.
        cursorController=FindFirstOf("NimbusPlayerController")
        if valid(cursorController)then
            previousShowCursor=cursorController.bShowMouseCursor
            cursorController.bShowMouseCursor=false
        end
        gatedWidgets={}
        for _,widget in ipairs(FindAllOf("MenuBaseWidget")or{})do
            local name=widget:GetFullName()
            if (name:find("/Engine/Transient.",1,true)or name:find("/Game/Maps/",1,true)) and not name:find("WidgetArchetype",1,true) and widget:IsEnableControl()then
                widget:SetIsEnableControl(false);gatedWidgets[#gatedWidgets+1]=widget
            end
        end
    else
        local ok,err=pcall(releaseManagementPause) -- 保持输入仍被面板接管，先完成原生暂停交接。
        if not ok then
            managementPauseError=tostring(err);note("MANAGEMENT_PAUSE_ERROR "..managementPauseError)
            requestOpen=requestOpen+1;lastUIKey=nil -- 明确显示失败并保留所有权，不自动重试或强制清零。
            return
        end
        managementPauseError=nil
        if valid(cursorController)then cursorController.bShowMouseCursor=previousShowCursor end
        cursorController=nil;previousShowCursor=nil
        for _,widget in ipairs(gatedWidgets)do if valid(widget)then widget:SetIsEnableControl(true)end end
        gatedWidgets={}
        panelOpen=false
    end
    lastUIKey=nil;note("MANAGEMENT_PANEL "..(open and "open"or "closed"))
end
local function ready()return valid(plane) and plane.bEnableWeaponInput and not plane:IsWeaponSystemDisabled()end

-- 两种输入独立启用，共用原生目标注视轴；L3仅回中，不切NEXT或注视。
function weaponSwitch.gaze.canSwitch()
    return runtimeAllowed and runtimeModEnabled and not errorText and isFlight and valid(plane)
        and valid(plane.InputComponent)and plane.bEnableCameraInput and not panelOpen and not supplySession
        and not(aircraftDebug and aircraftDebug:busy())and not game:IsGamePaused(plane)
end
function weaponSwitch.gaze.update(_,generation)
    local gaze=weaponSwitch.gaze
    if generation and generation~=gaze.generation then return end
    local inputs=(config.target_gaze_pad_enabled and 1 or 0)|(config.target_gaze_keyboard_enabled and 2 or 0)
    local allowed=inputs~=0 and gaze.canSwitch()
    local address=allowed and plane:GetAddress()or 0
    if gaze.address~=address then gaze.generation=gaze.generation+1;gaze.address=address;lastUIKey=nil end
    if gaze.failed then allowed=false;address=0 end
    local oldActive,oldAvailable=gaze.active,gaze.available
    if not gaze.failed then
        local ok,active,reason,inputToggle=gaze.invoke(address,allowed and 1 or 0,inputs,0)
        gaze.active=ok and active or false
        gaze.available=ok and allowed and reason~="waiting_input"and reason~="special_view"or false
        if not ok then gaze.failed=true;gaze.error=tostring(reason);note("TARGET_GAZE_ERROR "..gaze.error)end
        if inputToggle then
            gaze.response=gaze.response+1
            gaze.noticeWarning=not ok or not allowed or reason=="no_target"
            gaze.notice=reason=="no_target"and tr("没有可注视的目标。","No target is selected.")or ""
            note("TARGET_GAZE_TOGGLED active="..tostring(gaze.active).." allowed="..tostring(allowed).." reason="..tostring(reason))
            lastUIKey=nil
        end
    else gaze.active=false;gaze.available=false end
    if oldActive~=gaze.active or oldAvailable~=gaze.available then lastUIKey=nil end
end

-- 镜头保持只归还自己的轴作用域和R3计时，不改变兵装许可或原生暂停来源。
function weaponSwitch.camera.release()
    local camera=weaponSwitch.camera
    local called,ok,_,reason=pcall(camera.invoke,0,0,0,100,0,0,0)
    camera.available=false;camera.released=true
    if not called or not ok then
        camera.failed=true;camera.error=tostring(called and reason or ok);note("CAMERA_HOLD_RELEASE_ERROR "..camera.error)
    end
end
function weaponSwitch.camera.update()
    local camera=weaponSwitch.camera
    if camera.failed then camera.available=false;return end
    local allowed=runtimeAllowed and runtimeModEnabled and not errorText and isFlight and valid(plane)
        and valid(plane.InputComponent)and plane.bEnableCameraInput and not panelOpen and not supplySession
        and not(aircraftDebug and aircraftDebug:busy())and not game:IsGamePaused(plane)
    if not allowed and camera.released then return end -- 许可无效时，不重复跨DLL清理。
    local before=camera.available
    local ok,available,reason,toggle=camera.invoke(allowed and plane:GetAddress()or 0,allowed and 1 or 0,config.camera_center_key,
        config.camera_sensitivity,1,ConfigStore.centerShortcutMask(config),config.camera_hold_enabled and 1 or 0)
    camera.available=ok and available or false
    camera.released=not allowed or not ok
    if not ok then camera.failed=true;camera.error=tostring(reason);note("CAMERA_HOLD_ERROR "..camera.error);lastUIKey=nil end
    if ok and toggle then
        -- 先保存再替换运行设置；失败保留原开关，手势不在同次按压中重放。
        local saved,err=pcall(function()
            config=configStore:setCameraHold(config,not config.camera_hold_enabled);camera.error=nil
            note("CAMERA_HOLD_R3_TOGGLED "..tostring(config.camera_hold_enabled))
        end)
        if not saved then camera.error=tostring(err);note("CAMERA_HOLD_TOGGLE_SAVE_ERROR "..camera.error)end
        lastUIKey=nil
    end
    if before~=camera.available then lastUIKey=nil end
end

function weaponSwitch.view.release(force)
    local view=weaponSwitch.view
    view.marker(0)
    if force or not view.released then
        local called,ok,_,_,reason=pcall(view.invoke,0,0,0,0)
        if not called or not ok then view.error=tostring(called and reason or ok);note("VIEW_TARGET_RELEASE_ERROR "..view.error)end
    end
    view.available=false;view.released=true
end
-- NEXT仅在管理页选择模式，不再接收键盘或L3模式切换。
function weaponSwitch.view.update()
    local view=weaponSwitch.view
    if view.failed then view.marker(0);return end
    local allowed=(config.view_target_mode=="vr"or config.view_center_dot_enabled)and runtimeAllowed and runtimeModEnabled
        and not errorText and isFlight and valid(plane)and valid(plane.InputComponent)and plane.bEnableCameraInput
        and not panelOpen and not supplySession and not(aircraftDebug and aircraftDebug:busy())and not game:IsGamePaused(plane)
    if not allowed and view.released then view.marker(0);return end
    local before=view.available
    local ok,available,dot,reason=view.invoke(allowed and plane:GetAddress()or 0,allowed and 1 or 0,
        config.view_target_mode=="vr"and 1 or 0,config.view_center_dot_enabled and 1 or 0)
    view.available=ok and available or false;view.released=not allowed or not ok
    if not view.marker(ok and dot and plane:GetNimbusHUDEnabled()and 1 or 0)then
        view.release(true);view.failed=true;view.error="视野中心白点发布失败";lastUIKey=nil;return
    end
    if not ok then view.failed=true;view.error=tostring(reason);note("VIEW_TARGET_ERROR "..view.error);lastUIKey=nil end
    if before~=view.available then lastUIKey=nil end
end

-- 周期调度和最终选择都读取最新状态，刚解除暂停后立即切换也能正常出声。
local function updateCustomAudio()
    local allowed=isFlight and configured and not errorText and not supplySession and not panelOpen
        and valid(plane)and ready()and not game:IsGamePaused(plane) -- 原生暂停包含 F8 之外的暂停菜单。
    local ok,err=nativeCustomAudioUpdate(instance:GetAddress(),allowed and 1 or 0) -- 核心另核对前台与 SFXVolume。
    assert(ok,err or "自定义选择音状态更新失败")
end

-- 所有已配置方案统一接管选择提示；初始化/恢复用静默同步，真正切换用提交。
local function selectionAudio(op,target)
    if op==1 then updateCustomAudio()end
    local ok,err=nativeSelectionAudio(op==2 and 0 or plane:GetAddress(),op==2 and 0 or activator:GetAddress(),op,target or 0)
    assert(ok,err or "选择音效接口没有返回成功")
end
-- 错误收尾也明确报告解绑失败，避免异常穿出引擎回调或永久截住原生声音。
local function releaseRuntimeOwnership()
    weaponSwitch.pending=nil;internal=false
    weaponSwitch.camera.release()
    weaponSwitch.view.release()
    if weaponSwitch.salvo.service then weaponSwitch.salvo.service:release()end
    local gazeCall,gazeOK,_,gazeError=pcall(weaponSwitch.gaze.invoke,0,0,0,0)
    weaponSwitch.gaze.active=false;weaponSwitch.gaze.available=false
    if not gazeCall or not gazeOK then note("TARGET_GAZE_RELEASE_ERROR "..tostring(gazeCall and gazeError or gazeOK))end
    releaseMountPreview(0)
    local ok,err=pcall(function()selectionAudio(2)end)
    if not ok then note("SELECTION_AUDIO_RELEASE_ERROR "..tostring(err))end
    -- 发生任何控制器错误后已停止视觉调度，必须同时释放固定模型的所有权。
    local mountCallOK,mountOK,mountError=pcall(updateFixedMount,0,0)
    if not mountCallOK or not mountOK then note("FIXED_MOUNT_RELEASE_ERROR "..tostring(mountCallOK and mountError or mountOK))end
end

-- 就绪采样与两处提示使用同一最终选择；错误只停用新 HUD，保持武器逻辑。
refreshHUD=function(now)
    assert(IsInGameThread(),"武器 HUD 必须在游戏线程更新")
    if internal then return end
    -- HUD 生命周期不依赖武器输入许可；过场暂时禁止操作不能把已接管的多 SP 恢复成单 SP。
    local enabled=runtimeAllowed and not errorText and (not supplySession or supplySession.applied)and valid(plane)and plane:GetNimbusHUDEnabled()
    -- 右下 HUD 始终启用；中央简称选择常驻或短显，两处共用一次就绪采样和游戏时间。
    local gameTime=enabled and game:GetTimeSeconds(plane)or 0
    if enabled and not hudSamplingError then
        local callOK,sampleOK,mainReady,sp1Ready,sp2Ready,sp3Ready,err=pcall(readReadiness,
            plane:GetAddress(),activator:GetAddress(),gameTime,Profile.count(currentProfile))
        nativeState.ready=nil
        if not callOK or sampleOK~=true then
            hudSamplingError=tostring(callOK and (err or "候选核心的 HUD 内存接口不可用")or sampleOK)
            note("NATIVE_HUD_SAMPLE_ERROR "..hudSamplingError)
        else
            nativeState.ready={mainReady,sp1Ready,sp2Ready,sp3Ready}
        end
    end
    nativeState.armed=plane:IsSpecialWeaponArmed()
    -- 单 SP 出击也处理名称就绪状态，但保持原生面板高度。
    nativeHud:update(plane,currentProfile,flightOptions,nativeState,enabled and not hudSamplingError,now)
    if enabled and hudSamplingError then
        if not weaponPresentation.error then weaponPresentation:fail(hudSamplingError)end
    else
        weaponPresentation:update(plane,currentProfile,flightOptions,nativeState,enabled,now,config.central_weapon_display,gameTime)
    end
end
-- 由出击初始化和飞行切换共同使用的输入释放接口，在 configureFlight
-- 定义前先声明，避免 Lua 将其解析成全局变量。
local releaseWeaponInput
-- 在原生武器系统重建前先接管 HUD。否则 InitializeWeaponSystem 会先让
-- 游戏绘制一行原生 SP，下一次 Lua tick 才能插入第二行，多 SP 出击时会
-- 出现肉眼可见的 MSL+单 SP 短暂画面。预热只建立控件和最终文本，不写武器
-- 内存；真实就绪状态在核心初始化后由 refreshHUD 立即替换。
local function primeFlightHUD(profile,options,prewarm)
    local seed={active=0,armed=false,main_ammo=0,
        cached_sp={0,0,0},maximum={0,0,0}, -- 开场不读取可能尚未初始化的武器状态，计数以 pending 隐藏。
        slots={{},{},{}},ready={false,false,false,false},pending=true} -- 初始化前不把原生单 SP 数量冒充多 SP 实际弹药。
    local visible=plane:GetNimbusHUDEnabled()
    -- 补给起飞过场中预建右下行；原生 HUD 祖先仍控制全局显隐，不提前修改武器。
    nativeHud:update(plane,profile,options,seed,prewarm or visible,os.clock())
    weaponPresentation:update(plane,profile,options,seed,visible,os.clock(),config.central_weapon_display,game:GetTimeSeconds(plane))
end
-- 正常出击建立检查点；补给和显式调试重建保留任务检查点。
local function configureFlight(profile,reason)
    weaponSwitch.pending=nil -- 新方案不接收旧方案尚未完成的选择。
    releaseMountPreview(0) -- 已确认补给预览维持到此处，紧接原有兵装重建交接。
    local resupply=reason=="resupply" -- 原生补给必须先完成成功事务。
    local debugLoadout=reason=="debug"or reason=="debug_hotkey" -- 调试不伪造补给事务，不保存个人方案。
    local debugAircraft=reason=="debug_aircraft" -- 新玩家完整初始化，但检查点标记为本次任务临时方案。
    -- 补给在原生成功事件返回后即可重建；等待飞控输入开放会把旧挂载多留在起飞画面约一秒。
    assert((resupply and supplySession and supplySession.success and valid(plane)and valid(activator))or ready(),
        "武器初始化需要已开放的出击控制，或已经成功返回的补给事务")
    assert(not configured or resupply or debugLoadout,"Loadout is locked for this sortie")
    if debugLoadout then
        assert(configured and not supplySession and ((panelOpen and game:IsGamePaused(plane))or
            (reason=="debug_hotkey"and not panelOpen and not game:IsGamePaused(plane))),"调试换装的飞行或管理暂停状态不符")
        local allowed,err=debugLoadoutReady(plane:GetAddress(),activator:GetAddress()) -- 最后一次只读核对。
        assert(allowed,err)
    end
    -- 在触碰武器前检查候选协议；正式 0.1.7 核心不能冒充支持补给重选。
    if resupply and profile.b>=0 then
        local supported=package.loadlib(root.."bin/DualSP.dll","dualsp_resupply_supported")
        assert(supported,"补给重选需要支持 resupply_setup 的独立候选核心，当前正式 DLL 不支持")
    end
    local options=optionsFor(planeId)
    Profile.validate(profile,options,debugLoadout or debugAircraft)
    currentProfile=profile
    flightOptions=options
    selectionAudio(0,0) -- 重建前先隔离中间态选择音，不影响锁定音收尾。
    primeFlightHUD(profile,options)
    -- 仅在出击或已成功的补给事务内重建；正常切换仍由核心保存消耗与冷却。
    local audioReady,audioError=prepareWeaponRebuild(plane:GetAddress(),activator:GetAddress()) -- 保留旧槽直到停止事件完成。
    assert(audioReady,audioError or "原生锁定音收尾未完成")
    local mountOK,mountError=updateFixedMount(0,0) -- 原生重建之前释放上一组合，涵盖补给转单挂。
    assert(mountOK,mountError)
    applyFixedMountSetting()
    -- 第一槽也由核心同步原生编号后初始化；Lua 不再另建一条单挂初始化路径。
    -- 释放发射按键只负责输入收尾；锁定参数由完整原生初始化/切换流程更新。
    releaseWeaponInput(debugLoadout and "debug_loadout"or resupply and "resupply_loadout"or "mission_loadout")
    -- 单挂也进入核心快照，保证补给后在 1/2/3 槽之间切换仍能恢复旧检查点。
    native(debugLoadout and "debug_setup"or resupply and "resupply_setup"or "setup",0)
    if not resupply and not debugLoadout then native("checkpoint_save",0)end
    configured=true;active=0;lastArmed=plane:IsSpecialWeaponArmed();note("LOADOUT "..planeId.." "..json.encode(profile))
    debugProfileActive=debugLoadout or debugAircraft
    if not resupply and not debugLoadout then checkpointProfile=copyProfile(profile);checkpointDebugProfile=debugAircraft end
    if resupply then supplySession.applied=true end -- 数据就绪后立即展示，事务清理不再撤销刚接管的 HUD。
    -- 核心初始化后立即提交一次真实状态，避免等待 100 ms HUD 周期。
    refreshHUD(os.clock())
    if aircraftDebug then aircraftDebug:configured(planeId,profile)end
end
-- 在原生补给成功回调返回的同一帧提交，模型、弹药、HUD 共用一个事务边界。
local function completeSupplyHandoff()
    if not(supplySession and supplySession.success and isFlight and valid(plane)and valid(activator))then return end
    assert(not errorText,errorText)
    assert(not nativeSupply.error,nativeSupply.error)
    assert(planeId==supplySession.planeId and currentWorld==supplySession.world,"补给返回了不同机体或任务世界")
    local profile=nativeSupply:completeSupply() -- 成功之前不提交草稿，不在 NO、取消或降落期间重建。
    assert(supplySession.confirmed,"原生补给成功，但未收到多 SP 的起飞确认")
    instance:SetPlaneAndSWP(planeId,profile.a)
    assert(instance:GetSelectedPlaneSWP()==profile.a,"补给 SP1 原生选择未生效")
    configureFlight(profile,"resupply")
    config.profiles[tostring(planeId)]=copyProfile(profile);saveConfig()
    nativeSupply:reset{clearError=false,releaseInput=false};ownSupplyInput(false);supplySession=nil;lastUIKey=nil
    refreshHUD(os.clock())
    note("RESUPPLY_LOADOUT_APPLIED "..planeId.." "..json.encode(profile))
end
-- 重建前释放发射输入；该接口不负责更新 seeker，也不能作为停止锁定音的修复。
releaseWeaponInput=function(reason)
    assert(valid(activator),"Player weapon activator is unavailable")
    local ok,err=pcall(function()activator:ReleaseWeaponInput()end)
    assert(ok,err)
    note("WEAPON_INPUT_RELEASED "..reason)
end
-- 普通连射暂存选择；Falken TLS发射期间直接拒绝，不保存或自动重放请求。
function weaponSwitch.defer(desired,reason)
    internal=false;lastArmed=plane:IsSpecialWeaponArmed()
    if reason=="falken_tls_active"then
        weaponSwitch.clear("falken_tls_active");return
    end
    local pending=weaponSwitch.pending
    if not pending or pending.desired~=desired then
        pending={desired=desired,plane=plane:GetAddress(),activator=activator:GetAddress(),world=currentWorld,
            revision=weaponRevision,started=game:GetTimeSeconds(plane),salvo=weaponSwitch.salvo.service and weaponSwitch.salvo.service.ownedCall or false}
        weaponSwitch.pending=pending
        note("WEAPON_SWITCH_PENDING target="..desired.." reason="..tostring(reason or "native_busy"))
    end
    pending.reason=reason and reason~=""and reason or "native_busy"
    lastUIKey=nil
end
function weaponSwitch.clear(reason)
    if weaponSwitch.pending then note("WEAPON_SWITCH_CANCELLED "..reason);weaponSwitch.pending=nil;lastUIKey=nil end
end
-- 按下/释放始终成对；只有实际类别完成后才向两处 HUD 提交选择。
local function pressSwitch(desired)
    assert(IsInGameThread(),"武器类别切换必须在游戏线程完成")
    internal=true
    local desiredArmed=desired>0 -- 按逻辑目标核对最终类别，不能依赖中间态。
    -- 原生可能因连射或其他短暂条件保持旧类别；释放配对后交回待切换状态。
    local switched,switchError=weaponSwitch.press(plane:GetAddress(),activator:GetAddress())
    assert(switched,switchError)
    lastArmed=plane:IsSpecialWeaponArmed()
    internal=false
    if lastArmed~=desiredArmed then return false end
    selectionAudio(1,desired)
    note("WEAPON_CATEGORY_COMMITTED "..tostring(lastArmed))
    if configured and flightOptions and not supplySession then refreshHUD(os.clock())end
    return true
end
-- MSL/SP 类别转换与 SP 重建共同完成后才刷新，避免暴露中间的 MSL 状态。
local function selectLogicalWeapon(desired,retry)
    if not retry and weaponSwitch.salvo.service and not weaponSwitch.salvo.service.ownedCall then weaponSwitch.salvo.service:cancel("manual_selection")end
    assert(desired>=0 and desired<=3,"无效的逻辑武器选择")
    if not retry then weaponSwitch.clear("superseded")end
    local aircraftCode=catalogue and catalogue[tostring(planeId)]and catalogue[tostring(planeId)].code
    local falken=aircraftCode=="zoef"
    if falken then
        -- 放在ReleaseWeaponInput之前：拒绝切换时不通过释放输入干预正在发射的TLS。
        local ok,canSwitch,reason=weaponSwitch.read(plane:GetAddress(),activator:GetAddress())
        assert(ok,reason or "原生切换状态读取失败")
        if not canSwitch and reason=="falken_tls_active"then
            weaponSwitch.clear("falken_tls_active");lastArmed=plane:IsSpecialWeaponArmed()
            note("WEAPON_SWITCH_REJECTED FALKEN_TLS_ACTIVE");return false
        end
    end
    if not retry then
        releaseWeaponInput("logical_weapon_switch") -- 包括切回MSL；等待期间不重复释放输入。
        -- CFA原生释放只清按住状态，旧待发队列会按切换后的类别消费。仅新请求丢弃旧队列。
        if aircraftCode=="fa44"then
            local prepared,prepareError=weaponSwitch.prepareCfa(plane:GetAddress(),activator:GetAddress())
            assert(prepared,prepareError or "CFA切换准备失败")
        end
    end
    local ok,canSwitch,reason=weaponSwitch.read(plane:GetAddress(),activator:GetAddress())
    assert(ok,reason or "原生切换状态读取失败")
    if not canSwitch then weaponSwitch.defer(desired,reason);return false end
    internal=true
    -- Falken仅在已经自然退场时接受本次按键；其他机型保留既有TLS策略。
    -- 原生循环可能先经过 MSL，仍按最终逻辑目标和当前挂载判断，不能依据中间 armed 状态。
    if not falken and desired>0 and desired~=active+1 and flightOptions and flightOptions[currentProfile[Profile.fields[active+1]]+1].runtime_weapon_id==25 then
        local ended,err=finishTLS(plane:GetAddress(),activator:GetAddress())
        assert(ended,err or "TLS 原生收尾未完成")
    end
    if desired>0 and active~=desired-1 then
        -- 核心恢复 SP 时也必须执行完整飞机切换，保持武器与锁定组件一致。
        native("switch",desired-1)
        if weaponSwitch.pending then weaponSwitch.pending.revision=weaponRevision end
    end
    local armed=plane:IsSpecialWeaponArmed()
    if armed~=(desired>0)then
        if not pressSwitch(desired)then weaponSwitch.defer(desired,"原生暂未允许类别切换");return false end
    else
        lastArmed=armed;internal=false
        selectionAudio(1,desired)
        refreshHUD(os.clock())
    end
    weaponSwitch.pending=nil;lastUIKey=nil
    return true
end
weaponSwitch.salvo.service=dofile(root.."Scripts/gun_salvo.lua").new{
    invoke=weaponSwitch.salvo.invoke,note=note,
    select=function(target)return selectLogicalWeapon(target)end,
    clearPending=function()if weaponSwitch.pending and weaponSwitch.pending.salvo then weaponSwitch.clear("gun_salvo_cancelled")end end,
    context=function()
        local c={enabled=config.gun_salvo_enabled,valid=false,operable=false,eligible={},ammo={}}
        if not c.enabled and not weaponSwitch.salvo.service.bound then return c end
        c.valid=runtimeAllowed and runtimeModEnabled and isFlight and configured and not errorText
            and valid(plane)and valid(activator)and currentProfile~=nil and flightOptions~=nil and nativeState~=nil
        if not c.valid then return c end
        c.plane=plane:GetAddress();c.activator=activator:GetAddress()
        c.key=tostring(currentWorld)..":"..c.plane..":"..c.activator
        c.operable=not panelOpen and not supplySession and not internal and not restorePending and not checkpointPending
            and not(aircraftDebug and aircraftDebug:busy())and not game:IsGamePaused(plane)and plane.bEnableWeaponInput
        c.logical=plane:IsSpecialWeaponArmed()and active+1 or 0;c.revision=weaponRevision;c.time=game:GetTimeSeconds(plane)
        c.pending=weaponSwitch.pending~=nil
        c.pendingOwned=weaponSwitch.pending and weaponSwitch.pending.salvo or false
        for i=1,Profile.count(currentProfile)do
            local option=flightOptions[currentProfile[Profile.fields[i]]+1]
            if option and option.salvo_supported then c.eligible[#c.eligible+1]=i end
            c.ammo[i]=i==active+1 and activator:GetSpecialWeaponCount()or nativeState.cached_sp[i]
        end
        return c
    end}
-- 仅普通连射保留待切换请求；Falken TLS期间的选择已直接丢弃。
function weaponSwitch.pump()
    local pending=weaponSwitch.pending
    if not pending then return end
    if not configured or supplySession or restorePending or checkpointPending or errorText or not isFlight
        or not valid(plane)or not valid(activator)or plane:GetAddress()~=pending.plane
        or activator:GetAddress()~=pending.activator or currentWorld~=pending.world or weaponRevision~=pending.revision then
        weaponSwitch.clear("context_changed");return
    end
    if internal or panelOpen or not ready()or game:IsGamePaused(plane)then return end
    local time=game:GetTimeSeconds(plane)
    if time<pending.started or time-pending.started>5 then weaponSwitch.clear("expired");return end
    if pending.lastCheck and time-pending.lastCheck<.05 then return end
    pending.lastCheck=time
    if pending.desired>0 then
        local ammo=pending.desired==active+1 and activator:GetSpecialWeaponCount()or nativeState.cached_sp[pending.desired]
        if not ammo or not Profile.hasAmmo(ammo)then weaponSwitch.clear("empty_ammo");inputNotice="empty_ammo";return end
    end
    selectLogicalWeapon(pending.desired,true)
end
-- 直接选择只在允许武器操作的任务中执行；已选中目标时不重建。
local function directSelect(desired)
    assert(type(desired)=="number"and desired%1==0 and desired>=0 and desired<=3,"直接选择目标无效")
    if not configured or supplySession or panelOpen or internal or errorText or restorePending or checkpointPending
        or not isFlight or not ready()or game:IsGamePaused(plane)then return end
    inputNotice=nil
    weaponSwitch.salvo.service:cancel("manual_direct_selection")
    weaponSwitch.clear("new_direct_selection")
    if desired>Profile.count(currentProfile)then inputNotice="empty_sp";note("DIRECT_SELECTION_REJECTED EMPTY_SP "..desired);return end
    local armed=plane:IsSpecialWeaponArmed()
    if (armed and active+1 or 0)==desired then
        return
    end
    if desired>0 then
        -- 活动 SP 读取真实弹药，未活动 SP 使用切换时保存的核心计数。
        local ammo=desired==active+1 and activator:GetSpecialWeaponCount()or nativeState.cached_sp[desired]
        if not Profile.hasAmmo(ammo)then inputNotice="empty_ammo";note("DIRECT_SELECTION_REJECTED EMPTY_AMMO "..desired);return end
    end
    note("DIRECT_SELECTION_REQUESTED "..desired)
    selectLogicalWeapon(desired)
end
local function observeOriginalSwitch()
    if not configured then return end
    local cycleOK,cycleRequested=weaponSwitch.takeCycle(plane:GetAddress())
    assert(cycleOK,cycleRequested or "循环切换请求读取失败")
    local requestOK,emptyRequested=takeEmptySwitch(plane:GetAddress()) -- 不可操作期间丢弃过期请求。
    assert(requestOK,emptyRequested or "原生切换请求读取失败")
    if not configured or supplySession or internal or panelOpen or errorText or not ready()or game:IsGamePaused(plane)then return end
    local armed=plane:IsSpecialWeaponArmed()
    -- The live SP count is authoritative; inactive SP counts are captured on
    -- each swap. A consumed last round must not wait for the 100 ms HUD update.
    local ammo={} -- 只包含当前实际槽，末尾空槽不参与循环。
    for index=1,Profile.count(currentProfile)do ammo[index]=nativeState.cached_sp[index]end
    ammo[active+1]=activator:GetSpecialWeaponCount()
    if cycleRequested then
        -- 与十字键走同一选择事务，SP→SP之间不先切到MSL。
        local desired=weaponCycle.next(armed and active+1 or 0,ammo)
        selectLogicalWeapon(desired)
    elseif lastArmed~=nil and armed~=lastArmed then
        local desired=weaponCycle.next(lastArmed and active+1 or 0,ammo)
        selectLogicalWeapon(desired)
    elseif emptyRequested and not armed then
        -- 当前挂载已经耗尽时，原生不改变 armed。只在真实按键后换到还有弹药的 SP。
        local desired=weaponCycle.next(0,ammo)
        if desired>0 then selectLogicalWeapon(desired)end
    else
        -- MSL 沿用刚使用的 SP 挂载；下一次实际选中另一 SP 时才重建模型。
        lastArmed=armed
    end
end
-- 普通调试要求可操控；换机允许借用已核对的原生暂停，补给和检查点交接仍禁止。
local function canDebugLoadout(hotkey,allowNativePause)
    if runtimeReason=="mission13_fixed_sp"then return false,tr("第13关使用任务指定SP，多SP功能暂时停用。","Mission 13 uses its assigned SP. Multi-SP is temporarily disabled.")end
    if aircraftDebug and aircraftDebug:busy()then return false,tr("正在重开任务换机，请稍候。","Restarting the mission with another aircraft; please wait.")end
    if not runtimeAllowed or not isFlight or not configured then return false,tr("请先进入已配置兵装的单人任务。","Enter a configured single-player mission first.")end
    if errorText or managementPauseError then return false,tr("控制器或暂停交接出错，请查看上方错误。","Controller or pause handoff failed; see the error above.")end
    if supplySession or internal or restorePending or checkpointPending then return false,tr("正在补给或交接任务状态，请完成后再打开。","Resupply or a mission transition is in progress.")end
    local nativePause=allowNativePause and existingManagementPauseValid(managementPause)
    if not valid(instance)or not valid(plane)or not valid(activator)or(not ready()and not nativePause)then return false,tr("当前飞行控制尚未开放。","Flight controls are not ready.")end
    if hotkey and not panelOpen then
        if game:IsGamePaused(plane)then return false,tr("请先退出游戏暂停菜单。","Close the pause menu first.")end
    elseif not panelOpen or not game:IsGamePaused(plane)then return false,tr("正在等待管理界面暂停任务。","Waiting for management to pause the mission.")end
    return true
end
aircraftDebug=dofile(root.."Scripts/debug_aircraft.lua").new{
    json=json,copy=copyProfile,names=names,note=note,
    defaultProfile=function(options)return Profile.debugDefault(options,config)end,
    owned=loadExport("dualsp_debug_owned_aircraft"),
    changed=function()lastUIKey=nil end,
    game=function()return game end,catalogue=function()return catalogue end,
    nativeOnly=function(id,info)return modAircraft:nativeOnlyReason(id,info)end,
    allowed=function()return runtimeAllowed and runtimeModEnabled and runtimeReason~="mission13_fixed_sp"end,
    matches=isSelectedPlane,
    bind=function(pawn,map)cachedPlane=pawn;cachedWorld=map end,
    context=function()return {instance=instance,plane=plane,id=planeId,profile=currentProfile,session=debugSession,revision=weaponRevision}end,
    canChange=function()
        local allowed,reason=canDebugLoadout(false,true);if not allowed then return false,reason end -- 暂停可停用飞行输入，但整关重开不依赖该输入。
        if not managementPause or not(managementPause.owned or existingManagementPauseValid(managementPause))then
            return false,tr("暂停状态已变化，请关闭后重新打开管理界面换机。","Pause state changed. Close and reopen management to change aircraft.")
        end
        return true
    end,
    failFlight=function(message)errorText=tostring(message);releaseRuntimeOwnership()end,
    prepare=function()
        local pause=managementPause
        local fromNativePause=pause and pause.existing and not pause.owned
        local resumeManager=nil
        if fromNativePause then
            assert(existingManagementPauseValid(pause),"原生暂停身份已变化，未发出重开请求")
            -- SetGamePaused只改世界暂停，不能撤销GI暂停管理器保留的菜单原因；必须走原生关菜单/恢复路径。
            for _,name in ipairs({"/Script/Nimbus.UIManagerActor:GetCurrentUIManagerActor",
                "/Script/Nimbus.UIManagerActor:CloseMenuAndResumePlay"})do
                assert(valid(StaticFindObject(name)),"原生暂停菜单交接接口缺失："..name)
            end
            local defaultManager=StaticFindObject("/Script/Nimbus.Default__UIManagerActor")
            assert(valid(defaultManager),"原生暂停菜单管理器缺失")
            resumeManager=defaultManager:GetCurrentUIManagerActor()
            assert(valid(resumeManager)and valid(resumeManager:GetWorld())
                and resumeManager:GetWorld():GetAddress()==pause.world,"原生暂停菜单不属于当前换机世界")
        end
        setPanel(false)
        assert(not panelOpen and not managementPause and not managementPauseError,"管理界面暂停没有归还，未发出重开请求")
        if fromNativePause then
            -- 先保留原暂停，只有目标选择完成、即将重开时才通过原生接口交接。
            assert(existingManagementPauseValid(pause),"原生暂停身份已变化，未发出重开请求")
            note("DEBUG_AIRCRAFT_RESTART_FROM_NATIVE_PAUSE")
        else assert(not game:IsGamePaused(plane),"原生任务仍在暂停，请先退出暂停菜单")end
        requestClose=requestClose+1
        releaseWeaponInput("debug_aircraft")
        releaseRuntimeOwnership();nativeHud:reset();weaponPresentation:reset()
        -- 整关换机不携带旧机体的库存和检查点；原生重开负责生成玩家与任务系统。
        local ok,err=nativeRuntimeGate(0);assert(ok,err)
        ok,err=nativeRuntimeGate(1);assert(ok,err)
        resetCheckpointState();resetFlightTracking()
        return function(owner)
            local targetId,targetWeapon=instance:GetSelectedPlaneID(),instance:GetSelectedPlaneSWP()
            if fromNativePause then
                -- 仅明确换机才消费原生暂停菜单；普通关闭F6仍保留原菜单和暂停。
                assert(existingManagementPauseValid(pause),"重开前原生暂停身份已变化")
                assert(valid(resumeManager)and valid(resumeManager:GetWorld())
                    and resumeManager:GetWorld():GetAddress()==pause.world,"重开前原生暂停菜单身份已变化")
                resumeManager:CloseMenuAndResumePlay()
                assert(valid(pause.settings)and valid(pause.settings:GetWorld())
                    and pause.settings:GetWorld():GetAddress()==pause.world,"关闭暂停菜单时任务世界已变化，未重开任务")
                assert(not valid(pause.settings.Pauser)and not game:IsGamePaused(pause.settings),
                    "原生菜单暂停没有完成归还，未重开任务") -- 其他暂停原因仍存在时拒绝，不强行清除。
                note("DEBUG_AIRCRAFT_NATIVE_MENU_RESUMED world="..pause.world)
            end
            assert(instance:GetSelectedPlaneID()==targetId and instance:GetSelectedPlaneSWP()==targetWeapon,
                "关闭暂停菜单时目标机体选择已变化，未重开任务") -- 原生关菜单可能同步回调，不能让它撤回已核对的目标。
            -- 原生菜单已恢复后，重开失败交给外层恢复原机体选择；不能只用SetGamePaused(true)伪造旧菜单。
            owner:RestartLevel()
        end
    end}
-- 用户明确应用才换装；输入/过期请求拒绝与实际重建失败分开处理。
local function applyDebugLoadout(action)
    local profile=nil -- 校验完成前不替换当前方案。
    local ok,err=xpcall(function()
        assert(type(action.request)=="number"and action.request%1==0 and action.request>debugResponse,"调试请求编号过期或无效")
        assert(action.debug_protocol==1,"调试界面协议不匹配，请成套安装")
        assert(canDebugLoadout(),"当前不能调试换装：请在可操控的单人飞行中直接打开管理界面")
        assert(action.session==debugSession and action.revision==weaponRevision,"任务或兵装已改变，请重新打开调试页")
        assert(instance:GetWorld():GetAddress()==plane:GetWorld():GetAddress()and instance:GetSelectedPlaneID()==planeId,
            "当前任务机体身份已经改变")
        profile=Profile.withSettings(assert(action.profile,"缺少调试方案"),config)
        Profile.validate(profile,optionsFor(planeId),true) -- 任务调试允许未购买SP，仍核对实际类及多挂资格。
        local allowed,why=debugLoadoutReady(plane:GetAddress(),activator:GetAddress()) -- 有活动武器时明确拒绝，不排队偷换。
        assert(allowed,why)
    end,debug.traceback)
    if type(action.request)=="number"and action.request%1==0 and action.request>debugResponse then debugResponse=action.request end
    lastUIKey=nil
    if not ok then debugError=tostring(err);note("DEBUG_LOADOUT_REJECTED "..debugError);return end
    debugError=nil
    note("DEBUG_LOADOUT_BEGIN request="..action.request.." session="..debugSession.." from="..json.encode(currentProfile).." to="..json.encode(profile))
    -- 此后错误进入既有控制器故障处理，不能显示成功或继续使用半完成的新状态。
    configureFlight(profile,"debug")
    debugAck=action.request
    note("DEBUG_LOADOUT_APPLIED request="..debugAck.." profile="..json.encode(profile))
end
local function applyDebugHotkey(action)
    if type(action.request)~="number"or action.request%1~=0 or action.request<=hotkeyResponse then return end
    hotkeyResponse=action.request;lastUIKey=nil
    weaponSwitch.hotkeyWarning=true -- 拒绝/失败默认归为警告，明确成功后才降为普通提示。
    if runtimeReason=="mission13_fixed_sp"then
        hotkeyNotice=tr("第13关使用任务指定SP，多SP功能暂时停用。","Mission 13 uses its assigned SP. Multi-SP is temporarily disabled.");return
    end
    if aircraftDebug and aircraftDebug:busy()then hotkeyNotice=tr("正在重开任务换机，请稍候。","Restarting with another aircraft; please wait.");return end
    local profile=nil
    local accepted,why=pcall(function()
        assert(action.hotkey_protocol==1,"快捷键协议不匹配")
        if action.command=="mount"then return end -- 全局外观设置不依赖飞行、暂停、旧兵装版本或活动武器。
        assert(runtimeModEnabled,"本模组已关闭")
        assert(action.session==debugSession and action.revision==weaponRevision,"任务或兵装已改变，请重新按键")
        local allowed,reason=canDebugLoadout(true);assert(allowed,reason)
        assert(instance:GetWorld():GetAddress()==plane:GetWorld():GetAddress()and instance:GetSelectedPlaneID()==planeId,"当前任务机体身份已改变")
        local safe,reason=debugLoadoutReady(plane:GetAddress(),activator:GetAddress());assert(safe,reason)
        if action.command=="cycle"then
            profile=Profile.nextCombination(currentProfile,optionsFor(planeId),config,true)
            assert(profile,"没有其他可用 SP 组合")
        else assert(action.command=="mount","未知调试快捷操作")end
    end)
    if not accepted then hotkeyNotice=tostring(why);note("DEBUG_HOTKEY_REJECTED "..hotkeyNotice);return end
    if profile then
        configureFlight(profile,"debug_hotkey") -- 同一游戏线程完整重建，错误交给既有故障清理。
        local names={}
        for _,field in ipairs(Profile.fields)do if profile[field]>=0 then names[#names+1]=flightOptions[profile[field]+1].code:upper()end end
        hotkeyNotice="SP："..table.concat(names," + ")..tr("（已补满）"," (refilled)")
        weaponSwitch.hotkeyWarning=false
    else
        local nextConfig=json.decode(json.encode(config));nextConfig.task_mount_enabled=not config.task_mount_enabled
        local saved,err=pcall(saveConfig,nextConfig)
        if not saved then hotkeyNotice=tostring(err);note("DEBUG_HOTKEY_REJECTED "..hotkeyNotice);return end
        config=nextConfig
        -- 任务中直接重绑；机库/补给由预览接管，构造/检查点阶段沿既有重建边界应用。
        local applied,err=pcall(function()
            if runtimeAllowed and runtimeModEnabled and isFlight and configured and not supplySession
                and not internal and not restorePending and not errorText
                and valid(plane)and valid(activator)and valid(instance)
                and instance:GetWorld():GetAddress()==plane:GetWorld():GetAddress()then
                if config.task_mount_enabled and ensureMountAssets then ensureMountAssets(nil,plane,true,currentProfile)end
                local ok,warning=debugFixedMount(plane:GetAddress(),activator:GetAddress(),game:GetTimeSeconds(plane),config.task_mount_enabled and 1 or 0)
                assert(ok,warning)
                runtimeFixedMountEnabled=config.task_mount_enabled;mountOptionsApplied=true
                mountWarning=warning and warning~=""and warning or nil
            elseif not isFlight and not supplySession then applyFixedMountSetting(config.hangar_mount_enabled)
            else applyFixedMountSetting()end
            previewMountWarning=nil;supplyPreviewActive=false
            if runtimeFixedMountEnabled and lastMountPreviewContext and not isFlight then mountPreview(lastMountPreviewContext)end
            if supplySession and supplySession.menuClosed then maintainSupplyPreview()end
        end)
        if not applied then
            -- 外观接口失败独立回退，不把错误升级为兵装控制器故障。
            pcall(setFixedMountEnabled,0)
            runtimeFixedMountEnabled=false;mountOptionsApplied=true
            mountWarning="多 SP 挂载已回退为原生单 SP："..tostring(err)
        end
        hotkeyNotice=mountWarning or (config.task_mount_enabled and tr("任务多SP模型：开启","Mission multi-SP models: ON")or tr("任务多SP模型：关闭","Mission multi-SP models: OFF"))
        weaponSwitch.hotkeyWarning=mountWarning~=nil
        if not runtimeModEnabled then hotkeyNotice=hotkeyNotice..tr("（总开关关闭，仅保存设置）"," (saved; mod disabled)")end
    end
    note("DEBUG_HOTKEY_APPLIED "..action.command.." "..hotkeyNotice)
end
function debugWeapons.update()
    local allowed=runtimeAllowed and runtimeModEnabled and runtimeReason~="mission13_fixed_sp"and isFlight and configured
        and not supplySession and not internal and not restorePending and not checkpointPending and not errorText
        and not(aircraftDebug and aircraftDebug:busy())and valid(plane)and valid(activator)
    local ok,err=debugWeapons.invoke(allowed and plane:GetAddress()or 0,allowed and activator:GetAddress()or 0,allowed and 1 or 0,
        debugWeapons.infinite and 1 or 0,debugWeapons.fast and 1 or 0,allowed and game:GetTimeSeconds(plane)or 0)
    if not ok then
        debugWeapons.infinite=false;debugWeapons.fast=false
        if debugWeapons.error~=tostring(err)then note("DEBUG_WEAPONS_ERROR "..tostring(err))end
        debugWeapons.error=tostring(err)
    end
    return ok,err
end
function debugWeapons.apply(action)
    debugWeapons.response=tonumber(action.request)or 0
    local ok,err=pcall(function()
        assert(action.protocol==1 and action.session==debugSession and action.revision==weaponRevision,"调试界面协议不匹配，请成套安装")
        local allowed,reason=canDebugLoadout(false,true);assert(allowed,reason)
        assert(type(action.infinite)=="boolean"and type(action.fast)=="boolean","SP调试参数无效")
        debugWeapons.infinite=action.infinite;debugWeapons.fast=action.fast
        local applied,detail=debugWeapons.update();assert(applied,detail)
        debugWeapons.ack=debugWeapons.response;debugWeapons.error=nil
        note("DEBUG_WEAPONS_APPLIED request="..debugWeapons.ack.." plane="..planeId.." session="..debugSession
            .." revision="..weaponRevision.." infinite="..tostring(debugWeapons.infinite).." fast="..tostring(debugWeapons.fast))
    end)
    if not ok then debugWeapons.error=tostring(err);note("DEBUG_WEAPONS_REJECTED "..tostring(err))end
    lastUIKey=nil
end
local function applyAction(action)
    if action.type=="open"then setPanel(true)
    elseif action.type=="close"then setPanel(false)
    elseif action.type=="select_weapon"then
        if runtimeModEnabled and (action.source=="keyboard"or action.source=="dpad"and config.dpad_weapon_switch_enabled)then directSelect(action.target)end
    elseif action.type=="toggle_camera_hold"then
        if weaponSwitch.gaze.canSwitch()then
            local saved,err=pcall(function()
                config=configStore:setCameraHold(config,not config.camera_hold_enabled)
                weaponSwitch.camera.error=nil;weaponSwitch.camera.update()
                note("CAMERA_HOLD_KEY_TOGGLED "..tostring(config.camera_hold_enabled))
            end)
            if not saved then weaponSwitch.camera.error=tostring(err);note("CAMERA_HOLD_TOGGLE_SAVE_ERROR "..tostring(err))end
            lastUIKey=nil
        end
    elseif action.type=="debug_loadout"then if runtimeModEnabled then applyDebugLoadout(action)end
    elseif action.type=="debug_weapons"then debugWeapons.apply(action)
    elseif action.type=="debug_aircraft"then aircraftDebug:request(action)
    elseif action.type=="toggle_gun_salvo"then
        if runtimeAllowed and runtimeModEnabled then
            local saved,err=pcall(function()
                local nextConfig=json.decode(json.encode(config));nextConfig.gun_salvo_enabled=not config.gun_salvo_enabled
                configStore:save(nextConfig);config=nextConfig
                weaponSwitch.salvo.service.error=nil
                note("GUN_SALVO_TOGGLED "..tostring(config.gun_salvo_enabled))
            end)
            if not saved then weaponSwitch.salvo.service.error=tostring(err)end
            lastUIKey=nil
        end
    elseif action.type=="debug_hotkey"then applyDebugHotkey(action)
    elseif action.type=="settings"then
        -- 普通设置保存不触发调试换装；构造完整新设置后验证并保存。
        local ok,err=xpcall(function()
            config=configStore:applySettings(config,action)
            if not isFlight and not supplySession and not config.hangar_mount_enabled then
                releaseMountPreview(0);lastMountPreviewContext=nil -- 保存成功后立即归还机库模型，不触碰任务/补给预览。
            end
            weaponSwitch.salvo.service.failed=false;weaponSwitch.salvo.service.error=nil
            weaponSwitch.gaze.failed=false;weaponSwitch.gaze.error=nil;weaponSwitch.gaze.update(false)
            weaponSwitch.camera.release();weaponSwitch.camera.failed=false;weaponSwitch.camera.error=nil;weaponSwitch.camera.update()
            weaponSwitch.view.release(true);weaponSwitch.view.failed=false;weaponSwitch.view.error=nil;weaponSwitch.view.update()
            masterSwitch.failed=false;masterSwitch.error=nil;masterSwitch.lastCheck=-1
            tryApplyMasterSwitch(true)
            if not isFlight and not supplySession then applyFixedMountSetting(config.hangar_mount_enabled)end
            settingsAck=action.request;settingsError=nil;inputNotice=nil
            lastUIKey=nil;note("MANAGEMENT_SETTINGS_SAVED request="..settingsAck.." "..json.encode({
                language=config.language,mod=config.mod_enabled,task_models=config.task_mount_enabled,hangar_models=config.hangar_mount_enabled,
                dpad=config.dpad_weapon_switch_enabled,salvo=config.gun_salvo_enabled,gaze_pad=config.target_gaze_pad_enabled,
                gaze_keyboard=config.target_gaze_keyboard_enabled,camera_hold=config.camera_hold_enabled,view_target=config.view_target_mode,
                ammo_two=config.ammo_percent_two,ammo_three=config.ammo_percent_three}))
        end,debug.traceback)
        settingsResponse=action.request
        if not ok then settingsError=tostring(err);lastUIKey=nil;note("MANAGEMENT_SETTINGS_ERROR "..settingsError)end
    else error("Unknown UI action: "..tostring(action.type))end
end
local function locatePlane()
    local worldContext=instance:GetWorld()
    if not valid(worldContext)then return nil,""end
    local contextName=worldContext:GetFullName() -- 菜单和简报世界无需反复扫描玩家飞机。
    if not contextName:find("/Game/Maps/Mission/",1,true)and not contextName:find("/Game/Maps/FreeFlight/",1,true)then return nil,""end
    -- 开场、重启和新世界先沿原生玩家指针取得实例，避免等全局对象扫描周期。
    local player=game:GetPlayerPawn(instance,0)
    if valid(player)and player:IsA(StaticFindObject("/Script/Nimbus.PlayerPlane"))
        and player:GetWorld():GetAddress()==worldContext:GetAddress()then return player,contextName end
    if os.clock()-lastPlaneScan<=.25 then return nil,contextName end
    lastPlaneScan=os.clock() -- 仅未被控制器持有的构造阶段需要低频全局搜索。
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
    local appliedTaskMount=config.task_mount_enabled
    if isFlight or supplySession then appliedTaskMount=runtimeFixedMountEnabled end -- 机库核心状态不能冒充任务开关的应用状态。
    local info=catalogue and catalogue[tostring(planeId)]
    -- 普通管理参数与显式调试事务分开发布，渲染线程不接触玩家对象。
    local state={schema=1,scene=isFlight and "flight"or"menu",plane_id=planeId,plane_name=info and (names[info.code]or info.code:upper())or tr("请选择机体","Select an aircraft"),
        configured=configured,state=nativeState,request_open=requestOpen,request_close=requestClose,
        management_protocol=21,shortcuts=config.shortcuts,language=config.language,game_language=runtimeContext.language,
        mission_fixed_sp=runtimeReason=="mission13_fixed_sp",
        mod_enabled=config.mod_enabled,task_mount_enabled=config.task_mount_enabled,gun_salvo_enabled=config.gun_salvo_enabled,
        hangar_mount_enabled=config.hangar_mount_enabled,dpad_weapon_switch_enabled=config.dpad_weapon_switch_enabled,
        view_target_mode=config.view_target_mode,view_center_dot_enabled=config.view_center_dot_enabled,
        view_target={protocol=3,available=weaponSwitch.view.available,error=weaponSwitch.view.error},
        gun_salvo={available=weaponSwitch.salvo.service.available,reason=weaponSwitch.salvo.service.reason,error=weaponSwitch.salvo.service.error},central_weapon_display=config.central_weapon_display,
        target_gaze_pad_enabled=config.target_gaze_pad_enabled,target_gaze_keyboard_enabled=config.target_gaze_keyboard_enabled,camera_hold_enabled=config.camera_hold_enabled,
        camera_center_key=config.camera_center_key,camera_sensitivity=config.camera_sensitivity,
        runtime_mod_enabled=runtimeModEnabled,runtime_task_mount_enabled=appliedTaskMount,
        ammo_percent_two=config.ammo_percent_two,ammo_percent_three=config.ammo_percent_three,
        keyboard_keys=config.keyboard_keys,management_key=config.management_key,ammo_mode="percentage",
        mount_warning=previewMountWarning and (mountWarning and mountWarning.." / "..previewMountWarning or previewMountWarning)or mountWarning,
        settings_ack=settingsAck,settings_response=settingsResponse,settings_error=settingsError,input_notice=inputNotice}
    state.weapon_switch={pending=weaponSwitch.pending~=nil,target=weaponSwitch.pending and weaponSwitch.pending.desired or nil,
        reason=weaponSwitch.pending and weaponSwitch.pending.reason or nil}
    state.direct_selection_available=isFlight and configured and not supplySession and not panelOpen and not internal and not errorText
        and not restorePending and not checkpointPending
        and ready()and not game:IsGamePaused(plane)
    local debugAvailable,debugReason=canDebugLoadout() -- 界面禁用原因与最终提交检查共用同一判断。
    state.debug_loadout={protocol=1,session=debugSession,revision=weaponRevision,available=debugAvailable,reason=debugReason,
        response=debugResponse,ack=debugAck,error=debugError,allow_unowned=true,profile=configured and currentProfile or nil,options=flightOptions}
    local debugWeaponsAvailable=canDebugLoadout(false,true)
    state.debug_weapons={protocol=1,available=debugWeaponsAvailable,infinite=debugWeapons.infinite,fast=debugWeapons.fast,
        response=debugWeapons.response,ack=debugWeapons.ack,error=debugWeapons.error}
    state.debug_hotkey={protocol=1,response=hotkeyResponse,notice=hotkeyNotice,notice_warning=weaponSwitch.hotkeyWarning==true}
    state.target_gaze={protocol=4,switch_available=weaponSwitch.gaze.canSwitch(),active=weaponSwitch.gaze.active,available=weaponSwitch.gaze.available,
        generation=weaponSwitch.gaze.generation,response=weaponSwitch.gaze.response,notice=weaponSwitch.gaze.notice,notice_warning=weaponSwitch.gaze.noticeWarning==true,error=weaponSwitch.gaze.error}
    state.camera_hold={protocol=8,available=weaponSwitch.camera.available,error=weaponSwitch.camera.error}
    state.debug_aircraft=aircraftDebug:state(debugSession,weaponRevision)
    state.master_switch={pending=config.mod_enabled~=runtimeModEnabled,error=masterSwitch.error,response=masterSwitch.response,notice=masterSwitch.notice,
        reason=tr("请返回主菜单或单人任务选择页，保存的总开关会自动生效。","Return to the main menu or single-player mission selection; the saved master switch will apply automatically.")}
    state.management_pause={panel_open=panelOpen,owned=managementPause and managementPause.owned or false,
        error=managementPauseError,
        existing=managementPause and managementPause.existing or false,
        effective=isFlight and valid(plane)and game:IsGamePaused(plane)or false,
        weapon_input=isFlight and ready()and true or false} -- 明确区分面板、原生暂停与武器输入，便于复核不可选原因。
    state.sequence=publishSequence
    state.native_hud=nativeHud.status
    state.weapon_presentation=weaponPresentation.status
    state.native_loadout=nativeLoadout.status
    state.native_resupply=nativeSupply.status
    if nativeLoadout.error then state.menu_error=nativeLoadout.error end
    if nativeSupply.error then state.menu_error=nativeSupply.error end
    if nativeStandby.error then state.menu_error=nativeStandby.error end
    if nativeParts.error then state.menu_error=nativeParts.error end
    if nativeHud.error or hudSamplingError then state.hud_error=nativeHud.error or hudSamplingError end
    if weaponPresentation.error then state.central_error=weaponPresentation.error end
    if errorText then state.error=errorText;state.state=nil end
    local ok,err=publish(json.encode(state));assert(ok,err)
end
-- 禁用先关闭原生与渲染侧入口，再释放自己的控件和事务；不改多人兵装或配置。
local function checkRuntime(activatedWidget)
    if runtimeChanging then return false end
    local allowed,reason=runtimeContext.allowed(instance) -- 每次重新检查，避免事件早于周期更新。
    if activatedWidget and runtimeContext.isMultiplayerWidget(activatedWidget)then
        runtimeContext.enterMultiplayerMenu(instance);allowed=false;reason="multiplayer_menu"
    end
    if allowed then
        local id=instance:GetSelectedPlaneID()
        local info=catalogue and catalogue[tostring(id)]
        local passive=modAircraft:nativeOnlyReason(id,info)
        if passive then
            allowed=false;reason="aircraft_native_only"
            modAircraft:warn("native_only/"..id,"plane="..id.." code="..tostring(info and info.code).." reason="..passive)
        end
    end
    modAircraft.safetyPending=false
    if allowed==runtimeAllowed and reason==runtimeReason then return allowed end
    runtimeChanging=true
    runtimeAllowed=allowed;runtimeReason=reason
    local gazeOK,_,gazeError=weaponSwitch.gaze.invoke(0,0,0,0)
    assert(gazeOK,gazeError);weaponSwitch.gaze.active=false;weaponSwitch.gaze.available=false
    weaponSwitch.camera.release()
    weaponSwitch.view.release()
    local uiOK,uiErr=uiRuntimeGate(allowed and 1 or 0)
    assert(uiOK,uiErr)
    if not allowed or reason=="mission13_fixed_sp"then
        if reason=="mission13_fixed_sp"then aircraftDebug:cancel("第13关使用任务指定SP，已取消调试换机")end
        if reason=="aircraft_native_only"then aircraftDebug:cancel("当前机体暂由游戏原生处理，已取消调试换机")end
        if reason=="network_world"or reason=="multiplayer_mode"or reason=="multiplayer_menu"or reason=="multiplayer_menu_transition"then
            aircraftDebug:cancel("已离开单人任务，换机请求已取消")
        end
        releaseMountPreview(0)
        local coreOK,coreErr=nativeRuntimeGate(0) -- 清除音效、输入、旧玩家及检查点身份。
        assert(coreOK,coreErr)
        if panelOpen then setPanel(false)end
        nativeLoadout:reset{clearError=true,releaseInput=reason=="mission13_fixed_sp"or reason=="aircraft_native_only"};nativeSupply:reset{clearError=true,releaseInput=false}
        nativeStandby:reset{clearError=true,releaseInput=false};nativeParts:reset{clearError=true,releaseInput=false}
        nativeHud:reset();weaponPresentation:reset()
        supplySession=nil;resetCheckpointState()
        cachedPlane=nil;cachedWorld="";plane=nil;activator=nil;planeAddress=nil;currentWorld=""
        resetFlightTracking();isFlight=false;lastPlaneScan=-1
        -- 已生成但未消费的动作属于上一模式，只丢弃，不在返回单人后重放。
        drain() -- 领取并丢弃内存队列，不留旧模式动作。
    else
        local coreOK,coreErr=nativeRuntimeGate(runtimeModEnabled and 1 or 0)
        assert(coreOK,coreErr);errorText=nil;lastUIKey=nil
    end
    note("RUNTIME_MODE "..(allowed and "enabled "or "disabled ")..reason.." "..runtimeContext.describe(instance))
    runtimeChanging=false
    return allowed
end
-- 反射事件也共用显式失败边界，防止错误跨出引擎回调并留下上次的单人许可。
syncRuntime=function(activatedWidget)
    if runtimeFailed or runtimeChanging then return false end
    local ok,result=xpcall(function()return checkRuntime(activatedWidget)end,debug.traceback)
    if ok then return result end
    runtimeFailed=true;runtimeAllowed=false;runtimeChanging=true
    releaseMountPreview(0)
    errorText=tostring(result);note("RUNTIME_CONTEXT_ERROR "..errorText)
    local uiOK,uiErr=uiRuntimeGate(0);local coreOK,coreErr=nativeRuntimeGate(0)
    if not uiOK or not coreOK then note("RUNTIME_DISABLE_ERROR "..tostring(uiErr).." "..tostring(coreErr))end
    if panelOpen then
        local closed,err=pcall(function()setPanel(false)end)
        if not closed then note("RUNTIME_PANEL_RESTORE_ERROR "..tostring(err))end
    end
    -- 识别失败也释放已有自定义控件；分别记录清理错误，不让一个模块阻断其他模块收尾。
    for _,entry in ipairs({{"loadout",nativeLoadout},{"supply",nativeSupply},{"standby",nativeStandby},
        {"parts",nativeParts},{"hud",nativeHud},{"central",weaponPresentation}})do
        local cleaned,err=pcall(function()entry[2]:reset{clearError=true,releaseInput=false}end)
        if not cleaned then note("RUNTIME_VIEW_RESTORE_ERROR "..entry[1].." "..tostring(err))end
    end
    runtimeChanging=false
    return false
end
-- 不在任务、补给、机库草稿或过渡世界切换；保存的目标会在安全入口页自动应用。
tryApplyMasterSwitch=function(force)
    if config.mod_enabled==runtimeModEnabled or masterSwitch.failed then return end
    local now=os.clock()
    if not force and now-masterSwitch.lastCheck<.25 then return end
    masterSwitch.lastCheck=now
    if not runtimeAllowed or runtimeChanging or isFlight or supplySession or internal or managementPause
        or nativeLoadout:isPreparationHandoffPending()or not runtimeContext.canToggleMaster(instance,gatedWidgets)then return end
    runtimeChanging=true
    local target=config.mod_enabled
    local ok,err=xpcall(function()
        runtimeModEnabled=false -- 清理期间事件和原生钩子先保持停用。
        local disabled,why=nativeRuntimeGate(0);assert(disabled,why)
        local released,warning=releaseMountPreview(0);assert(released,warning)
        for _,adapter in ipairs({nativeLoadout,nativeSupply,nativeStandby,nativeParts,nativeHud,weaponPresentation})do
            adapter:reset{clearError=true,releaseInput=false}
        end
        resetCheckpointState();resetFlightTracking()
        cachedPlane=nil;cachedWorld="";plane=nil;activator=nil;planeAddress=nil;currentWorld="";isFlight=false;lastPlaneScan=-1
        lastMountPreviewContext=nil;scenePreviewRequested=false;previewMountWarning=nil;mountWarning=nil;errorText=nil
        debugError=nil;inputNotice=nil;settingsError=nil;mountOptionsApplied=false
        runtimeModEnabled=target
        local enabled,why=nativeRuntimeGate(target and 1 or 0);assert(enabled,why)
        applyFixedMountSetting(config.hangar_mount_enabled)
    end,debug.traceback)
    if not ok then
        runtimeModEnabled=false;pcall(nativeRuntimeGate,0);pcall(setFixedMountEnabled,0)
        masterSwitch.failed=true;masterSwitch.error=tostring(err)
        note("MASTER_SWITCH_ERROR "..masterSwitch.error)
    else
        masterSwitch.error=nil
        masterSwitch.response=masterSwitch.response+1
        masterSwitch.notice=runtimeModEnabled and tr("Mod总开关：开启","Mod: enabled")or tr("Mod总开关：关闭","Mod: disabled")
        note("MASTER_SWITCH_APPLIED "..tostring(runtimeModEnabled))
    end
    runtimeChanging=false;lastUIKey=nil
end
local function tick()
    if not valid(instance)then instance=FindFirstOf("NimbusGameInstance")end
    if not mountOptionsApplied then
        if isFlight or supplySession then applyFixedMountSetting()else applyFixedMountSetting(config.hangar_mount_enabled)end
    end
    local contextStarted=os.clock()
    local allowed=syncRuntime()
    recordStage("context",contextStarted)
    -- 载入时单人许可可能短暂消失；换机只保留数据，不在过渡世界调用机体接口。
    if aircraftDebug:pump(runtimeReason)then
        mountStartup.cancel()
        weaponSwitch.view.release()
        if allowed and (not lastUIKey or os.clock()-lastPublish>=.25)then lastPublish=os.clock();lastUIKey="aircraft_restart";publishState()end
        return
    end
    if not allowed then mountStartup.cancel();return end
    tryApplyMasterSwitch(false)
    if runtimeContext.updateLanguage(os.clock())then
        lastUIKey=nil;note("GAME_TEXT_CULTURE "..runtimeContext.culture)
        nativeLoadout:languageChanged();nativeSupply:languageChanged()
    end
    if not runtimeModEnabled or runtimeReason=="mission13_fixed_sp"then
        mountStartup.cancel()
        releaseMountPreview(0)
        -- 停用时只留下重新启用所需的管理入口，不接入页面、武器、HUD 或模型。
        if not game then game=StaticFindObject("/Script/Engine.Default__GameplayStatics")end
        local now=os.clock()
        if now-lastPlaneScan>.25 then
            local found,world=locatePlane();lastPlaneScan=now
            if (found and found:GetAddress()or nil)~=planeAddress and panelOpen then setPanel(false)end
            plane=found;planeAddress=found and found:GetAddress()or nil
            activator=found and found.PlayerWeaponActivator or nil;isFlight=found~=nil;currentWorld=world
        end
        weaponSwitch.gaze.update(false);weaponSwitch.camera.update();weaponSwitch.view.update()
        if now-lastActions>=.05 then
            lastActions=now
            local encoded=assert(drain(),"动作内存接口没有返回结果")
            if encoded~=""then
                local batch=json.decode(encoded)
                for _,action in ipairs(batch)do applyAction(action)end
            end
        end
        if not lastUIKey or now-lastPublish>=(panelOpen and .1 or 1)then
            lastPublish=now;lastUIKey="disabled";publishState()
        end
        return
    end
    -- 返回主菜单明确结束本轮补给，防止旧事务阻止下一次任务配置。
    if supplySession then
        local worldContext=instance:GetWorld()
        if valid(worldContext)and worldContext:GetFullName():find("/Game/Maps/Menu/",1,true)then
            nativeSupply:reset{clearError=true,releaseInput=false};ownSupplyInput(false);supplySession=nil;checkpointProfile=nil
            note("RESUPPLY_CANCELLED_LEAVE_MISSION")
        end
    end
    if not game then game=StaticFindObject("/Script/Engine.Default__GameplayStatics")end
    -- 先更新短提示生命周期，再处理切换；暂停和菜单中的提前返回也不会留下外部声音。
    updateCustomAudio()
    if not catalogue then
        local info=instance.GameInformation;if not valid(info)or not valid(info.PlaneDataTable)or not valid(info.WeaponDataTable)then return end
        local skinTable=info.SkinDataTable
        catalogue=json.decode(assert(catalogExport(info.PlaneDataTable:GetAddress(),info.WeaponDataTable:GetAddress(),
            valid(skinTable)and skinTable:GetAddress()or 0),"机体目录内存接口没有返回结果"))
        assert(not catalogue.error,catalogue.error)
        assert(catalogue._compat and catalogue._compat.schema==1,"机体兼容目录协议不匹配");note("CATALOGUE_READY")
        if not syncRuntime()then return end -- 目录刚就绪也先按纯数据退让，不先碰特殊机体/兵装对象。
    end
    -- A live player actor remains valid for its world. Registry searches are
    -- needed while acquiring a new actor, not four times per second in flight.
    if cachedPlane then
        local worldContext=instance:GetWorld()
        if not valid(cachedPlane)or not valid(worldContext)or cachedPlane:GetWorld():GetAddress()~=worldContext:GetAddress()then cachedPlane=nil;lastPlaneScan=-1 end
    end
    if not cachedPlane then
        cachedPlane,cachedWorld=locatePlane()
    end
    local found,world=cachedPlane,cachedWorld
    local address=found and found:GetAddress()or nil
    if address~=planeAddress then
        releaseMountPreview(0)
        debugSession=debugSession+1;debugError=nil -- 新实例即新调试上下文，旧请求不再有效。
        -- No stale actor pointers or cached loadout cross a world transition.
        local mountOK,mountError=updateFixedMount(0,0) -- 新世界不能继续持有旧模型身份。
        assert(mountOK,mountError)
        if panelOpen then setPanel(false)end
        selectionAudio(2)
        nativeHud:reset()
        weaponPresentation:reset()
        nativeLoadout:reset{clearError=true,releaseInput=false}
        nativeStandby:reset{clearError=true,releaseInput=false}
        nativeParts:reset{clearError=true,releaseInput=false}
        plane=found;planeAddress=address;activator=found and found.PlayerWeaponActivator or nil
        resetFlightTracking()
        if not supplySession then errorText=aircraftDebug.blocked end
        isFlight=found~=nil;currentWorld=world
        -- 补给可能暂时移走玩家机体；保留确认事务和检查点，等待原生成功事件。
        if not supplySession then
            resetCheckpointState()
        end
        note("WORLD_CHANGED "..world)
    end
    local selected=instance:GetSelectedPlaneID()
    if not isFlight and not supplySession then
        local token=modAircraft:selection(instance)
        if modAircraft.lastMenu~=token then
            if modAircraft.lastMenu then releaseMountPreview(0)end -- 同一预览实例换涂装也不能沿用旧挂载选项。
            previewOptions=nil;modAircraft.lastMenu=token
            for _,adapter in ipairs({nativeLoadout,nativeStandby,nativeParts})do
                if adapter.error then
                    note("MENU_ERROR_VARIANT_CONTEXT_ENDED prior="..adapter.error)
                    adapter:reset{clearError=true,releaseInput=false}
                end
            end
        end
    end
    weaponSwitch.gaze.update(false);weaponSwitch.camera.update();weaponSwitch.view.update()
    if selected~=lastSelected then
        releaseMountPreview(0)
        local previousPlaneId=planeId -- 页面错误属于引发错误的机体，换机后重新建立适配上下文。
        planeId=selected;lastSelected=selected;currentProfile=profileFor(planeId);previewOptions=nil
        if not isFlight and not supplySession then
            errorText=nil
            for _,adapter in ipairs({nativeLoadout,nativeStandby,nativeParts})do
                if adapter.error then
                    note("MENU_ERROR_CONTEXT_ENDED from="..previousPlaneId.." to="..planeId.." prior="..adapter.error)
                    adapter:reset{clearError=true,releaseInput=false} -- 原错误已写日志；仅新机体可重新接入，不循环重试失败页面。
                end
            end
        end
    end
    -- 所有机型共用开场提示：在选项、HUD和模型预建之前先显示；任务模型关闭不提示。
    if not config.task_mount_enabled or not isFlight or supplySession or errorText then
        mountStartup.cancel()
    elseif not configured and valid(activator)then
        local startupProfile=profileFor(planeId)
        local key=currentWorld..":"..tostring(planeAddress)..":"..activator:GetAddress()..":"..planeId..":"..
            startupProfile.a..":"..startupProfile.b..":"..startupProfile.c
        if not mountStartup.prepare(key)then return end
    end
    -- 玩家对象与 HUD 在 START MISSION 前已经生成；先建立显示，不提前初始化武器或消耗冷却。
    if isFlight and not configured and not supplySession and not errorText and valid(activator)and os.clock()-lastPreviewHUD>=.05 then
        lastPreviewHUD=os.clock()
        local saved=aircraftDebug:profile(planeId)or config.profiles[tostring(planeId)] -- 换机只展示当前任务临时方案。
        if saved then
            previewOptions=previewOptions or optionsFor(planeId)
            saved=aircraftDebug:resolveProfile(planeId,previewOptions)or saved
            primeFlightHUD(Profile.withSettings(saved,config),previewOptions)
            -- 换机开场可能尚未完成组件碰撞设置，等飞控开放后再接管模型；HUD预建仍继续。
            if not aircraftDebug:busy()or ready()then
                mountPreview{owner=instance,instance=instance,planeId=planeId,mode="flight",
                    profile=Profile.withSettings(saved,config),options=previewOptions}
            end
        end
    end
    -- 起飞过场即可预建已确认的显示，不等玩家武器输入重新开放。
    if supplySession and supplySession.menuClosed and supplySession.preview and isFlight and valid(activator)and not errorText then
        primeFlightHUD(supplySession.preview,supplySession.options,true)
        maintainSupplyPreview()
    end
    -- 若任务成功事件到达时玩家尚未回到世界，沿现有身份发现流程等待实例；不再等待武器输入开关。
    if supplySession and supplySession.success and not errorText then completeSupplyHandoff()end
    if isFlight and ready()and not configured and not supplySession and not errorText then
        local options=optionsFor(planeId)
        local temporary=aircraftDebug:resolveProfile(planeId,options)
        local saved=temporary or config.profiles[tostring(planeId)]
        local canUse=saved and Profile.validate(saved,options,temporary~=nil)
        assert(canUse,"没有已确认且可用的原生多 SP 配置，请返回机库通过原生兵装页确认后出击；管理界面不提供兵装选择")
        configureFlight(profileFor(planeId),temporary and "debug_aircraft"or nil)
    end
    local now=os.clock()
    if now-lastActions>=.05 then
        lastActions=now
        local encoded=assert(drain(),"动作内存接口没有返回结果")
        if encoded~=""then
            local batch=json.decode(encoded)
            -- 内存领取已经移走本批动作，后续空闲周期不会重放。
            for _,action in ipairs(batch)do applyAction(action)end
        end
    end
    debugWeapons.update()
    if aircraftDebug:busy()then publishState();return end -- 同一帧内也不继续维护原机体。
    if isFlight and configured and not supplySession and not errorText then
        if restorePending then
            weaponSwitch.clear("checkpoint_restore")
            restorePending=false;assert(checkpointProfile,"检查点缺少对应的多 SP 配置")
            currentProfile=copyProfile(checkpointProfile)
            nativeHud:reset()
            weaponPresentation:reset()
            selectionAudio(0,0) -- 恢复前先隔离原生初始化提示。
            applyFixedMountSetting()
            native("checkpoint_restore") -- 恢复快照自己的槽数、映射、弹药和冷却。
            instance:SetPlaneAndSWP(planeId,currentProfile.a)
            debugProfileActive=checkpointDebugProfile
            if not debugProfileActive then config.profiles[tostring(planeId)]=copyProfile(currentProfile);saveConfig()end
            lastArmed=plane:IsSpecialWeaponArmed()
            selectionAudio(0,lastArmed and active+1 or 0)
            note("CHECKPOINT_RESTORED")
        end
        if checkpointPending then
            weaponSwitch.clear("checkpoint_save")
            checkpointPending=false
            native("checkpoint_save")
            checkpointProfile=copyProfile(currentProfile);checkpointDebugProfile=debugProfileActive;note("CHECKPOINT_CAPTURED")
        end
        observeOriginalSwitch()
        weaponSwitch.pump()
        local mountOK,mountError=updateFixedMount(planeAddress,game:GetTimeSeconds(plane)) -- 统一更新；单 QAAM 保留四处库存模型，其他单挂归还原生。
        assert(mountOK,mountError)
        mountWarning=mountError~=""and mountError or nil -- 模型失败独立降级，不中断弹药、冷却或切换。
    end
    if now-lastHUD>=.1 then
        lastHUD=now
        if configured and not supplySession and valid(activator)then
            if not errorText then
                -- The inactive group changes only during a native transaction.
                -- The active count has a reflected getter; a periodic native
                -- file transaction is unnecessary for an ammo-only native HUD.
                nativeState.main_ammo=activator:GetMainWeaponCount()
                nativeState.special_ammo=activator:GetSpecialWeaponCount()
                nativeState.cached_sp[active+1]=nativeState.special_ammo
                nativeState.armed=plane:IsSpecialWeaponArmed()
                nativeState.slots=nil;nativeState.reload=nil
            end
        end
        if isFlight and configured and not supplySession and valid(plane)and nativeState and flightOptions then
            refreshHUD(now)
        end
    end
    scenePreviewRequested=false -- 先让活动页面提供候选，随后统一补齐 SKIN、EMBLEM 等没有多SP控件的页面。
    local pagesStarted=os.clock()
    if supplySession and supplySession.menuActive and not supplySession.menuClosed and not errorText then
        -- 只在实际补给菜单打开期间扫描控件，不在正常飞行中扫描补给页。
        nativeSupply:pump(pageContext{mode="resupply",now=now,inFlight=isFlight,panelOpen=panelOpen})
        if nativeSupply.error then error(nativeSupply.error)end
    elseif not isFlight and not supplySession then
        nativeLoadout:pump(pageContext{mode="preparation",now=now,inFlight=not runtimeContext.hangarVisible,panelOpen=panelOpen})
        if modAircraft.safetyPending and not syncRuntime()then return end
    end
    nativeStandby:pump(pageContext{mode="saved",now=now,inFlight=isFlight or supplySession~=nil or not runtimeContext.hangarVisible,panelOpen=panelOpen})
    if modAircraft.safetyPending and not syncRuntime()then return end
    nativeParts:pump(pageContext{mode="saved",now=now,inFlight=isFlight or supplySession~=nil or not runtimeContext.hangarVisible,panelOpen=panelOpen})
    if modAircraft.safetyPending and not syncRuntime()then return end
    recordStage("pages",pagesStarted)
    local previewStarted=os.clock()
    maintainScenePreview()
    recordStage("preview_scene",previewStarted)
    local uiKey=tostring(panelOpen)..":"..requestOpen..":"..requestClose..":"..planeId..":"..tostring(configured)..":"..tostring(errorText)..":"..nativeHud.status..":"..weaponPresentation.status..":"..nativeLoadout.status..":"..nativeSupply.status..":"..tostring(internal)..":"..tostring(inputNotice)
    if uiKey~=lastUIKey or now-lastPublish>=(panelOpen and .1 or 1)then
        local publishStarted=os.clock()
        lastPublish=now;lastUIKey=uiKey;publishState()
        recordStage("publish",publishStarted)
    end
    if configured then mountStartup.finish()end -- 本轮模型维护、HUD及状态发布也已完成。
end
RegisterHook("/Script/Nimbus.NimbusGameInstance:SetPlaneAndSWP",function(ctx,selectedPlane,selectedWeapon)
    if not runtimeModEnabled then return end
    if aircraftDebug:busy()then return end
    if not syncRuntime()or runtimeReason=="mission13_fixed_sp"then return end
    nativeLoadout:beforeNativeCommit(ctx,selectedPlane,selectedWeapon)
    if modAircraft.safetyPending and not syncRuntime()then return end
    nativeSupply:beforeNativeCommit(ctx,selectedPlane,selectedWeapon)
end,function()end)
-- 原生页面可能复用同一对象；真实输入激活事件作为新一轮准备的边界。
RegisterHook("/Script/Nimbus.MenuBaseWidget:SetIsEnableControl",function()end,function(ctx,enabled)
    if not runtimeModEnabled then return end
    if runtimeChanging then return end
    if not syncRuntime(enabled:get()and ctx:get()or nil)or runtimeReason=="mission13_fixed_sp"then return end
    nativeLoadout:onControlChanged(ctx,enabled)
    if modAircraft.safetyPending and not syncRuntime()then return end
    -- 激活事件内完成双栏，原生第一次绘制前即可使用模组文字；自身输入操作不重入。
    if not enabled:get()or nativeLoadout.controlDepth>0 or nativeSupply.controlDepth>0
        or not valid(instance)or not catalogue or panelOpen then return end
    local ok,err=xpcall(function()
        assert(IsInGameThread(),"菜单接入事件不在游戏线程")
        if instance:GetSelectedPlaneID()~=planeId then return end -- 机体改变交给主循环先更新身份。
        scenePreviewRequested=false
        local owner=ctx:get() -- 控件仍只接入登记页面；场景预览覆盖同一机库的其他页面。
        if not isFlight and owner:IsA(StaticFindObject("/Script/Nimbus.CampaignHangarWeaponWidget"))then
            nativeLoadout:activate(owner,pageContext{mode="preparation",now=os.clock(),inFlight=false,panelOpen=false})
        elseif not isFlight and owner:IsA(StaticFindObject("/Script/Nimbus.CampaignHangarReviewWidget"))then
            nativeStandby:activate(owner,pageContext{mode="saved",now=os.clock(),inFlight=false,panelOpen=false})
        elseif not isFlight and owner:IsA(StaticFindObject("/Script/Nimbus.CampaignHangarPartsWidget"))then
            nativeParts:activate(owner,pageContext{mode="saved",now=os.clock(),inFlight=false,panelOpen=false})
        elseif supplySession and supplySession.menuActive and not supplySession.menuClosed
            and owner:IsA(StaticFindObject("/Script/Nimbus.MissionResupplyWidget"))then
            nativeSupply:activate(owner,pageContext{mode="resupply",now=os.clock(),inFlight=isFlight,panelOpen=false})
        end
        if modAircraft.safetyPending then syncRuntime();return end
        maintainScenePreview() -- 页面激活事件内完成兜底，避免SKIN/EMBLEM先绘制一帧原生状态。
    end,debug.traceback)
    if not ok then
        errorText=tostring(err);note("NATIVE_MENU_ACTIVATION_ERROR "..errorText);releaseRuntimeOwnership()
        if not isFlight and not supplySession then modAircraft:quarantine(instance:GetSelectedPlaneID(),errorText);syncRuntime()end
    end
end)
-- These reflected entry points cover normal dynamic mission events. Their
-- checkpoint/resupply coverage remains experimental and is logged explicitly.
RegisterHook("/Script/Nimbus.Mission:OnStartNextPhaseHandler",function()end,function()
    if not runtimeModEnabled then return end
    if aircraftDebug:busy()then return end
    if not syncRuntime()or runtimeReason=="mission13_fixed_sp"then return end
    if configured then checkpointPending=true end
end)
RegisterHook("/Script/Nimbus.PlayerPlane:RestoreWeaponFromCheckpoint",function()end,function(ctx)
    if not runtimeModEnabled then return end
    if aircraftDebug:busy()then return end
    if not syncRuntime()or runtimeReason=="mission13_fixed_sp"then return end
    if configured and valid(plane)and ctx:get():GetAddress()==plane:GetAddress()then
        releaseMountPreview(0) -- 重试不能继承补给候选的视觉上下文。
        -- 检查点重试中止尚未完成的返航草稿，恢复已捕获方案。
        if supplySession then nativeSupply:reset{clearError=false,releaseInput=false};ownSupplyInput(false);supplySession=nil;note("RESUPPLY_CANCELLED_CHECKPOINT")end
        restorePending=true
    end
end)
-- 生命周期失败明确停用武器修改；不能忽略异常后继续应用旧方案。
local function supplyHook(callback)
    return function(...)
        if not runtimeModEnabled or runtimeChanging or errorText or aircraftDebug:busy()then return end
        local args={...}
        local ok,err=xpcall(function()
            assert(IsInGameThread(),"补给事件不在游戏线程")
            if not syncRuntime()or runtimeReason=="mission13_fixed_sp"then return end
            callback(table.unpack(args))
        end,debug.traceback)
        if not ok then errorText=tostring(err);note("NATIVE_RESUPPLY_ERROR "..errorText);releaseRuntimeOwnership()end
    end
end
RegisterHook("/Script/Nimbus.Mission:OnStartSupplyPhaseHandler",function()end,supplyHook(function(ctx)
    weaponSwitch.clear("resupply_started")
    assert(configured and currentProfile,"补给开始时模组尚未完成出击配置")
    assert(not supplySession,"上一轮补给事务尚未结束")
    ownSupplyInput(true) -- 页面生成前已隔离两个原生输入入口，避免第一帧穿透。
    -- 任务和原机体绑定用于排除其他任务、菜单或机体触发的事件。
    supplySession={mission=ctx:get():GetAddress(),world=currentWorld,planeId=planeId,profile=copyProfile(currentProfile),
        options=optionsFor(planeId),menuActive=false,menuClosed=false,confirmed=nil,success=false}
    -- 现有右下多 SP 行留在原生 HUD 树中，过场显隐由祖先控制，不露回单 SP 源。
    weaponPresentation:reset();note("RESUPPLY_STARTED "..planeId)
end))
RegisterHook("/Script/Nimbus.MiniGamePhase:OnStartSupplyMenuHandler",function()end,supplyHook(function(ctx)
    assert(supplySession,"补给菜单打开前未收到补给开始事件")
    assert(not supplySession.menuActive,"本轮补给菜单重复开始")
    assert(not supplySession.confirmed,"已经确认的补给菜单被重新打开")
    supplySession.menuPhase=ctx:get():GetAddress();supplySession.menuActive=true;supplySession.menuClosed=false
    nativeSupply:beginSupplyMenu(ctx:get());note("RESUPPLY_MENU_OPENED")
    -- 原生菜单创建完成的同一事件内接管；事件先于构造的任务仍由激活事件接入。
    nativeSupply:refreshNow(pageContext{mode="resupply",now=os.clock(),inFlight=isFlight,panelOpen=panelOpen})
    assert(not nativeSupply.error,nativeSupply.error)
end))
RegisterHook("/Script/Nimbus.MiniGamePhase:OnFinishdSupplyMenuHandler",supplyHook(function(ctx)
    -- 这里只观察已发出的 YES；异步关闭期间打开 F8 不应使合法结束通知失败。
    assert(supplySession and supplySession.menuActive,"补给结束阶段没有活动菜单")
    -- 严格匹配菜单开始时的阶段，不因关卡世界不同而重绑对象或误判跨任务。
    assert(supplySession.menuPhase==ctx:get():GetAddress(),"补给结束事件与菜单开始阶段不匹配")
    nativeSupply:beforeSupplyFinish(ctx)
end),supplyHook(function(ctx)
    nativeSupply:afterSupplyFinish(ctx)
    -- 只有结束回调已返回，草稿才可用于预建；关闭事件可能嵌套在结束回调内部。
    supplySession.preview=nativeSupply:previewSupply()
    if valid(plane)then primeFlightHUD(supplySession.preview,supplySession.options,true)end
end))
RegisterHook("/Script/Nimbus.MiniGamePhase:OnCloseSupplyMenu",function()end,supplyHook(function(ctx)
    if supplySession and supplySession.menuPhase==ctx:get():GetAddress()then
        -- 关闭不能单独提交草稿；结束回调可能尚未返回，保留控件事务。
        assert(nativeSupply:isSupplyHandoffPending(),"原生补给菜单在 YES 请求前关闭")
        supplySession.menuClosed=true
        if supplySession.preview and valid(plane)then primeFlightHUD(supplySession.preview,supplySession.options,true)end
        note("RESUPPLY_MENU_CLOSED")
    end
end))
RegisterHook("/Script/Nimbus.Mission:OnSuccessSupplyPhaseHandler",function()end,supplyHook(function(ctx)
    assert(supplySession and supplySession.mission==ctx:get():GetAddress(),"补给成功事件与当前任务不匹配")
    assert(not supplySession.success,"同一轮补给重复成功")
    supplySession.success=true;note("RESUPPLY_SUCCESS_PENDING")
    completeSupplyHandoff() -- 此处是原生事件后置回调，原生补给已经完成，画面尚未推进到下一帧。
end))
assert(type(LoopInGameThreadWithDelay)=="function" and type(ExecuteInGameThreadWithDelay)=="function" and type(IsInGameThread)=="function",
    "This DualSP version requires the bundled game-thread timer API")
overlay();note("DUALSP_LOADED 2.23.0 MANAGEMENT_UI")
-- LoopAsync + ExecuteInGameThread used to register new Lua closures from an
-- async Lua state every 20 ms while another state executed the controller.
-- The crash dump aborts in Registry::get_function_ref. Register one recurring
-- game-thread callback instead; delayed callbacks use the same scheduler.
local threadReady=false
local threadWaitReported=false
-- 每 15 秒汇总当前场景的调度耗时；只测本 Lua 回调，不把它当成整帧或 GPU 耗时。
local performance={scene=nil,count=0,total=0,peak=0,since=0}
local function recordTickCost(started)
    local now=os.clock();local elapsed=(now-started)*1000 -- 本机 Windows Lua 时钟的毫秒差值。
    local scene=supplySession and "resupply"or isFlight and "flight"or "menu"
    if performance.scene~=scene then
        performance={scene=scene,count=0,total=0,peak=0,since=now}
        performanceStages={} -- 不把前一场景尚未输出的分段计时混入新场景。
    end
    performance.count=performance.count+1;performance.total=performance.total+elapsed
    performance.peak=math.max(performance.peak,elapsed)
    if now-performance.since>=15 then
        note(string.format("PERF_TICK scene=%s samples=%d average_ms=%.3f peak_ms=%.3f page=%s",scene,performance.count,
            performance.total/performance.count,performance.peak,runtimeContext.observedMenu or "none"))
        -- 父阶段包含子阶段；只用于定位热点，不能把所有阶段相加当作总耗时。
        for _,name in ipairs({"context","options","pages","preview_scene","preview_native","publish"})do
            local row=performanceStages[name]
            if row then note(string.format("PERF_STAGE scene=%s stage=%s samples=%d total_ms=%.3f average_ms=%.3f peak_ms=%.3f",
                scene,name,row.count,row.total,row.total/row.count,row.peak))end
        end
        performanceStages={}
        performance={scene=scene,count=0,total=0,peak=0,since=now}
    end
end
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
            local started=os.clock() -- 不改变输入、武器和 HUD 的既有更新频率。
            tick();weaponSwitch.salvo.service:update();recordTickCost(started)
        end,debug.traceback)
        if not ok then
            mountStartup.cancel() -- 错误退出不能留下永不消失的加载提示。
            errorText=tostring(err);note("ERROR "..errorText)
            aircraftDebug:failed(errorText)
            releaseMountPreview(0)
            if runtimeChanging or not runtimeAllowed then
                -- 模式识别/清理失败只保留诊断，绝不沿用上次单人许可。
                runtimeAllowed=false;runtimeChanging=false;runtimeReason="context_error"
                local uiOK,uiErr=uiRuntimeGate(0);local coreOK,coreErr=nativeRuntimeGate(0)
                if not uiOK or not coreOK then note("RUNTIME_DISABLE_ERROR "..tostring(uiErr).." "..tostring(coreErr))end
                return false
            end
            releaseRuntimeOwnership()
            -- 尚未重建兵装时可完整交回原生；已配置/补给中的错误保留既有冻结边界，不能伪装成原生配装。
            if not configured and not supplySession and valid(instance)and catalogue then
                modAircraft:quarantine(instance:GetSelectedPlaneID(),errorText)
                if not syncRuntime()then return false end
            end
            -- Report once, freeze weapon mutations, and keep the UI/error visible.
            -- A user closes the panel to resume the game; no hidden loadout retry.
            local published,publishErr=pcall(publishState)
            if not published then note("UI_PUBLISH_ERROR "..tostring(publishErr))end
        end
    return false
end)
