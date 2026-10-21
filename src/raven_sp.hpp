// ADF-11F 原生三 SP 的模型回收交接；不接管无人机的攻击、目标或寿命。
#pragma once

struct RavenPose {
    ContinuousIdentity component,parent;
    uint64_t socket{};
    std::array<float,3> location{},rotation{};
};
struct RavenVisualState {
    ContinuousIdentity plane,activator,manager,meshManager;
    std::vector<RavenPose> poses;
} ravenVisual;
struct RavenPoseSetter {
    using Process=void(*)(void*,void*,void*);
    using Size=const unsigned short*(*)(const void*);
    using Count=const unsigned char*(*)(const void*);
    using Available=bool(*)();
    Process process{};Size size{};Count count{};Available available{};
    ContinuousIdentity function;
    int parameterSize{},location{},rotation{},teleport{};
} ravenPoseSetter;

void ravenBindPoseSetter(Addr component) {
    if(ravenPoseSetter.function.address) {
        require(continuousAlive(ravenPoseSetter.function),"UAV 回收：位置接口失效");return;
    }
    const auto module=GetModuleHandleW(L"UE4SS.dll");
    require(module!=nullptr,"UAV 回收：加载器未就绪");
    RavenPoseSetter next;
    next.process=reinterpret_cast<RavenPoseSetter::Process>(GetProcAddress(module,"?ProcessEvent@UObject@Unreal@RC@@QEAAXPEAVUFunction@23@PEAX@Z"));
    next.size=reinterpret_cast<RavenPoseSetter::Size>(GetProcAddress(module,"?GetParmsSize@UFunction@Unreal@RC@@QEBAAEBGXZ"));
    next.count=reinterpret_cast<RavenPoseSetter::Count>(GetProcAddress(module,"?GetNumParms@UFunction@Unreal@RC@@QEBAAEBEXZ"));
    next.available=reinterpret_cast<RavenPoseSetter::Available>(GetProcAddress(module,"?IsProcessEventAvailable@UE4SSRuntime@RC@@SA_NXZ"));
    require(next.process && next.size && next.count && next.available && next.available(),"UAV 回收：公开反射接口不可用");
    Addr function=0,cls=read<Addr>(component+0x10);
    for(int depth=0;cls && depth<32 && !function;++depth,cls=read<Addr>(cls+0x30)) {
        continuousIdentity(cls);Addr field=read<Addr>(cls+0x38);
        for(int n=0;field && n<1024;++n,field=read<Addr>(field+0x28)) {
            continuousIdentity(field);
            if(continuousName(field)=="K2_SetRelativeLocationAndRotation" && continuousName(read<Addr>(field+0x10))=="Function") {function=field;break;}
        }
        require(!field || function,"UAV 回收：位置接口字段链过长");
    }
    require(function && continuousName(read<Addr>(function+0x20))=="SceneComponent" &&
        continuousName(read<Addr>(read<Addr>(function+0x20)+0x20))=="/Script/Engine","UAV 回收：位置接口所属类不符");
    next.function=continuousIdentity(function);
    next.parameterSize=read<unsigned short>(reinterpret_cast<Addr>(next.size(reinterpret_cast<void*>(function))));
    require(read<unsigned char>(reinterpret_cast<Addr>(next.count(reinterpret_cast<void*>(function))))==5 &&
        next.parameterSize>0 && next.parameterSize<=512,"UAV 回收：位置接口参数布局不符");
    unsigned mask=0;Addr field=read<Addr>(function+0x38);std::vector<std::pair<int,int>> ranges;
    for(int n=0;field && n<6;++n,field=read<Addr>(field+0x28)) {
        continuousIdentity(field);const auto name=continuousName(field),kind=continuousName(read<Addr>(field+0x10));
        const int bytes=read<int>(field+0x34),offset=read<int>(field+0x44);
        require(offset>=0 && bytes>0 && offset+bytes<=next.parameterSize,"UAV 回收：位置参数越界");
        for(const auto& range:ranges)require(offset+bytes<=range.first || offset>=range.second,"UAV 回收：位置参数重叠");
        ranges.emplace_back(offset,offset+bytes);unsigned bit=0;
        if(name=="NewLocation" && kind=="StructProperty" && bytes==12){next.location=offset;bit=1;}
        else if(name=="NewRotation" && kind=="StructProperty" && bytes==12){next.rotation=offset;bit=2;}
        else if(name=="bTeleport" && kind=="BoolProperty" && bytes==1){next.teleport=offset;bit=4;}
        else if(name=="bSweep" && kind=="BoolProperty" && bytes==1)bit=8;
        else if(name=="SweepHitResult" && kind=="StructProperty")bit=16;
        require(bit && !(mask&bit),"UAV 回收：位置参数类型改变");mask|=bit;
    }
    require(!field && mask==31,"UAV 回收：位置参数缺失");ravenPoseSetter=next;
}
void ravenRestorePose(const RavenPose& pose) {
    require(continuousAlive(pose.component) && continuousAlive(pose.parent) &&
        read<Addr>(pose.component.address+0x100)==pose.parent.address && read<uint64_t>(pose.component.address+0x108)==pose.socket,
        "UAV 回收：原挂点身份改变");
    if(std::memcmp(reinterpret_cast<void*>(pose.component.address+0x15c),pose.location.data(),12)==0 &&
        std::memcmp(reinterpret_cast<void*>(pose.component.address+0x168),pose.rotation.data(),12)==0)return;
    ravenBindPoseSetter(pose.component.address);
    require(ravenPoseSetter.available(),"UAV 回收：位置接口尚不可用");
    alignas(16) std::array<unsigned char,512> params{};
    std::memcpy(params.data()+ravenPoseSetter.location,pose.location.data(),12);
    std::memcpy(params.data()+ravenPoseSetter.rotation,pose.rotation.data(),12);params[ravenPoseSetter.teleport]=1;
    ravenPoseSetter.process(reinterpret_cast<void*>(pose.component.address),reinterpret_cast<void*>(ravenPoseSetter.function.address),params.data());
    for(int i=0;i<3;++i)require(std::abs(read<float>(pose.component.address+0x15c+i*4)-pose.location[i])<.001f &&
        std::abs(read<float>(pose.component.address+0x168+i*4)-pose.rotation[i])<.001f,"UAV 回收：原挂点未归还");
}
bool ravenSameVisual(Addr plane,Addr activator) {
    return ravenVisual.plane.address==plane && ravenVisual.activator.address==activator &&
        continuousAlive(ravenVisual.plane) && continuousAlive(ravenVisual.activator) &&
        continuousAlive(ravenVisual.manager) && continuousAlive(ravenVisual.meshManager) &&
        read<Addr>(activator+0x260)==ravenVisual.manager.address &&
        read<Addr>(ravenVisual.manager.address+0x150)==ravenVisual.meshManager.address;
}
// 两个组件的原位在任务 setup 时取得；不能把回收曲线上的临时位置当作原位。
void ravenBindVisual(Addr plane,Addr activator) {
    if(ravenSameVisual(plane,activator)) {
        const auto pool=continuousArray(ravenVisual.meshManager.address+0x170);
        bool same=pool.count==static_cast<int>(ravenVisual.poses.size());
        for(int i=0;same && i<pool.count;++i)same=continuousAlive(ravenVisual.poses[i].component) &&
            read<Addr>(pool.data+i*8)==ravenVisual.poses[i].component.address;
        if(same)return;
    }
    RavenVisualState next;next.plane=continuousIdentity(plane);next.activator=continuousIdentity(activator);
    require(read<Addr>(plane+0x11a8)==activator,"UAV 回收：激活器不属于当前机体");
    next.manager=continuousIdentity(read<Addr>(activator+0x260));
    next.meshManager=continuousIdentity(read<Addr>(next.manager.address+0x150));
    require(read<Addr>(next.meshManager.address+0x128)==plane && read<Addr>(next.meshManager.address+0x130)==next.manager.address,
        "UAV 回收：模型管理器归属改变");
    const auto pool=continuousArray(next.meshManager.address+0x170); // 持久 SWP3 组件数组，不是战斗 Actor 池。
    require(pool.count==2,"UAV 回收：原生模型池数量改变");
    const Addr rack=read<Addr>(plane+0x1b90);
    require(continuousName(rack)=="SWP3","UAV 回收：原生挂架改变");
    for(int i=0;i<pool.count;++i) {
        RavenPose pose;pose.component=continuousIdentity(read<Addr>(pool.data+i*8));
        pose.parent=continuousIdentity(read<Addr>(pose.component.address+0x100));
        require(pose.parent.address==rack && continuousName(read<Addr>(pose.component.address+0x10))=="StaticMeshComponent" &&
            continuousName(read<Addr>(pose.component.address+0x728))=="w_uav_x0_SM","UAV 回收：原生组件或网格改变");
        pose.socket=read<uint64_t>(pose.component.address+0x108);
        for(int j=0;j<3;++j) {
            pose.location[j]=read<float>(pose.component.address+0x15c+j*4);pose.rotation[j]=read<float>(pose.component.address+0x168+j*4);
            require(std::isfinite(pose.location[j]) && std::isfinite(pose.rotation[j]),"UAV 回收：原位数据无效");
        }
        next.poses.push_back(pose);
    }
    ravenVisual=std::move(next);
}
ContinuousArray ravenQueue(Addr header) {
    auto array=continuousArray(header);
    if(array.capacity)checkMemory(array.data,static_cast<size_t>(array.capacity)*16);
    return array;
}
// 仅筛选，不修改原队列；后续所有组件检查通过后才能提交。
std::vector<std::array<unsigned char,16>> ravenKeepMainQueue(const ContinuousArray& delayed) {
    std::vector<std::array<unsigned char,16>> keep;
    for(int i=0;i<delayed.count;++i) {
        const Addr item=delayed.data+i*16;const auto group=read<unsigned char>(item);
        require(group<=2,"UAV 回收：延迟队列组别改变");
        if(group==2)continue;
        std::array<unsigned char,16> entry{};std::memcpy(entry.data(),reinterpret_cast<void*>(item),16);keep.push_back(entry);
    }
    return keep;
}
// 只裁掉 SP 组；MSL 的延迟显隐条目不参与本次换槽。
void ravenClearQueues() {
    const Addr manager=ravenVisual.meshManager.address;
    const auto delayed=ravenQueue(manager+0x110),moving=ravenQueue(manager+0x190);
    const auto keep=ravenKeepMainQueue(delayed);
    // 回收运动项直接持有组件；不允许旧指针在新配置里继续改位置。
    for(int i=0;i<moving.count;++i) {
        const Addr component=read<Addr>(moving.data+i*16);
        require(std::any_of(ravenVisual.poses.begin(),ravenVisual.poses.end(),[component](const auto& p){return p.component.address==component;}),
            "UAV 回收：运动队列含未知组件");
    }
    // 两组检查完成再更新数量，不复制头、扩大容量或跨分配器释放。
    for(size_t i=0;i<keep.size();++i)std::memcpy(reinterpret_cast<void*>(delayed.data+i*16),keep[i].data(),16);
    write<int>(delayed.header+8,static_cast<int>(keep.size()));write<int>(moving.header+8,0);
    for(const auto& pose:ravenVisual.poses)ravenRestorePose(pose);
}
void ravenBeforeRebuild(const Command& command) {
    if(!continuousRaven(command.plane))return;
    if(command.count>1 && !continuousRavenLoadout(command.activator)) {
        // Mod/涂装换成电子战等兵装时不因Raven身份整机拒绝；选中真实UAV仍保留原配置约束。
        for(int i=0;i<command.count;++i) {
            const Addr cls=read<Addr>(command.activator+0x158+command.configs[i]*0x30);
            require(!continuousClassDerived(cls,"WeaponUAV","/Script/Nimbus"),
                "ADF-11F 的多SP UAV仍只支持原生 TLS / QAAM / UAV 配置");
        }
    }
    if(!continuousRavenLoadout(command.activator))return;
    if(!ravenSameVisual(command.plane,command.activator)) {
        ravenVisual={};return; // 新任务先让原生创建持久模型池，再在 AfterRebuild 记录原位。
    }
    ravenBindVisual(command.plane,command.activator);ravenClearQueues();
}
void ravenAfterRebuild(const Command& command) {
    if(!continuousRaven(command.plane) || !continuousRavenLoadout(command.activator))return;
    ravenBindVisual(command.plane,command.activator);ravenClearQueues();
}
void ravenRestoreReload(const Command& command) {
    if(!continuousRaven(command.plane) || !continuousRavenLoadout(command.activator))return;
    require(ravenSameVisual(command.plane,command.activator),"UAV 回收：重建后的模型管理器改变");
    ravenClearQueues();
    const Addr cdo=read<Addr>(groupOf(command.activator,true)+0x40);
    if(read<unsigned char>(cdo+0x3b0)!=26)return;
    require(continuousSupported(command.plane,cdo),"UAV 回收：当前 UAV 不是原生机制");
    const auto slots=weaponSlotArray(groupOf(command.activator,true));
    require(slots.count==2,"UAV 回收：原生装填槽数改变");
    const Addr base=continuousBase();
    constexpr unsigned char hide[]={0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x6c,0x24,0x10,0x57,0x48,0x83,0xec,0x40};
    constexpr unsigned char show[]={0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x74,0x24,0x10,0x57,0x48,0x83,0xec,0x40};
    require(std::memcmp(reinterpret_cast<void*>(base+0x8a0730),hide,sizeof(hide))==0 &&
        std::memcmp(reinterpret_cast<void*>(base+0x8a0260),show,sizeof(show))==0,"UAV 回收：原生入队入口改变");
    const Addr manager=ravenVisual.meshManager.address;
    const bool canReturn=read<Addr>(manager+0x1a0) && read<Addr>(manager+0x1a8);
    const float returnTime=canReturn?read<float>(manager+0x188):0;
    require(std::isfinite(returnTime) && returnTime>=0 && returnTime<=10,"UAV 回收：原生回收时长改变");
    using Schedule=void(*)(void*,unsigned char,int,float,bool);
    for(int i=0;i<slots.count;++i) {
        const float duration=read<float>(slots.data+i*0x60+0x48),elapsed=read<float>(slots.data+i*0x60+0x4c);
        dualsp::validateReload(duration,elapsed);
        // 已就绪槽的一次性重建门槛交给原生 Tick，不重新制造一次两秒回收。
        const float remaining=duration-elapsed;
        if(remaining<=.001f)continue;
        const bool returning=canReturn && remaining<=returnTime;
        reinterpret_cast<Schedule>(base+0x8a0730)(reinterpret_cast<void*>(manager),2,i,returning?0:remaining,true);
        if(returning) {
            // 已处于最后两秒时直接进入曲线的对应进度，不再从两秒起点重新播放。
            const auto before=ravenQueue(manager+0x190);
            reinterpret_cast<Schedule>(base+0x8a0260)(reinterpret_cast<void*>(manager),2,i,0,true);
            const auto after=ravenQueue(manager+0x190);
            require(after.count==before.count || after.count==before.count+1,"UAV 回收：运动入队数量异常");
            if(after.count>before.count)write<float>(after.data+before.count*16+8,returnTime-remaining);
        }
    }
    nativeLog("RAVEN_UAV_RELOAD_REBUILT");
}
void ravenAbortRebuild() noexcept {
    try {if(ravenSameVisual(session.plane,session.activator))ravenClearQueues();}
    catch(const std::exception& error){continuousRuntimeFailure=error.what();nativeLog(std::string("RAVEN_ABORT_ERROR ")+error.what());}
}
