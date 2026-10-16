// 机库与补给只接管持久模型；不读写任务弹药、冷却、发射顺序。
#pragma once

bool mountSameIdentity(const MountIdentity& a,const MountIdentity& b) {
    return a.address==b.address && a.index==b.index && a.serial==b.serial;
}
// UE4.18 的反射字段链已经由现场只读采样核对；字段名、类型、尺寸全部匹配才使用。
size_t mountProperty(Addr object,const char* name,const char* kind,int size) {
    mountIdentity(object);
    Addr cls=read<Addr>(object+0x10);
    for(int depth=0;cls && depth<32;++depth,cls=read<Addr>(cls+0x30)) {
        mountIdentity(cls);
        Addr field=read<Addr>(cls+0x38);
        for(int count=0;field && count<1024;++count,field=read<Addr>(field+0x28)) {
            mountIdentity(field);
            if(mountName(field)!=name)continue;
            require(mountName(read<Addr>(field+0x10))==kind && read<int>(field+0x34)==size,"模型预览：反射字段类型改变");
            const int offset=read<int>(field+0x44);
            require(offset>=0x28 && offset<0x10000,"模型预览：反射字段越界");
            return static_cast<size_t>(offset);
        }
        require(!field,"模型预览：反射字段链过长");
    }
    throw std::runtime_error("模型预览：缺少必要反射字段");
}
bool mountClassIs(Addr object,const char* name,const char* package) {
    mountIdentity(object);
    Addr cls=read<Addr>(object+0x10);
    for(int depth=0;cls && depth<32;++depth,cls=read<Addr>(cls+0x30)) {
        mountIdentity(cls);
        if(mountName(cls)==name && mountName(read<Addr>(cls+0x20))==package)return true;
    }
    return false;
}
Addr mountOuterWorld(Addr object) {
    for(int depth=0;object && depth<24;++depth,object=read<Addr>(object+0x20)) {
        mountIdentity(object);
        if(mountClassIs(object,"World","/Script/Engine"))return object;
    }
    throw std::runtime_error("模型预览：实际模型没有有效世界");
}
bool mountOuterContains(Addr object,Addr expected) {
    for(int depth=0;object && depth<24;++depth,object=read<Addr>(object+0x20)) {
        mountIdentity(object);
        if(object==expected)return true;
    }
    return false;
}
// 按任务满载规则聚合，限于实际持久位置；没有创建额外弹体。
unsigned mountPreviewReadyMask(const FixedMountState& state,const std::array<int,3>& kinds,const std::array<int,3>& ammo,int count) {
    require(count>=1 && count<=3,"模型预览：槽数无效");
    const auto& specification=mountSpecification(state.aircraft);
    unsigned result=0;
    for(int slot=0;slot<count;++slot) {
        const int kind=kinds[slot];
        require(kind>=0 && kind<3 && ammo[slot]>=0 && ammo[slot]<=100000,"模型预览：配弹参数无效");
        if(state.aircraft==MountAircraft::F15e && kind==2)continue; // TLS 的空网格不是导弹，炮体由挂架显示。
        const int limit=mountShowReserve(state.aircraft,state.kindsMask,kind)?specification.logicalCounts[kind]:specification.loadedCounts[kind];
        for(int ordinal=0;ordinal<std::min(limit,ammo[slot]);++ordinal)result|=1u<<mountPhysical(state,kind,ordinal);
    }
    return result;
}
void mountPreviewRememberTask(const MountVisual& source) {
    // 两份上下文各记最新原生请求；退出补给预览后，任务仍能正确归还原生。
    if(!mountPreviewOwnsTask())return;
    for(auto& visual:fixedMount.visuals)if(mountSameIdentity(visual.identity,source.identity)) {
        visual.nativeVisible=source.nativeVisible;visual.nativeHidden=source.nativeHidden;return;
    }
}
void mountPreviewRestoreVisuals() {
    std::string error;
    for(const auto& item:mountPreview.layout.meshOverrides)try {
        mountApplyMesh(item,false);
    }catch(const std::exception& item) { if(!error.empty())error+="；";error+=item.what(); }
    for(const auto& visual:mountPreview.layout.visuals)try {
        if(!mountAlive(visual.identity))continue;
        mountOriginalHidden(reinterpret_cast<void*>(visual.identity.address),visual.nativeHidden,0);
        mountOriginalVisibility(reinterpret_cast<void*>(visual.identity.address),visual.nativeVisible,0);
    }catch(const std::exception& item) { if(!error.empty())error+="；";error+=item.what(); }
    if(mountPreview.layout.animatedRack.address)try {
        if(mountAlive(mountPreview.layout.animatedRack))mountSetRackAnimation(mountPreview.layout.animatedRack.address,
            mountPreview.layout.nativeRackPaused,mountPreview.layout.nativeRackTick);
    }catch(const std::exception& item) { if(!error.empty())error+="；";error+=item.what(); }
    require(error.empty(),error.c_str()); // 每项独立处理，归还失败保留整个上下文供下次重试。
}
bool mountPreviewRelease(Addr owner,bool resumeTask) {
    if(!mountPreview.active || (owner && owner!=mountPreview.owner.address))return true;
    try {
        mountPreview.restorePending=true;mountPreviewRestoreVisuals();mountPreview.restorePending=false;
        const bool resume=resumeTask && mountPreviewOwnsTask() && fixedMount.bound && mountAlive(fixedMount.plane) && mountAlive(fixedMount.meshManager);
        mountPreview={};
        if(resume) {
            if(fixedMount.animatedRack.address && mountAlive(fixedMount.animatedRack))mountSetRackAnimation(fixedMount.animatedRack.address,false,true);
            mountApply();
        } // 恢复本次仍有效任务的实际模型，不覆盖任何弹药缓存。
        return true;
    }catch(const std::exception& error) {
        mountPreview.nativeOnly=true;mountPreview.failed=true;
        mountPreview.warning=std::string("多 SP 模型预览已回退，原生显示仍待恢复：")+error.what();
        return false;
    }
}
void mountPreviewFail(const char* message) noexcept {
    if(mountPreview.failed)return;
    mountPreview.failed=true;mountPreview.nativeOnly=true;
    mountPreview.warning=std::string("多 SP 模型预览已回退为原生单 SP：")+message;
    try { mountPreview.restorePending=true;mountPreviewRestoreVisuals();mountPreview.restorePending=false; }
    catch(const std::exception& error) { mountPreview.warning+="；归还显示失败：";mountPreview.warning+=error.what(); }
    nativeLog(std::string("MOUNT_PREVIEW_FALLBACK ")+mountPreview.warning);
}
void mountPreviewRetryRestore() {
    if(mountPreview.failed && mountPreview.restorePending && GetTickCount64()-mountPreview.restoreAttempt>=200) {
        mountPreview.restoreAttempt=GetTickCount64();
        try { mountPreviewRestoreVisuals();mountPreview.restorePending=false; }catch(const std::exception&) { }
    }
}
void mountPreviewAnimate() {
    auto& state=mountPreview.layout;
    const Addr plane=state.plane.address;const unsigned mask=state.kindsMask;
    if(state.aircraft==MountAircraft::F35c) {
        const Addr rack=read<Addr>(plane+0x1b90);
        const bool paused=(read<unsigned>(rack+0x9b0)&2)!=0,tick=read<unsigned char>(rack+0x3f)!=0;
        const bool first=!state.animatedRack.address;
        if(first || paused || !tick) {
            state.animatedRack=mountIdentity(rack);state.nativeRackPaused=paused;state.nativeRackTick=tick;
            if(first && mountPreviewOwnsTask() && mountSameIdentity(fixedMount.animatedRack,state.animatedRack)) {
                state.nativeRackPaused=fixedMount.nativeRackPaused;state.nativeRackTick=fixedMount.nativeRackTick;
            } else if(mountPreviewOwnsTask() && mountSameIdentity(fixedMount.animatedRack,state.animatedRack)) {
                fixedMount.nativeRackPaused=paused;fixedMount.nativeRackTick=tick;
            }
        }
        // 只保持该原生骨架读取当前动画实例；不冻结骨架、不碰飞行 PartsAnimation。
        if(mask&4) { if(paused || !tick)mountSetRackAnimation(rack,false,true); }
        else if(paused!=state.nativeRackPaused || tick!=state.nativeRackTick)mountSetRackAnimation(rack,state.nativeRackPaused,state.nativeRackTick);
    }
}
// 组件复用时只能继承记录过的原生请求；新组件必须保留自己的初始状态。
void mountPreviewInheritNative(FixedMountState& next,const FixedMountState& prior) {
    for(auto& visual:next.visuals)for(const auto& old:prior.visuals)if(mountSameIdentity(visual.identity,old.identity)) {
        visual.nativeVisible=old.nativeVisible;visual.nativeHidden=old.nativeHidden;break;
    }
    if(mountSameIdentity(next.root,prior.root)) { next.rootVisible=prior.rootVisible;next.rootHidden=prior.rootHidden; }
    if(mountSameIdentity(next.weaponBase,prior.weaponBase)) { next.ancestorVisible=prior.ancestorVisible;next.ancestorHidden=prior.ancestorHidden; }
    for(const auto& visual:next.visuals)if(mountSameIdentity(visual.identity,prior.animatedRack)) {
        next.animatedRack=prior.animatedRack;next.nativeRackPaused=prior.nativeRackPaused;next.nativeRackTick=prior.nativeRackTick;break;
    }
}
bool mountPreviewMatchesManager(Addr manager) {
    if(manager==mountPreview.layout.meshManager.address)return true;
    if(mountPreview.mode!=1 || mountPreview.failed || !mountPreview.layout.bound)return false;
    // 换涂装可能连模型管理器一起重建；从已持有的激活器验证新入口，不扫描其他飞机。
    require(mountAlive(mountPreview.activator),"模型预览：机库激活器失效");
    const Addr current=read<Addr>(mountPreview.activator.address+0x260);
    if(!current)return false;
    mountIdentity(current);
    return read<Addr>(current+0x150)==manager;
}
bool mountPreviewLayoutChanged() {
    const auto& state=mountPreview.layout;
    if(read<Addr>(mountPreview.activator.address+0x260)!=state.manager.address || !mountAlive(state.manager))return true;
    if(read<Addr>(state.manager.address+0x150)!=state.meshManager.address || !mountAlive(state.meshManager))return true;
    if(read<Addr>(state.plane.address+0x160)!=state.root.address || !mountAlive(state.root) ||
       read<Addr>(state.plane.address+0x1ae0)!=state.weaponBase.address || !mountAlive(state.weaponBase))return true;
    const auto& specification=mountSpecification(state.aircraft);
    for(const auto& visual:state.visuals) {
        if(!mountAlive(visual.identity))return true;
        const Addr array=state.meshManager.address+0x150+visual.pool*16;
        if(read<int>(array+8)!=specification.poolCounts[visual.pool])return true;
        if((visual.index<0?read<Addr>(state.plane.address+specification.rackOffsets[visual.pool]):
            read<Addr>(read<Addr>(array)+visual.index*8))!=visual.identity.address)return true;
    }
    return false;
}
bool mountPreviewRebindHangar() {
    auto& prior=mountPreview.layout;
    const Addr manager=read<Addr>(mountPreview.activator.address+0x260);
    if(!manager)return false;
    mountIdentity(manager);
    const Addr meshManager=read<Addr>(manager+0x150);
    if(!meshManager)return false;
    mountIdentity(meshManager);
    // 原生可能先清空旧池、再填新池；不把明确的加载空窗永久锁成失败。
    for(int pool=0;pool<3;++pool)if(read<int>(meshManager+0x158+pool*16)==0)return false;
    FixedMountState next;
    next.aircraft=prior.aircraft;next.kindsMask=prior.kindsMask;
    const auto& specification=mountSpecification(next.aircraft);
    next.logicalCounts=specification.logicalCounts;next.physicalCount=specification.physicalCount;
    next.rackMask=mountRackMask(next.aircraft,next.kindsMask);next.readyMask=prior.readyMask;
    mountBindVisuals(next,prior.plane.address,mountPreview.activator.address); // 全量校验成功前不写新组件。
    mountPreviewInheritNative(next,prior);
    mountRestoreUnusedMeshes(prior,next);
    // 只归还已脱离本轮模型池的旧组件，复用组件不经过单挂状态。
    for(const auto& old:prior.visuals) {
        const bool reused=std::any_of(next.visuals.begin(),next.visuals.end(),[&](const MountVisual& item){return mountSameIdentity(item.identity,old.identity);});
        if(!reused && mountAlive(old.identity)) {
            mountOriginalHidden(reinterpret_cast<void*>(old.identity.address),old.nativeHidden,0);
            mountOriginalVisibility(reinterpret_cast<void*>(old.identity.address),old.nativeVisible,0);
        }
    }
    if(prior.animatedRack.address && !mountSameIdentity(prior.animatedRack,next.animatedRack) && mountAlive(prior.animatedRack))
        mountSetRackAnimation(prior.animatedRack.address,prior.nativeRackPaused,prior.nativeRackTick);
    next.bound=true;prior=std::move(next);
    nativeLog("MOUNT_PREVIEW_HANGAR_COMPONENTS_REBOUND");
    return true;
}
void mountPreviewApply() {
    if(!mountPreview.active)return;
    if(mountPreview.failed) { mountPreviewRetryRestore();return; }
    if(mountPreview.pending || !mountPreview.layout.bound)return;
    require(mountAlive(mountPreview.layout.plane) && mountAlive(mountPreview.activator) && mountAlive(mountPreview.world),"模型预览：展示实例或世界失效");
    if(mountPreview.mode==2 || mountPreview.mode==5) {
        // 补给页只在接入时证明身份；页面销毁后由同一任务玩家持有预览直到补给成功。
        require(session.plane==mountPreview.layout.plane.address && session.activator==mountPreview.activator.address,
            "模型预览：补给任务玩家改变");
    } else {
        require(mountAlive(mountPreview.owner) && mountAlive(mountPreview.source) &&
            read<Addr>(mountPreview.source.address+mountPreview.sourcePlaneOffset)==mountPreview.layout.plane.address,
            "模型预览：机库或玩家展示对象改变");
    }
    require(read<Addr>(mountPreview.layout.plane.address+0x11a8)==mountPreview.activator.address,"模型预览：飞机激活器已改变");
    if(mountPreview.mode==1 && mountPreviewLayoutChanged() && !mountPreviewRebindHangar())return;
    require(mountAlive(mountPreview.layout.manager) && mountAlive(mountPreview.layout.meshManager) &&
        mountAlive(mountPreview.layout.root) && mountAlive(mountPreview.layout.weaponBase),"模型预览：模型管理器或祖先失效");
    require(read<Addr>(mountPreview.layout.plane.address+0x11a8)==mountPreview.activator.address &&
        read<Addr>(mountPreview.activator.address+0x260)==mountPreview.layout.manager.address &&
        read<Addr>(mountPreview.layout.manager.address+0x150)==mountPreview.layout.meshManager.address,"模型预览：模型管理器被替换");
    if(!mountPreview.nativeOnly) {
        auto& state=mountPreview.layout;const auto& specification=mountSpecification(state.aircraft);
        for(int pool=0;pool<3;++pool) {
            const Addr rack=read<Addr>(state.plane.address+specification.rackOffsets[pool]);
            const Addr array=state.meshManager.address+0x150+pool*16;
            require(read<int>(array+8)==specification.poolCounts[pool],"模型预览：持久池数量改变");
            for(const auto& visual:state.visuals)if(visual.pool==pool) {
                require(mountAlive(visual.identity),"模型预览：持久组件身份失效");
                require(visual.index<0?visual.identity.address==rack:
                    read<Addr>(read<Addr>(array)+visual.index*8)==visual.identity.address,"模型预览：持久池实例被替换");
            }
        }
        mountPreviewAnimate();mountApplyVisuals(state);mountPreview.lastApply=GetTickCount64();
    }
}
bool mountPreviewFilter(void* object,bool requested,unsigned char propagation,bool hidden) {
    const Addr address=reinterpret_cast<Addr>(object);
    auto& state=mountPreview.layout;
    const auto original=hidden?mountOriginalHidden:mountOriginalVisibility;
    if(address==state.root.address || address==state.weaponBase.address) {
        require(mountAlive(state.root) && mountAlive(state.weaponBase),"模型预览：祖先身份失效");
        (address==state.root.address?(hidden?state.rootHidden:state.rootVisible):(hidden?state.ancestorHidden:state.ancestorVisible))=requested;
        if(mountPreviewOwnsTask()) {
            fixedMount.rootVisible=state.rootVisible;fixedMount.rootHidden=state.rootHidden;
            fixedMount.ancestorVisible=state.ancestorVisible;fixedMount.ancestorHidden=state.ancestorHidden;
        }
        original(object,requested,propagation);
        if(!mountPreview.nativeOnly)mountPreviewApply(); // 无传播请求也必须服从整组显隐；不反隐藏过场。
        return true;
    }
    for(auto& visual:state.visuals)if(visual.identity.address==address) {
        require(mountAlive(visual.identity),"模型预览：组件身份失效");
        (hidden?visual.nativeHidden:visual.nativeVisible)=requested;
        mountPreviewRememberTask(visual);
        if(visual.index<0 && propagation==2)for(auto& child:state.visuals)if(child.pool==visual.pool && child.index>=0) {
            (hidden?child.nativeHidden:child.nativeVisible)=requested;mountPreviewRememberTask(child);
        }
        original(object,mountPreview.nativeOnly?requested:(hidden?false:mountDesired(state,visual)),mountPreview.nativeOnly?propagation:0);
        return true;
    }
    // 绑定失败可能尚无完整池，但已知旧任务模型仍要绕过任务显隐接管。
    if(mountPreview.nativeOnly && mountPreviewOwnsTask())for(auto& visual:fixedMount.visuals)if(visual.identity.address==address) {
        require(mountAlive(visual.identity),"模型预览：原生回退组件失效");
        (hidden?visual.nativeHidden:visual.nativeVisible)=requested;
        original(object,requested,propagation);return true;
    }
    return false;
}
// 验证完成才绑定；失败锁定本次场景/机体身份，候选变化不触发重新接入。
void mountPreviewUpdate(Addr owner,Addr plane,Addr activator,int mode,const std::array<int,3>& configs,const std::array<int,3>& ammo) {
    if(!fixedMountEnabled || !runtimeEnabled) { mountPreviewRelease(0,false);return; }
    const auto ownerIdentity=mountIdentity(owner),planeIdentity=mountIdentity(plane),activatorIdentity=mountIdentity(activator);
    const bool same=mountPreview.active && mountSameIdentity(mountPreview.owner,ownerIdentity) &&
        mountSameIdentity(mountPreview.layout.plane,planeIdentity) && mountSameIdentity(mountPreview.activator,activatorIdentity) && mountPreview.mode==mode;
    if(!same) {
        require(mountPreviewRelease(),"模型预览：旧实例显示尚未归还");
        mountPreview.active=true;mountPreview.owner=ownerIdentity;mountPreview.layout.plane=planeIdentity;
        mountPreview.activator=activatorIdentity;mountPreview.mode=mode;
        if(fixedMount.bound && mountSameIdentity(fixedMount.plane,planeIdentity)) {
            mountPreview.layout=fixedMount; // 共享组件的原生快照只取任务已记录请求，不能采集多挂强制后的现场状态。
            mountPreview.layout.bound=false;
        }
    }
    if(mountPreview.failed) {
        mountPreviewRetryRestore();
        return; // 仅重试归还，绝不重新绑定或接管本次失败身份。
    }
    try {
        if(same && mountPreview.planCached && mountPreview.configs==configs && mountPreview.ammo==ammo) {
            if(GetTickCount64()-mountPreview.lastApply>=100)mountPreviewApply();
            return; // 稳定候选每100ms巡检；原生显隐钩子仍立即响应，不逐帧遍历全部池。
        }
        require(mode==1 || mode==2 || mode==3 || mode==5,"模型预览：未知场景模式");
        verifyVersion();mountInstallHooks();
        require(read<Addr>(plane+0x11a8)==activator,"模型预览：飞机与激活器不匹配");
        const Addr world=mountOuterWorld(plane);
        if(mountPreview.world.address)require(mountSameIdentity(mountPreview.world,mountIdentity(world)),"模型预览：世界改变");
        else mountPreview.world=mountIdentity(world);
        const auto base=reinterpret_cast<Addr>(GetModuleHandleW(nullptr));
        if(mode==5) {
            // 补给确认后页面可能销毁；只接受Lua保存的确认方案，且拥有者必须为当前任务玩家本身。
            require(owner==plane && session.plane==plane && session.activator==activator,
                "模型预览：补给过渡不是当前任务玩家");
        } else if(mode==2) {
            const Addr table=read<Addr>(owner);
            require(read<Addr>(table+0x408)==base+0x6fa1f0 && read<Addr>(table+0x420)==base+0x6fc6d0,"模型预览：补给页面类型不匹配");
            require(read<Addr>(owner+0x5c0)==plane && read<Addr>(owner+0x5c8)==activator &&
                session.plane==plane && session.activator==activator,"模型预览：补给页面不是当前任务玩家");
        } else {
            const Addr instance=read<Addr>(plane+mountProperty(plane,"GameInstance","ObjectProperty",8));
            require(mountClassIs(instance,"NimbusGameInstance","/Script/Nimbus") && mountOuterContains(owner,instance),"模型预览：机库页面不属于当前游戏实例");
            if(mode==1) {
                const Addr hangar=read<Addr>(instance+mountProperty(instance,"HangarManager","ObjectProperty",8));
                mountPreview.source=mountIdentity(hangar);
                mountPreview.sourcePlaneOffset=mountProperty(hangar,"LoadedPlayerPlane","ObjectProperty",8);
                require(read<Addr>(hangar+mountPreview.sourcePlaneOffset)==plane && mountOuterWorld(hangar)==world,"模型预览：机库实际展示对象改变");
            } else {
                // 开场只接入已被玩家控制器持有的实际飞机；不猜测或接管演出替身。
                const Addr controller=read<Addr>(plane+mountProperty(plane,"Controller","ObjectProperty",8));
                if(!controller) { mountPreview.pending=true;return; }
                require(mountClassIs(controller,"PlayerController","/Script/Engine"),"模型预览：开场飞机没有玩家控制器");
                mountPreview.source=mountIdentity(controller);
                mountPreview.sourcePlaneOffset=mountProperty(controller,"Pawn","ObjectProperty",8);
                require(read<Addr>(controller+mountPreview.sourcePlaneOffset)==plane && mountOuterWorld(controller)==world,
                    "模型预览：开场玩家归属不匹配");
            }
        }
        const auto aircraft=mountAircraft(plane);
        require(aircraft!=MountAircraft::Unsupported,"模型预览：机型未适配");
        const auto& specification=mountSpecification(aircraft);
        // 异步换机可能先出现 Pawn、后填充模型池；空池属于等待，不锁死该身份。
        const Addr manager=read<Addr>(activator+0x260);
        Addr meshManager=0;
        if(manager) { mountIdentity(manager);meshManager=read<Addr>(manager+0x150); }
        bool pending=!meshManager;
        if(meshManager) {
            mountIdentity(meshManager);
            for(int pool=0;pool<3;++pool)if(read<int>(meshManager+0x158+pool*16)==0)pending=true;
        }
        if(pending) {
            mountPreview.pending=true;mountPreview.nativeOnly=true;mountPreviewRestoreVisuals();return;
        }
        mountPreview.pending=false;mountPreview.nativeOnly=false;
        std::array<int,3> kinds{};
        int count=0;unsigned mask=0;bool empty=false;
        for(int slot=0;slot<3;++slot) {
            require(configs[slot]>=-1 && configs[slot]<3,"模型预览：原生配置越界");
            if(configs[slot]<0) { empty=true;continue; }
            require(!empty,"模型预览：空槽之后仍有兵装");
            const auto offset=mountProperty(activator,(std::string("SpecialWeaponConfig")+std::to_string(configs[slot]+1)).c_str(),"StructProperty",48);
            const Addr weaponClass=read<Addr>(activator+offset);mountIdentity(weaponClass);
            int kind=-1;
            for(int candidate=0;candidate<3;++candidate)if(mountName(weaponClass)==specification.weaponClasses[candidate])kind=candidate;
            require(kind>=0,"模型预览：兵装类未适配");
            const Addr package=read<Addr>(weaponClass+0x20);mountIdentity(package);
            const std::string expected=std::string("/Game/Blueprint/Weapons/Player/")+(aircraft==MountAircraft::Su30sm?"Variation/":"Base/")+std::string(specification.weaponClasses[kind]).substr(0,std::strlen(specification.weaponClasses[kind])-2);
            require(mountName(package)==expected,"模型预览：兵装原版资源路径不匹配");
            require(!(mask&(1u<<kind)),"模型预览：兵装重复");
            kinds[count++]=kind;mask|=1u<<kind;
        }
        require(count>0,"模型预览：没有有效兵装");
        if(!mountPreview.layout.bound || mountPreview.layout.kindsMask!=mask) {
            FixedMountState next;
            next.plane=planeIdentity;next.aircraft=aircraft;next.kindsMask=mask;
            next.logicalCounts=specification.logicalCounts;next.physicalCount=specification.physicalCount;next.rackMask=mountRackMask(aircraft,mask);
            mountBindVisuals(next,plane,activator); // 所有池/挂架/资源/Socket 完整验证后才允许写显示。
            if(mountPreviewOwnsTask())mountPreviewInheritNative(next,fixedMount);
            mountPreviewInheritNative(next,mountPreview.layout);
            mountRestoreUnusedMeshes(mountPreview.layout,next);
            next.bound=true;mountPreview.layout=std::move(next);
        }
        auto& state=mountPreview.layout;
        state.readyMask=mountPreviewReadyMask(state,kinds,ammo,count);
        mountPreview.configs=configs;mountPreview.ammo=ammo;mountPreview.planCached=true;
        mountPreviewApply();
    }catch(const std::exception& error) { mountPreviewFail(error.what()); }
}
