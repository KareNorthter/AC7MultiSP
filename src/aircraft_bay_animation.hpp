// 已核资源共用的舱门收尾：旧0/1聚合域逐帧收拢，公共域仅在SP互切事务保留真实进度。
// 不取得多挂布局资格，不强制骨骼求值，不修改作者资源。
#pragma once

#include "aircraft_bay_profiles.hpp"

struct AircraftBayAnimation {
    MountIdentity plane,activator,world,body,mesh,animation;
    const AircraftBayProfile* profile{};
    Addr base{},engine{},parts{};
    uint64_t generation{};
    size_t bodyOffset{},meshOffset{},animationOffset{};
    int count{};
    std::array<int,3> configs{0,-1,-1};
    std::array<Addr,3> weapons{};
    float seconds{},closed{};
    unsigned flags{};
    bool bound{},failed{},ignored{};
    struct Pending {
        MountIdentity plane,activator;
        const AircraftBayProfile* profile{};
        Addr engine{},parts{};
        uint64_t generation{},transaction{};
        int target{-1},request{},phase{};
        float progress{};
        bool valid{};
    } pending;
} aircraftBayAnimation;
uint64_t aircraftBayGeneration{}; // 绑定换代后即使仍是同一玩家/Parts，也不能消费上一代状态。
CompatibilityFailures aircraftBayFailures; // 舱门只关闭本能力，不向兵装事务或持续效果传播错误。

void aircraftBayRelease() noexcept { aircraftBayAnimation={}; }
void aircraftBayFail(const char* reason,const char* stage,const CompatibilityContext* attempted=nullptr) noexcept {
    auto& state=aircraftBayAnimation;
    if(attempted) {
        state={}; // 绑定失败属于本次尝试，不能把上一机体的身份带进故障记录。
        state.plane={attempted->player.address,attempted->player.index,attempted->player.serial};
        state.activator={attempted->activator.address,attempted->activator.index,attempted->activator.serial};
        state.world={attempted->owner.address,attempted->owner.index,attempted->owner.serial};
    }
    state.bound=false;state.failed=true;state.pending={};
    const CompatibilityContext context=attempted?*attempted:CompatibilityContext{
        compatibilityIdentity(state.plane),compatibilityIdentity(state.activator),compatibilityIdentity(state.world)};
    if(!context.player.address) {
        // 尚未取得玩家身份时没有取得写入资格，只归还本次绑定，不把错误记给上一次机体。
        try {nativeLog(Json{{"event","AIRCRAFT_BAY_BIND_REJECTED"},{"reason",reason}}.dump());}catch(...) { }
        return;
    }
    aircraftBayFailures.recordLocal(context,stage,reason);
    if(const auto* fault=aircraftBayFailures.find(context.player))
        compatibilityLogFault("bay_animation",*fault,aircraftBayFailures.global()!=nullptr);
}
bool aircraftBaySession(const AircraftBayAnimation& state) {
    if(session.plane!=state.plane.address || session.activator!=state.activator.address ||
       session.count!=state.count || session.configs!=state.configs)return false;
    for(int i=0;i<state.count;++i)if(session.special[i].weaponClass!=state.weapons[i])return false;
    return true;
}

