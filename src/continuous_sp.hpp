// 电子战和UAV按原生实现识别，TLS保留既有附着范围；仅限 build 9855922。
#pragma once

struct ContinuousIdentity { Addr address{}; int index{},serial{}; };
enum class ContinuousKind { None, Electronic, TLS, UAV };
struct ContinuousSupport {
    ContinuousKind kind{ContinuousKind::None};
    bool electronic{};
    const char* reason{"not_registered"};
    bool uav{}; // 按真实WeaponUAV类识别，不以菜单名称或编号26代替。
};
struct ContinuousEffect {
    ContinuousIdentity actor,plane,activator,level;
    Addr weaponClass{};
    ContinuousKind kind{ContinuousKind::None}; // 真实实现类型；Mod可把电子战兵装改为其他WeaponID。
    int source{};
    float age{};
    uint64_t retirement{}; // 原生退场标记按位比较，不推断其数值类型。
    bool released{},finalNotified{};
    std::vector<ContinuousIdentity> attachment;
    uint64_t socket{};
};
std::vector<ContinuousEffect> continuousEffects; // 弱身份；强引用由原生数组持有。
using ContinuousNotify=void(*)(void*,void*);
ContinuousNotify continuousOriginalRelease{},continuousOriginalRetire{};
using ContinuousEnd=void(*)(void*);
ContinuousEnd continuousOriginalUavEnd{};
bool continuousUavHookReady=false;
std::string continuousUavHookFailure;
bool continuousHooksReady=false;
std::string continuousHookFailure;
std::string continuousRuntimeFailure; // 回调错误留给下一次 HUD/命令读取，不能只写日志后继续运行。
bool continuousRebuildStarted=false; // 仅当前核心命令；拒绝调试或无效请求不能顺带结束仍在工作的效果。
std::atomic<DWORD> continuousThread{0};

