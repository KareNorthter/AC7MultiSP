// Su-30SM / F-35C / F-15E 固定挂载；由正式构建显式启用。
// 包含位置在核心私有命名空间中，复用已有内存校验、弹药缓存和日志。
#pragma once

struct MountIdentity { Addr address{}; int index{},serial{}; }; // 地址和对象序列号共同标识实例。
enum class MountAircraft { Unsupported, Su30sm, F35c, F15e, F02a, F15c, F15j, F15m, F16c, Fa27, Mr2k, Mrgn, Rflm, Typn, F22a, Su47, Su57tg, Pkfa, Dark, Zoef, Adf11f, Fa44, Su35, Su37, F04e, F14atg, J39e, Su30, Su34, M29a, F16x, M35d, F14a, F18e, F18etg, M31b, F14d, F18f, Su33, F18x, A10a, F02x, F104, M21b }; // 新机只追加索引，既有机体编号保持。
struct MountSpecification {
    const char* code; // 原版资源前缀。
    std::array<const char*,3> weaponClasses; // 真实武器类；空项表示该组合未适配。
    std::array<int,3> logicalCounts,poolCounts; // 逻辑队列与持久组件池分别核对。
    std::array<size_t,3> rackOffsets; // 各机型现场反射确认的字段位置。
    std::array<const char*,3> meshes; // 空模型仅用于已登记的固定 TLS 发射点。
    int physicalCount; // 可用于取位的全部候选位置数。
    std::array<int,3> loadedCounts; // 原版待发窗口；预览不能把备用队列全画成待发弹。
};
#include "mount_expansion_data.hpp"
#include "animated_mount_data.hpp"
bool mountExpanded(MountAircraft aircraft) {
    return aircraft==MountAircraft::Pkfa || (aircraft>=MountAircraft::F02a && aircraft<=MountAircraft::Typn) ||
        (aircraft>=MountAircraft::Su35 && aircraft<=MountAircraft::M21b);
}
bool mountLocal(MountAircraft aircraft) { return aircraft==MountAircraft::Pkfa || (aircraft>=MountAircraft::F14d && aircraft<=MountAircraft::F18x); }
int mountExpansionIndex(MountAircraft aircraft) {
    require(mountExpanded(aircraft),"扩展挂载：机型未登记");
    if(aircraft==MountAircraft::Pkfa)return 32; // 追加在现有32机之后，不挤占四架外挂候选的索引。
    return aircraft<=MountAircraft::Typn?static_cast<int>(aircraft)-static_cast<int>(MountAircraft::F02a):
        10+static_cast<int>(aircraft)-static_cast<int>(MountAircraft::Su35);
}
MountAircraft mountExpansionAircraft(size_t index) {
    require(index<mountExpansionData().at("planes").size(),"扩展挂载：机型索引越界");
    if(index==32)return MountAircraft::Pkfa;
    return static_cast<MountAircraft>(index<10?static_cast<int>(MountAircraft::F02a)+index:
        static_cast<int>(MountAircraft::Su35)+index-10);
}
bool mountAnimated(MountAircraft aircraft) {return aircraft>=MountAircraft::F22a && aircraft<=MountAircraft::Su57tg;}
bool mountExterior(MountAircraft aircraft) { return aircraft==MountAircraft::F22a; }
bool mountLight(MountAircraft aircraft) { return aircraft>=MountAircraft::Dark && aircraft<=MountAircraft::Fa44; }
bool mountNativeOnly(MountAircraft aircraft) { return aircraft==MountAircraft::Su47 || aircraft==MountAircraft::Su57tg; }
const MountSpecification& mountAnimatedSpecification(MountAircraft aircraft);
const MountSpecification& mountLightSpecification(MountAircraft aircraft);
const Json& mountAnimatedPlan(MountAircraft aircraft,unsigned mask);
const Json& mountExpansionDefinition(MountAircraft aircraft) {
    require(mountExpanded(aircraft),"扩展挂载：机型不在白名单");
    return mountExpansionData().at("planes").at(mountExpansionIndex(aircraft));
}
bool mountExpansionNative(MountAircraft aircraft,unsigned mask) {
    // 局部适配明确按组合退让；不能因同一机体有双挂方案而接管三挂。
    return mountExpanded(aircraft) && mask>=1 && mask<=7 &&
        mountExpansionDefinition(aircraft).at("plans").at(mask).value("native_only",false);
}
const MountSpecification& mountExpansionSpecification(MountAircraft aircraft) {
    static const auto specifications=[] {
        std::vector<MountSpecification> result;
        for(const auto& plane:mountExpansionData().at("planes")) {
            MountSpecification spec{};spec.code=plane.at("code").get_ref<const std::string&>().c_str();
            for(int i=0;i<3;++i) {
                const auto& group=plane.at("groups").at(i);
                spec.weaponClasses[i]=group.at("class").get_ref<const std::string&>().c_str();
                spec.logicalCounts[i]=spec.poolCounts[i]=static_cast<int>(group.at("sockets").size());
                spec.loadedCounts[i]=group.at("window").get<int>();
                spec.meshes[i]=group.at("mesh").is_null()?nullptr:group.at("mesh").get_ref<const std::string&>().c_str();
            }
            result.push_back(spec);
        }
        return result;
    }();
    return specifications.at(mountExpansionIndex(aircraft));
}
// 共用身份检查，布局映射仍按机型明确处理。
const MountSpecification& mountSpecification(MountAircraft aircraft) {
    static const MountSpecification su30{"su30sm",{"plwp_6aam_r0_C","plwp_lagm_r0_C","plwp_qaam_r0_C"},
        {6,6,4},{6,4,6},{0x1ba0,0x1b98,0x1b90},{"w_4aam_r0_SM","w_qaam_r0_SM","w_lasm_r0_SM"},6,{6,2,2}}; // 多挂共用原生六处支架，单 QAAM 保留四处。
    static const MountSpecification f35{"f35c",{"plwp_sod_a0_C","plwp_8agm_a0_C","plwp_4aam_a0_C"},
        {2,12,6},{2,6,12},{0x1ba0,0x1b90,0x1b98},{"w_sod_a0_closed_SM","w_4aam_a0_SM","w_8agm_a0_SM"},20,{2,8,4}}; // SWP2 保留骨架动画和原生挂点。
    static const MountSpecification f15{"f15e",{"plwp_6aam_a0_C","plwp_sffs_a0_C","plwp_tls_x0_C"},
        {6,12,1},{6,12,1},{0x1ba0,0x1b98,0x1b90},{"w_4aam_a0_SM","w_sffs_a0_closed_SM",nullptr},19,{6,2,1}}; // TLS 炮体在挂架内。
    static const MountSpecification f22{"f22a",{"plwp_qaam_a0_C","plwp_xsdb_a0_C","plwp_8aam_a0_C"},
        {6,6,14},{6,6,14},{},{"w_qaam_a0_SM","w_xsdb_a0_closed_SM","w_4aam_a0_SM"},14,{2,4,8}};
    switch(aircraft) {
        case MountAircraft::Su30sm:return su30;
        case MountAircraft::F35c:return f35;
        case MountAircraft::F15e:return f15;
        case MountAircraft::F22a:return f22;
        case MountAircraft::Pkfa:return mountExpansionSpecification(aircraft);
        default:if(mountLight(aircraft))return mountLightSpecification(aircraft);if(mountAnimated(aircraft))return mountAnimatedSpecification(aircraft);if(mountExpanded(aircraft))return mountExpansionSpecification(aircraft);throw std::runtime_error("固定挂载：机型未登记");
    }
}
struct MountVisual {
    MountIdentity identity; // 被本原型管理的一个模型或挂架。
    int pool{},index{}; // pool=0/1/2 对应 SWP1/2/3，index=-1 表示挂架本体。
    bool nativeVisible{},nativeHidden{}; // 原生最后请求的状态，用于明确退出时归还显示。
    uint64_t physicalBit{}; // 完整绑定后的首个物理位置位；随状态复制/移动，不保存元素指针。
    bool fixedEmitter{}; // 同一绑定内武器角色不变，避免逐组件重复查询JSON。
};
struct MountMeshOverride {
    MountIdentity component,original,replacement; // 只替换现成组件的弹体网格，保留原生挂点和父链。
    bool preserveCollision{}; // 仅完整碰撞数据已核验的静态挂架，实际换网格前后复核配置。
};
struct MountExteriorBones;
struct LightMountOwnership;
struct LocalMountOwnership;
struct FixedMountState {
    bool bound{},rebuilding{}; // 重建期间仅固定外观，不改变原生临时武器池取位。
    MountIdentity plane,manager,meshManager; // 限定当前玩家，其他飞机全部直通。
    MountIdentity root,weaponBase; // 现场确认的共同祖先：RootComponent → WeaponMeshBase → SWP。
    bool rootVisible{true},rootHidden{}; // 根组件总显隐独立于武器根，避免两类请求互相覆盖。
    bool ancestorVisible{true},ancestorHidden{}; // 祖先最近一次向下传播的整组显示请求。
    std::array<Addr,3> weaponClasses{}; // 会话最多三个选择槽的真实武器类。
    MountAircraft aircraft{MountAircraft::Unsupported}; // 同一会话只接入一个已匹配的机体。
    std::array<int,3> weaponKinds{}; // 对应各机型规格中的武器顺序，与用户选择顺序无关。
    std::array<int,3> logicalCounts{6,6,4}; // 原生队列长度，F-35C 使用 2/12/6；不改变装填窗口。
    unsigned kindsMask{}; // 每种真实武器占一位，组合变化时重新绑定布局。
    std::array<int,256> order=[] {std::array<int,256> result{};for(int i=0;i<256;++i)result[i]=i;return result;}(); // 当前原生槽对应的持久挂点序号。
    std::array<Addr,64> physical{};
    std::array<size_t,3> rackOffsets{};
    std::array<int,3> poolCounts{};
    std::array<std::vector<int>,3> expansionPositions,expansionSockets,expansionPools; // 逻辑兵装、物理位置和原生供位池分别记录；借位不改变父链。
    std::array<MountIdentity,3> animationRacks{};
    MountIdentity animationActivator;
    ::dualsp::common_bay::ResourceDescription animationDescription;
    std::array<int,64> animationProvider{};
    std::array<std::array<unsigned,2>,64> animationSocket{};
    int physicalCount{6}; // 只遍历本机布局，不把空余容量视为有效位置。
    unsigned rackMask{4}; // 明确选择挂架，固定炮体不按待发弹药显隐。
    std::vector<MountVisual> visuals; // 已核对的武器及挂架显隐；活动骨架保留原父链、动画和 Socket。
    std::vector<MountMeshOverride> meshOverrides; // 任务和预览分别持有原网格快照，退出时独立归还。
    MountIdentity animatedRack; // 已选择并需要持续播放的 F-35C SWP2；显隐接管也必须接管动画生命周期。
    bool nativeRackPaused{},nativeRackTick{}; // 最近一次原生重建请求，解除固定挂载时归还。
    uint64_t readyMask{}; // 实际物理位置的聚合就绪状态。
    ULONGLONG animatedVisualAudit{}; // 无状态变化时只周期核对显隐，骨骼/附着位置仍由原生逐帧更新。
    ULONGLONG localAnimationAudit{}; // 局部机构资格巡检独立计时，不被弹药显隐刷新延后。
    MountIdentity exteriorRack;
    size_t exteriorMeshOffset{};
    MountIdentity exteriorMesh;
    bool exteriorAnimationOwned{},exteriorNativeNoSkeleton{},exteriorNativeTick{};
    int exteriorConfig{-1};
    std::shared_ptr<MountExteriorBones> exteriorBones; // 任务/补给共享同组件时共用一份骨显隐恢复记录。
    std::shared_ptr<LightMountOwnership> light; // 四机独立轻量外观账本，绝不接管完整公共弹舱。
    std::shared_ptr<LocalMountOwnership> local; // 新四机共享实例租约，只维持原生SWP3局部机构。
    std::bitset<256> pendingReadyMask{}; // 已就绪但等待新池完成原生装填事件的逻辑槽。
    std::string error; // 钩子错误交回 Lua，不能在游戏调用栈中抛出异常。
} fixedMount;
bool fixedMountEnabled=false; // Lua 启动时明确启用；默认完全使用原生单 SP 外观。
bool fixedMountFailed=false; // 实验接入失败后，本次玩家会话保持原生回退。
std::string fixedMountWarning; // 回退提示独立于兵装事务错误。

