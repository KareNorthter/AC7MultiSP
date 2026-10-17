// 轻量Parts补齐：只处理已核原生通道，不取得整机共舱或骨骼发布控制。
// 原生完整更新每帧恰执行一次；普通MSL/misc、飞控及武器请求均保留。
#pragma once

struct LightMountAnimation {
    MountIdentity plane,world;
    MountAircraft aircraft{MountAircraft::Unsupported};
    Addr engine{},parts{};
    float seconds{},closed{};
    unsigned flags{};
} lightMountAnimation;

bool mountLightHasAnimation(MountAircraft aircraft) {
    return aircraft==MountAircraft::Dark || aircraft==MountAircraft::Zoef;
}
void mountLightAnimationRelease() {lightMountAnimation={};}

// 以下字段来自通用PlayerPlane→engine包装器→Parts原生链，并逐条校验原指令。
// 两机只改变已从资源核对的域旗帜，不能把F-35C的机型条件直接外推。
void mountLightAnimationBind() {
    auto& state=lightMountAnimation;
    require(fixedMount.bound && mountLightHasAnimation(fixedMount.aircraft),"轻量动画：没有有效任务绑定");
    if(mountMeshIdentityEqual(state.plane,fixedMount.plane) && state.aircraft==fixedMount.aircraft &&
       state.engine==read<Addr>(fixedMount.plane.address+0x1410))return;
    state={};
    auto& access=mountAnimationHost.access;
    const std::pair<unsigned,const char*> code[]={
        {0x810efa,"488d97400b0000488d8f08140000e843224c01"},
        {0x1cd315a,"488b5908"},{0x1cd3172,"48899368120000"},{0x1cd3181,"488d8b900f0000"},
        {0x1cd55e1,"488d8b900f00004533c04c8d4b600f28cee8a93d0100"},
        {0x1ce94a4,"4c8d430c488bcbe8c0d8ffff"},{0x1ce6bb4,"488d05bd853101488901"},
        {0x1ce6d70,"4d85c00f84fe170000488bc455564881ecb8000000"},{0x81e8f8,"8b874c0b0000"}};
    for(const auto& item:code)require(access.originalCodeMatches(item.first,item.second),"轻量动画：原生Parts代码资格改变");
    require(mountClassIs(fixedMount.plane.address,"PlayerPlane","/Script/Nimbus"),"轻量动画：不是原生玩家机体");
    state.plane=fixedMount.plane;state.aircraft=fixedMount.aircraft;
    state.world=mountIdentity(mountOuterWorld(state.plane.address));
    state.engine=read<Addr>(state.plane.address+0x1410);state.parts=state.engine+0xf90;
    const Addr base=reinterpret_cast<Addr>(GetModuleHandleW(nullptr));
    require(state.engine && !(state.engine&7) && access.moduleReadOnlyPointer(read<Addr>(state.engine)) &&
        read<Addr>(state.engine+0x1268)==state.plane.address+0xb40 && read<Addr>(state.parts)==base+0x2fff178,
        "轻量动画：引擎或Parts身份不符");
    state.flags=state.aircraft==MountAircraft::Dark?6u:2u;
    require(((read<unsigned>(state.parts+0x27c)>>18)&15u)==state.flags && !(read<unsigned>(state.parts+0xb0)&(1u<<20)),
        "轻量动画：原生分域旗帜与已核资源不同");
    state.seconds=read<float>(base+0x25a35ec);state.closed=read<float>(base+0x2d7cb00);
    require(std::abs(state.seconds-.4f)<.00001f && std::abs(state.closed+.01f)<.00001f,
        "轻量动画：原生开合常量改变");
    nativeLog(Json{{"event","LIGHT_PARTS_BOUND"},{"aircraft",mountSpecification(state.aircraft).code},{"domain_flags",state.flags}}.dump());
}

int mountLightAnimationCurrent() {
    const auto& state=lightMountAnimation;
    require(mountAlive(state.plane) && mountAlive(state.world) &&
        read<Addr>(state.plane.address+0x1410)==state.engine && read<Addr>(state.engine+0x1268)==state.plane.address+0xb40 &&
        read<Addr>(state.parts)==reinterpret_cast<Addr>(GetModuleHandleW(nullptr))+0x2fff178 &&
        ((read<unsigned>(state.parts+0x27c)>>18)&15u)==state.flags,
        "轻量动画：机体、引擎或分域改变");
    const int config=read<int>(state.parts+0x178);
    require(config>=0 && config<3 && config==read<int>(state.plane.address+0xb4c),"轻量动画：原生选择不同步");
    return config;
}
std::array<float,7> mountLightAnimationProgress() {
    std::array<float,7> values{};
    checkMemory(lightMountAnimation.parts+0x17c,sizeof(values));
    std::memcpy(values.data(),reinterpret_cast<void*>(lightMountAnimation.parts+0x17c),sizeof(values));
    for(float value:values)require(std::isfinite(value) && value>=-1 && value<=1,"轻量动画：进度越界");
    return values;
}
void mountLightAnimationPut(const std::array<float,7>& values) {
    const Addr address=lightMountAnimation.parts+0x17c;
    require(MountAnimationReflection::writable(address,sizeof(values)),"轻量动画：输出不可写");
    std::memcpy(reinterpret_cast<void*>(address),values.data(),sizeof(values));
}

// CFA旧姿态快照/强收舱方案保留于a4f7d1b（2.15）；现在恢复原生互切，不取得公共开盖/发射门槛。
void mountLightAnimationBeforeRebuild(const Command&,int) {}
void mountLightAnimationCommit() {
    if(!fixedMount.bound || !mountLightHasAnimation(fixedMount.aircraft)) {mountLightAnimationRelease();return;}
    mountLightAnimationBind();
}

bool mountLightAnimationParts(void* object,float delta,void* input) {
    auto& state=lightMountAnimation;
    if(!state.parts || reinterpret_cast<Addr>(object)!=state.parts || !fixedMount.bound || fixedMount.rebuilding ||
       !mountMeshIdentityEqual(state.plane,fixedMount.plane))return false;
    bool called=false;
    try {
        const int config=mountLightAnimationCurrent();
        require(input && reinterpret_cast<Addr>(input)==state.parts+0xc && std::isfinite(delta) && delta>=0,
            "轻量动画：原生输入或帧间隔改变");
        const auto before=mountLightAnimationProgress();
        called=true;mountOriginalPartsUpdate(object,delta,input);
        require(config==mountLightAnimationCurrent(),"轻量动画：原生更新期间选择改变");
        auto after=mountLightAnimationProgress();
        const unsigned options=read<unsigned>(reinterpret_cast<Addr>(input)+0x58);
        const float speed=(options&0x40)?read<float>(reinterpret_cast<Addr>(GetModuleHandleW(nullptr))+0x257beec):1.f;
        require(std::isfinite(speed) && speed>0,"轻量动画：原生速率无效");
        auto close=[&](float value) {return std::max(state.closed,value-delta/state.seconds*speed);};
        // Dark/Falken只收非当前独立通道；CFA根本不进入此更新路径。
        for(int channel=0;channel<3;++channel)if(channel!=config)after[1+channel]=close(before[1+channel]);
        after[4]=std::max(after[2],after[3]);after[5]=std::max(after[1],after[3]);after[6]=std::max(after[1],after[2]);
        mountLightAnimationPut(after);
        mountAnimationPartsAfter(object);
    }catch(const std::exception& error) {
        fixedMountFail(error.what());if(!called)mountOriginalPartsUpdate(object,delta,input);
    }
    return true;
}