// 只在事务完成后核对资源。相同会话直接返回，无关机体在同一UObject序列号内只分类一次。
void aircraftBayBind(const Command& command) noexcept {
    CompatibilityContext attempted;
    try {
        if(!runtimeEnabled || session.plane!=command.plane || session.activator!=command.activator) {
            aircraftBayRelease();return;
        }
        auto& state=aircraftBayAnimation;
        const auto plane=mountIdentity(command.plane);
        attempted.player=compatibilityIdentity(plane);
        if(!aircraftBayFailures.empty()) {
            const MountIdentityBatch identities;
            aircraftBayFailures.forgetDestroyedContexts([&](const auto& previous) {
                const auto& player=previous.player;
                return !identities.alive({player.address,player.index,player.serial});
            }); // 该辅助器仅比较对象表地址/序列号，不把PendingKill当成销毁。
            if(aircraftBayFailures.find(compatibilityIdentity(plane))) {
                aircraftBayRelease();state.plane=plane;state.failed=true;return;
            }
        }
        if(mountSameIdentity(state.plane,plane)) {
            if(state.ignored || state.failed)return;
            if(state.bound && aircraftBaySession(state) && read<Addr>(command.plane+0x1410)==state.engine)return;
        }
        aircraftBayRelease();state.plane=plane;
        for(const auto& profile:aircraftBayProfiles()) {
            const std::string name=std::string("AcePlayerPawn_")+profile.pawn;
            if(mountClassIs(command.plane,(name+"_C").c_str(),("/Game/Blueprint/Player/Pawn/"+name).c_str())) {
                state.profile=&profile;break;
            }
        }
        if(!state.profile) {state.ignored=true;return;}
        require(mountClassIs(command.plane,"PlayerPlane","/Script/Nimbus") &&
            session.gameThread==GetCurrentThreadId() && session.count>=1 && session.count<=3,
            "舱门：玩家类型、线程或配装无效");
        state.bodyOffset=mountProperty(command.plane,"PlaneBodyMesh","ObjectProperty",8);
        state.body=mountIdentity(read<Addr>(command.plane+state.bodyOffset));
        require(mountClassIs(state.body.address,"SkeletalMeshComponent","/Script/Engine"),"舱门：本体组件类型改变");
        state.meshOffset=mountProperty(state.body.address,"SkeletalMesh","ObjectProperty",8);
        state.animationOffset=mountProperty(state.body.address,"AnimClass","ClassProperty",8);
        state.mesh=mountIdentity(read<Addr>(state.body.address+state.meshOffset));
        state.animation=mountIdentity(read<Addr>(state.body.address+state.animationOffset));
        const std::string body=state.profile->body;
        if(mountName(state.mesh.address)!=body ||
           mountName(read<Addr>(state.mesh.address+0x20))!="/Game/Vehicles/Aircraft/"+body+"/"+body ||
           mountName(state.animation.address)!=body+"_AnimBP_C" ||
           mountName(read<Addr>(state.animation.address+0x20))!="/Game/Blueprint/Player/Body/"+body+"_AnimBP") {
            state.ignored=true;return; // 同名Pawn的原版/其他替换资源不取得Fenrir等白名单资格。
        }
        state.activator=mountIdentity(command.activator);
        state.world=mountIdentity(mountOuterWorld(command.plane));
        attempted.activator=compatibilityIdentity(state.activator);attempted.owner=compatibilityIdentity(state.world);
        state.count=session.count;state.configs=session.configs;
        for(int i=0;i<state.count;++i) {
            const int config=state.configs[i];require(config>=0 && config<3,"舱门：配置编号无效");
            const Addr weapon=session.special[i].weaponClass;mountIdentity(weapon);
            const std::string package=state.profile->weapons[config];
            require(mountName(weapon)==package.substr(package.find_last_of('/')+1)+"_C" &&
                mountName(read<Addr>(weapon+0x20))==package,"舱门：兵装资源已改变");
            state.weapons[i]=weapon;
        }
        mountInstallHooks(); // 复用已安装的Parts入口，不为新机体增加钩子。
        auto& access=mountAnimationHost.access;
        require(::dualsp::common_bay::native_parts::codeQualified(access) &&
            access.originalCodeMatches(0x1ce8013,"8b8e78010000f30f10867c01000085c9743683e901741783f9017544f30f118688010000f30f11868c010000eb2af30f118684010000f30f11868c010000f30f118694010000eb18f30f118680010000f30f118694010000f30f118690010000") &&
            access.originalCodeMatches(0x1ce7f69,"8b454c83f8017526"),"舱门：原生Parts或分域代码改变");
        // 公共域要写回请求/阶段，额外核对完整开关及活动兵装保舱分支，不能只认入口前缀。
        if(state.profile->strategy!=AircraftBayStrategy::Shared01Close)require(access.originalCodeMatches(0x1ce7f69,
            "8b454c83f8017526f30f588e7c0100000f2fcff30f118e7c0100007253c7867c0100000000803fb802000000eb4283f803751ef30f10867c010000f30f5cc10f2fc2f30f11867c0100007324418bc6eb1583f802750cc7867c0100000000803feb0e85c0750ac7867c0100000ad723bc8986740200008b86b0000000c1e814a8017427f30f108674010000f30f108e7c0100000f2fc1730af30f118e74010000eb08f30f11867c010000"),"公共舱：原生请求、阶段或保舱分支改变");
        state.base=reinterpret_cast<Addr>(GetModuleHandleW(nullptr));
        state.engine=read<Addr>(command.plane+0x1410);state.parts=state.engine+0xf90;
        require(state.engine && !(state.engine&7) && access.moduleReadOnlyPointer(read<Addr>(state.engine)) &&
            read<Addr>(state.engine+0x1268)==command.plane+0xb40 && read<Addr>(state.parts)==state.base+0x2fff178,
            "舱门：引擎或Parts身份改变");
        state.flags=(read<unsigned>(state.parts+0x27c)>>18)&15u;
        require(state.profile->knownFlags<0 || state.flags==static_cast<unsigned>(state.profile->knownFlags),
            "舱门：已知资源分域旗帜改变");
        state.seconds=read<float>(state.base+0x25a35ec);state.closed=read<float>(state.base+0x2d7cb00);
        require(std::abs(state.seconds-.4f)<.00001f && std::abs(state.closed+.01f)<.00001f,"舱门：原生开合常量改变");
        require(aircraftBayGeneration!=~uint64_t{0},"舱门：绑定代次耗尽");
        state.generation=++aircraftBayGeneration;state.bound=true;
        nativeLog(Json{{"event","AIRCRAFT_BAY_BOUND"},{"aircraft",body},{"domain_flags",state.flags},
            {"flags_previously_known",state.profile->knownFlags>=0},{"configs",state.configs},
            {"mode",state.profile->strategy==AircraftBayStrategy::Shared01Close?"shared01_close":"public_switch_continuity"}}.dump());
    }catch(const std::exception& error) {aircraftBayFail(error.what(),"bind",&attempted);}
}

