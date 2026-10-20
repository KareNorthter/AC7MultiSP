// 机库与补给只接管持久模型；不读写任务弹药、冷却、发射顺序。
#pragma once

bool mountPreviewRestoring=false; // 仅覆盖同步归还调用栈；待重试状态仍继续记录原生显隐请求。
bool mountPreviewApplying=false, mountPreviewApplyReentered=false; // 同一原生同步批次只维护一次完整预览。
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
// 按各组模型容量策略聚合，限于已有持久位置；不创建可发射弹药。
uint64_t mountPreviewReadyMask(const FixedMountState& state,const std::array<int,3>& kinds,const std::array<int,3>& ammo,int count) {
    require(count>=1 && count<=3,"模型预览：槽数无效");
    if(mountExterior(state.aircraft))return mountExteriorPreviewMask(state,kinds,ammo,count);
    const auto& specification=mountSpecification(state.aircraft);
    uint64_t result=0;
    for(int slot=0;slot<count;++slot) {
        const int kind=kinds[slot];
        require(kind>=0 && kind<3 && ammo[slot]>=0 && ammo[slot]<=100000,"模型预览：配弹参数无效");
        if((state.aircraft==MountAircraft::F15e && kind==2) || mountFixedEmitter(state.aircraft,kind))continue; // TLS 的空网格不是导弹，炮体由挂架显示。
        const int limit=mountShowReserve(state.aircraft,state.kindsMask,kind)?specification.logicalCounts[kind]:specification.loadedCounts[kind];
        const int visible=mountModelCapacityIndependent(state.aircraft,state.kindsMask,kind)?limit:std::min(limit,ammo[slot]);
        for(int ordinal=0;ordinal<visible;++ordinal)result|=uint64_t{1}<<mountPhysical(state,kind,ordinal);
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
    const bool priorRestoring=mountPreviewRestoring;
    mountPreviewRestoring=true;
    struct RestoreScope { bool prior; ~RestoreScope() { mountPreviewRestoring=prior; } } restoreScope{priorRestoring};
    std::string error;
    try { mountExteriorRelease(mountPreview.layout); }
    catch(const std::exception& item) { error=item.what(); }
    try { mountLightRelease(mountPreview.layout); }
    catch(const std::exception& item) { if(!error.empty())error+="；";error+=item.what(); }
    try { mountLocalRelease(mountPreview.layout); }
    catch(const std::exception& item) { if(!error.empty())error+="；";error+=item.what(); }
    if(mountPreviewOwnsTask())try { mountLocalBeforeRebuild(fixedMount); }
    catch(const std::exception& item) { if(!error.empty())error+="；";error+=item.what(); }
    if(!mountAnimationRelease(mountPreview.layout.plane.address,2))error="多 SP 动画控制仍待归还";
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
            if(mountLight(fixedMount.aircraft))mountLightMaintain(fixedMount);
            if(mountExterior(fixedMount.aircraft))mountExteriorMaintain(fixedMount);
            if(mountLocal(fixedMount.aircraft))mountLocalMaintain(fixedMount);
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
// 换皮肤、离开机库和开场载入会正常换代；先归还旧身份，由下一次真实入口重新绑定。
// 归还失败仍保留原来的失败状态，不能用生命周期等待掩盖网格接管或资源校验错误。
void mountPreviewWaitGeneration(const char* reason) {
    if(!mountPreview.active)return;
    const int mode=mountPreview.mode;
    if(!mountPreviewRelease(0,false))return;
    mountPreview.pending=true;
    mountPreview.lastApply=GetTickCount64();
    nativeLog(std::string("MOUNT_PREVIEW_WAIT_GENERATION mode=")+std::to_string(mode)+" "+reason);
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
    if(mountLight(state.aircraft)) { mountLightMaintain(state);return; }
    if(mountExterior(state.aircraft)) { mountExteriorMaintain(state);return; }
    if(mountAnimated(state.aircraft)) { mountAnimationEnsure(state,2);return; }
    if(mountLocal(state.aircraft)) { mountLocalMaintain(state);return; }
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
    mountLightInherit(next,prior);
    mountExteriorInherit(next,prior);
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
    if(mountPreviewRestoring)return false;
    if(manager==mountPreview.layout.meshManager.address)return true;
    if(mountPreview.mode!=1 || mountPreview.failed || !mountPreview.layout.bound)return false;
    // 换涂装可能连模型管理器一起重建；从已持有的激活器验证新入口，不扫描其他飞机。
    if(!mountAlive(mountPreview.activator)) {
        mountPreviewWaitGeneration("机库激活器换代");return false;
    }
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
    if(mountLight(state.aircraft))return mountLightLayoutChanged(state);
    if(mountExterior(state.aircraft)) {
        if(!mountAlive(state.exteriorRack) || read<Addr>(state.plane.address+state.rackOffsets[2])!=state.exteriorRack.address)return true;
        if(state.aircraft==MountAircraft::Pkfa)return false;
        const Addr array=state.meshManager.address+0x170;
        if(read<int>(array+8)!=state.poolCounts[2])return true;
        const Addr data=read<Addr>(array);
        for(const auto& visual:state.visuals)if(!mountAlive(visual.identity) ||
            (visual.index<0?state.exteriorRack.address:read<Addr>(data+visual.index*8))!=visual.identity.address)return true;
        return false;
    }
    std::array<Addr,3> arrays{},racks{};
    for(int pool=0;pool<3;++pool) {
        const Addr array=state.meshManager.address+0x150+pool*16;
        if(read<int>(array+8)!=state.poolCounts[pool])return true;
        arrays[pool]=read<Addr>(array);
        racks[pool]=read<Addr>(state.plane.address+state.rackOffsets[pool]);
    }
    for(const auto& visual:state.visuals) {
        if(!mountAlive(visual.identity))return true;
        if((visual.index<0?racks[visual.pool]:
            read<Addr>(arrays[visual.pool]+visual.index*8))!=visual.identity.address)return true;
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
    for(int pool=0;pool<3;++pool)if((!mountExterior(prior.aircraft) || (prior.aircraft==MountAircraft::F22a && pool==2)) &&
        (!mountLight(prior.aircraft) || mountLightOwnsPool(prior.aircraft,pool)) &&
        read<int>(meshManager+0x158+pool*16)==0 && !mountFixedEmitter(prior.aircraft,pool))return false;
    FixedMountState next;
    next.aircraft=prior.aircraft;next.kindsMask=prior.kindsMask;
    const auto& specification=mountSpecification(next.aircraft);
    next.logicalCounts=specification.logicalCounts;next.physicalCount=specification.physicalCount;
    next.rackMask=mountRackMask(next.aircraft,next.kindsMask);next.readyMask=prior.readyMask;
    mountBindVisuals(next,prior.plane.address,mountPreview.activator.address); // 全量校验成功前不写新组件。
    if(mountLight(prior.aircraft) && mountLightLayoutChanged(prior))mountLightRelease(prior);
    mountPreviewInheritNative(next,prior);
    mountLocalRelease(prior); // 新绑定已按实例继承租约；旧组件不再共享时逐项归还。
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
    if(mountExterior(prior.aircraft) && !mountSameIdentity(prior.exteriorRack,next.exteriorRack))mountExteriorRelease(prior);
    next.bound=true;prior=std::move(next);
    nativeLog("MOUNT_PREVIEW_HANGAR_COMPONENTS_REBOUND");
    return true;
}
void mountPreviewApply() {
    if(mountPreviewApplying) { mountPreviewApplyReentered=true;return; }
    if(!mountPreview.active)return;
    if(mountPreview.failed) { mountPreviewRetryRestore();return; }
    if(mountPreviewRestoring || mountPreview.pending || !mountPreview.layout.bound)return;
    mountPreviewApplying=true;mountPreviewApplyReentered=false;
    struct ApplyScope { ~ApplyScope() { mountPreviewApplying=false;mountPreviewApplyReentered=false; } } applyScope;
    const auto batchPlane=mountPreview.layout.plane;
    if(!mountAlive(mountPreview.layout.plane) || !mountAlive(mountPreview.activator) || !mountAlive(mountPreview.world)) {
        mountPreviewWaitGeneration("展示实例或世界换代");return;
    }
    if(mountPreview.mode==2 || mountPreview.mode==5) {
        // 补给页只在接入时证明身份；页面销毁后由同一任务玩家持有预览直到补给成功。
        if(session.plane!=mountPreview.layout.plane.address || session.activator!=mountPreview.activator.address) {
            mountPreviewWaitGeneration("补给任务玩家换代");return;
        }
    } else {
        if(!mountAlive(mountPreview.owner) || !mountAlive(mountPreview.source) ||
           read<Addr>(mountPreview.source.address+mountPreview.sourcePlaneOffset)!=mountPreview.layout.plane.address) {
            mountPreviewWaitGeneration("机库或玩家展示对象换代");return;
        }
    }
    if(read<Addr>(mountPreview.layout.plane.address+0x11a8)!=mountPreview.activator.address) {
        mountPreviewWaitGeneration("飞机激活器换代");return;
    }
    // 同一轮只核对一次完整池；机库确实重建后再核对新池。
    if(mountPreviewLayoutChanged()) {
        if(mountPreview.mode!=1) {
            mountPreviewWaitGeneration("持久池实例或管理器换代");return;
        }
        if(!mountPreviewRebindHangar()) {
            mountPreviewWaitGeneration("机库新模型池仍在载入");return;
        }
        if(mountPreviewLayoutChanged()) {
            mountPreviewWaitGeneration("机库模型池继续换代");return;
        }
    }
    require(mountAlive(mountPreview.layout.manager) && mountAlive(mountPreview.layout.meshManager) &&
        mountAlive(mountPreview.layout.root) && mountAlive(mountPreview.layout.weaponBase),"模型预览：模型管理器或祖先失效");
    require(read<Addr>(mountPreview.layout.plane.address+0x11a8)==mountPreview.activator.address &&
        read<Addr>(mountPreview.activator.address+0x260)==mountPreview.layout.manager.address &&
        read<Addr>(mountPreview.layout.manager.address+0x150)==mountPreview.layout.meshManager.address,"模型预览：模型管理器被替换");
    if(!mountPreview.nativeOnly) {
        auto& state=mountPreview.layout;
        mountPreviewAnimate();
        if(!mountPreview.active || mountPreview.failed || mountPreview.nativeOnly || !state.bound ||
           !mountSameIdentity(batchPlane,state.plane))return;
        mountApplyVisuals(state);mountPreview.lastApply=GetTickCount64();
        // SetStaticMesh/显隐可能同步触发总显隐回调；原生请求仍记账，批次末再核换代，不逐回调重复整池扫描。
        if(mountPreviewApplyReentered && mountPreview.active && !mountPreview.failed && !mountPreview.nativeOnly &&
           state.bound && mountSameIdentity(batchPlane,state.plane)) {
            const bool sourceChanged=(mountPreview.mode==2 || mountPreview.mode==5)?
                session.plane!=state.plane.address || session.activator!=mountPreview.activator.address:
                !mountAlive(mountPreview.owner) || !mountAlive(mountPreview.source) ||
                read<Addr>(mountPreview.source.address+mountPreview.sourcePlaneOffset)!=state.plane.address;
            if(!mountAlive(state.plane) || !mountAlive(mountPreview.activator) || !mountAlive(mountPreview.world) ||
               sourceChanged || read<Addr>(state.plane.address+0x11a8)!=mountPreview.activator.address ||
               mountPreviewLayoutChanged())mountPreviewWaitGeneration("预览同步更新期间模型换代");
        }
    }
}
bool mountPreviewFilter(void* object,bool requested,unsigned char propagation,bool hidden) {
    const Addr address=reinterpret_cast<Addr>(object);
    auto& state=mountPreview.layout;
    const auto original=hidden?mountOriginalHidden:mountOriginalVisibility;
    if(mountPreviewRestoring) { original(object,requested,propagation);return true; }
    if(address==state.root.address || address==state.weaponBase.address) {
        if(!mountAlive(state.root) || !mountAlive(state.weaponBase)) {
            mountPreviewWaitGeneration("展示祖先换代");original(object,requested,propagation);return true;
        }
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
        if(!mountAlive(visual.identity)) {
            mountPreviewWaitGeneration("展示组件换代");original(object,requested,propagation);return true;
        }
        (hidden?visual.nativeHidden:visual.nativeVisible)=requested;
        mountPreviewRememberTask(visual);
        if(visual.index<0 && propagation==2)for(auto& child:state.visuals)if(child.pool==visual.pool && child.index>=0) {
            (hidden?child.nativeHidden:child.nativeVisible)=requested;mountPreviewRememberTask(child);
        }
        if(mountPreview.nativeOnly)original(object,requested,propagation);
        else mountApplyOwnedVisibility(state,visual,object,hidden);
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
        if(same && mountPreview.planCached && !mountPreview.pending && mountPreview.configs==configs && mountPreview.ammo==ammo) {
            if(GetTickCount64()-mountPreview.lastApply>=100)mountPreviewApply();
            return; // 稳定候选每100ms巡检；原生显隐钩子仍立即响应，不逐帧遍历全部池。
        }
        require(mode==1 || mode==2 || mode==3 || mode==5,"模型预览：未知场景模式");
        verifyVersion();mountInstallHooks();
        require(read<Addr>(plane+0x11a8)==activator,"模型预览：飞机与激活器不匹配");
        const Addr world=mountOuterWorld(plane);
        if(mountPreview.world.address && !mountSameIdentity(mountPreview.world,mountIdentity(world))) {
            mountPreviewWaitGeneration("入口世界换代");return;
        }
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
                if(read<Addr>(hangar+mountPreview.sourcePlaneOffset)!=plane || mountOuterWorld(hangar)!=world) {
                    mountPreviewWaitGeneration("机库实际展示对象换代");return;
                }
            } else {
                // 开场只接入已被玩家控制器持有的实际飞机；不猜测或接管演出替身。
                const Addr controller=read<Addr>(plane+mountProperty(plane,"Controller","ObjectProperty",8));
                if(!controller) { mountPreview.pending=true;return; }
                require(mountClassIs(controller,"PlayerController","/Script/Engine"),"模型预览：开场飞机没有玩家控制器");
                mountPreview.source=mountIdentity(controller);
                mountPreview.sourcePlaneOffset=mountProperty(controller,"Pawn","ObjectProperty",8);
                if(read<Addr>(controller+mountPreview.sourcePlaneOffset)!=plane || mountOuterWorld(controller)!=world) {
                    mountPreviewWaitGeneration("开场玩家换代");return;
                }
            }
        }
        const auto aircraft=mountAircraft(plane);
        require(aircraft!=MountAircraft::Unsupported,"模型预览：机型未适配");
        // mode1覆盖机库、准备与其他任务外展示；补给和实际任务玩家保持任务内策略。
        if(mountNativeOnly(aircraft) || (aircraft==MountAircraft::Fa44 && mode==1)) {
            mountPreview.nativeOnly=true;mountPreviewRestoreVisuals();
            mountPreview.layout={};mountPreview.layout.plane=planeIdentity;mountPreview.layout.aircraft=aircraft;
            mountPreview.configs=configs;mountPreview.ammo=ammo;mountPreview.planCached=true;mountPreview.pending=false;return;
        }
        const auto& specification=mountSpecification(aircraft);
        // 异步换机可能先出现 Pawn、后填充模型池；空池属于等待，不锁死该身份。
        const Addr manager=read<Addr>(activator+0x260);
        Addr meshManager=0;
        if(manager) { mountIdentity(manager);meshManager=read<Addr>(manager+0x150); }
        bool pending=!meshManager;
        if(meshManager) {
            mountIdentity(meshManager);
            for(int pool=0;pool<3;++pool)if((!mountExterior(aircraft) || (aircraft==MountAircraft::F22a && pool==2)) &&
                (!mountLight(aircraft) || mountLightOwnsPool(aircraft,pool)) &&
                read<int>(meshManager+0x158+pool*16)==0 && !mountFixedEmitter(aircraft,pool))pending=true;
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
            const auto actual=mountName(package);
            const auto shortName=std::string(specification.weaponClasses[kind]).substr(0,std::strlen(specification.weaponClasses[kind])-2);
            require(actual==expected || ((mountExpanded(aircraft) || mountAnimated(aircraft) || mountExterior(aircraft) || mountLight(aircraft)) && actual=="/Game/Blueprint/Weapons/Player/Variation/"+shortName),"模型预览：兵装原版资源路径不匹配");
            require(!(mask&(1u<<kind)),"模型预览：兵装重复");
            kinds[count++]=kind;mask|=1u<<kind;
        }
        require(count>0,"模型预览：没有有效兵装");
        if(((mountExpanded(aircraft) || mountAnimated(aircraft) || mountExterior(aircraft) || mountLight(aircraft)) && count==1) ||
           (mountLight(aircraft) && mountLightNative(aircraft,mask)) ||
           (mountExterior(aircraft) && !(mask&4u))) {
            mountPreview.nativeOnly=true;mountPreviewRestoreVisuals();
            mountPreview.layout={};mountPreview.layout.plane=planeIdentity;mountPreview.layout.kindsMask=mask;
            mountPreview.configs=configs;mountPreview.ammo=ammo;mountPreview.planCached=true;return;
        }
        if(!mountPreview.layout.bound || mountPreview.layout.kindsMask!=mask) {
            if(mountLight(aircraft) && mountPreview.layout.bound)mountPreviewRestoreVisuals();
            if(mountLight(aircraft) && mountPreviewOwnsTask() && fixedMount.kindsMask!=mask)mountLightRelease(fixedMount);
            FixedMountState next;
            next.plane=planeIdentity;next.aircraft=aircraft;next.kindsMask=mask;
            next.logicalCounts=specification.logicalCounts;next.physicalCount=specification.physicalCount;next.rackMask=mountRackMask(aircraft,mask);
            mountBindVisuals(next,plane,activator); // 所有池/挂架/资源/Socket 完整验证后才允许写显示。
            if(mountPreviewOwnsTask())mountPreviewInheritNative(next,fixedMount);
            mountPreviewInheritNative(next,mountPreview.layout);
            mountRestoreUnusedMeshes(mountPreview.layout,next);
            mountLocalRelease(mountPreview.layout);
            next.bound=true;mountPreview.layout=std::move(next);
        }
        auto& state=mountPreview.layout;
        state.readyMask=mountPreviewReadyMask(state,kinds,ammo,count);
        mountPreview.configs=configs;mountPreview.ammo=ammo;mountPreview.planCached=true;
        mountPreviewApply();
    }catch(const std::exception& error) { mountPreviewFail(error.what()); }
}
