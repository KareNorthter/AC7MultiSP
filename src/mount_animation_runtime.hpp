// 三机产品接线；所有控制、见证和场景归还都限定在已经绑定的游戏线程。
#pragma once

MountAnimationHost mountAnimationHost;
MountAnimationExactPose mountAnimationExact;
::dualsp::common_bay::Snapshot mountAnimationPendingSwitch;
std::uint64_t mountAnimationGeneration{};
bool mountAnimationHooksReady{},mountAnimationCleaning{};
std::string mountAnimationHookFailure;
unsigned mountAnimationNativeDepth{};
bool mountAnimationClosedVisualsApplied{};
ULONGLONG mountAnimationLastMaintenance{};
struct MountAnimationProfile {
    struct Sample { std::uint64_t calls{},ticks{},peak{}; };
    std::array<Sample,3> samples{};
    std::uint64_t generation{};ULONGLONG last{};std::string aircraft;
    static std::uint64_t now() { LARGE_INTEGER value{};QueryPerformanceCounter(&value);return value.QuadPart; }
    void reset(std::uint64_t value,const char* code) { samples={};generation=value;aircraft=code;last=GetTickCount64(); }
    void record(unsigned kind,std::uint64_t ticks) noexcept {
        if(!generation)return;
        auto& s=samples[kind];++s.calls;s.ticks+=ticks;s.peak=std::max(s.peak,ticks);
        if(GetTickCount64()-last<2000)return;
        try {
            LARGE_INTEGER frequency{};QueryPerformanceFrequency(&frequency);
            Json values=Json::array();
            for(const auto& row:samples)values.push_back({{"calls",row.calls},
                {"mean_us",row.calls?double(row.ticks)*1000000.0/double(frequency.QuadPart)/row.calls:0.0},
                {"peak_us",double(row.peak)*1000000.0/double(frequency.QuadPart)}});
            nativeLog(Json{{"event","ANIMATED_WORK_COST"},{"aircraft",aircraft},{"generation",generation},
                {"order",{"parts_after","publication","ensure"}},{"samples",values},
                {"scope","本模块回调自耗时；发布原函数已排除，不是总帧时间"}}.dump());
        }catch(...) { }
        samples={};last=GetTickCount64();
    }
} mountAnimationProfile;
struct MountAnimationWorkTimer {
    unsigned kind{};std::uint64_t start{},total{};bool running{true};
    explicit MountAnimationWorkTimer(unsigned value):kind(value),start(MountAnimationProfile::now()){}
    void pause() { if(running) { total+=MountAnimationProfile::now()-start;running=false; } }
    void resume() { start=MountAnimationProfile::now();running=true; }
    ~MountAnimationWorkTimer() { pause();mountAnimationProfile.record(kind,total); }
};
using MountAnimationDisable=void(*)(void*,void*);
using MountAnimationTick=void(*)(void*,bool);
MountAnimationDisable mountAnimationOriginalDisable{};
MountAnimationTick mountAnimationOriginalTick{};
MountAnimationPublicationTracker::Original mountAnimationOriginalPublish{};
bool mountAnimationTickCodeCurrent() {
    return mountAnimationHooksReady && mountAnimationHost.access.originalCodeMatches(0x168aaa0,"48895c2408574883ec20f6413c02");
}

