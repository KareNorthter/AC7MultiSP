// ADFX-01 持续兵装实验；在核心私有命名空间内使用，仅限 build 9855922。
#pragma once

struct ContinuousIdentity { Addr address{}; int index{},serial{}; };
struct ContinuousEffect {
    ContinuousIdentity actor,plane,activator,level;
    Addr weaponClass{};
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
bool continuousHooksReady=false;
std::string continuousHookFailure;
std::string continuousRuntimeFailure; // 回调错误留给下一次 HUD/命令读取，不能只写日志后继续运行。
bool continuousRebuildStarted=false; // 仅当前核心命令；拒绝调试或无效请求不能顺带结束仍在工作的效果。
std::atomic<DWORD> continuousThread{0};

Addr continuousBase() {return reinterpret_cast<Addr>(GetModuleHandleW(nullptr));}
Addr continuousEntry(const ContinuousIdentity& id) {
    const int count=read<int>(continuousBase()+0x3bcfdbc);
    require(count>=0 && count<3000000,"持续兵装：对象表长度无效");
    if(!id.address || id.index<0 || id.index>=count)return 0;
    const Addr entry=read<Addr>(continuousBase()+0x3bcfdb0)+static_cast<Addr>(id.index)*24;
    return read<Addr>(entry)==id.address && read<int>(entry+16)==id.serial ? entry:0;
}
bool continuousAlive(const ContinuousIdentity& id) {
    const Addr entry=continuousEntry(id);
    return entry && !(read<unsigned>(entry+8)&0x30000000u); // Unreachable / PendingKill。
}
ContinuousIdentity continuousIdentity(Addr object) {
    require(object!=0,"持续兵装：对象为空");
    const int index=read<int>(object+0xc),count=read<int>(continuousBase()+0x3bcfdbc);
    require(index>=0 && index<count && count<3000000,"持续兵装：对象索引越界");
    const Addr entry=read<Addr>(continuousBase()+0x3bcfdb0)+static_cast<Addr>(index)*24;
    ContinuousIdentity id{object,index,read<int>(entry+16)};
    require(continuousAlive(id),"持续兵装：对象身份已失效");return id;
}
std::string continuousName(Addr object) {
    const unsigned index=read<unsigned>(object+0x18);
    require(index<4000000,"持续兵装：名称索引越界");
    const Addr names=read<Addr>(continuousBase()+0x3bc7070);
    const Addr chunk=read<Addr>(names+(index/0x4000)*8);
    const Addr entry=read<Addr>(chunk+(index%0x4000)*8);
    require(!(read<unsigned char>(entry)&1),"持续兵装：资源名编码不符");
    std::string name;
    for(int i=0;i<240;++i){const char c=read<char>(entry+16+i);if(!c)return name;name+=c;}
    throw std::runtime_error("持续兵装：资源名过长");
}
bool continuousClass(Addr cls,const char* name,const char* package) {
    return continuousName(cls)==name && continuousName(read<Addr>(cls+0x20))==package;
}
bool continuousMorgan(Addr plane) {
    continuousIdentity(plane);
    Addr cls=read<Addr>(plane+0x10);
    for(int i=0;cls && i<32;++i,cls=read<Addr>(cls+0x30))
        if(continuousClass(cls,"AcePlayerPawn_mrgn_C","/Game/Blueprint/Player/Pawn/AcePlayerPawn_mrgn"))return true;
    return false;
}
// 名称、包、原生虚表与冷却契约一起核对；不支持结束时才装填的替换兵装。
bool continuousSupported(Addr weapon) {
    const Addr cls=read<Addr>(weapon+0x10),table=read<Addr>(weapon),base=continuousBase();
    const int id=read<unsigned char>(weapon+0x3b0);
    const bool tls=id==25 && continuousClass(cls,"plwp_tls_x1_C","/Game/Blueprint/Weapons/Player/Variation/plwp_tls_x1");
    const bool ecm=id==31 && continuousClass(cls,"plwp_esm_x0_C","/Game/Blueprint/Weapons/Player/Base/plwp_esm_x0");
    return (tls || ecm) && read<unsigned char>(weapon+0x458)==1 && !read<unsigned char>(weapon+0x459) &&
        !read<Addr>(weapon+0x5d8) && read<Addr>(table+0x648)==base+0x894850 &&
        read<Addr>(table+0x628)==base+(tls?0x85f1c0:0x895fd0) &&
        read<Addr>(table+0x680)==base+(tls?0x861490:0x86bf90);
}
struct ContinuousArray {Addr header{},data{};int count{},capacity{};};
ContinuousArray continuousArray(Addr header) {
    ContinuousArray a{header,read<Addr>(header),read<int>(header+8),read<int>(header+12)};
    require(a.count>=0 && a.count<=a.capacity && a.capacity<=65536,"持续兵装：原生引用数组无效");
    if(a.capacity)checkMemory(a.data,static_cast<size_t>(a.capacity)*sizeof(Addr));
    return a;
}
bool continuousContains(const ContinuousArray& a,Addr actor) {
    for(int i=0;i<a.count;++i)if(read<Addr>(a.data+i*sizeof(Addr))==actor)return true;
    return false;
}
void continuousRemove(Addr header,Addr actor) {
    const auto a=continuousArray(header);int count=0;
    for(int i=0;i<a.count;++i){const Addr item=read<Addr>(a.data+i*sizeof(Addr));
        if(item!=actor)write<Addr>(a.data+count++*sizeof(Addr),item);}
    for(int i=count;i<a.count;++i)write<Addr>(a.data+i*sizeof(Addr),0);
    write<int>(header+8,count); // 不释放或更换原生分配区。
}
bool continuousSameShot(const ContinuousEffect& e) {
    if(!continuousAlive(e.actor) || !continuousAlive(e.plane) || !continuousAlive(e.activator) || !continuousAlive(e.level))return false;
    const Addr w=e.actor.address;
    return read<Addr>(e.plane.address+0x20)==e.level.address && read<Addr>(e.plane.address+0x11a8)==e.activator.address &&
        read<Addr>(w+0x3b8)==e.plane.address && read<Addr>(w+0x10)==e.weaponClass &&
        read<uint64_t>(w+0x370)==e.retirement && read<float>(w+0x610)>=e.age && read<unsigned char>(w+0x368)!=0;
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
    if(read<unsigned char>(e.actor.address+0x3b0)!=25)return;
    Addr component=read<Addr>(e.actor.address+0x160);
    for(int i=0;component && i<8;++i) {
        e.attachment.push_back(continuousIdentity(component));component=read<Addr>(component+0x100);
    }
    require(!component && e.attachment.size()==4,"持续兵装：TLS 附着层级改变");
    require(read<Addr>(e.actor.address+0x640)==e.attachment[1].address &&
        continuousName(e.attachment[1].address)=="SWP1" && continuousName(e.attachment[2].address)=="WeaponMeshBase" &&
        e.attachment[3].address==read<Addr>(e.plane.address+0x160),"持续兵装：TLS 不再附着原生 SWP1");
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
    require(continuousSupported(e.actor.address),"持续兵装：回退收尾接口改变");
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
    explicit ContinuousRebuild(const Command& command,bool strict=true) {
        if(command.op!="switch" || !continuousMorgan(command.plane))return;
        plane=command.plane;activator=command.activator;
        continuousInstall();continuousSweep();
        const auto active=continuousArray(activator+0x338),spawned=continuousArray(activator+0x2e8);
        // 活动数组已移除的尾迹，从现有弱身份记录保留，不恢复历史已退场对象。
        std::vector<ContinuousEffect> additions;
        for(int i=0;i<spawned.count;++i) {
            const Addr w=read<Addr>(spawned.data+i*sizeof(Addr));
            if(!w || read<Addr>(w+0x3b8)!=plane || !read<unsigned char>(w+0x368))continue;
            const int id=read<unsigned char>(w+0x3b0);if(id!=25 && id!=31)continue;
            if(!continuousSupported(w)) {
                require(!strict,"持续兵装：仅支持 ADFX-01 原生 TLS/IEWS 的发射即装填机制");continue;
            }
            if(std::any_of(continuousEffects.begin(),continuousEffects.end(),[w](const auto& e){return e.actor.address==w;}))continue;
            require(read<unsigned char>(w+0x5c0) || continuousContains(active,w),"持续兵装：发射对象缺少活动登记");
            ContinuousEffect e;e.actor=continuousIdentity(w);e.plane=continuousIdentity(plane);e.activator=continuousIdentity(activator);
            e.level=continuousIdentity(read<Addr>(plane+0x20));e.weaponClass=read<Addr>(w+0x10);e.source=session.active;
            e.age=read<float>(w+0x610);e.retirement=read<uint64_t>(w+0x370);e.released=read<unsigned char>(w+0x5c0)!=0;
            require(std::isfinite(e.age) && e.age>=0,"持续兵装：对象时间无效");continuousAttachment(e);additions.push_back(std::move(e));
        }
        require(continuousEffects.size()+additions.size()<=64,"持续兵装：待退场对象过多");
        continuousEffects.reserve(continuousEffects.size()+additions.size());
        roots.reserve(continuousEffects.size()+additions.size());
        // 先完成所有可失败的校验/保有，再登记新对象，避免构造失败改变现有收尾政策。
        for(const auto& e:continuousEffects)if(e.plane.address==plane && e.activator.address==activator) {
            require(continuousAttachmentIntact(e),"持续兵装：已有 TLS 附着改变");roots.emplace_back(e.actor);
        }
        for(const auto& e:additions)roots.emplace_back(e.actor);
        for(auto& e:additions){nativeLog(Json{{"event","CONTINUOUS_CAPTURED"},{"index",e.actor.index},{"serial",e.actor.serial},
            {"source",e.source},{"age",e.age},{"tail",e.released},{"attachment_count",e.attachment.size()}}.dump());
            continuousEffects.push_back(std::move(e));}
    }
    void restoreReferences() {
        if(!plane){committed=true;return;}
        const auto active=continuousArray(activator+0x338),spawned=continuousArray(activator+0x2e8);
        std::vector<Addr> addActive,addSpawned;
        for(auto& e:continuousEffects)if(e.plane.address==plane && e.activator.address==activator) {
            require(continuousSameShot(e),"持续兵装：重建改变了活动对象身份或发射代次");
            require(continuousAttachmentIntact(e),"持续兵装：重建破坏 TLS 附着");
            if(!continuousContains(spawned,e.actor.address))addSpawned.push_back(e.actor.address);
            if(!e.released && !read<unsigned char>(e.actor.address+0x5c0) && !continuousContains(active,e.actor.address))addActive.push_back(e.actor.address);
        }
        require(active.count+addActive.size()<=static_cast<size_t>(active.capacity) &&
            spawned.count+addSpawned.size()<=static_cast<size_t>(spawned.capacity),"持续兵装：原生引用数组容量不足，旧效果将结束");
        // 两组全部验证后才写元素与 Num，不复制数组头，不换分配器或扩大 Max。
        for(size_t i=0;i<addSpawned.size();++i)write<Addr>(spawned.data+(spawned.count+i)*sizeof(Addr),addSpawned[i]);
        for(size_t i=0;i<addActive.size();++i)write<Addr>(active.data+(active.count+i)*sizeof(Addr),addActive[i]);
        write<int>(spawned.header+8,spawned.count+static_cast<int>(addSpawned.size()));
        write<int>(active.header+8,active.count+static_cast<int>(addActive.size()));
        committed=true;
        if(!roots.empty())nativeLog(Json{{"event","CONTINUOUS_RETAINED"},{"count",roots.size()},
            {"active_added",addActive.size()},{"spawned_added",addSpawned.size()}}.dump());
    }
    ~ContinuousRebuild() noexcept {
        if(!plane || committed)return;
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