struct MountPreviewState {
    bool active{},nativeOnly{},failed{},pending{}; // 单选/失败仍持有视觉所有权，阻止旧任务布局覆盖原生。
    int mode{};
    bool planCached{},restorePending{};
    std::array<int,3> configs{-1,-1,-1},ammo{};
    ULONGLONG restoreAttempt{},lastApply{};
    MountIdentity owner,activator,world,source; // 机库管理器/玩家控制器或任务会话持有模型，页面只提供候选。
    size_t sourcePlaneOffset{};
    FixedMountState layout; // 独立模型上下文；绝不借用 session 或发射映射。
    std::string warning;
} mountPreview;
bool mountPreviewRelease(Addr owner=0,bool resumeTask=true);
void mountPreviewApply();
bool mountPreviewMatchesManager(Addr manager);
void mountPreviewFail(const char* message) noexcept;
bool mountPreviewFilter(void* object,bool requested,unsigned char propagation,bool hidden);
bool mountClassIs(Addr object,const char* name,const char* package);
Addr mountOuterWorld(Addr object);
bool mountAnimationRelease(Addr plane=0,unsigned scene=0);
void mountAnimationEnsure(FixedMountState& state,unsigned scene);
bool mountAnimationShow(const FixedMountState& state,const MountVisual& visual);
bool mountAnimationCopyTransform(void* output,const FixedMountState& state,int position);
void mountAnimationPartsAfter(void* parts);
void mountAnimationBeforePartsHook();
void mountAnimationAfterPartsHook();
bool mountAnimationTickCodeCurrent();
void mountBindExteriorVisuals(FixedMountState&,Addr);
bool mountExteriorDesired(const FixedMountState&,const MountVisual&,bool);
void mountExteriorMaintain(FixedMountState&);
void mountExteriorRelease(FixedMountState&);
void mountExteriorInherit(FixedMountState&,const FixedMountState&);
void mountExteriorBeforeRebuild(FixedMountState&);
void mountExteriorObserveInit(FixedMountState&);
void mountExteriorRefresh(double);
uint64_t mountExteriorPreviewMask(const FixedMountState&,const std::array<int,3>&,const std::array<int,3>&,int);
bool mountLightOwnsPool(MountAircraft,int);
bool mountLightNative(MountAircraft,unsigned);
bool mountLightMapped(MountAircraft,int);
unsigned mountLightRackMask(MountAircraft,unsigned);
void mountBindLightVisuals(FixedMountState&,Addr);
void mountLocalBind(FixedMountState&,Addr);
void mountLocalMaintain(FixedMountState&);
void mountLocalRelease(FixedMountState&);
void mountLocalBeforeRebuild(FixedMountState&);
void mountLocalRemember(void*,unsigned,bool=false) noexcept;
bool mountLocalCopyTransform(void*,const FixedMountState&,int);
bool mountLightDesired(const FixedMountState&,const MountVisual&,bool);
void mountLightMaintain(FixedMountState&);
void mountLightRelease(FixedMountState&,bool keepBones=false);
void mountLightBeforeRebuild(const Command&);
void mountLightInherit(FixedMountState&,const FixedMountState&);
bool mountLightLayoutChanged(const FixedMountState&);
bool mountLightCopyTransform(void*,const FixedMountState&,int);
bool mountLightAnimationParts(void*,float,void*);
bool aircraftBayParts(void*,float,void*);
bool mountInternalBayOpen(const FixedMountState&);
bool mountLightAnimationRelease();
void mountLightAnimationCommit();
void mountLightAnimationBeforeRebuild(const Command&,int);
bool mountCfaMuzzleFilter(void*,bool,unsigned char,bool);
bool mountCfaMuzzleRelease();
void mountCfaMuzzleSync(bool forceDiscovery=false);
bool mountPreviewOwnsTask() { return mountPreview.active && mountPreview.layout.plane.address==fixedMount.plane.address; }

using MountVisibility=void(*)(void*,bool,unsigned char); // 已读完整实现：0 不传播，1 仅通知子级渲染，2 改子级状态。
using MountTransform=bool(*)(void*,void*,int,int); // MeshManager、输出 FTransform、分组、逻辑槽。
using MountSelection=void(*)(void*,unsigned char,void*,int*); // 保留原生选槽，只替换已选槽的输出变换。
using MountMaster=void(*)(void*,bool,bool); // 原生总显隐及强制更新参数。
using MountPartsUpdate=void(*)(void*,float,void*); // 原生 PartsAnimation、帧间隔、输入结构；已核对完整函数及调用点。
MountVisibility mountOriginalVisibility{},mountOriginalHidden{}; // 绕过本原型钩子的引擎接口。
MountTransform mountOriginalTransform{}; // 原生直接取位入口。
MountSelection mountOriginalSelection{}; // 原生选槽取位入口。
MountMaster mountOriginalMaster{}; // 原生总显隐入口。
MountPartsUpdate mountOriginalPartsUpdate{}; // 完整原生动画更新仍执行一次，不重复推进其他飞控动画。
bool mountHooksReady=false; // 同一进程只安装一次，解绑后按对象身份直通。
std::string mountHookFailure; // 安装失败明确阻止本进程重试，不留下半套钩子反复接入。
std::atomic<DWORD> mountGameThread{0}; // 全局显隐入口可能被别的线程调用，只有绑定线程可读取本原型状态。

// 只解码本研究使用的 ASCII 对象名；名称表及最大长度均有边界。
std::string mountNameIndex(unsigned index,const NativeReadBatch& memory) {
    require(index<4000000,"固定挂载：名称索引越界");
    const Addr base=reinterpret_cast<Addr>(GetModuleHandleW(nullptr)); // 当前主映像。
    const Addr names=memory.read<Addr>(base+0x3bc7070); // 本 build 的分块名称表。
    const Addr chunk=memory.read<Addr>(names+(index/0x4000)*8); // 每块 16384 项。
    const Addr entry=memory.read<Addr>(chunk+(index%0x4000)*8); // 名称条目含 16 字节头。
    require((memory.read<unsigned char>(entry)&1)==0,"固定挂载：预期的资源名不是 ASCII");
    return readBoundedName(entry+16,"固定挂载：资源名不可读或没有终止符");
}
std::string mountNameIndex(unsigned index) {
    const NativeReadBatch memory;return mountNameIndex(index,memory);
}
// 资源身份使用对象本名，不依赖用户配置的机体编号。
std::string mountName(Addr object) { return mountNameIndex(read<unsigned>(object+0x18)); }

