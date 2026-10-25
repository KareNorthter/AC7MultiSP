-- 调试换机走原生整关重开；只在新的受控玩家机体真正就绪后确认成功。
local Module={}
-- 官方编号用于区分来源；候选来自实际目录，带DLC要求的机体必须核实拥有许可。
local official={
    [100]="f04e",[101]="f15c",[102]="f15e",[103]="f18f",[104]="f15j",[105]="f35c",[106]="a10a",[107]="f02a",
    [108]="f14d",[109]="f16c",[110]="f22a",[111]="j39e",[112]="m21b",[113]="m29a",[114]="m31b",[115]="mr2k",
    [116]="pkfa",[117]="rflm",[118]="su33",[119]="su34",[120]="su35",[121]="su37",[122]="su47",[123]="typn",
    [124]="yf23",[125]="su30",[126]="adf11f",[127]="x02s",[128]="su30sm",[129]="f104",[130]="zoef",[131]="mrgn",
    [132]="dark",[133]="f18e",[134]="f14a",[135]="f18etg",[136]="f14atg",[137]="su57tg",[138]="asfx",[139]="fa27",
    [140]="fa44",[141]="f15m",[142]="f16x",[143]="fb22",[145]="f18x",[146]="f02x",[147]="m35d"
}
local function valid(object)return object and object:IsValid()end
local function integer(value)return type(value)=="number"and value%1==0 end
local function address(object)return valid(object)and object:GetAddress()or nil end
function Module.new(deps)
    local translate=assert(deps.translate,"缺少换机通知语言提供器")
    -- 状态桥沿用原中文匹配文案；F6按手动语言从同一catalog取显示值，不受游戏语言影响。
    local function tr(key)return translate(key,"zh")end
    local self={response=0,ack=0,status="idle",notice="",options={},lastOptions=-1}
    local aircraftNames,nameInstance,nameCulture={}
    local function nameResolver(instance)
        -- 只缓存字符串；原生对象仅在本次刷新中使用，接口未就绪不影响候选资格。
        local ok,localizer,culture=pcall(function()
            local object=StaticFindObject("/Script/Nimbus.Default__LocalizationManager")
            if not valid(object)then return end
            local value=object:GetCulture()
            local text=type(value)=="string"and value or value:ToString()
            if type(text)~="string"or text==""then return end
            return object,text
        end)
        if not ok or not localizer or not culture then return function()end end
        local current=address(instance)
        if current~=nameInstance or culture~=nameCulture then
            aircraftNames={};nameInstance=current;nameCulture=culture
        end
        return function(code)
            local key="AircraftShort_Name_"..code:lower()
            if aircraftNames[key]then return aircraftNames[key]end
            local readOK,name=pcall(function()
                if not localizer:HasString(key)then return end
                return localizer:LocalizeString(key,true,""):ToString()
            end)
            if readOK and type(name)=="string"and name:find("%S")and #name<=256
                and name~=key and not name:find("%c")then
                aircraftNames[key]=name;return name
            end
        end
    end
    local function changed()deps.changed()end
    local function waiting(pending,stage)
        -- 只在阶段变化时记一次，不把换机等待变成逐帧日志。
        if pending.stage~=stage then
            pending.stage=stage;deps.note("DEBUG_AIRCRAFT_WAIT request="..pending.request.." stage="..stage)
        end
    end
    local function fail(message,stopFlight)
        self.pending=nil;self.status="failed";self.error=tostring(message);self.notice=tr("aircraft.change.failed.1db41bbb")..self.error
        if stopFlight then self.blocked=self.error;deps.failFlight(self.error)end
        deps.note("DEBUG_AIRCRAFT_FAILED "..self.error);changed()
    end
    local function controller(context,pawn)
        assert(valid(pawn)and valid(context.instance),"当前玩家机体尚未就绪")
        local world=context.instance:GetWorld()
        assert(valid(world)and address(pawn:GetWorld())==address(world),"玩家机体不属于当前任务世界")
        assert(address(deps.game():GetPlayerPawn(context.instance,0))==address(pawn),"当前机体未由本地玩家持有")
        local owner=pawn:GetController()
        assert(valid(owner)and owner:IsA(StaticFindObject("/Script/Engine.PlayerController")),"没有本地玩家控制器")
        assert(owner:IsLocalController()and owner:IsLocalPlayerController(),"当前控制器不是本地玩家")
        assert(address(owner:GetWorld())==address(world)and address(owner:K2_GetPawn())==address(pawn),"控制器与当前机体不匹配")
        assert(address(deps.game():GetGameInstance(pawn))==address(context.instance),"玩家机体的游戏实例已变化")
        return owner
    end
    local function rollback(pending)
        -- 仅原世界和原玩家仍然在场时恢复选择，不向已重开的世界写旧数据。
        local context=deps.context()
        if address(context.instance)~=pending.instance or not valid(context.instance)then return false end
        if address(context.instance:GetWorld())~=pending.world or address(context.plane)~=pending.plane then return false end
        if not deps.allowed()then return false end
        local ok,err=pcall(function()
            context.instance:SetPlaneAndSWP(pending.sourceId,pending.sourceWeapon)
            context.instance:SetSelectedPlaneSkinNo(pending.sourceSkin)
            context.instance:SetSelectedPlaneEmblemID(pending.sourceEmblem)
            assert(context.instance:GetSelectedPlaneID()==pending.sourceId and context.instance:GetSelectedPlaneSWP()==pending.sourceWeapon,
                "原机体选择未恢复")
            self.flight={id=pending.sourceId,profile=deps.copy(pending.sourceProfile),instance=pending.instance,map=pending.map}
        end)
        if not ok then deps.note("DEBUG_AIRCRAFT_ROLLBACK_ERROR "..tostring(err))end
        return ok
    end
    function self:busy()return self.pending~=nil end
    function self:cancel(reason)
        if self.pending then fail(reason)end
        self.flight=nil;self.blocked=nil;self.options={};self.lastOptions=-1
    end
    function self:profile(id)
        if self.flight and self.flight.id==id then return deps.copy(self.flight.profile)end
    end
    function self:resolveProfile(id,options)
        if self.flight and self.flight.id==id and self.flight.initial then
            self.flight.profile=deps.defaultProfile(options);self.flight.initial=false
        end
        return self:profile(id)
    end
    -- Patch是游戏补丁内容标记，不是付费许可；两种换机入口共用此判断。
    function self:contentOwned(dlcID)
        if dlcID=="Base"or dlcID=="Patch"or dlcID==""then return true end
        if type(dlcID)~="string"then return nil end
        local instance=deps.context().instance
        local dlc=valid(instance)and instance.DLCInformation or nil
        if not valid(dlc)then return nil end
        local checked,owned=pcall(function()return dlc:IsDLCOwned(dlcID)end)
        if checked then return owned==true end
    end
    function self:refreshOptions(force)
        if not force and os.clock()-self.lastOptions<1 then return end
        self.lastOptions=os.clock();self.options={};self.optionsError=nil
        local ok,err=pcall(function()
            local context=deps.context();assert(valid(context.instance),"游戏实例尚未就绪")
            local modName=nameResolver(context.instance)
            local owned,ownedKnown={},false
            local readOK,encoded=pcall(function()
                local accepted,text=deps.owned(context.instance:GetAddress());assert(accepted,text)
                local purchased=deps.json.decode(text);assert(type(purchased)=="table","已购机体列表无效")
                for _,id in ipairs(purchased)do assert(integer(id)and id>=0,"已购机体编号无效");owned[id]=true end
            end)
            ownedKnown=readOK
            if not readOK and self.ownedError~=tostring(encoded)then deps.note("DEBUG_AIRCRAFT_OWNERSHIP_UNKNOWN "..tostring(encoded))end
            self.ownedError=not readOK and tostring(encoded)or nil
            for key,info in pairs(deps.catalogue()or{})do
                local id=tonumber(key)
                -- 只接入目录中有完整三SP数据的机体；Mod仍须走原生重开及新玩家类身份确认。
                if integer(id)and id>=0 and id<=2147483647 and type(info)=="table"and type(info.code)=="string"
                    and #info.code>0 and #info.code<=128 and not info.code:find("[/%c%.]")
                    and info.supported==true and type(info.weapons)=="table"and #info.weapons==3
                    and not deps.nativeOnly(id,info)then
                    local isMod=official[id]~=info.code
                    local supplied=type(info.name)=="string"and info.name~=""and info.name or info.code:upper()
                    local name=deps.names[info.code]or supplied
                    local label=name
                    local licensed=nil
                    local extra=type(info.dlc_id)=="string"and info.dlc_id~=""and info.dlc_id~="Base"and info.dlc_id~="Patch"
                    if extra then
                        licensed=self:contentOwned(info.dlc_id)
                        label=label..(licensed==false and tr("dlc.not.owned.8f080e7d")or licensed==nil and tr("dlc.ownership.unknown.f1a7fac5")or "")
                    elseif not isMod and type(info.dlc_id)~="string"then label=label..tr("ownership.data.unavailable.b02bb5c3")
                    elseif not isMod and ownedKnown and not owned[id]then label=label..tr("not.purchased.336eda21")end
                    -- 无法确认DLC许可时不提供候选；替换官方机体的Mod同样核对目录DLCID。
                    local permitted=extra and licensed==true or not extra and (isMod or self:contentOwned(info.dlc_id)==true)
                    if permitted then
                        if isMod then
                            local localized=modName(info.code)
                            if localized then label=localized..label:sub(#name+1);name=localized end
                        end
                        self.options[#self.options+1]={plane_id=id,name=name,label=label,code=info.code,mod=isMod,
                            purchased=ownedKnown and owned[id]==true or nil,dlc_owned=licensed}
                    end
                end
            end
            table.sort(self.options,function(a,b)
                local left,right=a.name:lower(),b.name:lower()
                return left==right and a.plane_id<b.plane_id or left<right
            end)
        end)
        if not ok then self.options={};self.optionsError=tostring(err)end
    end
    function self:state(session,revision,includeOptions)
        local allowed,reason=deps.canChange()
        if self.pending then allowed=false;reason=tr("restarting.the.mission.and.waiting.996bed6e")end
        if allowed and includeOptions then self:refreshOptions(false)end
        if self.optionsError then allowed=false;reason=self.optionsError end
        return {protocol=2,session=session,revision=revision,available=allowed==true,reason=reason or "",
            options=includeOptions and self.options or nil,response=self.response,ack=self.ack,status=self.status,mode="restart",notice=self.notice,error=self.error}
    end
    function self:request(action)
        if not integer(action.request)or action.request<=self.response then return end
        self.response=action.request;changed()
        if self.pending then
            -- 不以重复点击打断已接受的唯一事务。
            deps.note("DEBUG_AIRCRAFT_REJECTED busy request="..action.request);return
        end
        local pending=nil
        local ok,err=pcall(function()
            assert(action.debug_aircraft_protocol==2,"换机界面协议不匹配")
            assert(deps.allowed(),"当前不是已启用Mod的单人任务")
            local allowed,reason=deps.canChange();assert(allowed,reason)
            local context=deps.context()
            assert(action.session==context.session and action.revision==context.revision,"任务或兵装已变化，请重新打开调试页")
            assert(integer(action.target_plane_id)and action.target_plane_id~=context.id,"请选择其他机体")
            self:refreshOptions(true)
            assert(not self.optionsError,self.optionsError)
            local target=nil
            for _,option in ipairs(self.options)do if option.plane_id==action.target_plane_id then target=option end end
            assert(target,"目标不是当前已加载目录中的可用三SP机体")
            for _,path in ipairs({"/Script/Engine.PlayerController:RestartLevel","/Script/Engine.Pawn:GetController",
                "/Script/Engine.Controller:K2_GetPawn","/Script/Engine.Controller:IsLocalController",
                "/Script/Engine.Controller:IsLocalPlayerController","/Script/Engine.GameplayStatics:GetGameInstance",
                "/Script/Nimbus.NimbusGameInstance:SetSelectedPlaneSkinNo","/Script/Nimbus.NimbusGameInstance:GetSelectedPlaneSkinNo",
                "/Script/Nimbus.NimbusGameInstance:SetSelectedPlaneEmblemID","/Script/Nimbus.NimbusGameInstance:GetSelectedPlaneEmblemID"})do
                assert(valid(StaticFindObject(path)),"原生换机接口缺失："..path)
            end
            local owner=controller(context,context.plane)
            assert(context.instance:GetSelectedPlaneID()==context.id,"任务机体与原生选择不符")
            pending={request=action.request,target=target.plane_id,code=target.code,name=target.name,instance=address(context.instance),
                world=address(context.instance:GetWorld()),map=context.instance:GetWorld():GetFullName(),plane=address(context.plane),
                sourceId=context.id,sourceWeapon=context.instance:GetSelectedPlaneSWP(),sourceSkin=context.instance:GetSelectedPlaneSkinNo(),
                sourceEmblem=context.instance:GetSelectedPlaneEmblemID(),sourceProfile=deps.copy(context.profile),started=os.clock()}
            self.pending=pending -- 原生调用可能触发生命周期钩子，先隔离旧兵装和检查点。
            local restart=deps.prepare()
            assert(type(restart)=="function","换机缺少已核对的暂停交接")
            context.instance:SetPlaneAndSWP(target.plane_id,0) -- 每机原生默认SP，无需购买额外兵装。
            context.instance:SetSelectedPlaneSkinNo(0)
            assert(context.instance:GetSelectedPlaneID()==target.plane_id and context.instance:GetSelectedPlaneSWP()==0,
                "原生机体选择没有接受目标机体")
            assert(context.instance:GetSelectedPlaneSkinNo()==0,"基础涂装没有就绪")
            self.ack=action.request;self.status="restarting";self.error=nil
            self.notice=tr("restarting.the.mission.waiting.for.492c291f")..target.name..tr("to.spawn.dfae5745")
            deps.note("DEBUG_AIRCRAFT_RESTART request="..action.request.." from="..context.id.." to="..target.plane_id)
            if restart(owner)==false then
                -- 原生关菜单可能跨调度完成，保存唯一事务，等待归还后才发出一次重开。
                pending.resume=restart;pending.resumeOwner=address(owner)
                self.notice=tr("waiting.for.the.native.pause.1135f7ad")
                waiting(pending,"waiting_native_resume");changed()
            end
        end)
        if not ok then
            local recovered=not pending or rollback(pending)
            fail(err,not recovered)
        end
    end
    function self:pump(reason)
        if not self.pending and not self.flight and not self.blocked then return false end -- 普通飞行不增加世界/类链查询。
        local context=deps.context()
        local world=valid(context.instance)and context.instance:GetWorld()or nil
        local map=valid(world)and world:GetFullName()or nil
        if self.flight and (address(context.instance)~=self.flight.instance or
            map and map~=self.flight.map)then
            self.flight=nil
        end
        if map then
            if map:find("/Game/Maps/Menu/",1,true)or map:find("/Game/UI/Menu/",1,true)then self.blocked=nil end
        end
        local pending=self.pending
        if not pending then return false end
        if address(context.instance)~=pending.instance then self:cancel("游戏实例已改变，请重新选择机体");return false end
        if reason=="network_world"or reason=="multiplayer_mode"or reason=="multiplayer_menu"or reason=="multiplayer_menu_transition"then
            self:cancel("已离开单人任务，换机请求已取消");return false
        end
        if valid(world)then
            if map:find("/Game/Maps/Menu/",1,true)or map:find("/Game/UI/Menu/",1,true)then
                self:cancel("已返回菜单，换机请求已取消");return false
            end
            if (map:find("/Game/Maps/Mission/",1,true)or map:find("/Game/Maps/FreeFlight/",1,true))and map~=pending.map then
                self:cancel("进入了其他任务，换机请求已取消");return false
            end
        end
        if os.clock()-pending.started>180 then
            local recovered=rollback(pending)
            fail("等待重开超时；未重复重开，请返回机库换机。",not recovered)
            return false
        end
        if not deps.allowed()or not valid(world)then waiting(pending,"waiting_world");return true end
        if pending.resume then
            local ok,err=pcall(function()
                assert(address(world)==pending.world and address(context.plane)==pending.plane,
                    "等待暂停归还时原世界或玩家已变化，未重开任务")
                assert(context.instance:GetSelectedPlaneID()==pending.target and context.instance:GetSelectedPlaneSWP()==0
                    and context.instance:GetSelectedPlaneSkinNo()==0,"等待暂停归还时目标选择已变化，未重开任务")
                local owner=controller(context,context.plane)
                assert(address(owner)==pending.resumeOwner,"等待暂停归还时控制器已变化，未重开任务")
                if pending.resume(owner)~=false then
                    pending.resume=nil;pending.resumeOwner=nil;pending.started=os.clock()
                    self.notice=tr("restarting.the.mission.waiting.for.492c291f")..pending.name..tr("to.spawn.dfae5745")
                    waiting(pending,"waiting_new_pawn");changed()
                end
            end)
            if not ok then local recovered=rollback(pending);fail(err,not recovered);return false end
            return true -- 本轮不重建旧玩家兵装，也不在同一调度重复执行重开。
        end
        local pawn=deps.game():GetPlayerPawn(context.instance,0)
        if valid(pawn)and not pawn:IsA(StaticFindObject("/Script/Nimbus.PlayerPlane"))then waiting(pending,"waiting_player_plane");return true end
        if not valid(pawn)or (address(world)==pending.world and address(pawn)==pending.plane)then
            if address(world)==pending.world and address(pawn)==pending.plane and os.clock()-pending.started>15 then
                local recovered=rollback(pending)
                fail("原生重开未生成新玩家机体，请返回机库换机。",not recovered);return false
            end
            waiting(pending,"waiting_new_pawn");return true
        end
        local matched=deps.matches(pawn,pending.code)
        if not matched then
            -- 旧机已被重新生成，说明该入口未采用目标选择；绝不把GI编号冒充换机成功。
            if pawn.bEnableWeaponInput and not pawn:IsWeaponSystemDisabled()then
                fail("任务仍生成原机体；当前任务不支持此重开入口，请返回机库换机。",true)
            end
            waiting(pending,"waiting_target_pawn");return true
        end
        if context.instance:GetSelectedPlaneID()~=pending.target then fail("重开期间原生机体选择被其他流程改变",true);return true end
        local held=pcall(controller,context,pawn)
        if not held then waiting(pending,"waiting_possession");return true end
        -- 在主循环完成新Pawn身份绑定后才允许兵装初始化。
        if not self.flight or self.flight.id~=pending.target then
            self.flight={id=pending.target,profile={a=0,b=-1,c=-1,ratio=1},initial=true,instance=pending.instance,map=pending.map}
        end
        deps.bind(pawn,pending.map)
        if not pawn.bEnableWeaponInput or pawn:IsWeaponSystemDisabled()then
            waiting(pending,"waiting_flight_input paused="..tostring(deps.game():IsGamePaused(pawn)))
        else waiting(pending,"waiting_weapon_setup")end
        return false
    end
    function self:configured(id,profile)
        if self.flight and self.flight.id==id then self.flight.profile=deps.copy(profile)end
        if self.pending and self.pending.target==id then
            local name=self.pending.name
            local count=1+(profile.b>=0 and 1 or 0)+(profile.c>=0 and 1 or 0)
            self.pending=nil;self.status="applied";self.error=nil
            self.notice=tr("aircraft.change.applied.41f605b6")..name..tr("punctuation.8ce1b17b")..count..tr("sp.weapons.loadout.debugging.is.321c05be")
            deps.note("DEBUG_AIRCRAFT_APPLIED plane="..id.." request="..self.ack);changed()
        end
    end
    function self:failed(message)if self.pending then fail(message)end end
    return self
end
return Module
