// CFA-50作者本体使用swcc控制顶部ADMM/TLS舱，swc3控制机腹EML盖板。
// 只补齐离开顶部舱后的收尾；不接管公共开舱请求、EML机构或骨骼求值。
#pragma once

struct Cfa50BayAnimation {
    MountIdentity plane,activator,world,body,mesh,animation;
    Addr engine{},parts{};
    size_t bodyOffset{},meshOffset{},animationOffset{};
    int count{};
    std::array<int,3> configs{0,-1,-1};
    std::array<Addr,3> weapons{};
    float seconds{},closed{};
    bool bound{},failed{},ignored{};
} cfa50BayAnimation;

void cfa50BayRelease() noexcept { cfa50BayAnimation={}; }

void cfa50BayFail(const char* reason) noexcept {
    cfa50BayAnimation.bound=false;cfa50BayAnimation.failed=true;
    try {nativeLog(Json{{"event","CFA50_BAY_FALLBACK"},{"reason",reason}}.dump());}catch(...) { }
}

bool cfa50BaySession(const Cfa50BayAnimation& state) {
    if(session.plane!=state.plane.address || session.activator!=state.activator.address ||
       session.count!=state.count || session.configs!=state.configs)return false;
    for(int i=0;i<state.count;++i)if(session.special[i].weaponClass!=state.weapons[i])return false;
    return true;
}

// 资格核对只在事务完成后进行，不在逐帧回调中遍历反射或查找资源。
void cfa50BayBind(const Command& command) noexcept {
    try {
        if(!runtimeEnabled || session.plane!=command.plane || session.activator!=command.activator) {
            cfa50BayRelease();return;
        }
        auto& state=cfa50BayAnimation;
        const auto plane=mountIdentity(command.plane);
        if(mountSameIdentity(state.plane,plane)) {
            if(state.ignored)return; // 原生UObject类身份在同一序列号内不变，不逐快照重复查父链。
            if(state.failed)return; // 同一玩家实例失败后保持原生，下一次出击才重新核对。
            if(state.bound && cfa50BaySession(state) &&
               read<Addr>(command.plane+0x1410)==state.engine)return;
        }
        cfa50BayRelease();
        state.plane=plane; // 保留失败实例身份，避免每个快照重复尝试。
        if(!mountClassIs(command.plane,"AcePlayerPawn_fa50_C","/Game/Blueprint/Player/Pawn/AcePlayerPawn_fa50")) {
            state.ignored=true;return;
        }
        require(mountClassIs(command.plane,"PlayerPlane","/Script/Nimbus") &&
            session.gameThread==GetCurrentThreadId() && session.count>=1 && session.count<=3,
            "CFA-50舱门：玩家类型、线程或配装无效");
        state.activator=mountIdentity(command.activator);
        state.world=mountIdentity(mountOuterWorld(command.plane));
        state.count=session.count;state.configs=session.configs;
        const std::array<const char*,3> names={"plwp_admm_fa50","plwp_dualtls_fa50","plwp_eml_fa50"};
        for(int i=0;i<state.count;++i) {
            const int config=state.configs[i];
            require(config>=0 && config<3,"CFA-50舱门：配置编号无效");
            const Addr weapon=session.special[i].weaponClass;mountIdentity(weapon);
            require(mountName(weapon)==std::string(names[config])+"_C" &&
                mountName(read<Addr>(weapon+0x20))==std::string("/Game/Blueprint/Weapons/Player/Variation/")+names[config],
                "CFA-50舱门：兵装资源已改变");
            state.weapons[i]=weapon;
        }
        state.bodyOffset=mountProperty(command.plane,"PlaneBodyMesh","ObjectProperty",8);
        state.body=mountIdentity(read<Addr>(command.plane+state.bodyOffset));
        require(mountClassIs(state.body.address,"SkeletalMeshComponent","/Script/Engine"),"CFA-50舱门：本体组件类型改变");
        state.meshOffset=mountProperty(state.body.address,"SkeletalMesh","ObjectProperty",8);
        state.animationOffset=mountProperty(state.body.address,"AnimClass","ClassProperty",8);
        state.mesh=mountIdentity(read<Addr>(state.body.address+state.meshOffset));
        state.animation=mountIdentity(read<Addr>(state.body.address+state.animationOffset));
        require(mountName(state.mesh.address)=="fa50" &&
            mountName(read<Addr>(state.mesh.address+0x20))=="/Game/Vehicles/Aircraft/fa50/fa50" &&
            mountName(state.animation.address)=="fa50_AnimBP_C" &&
            mountName(read<Addr>(state.animation.address+0x20))=="/Game/Blueprint/Player/Body/fa50_AnimBP",
            "CFA-50舱门：本体网格或动画资源改变");
        // CFA-50不在多挂外观白名单；本模块独立接现有Parts钩子，不授予多挂布局资格。
        mountInstallHooks();
        auto& access=mountAnimationHost.access;
        require(::dualsp::common_bay::native_parts::codeQualified(access) &&
            access.originalCodeMatches(0x1ce8013,"8b8e78010000f30f10867c01000085c9743683e901741783f9017544f30f118688010000f30f11868c010000eb2af30f118684010000f30f11868c010000f30f118694010000eb18f30f118680010000f30f118694010000f30f118690010000"),
            "CFA-50舱门：原生Parts或分域代码改变");
        state.engine=read<Addr>(command.plane+0x1410);state.parts=state.engine+0xf90;
        const Addr base=reinterpret_cast<Addr>(GetModuleHandleW(nullptr));
        require(state.engine && !(state.engine&7) && access.moduleReadOnlyPointer(read<Addr>(state.engine)) &&
            read<Addr>(state.engine+0x1268)==command.plane+0xb40 && read<Addr>(state.parts)==base+0x2fff178 &&
            ((read<unsigned>(state.parts+0x27c)>>18)&15u)==11u,
            "CFA-50舱门：引擎身份或作者分域旗帜改变");
        state.seconds=read<float>(base+0x25a35ec);state.closed=read<float>(base+0x2d7cb00);
        require(std::abs(state.seconds-.4f)<.00001f && std::abs(state.closed+.01f)<.00001f,
            "CFA-50舱门：原生开合常量改变");
        state.bound=true;
        nativeLog(Json{{"event","CFA50_BAY_BOUND"},{"domain_flags",11},{"configs",state.configs}}.dump());
    }catch(const std::exception& error) {cfa50BayFail(error.what());}
}

