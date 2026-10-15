// Su-30SM / F-35C / F-15E 固定挂载研究原型；仅由独立候选构建启用。
// 包含位置在核心私有命名空间中，复用已有内存校验、弹药缓存和日志。
#pragma once

struct MountIdentity { Addr address{}; int index{},serial{}; }; // 地址和对象序列号共同标识实例。
enum class MountAircraft { Unsupported, Su30sm, F35c, F15e }; // 只接入明确登记的机型和组合。
struct MountSpecification {
    const char* code; // 原版资源前缀。
    std::array<const char*,3> weaponClasses; // 真实武器类；空项表示该组合未适配。
    std::array<int,3> logicalCounts,poolCounts; // 逻辑队列与持久组件池分别核对。
    std::array<size_t,3> rackOffsets; // 各机型现场反射确认的字段位置。
    std::array<const char*,3> meshes; // 空模型仅用于已登记的固定 TLS 发射点。
    int physicalCount; // 可用于取位的全部候选位置数。
};
// 共用身份检查，布局映射仍按机型明确处理。
const MountSpecification& mountSpecification(MountAircraft aircraft) {
    static const MountSpecification su30{"su30sm",{"plwp_6aam_r0_C","plwp_lagm_r0_C","plwp_qaam_r0_C"},
        {6,6,4},{6,4,6},{0x1ba0,0x1b98,0x1b90},{"w_4aam_r0_SM","w_qaam_r0_SM","w_lasm_r0_SM"},6}; // 原型机。
    static const MountSpecification f35{"f35c",{"plwp_sod_a0_C","plwp_8agm_a0_C","plwp_4aam_a0_C"},
        {2,12,6},{2,6,12},{0x1ba0,0x1b90,0x1b98},{"w_sod_a0_closed_SM","w_4aam_a0_SM","w_8agm_a0_SM"},20}; // SWP2 保留骨架动画和原生挂点。
    static const MountSpecification f15{"f15e",{"plwp_6aam_a0_C","plwp_sffs_a0_C","plwp_tls_x0_C"},
        {6,12,1},{6,12,1},{0x1ba0,0x1b98,0x1b90},{"w_4aam_a0_SM","w_sffs_a0_closed_SM",nullptr},19}; // TLS 炮体在挂架内。
    switch(aircraft) {
        case MountAircraft::Su30sm:return su30;
        case MountAircraft::F35c:return f35;
        case MountAircraft::F15e:return f15;
        default:throw std::runtime_error("固定挂载：机型未登记");
    }
}
struct MountVisual {
    MountIdentity identity; // 被本原型管理的一个模型或挂架。
    int pool{},index{}; // pool=0/1/2 对应 SWP1/2/3，index=-1 表示挂架本体。
    bool nativeVisible{},nativeHidden{}; // 原生最后请求的状态，用于明确退出时归还显示。
};
struct FixedMountState {
    bool bound{},rebuilding{}; // 重建期间仅固定外观，不改变原生临时武器池取位。
    MountIdentity plane,manager,meshManager; // 限定当前玩家，其他飞机全部直通。
    MountIdentity root,weaponBase; // 现场确认的共同祖先：RootComponent → WeaponMeshBase → SWP。
    bool ancestorVisible{true},ancestorHidden{}; // 祖先最近一次向下传播的整组显示请求。
    std::array<Addr,3> weaponClasses{}; // 会话最多三个选择槽的真实武器类。
    MountAircraft aircraft{MountAircraft::Unsupported}; // 同一会话只接入一个已匹配的机体。
    std::array<int,3> weaponKinds{}; // 对应各机型规格中的武器顺序，与用户选择顺序无关。
    std::array<int,3> logicalCounts{6,6,4}; // 原生队列长度，F-35C 使用 2/12/6；不改变装填窗口。
    unsigned kindsMask{}; // 每种真实武器占一位，组合变化时重新绑定布局。
    std::array<int,12> order{0,1,2,3,4,5,6,7,8,9,10,11}; // 当前原生槽对应的持久挂点序号。
    std::array<Addr,20> physical{}; // 最大为 F-35C 两处 SOD、十二处 8AGM 和六处随舱门移动的 4AAM。
    int physicalCount{6}; // 只遍历本机布局，不把空余容量视为有效位置。
    unsigned rackMask{4}; // 明确选择挂架，固定炮体不按待发弹药显隐。
    std::vector<MountVisual> visuals; // 已核对的武器及挂架显隐；活动骨架保留原父链、动画和 Socket。
    unsigned readyMask{}; // 实际物理位置的聚合就绪状态。
    unsigned pendingReadyMask{}; // 已就绪但等待新池完成原生装填事件的逻辑槽。
    std::string error; // 钩子错误交回 Lua，不能在游戏调用栈中抛出异常。
} fixedMount;