struct AircraftBayFrame {
    int config{},request{},phase{};
    std::array<float,2> top{};
    float combined{},progress{},speed{};
    bool held{};
};
AircraftBayFrame aircraftBayCurrent() {
    const auto& state=aircraftBayAnimation;
    const MountIdentityBatch identities;const auto& memory=identities.memory;
    require(aircraftBaySession(state) && identities.alive(state.plane) && identities.alive(state.activator) &&
        identities.alive(state.world) && identities.alive(state.body) && identities.alive(state.mesh) && identities.alive(state.animation),
        "舱门：玩家会话或资源实例失效");
    require(memory.read<Addr>(state.plane.address+0x11a8)==state.activator.address &&
        memory.read<Addr>(state.plane.address+state.bodyOffset)==state.body.address &&
        memory.read<Addr>(state.body.address+state.meshOffset)==state.mesh.address &&
        memory.read<Addr>(state.body.address+state.animationOffset)==state.animation.address &&
        memory.read<Addr>(state.plane.address+0x1410)==state.engine &&
        memory.read<Addr>(state.engine+0x1268)==state.plane.address+0xb40 &&
        memory.read<Addr>(state.parts)==state.base+0x2fff178 &&
        ((memory.read<unsigned>(state.parts+0x27c)>>18)&15u)==state.flags,"舱门：组件、引擎或分域改变");
    AircraftBayFrame frame;frame.config=memory.read<int>(state.parts+0x178);
    require(frame.config>=0 && frame.config<3 && frame.config==memory.read<int>(state.plane.address+0xb4c) &&
        session.active>=0 && session.active<session.count && session.configs[session.active]==frame.config,
        "舱门：当前配置不同步");
    const auto outputs=memory.read<std::array<float,7>>(state.parts+0x17c);
    frame.progress=outputs[0];frame.top={outputs[1],outputs[2]};frame.combined=outputs[6];
    for(float value:{frame.progress,frame.top[0],frame.top[1],frame.combined})
        require(std::isfinite(value) && value>=-1 && value<=1,"舱门：动画进度越界");
    if(state.profile->strategy==AircraftBayStrategy::Shared01Close) {
        const unsigned options=memory.read<unsigned>(state.parts+0x64);
        frame.speed=(options&0x40)?memory.read<float>(state.base+0x257beec):1.f;
        require(std::isfinite(frame.speed) && frame.speed>0,"舱门：原生速率无效");
    } else {
        frame.request=memory.read<int>(state.parts+0x58);frame.phase=memory.read<int>(state.parts+0x274);
        require(frame.request>=0 && frame.request<=3 && frame.phase>=0 && frame.phase<=3,"舱门：原生请求或阶段越界");
        frame.held=(memory.read<unsigned>(state.parts+0xb0)&(1u<<20))!=0;
    }
    return frame; // 只读批次不跨原生调用或写入，不保留内存页缓存。
}

bool aircraftBayParts(void* object,float delta,void* input) {
    auto& state=aircraftBayAnimation;
    // 公共域机型不增加逐帧读取或写入；只使用下面的事务快照。
    if(!state.bound || reinterpret_cast<Addr>(object)!=state.parts || state.profile->strategy!=AircraftBayStrategy::Shared01Close ||
       !runtimeEnabled || controlledWeaponSwitch)return false;
    bool called=false;
    try {
        const auto before=aircraftBayCurrent();
        const auto plane=state.plane,activator=state.activator;const Addr parts=state.parts;
        const auto generation=state.generation;
        require(input && reinterpret_cast<Addr>(input)==parts+0xc && std::isfinite(delta) && delta>=0,
            "舱门：原生输入或帧间隔改变");
        called=true;mountOriginalPartsUpdate(object,delta,input); // 原生整套飞控/动画恰执行一次。
        if(!state.bound || !runtimeEnabled || state.parts!=parts || state.generation!=generation ||
           !mountSameIdentity(plane,state.plane) || !mountSameIdentity(activator,state.activator))return true;
        const auto native=aircraftBayCurrent();
        require(before.config==native.config,"舱门：原生更新中选择改变");
        auto after=native.top;
        for(int i=0;i<2;++i)if(i!=native.config)
            after[i]=std::max(state.closed,before.top[i]-delta/state.seconds*native.speed);
        const float top=std::max(after[0],after[1]);
        if(after==native.top && top==native.combined)return true;
        // 当前通道保留活动兵装保舱后的原生结果；只写swc1、swc2及swcc，公共值/发射门槛保持原生。
        require(MountAnimationReflection::writable(parts+0x180,sizeof(after)) &&
            MountAnimationReflection::writable(parts+0x194,sizeof(top)),"舱门：聚合输出不可写");
        std::memcpy(reinterpret_cast<void*>(parts+0x180),after.data(),sizeof(after));
        std::memcpy(reinterpret_cast<void*>(parts+0x194),&top,sizeof(top));
    }catch(const std::exception& error) {
        aircraftBayFail(error.what(),"parts_update");if(!called)mountOriginalPartsUpdate(object,delta,input);
    }
    return true;
}

