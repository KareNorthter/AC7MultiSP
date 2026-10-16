-- AC7DualSP 1.0.2-su30sm-three-sp.9：按实际武器类识别行为，读取模组提供的原生名称。
-- 出击和中途补给使用原生 UMG 页面，管理快捷键默认 F6，可配置以避开其他模组。
local source=debug.getinfo(1,"S").source:gsub("^@",""):gsub("\\","/")
local root=assert(source:match("^(.*)/Scripts/[^/]+$"),"Cannot locate DualSP directory").."/"
local json=dofile(root.."Scripts/json.lua")
local Profile=dofile(root.."Scripts/loadout_profile.lua") -- 三槽结构、购买校验和均分规则。
local weaponCycle=dofile(root.."Scripts/weapon_cycle.lua")
local runtimeContext=dofile(root.."Scripts/runtime_context.lua") -- 模式与游戏文本语言共用只读来源。
local tr=runtimeContext.text -- 原生页面文字跟随游戏，不跟随 F8 手动语言。
local runtimeAllowed=false -- 未确认单人状态前，所有模组功能均禁止。
local runtimeReason="initializing" -- 禁用原因写日志，不在多人界面绘制提示。
local runtimeChanging=false -- 模式清理期间阻止反射钩子重入。
local runtimeFailed=false -- 模式识别出错后明确停用到重启，不不断重试或沿用旧许可。
local syncRuntime=nil -- 完整状态清理函数在各模块定义之后建立。
local log=assert(io.open(root.."mod.log","a"))
local function note(message)log:write(os.date("!%Y-%m-%dT%H:%M:%SZ ")..tostring(message).."\n");log:flush()end
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
local function writeJson(path,value)
    local file=assert(io.open(path,"w"));file:write(json.encode(value));file:close()
end
local function loadExport(name,dll)
    local fn,err=package.loadlib(root.."bin/"..(dll or "DualSP.dll"),name);assert(fn,err);return fn
