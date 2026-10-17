-- 只读识别当前模式与游戏文本语言；不根据旧配置猜测是否允许修改游戏。
local Context={language="zh",culture=nil,lastLanguage=-1}
local system,game,ui,localizer=nil,nil,nil,nil -- 经过反射预检的原生默认对象。
local modeClasses={} -- UIGameMode 继承多人基类，必须结合已知菜单世界先识别其实际用途。
local multiWidgets={} -- 多人机库并非都继承 MultiMenuBaseWidget，必须完整列出其原生类型。
local widgetKinds={} -- 按类全名缓存分类，不重复执行十余次继承检查。
local singleEntryClasses={} -- 只在明确回到标题、主菜单或单人入口时解除多人菜单锁定。
local toggleMenuClasses={} -- 总开关只在没有机库草稿或任务事务的入口页切换。
local multiplayerMenuWorld=nil -- 通用选项/对话框不能把多人菜单误判成单人。
local menuBaseClass=nil -- 实例列表中只有原生菜单基类才具有输入许可接口。
local lastHangarWorld,lastHangarTime=nil,-1 -- 切页短暂隐藏保留预览，明确离开机库立即解除。
local function valid(object)return object and object:IsValid()end

-- 接口来源为本 build 的原生反射快照，缺失时明确停止而非换用猜测路径。
local function initialize()
    if system then return end
    for _,path in ipairs({"/Script/Engine.KismetSystemLibrary:IsStandalone",
        "/Script/Engine.GameplayStatics:GetGameMode","/Script/Nimbus.UIManagerActor:GetCurrentUIManagerActor",
        "/Script/Nimbus.LocalizationManager:GetCulture"})do
        assert(valid(StaticFindObject(path)),"Runtime context API missing: "..path)
    end
    local checkedSystem=StaticFindObject("/Script/Engine.Default__KismetSystemLibrary") -- 成功预检后才提交缓存。
    game=StaticFindObject("/Script/Engine.Default__GameplayStatics")
    ui=StaticFindObject("/Script/Nimbus.Default__UIManagerActor")
    localizer=StaticFindObject("/Script/Nimbus.Default__LocalizationManager")
    assert(valid(checkedSystem)and valid(game)and valid(ui)and valid(localizer),"Runtime context defaults missing")
    menuBaseClass=StaticFindObject("/Script/Nimbus.MenuBaseWidget")
    assert(valid(menuBaseClass),"MenuBaseWidget type missing")
    for _,name in ipairs({"MultiPlayerGameMode","UIGameMode"})do
        modeClasses[name]=StaticFindObject("/Script/Nimbus."..name)
        assert(valid(modeClasses[name]),"Game mode type missing: "..name)
    end
    for _,name in ipairs({"MultiMenuBaseWidget","MultiMissionMenuBaseWidget","MultiHangarWidget",
        "MultiHangarAircraftSetWidget","MultiHangarAircraftTreeWidget","MultiHangarAircraftWidget",
        "MultiHangarEmblemSwitchWidget","MultiHangarEmblemWidget","MultiHangarNameWidget",
        "MultiHangarPartsWidget","MultiHangarRadioWidget","MultiHangarReviewWidget",
        "MultiHangarSkinWidget","MultiHangarSortieSelectWidget","MultiHangarWeaponWidget"})do
        local class=StaticFindObject("/Script/Nimbus."..name) -- 均已在当前 build 反射索引中核对。
        assert(valid(class),"Multiplayer menu type missing: "..name)
        multiWidgets[#multiWidgets+1]=class
    end
    for _,name in ipairs({"MainMenuWidget","MainTitleWidget","CampaignMenuMainWidget","CampaignFreeMissionWidget","CampaignHangarSortieWidget"})do
        local class=StaticFindObject("/Script/Nimbus."..name)
        assert(valid(class),"Single-player menu type missing: "..name);singleEntryClasses[#singleEntryClasses+1]=class
    end
    for _,name in ipairs({"MainMenuWidget","MainTitleWidget","CampaignMenuMainWidget","CampaignFreeMissionWidget"})do
        toggleMenuClasses[#toggleMenuClasses+1]=StaticFindObject("/Script/Nimbus."..name)
    end
    system=checkedSystem
end

-- 多人页面激活事件早于下一次轮询，锁定当前菜单世界直到明确离开多人入口。
function Context.enterMultiplayerMenu(instance)
    if valid(instance)and valid(instance:GetWorld())then multiplayerMenuWorld=instance:GetWorld():GetAddress()end
end

-- 原生菜单即使仍在单机网络 World 中，也能通过正在操作的页面识别多人入口。
function Context.isMultiplayerWidget(widget,observe)
    initialize()
    if not valid(widget)then return false end
    local name=widget:GetClass():GetFullName() -- 同一蓝图类的实例使用同一分类。
    if observe then Context.observedMenu=name end -- 复用本来就有的类名查询，不增加全局搜索。
    if widgetKinds[name]~=nil then return widgetKinds[name]end
    local multiplayer=false
    for _,class in ipairs(multiWidgets)do if widget:IsA(class)then multiplayer=true;break end end
    widgetKinds[name]=multiplayer
    return multiplayer
end

-- 每次调度和修改型事件都核对当前世界，不缓存上一世界的许可。
function Context.allowed(instance)
    Context.observedMenu="none"
    Context.hangarVisible=false
    if not valid(instance)then return false,"waiting_instance"end
    initialize()
    local world=instance:GetWorld() -- 载入期间没有世界时保持禁止。
    if not valid(world)then return false,"waiting_world"end
    if multiplayerMenuWorld and multiplayerMenuWorld~=world:GetAddress()then multiplayerMenuWorld=nil end
    if not system:IsStandalone(instance)then return false,"network_world"end
    local mode=game:GetGameMode(instance) -- 类型继承不等同于正在进行多人对局。
    local name=world:GetFullName() -- 共享菜单例外只开放给已知菜单地图，不放行未知世界。
    local menuWorld=name:find("/Game/Maps/Menu/",1,true)or name:find("/Game/UI/Menu/Menu.",1,true)
        or name:find("/Game/UI/Menu/Boot.",1,true)
    local sharedMenu=menuWorld and valid(mode)and mode:IsA(modeClasses.UIGameMode) -- 本次现场确认的角色。
    if valid(mode)and mode:IsA(modeClasses.MultiPlayerGameMode)and not sharedMenu then
        return false,"multiplayer_mode"
    end
    local manager=ui:GetCurrentUIManagerActor() -- 只遍历原生菜单实例列表，不扫描全局控件表。
    local currentManager=valid(manager)and valid(manager:GetWorld())and manager:GetWorld():GetAddress()==world:GetAddress()
    local singleEntry=false -- 需实际可见且已开放输入，缓存的隐藏主菜单不算退出多人。
    local hangarVisible,otherMenu=false,false
    if currentManager then
        local multiplayer=false
        manager.MenuWidgetArray:ForEach(function(_,value)
            local widget=value:get() -- 复用池中的隐藏页面不参与当前模式判断。
            if valid(widget)and widget:IsA(menuBaseClass)and widget:IsVisible()then
                local className=widget:GetClass():GetFullName()
                local widgetWorld=widget:GetWorld()
                if valid(widgetWorld)and widgetWorld:GetAddress()==world:GetAddress()then
                    if className:find("/Game/UI/Menu/Hangar/",1,true)then hangarVisible=true
                    else otherMenu=true end
                end
                if widget:IsEnableControl()then
                    if Context.isMultiplayerWidget(widget,true)then multiplayer=true end
                    for _,class in ipairs(singleEntryClasses)do if widget:IsA(class)then singleEntry=true;break end end
                end
            end
        end)
        if multiplayer then multiplayerMenuWorld=world:GetAddress();return false,"multiplayer_menu"end
    end
    if hangarVisible then lastHangarWorld=world:GetAddress();lastHangarTime=os.clock()end
    Context.hangarVisible=hangarVisible or (not otherMenu and lastHangarWorld==world:GetAddress()and os.clock()-lastHangarTime<.35)
    if multiplayerMenuWorld then
        if not singleEntry then return false,"multiplayer_menu_transition"end
        multiplayerMenuWorld=nil
    end
    if name:find("/Game/Maps/Mission/",1,true)or name:find("/Game/Maps/FreeFlight/",1,true)then
        return valid(mode),valid(mode)and "singleplayer"or "waiting_mode"
    end
    if sharedMenu and currentManager then return true,"singleplayer_menu"end
    return false,"unsupported_or_transition_world"
end

-- 管理面板暂借了原生输入时，只接受面板实际持有的可见页面，不放行隐藏主菜单。
function Context.canToggleMaster(instance,heldWidgets)
    local allowed=Context.allowed(instance)
    if not allowed then return false end
    local world=instance:GetWorld()
    local name=world:GetFullName()
    if not(name:find("/Game/Maps/Menu/",1,true)or name:find("/Game/UI/Menu/Menu.",1,true)
        or name:find("/Game/UI/Menu/Boot.",1,true))then return false end
    local function suitable(widget,held)
        if not valid(widget)or not widget:IsA(menuBaseClass)or not widget:IsVisible()
            or not valid(widget:GetWorld())or widget:GetWorld():GetAddress()~=world:GetAddress()
            or not(held or widget:IsEnableControl())then return false end
        for _,class in ipairs(toggleMenuClasses)do if widget:IsA(class)then return true end end
        return false
    end
    for _,widget in ipairs(heldWidgets or {})do if suitable(widget,true)then return true end end
    local manager=ui:GetCurrentUIManagerActor()
    local safe=false
    if valid(manager)and valid(manager:GetWorld())and manager:GetWorld():GetAddress()==world:GetAddress()then
        manager.MenuWidgetArray:ForEach(function(_,value)if suitable(value:get(),false)then safe=true end end)
    end
    return safe
end

-- 仅在许可变化时记录上下文，便于区分共享菜单与真正的多人对局，不增加周期日志。
function Context.describe(instance)
    if not valid(instance)then return "instance=none"end
    local world=instance:GetWorld() -- 和许可判断使用同一游戏实例。
    if not valid(world)then return "world=none"end
    local mode=game:GetGameMode(instance) -- 名称只用于诊断，不以名称替代类型判断。
    return "world="..world:GetFullName().." mode="..(valid(mode)and mode:GetClass():GetFullName()or "none")
end

-- 原生游戏页面使用游戏文本语言；不以配音语言或 Windows 语言判断。
function Context.updateLanguage(now)
    initialize()
    if now-Context.lastLanguage<1 then return false end
    Context.lastLanguage=now
    local value=localizer:GetCulture() -- 本 build 返回 FString。
    local culture=type(value)=="string"and value or value:ToString()
    assert(type(culture)=="string"and #culture>0,"Game text culture is empty")
    local changed=culture~=Context.culture
    Context.culture=culture;Context.language=culture:lower():match("^zh")and "zh"or "en"
    return changed
end

-- 本 Mod 提供中文和英文；其他游戏语言使用英文提示，原生说明仍由游戏翻译。
function Context.text(zh,en)return Context.language=="zh"and zh or en end
return Context