struct F35CoverState {
    MountIdentity plane; // 动画通道跟随机体实例，不跟随当前武器池。
    Addr parts{}; // 飞行引擎内的 PartsAnimation 状态，由原生包装器定位。
    int selection{-1}; // 上一帧实际 SP 配置，用于同舱切换时继承进度。
    float seconds{},closed{}; // 从当前 build 的原生插值常量读取。
} f35Cover;

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
std::string mountNameIndex(unsigned index) {
    require(index<4000000,"固定挂载：名称索引越界");
    const Addr base=reinterpret_cast<Addr>(GetModuleHandleW(nullptr)); // 当前主映像。
    const Addr names=read<Addr>(base+0x3bc7070); // 本 build 的分块名称表。
    const Addr chunk=read<Addr>(names+(index/0x4000)*8); // 每块 16384 项。
    const Addr entry=read<Addr>(chunk+(index%0x4000)*8); // 名称条目含 16 字节头。
    require((read<unsigned char>(entry)&1)==0,"固定挂载：预期的资源名不是 ASCII");
    std::string name; // 最长读取 240 字节，不跨越未知名称空间。
    for(int offset=0;offset<240;++offset) {
        const char value=read<char>(entry+16+offset); // 单字节读取同时检查页边界。
        if(!value)return name;
        name.push_back(value);
    }
    throw std::runtime_error("固定挂载：资源名没有终止符");
}
// 资源身份使用对象本名，不依赖用户配置的机体编号。
std::string mountName(Addr object) { return mountNameIndex(read<unsigned>(object+0x18)); }