end
-- The validated weapon core and the rebuilt menu renderer are separate modules.
local bridge=loadExport("lab_native_execute")
loadExport("dualsp_three_sp_supported") -- 在读取或迁移配置前拒绝旧核心。
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
note("FIXED_MOUNT_CANDIDATE 1.0.2-su30sm-three-sp.9") -- 核心三 SP 命令协议 3，管理 UI 协议 4，须成套安装。
local takeEmptySwitch=loadExport("dualsp_take_empty_switch") -- 按下原生切换键才处理耗尽挂载，不预先换模型。
local finishTLS=loadExport("dualsp_finish_tls") -- 在缓存冷却和更换挂载之前结束当前激光。
local nativeMenuConfirm=loadExport("dualsp_menu_confirm") -- 双选内部步骤复用当前页面的原生确认音。
local nativeLoadoutAmmo=loadExport("dualsp_loadout_ammo") -- 三页统一读取部件加成后的完整载量。
local nativePartsAmmo=loadExport("dualsp_parts_ammo_preview") -- PART 只读候选快照；旧核心缺少接口时明确拒绝混装。
local nativeWeaponIcon=loadExport("dualsp_weapon_icon") -- 机库全名前复用原生分类图标。
local prepareWeaponRebuild=loadExport("dualsp_prepare_weapon_rebuild") -- 丢弃锁定槽前配对停止音效。
local catalogExport=loadExport("dualsp_catalogue")
local publish=loadExport("dualsp_publish","DualSPUI.dll")
local drain=loadExport("dualsp_drain_actions","DualSPUI.dll")
local overlay=loadExport("dualsp_overlay_start","DualSPUI.dll")
-- 管理版 Lua 必须搭配新版界面，防止旧 F8 再次提交兵装配置。
local managementProtocol=loadExport("dualsp_management_supported","DualSPUI.dll")
loadExport("dualsp_fixed_central_hint_supported","DualSPUI.dll") -- 修改配置前确认 UI 已移除独立中央开关。
local preparationNavigation=loadExport("dualsp_preparation_navigation","DualSPUI.dll") -- 原生页面的只读键盘导航。
local config=readJson(root.."profiles.json")or {schema=2,management_revision=4,profiles={},language="auto",keyboard_keys={49,50,51,52},management_key=117}
local migrateSettings=config.schema==1 -- 旧版方案只补空 SP3，比例统一重算。
assert(type(config.profiles)=="table"and (migrateSettings or config.schema==2),"不支持的配置版本")
if migrateSettings then
    assert(config.management_revision==nil or config.management_revision==2,"旧管理参数版本无效")
    for id,saved in pairs(config.profiles)do
        assert(saved.c==nil,"旧版方案含未知第三槽，停止迁移")
        saved.c=-1;config.profiles[id]=Profile.copy(saved)
    end
    config.keyboard_keys=config.keyboard_keys or {49,50,51}
    assert(#config.keyboard_keys==3,"旧版键位数量无效")
    -- 默认 4 与旧自定义键冲突时明确要求处理，不能偷偷改掉用户键位。
    for _,key in ipairs(config.keyboard_keys)do assert(key~=52,"旧键位已占用 4，请先在 1.0.2 调整后迁移")end
    config.keyboard_keys[4]=52
    config.schema=2;config.management_revision=3;config.ammo_ratio=nil
    config.language=config.language or "auto";config.central_hint=config.central_hint~=false
    config.management_key=config.management_key or 117
    config.hud=nil;config.direct_select=nil;config.ignore_unlocks=nil;config.hud_scale=nil;config.native_loadout=nil
    note("THREE_SP_MIGRATION schema=2 management=3 ammo=equal_share")
end
local migrateCentral=config.management_revision==3 -- 第 4 版取消中央开关，旧 false 也迁移为固定开启。
if migrateCentral then
    assert(type(config.central_hint)=="boolean","旧中央简称开关必须为布尔值")
    config.central_hint=nil;config.management_revision=4
    note("FIXED_CENTRAL_HINT_MIGRATION management=3->4 central=always_on")
end
assert(config.management_revision==4,"固定中央简称需要管理参数第 4 版")
for _,saved in pairs(config.profiles)do Profile.count(saved)end
-- 键位只接受单个普通键，管理快捷键和 Esc 不可用于武器选择。
local function supportedKey(key,managementKey)
    return type(key)=="number"and key%1==0 and
        ((key>=48 and key<=57)or(key>=65 and key<=90)or(key>=96 and key<=111)
        or(key>=112 and key<=135 and key~=managementKey)or(key>=186 and key<=192)or(key>=219 and key<=222)
        or key==8 or key==9 or key==13 or key==32 or(key>=33 and key<=40)or key==45 or key==46)
end
-- 所有管理设置先完整验证，再写盘，不允许部分保存或静默替换错误键位。
local function validateSettings(settings)
    local managementKey=settings.management_key -- 管理入口仅支持功能键，关闭游戏后修改配置。
    assert(type(managementKey)=="number"and managementKey%1==0 and managementKey>=112 and managementKey<=135,"管理快捷键须为 F1 至 F24 的整数键码")
    assert(settings.language=="auto"or settings.language=="zh"or settings.language=="en","Language must be auto, zh or en")
    assert(settings.central_hint==nil,"中央简称已固定开启，不接受旧开关")
    assert(settings.hud==nil and settings.direct_select==nil and settings.ignore_unlocks==nil,"旧功能开关已移除")
    assert(type(settings.keyboard_keys)=="table"and #settings.keyboard_keys==4,"需要 MSL/SP1/SP2/SP3 四个键位")
    local used={}
    for _,key in ipairs(settings.keyboard_keys)do
        assert(supportedKey(key,managementKey),"不支持此键位，管理快捷键和 Esc 为保留键")
        assert(not used[key],"四个武器选择键不能重复");used[key]=true
    end
    assert(settings.ammo_ratio==nil,"三 SP 已改为按数量均分，不接受旧比例滑块")
end
validateSettings(config)
-- 设置保存可传入完整新配置；只有写盘成功后才替换当前配置。
local function saveConfig(nextConfig)
    -- Keep the previous settings recoverable before replacing the small mod file.
    local old=io.open(root.."profiles.json","r")
    if old then local text=old:read("*a");old:close();local backup=assert(io.open(root.."profiles.previous.json","w"));backup:write(text);backup:close()end
    writeJson(root.."profiles.json",nextConfig or config)
end
if migrateSettings or migrateCentral then
    -- 首次迁移保留原文件；后续普通保存不能覆盖本次协议升级的回退副本。
    local backupPath=root..(migrateSettings and "profiles.before-three-sp.json"or"profiles.before-fixed-central-hint.json") -- 按来源版本分开备份。
    local original=assert(io.open(root.."profiles.json","r"));local bytes=original:read("*a");original:close()
    local existing=io.open(backupPath,"r")
    if existing then existing:close();error("配置迁移备份已存在，请核对上次迁移状态："..backupPath)end
    local backup=assert(io.open(backupPath,"w"));backup:write(bytes);backup:close()
    saveConfig();note("MANAGEMENT_MIGRATION_SAVED revision=4")
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
local weaponRevision=0 -- 只在兵装重建成功后递增；同槽补给也必须刷新剪影。
local nativeRequest=0
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
local descriptions={
    qaam="高机动空对空导弹",sasm="小型空爆导弹",hcaa="高容量空对空导弹",hpaa="高威力空对空导弹",hvaa="高速空对空导弹",laam="远程空对空导弹",saam="半主动雷达制导导弹",
    ["4aam"]="四目标空对空导弹",["6aam"]="六目标空对空导弹",["8aam"]="八目标空对空导弹",
    lasm="远程反舰导弹",lagm="远程空对地导弹",lacm="远程巡航导弹",["4agm"]="四目标空对地导弹",["8agm"]="八目标空对地导弹",
    ugb="无制导炸弹",gpb="精确制导炸弹",xsdb="小直径炸弹",sffs="自锻破片子母弹",sod="远距撒布弹药",faeb="燃料空气炸弹",
    eml="电磁轨道炮",plsl="脉冲激光炮",tls="战术激光系统",rkt="火箭弹",grkt="制导火箭弹",mgp="机炮吊舱",mpbm="多用途爆炸导弹",uav="无人机",ecm="电子干扰",mstm="多目标导弹",admm="全向多用途导弹"}
local unsupportedDual={[26]=true,[31]=true} -- 按实际武器类的 UAV/ECM 编号限制，不按可被模组重用的菜单简称。
local names={f04e="F-4E Phantom II",f15c="F-15C Eagle",f15e="F-15E Strike Eagle",f18f="F/A-18F Super Hornet",f16c="F-16C Fighting Falcon",f22a="F-22A Raptor",f35c="F-35C Lightning II",a10a="A-10C Thunderbolt II",f14d="F-14D Super Tomcat",f02a="F-2A",f15j="F-15J",j39e="Gripen E",m21b="MiG-21bis",m29a="MiG-29A",m31b="MiG-31B",mr2k="Mirage 2000-5",rflm="Rafale M",typn="Typhoon",pkfa="Su-57",su30="Su-30M2",su30sm="Su-30SM",su33="Su-33",su34="Su-34",su35="Su-35S",su37="Su-37",su47="Su-47",x02s="X-02S Strike Wyvern",adf11f="ADF-11F Raven",zoef="ADF-01 FALKEN",mrgn="ADFX-01 Morgan",dark="DarkStar",f18e="F/A-18E",f18etg="F/A-18E Top Gun",f14a="F-14A",f14atg="F-14A Top Gun",su57tg="Su-57 Top Gun",f104="F-104C",asfx="ASF-X Shinden II",fa27="XFA-27",fa44="CFA-44 Nosferatu",f15m="F-15 S/MTD",f16x="F-16XL",fb22="FB-22",f18x="F/A-18F Block III",f02x="F-2A Super Kai",m35d="MiG-35D",yf23="YF-23"}

local function valid(object)return object and object:IsValid()end
local function native(op,target)
    assert(IsInGameThread(),"Native weapon access requires the game thread")
    assert(valid(plane) and valid(activator),"Player plane is unavailable")
    local time=game:GetTimeSeconds(plane)
    local f=assert(io.open(root.."native-command.txt","w"))
    nativeRequest=nativeRequest+1
    f:write(string.format("%s %X %X %d %.9f 3 %d %d %d %d %d",op,plane:GetAddress(),activator:GetAddress(),target or 0,time,Profile.count(currentProfile),currentProfile.a,currentProfile.b,currentProfile.c,nativeRequest));f:close()
    bridge();local result=assert(readJson(root.."native-status.json"))
    assert(result.request==nativeRequest,"Native response does not match the current request")
    assert(result.ok,result.error)
    if op=="setup"or op=="resupply_setup"or op=="switch"or op=="checkpoint_restore"or op=="resupply"then
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
    -- 直接核对已加载实例的类及父类；不再按显示大小写拼路径后做全局对象查找。
    local baseName="aceplayerpawn_"..code:lower() -- 表内机体代码与资源显示名称可能大小写不同。
    local expected="blueprintgeneratedclass /game/blueprint/player/pawn/"..baseName.."."..baseName.."_c" -- 比较完整路径，避免同短名误认。
    local actualClass=sourcePlane:GetClass() -- 从当前原生预览/玩家实例取得真实类身份。
    while valid(actualClass)do
        if actualClass:GetFullName():lower()==expected then return true end
        actualClass=actualClass:GetSuperStruct() -- 涂装派生类必须能追溯到所选机体的基础类。
    end
    return false
end
local function optionsFor(id)
    -- Purchases cannot change during a sortie. Do not scan the global object
    -- registry or enumerate PurchasedWeapons for every HUD refresh.
    if configured and isFlight and id==planeId and flightOptions then return flightOptions end
    local info=catalogue and catalogue[tostring(id)]
    if not info or not info.supported then return {} end
    assert(IsInGameThread()and valid(instance),"弹药预览需要当前游戏线程与机体选择")
    -- 菜单直接读取机库当前展示的实例，任务读取玩家实例；不假定蓝图默认对象已具备完整组件。
    local sourcePlane=plane -- 任务实例已由现有世界/生命周期检查确定。
    if not isFlight then
        local hangar=instance.HangarManager -- 原生机库管理器持有实际展示对象，无需全局猜测或另行生成机体。
        assert(valid(hangar),"当前机库管理器未就绪")
        sourcePlane=hangar.LoadedPlayerPlane
        -- 选择编号先改变、预览模型后加载属于原生异步换机；返回显式等待原因，不能记成永久接入错误。
        if not valid(sourcePlane)then return {},"preview_missing selected="..info.code end
        if instance:GetSelectedPlaneID()~=id or not isSelectedPlane(sourcePlane,info.code)then
            return {},"preview_changing selected="..info.code.." actual="..sourcePlane:GetClass():GetFullName()
        end
    end
    assert(valid(sourcePlane),"当前机体实例未加载，请确认模型已安装："..info.code)
    assert(valid(sourcePlane:GetWorld())and sourcePlane:GetWorld():GetAddress()==instance:GetWorld():GetAddress(),"机体兵装来源不属于当前世界")
    -- 机库预览实例的 PlaneTypeID 可为 -1，以实际类继承链核对当前选择，保留完整路径约束。
    -- 菜单分支刚核对过同一实例的类链，避免重复遍历；任务实例仍在这里独立核对。
    if isFlight then assert(isSelectedPlane(sourcePlane,info.code),"任务机体与当前选择不符："..info.code)end
    local sourceActivator=sourcePlane.PlayerWeaponActivator -- 使用原生构造完成并应用蓝图覆盖的组件。
    assert(valid(sourceActivator),"当前机体实例兵装配置缺失："..info.code)
    local weaponIds={} -- 菜单 PWDT 编号与运行时 WeaponID 可以不同；部件计算必须用后者。
    for i=1,3 do
        local weaponClass=sourceActivator["SpecialWeaponConfig"..i].WeaponClass -- 每槽可被替换模组独立改写。
        assert(valid(weaponClass),"当前机体兵装类缺失："..info.code.."/"..i)
        local weaponDefault=weaponClass:GetCDO() -- 原生枚举仍受核心范围校验保护。
        assert(valid(weaponDefault),"当前机体兵装默认对象缺失："..info.code.."/"..i)
        weaponIds[i]=weaponDefault.WeaponID
        assert(type(weaponIds[i])=="number","原生兵装编号类型异常："..info.code.."/"..i)
    end
    local w=info.weapons -- 原生部件先作用于完整载量；草稿随后仍使用原配弹比例。
    local ok,encoded=nativeLoadoutAmmo(instance:GetAddress(),id,weaponIds[1],w[1].max,weaponIds[2],w[2].max,weaponIds[3],w[3].max)
    assert(ok,encoded);local amounts=json.decode(encoded)
    assert(type(amounts)=="table"and #amounts==3,"原生弹药预览结果不完整")
    local owned=ownedWeapons();local options={}
    for i,item in ipairs(info.weapons)do
        options[i]={index=item.index,code=item.code,runtime_weapon_id=weaponIds[i],max=amounts[i],base_max=item.max,category=item.category,type_key=item.type_key,
            stats=item.stats,description=descriptions[item.code]or item.code:upper(),
            owned=item.index==0 or owned[id*10+item.index]==true, -- 不再绕过游戏购买记录。
            available=not unsupportedDual[weaponIds[i]]}
    end
    return options
end
local function profileFor(id)
    local saved=config.profiles[tostring(id)]
    if saved then return Profile.copy(saved)end
    local selected=valid(instance) and instance:GetSelectedPlaneSWP()or 0
    return {a=math.max(0,math.min(2,selected)),b=-1,c=-1,ratio=1}
end
-- 复制小配置，避免补给草稿与检查点引用同一张可修改的表。
local copyProfile=Profile.copy -- 检查点、补给和预览使用同一复制规则。
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
nativeLoadout=dofile(root.."Scripts/native_loadout.lua").new(note,function(id,profile)
    assert(not isFlight and id==planeId,"Aircraft changed before native preparation commit")
    local options,waiting=optionsFor(id) -- 提交不能等待或使用前一机体的兵装。
    assert(not waiting,"确认时机库预览尚未就绪："..tostring(waiting))
    Profile.validate(profile,options)
    currentProfile=profile;config.profiles[tostring(id)]=profile;saveConfig()
    lastUIKey=nil;note("PROFILE_SAVED_NATIVE "..id)
end,nil,preparationNavigation,nil,nativePreparationAction,menuConfirm,nativeWeaponIcon,tr)
nativeStandby=dofile(root.."Scripts/native_loadout.lua").new(note,nil,"standby",preparationNavigation,nil,nil,nil,nil,tr)
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
nativeParts=dofile(root.."Scripts/native_loadout.lua").new(note,nil,"parts",preparationNavigation,nil,nil,nil,nil,tr,partsAmmoPreview)
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
end,nil,menuConfirm,nil,tr)
local function setPanel(open)
    if open and not runtimeAllowed then return end
    if panelOpen==open then return end
    if open and nativeLoadout:isPreparationHandoffPending()then
        note("MANAGEMENT_WAITING_FOR_PREPARATION_CLOSE");return
    end
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
    -- 右下 HUD 始终启用；中央简称可独立关闭，两处显示共用一次就绪采样。
    if enabled and not hudSamplingError then
        local callOK,sampleOK,mainReady,sp1Ready,sp2Ready,sp3Ready,err=pcall(readReadiness,
            plane:GetAddress(),activator:GetAddress(),game:GetTimeSeconds(plane),Profile.count(currentProfile))
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
        weaponPresentation:update(plane,currentProfile,flightOptions,nativeState,enabled,now)
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
    weaponPresentation:update(plane,profile,options,seed,visible,os.clock())
end
-- 正常出击建立检查点；补给重建只替换当前兵装，保留任务检查点。
local function configureFlight(profile,resupply)
    -- 补给在原生成功事件返回后即可重建；等待飞控输入开放会把旧挂载多留在起飞画面约一秒。
    assert((resupply and supplySession and supplySession.success and valid(plane)and valid(activator))or ready(),
        "武器初始化需要已开放的出击控制，或已经成功返回的补给事务")
    assert(not configured or resupply,"Loadout is locked for this sortie")
    -- 在触碰武器前检查候选协议；正式 0.1.7 核心不能冒充支持补给重选。
    if resupply and profile.b>=0 then
        local supported=package.loadlib(root.."bin/DualSP.dll","dualsp_resupply_supported")
        assert(supported,"补给重选需要支持 resupply_setup 的独立候选核心，当前正式 DLL 不支持")
    end
    local options=optionsFor(planeId)
    Profile.validate(profile,options)
    currentProfile=profile
    flightOptions=options
    selectionAudio(0,0) -- 重建前先隔离中间态选择音，不影响锁定音收尾。
    primeFlightHUD(profile,options)
    -- 仅在出击或已成功的补给事务内重建；正常切换仍由核心保存消耗与冷却。
    local audioReady,audioError=prepareWeaponRebuild(plane:GetAddress(),activator:GetAddress()) -- 保留旧槽直到停止事件完成。
    assert(audioReady,audioError or "原生锁定音收尾未完成")
    local mountOK,mountError=updateFixedMount(0,0) -- 原生重建之前释放上一组合，涵盖补给转单挂。
    assert(mountOK,mountError)
    -- 第一槽也由核心同步原生编号后初始化；Lua 不再另建一条单挂初始化路径。
    -- 释放发射按键只负责输入收尾；锁定参数由完整原生初始化/切换流程更新。
    releaseWeaponInput(resupply and "resupply_loadout"or "mission_loadout")
    -- 单挂也进入核心快照，保证补给后在 1/2/3 槽之间切换仍能恢复旧检查点。
    native(resupply and "resupply_setup"or "setup",0)
    if not resupply then native("checkpoint_save",0)end
    configured=true;active=0;lastArmed=plane:IsSpecialWeaponArmed();note("LOADOUT "..planeId.." "..json.encode(profile))
    if not resupply then checkpointProfile=copyProfile(profile)end
    if resupply then supplySession.applied=true end -- 数据就绪后立即展示，事务清理不再撤销刚接管的 HUD。
    -- 核心初始化后立即提交一次真实状态，避免等待 100 ms HUD 周期。
    refreshHUD(os.clock())
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
    configureFlight(profile,true)
    config.profiles[tostring(planeId)]=copyProfile(profile);saveConfig()
    nativeSupply:reset(false,false);ownSupplyInput(false);supplySession=nil;lastUIKey=nil
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
-- 原生按下/释放属于同一切换事务，完成后再向两处 HUD 提交选择。
local function pressSwitch(desired)
    assert(IsInGameThread(),"武器类别切换必须在游戏线程完成")
    internal=true
    local desiredArmed=desired>0 -- 按逻辑目标核对最终类别，不能依赖中间态。
    -- 原生按下会同步更新类别；释放只结束长按输入。与核心 keepArmed 一样
    -- 在同一调用中完成，避免等待期间滚轮再次切换，导致旧回调误判失败。
    plane:OnWeaponSwitchPressed()
    plane:OnWeaponSwitchReleased()
    lastArmed=plane:IsSpecialWeaponArmed()
    assert(lastArmed==desiredArmed,"原生武器类别切换未完成")
    internal=false
    selectionAudio(1,desired)
    note("WEAPON_CATEGORY_COMMITTED "..tostring(lastArmed))
    if configured and flightOptions and not supplySession then refreshHUD(os.clock())end
end
-- MSL/SP 类别转换与 SP 重建共同完成后才刷新，避免暴露中间的 MSL 状态。
local function selectLogicalWeapon(desired)
    assert(desired>=0 and desired<=3,"无效的逻辑武器选择")
    internal=true
    -- 仅在更换 SP、即将销毁旧池前收尾 TLS。切到 MSL 保留当前 SP 池，让光束按原生寿命结束。
    -- 原生循环可能先经过 MSL，仍按最终逻辑目标和当前挂载判断，不能依据中间 armed 状态。
    if desired>0 and desired~=active+1 and flightOptions and flightOptions[currentProfile[Profile.fields[active+1]]+1].runtime_weapon_id==25 then
        local ended,err=finishTLS(plane:GetAddress(),activator:GetAddress())
        assert(ended,err or "TLS 原生收尾未完成")
    end
    if desired>0 and active~=desired-1 then
        -- 核心恢复 SP 时也必须执行完整飞机切换，保持武器与锁定组件一致。
        releaseWeaponInput("special_slot_switch")
        native("switch",desired-1)
    end
    local armed=plane:IsSpecialWeaponArmed()
    if armed~=(desired>0)then
        pressSwitch(desired)
    else
        lastArmed=armed;internal=false
        selectionAudio(1,desired)
        refreshHUD(os.clock())
    end
end
-- 直接选择只在允许武器操作的任务中执行；已选中目标时不重建。
local function directSelect(desired)
    assert(type(desired)=="number"and desired%1==0 and desired>=0 and desired<=3,"直接选择目标无效")
    if not configured or supplySession or panelOpen or internal or errorText or restorePending or checkpointPending
        or not isFlight or not ready()or game:IsGamePaused(plane)then return end
    inputNotice=nil
    if desired>Profile.count(currentProfile)then inputNotice="empty_sp";note("DIRECT_SELECTION_REJECTED EMPTY_SP "..desired);return end
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
    if not configured then return end
    local requestOK,emptyRequested=takeEmptySwitch(plane:GetAddress()) -- 不可操作期间丢弃过期请求。
    assert(requestOK,emptyRequested or "原生切换请求读取失败")
    if not configured or supplySession or internal or panelOpen or errorText or not ready()or game:IsGamePaused(plane)then return end
    local armed=plane:IsSpecialWeaponArmed()
    -- The live SP count is authoritative; inactive SP counts are captured on
    -- each swap. A consumed last round must not wait for the 100 ms HUD update.
    local ammo={} -- 只包含当前实际槽，末尾空槽不参与循环。
    for index=1,Profile.count(currentProfile)do ammo[index]=nativeState.cached_sp[index]end
    ammo[active+1]=activator:GetSpecialWeaponCount()
    if lastArmed~=nil and armed~=lastArmed then
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
local function applyAction(action)
    if action.type=="open"then setPanel(true)
    elseif action.type=="close"then setPanel(false)
    elseif action.type=="select_weapon"then directSelect(action.target)
    elseif action.type=="settings"then
        -- 管理界面没有选兵装的权限；构造完整新设置后验证并保存。
        local ok,err=xpcall(function()
            assert(type(action.request)=="number"and action.request%1==0 and action.request>0,"管理保存请求编号无效")
            local nextConfig=json.decode(json.encode(config))
            assert(action.management_protocol==4,"管理界面协议不匹配，请成套安装")
            assert(action.central_hint==nil,"中央简称已固定开启，不接受旧开关")
            for _,field in ipairs({"language","keyboard_keys"})do
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
    local contextName=worldContext:GetFullName() -- 菜单和简报世界无需反复扫描玩家飞机。
    if not contextName:find("/Game/Maps/Mission/",1,true)and not contextName:find("/Game/Maps/FreeFlight/",1,true)then return nil,""end
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
    local state={schema=1,scene=isFlight and "flight"or"menu",plane_id=planeId,plane_name=info and (names[info.code]or info.code:upper())or tr("请选择机体","Select an aircraft"),
        configured=configured,state=nativeState,request_open=requestOpen,request_close=requestClose,
        management_protocol=4,language=config.language,game_language=runtimeContext.language,
        keyboard_keys=config.keyboard_keys,management_key=config.management_key,ammo_mode="equal_share",
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
    if nativeStandby.error then state.menu_error=nativeStandby.error end
    if nativeParts.error then state.menu_error=nativeParts.error end
    if nativeHud.error or hudSamplingError then state.hud_error=nativeHud.error or hudSamplingError end
    if weaponPresentation.error then state.central_error=weaponPresentation.error end
    if errorText then state.error=errorText;state.state=nil end
    writeJson(root.."ui-state.pending.json",state);publish()
end
-- 禁用先关闭原生与渲染侧入口，再释放自己的控件和事务；不改多人兵装或配置。
local function checkRuntime(activatedWidget)
    if runtimeChanging then return false end
    local allowed,reason=runtimeContext.allowed(instance) -- 每次重新检查，避免事件早于周期更新。
    if activatedWidget and runtimeContext.isMultiplayerWidget(activatedWidget)then
        runtimeContext.enterMultiplayerMenu(instance);allowed=false;reason="multiplayer_menu"
    end
    if allowed==runtimeAllowed and reason==runtimeReason then return allowed end
    runtimeChanging=true
    runtimeAllowed=allowed;runtimeReason=reason
    local uiOK,uiErr=uiRuntimeGate(allowed and 1 or 0)
    assert(uiOK,uiErr)
    if not allowed then
        local coreOK,coreErr=nativeRuntimeGate(0) -- 清除音效、输入、旧玩家及检查点身份。
        assert(coreOK,coreErr)
        if panelOpen then setPanel(false)end
        nativeLoadout:reset(true,false);nativeSupply:reset(true,false)
        nativeStandby:reset(true,false);nativeParts:reset(true,false)
        nativeHud:reset();weaponPresentation:reset()
        supplySession=nil;checkpointProfile=nil;checkpointPending=false;restorePending=false
        cachedPlane=nil;cachedWorld="";plane=nil;activator=nil;planeAddress=nil;currentWorld=""
        configured=false;isFlight=false;internal=false;lastArmed=nil;nativeState=nil
        currentProfile=nil;flightOptions=nil;previewOptions=nil;lastPlaneScan=-1;lastSelected=-1
        lastHUD=-1;lastPreviewHUD=-1;lastUIKey=nil;inputNotice=nil;hudSamplingError=nil
        -- 已生成但未消费的动作属于上一模式，只丢弃，不在返回单人后重放。
        local stale=io.open(root.."ui-actions.json","r")
        if stale then stale:close();assert(os.remove(root.."ui-actions.json"),"Cannot discard stale UI actions")end
    else
        local coreOK,coreErr=nativeRuntimeGate(1)
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
        local cleaned,err=pcall(function()entry[2]:reset(true,false)end)
        if not cleaned then note("RUNTIME_VIEW_RESTORE_ERROR "..entry[1].." "..tostring(err))end
    end
    runtimeChanging=false
    return false
end
local function tick()
    if not valid(instance)then instance=FindFirstOf("NimbusGameInstance")end
    if not syncRuntime()then return end
    if runtimeContext.updateLanguage(os.clock())then
        lastUIKey=nil;note("GAME_TEXT_CULTURE "..runtimeContext.culture)
        nativeLoadout:languageChanged();nativeSupply:languageChanged()
    end
    -- 返回主菜单明确结束本轮补给，防止旧事务阻止下一次任务配置。
    if supplySession then
        local worldContext=instance:GetWorld()
        if valid(worldContext)and worldContext:GetFullName():find("/Game/Maps/Menu/",1,true)then
            nativeSupply:reset(true,false);ownSupplyInput(false);supplySession=nil;checkpointProfile=nil
            note("RESUPPLY_CANCELLED_LEAVE_MISSION")
        end
    end
    if not game then game=StaticFindObject("/Script/Engine.Default__GameplayStatics")end
    -- 先更新短提示生命周期，再处理切换；暂停和菜单中的提前返回也不会留下外部声音。
    updateCustomAudio()
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
        local mountOK,mountError=updateFixedMount(0,0) -- 新世界不能继续持有旧模型身份。
        assert(mountOK,mountError)
        if panelOpen then setPanel(false)end
        selectionAudio(2)
        nativeHud:reset()
        weaponPresentation:reset()
        nativeLoadout:reset(true,false)
        nativeStandby:reset(true,false)
        nativeParts:reset(true,false)
        plane=found;planeAddress=address;activator=found and found.PlayerWeaponActivator or nil
        configured=false;lastArmed=nil;nativeState=nil;internal=false;hudSamplingError=nil;inputNotice=nil
        if not supplySession then errorText=nil end
        flightOptions=nil;lastUIKey=nil;lastHUD=-1;previewOptions=nil;lastPreviewHUD=-1
        isFlight=found~=nil;currentWorld=world;currentProfile=nil;lastSelected=-1
        -- 补给可能暂时移走玩家机体；保留确认事务和检查点，等待原生成功事件。
        if not supplySession then checkpointPending=false;restorePending=false;checkpointProfile=nil end
        note("WORLD_CHANGED "..world)
    end
    local selected=instance:GetSelectedPlaneID()
    if selected~=lastSelected then
        local previousPlaneId=planeId -- 页面错误属于引发错误的机体，换机后重新建立适配上下文。
        planeId=selected;lastSelected=selected;currentProfile=profileFor(planeId);previewOptions=nil
        if not isFlight and not supplySession then
            errorText=nil
            for _,adapter in ipairs({nativeLoadout,nativeStandby,nativeParts})do
                if adapter.error then
                    note("MENU_ERROR_CONTEXT_ENDED from="..previousPlaneId.." to="..planeId.." prior="..adapter.error)
                    adapter:reset(true,false) -- 原错误已写日志；仅新机体可重新接入，不循环重试失败页面。
                end
            end
        end
    end
    -- 玩家对象与 HUD 在 START MISSION 前已经生成；先建立显示，不提前初始化武器或消耗冷却。
    if isFlight and not configured and not supplySession and not errorText and valid(activator)and os.clock()-lastPreviewHUD>=.05 then
        lastPreviewHUD=os.clock()
        local saved=config.profiles[tostring(planeId)] -- 只展示已保存方案，开场期间不推导新的配弹比例。
        if saved then
            previewOptions=previewOptions or optionsFor(planeId)
            primeFlightHUD(copyProfile(saved),previewOptions)
        end
    end
    -- 起飞过场即可预建已确认的显示，不等玩家武器输入重新开放。
    if supplySession and supplySession.menuClosed and supplySession.preview and isFlight and valid(activator)and not errorText then
        primeFlightHUD(supplySession.preview,supplySession.options,true)
    end
    -- 若任务成功事件到达时玩家尚未回到世界，沿现有身份发现流程等待实例；不再等待武器输入开关。
    if supplySession and supplySession.success and not errorText then completeSupplyHandoff()end
    if isFlight and ready()and not configured and not supplySession and not errorText then
        local saved=config.profiles[tostring(planeId)]
        local options=optionsFor(planeId)
        local canUse=saved and Profile.validate(saved,options)
        assert(canUse,"没有已确认且可用的原生多 SP 配置，请返回机库通过原生兵装页确认后出击；管理界面不提供兵装选择")
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
            restorePending=false;assert(checkpointProfile,"检查点缺少对应的多 SP 配置")
            currentProfile=copyProfile(checkpointProfile)
            nativeHud:reset()
            weaponPresentation:reset()
            selectionAudio(0,0) -- 恢复前先隔离原生初始化提示。
            native("checkpoint_restore") -- 恢复快照自己的槽数、映射、弹药和冷却。
            instance:SetPlaneAndSWP(planeId,currentProfile.a)
            config.profiles[tostring(planeId)]=copyProfile(currentProfile);saveConfig()
            lastArmed=plane:IsSpecialWeaponArmed()
            selectionAudio(0,lastArmed and active+1 or 0)
            note("CHECKPOINT_RESTORED")
        end
        if checkpointPending then
            checkpointPending=false
            native("checkpoint_save")
            checkpointProfile=copyProfile(currentProfile);note("CHECKPOINT_CAPTURED")
        end
        observeOriginalSwitch()
        local mountOK,mountError=updateFixedMount(planeAddress,game:GetTimeSeconds(plane)) -- 三种 SP 统一更新，单挂由核心明确解绑。
        assert(mountOK,mountError)
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
    if supplySession and supplySession.menuActive and not supplySession.menuClosed and not errorText then
        -- 只在实际补给菜单打开期间扫描控件，不在正常飞行中扫描补给页。
        nativeSupply:pump(instance,supplySession.planeId,function()
                -- 新参数只用于新开的原生选择草稿，不改返航前方案及取消恢复。
                return copyProfile(supplySession.profile)
            end,
            function()return supplySession.options end,now,isFlight,panelOpen)
        if nativeSupply.error then error(nativeSupply.error)end
    elseif not isFlight and not supplySession then
        nativeLoadout:pump(instance,planeId,function()return profileFor(planeId)end,function()return optionsFor(planeId)end,now,false,panelOpen)
    end
    nativeStandby:pump(instance,planeId,function()
        local saved=config.profiles[tostring(planeId)];return saved and copyProfile(saved)or nil
    end,function()return optionsFor(planeId)end,now,isFlight or supplySession~=nil,panelOpen)
    nativeParts:pump(instance,planeId,function()
        local saved=config.profiles[tostring(planeId)];return saved and copyProfile(saved)or nil
    end,function()return optionsFor(planeId)end,now,isFlight or supplySession~=nil,panelOpen)
    local uiKey=tostring(panelOpen)..":"..requestOpen..":"..requestClose..":"..planeId..":"..tostring(configured)..":"..tostring(errorText)..":"..nativeHud.status..":"..weaponPresentation.status..":"..nativeLoadout.status..":"..nativeSupply.status..":"..tostring(internal)..":"..tostring(inputNotice)
    if uiKey~=lastUIKey or now-lastPublish>=(panelOpen and .1 or 1)then
        lastPublish=now;lastUIKey=uiKey;publishState()
    end
end
RegisterHook("/Script/Nimbus.NimbusGameInstance:SetPlaneAndSWP",function(ctx,selectedPlane,selectedWeapon)
    if not syncRuntime()then return end
    nativeLoadout:beforeNativeCommit(ctx,selectedPlane,selectedWeapon)
    nativeSupply:beforeNativeCommit(ctx,selectedPlane,selectedWeapon)
end,function()end)
-- 原生页面可能复用同一对象；真实输入激活事件作为新一轮准备的边界。
RegisterHook("/Script/Nimbus.MenuBaseWidget:SetIsEnableControl",function()end,function(ctx,enabled)
    if runtimeChanging then return end
    if not syncRuntime(enabled:get()and ctx:get()or nil)then return end
    nativeLoadout:onControlChanged(ctx,enabled)
    -- 激活事件内完成双栏，原生第一次绘制前即可使用模组文字；自身输入操作不重入。
    if not enabled:get()or nativeLoadout.controlDepth>0 or nativeSupply.controlDepth>0
        or not valid(instance)or not catalogue or panelOpen then return end
    local ok,err=xpcall(function()
        assert(IsInGameThread(),"菜单接入事件不在游戏线程")
        if instance:GetSelectedPlaneID()~=planeId then return end -- 机体改变交给主循环先更新身份。
        local owner=ctx:get() -- 只接入已核对的三个页面，不枚举其他菜单。
        if not isFlight and owner:IsA(StaticFindObject("/Script/Nimbus.CampaignHangarWeaponWidget"))then
            nativeLoadout:activate(owner,instance,planeId,function()return profileFor(planeId)end,function()return optionsFor(planeId)end,os.clock(),false,false)
        elseif not isFlight and owner:IsA(StaticFindObject("/Script/Nimbus.CampaignHangarReviewWidget"))then
            nativeStandby:activate(owner,instance,planeId,function()
                local saved=config.profiles[tostring(planeId)];return saved and copyProfile(saved)or nil
            end,function()return optionsFor(planeId)end,os.clock(),false,false)
        elseif not isFlight and owner:IsA(StaticFindObject("/Script/Nimbus.CampaignHangarPartsWidget"))then
            nativeParts:activate(owner,instance,planeId,function()
                local saved=config.profiles[tostring(planeId)];return saved and copyProfile(saved)or nil
            end,function()return optionsFor(planeId)end,os.clock(),false,false)
        elseif supplySession and supplySession.menuActive and not supplySession.menuClosed
            and owner:IsA(StaticFindObject("/Script/Nimbus.MissionResupplyWidget"))then
            nativeSupply:activate(owner,instance,supplySession.planeId,function()
                return copyProfile(supplySession.profile)
            end,function()return supplySession.options end,os.clock(),isFlight,false)
        end
    end,debug.traceback)
    if not ok then errorText=tostring(err);note("NATIVE_MENU_ACTIVATION_ERROR "..errorText);releaseRuntimeOwnership()end
end)
-- These reflected entry points cover normal dynamic mission events. Their
-- checkpoint/resupply coverage remains experimental and is logged explicitly.
RegisterHook("/Script/Nimbus.Mission:OnStartNextPhaseHandler",function()end,function()
    if not syncRuntime()then return end
    if configured then checkpointPending=true end
end)
RegisterHook("/Script/Nimbus.PlayerPlane:RestoreWeaponFromCheckpoint",function()end,function(ctx)
    if not syncRuntime()then return end
    if configured and valid(plane)and ctx:get():GetAddress()==plane:GetAddress()then
        -- 检查点重试中止尚未完成的返航草稿，恢复已捕获方案。
        if supplySession then nativeSupply:reset(false,false);ownSupplyInput(false);supplySession=nil;note("RESUPPLY_CANCELLED_CHECKPOINT")end
        restorePending=true
    end
end)
-- 生命周期失败明确停用武器修改；不能忽略异常后继续应用旧方案。
local function supplyHook(callback)
    return function(...)
        if runtimeChanging or errorText then return end
        local args={...}
        local ok,err=xpcall(function()
            assert(IsInGameThread(),"补给事件不在游戏线程")
            if not syncRuntime()then return end
            callback(table.unpack(args))
        end,debug.traceback)
        if not ok then errorText=tostring(err);note("NATIVE_RESUPPLY_ERROR "..errorText);releaseRuntimeOwnership()end
    end
end
RegisterHook("/Script/Nimbus.Mission:OnStartSupplyPhaseHandler",function()end,supplyHook(function(ctx)
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
    nativeSupply:refreshNow(instance,supplySession.planeId,function()
        return copyProfile(supplySession.profile)
    end,function()return supplySession.options end,os.clock(),isFlight,panelOpen)
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
overlay();note("DUALSP_LOADED 1.0.2-su30sm-three-sp.9 MANAGEMENT_UI")
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
    if performance.scene~=scene then performance={scene=scene,count=0,total=0,peak=0,since=now}end
    performance.count=performance.count+1;performance.total=performance.total+elapsed
    performance.peak=math.max(performance.peak,elapsed)
    if now-performance.since>=15 then
        note(string.format("PERF_TICK scene=%s samples=%d average_ms=%.3f peak_ms=%.3f",scene,performance.count,
            performance.total/performance.count,performance.peak))
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
            tick();recordTickCost(started)
        end,debug.traceback)
        if not ok then
            errorText=tostring(err);note("ERROR "..errorText)
            if runtimeChanging or not runtimeAllowed then
                -- 模式识别/清理失败只保留诊断，绝不沿用上次单人许可。
                runtimeAllowed=false;runtimeChanging=false;runtimeReason="context_error"
                local uiOK,uiErr=uiRuntimeGate(0);local coreOK,coreErr=nativeRuntimeGate(0)
                if not uiOK or not coreOK then note("RUNTIME_DISABLE_ERROR "..tostring(uiErr).." "..tostring(coreErr))end
                return false
            end
            releaseRuntimeOwnership()
            -- Report once, freeze weapon mutations, and keep the UI/error visible.
            -- A user closes the panel to resume the game; no hidden loadout retry.
            local published,publishErr=pcall(publishState)
            if not published then note("UI_PUBLISH_ERROR "..tostring(publishErr))end
        end
    return false
end)
