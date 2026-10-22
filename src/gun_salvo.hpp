#pragma once
// 机枪与额外SP合并按住责任。只观察原生玩家动作，不读取固定手柄键。
// 本头文件位于核心私有命名空间，复用会话、弱身份及构建检查。
struct GunSalvo {
    ContinuousIdentity plane,activator,weapon;
    DWORD thread{};
    bool allowed{},gun{},main{},extra{},blocked{};
    uint64_t press{};
    ULONGLONG heartbeat{};
} gunSalvo;
bool salvoNeedRelease=false; // 旧会话仍按住时，新上下文不能继承为一次新按下。
using SalvoAction=void(*)(void*,bool);
using SalvoTick=void(*)(void*,float);
SalvoAction originalSalvoGun{},originalSalvoMain{};
SalvoTick originalSalvoTick{};

bool salvoOwner(Addr plane) {
    return gunSalvo.plane.address==plane && gunSalvo.thread==GetCurrentThreadId();
}
bool salvoIdentities() {
    return continuousAlive(gunSalvo.plane) && continuousAlive(gunSalvo.activator) &&
        read<Addr>(gunSalvo.plane.address+0x11a8)==gunSalvo.activator.address &&
        read<Addr>(gunSalvo.activator.address+0x250)==gunSalvo.plane.address;
}
bool salvoLive() {
    return gunSalvo.allowed && runtimeEnabled && GetTickCount64()-gunSalvo.heartbeat<=500 && targetGazeFocused();
}
void salvoStopExtra() {
    if(!gunSalvo.extra)return;
    if(gunSalvo.thread!=GetCurrentThreadId())return;
    if(!salvoIdentities()){gunSalvo.extra=false;return;}
    // 玩家仍按住SP时保留其原生责任；不通过ReleaseWeaponInput影响机枪和干扰弹。
    if(!gunSalvo.main)
        originalSalvoMain(reinterpret_cast<void*>(gunSalvo.plane.address),false);
    gunSalvo.extra=false;
}
void salvoFailure() noexcept {
    gunSalvo.allowed=false;gunSalvo.blocked=true;salvoNeedRelease=true;
    try {salvoStopExtra();}catch(...) {} // 保留归还责任，下次原生更新仍会尝试收尾。
}
void salvoGunAction(void* plane,bool down) {
    originalSalvoGun(plane,down); // 原生机枪始终执行一次。
    if(!salvoOwner(reinterpret_cast<Addr>(plane))) {
        // 停用期间当前玩家真正松键，也能解除旧按住的隔离；不必吞掉下次完整按键。
        if(!down && session.plane==reinterpret_cast<Addr>(plane) && session.gameThread==GetCurrentThreadId())salvoNeedRelease=false;
        return;
    }
    try {
        const bool was=gunSalvo.gun;gunSalvo.gun=down;
        if(!down){gunSalvo.blocked=false;salvoNeedRelease=false;salvoStopExtra();}
        else if(!salvoLive())gunSalvo.blocked=true;
        else if(!was && !gunSalvo.blocked)++gunSalvo.press;
    }catch(...) {salvoFailure();}
}
void salvoMainAction(void* plane,bool down) {
    if(!salvoOwner(reinterpret_cast<Addr>(plane))) {originalSalvoMain(plane,down);return;}
    try {
        const bool owned=gunSalvo.extra;gunSalvo.main=down;
        if(gunSalvo.extra && salvoLive() && salvoIdentities())return; // 合并后的按住状态仍为真，不重复排队或提前释放。
        salvoStopExtra();
        if(owned && !down)return; // 归还已经转发过这次合并后的释放。
    }catch(...) {salvoFailure();}
    originalSalvoMain(plane,down);
}
void salvoMainTick(void* activator,float delta) {
    if(gunSalvo.extra && gunSalvo.activator.address==reinterpret_cast<Addr>(activator) &&
        gunSalvo.thread==GetCurrentThreadId())try {
        if(!salvoLive() || !salvoIdentities() || !read<unsigned char>(gunSalvo.activator.address+0x2a1) ||
            !continuousAlive(gunSalvo.weapon) || read<Addr>(groupOf(gunSalvo.activator.address,true)+0x40)!=gunSalvo.weapon.address)salvoFailure();
    }catch(...) {salvoFailure();}
    debugWeaponsTick(reinterpret_cast<Addr>(activator)); // 在原生装填前修正当前窗口，弹体仍由原生Tick创建。
    originalSalvoTick(activator,delta); // 心跳中断时也先停自有输入，不留下永久连射。
}
void installGunSalvo() {
    static bool installed=false;
    if(installed)return;
    verifyVersion();const auto base=continuousBase();
    constexpr unsigned char gun[]={0x48,0x8b,0x89,0xa8,0x11,0,0,0x48,0x85,0xc9,0x74,0x0d,0x84,0xd2,0x0f,0x85};
    constexpr unsigned char main[]={0x40,0x53,0x48,0x83,0xec,0x20,0x48,0x8b,0xd9,0x48,0x8b,0x89,0xa8,0x11,0,0};
    constexpr unsigned char tick[]={0x40,0x55,0x57,0x48,0x8d,0x6c,0x24,0xb1,0x48,0x81,0xec,0x98,0,0,0};
    auto verify=[base](Addr offset,const auto& bytes){checkMemory(base+offset,sizeof(bytes));
        require(!std::memcmp(reinterpret_cast<void*>(base+offset),bytes,sizeof(bytes)),"机炮齐射原生接口与当前构建不符");};
    verify(0x8238a0,gun);verify(0x8236d0,main);verify(0x889c90,tick);
    const auto status=MH_Initialize();require(status==MH_OK || status==MH_ERROR_ALREADY_INITIALIZED,"机炮齐射钩子初始化失败");
    const Addr entries[]={base+0x8238a0,base+0x8236d0,base+0x889c90};
    void* hooks[]={reinterpret_cast<void*>(&salvoGunAction),reinterpret_cast<void*>(&salvoMainAction),reinterpret_cast<void*>(&salvoMainTick)};
    void** originals[]={reinterpret_cast<void**>(&originalSalvoGun),reinterpret_cast<void**>(&originalSalvoMain),reinterpret_cast<void**>(&originalSalvoTick)};
    int created=0;
    try {
        for(int i=0;i<3;++i){require(MH_CreateHook(reinterpret_cast<void*>(entries[i]),hooks[i],originals[i])==MH_OK,"机炮齐射钩子创建失败");++created;}
        for(const auto entry:entries)require(MH_QueueEnableHook(reinterpret_cast<void*>(entry))==MH_OK,"机炮齐射钩子排队失败");
        require(MH_ApplyQueued()==MH_OK,"机炮齐射钩子启用失败");
        installed=true;
    }catch(...) {
        for(int i=0;i<created;++i){MH_DisableHook(reinterpret_cast<void*>(entries[i]));MH_RemoveHook(reinterpret_cast<void*>(entries[i]));}
        throw;
    }
}
bool salvoCurrentSupported() {
    const auto weapon=read<Addr>(groupOf(gunSalvo.activator.address,true)+0x40);
    const auto identity=continuousIdentity(weapon);
    const auto name=mountNameIndex(read<unsigned>(weapon+0x18));
    const auto id=read<unsigned char>(weapon+0x3b0);
    if(!continuousAlive(identity) || read<unsigned char>(weapon+0x451)!=1)return false;
    // 与Lua保持同一精确类/编号清单，不按名称前缀或编号单独放行。
    static constexpr struct {const char* name;unsigned char id;} supported[]={
        {"Default__plwp_cgp_f15smt_C",17},{"Default__plwp_gplsl_arsb_C",22},
        {"Default__plwp_gun_adf11_C",0},{"Default__plwp_hgn_adf11_C",0},{"Default__plwp_hgp_x0_C",17},
        {"Default__plwp_mgp_31m_C",17},{"Default__plwp_mgp_31ma_C",17},{"Default__plwp_mgp_C",17},
        {"Default__plwp_mgp_active_C",17},{"Default__plwp_mgp_fa36_C",17},{"Default__plwp_mgp_qh66_C",17},
        {"Default__plwp_mgp_r0_C",17},{"Default__plwp_mgp_rmq4_brc_C",17},{"Default__plwp_mgp_uavcx_C",0},
        {"Default__plwp_mgp_va_C",17},{"Default__plwp_mgp_vb_C",17},
        {"Default__plwp_plsl_8m_C",22},{"Default__plwp_plsl_qh66_C",32},
        {"Default__plwp_plsl_rmq4_C",32},{"Default__plwp_plsl_ua45_C",32},
        {"Default__plwp_plsl_x0_C",22},{"Default__plwp_plsl_x1_C",22},
        {"Default__plwp_plsl_x308_C",22},{"Default__plwp_plsl_xr302_C",22},{"Default__plwp_rgun_zp_C",17}
    };
    for(const auto& item:supported)if(id==item.id && name==item.name)return true;
    return false;
}
// action=-1只读状态，0归还额外输入，1按住当前合格SP；resume仅供本次自动切换后恢复真实机枪按住。
int gunSalvoLua(void* luaState) {
    const dualsp::LuaApi* api=nullptr;
    try {
        static const dualsp::LuaApi bound;api=&bound;
        require(api->top(luaState)==5,"机炮齐射参数数量无效");
        const Addr plane=api->readInteger(luaState,1),activator=api->readInteger(luaState,2);
        const auto allowed=api->readInteger(luaState,3),action=api->readInteger(luaState,4),resume=api->readInteger(luaState,5);
        require((allowed==0 || allowed==1) && action>=-1 && action<=1 && (resume==0 || resume==1),"机炮齐射参数无效");
        if(!plane || !activator) {
            salvoNeedRelease=salvoNeedRelease || gunSalvo.gun;
            salvoStopExtra();gunSalvo={};
        }else {
            require(session.plane==plane && session.activator==activator && session.gameThread==GetCurrentThreadId(),"机炮齐射不属于当前玩家会话");
            if(!salvoOwner(plane) || gunSalvo.activator.address!=activator || !salvoIdentities()) {
                salvoNeedRelease=salvoNeedRelease || gunSalvo.gun;salvoStopExtra();gunSalvo={};
                if(allowed) {
                    installGunSalvo();gunSalvo.plane=continuousIdentity(plane);gunSalvo.activator=continuousIdentity(activator);
                    gunSalvo.thread=GetCurrentThreadId();gunSalvo.gun=read<unsigned char>(activator+0x2a4)!=0;
                    gunSalvo.main=read<unsigned char>(activator+0x2a5)!=0;gunSalvo.blocked=salvoNeedRelease || gunSalvo.gun || gunSalvo.main;
                }
            }
            if(gunSalvo.plane.address) {
                const bool stale=gunSalvo.heartbeat && GetTickCount64()-gunSalvo.heartbeat>500;
                gunSalvo.allowed=allowed && targetGazeFocused();gunSalvo.heartbeat=GetTickCount64();
                if(stale || !gunSalvo.allowed) {
                    gunSalvo.blocked=gunSalvo.blocked || gunSalvo.gun;
                    gunSalvo.main=gunSalvo.main && read<unsigned char>(activator+0x2a5)!=0;
                    salvoStopExtra();
                }
                if(action==0)salvoStopExtra();
                if(resume && salvoLive() && gunSalvo.gun && !gunSalvo.blocked && !read<unsigned char>(activator+0x2a4))
                    originalSalvoGun(reinterpret_cast<void*>(plane),true);
                if(action==1 && salvoLive() && gunSalvo.gun && !gunSalvo.blocked && !gunSalvo.extra && read<unsigned char>(activator+0x2a4)) {
                    require(read<unsigned char>(activator+0x2a1) && salvoCurrentSupported(),"当前SP不支持机炮齐射");
                    const int ammo=read<int>(groupOf(activator,true)+8);
                    require(dualsp::validAmmoCount(ammo),"机炮齐射：弹药数量无效");
                    if(ammo!=0) { // 负数无限库存也可发射，实际节奏仍由原生处理。
                        gunSalvo.weapon=continuousIdentity(read<Addr>(groupOf(activator,true)+0x40));
                        if(!gunSalvo.main)originalSalvoMain(reinterpret_cast<void*>(plane),true);
                        gunSalvo.extra=read<unsigned char>(activator+0x2a5)!=0; // 原生拒绝时不冒充已接管。
                    }
                }
            }
        }
        api->boolean(luaState,true);api->boolean(luaState,gunSalvo.gun && !gunSalvo.blocked);
        const auto press=std::to_string(gunSalvo.press);
        api->boolean(luaState,gunSalvo.main);api->string(luaState,press.data(),press.size());api->string(luaState,"",0);return 5;
    }catch(const std::exception& error) {
        salvoFailure();nativeLog(std::string("GUN_SALVO_ERROR ")+error.what());
        if(api){api->boolean(luaState,false);api->boolean(luaState,false);api->boolean(luaState,false);api->string(luaState,"0",1);
            api->string(luaState,error.what(),std::strlen(error.what()));return 5;}return 0;
    }
}
