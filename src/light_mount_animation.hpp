// 轻量Parts补齐：只处理已核原生通道，不取得整机共舱或骨骼发布控制。
// 原生完整更新每帧恰执行一次；普通MSL/misc、飞控及武器请求均保留。
#pragma once

struct LightMountAnimation {
    MountIdentity plane,world;
    MountAircraft aircraft{MountAircraft::Unsupported};
    Addr engine{},parts{};
    float seconds{},closed{};
    unsigned flags{};
    struct CfaSwitch {
        MountIdentity plane,activator;
        Addr engine{},parts{};
        uint64_t transaction{};
        int target{-1},request{},phase{};
        float progress{};
        bool valid{};
    } pending;
    // 只接管ADMM盖板的swc1输出；公共spwc和发射阶段不做逐帧强写。
    struct CfaBody { float native{},written{};bool owned{}; } cfaBody;
} lightMountAnimation;

bool mountLightHasAnimation(MountAircraft aircraft) {
    return aircraft==MountAircraft::Dark || aircraft==MountAircraft::Zoef || aircraft==MountAircraft::Fa44;
}
void mountLightAnimationReturnCfaBody() {
    auto& state=lightMountAnimation;
    if(!state.cfaBody.owned)return;
    // 实例销毁/引擎换代后不访问旧Parts；原生已改写时不以旧基线覆盖它。
    if(mountAlive(state.plane) && read<Addr>(state.plane.address+0x1410)==state.engine &&
       read<Addr>(state.engine+0x1268)==state.plane.address+0xb40 &&
       read<Addr>(state.parts)==reinterpret_cast<Addr>(GetModuleHandleW(nullptr))+0x2fff178 &&
       read<float>(state.parts+0x180)==state.cfaBody.written) {
        require(MountAnimationReflection::writable(state.parts+0x180,sizeof(float)),"CFA盖板：归还输出不可写");
        std::memcpy(reinterpret_cast<void*>(state.parts+0x180),&state.cfaBody.native,sizeof(float));
    }
    state.cfaBody={};
}
bool mountLightAnimationRelease() {
    lightMountAnimation.pending={};
    try {mountLightAnimationReturnCfaBody();lightMountAnimation={};return true;}
    catch(const std::exception& error) {
        nativeLog(std::string("LIGHT_PARTS_RELEASE_ERROR ")+error.what());return false;
    } // 失败保留唯一输出的归还责任，后续退出流程重试。
}