Addr continuousBase() {return reinterpret_cast<Addr>(GetModuleHandleW(nullptr));}
// 仅用于不调用引擎、不写入的短块；PendingKill/Unreachable也每次读取，不缓存身份结论。
struct ContinuousReadBatch {
    NativeReadBatch memory;
    Addr entries{};int count{};
    ContinuousReadBatch() {
        const Addr header=continuousBase()+0x3bcfdb0;
        entries=memory.read<Addr>(header);count=memory.read<int>(header+12);
        require(count>=0 && count<3000000,"持续兵装：对象表长度无效");
    }
    template<class T> T read(Addr address) const {return memory.read<T>(address);}
    Addr entry(const ContinuousIdentity& id) const {
        if(!id.address || id.index<0 || id.index>=count)return 0;
        const Addr item=entries+static_cast<Addr>(id.index)*24;
        return read<Addr>(item)==id.address && read<int>(item+16)==id.serial?item:0;
    }
    bool alive(const ContinuousIdentity& id) const {
        const Addr item=entry(id);
        return item && !(read<unsigned>(item+8)&0x30000000u);
    }
    ContinuousIdentity identity(Addr object) const {
        require(object!=0,"持续兵装：对象为空");
        const int index=read<int>(object+0xc);
        require(index>=0 && index<count,"持续兵装：对象索引越界");
        const Addr item=entries+static_cast<Addr>(index)*24;
        ContinuousIdentity id{object,index,read<int>(item+16)};
        require(alive(id),"持续兵装：对象身份已失效");return id;
    }
};
Addr continuousEntry(const ContinuousIdentity& id) {return ContinuousReadBatch{}.entry(id);}
bool continuousAlive(const ContinuousIdentity& id) {
    return ContinuousReadBatch{}.alive(id);
}
ContinuousIdentity continuousIdentity(Addr object) {
    return ContinuousReadBatch{}.identity(object);
}
std::string continuousName(Addr object,const NativeReadBatch& memory) {
    const unsigned index=memory.read<unsigned>(object+0x18);
    require(index<4000000,"持续兵装：名称索引越界");
    const Addr names=memory.read<Addr>(continuousBase()+0x3bc7070);
    const Addr chunk=memory.read<Addr>(names+(index/0x4000)*8);
    const Addr entry=memory.read<Addr>(chunk+(index%0x4000)*8);
    require(!(memory.read<unsigned char>(entry)&1),"持续兵装：资源名编码不符");
    return readBoundedName(entry+16,"持续兵装：资源名不可读或过长");
}
std::string continuousName(Addr object) {const NativeReadBatch memory;return continuousName(object,memory);}
bool continuousClass(Addr cls,const char* name,const char* package,const NativeReadBatch& memory) {
    return continuousName(cls,memory)==name && continuousName(memory.read<Addr>(cls+0x20),memory)==package;
}
bool continuousClass(Addr cls,const char* name,const char* package) {
    const NativeReadBatch memory;return continuousClass(cls,name,package,memory);
}
// 接受直接继承原生类和任意合法包中的蓝图派生，不按菜单简称、文件名或机型放行。
bool continuousClassDerived(Addr cls,const char* name,const char* package) {
    const NativeReadBatch memory;
    for(int i=0;cls && i<64;++i,cls=memory.read<Addr>(cls+0x30))
        if(continuousClass(cls,name,package,memory))return true;
    require(!cls,"持续兵装：类继承链无效或过长");
    return false;
}
bool continuousElectronicClass(Addr cls) {return continuousClassDerived(cls,"ElectronicSupportMeasures","/Script/Nimbus");}
bool continuousUavClass(Addr cls) {return continuousClassDerived(cls,"WeaponUAV","/Script/Nimbus");}
bool continuousPlaneClass(Addr plane,const char* name,const char* package) {
    const ContinuousReadBatch memory;memory.identity(plane);
    Addr cls=memory.read<Addr>(plane+0x10);
    for(int i=0;cls && i<32;++i,cls=memory.read<Addr>(cls+0x30))
        if(continuousClass(cls,name,package,memory.memory))return true;
    return false;
}
bool continuousMorgan(Addr plane) {
    return continuousPlaneClass(plane,"AcePlayerPawn_mrgn_C","/Game/Blueprint/Player/Pawn/AcePlayerPawn_mrgn");
}
bool continuousRaven(Addr plane) {
    return continuousPlaneClass(plane,"AcePlayerPawn_adf11f_C","/Game/Blueprint/Player/Pawn/AcePlayerPawn_adf11f");
}
bool continuousRavenLoadout(Addr activator) {
    const std::array<const char*,3> names{"plwp_tls_x3_C","plwp_qaam_a0_C","plwp_WeaponUAV_C"};
    const std::array<const char*,3> packages{"/Game/Blueprint/Weapons/Player/Variation/plwp_tls_x3",
        "/Game/Blueprint/Weapons/Player/Base/plwp_qaam_a0","/Game/Blueprint/Weapons/Player/Base/plwp_WeaponUAV"};
    for(int i=0;i<3;++i) {
        const Addr cls=read<Addr>(activator+0x158+i*0x30); // PlayerSpecialWeaponConfig.WeaponClass。
        if(!cls || !continuousClass(cls,names[i],packages[i]))return false;
        if(i<2 && continuousElectronicClass(cls))return false;
        if(i==2 && !continuousClassDerived(cls,"WeaponUAV","/Script/Nimbus"))return false;
    }
    return true;
}
bool continuousXfa27(Addr plane) {
    return continuousPlaneClass(plane,"AcePlayerPawn_fa27_C","/Game/Blueprint/Player/Pawn/AcePlayerPawn_fa27");
}
bool continuousCfa44(Addr plane) {
    // 切换等待会重复查询，只缓存机体弱身份与机型，不保留发射状态。
    static ContinuousIdentity cached;
    static bool matched=false;
    if(cached.address==plane && continuousAlive(cached))return matched;
    const auto current=continuousIdentity(plane);
    const bool cfa=continuousPlaneClass(plane,"AcePlayerPawn_fa44_C","/Game/Blueprint/Player/Pawn/AcePlayerPawn_fa44");
    cached=current;matched=cfa;return matched;
}
bool continuousSu37(Addr plane) {
    return continuousPlaneClass(plane,"AcePlayerPawn_su37_C","/Game/Blueprint/Player/Pawn/AcePlayerPawn_su37");
}
// 只缓存机体弱身份与机型结论；不缓存发射体、组件或反射字段。
bool continuousFalken(Addr plane) {
    static ContinuousIdentity cached;
    static bool matched=false;
    if(cached.address==plane && continuousAlive(cached))return matched;
    const auto current=continuousIdentity(plane);
    const bool falken=continuousPlaneClass(plane,"AcePlayerPawn_zoef_C","/Game/Blueprint/Player/Pawn/AcePlayerPawn_zoef");
    cached=current;matched=falken;return matched;
}
// 发射结束请求不等于尾迹已经退场；两组原生引用中的存活TLS都须自然结束。
// 仅切换请求/等待时读取，不扫描全局对象、不调用End或组件显隐，也不依赖外观开关。
bool continuousFalkenTLSBusy(Addr plane,Addr activator) {
    if(!continuousFalken(plane))return false;
    for(const Addr header:{activator+0x338,activator+0x2e8}) {
        const Addr data=read<Addr>(header);
        const int count=read<int>(header+8),capacity=read<int>(header+12);
        require(count>=0 && count<=capacity && capacity<=65536,"Falken TLS等待：原生武器数组无效");
        if(count)checkMemory(data,static_cast<size_t>(count)*sizeof(Addr));
        for(int i=0;i<count;++i) {
            const Addr weapon=read<Addr>(data+i*sizeof(Addr));
            if(weapon && read<unsigned char>(weapon+0x3b0)==25 && read<Addr>(weapon+0x3b8)==plane &&
                read<unsigned char>(weapon+0x368) && !continuousElectronicClass(read<Addr>(weapon+0x10)))return true;
        }
    }
    return false;
}
bool continuousLegacyPlane(Addr plane) {return continuousCfa44(plane) || continuousMorgan(plane) || continuousRaven(plane) ||
    continuousXfa27(plane) || continuousSu37(plane);} // CFA已有弱身份机型缓存，先命中，避免先遍历另外三架的类链。
