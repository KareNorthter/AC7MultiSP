-- 快速自由任务的独立请求、临时1至3SP方案及跨世界等待；不替换个人配装或旧检查点。
local Module={}
local function valid(object)return object and object:IsValid()end
local function address(object)return valid(object)and object:GetAddress()or 0 end
local function integer(value,minimum,maximum)return type(value)=="number"and value%1==0 and value>=minimum and value<=maximum end
function Module.new(deps)
    local Profile=assert(deps.profile,"快速任务缺少共用配装规则")
    local self={response=0,ack=0,status="idle",tasks={},planes={},generation=0,lastRefresh=-100,autoChecked=false}
    local path=deps.root.."quick-mission.json"
    local function normalize(value)
        assert(type(value)=="table"and (value.schema==1 or value.schema==2),"快速任务预设格式无效")
        assert(integer(value.mission_id,1,2147483647)and integer(value.plane_id,0,2147483647),"快速任务预设编号无效")
        assert(integer(value.difficulty,0,3)and type(value.auto_launch)=="boolean","快速任务预设参数无效")
        assert(value.skip_logos==nil or type(value.skip_logos)=="boolean","片头设置格式无效")
        local loadout=value.schema==1 and {a=value.swp,b=-1,c=-1}or {a=value.a,b=value.b,c=value.c}
        Profile.count(loadout) -- 读取旧预设只转换内存；提交保存或出击时按原流程备份原件。
        return {schema=2,mission_id=value.mission_id,plane_id=value.plane_id,a=loadout.a,b=loadout.b,c=loadout.c,difficulty=value.difficulty,
            auto_launch=value.auto_launch,skip_logos=value.skip_logos~=false}
    end
    self.saved={schema=2,mission_id=101,plane_id=109,a=0,b=-1,c=-1,difficulty=1,auto_launch=false,skip_logos=true}
    do
        local file=io.open(path,"rb")
        if file then
            local content=file:read("*a");file:close()
            local ok,value,schema=pcall(function()
                local original=deps.json.decode(content);return normalize(original),original.schema
            end)
            if ok then self.saved=value;self.lastSaved=schema==2 and deps.json.encode(value)or nil
            else self.error_key="quick.preset.invalid";deps.note("QUICK_MISSION_PRESET_ERROR "..tostring(value))end
        end
    end
    self.autoEligible=self.saved.auto_launch -- 只消费启动时的值；本次保存开启不会立即自动出击。
    local function changed()deps.changed()end
    local function native(op,menu,preset)
        local context=deps.context();local instance=context.instance
        assert(valid(instance)and valid(instance:GetWorld())and valid(instance.GameInformation),"快速任务GI尚未就绪")
        local table=instance.GameInformation.MissionDataTable
        assert(valid(table),"快速任务表尚未就绪")
        preset=preset or self.saved
        local ok,text=deps.native(op,instance:GetAddress(),instance:GetWorld():GetAddress(),table:GetAddress(),address(menu),
            preset.mission_id,preset.plane_id,preset.a,preset.difficulty)
        assert(ok,text);return deps.json.decode(text)
    end
    local function license(dlc)
        return deps.aircraft:contentOwned(dlc)==true
    end
    function self:busy()return self.pending~=nil end
    function self:waiting()return self.pending and self.pending.stage~="weapons"end
    function self:startup()
        if not self.saved.skip_logos or not deps.enabled()or self.logoFailed then return end
        -- 视觉辅助失败只停本次跳过，不向主调度冒泡并隔离当前机体。
        local startupOK,startupError=pcall(function()
            local instance=deps.context().instance
            if not valid(instance)or not valid(instance:GetWorld())then return end
            local world=instance:GetWorld()
            if not world:GetFullName():find("/Game/UI/Menu/Boot.",1,true)then return end
            local ui=StaticFindObject("/Script/Nimbus.Default__UIManagerActor")
            local class=StaticFindObject("/Script/Nimbus.CopyrightWidget")
            if not valid(ui)or not valid(class)then return end
            local manager=ui:GetCurrentUIManagerActor()
            if not valid(manager)or address(manager:GetWorld())~=address(world)then return end
            manager.MenuWidgetArray:ForEach(function(_,value)
                local widget=value:get()
                if not valid(widget)or not widget:IsA(class)or not widget:IsVisible()or not widget:IsEnableControl()
                    or address(widget:GetWorld())~=address(world)or self.logoWidget==address(widget)then return end
                local ok,accepted,err=pcall(deps.logos,address(instance),address(world),address(widget))
                if not ok or err then
                    self.logoFailed=true;self.error_key="quick.logos_failed";deps.note("STARTUP_LOGOS_FAILED "..tostring(err or accepted));changed()
                elseif accepted then
                    -- 本构建没有可调用的Widget:SetRenderOpacity；由原生结束状态完成交接。
                    self.logoWidget=address(widget);deps.note("STARTUP_LOGOS_NATIVE_WAIT")
                end
            end)
        end)
        if not startupOK then
            self.logoFailed=true;self.error_key="quick.logos_failed"
            deps.note("STARTUP_LOGOS_FAILED "..tostring(startupError));changed()
        end
    end
    function self:profile(id)
        if self.flight and self.flight.id==id then return Profile.copy(self.flight.profile)end
    end
    function self:validateProfile(id,options)
        local profile=self:profile(id)
        if not profile then return true end
        local ok,err=pcall(Profile.validate,profile,options,true)
        if not ok then
            self:fail("quick.loadout_invalid",err,true)
            return false -- 用户选择不合格组合不等于机体故障，不向主循环抛错或隔离整个机体。
        end
        return true
    end
    function self:fail(key,detail,block)
        local stage=self.pending and self.pending.stage or "validation"
        self:releaseSkip()
        self.pending=nil;self.flight=nil;self.status="failed";self.error_key=key
        if block then
            local instance=deps.context().instance
            self.blocked=tostring(detail);self.blockWorld=valid(instance)and address(instance:GetWorld())or 0;deps.block(self.blocked)
        end
        deps.note("QUICK_MISSION_FAILED stage="..stage.." "..tostring(detail));changed()
    end
    function self:failed(detail)if self.pending then self:fail("quick.failed",detail,true)end end
    function self:refresh(force)
        if not force and os.time()-self.lastRefresh<5 then return end
        self.lastRefresh=os.time()
        local ok,err=pcall(function()
            local context=deps.context();local menu=deps.menu()
            assert(valid(menu),"快速任务只允许主菜单启动")
            local result=native(0,menu)
            assert(result.protocol==1 and type(result.tasks)=="table"and #result.tasks==23,"快速任务原生协议不匹配")
            self.tasks=result.tasks
            for _,task in ipairs(self.tasks)do task.licensed=license(task.dlc_id)end
            self.planes={};deps.aircraft:refreshOptions(true)
            for _,option in ipairs(deps.aircraft.options)do
                local entry=deps.catalogue()[tostring(option.plane_id)]
                if entry and entry.supported and type(entry.weapons)=="table"and #entry.weapons==3 then
                    self.planes[#self.planes+1]={plane_id=option.plane_id,label=option.label,code=option.code,weapons=entry.weapons,dlc_id=entry.dlc_id}
                end
            end
            self.dataError=nil
        end)
        if not ok then self.dataError=tostring(err);deps.note("QUICK_MISSION_CATALOGUE_ERROR "..self.dataError)end
    end
    local function writePreset(preset)
        local encoded=deps.json.encode(preset)
        if encoded==self.lastSaved then self.saved=preset;return end
        -- 新文件落盘后才让出旧配置；失败保留原件，不删除或覆盖备份。
        local pending=path..".pending"
        local file=assert(io.open(pending,"wb"));local written,why=file:write(encoded);local closed,closeWhy=file:close()
        assert(written,why);assert(closed,closeWhy)
        local previous=io.open(path,"rb");local backup=nil
        if previous then
            previous:close();backup=deps.root.."quick-mission.previous-"..os.date("%Y%m%d-%H%M%S").."-"..self.response..".json"
            assert(os.rename(path,backup))
        end
        local committed,err=os.rename(pending,path)
        if not committed and backup then os.rename(backup,path)end
        assert(committed,err);self.saved=preset;self.lastSaved=encoded
    end
    function self:request(action)
        if not integer(action.request,1,2147483647)or action.request<=self.response then return end
        self.response=action.request;changed()
        if action.type=="quick_mission_cancel_auto"then self.autoEligible=false;self.autoChecked=true;self.ack=action.request;return end
        if self.pending then return end
        local source=nil
        local ok,err=pcall(function()
            assert(action.quick_protocol==2 and action.generation==self.generation,"快速任务请求已过期")
            local context=deps.context();assert(deps.enabled(),"快速任务需要先开启Mod")
            local menu=deps.menu();assert(valid(menu),"快速任务只允许主菜单启动")
            local preset=normalize(action.preset)
            if action.type=="quick_mission_save"then
                writePreset(preset);self.error_key=nil;self.status="saved";self.ack=action.request;return
            end
            assert(action.type=="quick_mission_start","快速任务操作无效")
            assert(not deps.otherBusy(),"正在处理其他调试事务")
            self:refresh(true);assert(not self.dataError,self.dataError)
            local task,target=nil,nil
            for _,entry in ipairs(self.tasks)do if entry.mission_id==preset.mission_id then task=entry end end
            for _,entry in ipairs(self.planes)do if entry.plane_id==preset.plane_id then target=entry end end
            assert(task and target,"快速任务目标不在当前目录中")
            assert(license(task.dlc_id)and license(target.dlc_id),"快速任务DLC许可尚未确认")
            writePreset(preset)
            source={instance=address(context.instance),world=address(context.instance:GetWorld()),plane=context.instance:GetSelectedPlaneID(),
                swp=context.instance:GetSelectedPlaneSWP(),skin=context.instance:GetSelectedPlaneSkinNo()}
            self.pending={instance=source.instance,sourceWorld=source.world,started=os.time(),task=task,target=target,preset=preset,
                profile=Profile.withSettings(preset,deps.ammoSettings()),skippedMini={},lastSkip=-1,stage="preparing"}
            self.flight=nil;self.blocked=nil;self.autoEligible=false;self.autoChecked=true
            deps.prepare()
            deps.note("QUICK_MISSION_PREPARED task="..task.code.." request="..action.request)
            assert(address(context.instance)==source.instance and address(context.instance:GetWorld())==source.world,"快速任务准备时世界已变化")
            context.instance:SetPlaneAndSWP(preset.plane_id,preset.a)
            context.instance:SetSelectedPlaneSkinNo(0)
            assert(context.instance:GetSelectedPlaneID()==preset.plane_id and context.instance:GetSelectedPlaneSWP()==preset.a,
                "快速任务机体选择被原生拒绝")
            self.pending.stage="native_request"
            deps.note("QUICK_MISSION_NATIVE_REQUEST task="..task.code.." plane="..preset.plane_id.." configs="..preset.a..","..preset.b..","..preset.c)
            local result=native(1,menu,preset)
            assert(result.protocol==1 and result.accepted==true,"原生加载器没有接受快速任务")
            self.pending.accepted=true;self.pending.stage="loading";self.ack=action.request;self.status="loading";self.error_key=nil
            deps.note("QUICK_MISSION_ACCEPTED task="..task.code.." plane="..preset.plane_id.." configs="..preset.a..","..preset.b..","..preset.c);changed()
        end)
        if not ok then
            local context=deps.context()
            if source and address(context.instance)==source.instance and address(context.instance:GetWorld())==source.world
                and self.pending and not self.pending.accepted then
                pcall(function()context.instance:SetPlaneAndSWP(source.plane,source.swp);context.instance:SetSelectedPlaneSkinNo(source.skin)end)
            end
            self:fail("quick.failed",err,false)
        end
    end
    function self:configured(id)
        if self.pending and self.flight and self.flight.id==id then
            self.pending=nil;self.status="applied";self.error_key=nil;changed();deps.note("QUICK_MISSION_APPLIED plane="..id)
        end
    end
    function self:releaseSkip()
        local owner=self.skipOwner;self.skipOwner=nil
        local instance=deps.context().instance
        if valid(instance)and valid(owner)and address(owner:GetWorld())==address(instance:GetWorld())then
            local ok,err=pcall(function()owner:SetIsSkipIGC(false)end)
            if not ok then deps.note("QUICK_MISSION_SKIP_RELEASE_ERROR "..tostring(err))end
        end
    end
    function self:pump(reason)
        local context=deps.context();local instance=context.instance
        if self.pending and os.time()-self.pending.started>180 then
            self:fail("quick.timeout","快速任务加载或飞控就绪超时",valid(instance));return false
        end
        if not valid(instance)then return self.pending~=nil end
        local world=instance:GetWorld();local map=valid(world)and world:GetFullName()or ""
        if self.blocked and address(world)~=self.blockWorld then self.blocked=nil;self.blockWorld=nil end
        if self.flight and (address(instance)~=self.flight.instance or map~=""and map~=self.flight.map)then self.flight=nil end
        local pending=self.pending
        if not pending then return false end
        if address(instance)~=pending.instance then self:fail("quick.changed","快速任务GI已变化",false);return false end
        local transition=map:find("/Game/Maps/Transition/Transition.",1,true)~=nil
        if reason=="network_world"or reason=="multiplayer_menu"or reason=="multiplayer_menu_transition"or reason=="multiplayer_mode"and not transition then
            self:fail("quick.changed","快速任务进入了非预期网络或多人环境",false);return false
        end
        if not valid(world)or transition then return true end -- 等待数据不授予过渡世界兵装许可。
        if map:find("/Game/UI/Menu/Menu.",1,true)then
            if address(world)~=pending.sourceWorld or os.time()-pending.started>15 then self:fail("quick.changed","快速任务未离开主菜单或已返回菜单",false);return false end
            return true
        end
        if not map:find("/Game/Maps/Mission/"..pending.task.code.."/",1,true)then
            if map:find("/Game/Maps/Mission/",1,true)then self:fail("quick.changed","进入了其他测试任务",true);return false end
            return true
        end
        if instance:GetMissionID()~=pending.preset.mission_id then self:fail("quick.changed","快速任务编号与目标世界不符",true);return false end
        local game=deps.game();local owner=game:GetPlayerController(instance,0)
        if not valid(owner)or address(owner:GetWorld())~=address(world)or not owner:IsLocalController()or not owner:IsLocalPlayerController()then return true end
        local pawn=game:GetPlayerPawn(instance,0)
        local ready=valid(pawn)and pawn:IsA(StaticFindObject("/Script/Nimbus.PlayerPlane"))and address(pawn:GetWorld())==address(world)
            and address(owner:K2_GetPawn())==address(pawn)and pawn.bEnableWeaponInput and not pawn:IsWeaponSystemDisabled()
        if ready then
            self:releaseSkip()
            if not deps.matches(pawn,pending.target.code)then self:fail("quick.wrong_aircraft","快速任务实际玩家不是指定机体",true);return false end
            deps.bind(pawn,map)
            if pending.task.fixed_sp then
                self.pending=nil;self.status="native_sp";self.error_key=nil;changed();return false
            end
            self.flight={id=pending.preset.plane_id,profile=Profile.copy(pending.profile),instance=pending.instance,map=map}
            if pending.stage~="weapons"then pending.stage="weapons";self.status="weapons";changed()end
            return false -- 主调度核对真实多挂资格并完成完整临时配装后才确认成功。
        end
        if not pending.skipFailed and os.time()~=pending.lastSkip then
            pending.lastSkip=os.time()
            local skipped,skipError=pcall(function()
                assert(valid(StaticFindObject("/Script/Nimbus.NimbusPlayerController:SetIsSkipIGC"))and
                    valid(StaticFindObject("/Script/Nimbus.Mission:SkipMiniGame")),"快速任务跳过接口缺失")
                owner:SetIsSkipIGC(true);self.skipOwner=owner
                -- 不调用默认对象的GetCurrentMission；只认当前世界唯一的实际任务实例。
                local mission=nil
                for _,candidate in ipairs(FindAllOf("Mission")or{})do
                    if valid(candidate)and not candidate:HasAnyFlags(0x10)and address(candidate:GetWorld())==address(world)then
                        assert(not mission,"快速任务当前世界的任务实例不唯一")
                        mission=candidate
                    end
                end
                if valid(mission)and address(mission:GetWorld())==address(world)then
                    local phase=mission.CurrentPhase
                    -- 只跳本次出击的起飞/开场补油；不取消任务阶段、补给菜单或战斗玩法。
                    if valid(phase)and phase:IsA(StaticFindObject("/Script/Nimbus.MiniGamePhase"))then
                        local label=phase:GetClass():GetFullName()
                        if (label:find("Takeoff",1,true)or label:find("Refuel",1,true))and not pending.skippedMini[address(phase)]then
                            pending.skippedMini[address(phase)]=true;mission:SkipMiniGame()
                        end
                    end
                end
            end)
            if not skipped then
                pending.skipFailed=true;self:releaseSkip();self.error_key="quick.intro_failed"
                deps.note("QUICK_MISSION_SKIP_ERROR "..tostring(skipError));changed()
            end
        end
        return true
    end
    function self:updateAuto()
        if not self.autoEligible or self.autoChecked or self.pending then return end
        if not deps.enabled()then return end
        local menu=deps.menu()
        if not valid(menu)then self.autoReady=nil;return end
        if deps.panel()then self.autoReady=nil;return end
        self.autoReady=self.autoReady or os.time()
        if os.time()-self.autoReady<2 then return end
        self.autoChecked=true
        self:request{type="quick_mission_start",quick_protocol=2,request=self.response+1,generation=self.generation,preset=self.saved}
    end
    function self:state()
        local menu=deps.enabled()and deps.menu()or nil
        local token=valid(menu)and (address(deps.context().instance)..":"..address(menu))or ""
        if token~=self.menuToken then self.menuToken=token;self.generation=self.generation+1;self.lastRefresh=-100 end
        if valid(menu)and not self.pending then self:refresh(false)end
        return {protocol=2,available=valid(menu)and not self.pending and not self.dataError and not deps.otherBusy()or false,
            reason_key=not deps.enabled()and "quick.mod_off"or not valid(menu)and "quick.main_menu_only"or self.dataError and "quick.unavailable"or "",
            generation=self.generation,response=self.response,ack=self.ack,status=self.status,error_key=self.error_key,
            tasks=self.tasks,planes=self.planes,preset=self.saved,auto_pending=self.autoEligible and not self.autoChecked}
    end
    return self
end
return Module