// 对象表只做按索引访问，避免在每次更新中扫描全局对象。
MountIdentity mountIdentity(Addr object) {
    const NativeReadBatch memory;
    const Addr base=reinterpret_cast<Addr>(GetModuleHandleW(nullptr)); // 当前 UObject 表。
    const int index=memory.read<int>(object+0xc),count=memory.read<int>(base+0x3bcfdb0+12); // 实例索引和表长度。
    require(index>=0 && index<count && count<3000000,"固定挂载：对象索引无效");
    const Addr entry=memory.read<Addr>(base+0x3bcfdb0)+static_cast<Addr>(index)*24; // 每项含地址与序列号。
    require(memory.read<Addr>(entry)==object,"固定挂载：对象表身份不匹配");
    return {object,index,memory.read<int>(entry+16)};
}
// 先核对对象表，再读取实例，拒绝已销毁对象及地址复用。
bool mountAlive(const MountIdentity& identity) {
    if(!identity.address)return false;
    const NativeReadBatch memory;
    const Addr base=reinterpret_cast<Addr>(GetModuleHandleW(nullptr)); // 对象表全局地址。
    const int count=memory.read<int>(base+0x3bcfdb0+12); // 当前长度。
    require(count>=0 && count<3000000,"固定挂载：对象表长度无效");
    if(identity.index<0 || identity.index>=count)return false;
    const Addr entry=memory.read<Addr>(base+0x3bcfdb0)+static_cast<Addr>(identity.index)*24; // 当前条目。
    return memory.read<Addr>(entry)==identity.address && memory.read<int>(entry+16)==identity.serial;
}
// 同一段只读循环复用对象表头，条目仍逐个核对地址和序列号；不缓存跨帧内存页。
// 作用域内不能调用ProcessEvent、生成/回收对象或其他可能重入引擎的接口。
struct MountIdentityBatch {
    NativeReadBatch memory;
    Addr entries{};int count{};
    MountIdentityBatch() {
        const Addr address=reinterpret_cast<Addr>(GetModuleHandleW(nullptr))+0x3bcfdb0;
        const auto header=memory.read<std::array<unsigned char,16>>(address);
        std::memcpy(&entries,header.data(),8);std::memcpy(&count,header.data()+12,4);
        require(count>=0 && count<3000000,"固定挂载：对象表长度无效");
    }
    bool alive(const MountIdentity& id) const {
        if(!id.address || id.index<0 || id.index>=count)return false;
        const Addr entry=entries+static_cast<Addr>(id.index)*24;
        const auto bytes=memory.read<std::array<unsigned char,24>>(entry);
        Addr object{};int serial{};std::memcpy(&object,bytes.data(),8);std::memcpy(&serial,bytes.data()+16,4);
        return object==id.address && serial==id.serial;
    }
    MountIdentity identity(Addr object) const {
        const int index=memory.read<int>(object+0xc);
        require(index>=0 && index<count,"固定挂载：对象索引无效");
        const auto bytes=memory.read<std::array<unsigned char,24>>(entries+static_cast<Addr>(index)*24);
        Addr address{};int serial{};std::memcpy(&address,bytes.data(),8);std::memcpy(&serial,bytes.data()+16,4);
        require(address==object,"固定挂载：对象表身份不匹配");
        return {object,index,serial};
    }
    std::string name(Addr object) const {return mountNameIndex(memory.read<unsigned>(object+0x18),memory);}
    template<class T> T read(Addr address) const {return memory.read<T>(address);}
};
// 使用已核对的组件 Tick 接口更新调度队列，不能仅改 Tick 状态字节。
void mountSetRackAnimation(Addr rack,bool paused,bool tick) {
    const Addr base=reinterpret_cast<Addr>(GetModuleHandleW(nullptr)); // 当前构建的 SetComponentTickEnabled。
    constexpr unsigned char prefix[]={0x48,0x89,0x5c,0x24,0x08,0x57,0x48,0x83,0xec,0x20,0xf6,0x41,0x3c,0x02};
    require(read<Addr>(read<Addr>(rack)+0x308)==base+0x168aaa0 &&
        (std::memcmp(reinterpret_cast<void*>(base+0x168aaa0),prefix,sizeof(prefix))==0 || mountAnimationTickCodeCurrent()),"活动挂架：原生 Tick 接口改变");
    const auto flags=read<unsigned>(rack+0x9b0); // 旧F-35C接口管理的是bNoSkeletonUpdate；保留原有行为，真正Pause为另一位。
    write<unsigned>(rack+0x9b0,paused?(flags|2u):(flags&~2u));
    using SetTick=void(*)(void*,bool); // 原生维护注册队列和启用状态。
    reinterpret_cast<SetTick>(base+0x168aaa0)(reinterpret_cast<void*>(rack),tick);
    require((read<unsigned char>(rack+0x3f)!=0)==tick,"活动挂架：Tick 状态未按请求更新");
}
// 涂装派生类必须继承已登记原版机体的完整资源类。
MountAircraft mountAircraft(Addr plane) {
    Addr cls=read<Addr>(plane+0x10); // 从真实实例沿类父链检查。
    for(int depth=0;cls && depth<32;++depth,cls=read<Addr>(cls+0x30)) {
        const std::string name=mountName(cls);
        for(size_t i=0;i<mountExpansionData().at("planes").size();++i) {
            const auto code=mountExpansionData().at("planes").at(i).at("code").get<std::string>();
            if(name=="AcePlayerPawn_"+code+"_C" && mountName(read<Addr>(cls+0x20))=="/Game/Blueprint/Player/Pawn/AcePlayerPawn_"+code)
                return mountExpansionAircraft(i);
        } // 只有已知类名才进一步检查所属资源包。
        if(name=="AcePlayerPawn_su30sm_C" && mountName(read<Addr>(cls+0x20))=="/Game/Blueprint/Player/Pawn/AcePlayerPawn_su30sm")return MountAircraft::Su30sm;
        if(name=="AcePlayerPawn_f35c_C" && mountName(read<Addr>(cls+0x20))=="/Game/Blueprint/Player/Pawn/AcePlayerPawn_f35c")return MountAircraft::F35c;
        if(name=="AcePlayerPawn_f15e_C" && mountName(read<Addr>(cls+0x20))=="/Game/Blueprint/Player/Pawn/AcePlayerPawn_f15e")return MountAircraft::F15e;
        if(name=="AcePlayerPawn_pkfa_C" && mountName(read<Addr>(cls+0x20))=="/Game/Blueprint/Player/Pawn/AcePlayerPawn_pkfa")return MountAircraft::Pkfa;
        for(int i=0;i<4;++i) {
            const auto aircraft=static_cast<MountAircraft>(static_cast<int>(MountAircraft::Dark)+i);
            const std::string code=mountLightSpecification(aircraft).code;
            if(name=="AcePlayerPawn_"+code+"_C" && mountName(read<Addr>(cls+0x20))=="/Game/Blueprint/Player/Pawn/AcePlayerPawn_"+code)return aircraft;
        }
        for(size_t i=0;i<animatedMountData().at("planes").size();++i) {
            const auto code=animatedMountData().at("planes").at(i).at("code").get<std::string>();
            if(name=="AcePlayerPawn_"+code+"_C" && mountName(read<Addr>(cls+0x20))=="/Game/Blueprint/Player/Pawn/AcePlayerPawn_"+code)
                return static_cast<MountAircraft>(static_cast<int>(MountAircraft::F22a)+static_cast<int>(i));
        }
    }
    return MountAircraft::Unsupported;
}
// 绑定成功后，活动组采样继承持久序号；原生初始化期间的新池使用新顺序。
int fixedMountOrdinal(Addr weaponClass,int index) {
    if(mountExterior(fixedMount.aircraft))return index; // 机外保留路线使用原生取位，不延续旧全挂的槽位换位。
    if(fixedMount.bound && !fixedMount.rebuilding && index>=0 && index<fixedMount.logicalCounts[fixedMount.weaponKinds[session.active]] &&
       weaponClass==fixedMount.weaponClasses[session.active])return fixedMount.order[index];
    return index;
}
// 只识别本次重建主动写入的 nextafter 门槛，不用宽泛误差把真正未装填的弹判为就绪。
float fixedMountElapsed(int index,float duration,float elapsed) {
    if(!fixedMount.bound || fixedMount.rebuilding || index<0 || index>=fixedMount.logicalCounts[fixedMount.weaponKinds[session.active]])return elapsed;
    const auto bit=static_cast<size_t>(index); // 原生数组下标，与本次 restore 的记录一致。
    if(!fixedMount.pendingReadyMask.test(bit))return elapsed;
    if(duration>0 && elapsed==std::nextafter(duration,0.0f))return duration;
    fixedMount.pendingReadyMask.reset(bit); // 原生已完成装填或发射，后续完全使用实际计时。
    return elapsed;
}
#include "mount_mesh_override.hpp"
#include "mount_expansion.hpp"
#include "mount_animated_layout.hpp"
// 所有取位及F-15E换位完成后建立反向表，保留原线性查找的首个匹配语义。
void mountBindVisualPositions(FixedMountState& state) {
    require(state.physicalCount>=0 && state.physicalCount<=static_cast<int>(state.physical.size()),"固定挂载：物理位置表越界");
    std::array<bool,3> emitters{};
    for(int pool=0;pool<3;++pool)emitters[pool]=mountFixedEmitter(state.aircraft,pool);
    for(auto& visual:state.visuals) {
        visual.physicalBit=0;visual.fixedEmitter=emitters[visual.pool];
        if(visual.index<0)continue;
        for(int position=0;position<state.physicalCount;++position)
            if(state.physical[position]==visual.identity.address) {
                visual.physicalBit=uint64_t{1}<<position;break;
            }
    }
}
// 只在当前批量应用内复用公共标志；显隐钩子的独立请求仍重新读取现场值。
bool mountGroupVisible(const FixedMountState& state) {
    const NativeReadBatch memory;
    const bool master=memory.read<unsigned char>(state.meshManager.address+0x120)!=0;
    const bool planeHidden=(memory.read<unsigned char>(state.plane.address+0x84)&1)!=0;
    return master && !planeHidden && state.rootVisible && !state.rootHidden && state.ancestorVisible && !state.ancestorHidden;
}
// 所有可视组件均保留原父链；挂架网格按机型和组合独立选择。
bool mountDesired(const FixedMountState& state,const MountVisual& visual,bool groupVisible,int animationGate=-1) {
    if(mountLight(state.aircraft))return mountLightDesired(state,visual,groupVisible);
    if(mountExterior(state.aircraft))return mountExteriorDesired(state,visual,groupVisible);
    if(!groupVisible)return false;
    if(visual.index<0) {
        if(state.aircraft==MountAircraft::Pkfa && visual.pool<2 &&
           !(animationGate<0?mountInternalBayOpen(state):animationGate!=0))return false;
        return (state.rackMask&(1u<<visual.pool))!=0; // 按已核对的挂架组合显示。
    }
    if(mountAnimated(state.aircraft) && !(animationGate<0?mountAnimationShow(state,visual):animationGate!=0))return false;
    if(state.aircraft==MountAircraft::F15e && visual.pool==2)return (state.kindsMask&4)!=0; // 空 TLS 发射组件与炮体同寿命。
    if(visual.fixedEmitter)
        return (state.kindsMask&(1u<<visual.pool)) && visual.nativeVisible; // 固定炮口/效果源不套用消耗弹模型规则。
    if(state.aircraft==MountAircraft::Pkfa &&
       !(animationGate<0?mountInternalBayOpen(state):animationGate!=0))return false;
    return (state.readyMask&visual.physicalBit)!=0;
}
bool mountDesired(const FixedMountState& state,const MountVisual& visual) { return mountDesired(state,visual,mountGroupVisible(state)); }
bool mountCfaEcmVisual(const FixedMountState& state,const MountVisual& visual) {
    return state.aircraft==MountAircraft::Fa44 && visual.pool==2 && visual.index<0;
}
// CFA固定架的最新原生请求由调用方记账；目标状态不变时不重复通知渲染。
void mountApplyOwnedVisibility(const FixedMountState& state,const MountVisual& visual,void* object,bool hidden) {
    const bool desired=hidden?mountCfaEcmVisual(state,visual):mountDesired(state,visual);
    if(state.aircraft==MountAircraft::Fa44 &&
       bool(read<unsigned char>(visual.identity.address+0x1bc)&(hidden?0x20:0x10))==desired)return;
    (hidden?mountOriginalHidden:mountOriginalVisibility)(object,desired,0);
}
// 只在状态发生变化时调用引擎更新；不逐帧强制反隐藏。
void mountApplyVisuals(const FixedMountState& state) {
    if(!mountMeshChanging && !state.meshOverrides.empty()) {
        const auto owner=state.plane;
        const auto changes=state.meshOverrides; // 同步原生回调可能释放上下文，不能持有其元素引用。
        for(const auto& item:changes) {
            mountApplyMesh(item,true);
            if(!state.bound || !mountMeshIdentityEqual(owner,state.plane))return;
        }
    }
    if(state.visuals.empty())return; // 空状态不新增管理器或飞机读取。
    const bool groupVisible=mountGroupVisible(state);
    // 同步批次只做一次完整姿态重验；原生独立显隐请求仍走默认逐次核对。
    const int animationGate=state.aircraft==MountAircraft::Pkfa?int(groupVisible && mountInternalBayOpen(state)):
        mountAnimated(state.aircraft) && !mountExterior(state.aircraft)?int(groupVisible && mountAnimationShow(state,{})):-1;
    for(const auto& visual:state.visuals) {
        const Addr object=visual.identity.address; // 已验证的当前组件。
        unsigned char flags{};
        {
            const MountIdentityBatch identities;
            require(identities.alive(visual.identity),"固定挂载：组件已销毁");
            flags=identities.read<unsigned char>(object+0x1bc); // 身份及两个显隐位共享本组件的读取批次。
        } // 后面的原生显隐可以重入，下一组件重新取得当前对象表和可读区间。
        const bool desired=mountDesired(state,visual,groupVisible,animationGate); // 聚合结果和本批次原生总显隐共同决定。
        const bool desiredHidden=mountCfaEcmVisual(state,visual);
        if(bool(flags&0x20)!=desiredHidden)mountOriginalHidden(reinterpret_cast<void*>(object),desiredHidden,0);
        if(bool(flags&0x10)!=desired)mountOriginalVisibility(reinterpret_cast<void*>(object),desired,0);
    }
}
void mountApply() { if(!mountPreviewOwnsTask())mountApplyVisuals(fixedMount); }
bool mountDesired(const MountVisual& visual) { return mountDesired(fixedMount,visual); }
// 每项独立归还；一项失败不能留下其他多挂模型，待归还项保留身份供后续更新处理。
bool fixedMountRelease() {
    const bool previewReleased=mountPreviewRelease(0,false); // 预览归还失败也必须撤销任务显隐接管；待归还身份仍保存在预览上下文。
    const bool hadPending=!fixedMount.error.empty();
    const bool lightAnimationReleased=mountLightAnimationRelease();
    fixedMount.bound=false; // 先直通，避免归还显示的调用再次被过滤。
    const bool animationReleased=mountAnimationRelease(fixedMount.plane.address,1);
    FixedMountState pending; // 只保留未完成恢复的对象，不重新启用显隐或取位接管。
    auto failure=[&](const std::exception& error) {
        if(!pending.error.empty())pending.error+="；";
        pending.error+=error.what();
    };
    if(!mountCfaMuzzleRelease())failure(std::runtime_error("CFA炮口：原生显示仍待归还"));
    try { mountExteriorRelease(fixedMount); }
    catch(const std::exception& error) {
        pending.aircraft=fixedMount.aircraft;pending.exteriorRack=fixedMount.exteriorRack;
        pending.exteriorAnimationOwned=fixedMount.exteriorAnimationOwned;
        pending.exteriorNativeNoSkeleton=fixedMount.exteriorNativeNoSkeleton;pending.exteriorNativeTick=fixedMount.exteriorNativeTick;
        pending.exteriorBones=fixedMount.exteriorBones;failure(error);
    }
    try { mountLightRelease(fixedMount); }
    catch(const std::exception& error) { pending.aircraft=fixedMount.aircraft;pending.light=fixedMount.light;failure(error); }
    try { mountLocalRelease(fixedMount); }
    catch(const std::exception& error) { pending.aircraft=fixedMount.aircraft;pending.local=fixedMount.local;failure(error); }
    for(const auto& item:fixedMount.meshOverrides)try {
        mountApplyMesh(item,false);
    }catch(const std::exception& error) { pending.meshOverrides.push_back(item);failure(error); }
    try {
        if(mountAlive(fixedMount.animatedRack))
            mountSetRackAnimation(fixedMount.animatedRack.address,fixedMount.nativeRackPaused,fixedMount.nativeRackTick);
    }catch(const std::exception& error) {
        pending.animatedRack=fixedMount.animatedRack;
        pending.nativeRackPaused=fixedMount.nativeRackPaused;pending.nativeRackTick=fixedMount.nativeRackTick;
        failure(error);
    }
    for(const auto& visual:fixedMount.visuals)try {
        if(mountAlive(visual.identity)) {
            mountOriginalHidden(reinterpret_cast<void*>(visual.identity.address),visual.nativeHidden,0);
            mountOriginalVisibility(reinterpret_cast<void*>(visual.identity.address),visual.nativeVisible,0);
        }
    }catch(const std::exception& error) { pending.visuals.push_back(visual);failure(error); }
    fixedMount=std::move(pending);
    if(!fixedMount.error.empty()) {
        fixedMountFailed=true;
        fixedMountWarning="多 SP 挂载已回退，部分原生显示仍待恢复："+fixedMount.error;
        return false;
    }
    if(!animationReleased) { fixedMountWarning="多 SP 动画控制仍待归还";return false; }
    if(!lightAnimationReleased) { fixedMountWarning="CFA盖板输出仍待归还";return false; }
    if(!previewReleased) { fixedMountWarning=mountPreview.warning;return false; }
    if(hadPending)fixedMountWarning="多 SP 挂载已回退为原生单 SP，原生显示已恢复";
    return true;
}
// 外观错误只停实验接管；弹药、切换和其他功能继续使用已完成的原生事务。
void fixedMountFail(const char* message) noexcept {
    const std::string failure=message; // 清理会销毁状态字符串，先取得独立副本。
    fixedMountFailed=true;fixedMountWarning="多 SP 挂载已回退为原生单 SP："+failure;
    try {
        nativeLog(std::string("FIXED_MOUNT_ERROR ")+failure);
        fixedMountRelease();
    }catch(const std::exception& cleanup) {
        fixedMount.bound=false;
        fixedMountWarning=failure+"；归还显示失败："+cleanup.what();
        nativeLog(std::string("FIXED_MOUNT_RELEASE_ERROR ")+fixedMountWarning);
    }
}
// 原生仅把公共 SP 进度写给当前通道，切换配置后旧通道会冻结；在同一动画更新中补齐各舱的连续状态。
void mountPartsUpdate(void* object,float delta,void* input) {
    if(GetCurrentThreadId()==mountGameThread.load() && aircraftBayParts(object,delta,input))return;
    if(GetCurrentThreadId()==mountGameThread.load() && mountLightAnimationParts(object,delta,input))return;
    mountOriginalPartsUpdate(object,delta,input);
    if(GetCurrentThreadId()==mountGameThread.load())mountAnimationPartsAfter(object);
}
// 原生显隐请求仍记账，但固定布局的组件由聚合状态决定；挂架请求不传播到武器。
void mountFilterVisibility(void* object,bool requested,unsigned char propagation,bool hidden) {
    const auto original=hidden?mountOriginalHidden:mountOriginalVisibility; // 两种引擎接口分别处理。
    if(GetCurrentThreadId()==mountGameThread.load() && mountPreview.active) {
        try { if(mountPreviewFilter(object,requested,propagation,hidden))return; }
        catch(const std::exception& error) { mountPreviewFail(error.what());original(object,requested,propagation);return; }
    }
    if(GetCurrentThreadId()==mountGameThread.load() && (fixedMount.bound || !fixedMount.visuals.empty()))try {
        const Addr address=reinterpret_cast<Addr>(object); // 先处理整架飞机/整组挂载的传播请求。
        if(propagation==2 && (address==fixedMount.root.address || address==fixedMount.weaponBase.address)) {
            require(mountAlive(fixedMount.root) && mountAlive(fixedMount.weaponBase),"固定挂载：祖先组件身份失效");
            (address==fixedMount.root.address?(hidden?fixedMount.rootHidden:fixedMount.rootVisible):(hidden?fixedMount.ancestorHidden:fixedMount.ancestorVisible))=requested;
            // 原生随后遍历后代并以参数 0 回调同一入口，固定模型将服从这次整组请求。
        }
        for(auto& visual:fixedMount.visuals)if(visual.identity.address==reinterpret_cast<Addr>(object)) {
            require(mountAlive(visual.identity),"固定挂载：显隐对象身份失效");
            (hidden?visual.nativeHidden:visual.nativeVisible)=requested;
            // 原生挂架传播语义保留在归还记录中，固定显示期间不覆盖子模型。
            if(visual.index<0 && propagation==2)for(auto& child:fixedMount.visuals)
                if(child.pool==visual.pool && child.index>=0)(hidden?child.nativeHidden:child.nativeVisible)=requested;
            if(!fixedMount.bound) { original(object,requested,propagation);return; } // 待归还期间跟踪新的原生请求，避免稍后恢复旧快照。
            mountApplyOwnedVisibility(fixedMount,visual,object,hidden);
            return;
        }
    }catch(const std::exception& error) { fixedMountFail(error.what()); }
    if(GetCurrentThreadId()==mountGameThread.load())try {
        if(mountCfaMuzzleFilter(object,requested,propagation,hidden))return;
    }catch(const std::exception& error) { fixedMountFail(error.what()); }
    original(object,requested,propagation); // 非本机模型或明确退出后的原生行为。
}
// 两个入口保留引擎传播枚举，不将其简化为 bool ABI。
void mountVisibility(void* object,bool visible,unsigned char propagation) { mountFilterVisibility(object,visible,propagation,false); }
void mountHidden(void* object,bool hidden,unsigned char propagation) { mountFilterVisibility(object,hidden,propagation,true); }