// 对象表只做按索引访问，避免在每次更新中扫描全局对象。
MountIdentity mountIdentity(Addr object) {
    const Addr base=reinterpret_cast<Addr>(GetModuleHandleW(nullptr)); // 当前 UObject 表。
    const int index=read<int>(object+0xc),count=read<int>(base+0x3bcfdb0+12); // 实例索引和表长度。
    require(index>=0 && index<count && count<3000000,"固定挂载：对象索引无效");
    const Addr entry=read<Addr>(base+0x3bcfdb0)+static_cast<Addr>(index)*24; // 每项含地址与序列号。
    require(read<Addr>(entry)==object,"固定挂载：对象表身份不匹配");
    return {object,index,read<int>(entry+16)};
}
// 先核对对象表，再读取实例，拒绝已销毁对象及地址复用。
bool mountAlive(const MountIdentity& identity) {
    if(!identity.address)return false;
    const Addr base=reinterpret_cast<Addr>(GetModuleHandleW(nullptr)); // 对象表全局地址。
    const int count=read<int>(base+0x3bcfdb0+12); // 当前长度。
    require(count>=0 && count<3000000,"固定挂载：对象表长度无效");
    if(identity.index<0 || identity.index>=count)return false;
    const Addr entry=read<Addr>(base+0x3bcfdb0)+static_cast<Addr>(identity.index)*24; // 当前条目。
    return read<Addr>(entry)==identity.address && read<int>(entry+16)==identity.serial;
}
// 涂装派生类必须继承已登记原版机体的完整资源类。
MountAircraft mountAircraft(Addr plane) {
    Addr cls=read<Addr>(plane+0x10); // 从真实实例沿类父链检查。
    for(int depth=0;cls && depth<32;++depth,cls=read<Addr>(cls+0x30)) {
        const std::string name=mountName(cls); // 只有已知类名才进一步检查所属资源包。
        if(name=="AcePlayerPawn_su30sm_C" && mountName(read<Addr>(cls+0x20))=="/Game/Blueprint/Player/Pawn/AcePlayerPawn_su30sm")return MountAircraft::Su30sm;
        if(name=="AcePlayerPawn_f35c_C" && mountName(read<Addr>(cls+0x20))=="/Game/Blueprint/Player/Pawn/AcePlayerPawn_f35c")return MountAircraft::F35c;
        if(name=="AcePlayerPawn_f15e_C" && mountName(read<Addr>(cls+0x20))=="/Game/Blueprint/Player/Pawn/AcePlayerPawn_f15e")return MountAircraft::F15e;
    }
    return MountAircraft::Unsupported;
}
// 绑定成功后，活动组采样继承持久序号；原生初始化期间的新池使用新顺序。
int fixedMountOrdinal(Addr weaponClass,int index) {
    if(fixedMount.bound && !fixedMount.rebuilding && index>=0 && index<fixedMount.logicalCounts[fixedMount.weaponKinds[session.active]] &&
       weaponClass==fixedMount.weaponClasses[session.active])return fixedMount.order[index];
    return index;
}
// 只识别本次重建主动写入的 nextafter 门槛，不用宽泛误差把真正未装填的弹判为就绪。
float fixedMountElapsed(int index,float duration,float elapsed) {
    if(!fixedMount.bound || fixedMount.rebuilding || index<0 || index>=fixedMount.logicalCounts[fixedMount.weaponKinds[session.active]])return elapsed;
    const unsigned bit=1u<<index; // 原生数组下标，与本次 restore 的记录一致。
    if(!(fixedMount.pendingReadyMask&bit))return elapsed;
    if(duration>0 && elapsed==std::nextafter(duration,0.0f))return duration;
    fixedMount.pendingReadyMask&=~bit; // 原生已完成装填或发射，后续完全使用实际计时。
    return elapsed;
}
// 所有可视组件均保留原父链；只使用一个挂架网格，避免重复几何。
bool mountDesired(const MountVisual& visual) {
    const bool master=read<unsigned char>(fixedMount.meshManager.address+0x120)!=0; // 原生总显隐。
    if(!master || !fixedMount.ancestorVisible || fixedMount.ancestorHidden)return false;
    if(visual.index<0)return (fixedMount.rackMask&(1u<<visual.pool))!=0; // 按已核对的挂架组合显示。
    if(fixedMount.aircraft==MountAircraft::F15e && visual.pool==2)return (fixedMount.kindsMask&4)!=0; // 空 TLS 发射组件与炮体同寿命。
    for(int position=0;position<fixedMount.physicalCount;++position) // 物理位置表同时用于模型显隐和离架变换。
        if(fixedMount.physical[position]==visual.identity.address)return (fixedMount.readyMask&(1u<<position))!=0;
    return false;
}
// 只在状态发生变化时调用引擎更新；不逐帧强制反隐藏。
void mountApply() {
    for(const auto& visual:fixedMount.visuals) {
        require(mountAlive(visual.identity),"固定挂载：组件已销毁");
        const Addr object=visual.identity.address; // 已验证的当前组件。
        const auto flags=read<unsigned char>(object+0x1bc); // 两个反射确认的显隐位。
        const bool desired=mountDesired(visual); // 聚合结果和原生总显隐共同决定。
        if(flags&0x20)mountOriginalHidden(reinterpret_cast<void*>(object),false,0);
        if(bool(flags&0x10)!=desired)mountOriginalVisibility(reinterpret_cast<void*>(object),desired,0);
    }
}
// 退出固定挂载时归还原生最后请求的显示；失效对象不再解引用。
void fixedMountRelease() {
    f35Cover={}; // 生命周期结束或重建后由当前飞机重新绑定动画状态。
    fixedMount.bound=false; // 先直通，避免归还显示的调用再次被过滤。
    for(const auto& visual:fixedMount.visuals)if(mountAlive(visual.identity)) {
        mountOriginalHidden(reinterpret_cast<void*>(visual.identity.address),visual.nativeHidden,0);
        mountOriginalVisibility(reinterpret_cast<void*>(visual.identity.address),visual.nativeVisible,0);
    }
    fixedMount={};
}
// 明确记录错误并停止原型；下次 Lua 更新继续上报，不能伪装为成功。
void fixedMountFail(const char* message) noexcept {
    const std::string failure=message; // 清理会销毁状态字符串，先取得独立副本。
    try {
        nativeLog(std::string("FIXED_MOUNT_ERROR ")+failure);
        fixedMountRelease();fixedMount.error=failure;
    }catch(const std::exception& cleanup) {
        fixedMount.bound=false;
        fixedMount.error=failure+"；归还显示失败："+cleanup.what();
        nativeLog(std::string("FIXED_MOUNT_RELEASE_ERROR ")+fixedMount.error);
    }
}
// 原生仅把公共 SP 进度写给当前通道，切换配置后旧通道会冻结；在同一动画更新中补齐各舱的连续状态。
void mountPartsUpdate(void* object,float delta,void* input) {
    if(GetCurrentThreadId()!=mountGameThread.load() || !f35Cover.parts ||
       reinterpret_cast<Addr>(object)!=f35Cover.parts || !input) {
        mountOriginalPartsUpdate(object,delta,input);return;
    }
    bool called=false; // 错误也不能重复执行整套原生动画更新。
    try {
        require(mountAlive(f35Cover.plane),"F-35C 舱门：机体身份失效");
        const Addr parts=f35Cover.parts; // 现场核对的状态基址，不使用其他机型偏移猜测。
        require(read<Addr>(f35Cover.plane.address+0x1410)+0xf90==parts,"F-35C 舱门：引擎实例改变");
        require(reinterpret_cast<Addr>(input)==parts+0xc,"F-35C 舱门：原生输入结构改变");
        require(std::isfinite(delta) && delta>=0,"F-35C 舱门：帧间隔无效");
        const int selected=read<int>(parts+0x178); // 与 PlaneSWP 同步的原生配置编号。
        require(selected>=0 && selected<3,"F-35C 舱门：配置编号无效");
        std::array<float,3> before{}; // 三个独立舱门通道在本帧开始时的姿态。
        for(int index=0;index<3;++index) {
            before[index]=read<float>(parts+0x180+index*4);
            require(std::isfinite(before[index]) && before[index]>=-1 && before[index]<=1,"F-35C 舱门：动画进度越界");
        }
        if(selected!=f35Cover.selection && selected!=2) {
            // SOD 与 4AAM 的共有舱体保持已打开的最大进度；从外置 8AGM 切回也不跳回动画起点。
            write<float>(parts+0x17c,std::max(before[0],before[1]));
        }
        called=true;mountOriginalPartsUpdate(object,delta,input);
        const unsigned options=read<unsigned>(reinterpret_cast<Addr>(input)+0x58); // 原生同一分支的开关速率选项。
        const Addr base=reinterpret_cast<Addr>(GetModuleHandleW(nullptr)); // 与原生公式使用同一常量。
        const float speed=(options&0x40)?read<float>(base+0x257beec):1.f;
        require(std::isfinite(speed) && speed>0,"F-35C 舱门：原生速率无效");
        for(int index=0;index<3;++index)if(index!=selected) {
            // 非当前舱仍按原生速率收拢，不锁死姿态，也不重新启动动画实例。
            write<float>(parts+0x180+index*4,std::max(f35Cover.closed,before[index]-delta/f35Cover.seconds*speed));
        }
        const float first=read<float>(parts+0x180),second=read<float>(parts+0x184),third=read<float>(parts+0x188); // 当前通道保留原生发射/开关决策。
        write<float>(parts+0x18c,std::max(second,third)); // 原生 SWP2/3 共用输出。
        write<float>(parts+0x190,std::max(first,third)); // 原生 SWP1/3 共用输出。
        write<float>(parts+0x194,std::max(first,second)); // 原生 SWP1/2 共用输出。
        f35Cover.selection=selected;
    }catch(const std::exception& error) {
        fixedMountFail(error.what());
        if(!called)mountOriginalPartsUpdate(object,delta,input);
    }
}
// 原生显隐请求仍记账，但固定布局的组件由聚合状态决定；挂架请求不传播到武器。
void mountFilterVisibility(void* object,bool requested,unsigned char propagation,bool hidden) {
    const auto original=hidden?mountOriginalHidden:mountOriginalVisibility; // 两种引擎接口分别处理。
    if(GetCurrentThreadId()==mountGameThread.load() && fixedMount.bound)try {
        const Addr address=reinterpret_cast<Addr>(object); // 先处理整架飞机/整组挂载的传播请求。
        if(propagation==2 && (address==fixedMount.root.address || address==fixedMount.weaponBase.address)) {
            require(mountAlive(fixedMount.root) && mountAlive(fixedMount.weaponBase),"固定挂载：祖先组件身份失效");
            (hidden?fixedMount.ancestorHidden:fixedMount.ancestorVisible)=requested;
            // 原生随后遍历后代并以参数 0 回调同一入口，固定模型将服从这次整组请求。
        }
        for(auto& visual:fixedMount.visuals)if(visual.identity.address==reinterpret_cast<Addr>(object)) {
            require(mountAlive(visual.identity),"固定挂载：显隐对象身份失效");
            (hidden?visual.nativeHidden:visual.nativeVisible)=requested;
            // 原生挂架传播语义保留在归还记录中，固定显示期间不覆盖子模型。
            if(visual.index<0 && propagation==2)for(auto& child:fixedMount.visuals)
                if(child.pool==visual.pool && child.index>=0)(hidden?child.nativeHidden:child.nativeVisible)=requested;
            original(object,hidden?false:mountDesired(visual),0);
            return;
        }
    }catch(const std::exception& error) { fixedMountFail(error.what()); }
    original(object,requested,propagation); // 非本机模型或明确退出后的原生行为。
}
// 两个入口保留引擎传播枚举，不将其简化为 bool ABI。
void mountVisibility(void* object,bool visible,unsigned char propagation) { mountFilterVisibility(object,visible,propagation,false); }
void mountHidden(void* object,bool hidden,unsigned char propagation) { mountFilterVisibility(object,hidden,propagation,true); }