struct Cfa50BayFrame { int config{};std::array<float,2> top{};float combined{},speed{}; };
Cfa50BayFrame cfa50BayCurrent() {
    const auto& state=cfa50BayAnimation;
    const MountIdentityBatch identities;const auto& memory=identities.memory;
    require(cfa50BaySession(state) && identities.alive(state.plane) && identities.alive(state.activator) &&
        identities.alive(state.world) && identities.alive(state.body) && identities.alive(state.mesh) && identities.alive(state.animation),
        "CFA-50舱门：玩家会话或资源实例失效");
    require(memory.read<Addr>(state.plane.address+0x11a8)==state.activator.address &&
        memory.read<Addr>(state.plane.address+state.bodyOffset)==state.body.address &&
        memory.read<Addr>(state.body.address+state.meshOffset)==state.mesh.address &&
        memory.read<Addr>(state.body.address+state.animationOffset)==state.animation.address &&
        memory.read<Addr>(state.plane.address+0x1410)==state.engine &&
        memory.read<Addr>(state.engine+0x1268)==state.plane.address+0xb40 &&
        memory.read<Addr>(state.parts)==reinterpret_cast<Addr>(GetModuleHandleW(nullptr))+0x2fff178 &&
        ((memory.read<unsigned>(state.parts+0x27c)>>18)&15u)==11u,
        "CFA-50舱门：组件、引擎或分域改变");
    Cfa50BayFrame frame;frame.config=memory.read<int>(state.parts+0x178);
    require(frame.config>=0 && frame.config<3 && frame.config==memory.read<int>(state.plane.address+0xb4c) &&
        session.active>=0 && session.active<session.count && session.configs[session.active]==frame.config,
        "CFA-50舱门：当前配置不同步");
    frame.top={memory.read<float>(state.parts+0x180),memory.read<float>(state.parts+0x184)};
    for(float v:frame.top)require(std::isfinite(v) && v>=state.closed && v<=1,"CFA-50舱门：顶部进度越界");
    frame.combined=memory.read<float>(state.parts+0x194);
    require(std::isfinite(frame.combined) && frame.combined>=state.closed && frame.combined<=1,
        "CFA-50舱门：顶部聚合进度越界");
    const unsigned options=memory.read<unsigned>(state.parts+0x64);
    frame.speed=(options&0x40)?memory.read<float>(reinterpret_cast<Addr>(GetModuleHandleW(nullptr))+0x257beec):1.f;
    require(std::isfinite(frame.speed) && frame.speed>0,"CFA-50舱门：原生速率无效");
    return frame; // 只读批次在此结束，不跨下面的原生调用或写入。
}

bool cfa50BayParts(void* object,float delta,void* input) {
    auto& state=cfa50BayAnimation;
    if(!state.bound || !runtimeEnabled || controlledWeaponSwitch || reinterpret_cast<Addr>(object)!=state.parts)return false;
    bool called=false;
    try {
        const auto before=cfa50BayCurrent();
        const auto plane=state.plane,activator=state.activator;const Addr parts=state.parts;
        require(input && reinterpret_cast<Addr>(input)==state.parts+0xc && std::isfinite(delta) && delta>=0,
            "CFA-50舱门：原生输入或帧间隔改变");
        called=true;mountOriginalPartsUpdate(object,delta,input); // 原生整套飞控/动画恰执行一次。
        // 原生调用可能重入退出/重新绑定，必须重新核对，不能以旧引用继续访问Parts。
        if(!state.bound || !runtimeEnabled || state.parts!=parts ||
           !mountSameIdentity(plane,state.plane) || !mountSameIdentity(activator,state.activator))return true;
        const auto native=cfa50BayCurrent();
        require(before.config==native.config,"CFA-50舱门：原生更新中选择改变");
        auto after=native.top;
        // 原生可能因TLS等活动兵装保舱（Parts+b0的bit20）；当前通道仍取原生最终结果。
        for(int i=0;i<2;++i) {
            if(i!=native.config)after[i]=std::max(state.closed,before.top[i]-delta/state.seconds*native.speed);
        }
        const float top=std::max(after[0],after[1]);
        if(after==native.top && top==native.combined)return true;
        // 先预检全部写区；只写顶部两个来源及其聚合，不写spwc、swc3、request或phase。
        require(MountAnimationReflection::writable(state.parts+0x180,sizeof(after)) &&
            MountAnimationReflection::writable(state.parts+0x194,sizeof(top)),"CFA-50舱门：顶部输出不可写");
        std::memcpy(reinterpret_cast<void*>(state.parts+0x180),after.data(),sizeof(after));
        std::memcpy(reinterpret_cast<void*>(state.parts+0x194),&top,sizeof(top));
    }catch(const std::exception& error) {
        cfa50BayFail(error.what());
        if(!called)mountOriginalPartsUpdate(object,delta,input);
    }
    return true;
}