// 同一个映射同时用于显示和发射；QAAM 四槽、6AAM/LAGM 六槽均按原生顺序轮转。
int mountPhysical(const FixedMountState& state,int kind,int ordinal) {
    require(kind>=0 && kind<3,"固定挂载：未知武器种类");
    if(mountExpanded(state.aircraft) || mountAnimated(state.aircraft) || mountLight(state.aircraft)) {
        require(ordinal>=0 && ordinal<(mountAnimated(state.aircraft)?256:state.logicalCounts[kind]),"扩展挂载：持久序号越界");
        const auto& positions=state.expansionPositions[kind];
        require(!positions.empty(),"扩展挂载：未登记发射位置");
        return positions.at(static_cast<size_t>(ordinal)%positions.size());
    }
    const int count=state.logicalCounts[kind]; // 逻辑数量不因物理位置减少而改变。
    require(ordinal>=0 && ordinal<count,"固定挂载：持久挂点序号无效");
    if(state.aircraft==MountAircraft::Su30sm && (state.kindsMask==1 || state.kindsMask==2 || state.kindsMask==4))return ordinal; // 单选保留自己的原生池。
    if(state.aircraft==MountAircraft::F35c) {
        // 用户反馈 sp21/22 与发射中的 SOD 穿插；共存恢复仅使用 sp25/26。
        if(kind==2 && (state.kindsMask&1))return 18+ordinal%2;
        return (kind==0?0:kind==1?2:14)+ordinal; // 其余组合保留自己的原生位置和父链。
    }
    if(state.aircraft==MountAircraft::F15e) {
        if(kind==2)return 18; // 固定激光发射点保持原位。
        if(kind==1)return (state.kindsMask&1)?12+ordinal%6:6+ordinal; // 同选6AAM时，SFFS使用内侧sp27–32。
        return ordinal; // 机腹四枚的物理表改指向外侧炸弹位，外翼两枚保留原位。
    }
    if(kind==1)return state.kindsMask==6?2+(ordinal<4?ordinal:ordinal-2):4+ordinal%2;
    if(kind==2)return ordinal%2; // 用户选择恢复原生有共用导轨的 sp23/24。
    require(kind==0,"固定挂载：未知武器种类");
    if(state.kindsMask==5)return 2+(ordinal<4?ordinal:ordinal-2); // QAAM＋6AAM 使用内翼、机腹四处导弹位。
    return (state.kindsMask&4)?2+ordinal%2:(ordinal<4?ordinal:ordinal-2); // 三挂两处6AAM；无QAAM时四处。
}
int mountPhysical(int kind,int ordinal) { return mountPhysical(fixedMount,kind,ordinal); }
// 逻辑槽映射物理位置，不修改原生隐藏/恢复索引，也不改变六发数量。
bool mountCopyTransform(void* output,int slot) {
    const int count=fixedMount.logicalCounts[fixedMount.weaponKinds[session.active]]; // 当前武器自己的逻辑槽数。
    require(slot>=0 && slot<count,"固定挂载：发射槽超出原生范围");
    const int ordinal=fixedMount.order[slot],kind=fixedMount.weaponKinds[session.active]; // 已重排的持久序号。
    require(ordinal>=0 && ordinal<count,"固定挂载：持久挂点序号无效");
    if(fixedMount.aircraft==MountAircraft::M21b && kind==0) {
        const NativeReadBatch memory;
        const Addr weapon=memory.read<Addr>(groupOf(session.activator,true)+0x40);
        require(count==2 && mountName(weapon)=="Default__plwp_mgp_r0_C" &&
            memory.read<int>(weapon+0x44c)==2 && memory.read<unsigned char>(weapon+0x450)==1 &&
            memory.read<unsigned char>(weapon+0x451)==1,"单MGP：原生两槽齐射契约改变");
        // 不写Damage、LoadTime、弹道或弹药；保留Gunplay等Mod当时的实际两发效果。
    }
    const int physical=mountPhysical(kind,ordinal); // 与显隐聚合共用映射。
    if(mountLight(fixedMount.aircraft))return mountLightCopyTransform(output,fixedMount,physical);
    if(mountAnimated(fixedMount.aircraft))return mountAnimationCopyTransform(output,fixedMount,physical);
    if(mountLocal(fixedMount.aircraft))return mountLocalCopyTransform(output,fixedMount,physical);
    const Addr component=fixedMount.physical[physical]; // 与可见模型一致的世界变换来源。
    const auto visual=std::find_if(fixedMount.visuals.begin(),fixedMount.visuals.end(),[&](const MountVisual& item){return item.identity.address==component;}); // 验证目的模型仍属于本次出击。
    require(visual!=fixedMount.visuals.end() && mountAlive(visual->identity),"固定挂载：目的模型身份失效");
    checkMemory(component+0x180,48);checkMemory(reinterpret_cast<Addr>(output),48);
    std::memcpy(output,reinterpret_cast<void*>(component+0x180),48);
    return true;
}
// 原生显示和实际发射都可能使用逻辑槽取位；普通导弹、其他飞机及重建过程保持原生。
bool mountTransform(void* manager,void* output,int group,int slot) {
    if(GetCurrentThreadId()==mountGameThread.load() && fixedMount.bound && !fixedMount.rebuilding && mountNeedsMappedTransform(fixedMount,fixedMount.weaponKinds[session.active]) && group==2 &&
       reinterpret_cast<Addr>(manager)==fixedMount.meshManager.address)try {
        {
            const MountIdentityBatch identities;
            require(identities.alive(fixedMount.plane) && identities.alive(fixedMount.meshManager),"固定挂载：发射身份失效");
        } // 映射可能调用引擎取位，不能沿用这里的读取批次。
        if(mountCopyTransform(output,slot))return true;
    }catch(const std::exception& error) { fixedMountFail(error.what()); }
    return mountOriginalTransform(manager,output,group,slot);
}
// 先由原生选出就绪槽，再替换结果，覆盖原生使用缓存变换的另一条发射路径。
void mountSelection(void* manager,unsigned char group,void* output,int* selected) {
    mountOriginalSelection(manager,group,output,selected);
    if(GetCurrentThreadId()==mountGameThread.load() && fixedMount.bound && !fixedMount.rebuilding && mountNeedsMappedTransform(fixedMount,fixedMount.weaponKinds[session.active]) && group==2 && *selected>=0 &&
       reinterpret_cast<Addr>(manager)==fixedMount.manager.address)try {
        require(mountAlive(fixedMount.plane) && mountAlive(fixedMount.manager),"固定挂载：选槽身份失效");
        const Addr nativeGroup=groupOf(session.activator,true); // 原生输出是索引表位置，不是物理挂点。
        const int count=read<int>(nativeGroup+0x70); // 活动索引表长度。
        require(*selected<count && count<=fixedMount.logicalCounts[fixedMount.weaponKinds[session.active]],"固定挂载：原生选槽索引无效");
        const int slot=read<int>(read<Addr>(nativeGroup+0x68)+static_cast<Addr>(*selected)*4); // 转换为逻辑槽。
        mountCopyTransform(output,slot);
    }catch(const std::exception& error) { fixedMountFail(error.what()); }
}
// 原生总显隐只遍历当前路由；补充更新未激活 SP 的固定模型。
void mountMaster(void* manager,bool visible,bool force) {
    mountOriginalMaster(manager,visible,force);
    if(GetCurrentThreadId()==mountGameThread.load() && mountPreview.active)try {
        if(mountPreviewMatchesManager(reinterpret_cast<Addr>(manager))) { mountPreviewApply();return; }
    }catch(const std::exception& error) { mountPreviewFail(error.what());return; }
    if(GetCurrentThreadId()==mountGameThread.load() && fixedMount.bound && reinterpret_cast<Addr>(manager)==fixedMount.meshManager.address)try { mountApply(); }
    catch(const std::exception& error) { fixedMountFail(error.what()); }
}
// 核对固定 build 的入口指纹，整组创建后一次启用；不是扫描或猜测近似函数。
void mountInstallHooks() {
    if(mountHooksReady) {
        require(GetCurrentThreadId()==mountGameThread.load(),"固定挂载：绑定线程改变");
        return;
    }
    require(mountHookFailure.empty(),mountHookFailure.c_str());
    struct Entry { Addr rva; const char* prefix; void* hook; void** original; }; // 已读代码入口及指纹。
    const Entry entries[]={
        {0x16f8bc0,"48895c2418555657488d6c24b94881ec00010000",reinterpret_cast<void*>(&mountVisibility),reinterpret_cast<void**>(&mountOriginalVisibility)},
        {0x16f62b0,"48895c2418555657488d6c24b94881ec00010000",reinterpret_cast<void*>(&mountHidden),reinterpret_cast<void**>(&mountOriginalHidden)},
        {0x8a0960,"40534883ec60488b0593760103",reinterpret_cast<void*>(&mountTransform),reinterpret_cast<void**>(&mountOriginalTransform)},
        {0x89cdb0,"40555657415441574881eca0000000",reinterpret_cast<void*>(&mountSelection),reinterpret_cast<void**>(&mountOriginalSelection)},
        {0x8a11b0,"405641574883ec280fb6f2",reinterpret_cast<void*>(&mountMaster),reinterpret_cast<void**>(&mountOriginalMaster)},
        {0x1ce6d70,"4d85c00f84fe170000488bc455564881ecb8000000",reinterpret_cast<void*>(&mountPartsUpdate),reinterpret_cast<void**>(&mountOriginalPartsUpdate)}};
    const Addr base=reinterpret_cast<Addr>(GetModuleHandleW(nullptr)); // 本进程游戏映像。
    for(const auto& entry:entries) {
        const std::string hex=entry.prefix; // 精确字节对，不接受通配符。
        for(size_t i=0;i<hex.size();i+=2)
            require(read<unsigned char>(base+entry.rva+i/2)==std::stoul(hex.substr(i,2),nullptr,16),"固定挂载：入口指纹不匹配");
    }
    const auto status=MH_Initialize(); // 复用现有核心的 MinHook 实例。
    require(status==MH_OK || status==MH_ERROR_ALREADY_INITIALIZED,"固定挂载：钩子库初始化失败");
    mountGameThread.store(GetCurrentThreadId()); // 完成所有原生指针初始化后才批量启用钩子。
    std::vector<void*> created; // 记录本次创建的入口，失败时只撤销本原型的钩子。
    try {
        mountAnimationBeforePartsHook();
        for(const auto& entry:entries) {
            void* target=reinterpret_cast<void*>(base+entry.rva); // 当前已核对入口。
            require(MH_CreateHook(target,entry.hook,entry.original)==MH_OK,"固定挂载：创建钩子失败");
            created.push_back(target);
        }
        for(void* target:created)require(MH_QueueEnableHook(target)==MH_OK,"固定挂载：安排启用钩子失败");
        require(MH_ApplyQueued()==MH_OK,"固定挂载：启用钩子失败");
        mountAnimationAfterPartsHook();
    }catch(const std::exception& error) {
        mountHookFailure=error.what();
        for(auto target=created.rbegin();target!=created.rend();++target) {
            const auto cleanup=MH_RemoveHook(*target); // 同时撤销可能已启用的部分，失败也必须报告。
            if(cleanup!=MH_OK)mountHookFailure+="；撤销钩子失败："+std::to_string(cleanup);
        }
        throw std::runtime_error(mountHookFailure);
    }
    mountHooksReady=true;nativeLog("FIXED_MOUNT_HOOKS_READY");
}
unsigned mountRackMask(MountAircraft aircraft,unsigned kindsMask) {
    if(mountLight(aircraft))return mountLightRackMask(aircraft,kindsMask);
    if(mountExterior(aircraft))return kindsMask&4u;
    if(mountAnimated(aircraft))return mountAnimatedPlan(aircraft,kindsMask).at("racks").get<unsigned>();
    if(mountExpanded(aircraft))return mountExpansionPlan(aircraft,kindsMask).at("racks").get<unsigned>();
    if(aircraft==MountAircraft::F35c)return (kindsMask&1)|((kindsMask&2)<<1)|((kindsMask&4)>>1);
    if(aircraft==MountAircraft::F15e)return kindsMask; // 六处 6AAM 恢复后保留对应挂架，SFFS 独立缩减弹体。
    if(kindsMask==1 || kindsMask==2 || kindsMask==4)return (kindsMask&1)|((kindsMask&2)<<1)|((kindsMask&4)>>1);
    return 4; // Su-30SM 多挂共用 SWP3 挂架。
}
bool mountShowReserve(MountAircraft aircraft,unsigned kindsMask,int kind) {
    if(aircraft==MountAircraft::Zoef && kind==2)return true;
    if(aircraft==MountAircraft::Adf11f && kindsMask==3 && kind==1)return true;
    if(mountExpanded(aircraft))return !mountFixedEmitter(aircraft,kind) && !mountExpansionDefinition(aircraft).at("groups").at(kind).at("launcher").get<bool>();
    return (aircraft==MountAircraft::Su30sm && ((kindsMask==6 && kind==1) || (kindsMask==4 && kind==2))) ||
        (aircraft==MountAircraft::F15e && kind==1) || (aircraft==MountAircraft::F35c && kind==1);
}
void mountBindVisuals(FixedMountState& state,Addr plane,Addr activator) {
    const auto aircraft=state.aircraft;const auto kindsMask=state.kindsMask;
    state.rackOffsets=mountSpecification(aircraft).rackOffsets;state.poolCounts=mountSpecification(aircraft).poolCounts;
    const auto& specification=mountSpecification(aircraft);
    const bool single=aircraft==MountAircraft::Su30sm && (kindsMask==1 || kindsMask==2 || kindsMask==4);
    if(single)state.physicalCount=specification.logicalCounts[kindsMask==1?0:kindsMask==2?1:2];
    state.plane=mountIdentity(plane);
    state.animationActivator=mountIdentity(activator);
    state.manager=mountIdentity(read<Addr>(activator+0x260));
    state.meshManager=mountIdentity(read<Addr>(state.manager.address+0x150));
    state.root=mountIdentity(read<Addr>(plane+0x160));
    state.weaponBase=mountIdentity(read<Addr>(plane+0x1ae0));
    require(read<Addr>(state.weaponBase.address+0x100)==state.root.address &&
        read<Addr>(state.root.address+0x100)==0,"固定挂载：共同祖先链不匹配");
    const auto rootFlags=read<unsigned char>(state.root.address+0x1bc);
    state.rootVisible=bool(rootFlags&0x10);state.rootHidden=bool(rootFlags&0x20);
    const auto ancestorFlags=read<unsigned char>(state.weaponBase.address+0x1bc); // 初始总组状态来自共同挂载父级。
    state.ancestorVisible=bool(ancestorFlags&0x10);state.ancestorHidden=bool(ancestorFlags&0x20);
    if(mountLight(aircraft)) { mountBindLightVisuals(state,plane);mountBindVisualPositions(state);return; }
    if(mountExterior(aircraft)) { mountBindExteriorVisuals(state,plane);mountBindVisualPositions(state);return; }
    if(mountExpanded(aircraft)) {mountBindExpansionVisuals(state,plane);mountBindVisualPositions(state);return;}
    if(mountAnimated(aircraft)) {mountBindAnimatedVisuals(state,plane);mountBindVisualPositions(state);return;}
    for(int pool=0;pool<3;++pool) {
        if(specification.poolCounts[pool]==0)continue; // 未登记活动弹仓不进入静态显隐过滤。
        const Addr rack=read<Addr>(plane+specification.rackOffsets[pool]); // 原生挂架父组件。
        mountIdentity(rack); // 读组件布局前先确认当前对象表身份。
        const bool skeletal=aircraft==MountAircraft::F35c && pool==1; // 仅现场核对的 F-35C SWP2 是活动骨架。
        require(mountName(read<Addr>(rack+0x10))==(skeletal?"SkeletalMeshComponent":"StaticMeshComponent"),"固定挂载：挂架类型不符");
        require(read<Addr>(rack+0x100)==state.weaponBase.address,"固定挂载：挂架共同父级不匹配");
        const std::string expected=std::string(specification.code)+"_SWP"+std::to_string(pool+1)+(skeletal?"":"_SM"); // 精确资源名。
        require(mountName(read<Addr>(rack+(skeletal?0x718:0x728)))==expected,"固定挂载：挂架资源被替换");
        const auto flags=read<unsigned char>(rack+0x1bc); // 退出原型时归还的原生状态。
        state.visuals.push_back({mountIdentity(rack),pool,-1,bool(flags&0x10),bool(flags&0x20)});
        const Addr array=state.meshManager.address+0x150+pool*16; // 三套持久模型池。
        const int count=read<int>(array+8),expectedCount=specification.poolCounts[pool]; // 原生持久模型池数量。
        require(count==expectedCount && read<int>(array+12)>=count,"固定挂载：模型池数量不匹配");
        for(int index=0;index<count;++index) {
            const Addr component=read<Addr>(read<Addr>(array)+index*8); // 数组顺序须与 Socket 一致。
            require(mountName(read<Addr>(mountIdentity(component).address+0x10))=="StaticMeshComponent","固定挂载：武器组件类型不符");
            std::string socket=mountNameIndex(read<unsigned>(component+0x108)); // 资源混用 SP/sp 大小写。
            for(char& character:socket)if(character>='A' && character<='Z')character+=32;
            require(socket=="sp"+std::to_string((pool+1)*10+index+1) && read<Addr>(component+0x100)==rack,"固定挂载：Socket 或父链不匹配");
            const Addr mesh=mountBaselineMesh(mountIdentity(component),read<Addr>(component+0x728)); // 已接管组件使用保存的原网格核对。
            require(specification.meshes[pool]?mesh && mountName(mesh)==specification.meshes[pool]:mesh==0,"固定挂载：武器模型被替换");
            const auto flags=read<unsigned char>(component+0x1bc); // 原生显隐快照。
            state.visuals.push_back({mountIdentity(component),pool,index,bool(flags&0x10),bool(flags&0x20)});
            if(aircraft==MountAircraft::F35c)state.physical[(pool==0?0:pool==1?14:2)+index]=component; // 每种兵装保持自己的原生组件和父链。
            else if(aircraft==MountAircraft::F15e)state.physical[(pool==0?0:pool==1?6:18)+index]=component; // 原生池与物理表固定对应。
            else if(single) {
                if(pool==(kindsMask==1?0:kindsMask==2?2:1))state.physical[index]=component;
            } else {
                const bool hasQaam=(kindsMask&4)!=0;
                if(pool==0 && ((kindsMask==5 && index>=2) || (kindsMask!=5 && index<4 && (!hasQaam || index>=2))))state.physical[index]=component;
                if(hasQaam && pool==1 && index>=2)state.physical[index-2]=component; // sp23/24 与SWP3原生导轨对应。
                if(pool==2 && kindsMask!=5 && (index>=4 || (kindsMask==6 && index>=2)))state.physical[index]=component;
            }
        }
    }
    require(std::all_of(state.physical.begin(),state.physical.begin()+state.physicalCount,[](Addr component){return component!=0;}),"固定挂载：预期物理模型未齐备");
    if(aircraft==MountAircraft::F15e && (kindsMask&3)==3) {
        // 使用外侧前后两对sp21/22/25/26，避开同侧导弹首尾重叠；中间一对不放导弹。
        constexpr int outerPositions[]={6,7,10,11};
        const MountIdentity missile=mountIdentity(read<Addr>(state.physical[0]+0x728));
        for(int index=0;index<4;++index) {
            const Addr component=state.physical[outerPositions[index]];
            const auto identity=mountIdentity(component);
            state.meshOverrides.push_back({identity,mountIdentity(mountBaselineMesh(identity,read<Addr>(component+0x728))),missile});
            state.physical[index+2]=component; // 显示和真实离架共同使用新位置，旧机腹导弹组件隐藏。
        }
    }
    mountBindVisualPositions(state);
}
#include "mount_preview.hpp"
#include "mount_animation_reflection_adapter.hpp"
#include "static_mount_collision.hpp"
#include "mount_rack_collision.hpp"
#include "light_mount.hpp"
#include "cfa_muzzle_visual.hpp"
#include "f22_exterior_bones.hpp"
#include "mount_exterior.hpp"
template<class T> T mountAnimationRead(Addr address) { return read<T>(address); }
#include "mount_animation_product_access.hpp"
#include "mount_animation_publication_tracker.hpp"
#include "mount_animation_host.hpp"
#include "mount_animation_exact_pose.hpp"
#include "mount_animation_runtime.hpp"
#include "local_mount.hpp"
#include "light_mount_animation.hpp"
#include "aircraft_bay_animation.hpp"
// 任务内的内舱弹体只在真实共舱输出全开时显示，不隐藏Body或修改原生发射门槛。
bool mountInternalBayOpen(const FixedMountState& state) {
    if(state.rebuilding || controlledWeaponSwitch)return false; // Init中原生选择与会话暂不同步，不读取旧事务。
    if(mountPreview.active && &state==&mountPreview.layout && !mountPreviewOwnsTask())return true;
    const auto& bay=aircraftBayAnimation;
    if(!bay.bound || !mountSameIdentity(bay.plane,state.plane) || !bay.profile ||
       std::strcmp(bay.profile->body,"pkfa")!=0)return false;
    return aircraftBayCurrent().combined==1.f;
}
// 与已有 restore 使用同一次队列旋转，禁止将物理映射重新按新数组下标编号。
void fixedMountRestoreOrder(const Group& saved,int next) {
    const int count=static_cast<int>(saved.slots.size()); // QAAM 必须保留自己的四槽旋转。
    if(count<=0 || count>static_cast<int>(fixedMount.order.size()))return; // 未适配大队列不影响兵装恢复，视觉接入另行拒绝。
    for(int index=0;index<count;++index)fixedMount.order[index]=saved.slots[(next+index)%count].mountOrdinal;
}
// 按登记的模型容量、装填窗口及备用位置聚合；实际弹药不被改写，未激活SP按游戏时间推进。
void fixedMountRefresh(double time) {
    if(!fixedMountEnabled || fixedMountFailed) { mountPreviewRelease(0,false);fixedMountRelease();return; }
    if(!fixedMount.bound || mountPreviewOwnsTask())return;
    if(mountExterior(fixedMount.aircraft)) { mountExteriorRefresh(time);return; }
    if(mountLocal(fixedMount.aircraft) && GetTickCount64()-fixedMount.localAnimationAudit>=100) {
        mountLocalMaintain(fixedMount);fixedMount.localAnimationAudit=GetTickCount64();
    }
    if(mountLight(fixedMount.aircraft)) {
        if(GetTickCount64()-fixedMount.animatedVisualAudit<100)return;
        mountLightMaintain(fixedMount);
        if(fixedMount.aircraft==MountAircraft::Fa44) {
            // CFA仅登记三副固定架，126个空发射组件没有可消耗弹体，无须捕获ADMM队列来算外观。
            mountApply();fixedMount.animatedVisualAudit=GetTickCount64();return;
        }
    }
    require(mountAlive(fixedMount.plane) && mountAlive(fixedMount.meshManager),"固定挂载：更新时身份失效");
    if(mountAnimated(fixedMount.aircraft))mountAnimationEnsure(fixedMount,1);
    uint64_t mask=0; // 本次所有物理位置的完整结果。
    for(int weapon=0;weapon<session.count;++weapon) {
        if(mountLight(fixedMount.aircraft) && !mountLightMapped(fixedMount.aircraft,fixedMount.weaponKinds[weapon]))continue;
        if((fixedMount.aircraft==MountAircraft::F15e && fixedMount.weaponKinds[weapon]==2) || mountFixedEmitter(fixedMount.aircraft,fixedMount.weaponKinds[weapon]))continue; // 固定炮体常驻，不把效果寿命当作导弹冷却。
        const Group live=weapon==session.active?capture(session.activator,true,time):Group{};
        const Group& group=weapon==session.active?live:session.special[weapon]; // 非活动队列只读引用，不复制弹槽。
        const int ammo=scalarInt(group,0),window=scalarInt(group,8),cursor=scalarInt(group,12); // 原生队列状态。
        const int count=fixedMount.logicalCounts[fixedMount.weaponKinds[weapon]]; // 槽数与模型池一一核对。
        require(group.slots.size()==static_cast<size_t>(count) && dualsp::validAmmoCount(ammo) && window>=0 && window<=count && cursor>=0 && cursor<count,"固定挂载：逻辑队列无效");
        require(time+0.001>=group.capturedTime,"固定挂载：游戏时间回退，等待检查点恢复");
        const double delta=std::max(0.0,time-group.capturedTime); // 与现有冷却缓存使用相同的时间校验。
        const bool showReserve=mountShowReserve(fixedMount.aircraft,fixedMount.kindsMask,fixedMount.weaponKinds[weapon]); // 备用弹外观不表示同时可发射。
        const int visualLimit=showReserve?count:window; // 显示范围不改变原生装填窗口。
        const int available=ammo<0?visualLimit:std::min(visualLimit,ammo); // 布局容量保留，正常库存耗尽时清空消耗弹模型。
        uint64_t weaponMask=0,readyWindow=0,vacancies=0; // 备用库存、实际待发弹和刚发射空位分别聚合。
        for(int distance=0;distance<available;++distance) {
            const Slot& slot=group.slots[(cursor+distance)%count]; // 窗口中的一个逻辑弹位。
            require(slot.mountOrdinal>=0 && slot.mountOrdinal<count,"固定挂载：缓存挂点序号无效");
            const bool ready=distance<window && dualsp::advanceReload(slot.duration,slot.elapsed,delta,true)>=slot.duration;
            if(!showReserve && !ready)continue; // 备用弹按库存占位，已发射空位在下方按真实补装单独扣除。
            const int physical=mountPhysical(fixedMount.weaponKinds[weapon],slot.mountOrdinal); // 与离架路径一致。
            weaponMask|=uint64_t{1}<<physical;
            if(ready)readyWindow|=uint64_t{1}<<physical;
        }
        if(showReserve) {
            // 原生把装填计时交给窗口尾部的新槽；不能把窗口外的旧槽立即当作满载备用弹。
            // 冷却槽向前回溯一个装填窗口，得到它替代的已发射槽；该处保持空位直到真实补装完成。
            for(int distance=0;distance<std::min(window,available);++distance) {
                const Slot& loading=group.slots[(cursor+distance)%count]; // 新进入待发窗口的槽。
                if(dualsp::advanceReload(loading.duration,loading.elapsed,delta,true)>=loading.duration)continue;
                const Slot& fired=group.slots[(cursor+distance+count-window)%count]; // 由原生轮转关系确定空位，不另外计时。
                vacancies|=uint64_t{1}<<mountPhysical(fixedMount.weaponKinds[weapon],fired.mountOrdinal);
            }
            weaponMask=(weaponMask&~vacancies)|readyWindow; // 共用位置已有真实待发弹时，真实待发状态优先。
        }
        mask|=weaponMask;
    }
    const bool changed=mask!=fixedMount.readyMask;
    if(changed)nativeLog(Json{{"event","FIXED_MOUNT_MASK"},{"before",fixedMount.readyMask},{"after",mask},{"game_time",time}}.dump());
    fixedMount.readyMask=mask;
    const auto now=GetTickCount64();
    // 弹体变化仍当轮提交；静态显隐兜底与活动挂架共用100ms巡检，原生显隐钩子仍即时响应。
    if(changed || now-fixedMount.animatedVisualAudit>=100) {
        mountApply();fixedMount.animatedVisualAudit=now;
    }
}
// 在完整兵装事务后绑定；静态组合先匹配真实类，再检查各机体现场核对的布局。
void fixedMountCommit(const Command& command) {
    if(!fixedMountEnabled || fixedMountFailed) { mountPreviewRelease(0,false);fixedMountRelease();return; }
    CfaWorkScope admission(5);
    if(command.op=="checkpoint_save" || command.op=="snapshot") { fixedMountRefresh(command.time);return; }
    if(session.count<1) { fixedMountRelease();return; }
    const MountAircraft aircraft=mountAircraft(command.plane); // 未登记机体继续使用原生当前 SP 模型。
    if(aircraft==MountAircraft::Unsupported || mountNativeOnly(aircraft)) { fixedMountRelease();return; }
    const MountSpecification& specification=mountSpecification(aircraft); // 原版类、布局和资源必须成套匹配。
    std::array<int,3> kinds{}; // 选择槽到真实武器种类的映射。
    unsigned kindsMask=0; // 拒绝重复类，三挂必须恰好包含三种原版兵装。
    for(int index=0;index<session.count;++index) {
        const std::string name=mountName(session.special[index].weaponClass); // 必须使用真实武器类。
        int kind=-1; // 未登记类保留原生显示，不套用近似武器。
        for(int candidate=0;candidate<3;++candidate)
            if(specification.weaponClasses[candidate] && name==specification.weaponClasses[candidate])kind=candidate;
        if(kind<0) { fixedMountRelease();return; }
        if(mountAnimated(aircraft))require(mountName(read<Addr>(session.special[index].weaponClass+0x20))==
            mountAnimatedDefinition(aircraft).at("groups").at(kind).at("packages").at("class").get<std::string>(),
            "动画挂载：原生兵装类包不符");
        kinds[index]=kind;
        require(!(kindsMask&(1u<<kinds[index])),"固定挂载：武器类重复");
        kindsMask|=1u<<kinds[index];
    }
    // 单 QAAM 也接管显隐，保留原生四处库存外观；其他单挂仍由原生管理。
    const bool singleQaam=session.count==1 && aircraft==MountAircraft::Su30sm && kindsMask==4;
    admission.stop();
    if(session.count==1 && !singleQaam) { fixedMountRelease();return; }
    if(mountExpansionNative(aircraft,kindsMask)) { fixedMountRelease();return; }
    if(mountExterior(aircraft) && !(kindsMask&4u)) { fixedMountRelease();return; } // 无8AAM/PLSL的组合完全原生。
    if(mountLight(aircraft) && mountLightNative(aircraft,kindsMask)) { fixedMountRelease();return; }
    require(singleQaam || (session.count==2 && (kindsMask==3 || kindsMask==5 || kindsMask==6)) ||
        (session.count==3 && kindsMask==7),"固定挂载：已识别兵装组合与槽数不符");
        if(!fixedMount.bound || fixedMount.plane.address!=command.plane || fixedMount.aircraft!=aircraft || fixedMount.kindsMask!=kindsMask) {
        const auto order=fixedMount.order; // restore 可能已为检查点的新实例重建顺序。
        const auto pending=fixedMount.pendingReadyMask; // 同时保留新池一次性装填完成事件。
        if(!fixedMountRelease())return; // 未全部归还时不能覆盖待恢复对象或重新接管。
        fixedMount.order=order;fixedMount.pendingReadyMask=pending;
        fixedMount.kindsMask=kindsMask;
        fixedMount.aircraft=aircraft;
        fixedMount.logicalCounts=specification.logicalCounts;
        fixedMount.physicalCount=specification.physicalCount;
        fixedMount.rackMask=mountRackMask(aircraft,kindsMask);
        mountInstallHooks();
        mountBindVisuals(fixedMount,command.plane,command.activator);
        fixedMount.bound=true;
        nativeLog(Json{{"event","FIXED_MOUNT_BOUND"},{"aircraft",specification.code},{"kinds_mask",kindsMask},
            {"rack_mask",fixedMount.rackMask},{"physical_positions",fixedMount.physicalCount}}.dump()); // 位置数不冒充可发射弹数。
    }
    if(mountExpanded(aircraft) || mountAnimated(aircraft) || mountExterior(aircraft) || mountLight(aircraft))for(int slot=0;slot<session.count;++slot) {
        const auto count=session.special[slot].slots.size();
        require(count>0 && count<=fixedMount.order.size(),"扩展挂载：原生队列长度超出保护边界");
        fixedMount.logicalCounts[kinds[slot]]=static_cast<int>(count); // 使用已捕获的真实队列长度，绝不把Socket数写回游戏。
    }
    fixedMount.weaponKinds=kinds;
    for(int index=0;index<session.count;++index)fixedMount.weaponClasses[index]=session.special[index].weaponClass;
    if(command.op=="setup" || command.op=="resupply_setup" || command.op=="debug_setup" || command.op=="resupply")for(size_t i=0;i<fixedMount.order.size();++i)fixedMount.order[i]=static_cast<int>(i);
    if(aircraft==MountAircraft::F35c && (kindsMask&4) &&
       (command.op=="setup" || command.op=="resupply_setup" || command.op=="debug_setup" || command.op=="resupply" ||
        command.op=="switch" || command.op=="checkpoint_restore" || command.op=="debug_mount")) {
        // 原生初始化暂停非当前挂架并关闭 Tick；持久显示的 SWP2 必须继续读取 Swcc，才能随腹舱收回。
        const Addr rack=read<Addr>(command.plane+0x1b90); // 已在绑定时核对骨架类型和资源。
        fixedMount.animatedRack=mountIdentity(rack);
        fixedMount.nativeRackPaused=(read<unsigned>(rack+0x9b0)&2)!=0;
        fixedMount.nativeRackTick=read<unsigned char>(rack+0x3f)!=0;
        mountSetRackAnimation(rack,false,true);
    }
    fixedMount.rebuilding=false;
    if(mountLocal(aircraft))mountLocalMaintain(fixedMount);
    { CfaWorkScope cost(6);mountLightAnimationCommit(); } // CFA公共进度仍由原生推进。
    { CfaWorkScope cost(7);mountCfaMuzzlePrune();mountCfaMuzzleSync(true); } // 重建提交必须重新发现炮口，不复用旧组件资格。
    fixedMount.animatedVisualAudit=0; // 兵装重建后所有布局立即刷新，不能被稳定期巡检限频挡住。
    fixedMountRefresh(command.time);
}
// 核心只传递重建上下文，不直接改写挂载/舱门内部状态。
struct MountRebuildSnapshot {
    ::dualsp::common_bay::Snapshot common;
};
MountRebuildSnapshot fixedMountBeforeRebuild(const Command& command,int nativeConfig,const Session& current) {
    MountRebuildSnapshot saved;
    mountPreviewRelease(0,false); // 任何真实兵装重建都先结束只控制外观的预览。
    if(!fixedMountEnabled || fixedMountFailed)return saved;
    try {
        if(mountExterior(fixedMount.aircraft))mountExteriorBeforeRebuild(fixedMount);
        if(mountLight(fixedMount.aircraft))mountLightBeforeRebuild(command); // CFA同布局保骨租约，动画调度仍归还后重采。
        if(mountLocal(fixedMount.aircraft))mountLocalBeforeRebuild(fixedMount); // Init期间以真实原生请求形成下一次恢复基线。
        mountLightAnimationBeforeRebuild(command,nativeConfig);
        require(!mountAnimationPendingSwitch.valid,"动画挂载：上一切换尚未完成，拒绝覆盖快照");
        if(mountAnimationHost.controller.active() && mountAnimationHost.binding.scene==1 &&
           mountAnimationHost.binding.plane.address==command.plane) {
            if(command.op=="switch" && read<unsigned char>(command.activator+0x2a1)!=0) {
                const auto status=mountAnimationHost.controller.captureSwitch(mountAnimationHost,mountAnimationHost.binding,
                    current.configs[current.active],nativeConfig,command.request,saved.common);
                require(status==::dualsp::common_bay::Error::None || status==::dualsp::common_bay::Error::NativeTransition,
                    "动画挂载：同舱切换快照失败");
            } else if(command.op!="switch")require(mountAnimationRelease(command.plane,1),"动画挂载：重建前归还失败");
        }
        fixedMount.rebuilding=true;
        fixedMount.pendingReadyMask.reset();
        return saved;
    }catch(const std::exception& error) { fixedMountFail(error.what());return {}; }
}
void fixedMountAfterRebuild(const MountRebuildSnapshot& saved) {
    if(!fixedMountEnabled || fixedMountFailed)return;
    try {
        if(mountExterior(fixedMount.aircraft))mountExteriorObserveInit(fixedMount);
        if(saved.common.valid) {
            // Init完成并不代表原生SP已恢复；keepArmed仍要执行完整切换，等它结束后再还快照。
            mountAnimationQueueSwitch(saved.common);
        } else if(mountAnimationHost.controller.active())require(mountAnimationHost.maintain(),"动画挂载：重建后维持失败");
    }catch(const std::exception& error) { fixedMountFail(error.what()); }
}
void fixedMountRecordReload(bool special,int slot,float logicalElapsed,float elapsed,float duration) {
    if(special && slot<static_cast<int>(fixedMount.order.size()) && logicalElapsed>=duration && elapsed<duration)
        fixedMount.pendingReadyMask.set(static_cast<size_t>(slot));
}
void fixedMountUpdate(Addr plane,Addr sessionPlane,double time) {
    if(!fixedMountEnabled || fixedMountFailed) { mountPreviewRelease(0,false);fixedMountRelease();return; }
    if(fixedMount.bound) {
        require(fixedMount.plane.address==plane && sessionPlane==plane,"固定挂载玩家会话不一致");
        fixedMountRefresh(time);
    }
}
