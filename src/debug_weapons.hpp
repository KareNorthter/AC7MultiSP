#pragma once
// 只接管已配置的玩家SP；复用原生扣弹和装填队列，不改武器蓝图。
struct DebugWeapons {
    ContinuousIdentity plane,activator;
    DWORD thread{};
    ULONGLONG heartbeat{};
    bool allowed{};
    std::string error;
} debugWeapons;
using DebugConsume=void(*)(void*,unsigned char,int,void*,bool);
DebugConsume originalDebugConsume{};

bool debugWeaponsLive(Addr activator) {
    return runtimeEnabled && debugWeapons.allowed && GetCurrentThreadId()==debugWeapons.thread &&
        GetTickCount64()-debugWeapons.heartbeat<=500 && activator==debugWeapons.activator.address &&
        session.activator==activator && session.plane==debugWeapons.plane.address &&
        session.gameThread==GetCurrentThreadId() && continuousAlive(debugWeapons.plane) &&
        continuousAlive(debugWeapons.activator) && read<Addr>(session.plane+0x11a8)==activator &&
        read<Addr>(activator+0x250)==session.plane;
}
void debugWeaponsFailure(const char* error) noexcept {
    debugWeapons.allowed=false;debugInfiniteAmmo=false;debugFastReload=false;
    try {debugWeapons.error=error;nativeLog(std::string("DEBUG_WEAPONS_ERROR ")+error);}catch(...) {}
}
void debugBoostReload(Addr activator) {
    if(!debugFastReload)return;
    const Addr group=groupOf(activator,true);
    if(!debugReloadEligible(group))return;
    const auto slots=weaponSlotArray(group);
    const int window=read<int>(group+0x10),next=read<int>(group+0x14);
    for(int i=0;i<window;++i) {
        const Addr timer=slots.data+static_cast<size_t>((next+i)%slots.count)*0x60+0x48;
        const float duration=read<float>(timer),elapsed=read<float>(timer+4);
        dualsp::validateReload(duration,elapsed);
        // 负值仍归原生生命周期处理；保留至少一次跨过完成点的原生更新。
        if(duration>1 && elapsed>=0 && elapsed<duration-1)write<float>(timer+4,duration-1);
    }
}
void debugWeaponsTick(Addr activator) noexcept {
    if(!debugFastReload)return;
    try {if(debugWeaponsLive(activator))debugBoostReload(activator);}
    catch(const std::exception& error){debugWeaponsFailure(error.what());}
}
void debugConsume(void* owner,unsigned char kind,int index,void* weapon,bool consume) {
    Addr group=0;int ammo=0;
    try {
        if(kind==2 && debugInfiniteAmmo && debugWeaponsLive(debugWeapons.activator.address) &&
            read<Addr>(session.activator+0x260)==reinterpret_cast<Addr>(owner)) {
            group=groupOf(session.activator,true);ammo=read<int>(group+8);
            if(ammo>0)write<int>(group+8,-1);else group=0;
        }
    }catch(const std::exception& error){debugWeaponsFailure(error.what());}
    // 原生负库存分支跳过扣减，同时正常维护发射槽及装填，不把最后一发标成耗尽。
    try {originalDebugConsume(owner,kind,index,weapon,consume);}
    catch(...) {if(group)write<int>(group+8,ammo);throw;}
    if(group)write<int>(group+8,ammo);
}
void installDebugWeapons() {
    static bool installed=false;
    if(installed)return;
    verifyVersion();const auto base=continuousBase();
    constexpr unsigned char entry[]={0x40,0x53,0x57,0x41,0x54,0x41,0x55,0x41,0x56,0x48,0x81,0xec,0x80,0,0,0};
    constexpr unsigned char decrement[]={0xff,0x4b,0x08,0xff,0x4b,0x0c};
    require(!std::memcmp(reinterpret_cast<void*>(base+0x89f360),entry,sizeof(entry)) &&
        !std::memcmp(reinterpret_cast<void*>(base+0x89f516),decrement,sizeof(decrement)),"SP调试原生接口与当前构建不符");
    installGunSalvo();
    const auto target=reinterpret_cast<void*>(base+0x89f360);
    require(MH_CreateHook(target,reinterpret_cast<void*>(&debugConsume),reinterpret_cast<void**>(&originalDebugConsume))==MH_OK,"SP调试钩子创建失败");
    if(MH_EnableHook(target)!=MH_OK) {MH_RemoveHook(target);throw std::runtime_error("SP调试钩子启用失败");}
    installed=true;
}
void debugSetFastReload(bool enabled,double time) {
    if(enabled==debugFastReload)return;
    for(int index=0;index<session.count;++index) {
        auto& group=session.special[index];
        if(index==session.active || !group.valid)continue;
        require(time+0.001>=group.capturedTime,"Game time rewound; checkpoint restoration required");
        const int next=scalarInt(group,12),window=scalarInt(group,8),count=static_cast<int>(group.slots.size());
        require(count>0 && next>=0 && next<count && window>=0 && window<=count,"Invalid cached launch cursor");
        const double delta=std::max(0.0,time-group.capturedTime);
        for(int i=0;i<window;++i) {
            auto& slot=group.slots[(next+i)%count];slot.elapsed=cachedReload(group,slot,delta,true);
        }
        group.capturedTime=time;
    }
    debugFastReload=enabled;
}
int debugWeaponsLua(void* luaState) {
    const dualsp::LuaApi* api=nullptr;
    try {
        static const dualsp::LuaApi bound;api=&bound;
        require(api->top(luaState)==6,"SP调试参数数量无效");
        const Addr plane=api->readInteger(luaState,1),activator=api->readInteger(luaState,2);
        const auto allowed=api->readInteger(luaState,3),infinite=api->readInteger(luaState,4),fast=api->readInteger(luaState,5);
        const double time=api->readNumber(luaState,6);
        require((allowed==0 || allowed==1) && (infinite==0 || infinite==1) && (fast==0 || fast==1) && std::isfinite(time) && time>=0,"SP调试参数无效");
        require(debugWeapons.error.empty(),debugWeapons.error.c_str());
        if(!allowed || (!infinite && !fast && !debugInfiniteAmmo && !debugFastReload))debugWeapons.allowed=false;
        else {
            require(runtimeEnabled && session.plane==plane && session.activator==activator && session.gameThread==GetCurrentThreadId(),"SP调试不属于当前玩家会话");
            const auto player=continuousIdentity(plane),component=continuousIdentity(activator);
            require(continuousAlive(player) && continuousAlive(component) && read<Addr>(plane+0x11a8)==activator && read<Addr>(activator+0x250)==plane,"SP调试玩家身份已改变");
            if(infinite || fast)installDebugWeapons();
            debugSetFastReload(fast!=0,time);debugInfiniteAmmo=infinite!=0;
            debugWeapons.plane=player;debugWeapons.activator=component;debugWeapons.thread=GetCurrentThreadId();
            debugWeapons.heartbeat=GetTickCount64();debugWeapons.allowed=true;
            debugBoostReload(activator);
        }
        api->boolean(luaState,true);api->string(luaState,"",0);return 2;
    }catch(const std::exception& error) {
        debugWeaponsFailure(error.what());
        if(api){api->boolean(luaState,false);api->string(luaState,error.what(),std::strlen(error.what()));return 2;}
        return 0;
    }
}