// 公共域只消除模组Init对同一本体开合状态的重置。MSL→SP、SP→MSL和主动关闭仍完整走原生。
void aircraftBayBeforeRebuild(const Command& command,int target) noexcept {
    auto& state=aircraftBayAnimation;state.pending={};
    if(!state.bound || state.profile->strategy==AircraftBayStrategy::Shared01Close || !runtimeEnabled || command.op!="switch")return;
    try {
        require(command.plane==state.plane.address && command.activator==state.activator.address &&
            target>=0 && target<3,"公共舱：事务身份或目标改变");
        const auto frame=aircraftBayCurrent();
        require(target!=frame.config,"公共舱：目标没有改变");
        const auto armed=read<unsigned char>(command.activator+0x2a1);
        require(armed<=1,"公共舱：兵装类别无效");
        if(!armed || frame.held || (frame.request!=1 && frame.request!=2))return;
        require(frame.phase!=2 || frame.progress==1.f,"公共舱：全开阶段与进度不同步");
        state.pending={state.plane,state.activator,state.profile,state.engine,state.parts,
            state.generation,command.request,target,frame.request,frame.phase,frame.progress,true};
    }catch(const std::exception& error) {aircraftBayFail(error.what(),"before_rebuild");}
}
void aircraftBayAfterArmed(const Command& command,bool armed) noexcept {
    auto& state=aircraftBayAnimation;const auto saved=state.pending;state.pending={};
    if(!saved.valid || !state.bound || !runtimeEnabled)return;
    try {
        require(command.op=="switch" && armed && command.request==saved.transaction &&
            command.plane==saved.plane.address && command.activator==saved.activator.address &&
            mountSameIdentity(saved.plane,state.plane) && mountSameIdentity(saved.activator,state.activator) &&
            state.profile==saved.profile && state.engine==saved.engine && state.parts==saved.parts &&
            state.generation==saved.generation &&
            read<unsigned char>(command.activator+0x2a1)==1,"公共舱：切换完成身份改变");
        const auto native=aircraftBayCurrent();
        require(native.config==saved.target,"公共舱：切换完成配置不同步");
        if(native.held || (native.request!=1 && native.request!=2))return; // 新兵装保舱或主动关闭时保留新请求。
        if(native.request==saved.request && native.phase==saved.phase && native.progress==saved.progress)return;
        require(MountAnimationReflection::writable(state.parts+0x58,sizeof(int)) &&
            MountAnimationReflection::writable(state.parts+0x274,sizeof(int)) &&
            MountAnimationReflection::writable(state.parts+0x17c,sizeof(float)),"公共舱：恢复区不可写");
        // 恢复实际采到的三项，不把半开变全开，不恢复旧配置输出，也不重放以后某次事务。
        std::memcpy(reinterpret_cast<void*>(state.parts+0x58),&saved.request,sizeof(int));
        std::memcpy(reinterpret_cast<void*>(state.parts+0x274),&saved.phase,sizeof(int));
        std::memcpy(reinterpret_cast<void*>(state.parts+0x17c),&saved.progress,sizeof(float));
        nativeLog(Json{{"event","AIRCRAFT_BAY_SWITCH_RESTORED"},{"aircraft",state.profile->body},
            {"request",command.request},{"target",saved.target},{"cover_request",saved.request},
            {"phase",saved.phase},{"progress",saved.progress}}.dump());
    }catch(const std::exception& error) {aircraftBayFail(error.what(),"after_armed");}
}
struct AircraftBayCompletion {
    const Command& command;
    ~AircraftBayCompletion() {
        auto& pending=aircraftBayAnimation.pending;
        if(pending.valid && pending.transaction==command.request)pending={};
    }
};