// 电子战/UAV均与WeaponID解耦；仍逐次核对原生回调和发射即装填契约。
ContinuousSupport continuousSupport(Addr plane,Addr weapon) {
    const Addr cls=read<Addr>(weapon+0x10),table=read<Addr>(weapon),base=continuousBase();
    const bool electronic=continuousElectronicClass(cls);
    if(electronic) {
        if(read<unsigned char>(weapon+0x458)!=1 || read<unsigned char>(weapon+0x459) || read<Addr>(weapon+0x5d8))
            return {ContinuousKind::None,true,"electronic_reload_contract_changed"};
        if(read<Addr>(table+0x648)!=base+0x894850 || read<Addr>(table+0x628)!=base+0x895fd0 ||
            read<Addr>(table+0x680)!=base+0x86bf90)
            return {ContinuousKind::None,true,"electronic_native_callbacks_changed"};
        return {ContinuousKind::Electronic,true,"native_electronic"};
    }
    if(continuousUavClass(cls)) {
        if(read<unsigned char>(weapon+0x458)!=1 || read<unsigned char>(weapon+0x459) || read<Addr>(weapon+0x5d8) ||
            read<unsigned char>(weapon+0x45d) || read<float>(weapon+0x518)!=0)
            return {ContinuousKind::None,false,"uav_reload_contract_changed",true};
        const bool supported=read<Addr>(table+0x618)==base+0x8a6bf0 && read<Addr>(table+0x628)==base+0x8779a0 &&
            read<Addr>(table+0x648)==base+0x8a5260 && read<Addr>(table+0x668)==base+0x878d40 && read<Addr>(table+0x680)==base+0x878eb0;
        return {supported?ContinuousKind::UAV:ContinuousKind::None,false,
            supported?"native_uav":"uav_native_callbacks_changed",true};
    }
    const int id=read<unsigned char>(weapon+0x3b0);
    if(continuousClassDerived(cls,"LaserInterceptor","/Script/Nimbus"))
        return {ContinuousKind::None,false,"interceptor_lifecycle_not_supported"};
    if(id!=25 && id!=26)return {ContinuousKind::None,false,id==31?"electronic_class_unrecognized":"not_continuous"};
    const bool raven=continuousRaven(plane),morgan=!raven && continuousMorgan(plane);
    const bool tls=id==25 && ((morgan && continuousClass(cls,"plwp_tls_x1_C","/Game/Blueprint/Weapons/Player/Variation/plwp_tls_x1")) ||
        (raven && continuousClass(cls,"plwp_tls_x3_C","/Game/Blueprint/Weapons/Player/Variation/plwp_tls_x3")) ||
        (continuousSu37(plane) && continuousClass(cls,"plwp_tls_x0_C","/Game/Blueprint/Weapons/Player/Base/plwp_tls_x0")));
    if(!tls || read<unsigned char>(weapon+0x458)!=1 || read<unsigned char>(weapon+0x459) || read<Addr>(weapon+0x5d8))return {};
    const bool supported=read<Addr>(table+0x648)==base+0x894850 && read<Addr>(table+0x628)==base+0x85f1c0 &&
        read<Addr>(table+0x680)==base+0x861490;
    return {supported?ContinuousKind::TLS:ContinuousKind::None,false,supported?"native_tls":"tls_contract_changed"};
}
bool continuousSupported(Addr plane,Addr weapon) {return continuousSupport(plane,weapon).kind!=ContinuousKind::None;}
struct ContinuousArray {Addr header{},data{};int count{},capacity{};};
ContinuousArray continuousArray(Addr header) {
    const NativeReadBatch memory;
    ContinuousArray a{header,memory.read<Addr>(header),memory.read<int>(header+8),memory.read<int>(header+12)};
    require(a.count>=0 && a.count<=a.capacity && a.capacity<=65536,"持续兵装：原生引用数组无效");
    if(a.capacity)checkMemory(a.data,static_cast<size_t>(a.capacity)*sizeof(Addr));
    return a;
}
bool continuousContains(const ContinuousArray& a,Addr actor) {
    const NativeReadBatch memory;
    for(int i=0;i<a.count;++i)if(memory.read<Addr>(a.data+i*sizeof(Addr))==actor)return true;
    return false;
}
// 加载器公开的 FMemory 接口最终调用游戏 GMalloc；不能用 CRT/std::vector 的内存替换 UPROPERTY 数组。
using ContinuousRealloc=void*(*)(void*,size_t,unsigned);
ContinuousRealloc continuousAllocator() {
    const auto module=GetModuleHandleW(L"UE4SS.dll");
    require(module!=nullptr,"持续兵装：加载器未就绪，无法准备引用容量");
    const auto ready=reinterpret_cast<Addr>(GetProcAddress(module,"?bVersionedContainerIsInitialized@StaticStorage@UnrealInitializer@Unreal@RC@@2_NA"));
    const auto allocator=reinterpret_cast<Addr>(GetProcAddress(module,"?GMalloc@Unreal@RC@@3PEAPEAVFMalloc@12@EA"));
    const auto realloc=reinterpret_cast<ContinuousRealloc>(GetProcAddress(module,"?Realloc@FMemory@Unreal@RC@@SAPEAXPEAX_KI@Z"));
    require(ready && allocator && realloc,"持续兵装：加载器缺少原生引用分配接口");
    require(read<unsigned char>(ready)==1 && read<Addr>(allocator) && read<Addr>(read<Addr>(allocator)),
        "持续兵装：游戏分配器尚未绑定");
    return realloc;
}
// 分配入口延迟取得；离线检查可验证移动/失败分配，运行时始终使用游戏分配器。
template<typename AllocatorProvider>
void continuousReserveUsing(Addr header,size_t required,const char* kind,AllocatorProvider allocatorProvider) {
    const auto before=continuousArray(header);
    require(required<=65536,"持续兵装：引用数量超过安全边界");
    if(required<=static_cast<size_t>(before.capacity))return;
    const int capacity=std::max(static_cast<int>(required),std::min(65536,before.capacity+std::max(4,before.capacity/2)));
    checkMemory(header,16); // 分配前完成头部检查；成功后立即公布新指针，不能再持有已释放的旧 data。
    const auto realloc=allocatorProvider();
    const Addr data=reinterpret_cast<Addr>(realloc(reinterpret_cast<void*>(before.data),static_cast<size_t>(capacity)*sizeof(Addr),0));
    require(data!=0,"持续兵装：游戏分配器未返回引用空间");
    std::memcpy(reinterpret_cast<void*>(header),&data,sizeof(data));
    std::memcpy(reinterpret_cast<void*>(header+12),&capacity,sizeof(capacity)); // Num 不变；新增元素尚未写入。
    const auto after=continuousArray(header);
    require(after.count==before.count && after.capacity>=static_cast<int>(required),"持续兵装：引用扩容后布局改变");
    nativeLog(Json{{"event","CONTINUOUS_REFERENCE_GROWN"},{"array",kind},{"count",before.count},
        {"old_capacity",before.capacity},{"required",required},{"new_capacity",after.capacity}}.dump());
}
void continuousReserve(Addr header,size_t required,const char* kind) {
    continuousReserveUsing(header,required,kind,continuousAllocator);
}
// UAV可仅由Active保有；并集去重后再逐项核对身份，不能复制两份旧数组头。
std::vector<Addr> continuousCandidates(const ContinuousArray& active,const ContinuousArray& spawned) {
    std::vector<Addr> result;
    const NativeReadBatch memory;
    for(const auto& array:{active,spawned})for(int i=0;i<array.count;++i) {
        const Addr object=memory.read<Addr>(array.data+i*sizeof(Addr));
        if(object && std::find(result.begin(),result.end(),object)==result.end())result.push_back(object);
    }
    return result;
}
void continuousRemove(Addr header,Addr actor) {
    const auto a=continuousArray(header);int count=0;
    for(int i=0;i<a.count;++i){const Addr item=read<Addr>(a.data+i*sizeof(Addr));
        if(item!=actor)write<Addr>(a.data+count++*sizeof(Addr),item);}
    for(int i=count;i<a.count;++i)write<Addr>(a.data+i*sizeof(Addr),0);
    write<int>(header+8,count); // 不释放或更换原生分配区。
}
bool continuousSameShot(const ContinuousEffect& e) {
    const ContinuousReadBatch memory;
    if(!memory.alive(e.actor) || !memory.alive(e.plane) || !memory.alive(e.activator) || !memory.alive(e.level))return false;
    const Addr w=e.actor.address;
    return memory.read<Addr>(e.plane.address+0x20)==e.level.address && memory.read<Addr>(e.plane.address+0x11a8)==e.activator.address &&
        memory.read<Addr>(w+0x3b8)==e.plane.address && memory.read<Addr>(w+0x10)==e.weaponClass &&
        memory.read<uint64_t>(w+0x370)==e.retirement && memory.read<float>(w+0x610)>=e.age && memory.read<unsigned char>(w+0x368)!=0;
}
void continuousSweep() {
    continuousEffects.erase(std::remove_if(continuousEffects.begin(),continuousEffects.end(),
        [](const auto& e){return !continuousSameShot(e);}),continuousEffects.end());
}
// 两次通知都只属于旧发射代次，即使用户切回同一逻辑槽也不能收尾新发射体。
// 不在通知内部删除记录：此时尾迹、虚表清理和活动位归零可能尚未完成。
void continuousNotification(void* activator,void* weapon,bool final) noexcept {
    auto original=final?continuousOriginalRetire:continuousOriginalRelease;
    if(GetCurrentThreadId()!=continuousThread.load()){original(activator,weapon);return;}
    for(auto& e:continuousEffects) {
        if(e.actor.address!=reinterpret_cast<Addr>(weapon) || e.activator.address!=reinterpret_cast<Addr>(activator))continue;
        try {
            if(!continuousSameShot(e))continue;
            if(final)e.finalNotified=true;
            else {continuousRemove(e.activator.address+0x338,e.actor.address);e.released=true;}
            nativeLog(Json{{"event",final?"CONTINUOUS_RETIRE_ISOLATED":"CONTINUOUS_RELEASE_ISOLATED"},
                {"source",e.source},{"index",e.actor.index},{"serial",e.actor.serial}}.dump());
        }catch(const std::exception& error){continuousRuntimeFailure=error.what();nativeLog(std::string("CONTINUOUS_CALLBACK_ERROR ")+error.what());}
        return; // 校验异常也不能退回会修改当前 SP 的通知；Actor 自身收尾仍继续。
    }
    original(activator,weapon);
}
void continuousRelease(void* activator,void* weapon){continuousNotification(activator,weapon,false);}
void continuousRetire(void* activator,void* weapon){continuousNotification(activator,weapon,true);}
// UAV 零尾迹直接到这里，先补足源对象 Release，再完整执行其导弹/武器父类收尾。
void continuousUavEnd(void* weapon) {
    if(GetCurrentThreadId()==continuousThread.load())for(auto& e:continuousEffects) {
        if(e.actor.address!=reinterpret_cast<Addr>(weapon))continue;
        try {if(continuousSameShot(e)) {
            continuousRemove(e.activator.address+0x338,e.actor.address);e.released=true;
            nativeLog(Json{{"event","CONTINUOUS_UAV_RELEASE_ISOLATED"},{"source",e.source},{"index",e.actor.index},{"serial",e.actor.serial}}.dump());
        }}
        catch(const std::exception& error){continuousRuntimeFailure=error.what();nativeLog(std::string("CONTINUOUS_UAV_END_ERROR ")+error.what());}
        break;
    }
    continuousOriginalUavEnd(weapon);
}
void continuousInstallUav() {
    require(continuousUavHookFailure.empty(),continuousUavHookFailure.c_str());
    if(continuousUavHookReady)return;
    const Addr base=continuousBase();
    constexpr unsigned char end[]={0x40,0x53,0x48,0x83,0xec,0x20,0x33,0xd2,0x48,0x8b,0xd9,0xe8,0x20,0x7f,0xff,0xff};
    constexpr unsigned char parent[]={0x40,0x53,0x48,0x83,0xec,0x20,0x48,0x8b,0xd9,0x33,0xd2,0x48,0x81,0xc1,0xe0,0x06,0,0};
    try {
        require(std::memcmp(reinterpret_cast<void*>(base+0x878eb0),end,sizeof(end))==0 &&
            std::memcmp(reinterpret_cast<void*>(base+0x871ca0),parent,sizeof(parent))==0,"持续兵装：UAV 收尾入口不匹配");
        require(MH_CreateHook(reinterpret_cast<void*>(base+0x878eb0),reinterpret_cast<void*>(&continuousUavEnd),
            reinterpret_cast<void**>(&continuousOriginalUavEnd))==MH_OK,"持续兵装：UAV 收尾接入失败");
        require(MH_EnableHook(reinterpret_cast<void*>(base+0x878eb0))==MH_OK,"持续兵装：UAV 收尾启用失败");
        continuousUavHookReady=true;
    }catch(const std::exception& error){MH_DisableHook(reinterpret_cast<void*>(base+0x878eb0));continuousUavHookFailure=error.what();throw;}
}
void continuousInstall() {
    require(continuousHookFailure.empty(),continuousHookFailure.c_str());
    if(continuousHooksReady){require(GetCurrentThreadId()==continuousThread.load(),"持续兵装：调用线程改变");return;}
    const Addr base=continuousBase();
    constexpr unsigned char release[]={0x48,0x85,0xd2,0x0f,0x84,0xbe,0x02,0,0,0x48,0x89,0x54,0x24,0x10};
    constexpr unsigned char retire[]={0x48,0x85,0xd2,0x0f,0x84,0x92,0,0,0,0x48,0x89,0x5c,0x24,0x08};
    constexpr unsigned char end[]={0x80,0xb9,0x68,0x03,0,0,0,0x74,0x0d,0x80,0xb9,0xc0,0x05,0,0,0,0x0f,0x84,0x2a,0x14,0,0,0xc3};
    try {
        require(std::memcmp(reinterpret_cast<void*>(base+0x885240),release,sizeof(release))==0 &&
            std::memcmp(reinterpret_cast<void*>(base+0x885510),retire,sizeof(retire))==0 &&
            std::memcmp(reinterpret_cast<void*>(base+0x894af0),end,sizeof(end))==0,"持续兵装：原生收尾入口不匹配");
        const auto status=MH_Initialize();
        require(status==MH_OK || status==MH_ERROR_ALREADY_INITIALIZED,"持续兵装：钩子初始化失败");
        require(MH_CreateHook(reinterpret_cast<void*>(base+0x885240),reinterpret_cast<void*>(&continuousRelease),
            reinterpret_cast<void**>(&continuousOriginalRelease))==MH_OK,"持续兵装：结束通知接入失败");
        require(MH_CreateHook(reinterpret_cast<void*>(base+0x885510),reinterpret_cast<void*>(&continuousRetire),
            reinterpret_cast<void**>(&continuousOriginalRetire))==MH_OK,"持续兵装：退场通知接入失败");
        require(MH_QueueEnableHook(reinterpret_cast<void*>(base+0x885240))==MH_OK &&
            MH_QueueEnableHook(reinterpret_cast<void*>(base+0x885510))==MH_OK && MH_ApplyQueued()==MH_OK,"持续兵装：通知隔离启用失败");
        continuousThread=GetCurrentThreadId();continuousHooksReady=true;
    }catch(const std::exception& error) {
        MH_DisableHook(reinterpret_cast<void*>(base+0x885240));MH_DisableHook(reinterpret_cast<void*>(base+0x885510));
        continuousHookFailure=error.what();throw;
    }
}
void continuousAttachment(ContinuousEffect& e) {
    if(e.kind!=ContinuousKind::TLS)return;
    Addr component=read<Addr>(e.actor.address+0x160);
    for(int i=0;component && i<8;++i) {
        e.attachment.push_back(continuousIdentity(component));component=read<Addr>(component+0x100);
    }
    require(!component && e.attachment.size()==4,"持续兵装：TLS 附着层级改变");
    const char* expected=continuousSu37(e.plane.address)?"SWP3":"SWP1";
    require(read<Addr>(e.actor.address+0x640)==e.attachment[1].address &&
        continuousName(e.attachment[1].address)==expected && continuousName(e.attachment[2].address)=="WeaponMeshBase" &&
        e.attachment[3].address==read<Addr>(e.plane.address+0x160),"持续兵装：TLS 原生挂架附着改变");
    e.socket=read<uint64_t>(e.actor.address+0x648);
    require(e.socket==read<uint64_t>(e.attachment[0].address+0x108),"持续兵装：TLS Socket 不一致");
}
bool continuousAttachmentIntact(const ContinuousEffect& e) {
    if(e.attachment.empty() || e.finalNotified)return true;
    if(read<Addr>(e.actor.address+0x160)!=e.attachment.front().address ||
        read<Addr>(e.actor.address+0x640)!=e.attachment[1].address || read<uint64_t>(e.actor.address+0x648)!=e.socket)return false;
    for(size_t i=0;i<e.attachment.size();++i)
        if(!continuousAlive(e.attachment[i]) || read<Addr>(e.attachment[i].address+0x100)!=(i+1<e.attachment.size()?e.attachment[i+1].address:0))return false;
    return read<uint64_t>(e.attachment[0].address+0x108)==e.socket;
}
// 完整原生结束 + 最终退场，用于补给/检查点/重建失败。不能把两秒尾迹丢在无引用区间。
void continuousFinish(ContinuousEffect& e) {
    if(!continuousSameShot(e))return;
    require(continuousSupported(e.plane.address,e.actor.address),"持续兵装：回退收尾接口改变");
    using End=void(*)(void*);
    reinterpret_cast<End>(continuousBase()+0x894af0)(reinterpret_cast<void*>(e.actor.address));
    if(continuousSameShot(e))reinterpret_cast<End>(read<Addr>(read<Addr>(e.actor.address)+0x680))(reinterpret_cast<void*>(e.actor.address));
    require(!continuousAlive(e.actor) || !read<unsigned char>(e.actor.address+0x368),"持续兵装：回退后对象未退场");
}
void continuousDrain(Addr plane,Addr activator);

