#pragma once
// 只接管已配置的玩家SP；复用原生扣弹和装填队列，不改武器蓝图。
struct DebugWeapons {
    ContinuousIdentity plane,activator;
    DWORD thread{};
    ULONGLONG heartbeat{};
    bool allowed{};
    Addr repairPool{};int repairCursor{-1},repairAmmo{},repairWindow{}; // 无限弹药单独开启时仅在队列变更后检查窗口。
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
bool debugQueueNeedsRepair() {
    return session.active>=0 && session.active<session.count && session.special[session.active].debugRecycled;
}
void debugBoostReload(Addr activator) {
    const bool recycle=debugInfiniteAmmo || debugQueueNeedsRepair();
    if(!debugFastReload && !recycle)return;
    const Addr group=groupOf(activator,true);
    if(!debugReloadEligible(group))return;
    const int ammo=read<int>(group+8),window=read<int>(group+0x10),next=read<int>(group+0x14);
    const Addr pool=read<Addr>(group+0x58);
    if(!debugFastReload && pool==debugWeapons.repairPool && next==debugWeapons.repairCursor && ammo==debugWeapons.repairAmmo && window==debugWeapons.repairWindow)return;
    const auto slots=weaponSlotArray(group); // 一次验证完整数组，窗口内直接读写，避免每槽查页。
    int awakened=0;
    for(int i=0;i<window;++i) {
        const Addr timer=slots.data+static_cast<size_t>((next+i)%slots.count)*0x60+0x48;
        std::array<float,2> values{};std::memcpy(values.data(),reinterpret_cast<void*>(timer),sizeof(values));
        const float duration=values[0];float elapsed=values[1];
        dualsp::validateReload(duration,elapsed);
        // 只激活当前装填窗口里的精确-1；不预装整池，不改-2待发状态或负延迟。
        if(recycle && ammo!=0 && elapsed==-1.f){elapsed=0;++awakened;}
        if(debugFastReload && duration>1 && elapsed>=0 && elapsed<duration-1)elapsed=duration-1;
        if(elapsed!=values[1])std::memcpy(reinterpret_cast<void*>(timer+4),&elapsed,sizeof(elapsed));
    }
    if(awakened)nativeLog(Json{{"event","DEBUG_RELOAD_POOL_ACTIVATED"},{"slot",session.active},
        {"cursor",next},{"count",awakened},{"pool",slots.count}}.dump());
    debugWeapons.repairPool=pool;debugWeapons.repairCursor=next;debugWeapons.repairAmmo=ammo;debugWeapons.repairWindow=window;
}
void debugWeaponsTick(Addr activator) noexcept {
    if(!debugInfiniteAmmo && !debugFastReload && !debugQueueNeedsRepair())return;
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
void debugSetOptions(bool infinite,bool fast,double time) {
    if(infinite==debugInfiniteAmmo && fast==debugFastReload)return;
    for(int index=0;index<session.count;++index) {
        auto& group=session.special[index];
        if(index==session.active || !group.valid)continue;
        settleCachedReload(group,time); // 与检查点共用旧开关结算，防止追溯加速。
    }
    debugFastReload=fast;debugInfiniteAmmo=infinite;debugWeapons.repairPool=0;
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
        if(!allowed || (!infinite && !fast && !debugInfiniteAmmo && !debugFastReload &&
            !std::any_of(session.special.begin(),session.special.end(),[](const Group& group){return group.debugRecycled;})))debugWeapons.allowed=false;
        else {
            require(runtimeEnabled && session.plane==plane && session.activator==activator && session.gameThread==GetCurrentThreadId(),"SP调试不属于当前玩家会话");
            const auto player=continuousIdentity(plane),component=continuousIdentity(activator);
            require(continuousAlive(player) && continuousAlive(component) && read<Addr>(plane+0x11a8)==activator && read<Addr>(activator+0x250)==plane,"SP调试玩家身份已改变");
            if(infinite || fast)installDebugWeapons();
            debugSetOptions(infinite!=0,fast!=0,time);
            // 检查点/换装可重建Group而开关保持不变；每次有效续约同步三槽资格。
            if(infinite)for(int index=0;index<session.count;++index)
                if(session.special[index].fastReloadEligible)session.special[index].debugRecycled=true;
            debugWeapons.plane=player;debugWeapons.activator=component;debugWeapons.thread=GetCurrentThreadId();
            debugWeapons.heartbeat=GetTickCount64();debugWeapons.allowed=true;
            // 原生Tick在下一次装填更新前处理窗口；心跳不重复扫描物理槽。
        }
        api->boolean(luaState,true);api->string(luaState,"",0);return 2;
    }catch(const std::exception& error) {
        debugWeaponsFailure(error.what());
        if(api){api->boolean(luaState,false);api->string(luaState,error.what(),std::strlen(error.what()));return 2;}
        return 0;
    }
}