// 同一个映射同时用于显示和发射；QAAM 四槽、6AAM/LAGM 六槽均按原生顺序轮转。
int mountPhysical(int kind,int ordinal) {
    require(kind>=0 && kind<3,"固定挂载：未知武器种类");
    const int count=fixedMount.logicalCounts[kind]; // 逻辑数量不因物理位置减少而改变。
    require(ordinal>=0 && ordinal<count,"固定挂载：持久挂点序号无效");
    if(fixedMount.aircraft==MountAircraft::F35c) {
        // 同选 SOD 时只使用最内侧两个活动点 sp25/26，开舱姿态与 SOD 的模型范围分离。
        // 六逻辑槽和四发窗口保持原样，多枚弹可依序从同一物理位置离架；收拢过程仍须实玩观察。
        if(kind==2 && (fixedMount.kindsMask&1))return 18+ordinal%2;
        return (kind==0?0:kind==1?2:14)+ordinal; // 其余组合保留自己的原生位置和父链。
    }
    if(fixedMount.aircraft==MountAircraft::F15e) {
        if(kind==2)return 18; // 固定激光发射点保持原位。
        if(kind==1)return 6+ordinal; // SFFS 保留十二个原生位置。
        return (fixedMount.kindsMask&2)?ordinal%2:ordinal; // 同选 SFFS 时只用两处外侧导弹位，避开内侧炸弹。
    }
    if(kind==1)return fixedMount.kindsMask==6?2+(ordinal<4?ordinal:ordinal-2):4+ordinal%2;
    if(kind==2)return ordinal%2;
    require(kind==0,"固定挂载：未知武器种类");
    if(fixedMount.kindsMask==5)return 2+(ordinal<4?ordinal:ordinal-2); // QAAM＋6AAM 利用内翼及机腹四处。
    return (fixedMount.kindsMask&4)?2+ordinal%2:(ordinal<4?ordinal:ordinal-2); // 三挂两处；原四加二保持原样。
}
// 逻辑槽映射物理位置，不修改原生隐藏/恢复索引，也不改变六发数量。
void mountCopyTransform(void* output,int slot) {
    const int count=fixedMount.logicalCounts[fixedMount.weaponKinds[session.active]]; // 当前武器自己的逻辑槽数。
    require(slot>=0 && slot<count,"固定挂载：发射槽超出原生范围");
    const int ordinal=fixedMount.order[slot],kind=fixedMount.weaponKinds[session.active]; // 已重排的持久序号。
    require(ordinal>=0 && ordinal<count,"固定挂载：持久挂点序号无效");
    const int physical=mountPhysical(kind,ordinal); // 与显隐聚合共用映射。
    const Addr component=fixedMount.physical[physical]; // 与可见模型一致的世界变换来源。
    const auto visual=std::find_if(fixedMount.visuals.begin(),fixedMount.visuals.end(),[&](const MountVisual& item){return item.identity.address==component;}); // 验证目的模型仍属于本次出击。
    require(visual!=fixedMount.visuals.end() && mountAlive(visual->identity),"固定挂载：目的模型身份失效");
    checkMemory(component+0x180,48);checkMemory(reinterpret_cast<Addr>(output),48);
    std::memcpy(output,reinterpret_cast<void*>(component+0x180),48);
}
// 直接发射路径使用逻辑槽取位；普通导弹、其他飞机及重建过程保持原生。
bool mountTransform(void* manager,void* output,int group,int slot) {
    if(GetCurrentThreadId()==mountGameThread.load() && fixedMount.bound && !fixedMount.rebuilding && group==2 &&
       reinterpret_cast<Addr>(manager)==fixedMount.meshManager.address)try {
        require(mountAlive(fixedMount.plane) && mountAlive(fixedMount.meshManager),"固定挂载：发射身份失效");
        mountCopyTransform(output,slot);return true;
    }catch(const std::exception& error) { fixedMountFail(error.what()); }
    return mountOriginalTransform(manager,output,group,slot);
}
// 先由原生选出就绪槽，再替换结果，覆盖原生使用缓存变换的另一条发射路径。
void mountSelection(void* manager,unsigned char group,void* output,int* selected) {
    mountOriginalSelection(manager,group,output,selected);
    if(GetCurrentThreadId()==mountGameThread.load() && fixedMount.bound && !fixedMount.rebuilding && group==2 && *selected>=0 &&
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
        for(const auto& entry:entries) {
            void* target=reinterpret_cast<void*>(base+entry.rva); // 当前已核对入口。
            require(MH_CreateHook(target,entry.hook,entry.original)==MH_OK,"固定挂载：创建钩子失败");
            created.push_back(target);
        }
        for(void* target:created)require(MH_QueueEnableHook(target)==MH_OK,"固定挂载：安排启用钩子失败");
        require(MH_ApplyQueued()==MH_OK,"固定挂载：启用钩子失败");
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
// 与已有 restore 使用同一次队列旋转，禁止将物理映射重新按新数组下标编号。
void fixedMountRestoreOrder(const Group& saved,int next) {
    const int count=static_cast<int>(saved.slots.size()); // QAAM 必须保留自己的四槽旋转。
    if(count!=1 && count!=2 && count!=4 && count!=6 && count!=12)return; // 固定 TLS 也保留自己的单槽身份。
    for(int index=0;index<count;++index)fixedMount.order[index]=saved.slots[(next+index)%count].mountOrdinal;
}
// 根据真实弹药和允许装填的窗口聚合六处显示；未激活 SP 使用游戏时间推进。
void fixedMountRefresh(double time) {
    require(fixedMount.error.empty(),fixedMount.error.c_str());
    if(!fixedMount.bound)return;
    require(mountAlive(fixedMount.plane) && mountAlive(fixedMount.meshManager),"固定挂载：更新时身份失效");
    unsigned mask=0; // 本次所有物理位置的完整结果。
    for(int weapon=0;weapon<session.count;++weapon) {
        if(fixedMount.aircraft==MountAircraft::F15e && fixedMount.weaponKinds[weapon]==2)continue; // 固定炮体常驻，不把效果寿命当作导弹冷却。
        const Group group=weapon==session.active?capture(session.activator,true,time):session.special[weapon]; // 当前或缓存队列。
        const int ammo=scalarInt(group,0),window=scalarInt(group,8),cursor=scalarInt(group,12); // 原生队列状态。
        const int count=fixedMount.logicalCounts[fixedMount.weaponKinds[weapon]]; // 槽数与模型池一一核对。
        require(group.slots.size()==static_cast<size_t>(count) && ammo>=-1 && window>=0 && window<=count && cursor>=0 && cursor<count,"固定挂载：逻辑队列无效");
        require(time+0.001>=group.capturedTime,"固定挂载：游戏时间回退，等待检查点恢复");
        const double delta=std::max(0.0,time-group.capturedTime); // 与现有冷却缓存使用相同的时间校验。
        const bool showReserve=(fixedMount.aircraft==MountAircraft::Su30sm && fixedMount.kindsMask==6 && fixedMount.weaponKinds[weapon]==1) ||
            (fixedMount.aircraft==MountAircraft::F15e && fixedMount.weaponKinds[weapon]==1); // 显示实际库存中的备用弹，不表示同时可发射。
        const int visualLimit=showReserve?count:window; // 显示范围不改变原生装填窗口。
        const int available=ammo<0?visualLimit:std::min(visualLimit,ammo); // 最后一发仍只分配一个物理位置。
        unsigned weaponMask=0,readyWindow=0,vacancies=0; // 备用库存、实际待发弹和刚发射空位分别聚合。
        for(int distance=0;distance<available;++distance) {
            const Slot& slot=group.slots[(cursor+distance)%count]; // 窗口中的一个逻辑弹位。
            require(slot.mountOrdinal>=0 && slot.mountOrdinal<count,"固定挂载：缓存挂点序号无效");
            if(distance<window && dualsp::advanceReload(slot.duration,slot.elapsed,delta,true)<slot.duration)continue; // 待发窗口按真实冷却，备用槽仅表示库存。
            const int physical=mountPhysical(fixedMount.weaponKinds[weapon],slot.mountOrdinal); // 与离架路径一致。
            weaponMask|=1u<<physical;
            if(distance<window)readyWindow|=1u<<physical;
        }
        if(fixedMount.aircraft==MountAircraft::Su30sm && fixedMount.kindsMask==6 && fixedMount.weaponKinds[weapon]==1) {
            // LAGM 发射后，原生把装填计时交给窗口尾部的新槽；不能把窗口外的旧槽立即当作满载备用弹。
            // 冷却槽向前回溯一个装填窗口，得到它替代的已发射槽；该处保持空位直到真实补装完成。
            for(int distance=0;distance<std::min(window,available);++distance) {
                const Slot& loading=group.slots[(cursor+distance)%count]; // 新进入待发窗口的槽。
                if(dualsp::advanceReload(loading.duration,loading.elapsed,delta,true)>=loading.duration)continue;
                const Slot& fired=group.slots[(cursor+distance+count-window)%count]; // 由原生轮转关系确定空位，不另外计时。
                vacancies|=1u<<mountPhysical(fixedMount.weaponKinds[weapon],fired.mountOrdinal);
            }
            weaponMask=(weaponMask&~vacancies)|readyWindow; // 共用位置已有真实待发弹时，真实待发状态优先。
        }
        mask|=weaponMask;
    }
    if(mask!=fixedMount.readyMask)nativeLog(Json{{"event","FIXED_MOUNT_MASK"},{"before",fixedMount.readyMask},{"after",mask},{"game_time",time}}.dump());
    fixedMount.readyMask=mask;mountApply();
}
// 在完整兵装事务后绑定；静态组合先匹配真实类，再检查各机体现场核对的布局。
void fixedMountCommit(const Command& command) {
    require(fixedMount.error.empty(),fixedMount.error.c_str());
    if(command.op=="checkpoint_save" || command.op=="snapshot") { fixedMountRefresh(command.time);return; }
    if(session.count<2) { fixedMountRelease();return; }
    const MountAircraft aircraft=mountAircraft(command.plane); // 未登记机体继续使用原生当前 SP 模型。
    if(aircraft==MountAircraft::Unsupported) { fixedMountRelease();return; }
    const MountSpecification& specification=mountSpecification(aircraft); // 原版类、布局和资源必须成套匹配。
    std::array<int,3> kinds{}; // 选择槽到真实武器种类的映射。
    unsigned kindsMask=0; // 拒绝重复类，三挂必须恰好包含三种原版兵装。
    for(int index=0;index<session.count;++index) {
        const std::string name=mountName(session.special[index].weaponClass); // 必须使用真实武器类。
        int kind=-1; // 未登记类保留原生显示，不套用近似武器。
        for(int candidate=0;candidate<3;++candidate)
            if(specification.weaponClasses[candidate] && name==specification.weaponClasses[candidate])kind=candidate;
        if(kind<0) { fixedMountRelease();return; }
        kinds[index]=kind;
        require(!(kindsMask&(1u<<kinds[index])),"固定挂载：武器类重复");
        kindsMask|=1u<<kinds[index];
    }
    require((session.count==2 && (kindsMask==3 || kindsMask==5 || kindsMask==6)) ||
        (session.count==3 && kindsMask==7),"固定挂载：已识别兵装组合与槽数不符");
    const bool hasQaam=aircraft==MountAircraft::Su30sm && (kindsMask&4)!=0; // 仅 Su-30SM 的第三种武器是 QAAM。
    if(!fixedMount.bound || fixedMount.plane.address!=command.plane || fixedMount.aircraft!=aircraft || fixedMount.kindsMask!=kindsMask) {
        const auto order=fixedMount.order; // restore 可能已为检查点的新实例重建顺序。
        const unsigned pending=fixedMount.pendingReadyMask; // 同时保留新池一次性装填完成事件。
        fixedMountRelease();fixedMount.order=order;fixedMount.pendingReadyMask=pending;
        fixedMount.kindsMask=kindsMask;
        fixedMount.aircraft=aircraft;
        fixedMount.logicalCounts=specification.logicalCounts;
        fixedMount.physicalCount=specification.physicalCount;
        fixedMount.rackMask=4;
        if(aircraft==MountAircraft::F35c)fixedMount.rackMask=(kindsMask&1)|((kindsMask&2)<<1)|((kindsMask&4)>>1); // 武器种类次序与原生挂架次序分开。
        if(aircraft==MountAircraft::F15e)fixedMount.rackMask=(kindsMask&2)?(kindsMask&~1u):kindsMask; // 同选 SFFS 时不用只承载内侧 6AAM 的 SWP1 网格。
        mountInstallHooks();
        fixedMount.plane=mountIdentity(command.plane);
        fixedMount.manager=mountIdentity(read<Addr>(command.activator+0x260));
        fixedMount.meshManager=mountIdentity(read<Addr>(fixedMount.manager.address+0x150));
        fixedMount.root=mountIdentity(read<Addr>(command.plane+0x160));
        fixedMount.weaponBase=mountIdentity(read<Addr>(command.plane+0x1ae0));
        require(read<Addr>(fixedMount.weaponBase.address+0x100)==fixedMount.root.address &&
            read<Addr>(fixedMount.root.address+0x100)==0,"固定挂载：共同祖先链不匹配");
        const auto ancestorFlags=read<unsigned char>(fixedMount.weaponBase.address+0x1bc); // 初始总组状态来自共同挂载父级。
        fixedMount.ancestorVisible=bool(ancestorFlags&0x10);fixedMount.ancestorHidden=bool(ancestorFlags&0x20);
        for(int pool=0;pool<3;++pool) {
            if(specification.poolCounts[pool]==0)continue; // 未登记活动弹仓不进入静态显隐过滤。
            const Addr rack=read<Addr>(command.plane+specification.rackOffsets[pool]); // 原生挂架父组件。
            const bool skeletal=aircraft==MountAircraft::F35c && pool==1; // 仅现场核对的 F-35C SWP2 是活动骨架。
            require(mountName(read<Addr>(rack+0x10))==(skeletal?"SkeletalMeshComponent":"StaticMeshComponent"),"固定挂载：挂架类型不符");
            require(read<Addr>(rack+0x100)==fixedMount.weaponBase.address,"固定挂载：挂架共同父级不匹配");
            const std::string expected=std::string(specification.code)+"_SWP"+std::to_string(pool+1)+(skeletal?"":"_SM"); // 精确资源名。
            require(mountName(read<Addr>(rack+(skeletal?0x718:0x728)))==expected,"固定挂载：挂架资源被替换");
            const auto flags=read<unsigned char>(rack+0x1bc); // 退出原型时归还的原生状态。
            fixedMount.visuals.push_back({mountIdentity(rack),pool,-1,bool(flags&0x10),bool(flags&0x20)});
            const Addr array=fixedMount.meshManager.address+0x150+pool*16; // 三套持久模型池。
            const int count=read<int>(array+8),expectedCount=specification.poolCounts[pool]; // 原生持久模型池数量。
            require(count==expectedCount && read<int>(array+12)>=count,"固定挂载：模型池数量不匹配");
            for(int index=0;index<count;++index) {
                const Addr component=read<Addr>(read<Addr>(array)+index*8); // 数组顺序须与 Socket 一致。
                std::string socket=mountNameIndex(read<unsigned>(component+0x108)); // 资源混用 SP/sp 大小写。
                for(char& character:socket)if(character>='A' && character<='Z')character+=32;
                require(socket=="sp"+std::to_string((pool+1)*10+index+1) && read<Addr>(component+0x100)==rack,"固定挂载：Socket 或父链不匹配");
                const Addr mesh=read<Addr>(component+0x728); // TLS 空模型是明确采样结果，不是缺失资源的容错。
                require(specification.meshes[pool]?mesh && mountName(mesh)==specification.meshes[pool]:mesh==0,"固定挂载：武器模型被替换");
                const auto flags=read<unsigned char>(component+0x1bc); // 原生显隐快照。
                fixedMount.visuals.push_back({mountIdentity(component),pool,index,bool(flags&0x10),bool(flags&0x20)});
                if(aircraft==MountAircraft::F35c)fixedMount.physical[(pool==0?0:pool==1?14:2)+index]=component; // 每种兵装保持自己的原生组件和父链。
                else if(aircraft==MountAircraft::F15e)fixedMount.physical[(pool==0?0:pool==1?6:18)+index]=component; // 原生池与物理表固定对应。
                else {
                    if(pool==0 && ((kindsMask==5 && index>=2) || (kindsMask!=5 && index<4 && (!hasQaam || index>=2))))fixedMount.physical[index]=component;
                    if(hasQaam && pool==1 && index>=2)fixedMount.physical[index-2]=component; // QAAM 的 sp23/24 为原生外翼位置。
                    if(pool==2 && kindsMask!=5 && (index>=4 || (kindsMask==6 && index>=2)))fixedMount.physical[index]=component;
                }
            }
        }
        require(std::all_of(fixedMount.physical.begin(),fixedMount.physical.begin()+fixedMount.physicalCount,[](Addr component){return component!=0;}),"固定挂载：预期物理模型未齐备");
        fixedMount.bound=true;
        nativeLog(Json{{"event","FIXED_MOUNT_BOUND"},{"aircraft",specification.code},{"kinds_mask",kindsMask},
            {"rack_mask",fixedMount.rackMask},{"physical_positions",fixedMount.physicalCount}}.dump()); // 位置数不冒充可发射弹数。
    }
    fixedMount.weaponKinds=kinds;
    for(int index=0;index<session.count;++index)fixedMount.weaponClasses[index]=session.special[index].weaponClass;
    if(command.op=="setup" || command.op=="resupply_setup" || command.op=="resupply")fixedMount.order={0,1,2,3,4,5,6,7,8,9,10,11};
    fixedMount.rebuilding=false;
    fixedMountRefresh(command.time);
}
// 动画状态独立于是否选择多挂；补给转单挂后也应正常关闭上一组舱门。
void bindF35Cover(Addr plane) {
    if(mountAircraft(plane)!=MountAircraft::F35c) { f35Cover={};return; }
    const Addr parts=read<Addr>(plane+0x1410)+0xf90; // 已核对原生包装器与 PartsAnimation 的地址关系。
    if(f35Cover.plane.address==plane && f35Cover.parts==parts)return;
    mountInstallHooks();
    const Addr base=reinterpret_cast<Addr>(GetModuleHandleW(nullptr)); // 常量来自原生插值指令，不设置自行猜测的动画时长。
    const float seconds=read<float>(base+0x25a35ec),closed=read<float>(base+0x2d7cb00);
    require(std::abs(seconds-.4f)<.00001f && std::abs(closed+.01f)<.00001f,"F-35C 舱门：原生插值常量改变");
    require(((read<unsigned>(parts+0x27c)>>18)&15)==6 && !(read<unsigned>(parts+0xb0)&(1u<<20)),
        "F-35C 舱门：原生配置不再是独立 SP1/SP2 舱门"); // 已采样的模型与动画选项必须同时匹配。
    f35Cover={mountIdentity(plane),parts,read<int>(parts+0x178),seconds,closed};
    nativeLog("F35_COVER_CHANNELS_BOUND");
}