// 以下字段来自通用PlayerPlane→engine包装器→Parts原生链，并逐条校验原指令。
// 三机只使用已从资源核对的域旗帜，不能把F-35C的机型条件直接外推。
void mountLightAnimationBind() {
    auto& state=lightMountAnimation;
    require(fixedMount.bound && mountLightHasAnimation(fixedMount.aircraft),"轻量动画：没有有效任务绑定");
    if(mountMeshIdentityEqual(state.plane,fixedMount.plane) && state.aircraft==fixedMount.aircraft &&
       state.engine==read<Addr>(fixedMount.plane.address+0x1410))return;
    require(mountLightAnimationRelease(),"轻量动画：旧盖板输出尚未归还");
    auto& access=mountAnimationHost.access;
    const std::pair<unsigned,const char*> code[]={
        {0x810efa,"488d97400b0000488d8f08140000e843224c01"},
        {0x1cd315a,"488b5908"},{0x1cd3172,"48899368120000"},{0x1cd3181,"488d8b900f0000"},
        {0x1cd55e1,"488d8b900f00004533c04c8d4b600f28cee8a93d0100"},
        {0x1ce94a4,"4c8d430c488bcbe8c0d8ffff"},{0x1ce6bb4,"488d05bd853101488901"},
        {0x1ce6d70,"4d85c00f84fe170000488bc455564881ecb8000000"},{0x81e8f8,"8b874c0b0000"}};
    for(const auto& item:code)require(access.originalCodeMatches(item.first,item.second),"轻量动画：原生Parts代码资格改变");
    if(fixedMount.aircraft==MountAircraft::Fa44) {
        require(access.originalCodeMatches(0x1ce7f69,"8b454c83f8017526") &&
            access.originalCodeMatches(0x1ce7f86,"c7867c0100000000803fb802000000") &&
            access.originalCodeMatches(0x1ce8013,"8b8e78010000f30f10867c010000") &&
            access.originalCodeMatches(0x1ce805b,"f30f118680010000"),
            "CFA舱门：原生开舱及全开阶段入口改变");
    }
    require(mountClassIs(fixedMount.plane.address,"PlayerPlane","/Script/Nimbus"),"轻量动画：不是原生玩家机体");
    state.plane=fixedMount.plane;state.aircraft=fixedMount.aircraft;
    state.world=mountIdentity(mountOuterWorld(state.plane.address));
    state.engine=read<Addr>(state.plane.address+0x1410);state.parts=state.engine+0xf90;
    const Addr base=reinterpret_cast<Addr>(GetModuleHandleW(nullptr));
    require(state.engine && !(state.engine&7) && access.moduleReadOnlyPointer(read<Addr>(state.engine)) &&
        read<Addr>(state.engine+0x1268)==state.plane.address+0xb40 && read<Addr>(state.parts)==base+0x2fff178,
        "轻量动画：引擎或Parts身份不符");
    state.flags=state.aircraft==MountAircraft::Dark?6u:state.aircraft==MountAircraft::Zoef?2u:1u;
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

// CFA三副机构使用spwc，Body另有swc1盖板；选有ADMM时让这组盖板跟随真实公共进度。
// 只复制一个输出，不能压低ECM的spwc或伪造全开阶段，也不逐骨检查/反射调用。
void mountLightAnimationCfaBody(int config) {
    auto& state=lightMountAnimation;
    if(!(fixedMount.kindsMask&1u))return;
    std::array<float,2> values{};
    checkMemory(state.parts+0x17c,sizeof(values));
    std::memcpy(values.data(),reinterpret_cast<void*>(state.parts+0x17c),sizeof(values));
    for(float value:values)require(std::isfinite(value) && value>=-1 && value<=1,"CFA盖板：进度无效");
    auto& owned=state.cfaBody;
    // config0由原生本帧写swc1；其他配置只有观察到新值时才更新原生基线。
    if(config==0 || !owned.owned || values[1]!=owned.written)owned={values[1],values[1],false};
    if(values[0]==values[1])return;
    require(MountAnimationReflection::writable(state.parts+0x180,sizeof(float)),"CFA盖板：同步输出不可写");
    owned.written=values[0];owned.owned=true;
    std::memcpy(reinterpret_cast<void*>(state.parts+0x180),&values[0],sizeof(float));
}
int mountLightAnimationCfaFrameCurrent() {
    const auto& state=lightMountAnimation;
    // 完整资格在绑定/切换时核对；本次原生回调已给出Parts，只复核活机体、所属引擎和当前域。
    require(mountAlive(state.plane) && read<Addr>(state.plane.address+0x1410)==state.engine &&
        ((read<unsigned>(state.parts+0x27c)>>18)&15u)==1u,"CFA盖板：玩家或公共舱域改变");
    const int config=read<int>(state.parts+0x178);
    require(config>=0 && config<3,"CFA盖板：原生配置越界");
    return config;
}
void mountLightAnimationBeforeRebuild(const Command& command,int target) {
    auto& state=lightMountAnimation;state.pending={};
    if(!fixedMount.bound || fixedMount.aircraft!=MountAircraft::Fa44)return;
    mountLightAnimationBind();
    // 先归还附属盖板，让Init产生的新原生值成为下一次基线。
    mountLightAnimationReturnCfaBody();
    if(command.op!="switch" || command.plane!=state.plane.address ||
       command.activator!=fixedMount.animationActivator.address)return;
    const int prior=mountLightAnimationCurrent();
    require(target>=0 && target<3 && prior!=target,"CFA舱门：切换配置无效");
    require(mountAlive(fixedMount.animationActivator),"CFA舱门：切换激活器失效");
    const auto armed=read<unsigned char>(command.activator+0x2a1);
    require(armed<=1,"CFA舱门：兵装类别无效");
    if(!armed)return; // MSL选另一SP仍走原生开舱，不凭尚未收完的姿态猜测SP互切。
    LightMountAnimation::CfaSwitch saved;
    saved.plane=state.plane;saved.activator=fixedMount.animationActivator;
    saved.engine=state.engine;saved.parts=state.parts;saved.target=target;saved.transaction=command.request;
    saved.request=read<int>(state.parts+0x58);saved.phase=read<int>(state.parts+0x274);
    saved.progress=read<float>(state.parts+0x17c);
    require(saved.request>=0 && saved.request<=3 && saved.phase>=0 && saved.phase<=3 &&
        std::isfinite(saved.progress) && saved.progress>=-1 && saved.progress<=1,"CFA舱门：切换前状态无效");
    if(saved.request!=1 && saved.request!=2)return; // 保留原生主动关闭等特殊情况，不强制永久开舱。
    require(saved.phase!=2 || saved.progress==1.f,"CFA舱门：全开阶段与进度不同步");
    saved.valid=true;state.pending=saved;
}
// Init及完整飞机SP切换都会改舱门，必须等keepArmed完成后消费一次；不能延迟到未来某次重选SP。
void mountLightAnimationAfterArmed(const Command& command,bool armed) noexcept {
    auto& state=lightMountAnimation;
    const auto saved=state.pending;state.pending={};
    if(!saved.valid)return;
    try {
        require(fixedMountEnabled && !fixedMountFailed && fixedMount.bound &&
            fixedMount.aircraft==MountAircraft::Fa44 && command.op=="switch" && armed &&
            command.request==saved.transaction && command.plane==saved.plane.address &&
            command.activator==saved.activator.address && mountMeshIdentityEqual(saved.plane,state.plane) &&
            mountMeshIdentityEqual(saved.plane,fixedMount.plane) && state.engine==saved.engine && state.parts==saved.parts &&
            mountMeshIdentityEqual(saved.activator,fixedMount.animationActivator) && mountAlive(saved.activator) &&
            read<unsigned char>(saved.activator.address+0x2a1)==1 && saved.target==mountLightAnimationCurrent() &&
            session.plane==command.plane && session.activator==command.activator &&
            session.active>=0 && session.active<session.count && session.configs[session.active]==saved.target,
            "CFA舱门：切换完成身份或目标不符");
        // 三处完整预检后同栈写回，不穿插引擎调用；保留采样阶段，不把半开直接变成可发射。
        require(MountAnimationReflection::writable(state.parts+0x58,sizeof(int)) &&
            MountAnimationReflection::writable(state.parts+0x274,sizeof(int)) &&
            MountAnimationReflection::writable(state.parts+0x17c,sizeof(float)),"CFA舱门：恢复区不可写");
        std::memcpy(reinterpret_cast<void*>(state.parts+0x58),&saved.request,sizeof(int));
        std::memcpy(reinterpret_cast<void*>(state.parts+0x274),&saved.phase,sizeof(int));
        std::memcpy(reinterpret_cast<void*>(state.parts+0x17c),&saved.progress,sizeof(float));
        mountLightAnimationCfaBody(saved.target);
    }catch(const std::exception& error) {fixedMountFail(error.what());}
}
void mountLightAnimationCommit() {
    if(!fixedMount.bound || !mountLightHasAnimation(fixedMount.aircraft)) {
        require(mountLightAnimationRelease(),"轻量动画：退出时盖板仍待归还");return;
    }
    mountLightAnimationBind();
    require(!lightMountAnimation.pending.valid,"CFA舱门：切换快照未在事务内消费");
}

bool mountLightAnimationParts(void* object,float delta,void* input) {
    auto& state=lightMountAnimation;
    if(!state.parts || reinterpret_cast<Addr>(object)!=state.parts || !fixedMount.bound || fixedMount.rebuilding ||
       !mountMeshIdentityEqual(state.plane,fixedMount.plane))return false;
    if(state.aircraft==MountAircraft::Fa44) {
        // 无ADMM则没有额外swc1盖板，连通道同步也无需执行。
        if(!(fixedMount.kindsMask&1u))return false;
        mountOriginalPartsUpdate(object,delta,input);
        try {
            // 原生调用可能触发退出/换代；返回后重新确认范围，再读输出。
            if(!fixedMount.bound || fixedMount.rebuilding || state.aircraft!=MountAircraft::Fa44 ||
               reinterpret_cast<Addr>(object)!=state.parts || !mountMeshIdentityEqual(state.plane,fixedMount.plane))return true;
            require(input && reinterpret_cast<Addr>(input)==state.parts+0xc,"CFA盖板：原生输入改变");
            mountLightAnimationCfaBody(mountLightAnimationCfaFrameCurrent());
        }catch(const std::exception& error) {fixedMountFail(error.what());}
        return true;
    }
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
        // Dark/Falken只收非当前独立通道；CFA不执行这套进度衰减。
        for(int channel=0;channel<3;++channel)if(channel!=config)after[1+channel]=close(before[1+channel]);
        after[4]=std::max(after[2],after[3]);after[5]=std::max(after[1],after[3]);after[6]=std::max(after[1],after[2]);
        mountLightAnimationPut(after);
        mountAnimationPartsAfter(object);
    }catch(const std::exception& error) {
        fixedMountFail(error.what());if(!called)mountOriginalPartsUpdate(object,delta,input);
    }
    return true;
}