::dualsp::common_bay::Identity mountAnimationIdentity(MountIdentity id) { return {id.address,id.index,id.serial}; }
::dualsp::common_bay::Aircraft mountAnimationAircraft(MountAircraft aircraft) {
    using A=::dualsp::common_bay::Aircraft;
    if(aircraft==MountAircraft::F22a)return A::F22a;
    if(aircraft==MountAircraft::Su47)return A::Su47;
    require(aircraft==MountAircraft::Su57tg,"动画挂载：未知机型");return A::Su57TopGun;
}
FixedMountState* mountAnimationLayout() {
    const auto& binding=mountAnimationHost.binding;
    if(binding.scene==2 && mountPreview.active && !mountPreview.failed && mountPreview.layout.bound &&
       mountPreview.layout.plane.address==binding.plane.address)return &mountPreview.layout;
    if(binding.scene==1 && fixedMount.bound && !mountPreviewOwnsTask() &&
       fixedMount.plane.address==binding.plane.address)return &fixedMount;
    return nullptr;
}
void mountAnimationFail(const char* message) noexcept {
    if(mountAnimationCleaning)return;
    mountAnimationCleaning=true;
    const unsigned scene=mountAnimationHost.binding.scene;
    if(scene==2)mountPreviewFail(message);else fixedMountFail(message);
    mountAnimationCleaning=false;
}
bool mountAnimationRelease(Addr plane,unsigned scene) {
    if(!mountAnimationHost.access.ownerRegistered && !mountAnimationHost.controller.restoring()) {
        mountAnimationPendingSwitch={};return true;
    }
    if((plane && plane!=mountAnimationHost.binding.plane.address) ||
       (scene && scene!=mountAnimationHost.binding.scene))return true;
    mountAnimationExact.clear();
    mountAnimationPendingSwitch={};
    mountAnimationClosedVisualsApplied=false;
    mountAnimationLastMaintenance=0;
    return mountAnimationHost.release();
}
void mountAnimationBeforePartsHook() {
    mountAnimationHost.access.beforeOwnHook(0x1ce6d70,"4d85c00f84fe170000488bc455564881ecb8000000");
}
void mountAnimationAfterPartsHook() {
    mountAnimationHost.access.afterOwnHook(0x1ce6d70,reinterpret_cast<Addr>(&mountPartsUpdate),
                                         reinterpret_cast<Addr>(mountOriginalPartsUpdate));
}
void mountAnimationRemember(void* component,unsigned fields,bool tickWanted=false) noexcept {
    if(GetCurrentThreadId()!=mountGameThread.load() || mountAnimationHost.access.reflection.ownWrite ||
       !mountAnimationHost.access.ownerRegistered)return;
    try {
        for(auto& rack:mountAnimationHost.racks)if(rack.prepared && rack.identity.address==reinterpret_cast<Addr>(component)) {
            MountAnimationHost::Request state;
            require(mountAnimationHost.readRack(rack.identity,state),"动画挂载：原生请求读取失败");
            if(fields&2u)state.tick=tickWanted;
            if(mountAnimationNativeDepth)mountAnimationHost.controller.rememberNativeRequest(mountAnimationHost,rack.identity,state,fields);
            else mountAnimationHost.controller.nativeRequest(mountAnimationHost,rack.identity,state,fields);
            if(!mountAnimationNativeDepth && mountAnimationHost.controller.active())
                require(mountAnimationHost.maintain(),"动画挂载：原生请求后维持失败");
            break;
        }
    }catch(const std::exception& error) { mountAnimationFail(error.what()); }
}
void mountAnimationTick(void* component,bool enabled) {
    mountAnimationOriginalTick(component,enabled);
    mountAnimationRemember(component,2u,enabled);
}
void mountAnimationDisable(void* owner,void* component) {
    if(GetCurrentThreadId()!=mountGameThread.load()) { mountAnimationOriginalDisable(owner,component);return; }
    ++mountAnimationNativeDepth;
    mountAnimationOriginalDisable(owner,component); // 原生物理/可见性/注册行为完整执行一次。
    --mountAnimationNativeDepth;
    mountAnimationRemember(component,4u); // Tick由其独立入口记账，随后直接写的NoSkeletonUpdate单独记录。
}
void mountAnimationPublish(void* component) {
    auto& host=mountAnimationHost;
    if(GetCurrentThreadId()!=mountGameThread.load() || mountAnimationNativeDepth ||
       !host.controller.active() || !mountAnimationExact.prepared) {
        mountAnimationOriginalPublish(component);return;
    }
    int slot=-1;
    for(unsigned i=0;i<4;++i)if((mountAnimationExact.required&(1u<<i)) &&
        mountAnimationExact.components[i].object.address==reinterpret_cast<Addr>(component)) { slot=static_cast<int>(i);break; }
    if(slot<0) { mountAnimationOriginalPublish(component);return; }
    MountAnimationWorkTimer work(1);
    auto& stamp=mountAnimationExact.published[slot];stamp={};
    MountAnimationPublicationTracker::Snapshot before;
    ::dualsp::common_bay::PartsSample prior;
    bool admitted=false;
    const auto id=mountAnimationIdentity(mountAnimationExact.components[slot].object);
    try {
        admitted=host.bindingAlive(host.binding) && mountAnimationExact.resourcesCurrent(mountAnimationExact.components[slot]) &&
            MountAnimationPublicationTracker::codeQualified(host.access,id,mountAnimationOriginalPublish);
        if(admitted) {
            before=MountAnimationPublicationTracker::snapshot(host.access,id,&mountAnimationExact.components[slot].masterPose);
            admitted=before.pending==1 && before.animationFrame==static_cast<unsigned>(before.frame) &&
                host.readParts(host.binding,prior) && MountAnimationPublicationTracker::nativeOpen(prior);
        }
    }catch(...) { admitted=false; }
    work.pause();
    mountAnimationOriginalPublish(component); // 所有分支恰调用一次完整原生发布，不主动补帧。
    work.resume();
    try {
        if(admitted && host.bindingAlive(host.binding)) {
            const auto after=MountAnimationPublicationTracker::snapshot(host.access,id,&mountAnimationExact.components[slot].masterPose);
            ::dualsp::common_bay::PartsSample native;
            if(after.frame==before.frame && after.animationFrame==before.animationFrame && !after.pending &&
               after.doubleBuffered==before.doubleBuffered && after.readIndex==(before.doubleBuffered?before.writeIndex:0) &&
               after.writeIndex==(before.doubleBuffered?1-before.writeIndex:0) && after.componentToWorld==before.componentToWorld &&
               host.readParts(host.binding,native) && MountAnimationPublicationTracker::sameParts(prior,native))
                stamp={after.frame,after.readIndex,true};
        }
        // 保存的是发布事实；显示/发射仍重新核对当帧、缓冲、全部有效骨骼、Socket与相对变换。
        bool complete=true;
        const auto frame=read<std::uint64_t>(host.access.moduleBase()+0x3ac5520);
        for(unsigned i=0;i<4;++i)if((mountAnimationExact.required&(1u<<i)) &&
            (!mountAnimationExact.published[i].valid || mountAnimationExact.published[i].frame!=frame))complete=false;
        // 等本帧所需组件全部发布后统一应用一次，避免每个组件发布时整组先隐藏、再重复显示。
        if(complete)if(auto* state=mountAnimationLayout())mountApplyVisuals(*state);
    }catch(const std::exception& error) { mountAnimationFail(error.what()); }
}
void mountAnimationInstallHooks() {
    if(mountAnimationHooksReady)return;
    require(mountAnimationHookFailure.empty(),mountAnimationHookFailure.c_str());
    struct Entry { unsigned rva;const char* code;void* detour;void** original; };
    const Entry entries[]={
        {0x7f0210,"4885d20f84a1000000534883ec20488b02",reinterpret_cast<void*>(&mountAnimationDisable),reinterpret_cast<void**>(&mountAnimationOriginalDisable)},
        {0x168aaa0,"48895c2408574883ec20f6413c02",reinterpret_cast<void*>(&mountAnimationTick),reinterpret_cast<void**>(&mountAnimationOriginalTick)},
        {MountAnimationPublicationTracker::publishRva,MountAnimationPublicationTracker::publishCode,
            reinterpret_cast<void*>(&mountAnimationPublish),reinterpret_cast<void**>(&mountAnimationOriginalPublish)}};
    std::vector<void*> created;
    try {
        for(const auto& entry:entries)mountAnimationHost.access.beforeOwnHook(entry.rva,entry.code);
        for(const auto& entry:entries) {
            auto* target=reinterpret_cast<void*>(mountAnimationHost.access.moduleBase()+entry.rva);
            require(MH_CreateHook(target,entry.detour,entry.original)==MH_OK,"动画挂载：创建观察钩子失败");
            created.push_back(target);
        }
        for(auto* target:created)require(MH_QueueEnableHook(target)==MH_OK,"动画挂载：安排观察钩子失败");
        require(MH_ApplyQueued()==MH_OK,"动画挂载：启用观察钩子失败");
        for(const auto& entry:entries)mountAnimationHost.access.afterOwnHook(entry.rva,reinterpret_cast<Addr>(entry.detour),reinterpret_cast<Addr>(*entry.original));
        mountAnimationHooksReady=true;
    }catch(const std::exception& error) {
        mountAnimationHookFailure=error.what();
        for(auto it=created.rbegin();it!=created.rend();++it) {
            const auto result=MH_RemoveHook(*it);
            if(result!=MH_OK)mountAnimationHookFailure+="；撤销动画钩子失败："+std::to_string(result);
        }
        throw std::runtime_error(mountAnimationHookFailure);
    }
}
void mountAnimationPrepareEndpoints(const FixedMountState& state) {
    mountAnimationExact.clear();
    if(state.aircraft==MountAircraft::Su47)mountAnimationExact.prepare(state,mountAnimationHost);
}
void mountAnimationEnsure(FixedMountState& state,unsigned scene) {
    if(!mountAnimated(state.aircraft) || mountExterior(state.aircraft) || mountNativeOnly(state.aircraft))return; // 完整弹舱方案留档，产品不再取得共舱控制。
    MountAnimationWorkTimer work(2);
    require(!mountAnimationNativeDepth,"动画挂载：原生初始化期间不允许新接管");
    const auto aircraft=mountAnimationAircraft(state.aircraft);
    if(mountAnimationHost.controller.active() && mountAnimationHost.binding.plane.address==state.plane.address &&
       mountAnimationHost.binding.scene==scene && mountAnimationHost.description.planEquippedMask==state.kindsMask) {
        const auto now=GetTickCount64();
        if(now-mountAnimationLastMaintenance>=100) {
            require(mountAnimationHost.maintain(),"动画挂载：维持原生骨架更新失败");mountAnimationLastMaintenance=now;
        }
        return; // 原生入口、Init尾部与发射前仍即时维护；这里仅周期兜底，不逐帧重复。
    }
    require(mountAnimationRelease(),"动画挂载：旧场景控制仍待归还");
    mountAnimationInstallHooks();
    using B=::dualsp::common_bay::Binding;B binding;
    binding.world=mountAnimationIdentity(mountIdentity(mountOuterWorld(state.plane.address)));
    binding.plane=mountAnimationIdentity(state.plane);binding.activator=mountAnimationIdentity(state.animationActivator);
    binding.root=mountAnimationIdentity(state.root);binding.weaponBase=mountAnimationIdentity(state.weaponBase);
    for(unsigned i=0;i<3;++i)binding.racks[i]=mountAnimationIdentity(state.animationRacks[i]);
    binding.engine=read<Addr>(state.plane.address+0x1410);binding.parts=binding.engine+0xf90;
    require(mountAnimationGeneration!=~std::uint64_t{0},"动画挂载：绑定代次耗尽");
    binding.generation=++mountAnimationGeneration;binding.scene=scene;
    mountAnimationHost.access.rackOffsets=state.rackOffsets;
    require(mountAnimationHost.prepare(aircraft,binding,state.animationDescription),"动画挂载：宿主预检或接管失败");
    try { mountAnimationPrepareEndpoints(state); }
    catch(...) { mountAnimationRelease();throw; }
    mountAnimationProfile.reset(binding.generation,mountSpecification(state.aircraft).code);
    mountAnimationClosedVisualsApplied=false;
    mountAnimationLastMaintenance=GetTickCount64();
    nativeLog(Json{{"event","ANIMATED_MOUNT_BOUND"},{"aircraft",mountSpecification(state.aircraft).code},
        {"scene",scene},{"generation",binding.generation},{"mask",state.kindsMask}}.dump());
}
bool mountAnimationShow(const FixedMountState& state,const MountVisual&) {
    if(state.aircraft!=MountAircraft::Su47)return true;
    return mountAnimationHost.binding.plane.address==state.plane.address && mountAnimationExact.matches(mountAnimationHost);
}
void mountAnimationQueueSwitch(const ::dualsp::common_bay::Snapshot& saved) {
    require(saved.valid && !mountAnimationPendingSwitch.valid && mountAnimationHost.controller.active() &&
        ::dualsp::common_bay::same(saved.binding,mountAnimationHost.binding),"动画挂载：切换快照排队身份不符");
    mountAnimationPendingSwitch=saved;
}
void mountAnimationAfterArmed(const Command& command,bool armed) noexcept {
    if(!mountAnimationPendingSwitch.valid)return;
    auto saved=mountAnimationPendingSwitch;mountAnimationPendingSwitch={};
    try {
        require(command.op=="switch" && armed && command.request==saved.transaction &&
            session.plane==command.plane && session.activator==command.activator &&
            session.configs[session.active]==saved.targetConfig,"动画挂载：原生SP恢复与切换快照不匹配");
        require(mountAnimationHost.controller.restoreSwitch(mountAnimationHost,mountAnimationHost.binding,
            command.request,saved)==::dualsp::common_bay::Error::None,"动画挂载：完整原生SP恢复后归还舱门状态失败");
    }catch(const std::exception& error) { mountAnimationFail(error.what()); }
}
struct MountAnimationArmedCompletion {
    const Command& command;bool completed{};
    explicit MountAnimationArmedCompletion(const Command& value):command(value){}
    ~MountAnimationArmedCompletion() {
        if(!completed && mountAnimationPendingSwitch.valid && mountAnimationPendingSwitch.transaction==command.request)
            mountAnimationFail("动画挂载：原生SP恢复中断，取消旧舱门快照");
    }
};
bool mountAnimationCopyTransform(void* output,const FixedMountState& state,int position) {
    require(position>=0 && position<state.physicalCount,"动画挂载：物理位置越界");
    auto& host=mountAnimationHost;
    require(host.controller.active() && host.binding.scene==1 && host.binding.plane.address==state.plane.address &&
        host.maintain(),"动画挂载：发射宿主身份或调度失效");
    const unsigned group=static_cast<unsigned>(state.animationProvider[position]);
    require(group<3,"动画挂载：provider编号越界");
    const auto id=host.binding.racks[group];
    const auto& name=state.animationSocket[position];
    if(state.animationDescription.skeletalMask&(1u<<group)) {
        const auto frame=read<std::uint64_t>(host.access.moduleBase()+0x3ac5520);
        // 正常的发布先后或未达可视门槛只旁路本次取位，不改变原生发射门控，也不永久关闭会话。
        if(read<unsigned>(id.address+0xd24)!=static_cast<unsigned>(frame) || read<unsigned char>(id.address+0x828)!=0)return false;
        if(state.aircraft==MountAircraft::Su47) {
            const auto visual=std::find_if(state.visuals.begin(),state.visuals.end(),[&](const MountVisual& v){return v.identity.address==state.physical[position];});
            require(visual!=state.visuals.end(),"Su-47：发射模型映射缺失");
            if(!mountAnimationShow(state,*visual))return false;
        }
    }
    const auto pose=host.access.reflection.socketTransform(MountAnimationProductAccess::productIdentity(id),
        {name[0],name[1]},MountAnimationReflection::Space::World);
    // 输出按本机已反射核对的FTransform原生48字节布局，来自当次原生Socket查询。
    alignas(16) std::array<float,12> transform{};
    std::copy(pose.rotation.begin(),pose.rotation.end(),transform.begin());
    std::copy(pose.position.begin(),pose.position.end(),transform.begin()+4);
    std::copy(pose.scale.begin(),pose.scale.end(),transform.begin()+8);
    checkMemory(reinterpret_cast<Addr>(output),48);std::memcpy(output,transform.data(),48);
    return true;
}
void mountAnimationPartsAfter(void* parts) {
    if(mountAnimationNativeDepth || !mountAnimationHost.controller.active() || mountAnimationHost.binding.parts!=reinterpret_cast<Addr>(parts))return;
    if(mountAnimationHost.aircraft!=::dualsp::common_bay::Aircraft::Su47)return;
    MountAnimationWorkTimer work(0);
    try {
        ::dualsp::common_bay::PartsSample native;
        require(mountAnimationHost.readParts(mountAnimationHost.binding,native),"动画挂载：Parts更新后实例改变");
        // 收拢立即隐藏内部弹体；全开显示由最后一次所需骨骼发布统一决定。
        if(!MountAnimationPublicationTracker::nativeOpen(native)) {
            if(!mountAnimationClosedVisualsApplied)if(auto* state=mountAnimationLayout()) {
                mountApplyVisuals(*state);mountAnimationClosedVisualsApplied=true;
            }
        } else mountAnimationClosedVisualsApplied=false;
    }catch(const std::exception& error) { mountAnimationFail(error.what()); }
}
// 整次原生Initialize期间不反复强制字段；原生最后的条件赋值全部完成后才观察三项真实请求。
// 此对象只包住实际Init调用，析构不抛异常；随后的restoreSwitch/maintain负责恢复接管。
struct MountAnimationNativeRebuild {
    bool observing{},returned{};
    explicit MountAnimationNativeRebuild(Addr plane) {
        observing=mountAnimationHost.controller.active() && mountAnimationHost.binding.plane.address==plane;
        if(observing)++mountAnimationNativeDepth;
    }
    ~MountAnimationNativeRebuild() {
        if(!observing)return;
        bool failed=!returned;
        try {
            require(mountAnimationHost.bindingAlive(mountAnimationHost.binding),"动画挂载：原生初始化改变了组件身份");
            std::array<MountAnimationHost::Request,3> wanted{};
            for(unsigned i=0;i<3;++i)if(mountAnimationHost.racks[i].prepared &&
                !mountAnimationHost.readRack(mountAnimationHost.racks[i].identity,wanted[i]))failed=true;
            if(!failed)for(unsigned i=0;i<3;++i)if(mountAnimationHost.racks[i].prepared) {
                const unsigned fields=4u|(wanted[i].paused?1u:0u);
                mountAnimationHost.controller.rememberNativeRequest(mountAnimationHost,mountAnimationHost.racks[i].identity,wanted[i],fields);
            }
        }catch(...) { failed=true; }
        --mountAnimationNativeDepth;
        if(failed)mountAnimationFail("动画挂载：原生初始化结束后的请求观察失败");
    }
};
