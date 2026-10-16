-- FOV editor 1.1 的独立键盘适配；保留作者 PAK，不接入 DualSP 或鼠标飞控内部状态。
local actor=nil -- 当前任务中由本脚本创建的编辑器实例。
local owner=nil -- 编辑器对应的玩家飞机，换机或重载后重新绑定。
local engine=nil -- 原生引擎实例；只从活动视口寻找当前世界。
local gameplay=nil -- 原生游戏工具默认对象。
local system=nil -- 原生模式检查及屏幕提示工具。
local planeClass=nil -- 作者蓝图要求的 AcePlayerPawn 类型。
local multiplayerClass=nil -- 多人模式不载入编辑器。
local commands={} -- 按键回调仅排队；所有 UObject 操作均在游戏线程执行。
local stopped=false -- 接入失败后明确停止本适配，不反复尝试或影响其他模组。
local views={ -- 对应原生视角枚举、相机、作者设置字段和默认值。
    [0]={camera="FirstPersonCamera",setting="FirstPerson_FOV",current="Current_FP_FOV_Value",default=74,label="First person"},
    [1]={camera="CockpitCamera",setting="Cockpit_FOV",current="Current_CP_FOV_Value",default=74,label="Cockpit"},
    [2]={camera="ThirdPersonCamera",setting="ThirdPerson_FOV",current="Current_TP_FOV_Value",default=63,label="Third person"},
}

-- 原生对象有效性检查；加载过渡期间的空对象不属于异常。
local function valid(object)
    return object and object:IsValid()
end

-- 日志统一进入 UE4SS.log，便于与其他模组的加载和按键记录对照。
local function log(message)
    print("[AC7FOVKeyboard] "..message.."\n")
end

-- 明确显示当前操作或接入错误，避免只有日志而用户看不见原因。
local function show(world,message)
    system:PrintString(world,message,true,true,{R=0.2,G=1.0,B=0.3,A=1.0},5.0)
end

-- 只移除本脚本创建的控件和 Actor，不触碰其他模组的输入与界面。
local function release()
    if valid(actor)then
        local widget=actor["Log Widget"] -- 作者创建的数值提示控件。
        if valid(widget)then widget:RemoveFromParent()end
        actor:K2_DestroyActor()
    end
    actor=nil;owner=nil
end

-- 按 UE4 的资产注册表接口加载指定蓝图；不安装自动加载所有蓝图的全局加载器。
local function loadClass(path)
    local registry=StaticFindObject("/Script/AssetRegistry.Default__AssetRegistryHelpers") -- UE4 原生资产工具。
    assert(valid(registry),"AssetRegistryHelpers unavailable")
    local class=registry:GetAsset({ObjectPath=FName(path)}) -- GetAsset 同步加载指定资产。
    assert(valid(class),"Cannot load asset: "..path)
    return class
end

-- 获取活动单人任务及玩家飞机；菜单、多人和世界切换期间均不创建编辑器。
local function context()
    if not valid(engine)then engine=FindFirstOf("GameEngine")end
    if not valid(engine)or not valid(engine.GameViewport)then return end
    local world=engine.GameViewport.World -- 不能用全局找到的旧任务飞机代替当前世界。
    if not valid(world)then return end
    local name=world:GetFullName() -- 只接受已知战役/自由飞行地图。
    if not name:find("/Game/Maps/Mission/",1,true)and not name:find("/Game/Maps/FreeFlight/",1,true)then return end
    if not valid(gameplay)then
        gameplay=StaticFindObject("/Script/Engine.Default__GameplayStatics")
        system=StaticFindObject("/Script/Engine.Default__KismetSystemLibrary")
        multiplayerClass=StaticFindObject("/Script/Nimbus.MultiPlayerGameMode")
        assert(valid(gameplay)and valid(system)and valid(multiplayerClass),"Native context APIs unavailable")
    end
    local mode=gameplay:GetGameMode(world) -- 菜单已排除，不依赖多人基类判断菜单用途。
    if not valid(mode)or mode:IsA(multiplayerClass)or not system:IsStandalone(world)then return end
    local instance=world.OwningGameInstance -- 与飞控一致，只接受一个本地玩家。
    if not valid(instance)or #instance.LocalPlayers~=1 then return end
    local controller=instance.LocalPlayers[1].PlayerController -- 不捕获 AI 或预览机。
    if not valid(controller)then return end
    local pawn=controller.Pawn -- 作者 BeginPlay 也从当前玩家取得飞机。
    if not valid(pawn)then return end
    if not valid(planeClass)then planeClass=loadClass("/Game/Blueprint/Player/Pawn/AcePlayerPawn.AcePlayerPawn_C")end
    if not pawn:IsA(planeClass)then return end
    assert(valid(pawn.CameraViewComponent),"CameraViewComponent unavailable")
    for _,view in pairs(views)do assert(valid(pawn[view.camera]),"Camera unavailable: "..view.camera)end
    return world,controller,pawn
end