// RootSet 仅覆盖同步重建的引用空窗。正常帧仍由原生 UPROPERTY 数组保有对象。
// FUObjectItem::Flags 是 UE4.18 的内部标志；绝不写 UObject::ObjectFlags。
struct ContinuousRoot {
    ContinuousIdentity id;bool owned{};
    explicit ContinuousRoot(ContinuousIdentity identity):id(identity) {
        const Addr entry=continuousEntry(id);require(entry && continuousAlive(id),"持续兵装：无法取得临时保有");
        const LONG prior=InterlockedOr(reinterpret_cast<volatile LONG*>(entry+8),0x40000000L);
        owned=(prior&0x40000000L)==0;
    }
    ContinuousRoot(const ContinuousRoot&)=delete;
    ContinuousRoot& operator=(const ContinuousRoot&)=delete;
    ContinuousRoot(ContinuousRoot&& other) noexcept:id(other.id),owned(other.owned){other.owned=false;}
    ~ContinuousRoot() noexcept {
        if(!owned)return;
        try{const Addr entry=continuousEntry(id);if(entry)InterlockedAnd(reinterpret_cast<volatile LONG*>(entry+8),~0x40000000L);}
        catch(const std::exception& error){continuousRuntimeFailure=error.what();nativeLog(std::string("CONTINUOUS_ROOT_RELEASE_ERROR ")+error.what());}
    }
};
struct ContinuousRebuild {
    Addr plane{},activator{};
    std::vector<ContinuousRoot> roots;
    bool committed=false;
    bool rebuilding=false;
    explicit ContinuousRebuild(const Command& command,bool strict=true) {
        if(command.op!="switch")return;
        plane=command.plane;activator=command.activator;
        require(read<Addr>(plane+0x11a8)==activator,"持续兵装：激活器不属于当前机体");
        continuousSweep();
        const bool legacy=continuousLegacyPlane(plane);
        const auto active=continuousArray(activator+0x338),spawned=continuousArray(activator+0x2e8);
        // 活动数组已移除的尾迹，从现有弱身份记录保留，不恢复历史已退场对象。
        std::vector<ContinuousEffect> additions;
        const auto candidates=continuousCandidates(active,spawned);
        for(const Addr w:candidates) {
            if(!w || read<Addr>(w+0x3b8)!=plane || !read<unsigned char>(w+0x368))continue;
            const int id=read<unsigned char>(w+0x3b0);
            const auto support=continuousSupport(plane,w);
            if(!support.electronic && !support.uav && (!legacy || (id!=25 && id!=26 && id!=31)))continue;
            if(support.kind==ContinuousKind::None) {
                if(strict)nativeLog(Json{{"event","CONTINUOUS_UNSUPPORTED"},{"weapon",continuousName(w)},
                    {"weapon_id",id},{"electronic",support.electronic},{"reason",support.reason}}.dump());
                require(!strict,"持续兵装：当前兵装的原生回调或装填机制尚不支持，已拒绝本次换槽");continue;
            }
            if(std::any_of(continuousEffects.begin(),continuousEffects.end(),[w](const auto& e){return e.actor.address==w;}))continue;
            require(read<unsigned char>(w+0x5c0) || continuousContains(active,w),"持续兵装：发射对象缺少活动登记");
            ContinuousEffect e;e.actor=continuousIdentity(w);e.plane=continuousIdentity(plane);e.activator=continuousIdentity(activator);
            e.level=continuousIdentity(read<Addr>(plane+0x20));e.weaponClass=read<Addr>(w+0x10);e.kind=support.kind;e.source=-1;
            // 同一真实类可被多个配置槽复用，先记录当前槽；电子战附加效果也可不属于选中的三SP。
            if(session.plane==plane && session.activator==activator) {
                if(session.active>=0 && session.active<session.count && session.special[session.active].valid &&
                    session.special[session.active].weaponClass==e.weaponClass)e.source=session.active;
                else for(int slot=0;slot<session.count;++slot)if(session.special[slot].valid && session.special[slot].weaponClass==e.weaponClass)e.source=slot;
            }
            require(e.source>=0 || e.kind==ContinuousKind::Electronic,"持续兵装：找不到发射源逻辑槽");
            e.age=read<float>(w+0x610);e.retirement=read<uint64_t>(w+0x370);e.released=read<unsigned char>(w+0x5c0)!=0;
            require(std::isfinite(e.age) && e.age>=0,"持续兵装：对象时间无效");continuousAttachment(e);additions.push_back(std::move(e));
        }
        require(continuousEffects.size()+additions.size()<=64,"持续兵装：待退场对象过多");
        const bool retained=std::any_of(continuousEffects.begin(),continuousEffects.end(),[this](const auto& e){
            return e.plane.address==plane && e.activator.address==activator;});
        if(additions.empty() && !retained){plane=activator=0;return;}
        continuousInstall();
        if(std::any_of(additions.begin(),additions.end(),[](const auto& e){return e.kind==ContinuousKind::UAV;}) ||
            std::any_of(continuousEffects.begin(),continuousEffects.end(),[this](const auto& e){
                return e.plane.address==plane && e.activator.address==activator && e.kind==ContinuousKind::UAV;}))continuousInstallUav();
        // 有在途效果时先核对分配后端；不能等原生重建已清空引用后才发现接口缺失。
        continuousAllocator();
        continuousEffects.reserve(continuousEffects.size()+additions.size());
        roots.reserve(continuousEffects.size()+additions.size());
        // 先完成所有可失败的校验/保有，再登记新对象，避免构造失败改变现有收尾政策。
        for(const auto& e:continuousEffects)if(e.plane.address==plane && e.activator.address==activator) {
            require(continuousAttachmentIntact(e),"持续兵装：已有 TLS 附着改变");roots.emplace_back(e.actor);
        }
        for(const auto& e:additions)roots.emplace_back(e.actor);
        for(auto& e:additions){nativeLog(Json{{"event","CONTINUOUS_CAPTURED"},{"index",e.actor.index},{"serial",e.actor.serial},
            {"source",e.source},{"electronic",e.kind==ContinuousKind::Electronic},
            {"age",e.age},{"tail",e.released},{"attachment_count",e.attachment.size()}}.dump());
            continuousEffects.push_back(std::move(e));}
    }
    void restoreReferences() {
        if(!plane){committed=true;return;}
        auto active=continuousArray(activator+0x338),spawned=continuousArray(activator+0x2e8);
        std::vector<Addr> addActive,addSpawned;
        for(auto& e:continuousEffects)if(e.plane.address==plane && e.activator.address==activator) {
            require(continuousSameShot(e),"持续兵装：重建改变了活动对象身份或发射代次");
            require(continuousAttachmentIntact(e),"持续兵装：重建破坏 TLS 附着");
            // UAV 由 ActiveWeapons 保有，SpawnedWeapons 会被新 MSL 覆盖，且新 SP 的小池未必容得下两架 UAV。
            if(e.kind!=ContinuousKind::UAV && !continuousContains(spawned,e.actor.address))addSpawned.push_back(e.actor.address);
            if(!e.released && !read<unsigned char>(e.actor.address+0x5c0) && !continuousContains(active,e.actor.address))addActive.push_back(e.actor.address);
        }
        const size_t neededActive=active.count+addActive.size(),neededSpawned=spawned.count+addSpawned.size();
        nativeLog(Json{{"event","CONTINUOUS_REFERENCE_PLAN"},{"active_count",active.count},{"active_capacity",active.capacity},
            {"active_required",neededActive},{"spawned_count",spawned.count},{"spawned_capacity",spawned.capacity},{"spawned_required",neededSpawned}}.dump());
        continuousReserve(activator+0x338,neededActive,"active");
        continuousReserve(activator+0x2e8,neededSpawned,"spawned");
        // Realloc 可移动 data；两组准备完毕后重新读取，禁止使用旧分配区地址。
        active=continuousArray(activator+0x338);spawned=continuousArray(activator+0x2e8);
        require(active.count+addActive.size()==neededActive && spawned.count+addSpawned.size()==neededSpawned,
            "持续兵装：容量准备期间原生引用数量改变");
        require(active.count+addActive.size()<=static_cast<size_t>(active.capacity) &&
            spawned.count+addSpawned.size()<=static_cast<size_t>(spawned.capacity),"持续兵装：原生引用容量准备失败");
        // 两组容量就绪后才写缺项与 Num，不复制旧数组头，不改变元素数量语义或分配器。
        for(size_t i=0;i<addSpawned.size();++i)write<Addr>(spawned.data+(spawned.count+i)*sizeof(Addr),addSpawned[i]);
        for(size_t i=0;i<addActive.size();++i)write<Addr>(active.data+(active.count+i)*sizeof(Addr),addActive[i]);
        write<int>(spawned.header+8,spawned.count+static_cast<int>(addSpawned.size()));
        write<int>(active.header+8,active.count+static_cast<int>(addActive.size()));
        committed=true;
        if(!roots.empty())nativeLog(Json{{"event","CONTINUOUS_RETAINED"},{"count",roots.size()},
            {"active_added",addActive.size()},{"spawned_added",addSpawned.size()}}.dump());
    }
    ~ContinuousRebuild() noexcept {
        if(!plane || committed || !rebuilding)return;
        // 临时 root 析构前结束旧效果；异常明确记录，不能声称已经恢复持续行为。
        for(auto& e:continuousEffects)if(e.plane.address==plane && e.activator.address==activator)
            try{continuousFinish(e);}catch(const std::exception& error){continuousRuntimeFailure=error.what();nativeLog(std::string("CONTINUOUS_ABORT_ERROR ")+error.what());}
    }
};
void continuousDrain(Addr plane,Addr activator) {
    // 边界也捕获从未换槽的新发射体，包含已移出 ActiveWeapons 的尾迹。
    // 这里只处理本实验真实类型，其他替换武器仍由其原生边界收尾。
    Command boundary;boundary.op="switch";boundary.plane=plane;boundary.activator=activator;
    ContinuousRebuild retained(boundary,false);
    for(auto& e:continuousEffects)if(e.plane.address==plane && e.activator.address==activator)continuousFinish(e);
    retained.committed=true;continuousSweep();
}
void continuousAbortRetained() noexcept {
    // 覆盖 initialize 后恢复库存、类别或最终提交失败；不吸收未知新玩家对象。
    for(auto& e:continuousEffects)if(e.plane.address==session.plane && e.activator.address==session.activator)
        try{continuousFinish(e);}catch(const std::exception& error){continuousRuntimeFailure=error.what();nativeLog(std::string("CONTINUOUS_ABORT_ERROR ")+error.what());}
}