-- 在真实任务飞机就绪后创建编辑器，确保作者 BeginPlay 正确加载三组 FOV 和保存对象。
local function attach(world,controller,pawn)
    release()
    local class=loadClass("/Game/Mods/FOVEditor/ModActor.ModActor_C") -- 唯一要创建的外部蓝图。
    local defaults=class:GetCDO() -- 在创建之前禁止原有无线电/雷达组合输入。
    assert(valid(defaults),"FOV editor defaults unavailable")
    defaults.bAllowInGameInput=false
    actor=world:SpawnActor(class,{X=0,Y=0,Z=0},{Pitch=0,Yaw=0,Roll=0})
    assert(valid(actor),"FOV editor spawn failed")
    actor:DisableInput(controller)
    actor:SetActorTickEnabled(false) -- 本脚本提供输入，作者的逐帧按键循环不再需要。
    assert(actor.bAllowInGameInput==false,"FOV editor input isolation failed")
    assert(valid(actor.PlayerPlane)and actor.PlayerPlane:GetAddress()==pawn:GetAddress(),"FOV editor player initialization failed")
    assert(valid(actor.Settings)and valid(actor["Log Widget"]),"FOV editor settings/widget initialization failed")
    owner=pawn
    log("ATTACHED "..pawn:GetFullName())
    show(world,"FOV: Ctrl+PgUp / PgDn = +/-1; Ctrl+Home = reset; Ctrl+End = show")
end

-- 只调整当前视角，使用作者的设置存档及提示；不写相机位置、飞行输入或全局按键映射。
local function adjust(world,controller,pawn,command)
    local view=views[pawn.CameraViewComponent:GetCurrentCameraViewType()] -- 特殊镜头没有可调整项。
    local target=controller:GetViewTarget() -- 过场、暂停和其他操作界面不得误调。
    if not view or gameplay:IsGamePaused(world)or controller.bCinematicMode or controller:IsMoveInputIgnored()
        or not valid(target)or target:GetAddress()~=pawn:GetAddress()then
        show(world,"FOV: return to normal flight view before adjusting")
        return
    end
    local camera=pawn[view.camera] -- 直接读取光学 FOV，不使用鼠标飞控相机距离。
    local value=camera.FieldOfView -- 当前实时值，允许用户看见其他插件改变后的实际数值。
    assert(type(value)=="number"and value==value,"Invalid camera FOV")
    if command~="show"then
        value=command=="reset"and view.default or math.max(30,math.min(120,value+command))
        camera:SetFieldOfView(value)
        actor.Settings[view.setting]=value
        actor[view.current]=value
        -- 原方法忽略存盘返回值；这里直接检查同一原生保存接口，失败明确报错。
        assert(gameplay:SaveGameToSlot(actor.Settings,"Mods/FOVEditor/Settings",0),"FOV settings save failed")
    end
    actor:PushNotification()
    show(world,string.format("FOV %s: %.1f",view.label,value))
    log(string.format("FOV view=%s value=%.1f action=%s",view.label,value,tostring(command)))
end

-- 独立键盘组合键；不注册手柄输入，也不改 F6/F8 等现有管理与飞控快捷键。
local function bind(key,command)
    RegisterKeyBind(key,{ModifierKey.CONTROL},function()
        if not stopped then commands[#commands+1]=command end
    end)
end
bind(0x21,1) -- Windows VK_PRIOR：PageUp。
bind(0x22,-1) -- Windows VK_NEXT：PageDown。
bind(0x24,"reset") -- Windows VK_HOME：恢复当前视角的作者默认值。
bind(0x23,"show") -- Windows VK_END：显示当前数值。

assert(type(LoopInGameThreadAfterFrames)=="function","Bundled game-thread scheduler required")
log("LOADED 1.0; original radio/gamepad FOV input disabled; keyboard Ctrl+PgUp/PgDn/Home/End")
-- 固定注册一个游戏线程回调，避免每帧从异步状态不断创建回调。
LoopInGameThreadAfterFrames(15,function()
    if stopped then return true end
    local world=nil -- 供明确的错误提示使用，不把旧世界留到下次。
    local ok,err=xpcall(function()
        local controller,pawn -- 本次回调内重新核对活动上下文。
        world,controller,pawn=context()
        if not world then release();commands={};return end
        if not valid(actor)or not valid(owner)or owner:GetAddress()~=pawn:GetAddress()then
            -- 初次接入等到玩家取得控制，避免关卡刚创建飞机时 BeginPlay 尚未完成。
            release()
            if gameplay:IsGamePaused(world)or pawn.bEnableMovmentInput~=true or controller:IsMoveInputIgnored()then
                commands={};return
            end
            attach(world,controller,pawn)
        end
        local pending=commands -- 消费快照，按键回调只产生普通 Lua 数据。
        commands={}
        for _,command in ipairs(pending)do adjust(world,controller,pawn,command)end
    end,debug.traceback)
    if not ok then
        stopped=true;commands={};log("ERROR STOPPED: "..tostring(err))
        -- 清理失败也单独报告，绝不掩盖最初错误或继续修改相机。
        local cleaned,cleanupError=pcall(release)
        if not cleaned then log("CLEANUP ERROR: "..tostring(cleanupError))end
        if valid(world)and valid(system)then show(world,"FOV editor stopped: see AC7FOVKeyboard ERROR in UE4SS.log")end
    end
    return stopped
end)
