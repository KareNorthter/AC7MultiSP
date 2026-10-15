// Su-30SM / F-35C 静态固定挂载研究原型；仅由独立候选构建启用。
// 包含位置在核心私有命名空间中，复用已有内存校验、弹药缓存和日志。
#pragma once

struct MountIdentity { Addr address{}; int index{},serial{}; }; // 地址和对象序列号共同标识实例。
enum class MountAircraft { Unsupported, Su30sm, F35c }; // 活动弹仓机型不进入静态模型处理。
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
    std::array<int,3> weaponKinds{}; // Su-30SM：6AAM/LAGM/QAAM；F-35C：SOD/8AGM；不依赖选择顺序。
    std::array<int,3> logicalCounts{6,6,4}; // 原生队列长度，F-35C 使用 2/12；不改变装填窗口。
    unsigned kindsMask{}; // 每种真实武器占一位，组合变化时重新绑定布局。
    std::array<int,12> order{0,1,2,3,4,5,6,7,8,9,10,11}; // 当前原生槽对应的持久挂点序号。
    std::array<Addr,14> physical{}; // Su-30SM 六处；F-35C 两处 SOD＋十二处 8AGM。
    int physicalCount{6}; // 只遍历本机布局，不把空余容量视为有效位置。
    unsigned rackMask{4}; // Su-30SM 仅 SWP3；F-35C 同时显示互不重叠的 SWP1/3。
    std::vector<MountVisual> visuals; // 只接管已验证静态模型，不包含普通导弹及活动舱门。
    unsigned readyMask{}; // 实际物理位置的聚合就绪状态。
    unsigned pendingReadyMask{}; // 已就绪但等待新池完成原生装填事件的逻辑槽。
    std::string error; // 钩子错误交回 Lua，不能在游戏调用栈中抛出异常。
} fixedMount;

using MountVisibility=void(*)(void*,bool,unsigned char); // 已读完整实现：0 不传播，1 仅通知子级渲染，2 改子级状态。
using MountTransform=bool(*)(void*,void*,int,int); // MeshManager、输出 FTransform、分组、逻辑槽。
using MountSelection=void(*)(void*,unsigned char,void*,int*); // 保留原生选槽，只替换已选槽的输出变换。
using MountMaster=void(*)(void*,bool,bool); // 原生总显隐及强制更新参数。
MountVisibility mountOriginalVisibility{},mountOriginalHidden{}; // 绕过本原型钩子的引擎接口。
MountTransform mountOriginalTransform{}; // 原生直接取位入口。
MountSelection mountOriginalSelection{}; // 原生选槽取位入口。
MountMaster mountOriginalMaster{}; // 原生总显隐入口。
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
    if(fixedMount.aircraft==MountAircraft::F35c)return kind==0?ordinal:2+ordinal; // 两个静态组保留一一映射。
    if(kind==1)return 4+ordinal%2;
    if(kind==2)return ordinal%2;
    require(kind==0,"固定挂载：未知武器种类");
    return (fixedMount.kindsMask&4)?2+ordinal%2:(ordinal<4?ordinal:ordinal-2); // 有 QAAM 时让出外翼；四加二组合保持原样。
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
        {0x8a11b0,"405641574883ec280fb6f2",reinterpret_cast<void*>(&mountMaster),reinterpret_cast<void**>(&mountOriginalMaster)}};
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
    if(count!=2 && count!=4 && count!=6 && count!=12)return; // 其他队列不会绑定本原型。
    for(int index=0;index<count;++index)fixedMount.order[index]=saved.slots[(next+index)%count].mountOrdinal;
}
// 根据真实弹药和允许装填的窗口聚合六处显示；未激活 SP 使用游戏时间推进。
void fixedMountRefresh(double time) {
    require(fixedMount.error.empty(),fixedMount.error.c_str());
    if(!fixedMount.bound)return;
    require(mountAlive(fixedMount.plane) && mountAlive(fixedMount.meshManager),"固定挂载：更新时身份失效");
    unsigned mask=0; // 本次所有物理位置的完整结果。
    for(int weapon=0;weapon<session.count;++weapon) {
        const Group group=weapon==session.active?capture(session.activator,true,time):session.special[weapon]; // 当前或缓存队列。
        const int ammo=scalarInt(group,0),window=scalarInt(group,8),cursor=scalarInt(group,12); // 原生队列状态。
        const int count=fixedMount.logicalCounts[fixedMount.weaponKinds[weapon]]; // 槽数与模型池一一核对。
        require(group.slots.size()==static_cast<size_t>(count) && ammo>=-1 && window>=0 && window<=count && cursor>=0 && cursor<count,"固定挂载：逻辑队列无效");
        require(time+0.001>=group.capturedTime,"固定挂载：游戏时间回退，等待检查点恢复");
        const double delta=std::max(0.0,time-group.capturedTime); // 与现有冷却缓存使用相同的时间校验。
        const int available=ammo<0?window:std::min(window,ammo); // 不让最后一发同时分配给多个位置。
        for(int distance=0;distance<available;++distance) {
            const Slot& slot=group.slots[(cursor+distance)%count]; // 窗口中的一个逻辑弹位。
            require(slot.mountOrdinal>=0 && slot.mountOrdinal<count,"固定挂载：缓存挂点序号无效");
            if(dualsp::advanceReload(slot.duration,slot.elapsed,delta,true)<slot.duration)continue;
            const int physical=mountPhysical(fixedMount.weaponKinds[weapon],slot.mountOrdinal); // 与离架路径一致。
            mask|=1u<<physical;
        }
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
    const bool f35=aircraft==MountAircraft::F35c; // F-35C 仅接入 SOD＋8AGM，不接入活动弹仓。
    std::array<int,3> kinds{}; // 选择槽到真实武器种类的映射。
    unsigned kindsMask=0; // 拒绝重复类，三挂必须恰好包含三种原版兵装。
    for(int index=0;index<session.count;++index) {
        const std::string name=mountName(session.special[index].weaponClass); // 必须使用真实武器类。
        if(name==(f35?"plwp_sod_a0_C":"plwp_6aam_r0_C"))kinds[index]=0;
        else if(name==(f35?"plwp_8agm_a0_C":"plwp_lagm_r0_C"))kinds[index]=1;
        else if(!f35 && name=="plwp_qaam_r0_C")kinds[index]=2;
        else { fixedMountRelease();return; } // 此组合不在原型作用范围内，正常保留原生双SP。
        require(!(kindsMask&(1u<<kinds[index])),"固定挂载：武器类重复");
        kindsMask|=1u<<kinds[index];
    }
    require((session.count==2 && (kindsMask==3 || kindsMask==5 || kindsMask==6)) ||
        (session.count==3 && kindsMask==7),"固定挂载：已识别兵装组合与槽数不符");
    const bool hasQaam=(kindsMask&4)!=0; // 含 QAAM 的组合沿用三挂位置，未选武器由就绪掩码保持隐藏。
    if(!fixedMount.bound || fixedMount.plane.address!=command.plane || fixedMount.aircraft!=aircraft || fixedMount.kindsMask!=kindsMask) {
        const auto order=fixedMount.order; // restore 可能已为检查点的新实例重建顺序。
        const unsigned pending=fixedMount.pendingReadyMask; // 同时保留新池一次性装填完成事件。
        fixedMountRelease();fixedMount.order=order;fixedMount.pendingReadyMask=pending;
        fixedMount.kindsMask=kindsMask;
        fixedMount.aircraft=aircraft;
        fixedMount.logicalCounts=f35?std::array<int,3>{2,12,0}:std::array<int,3>{6,6,4};
        fixedMount.physicalCount=f35?14:6;
        fixedMount.rackMask=f35?5:4;
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
        const std::array<size_t,3> offsets=f35?std::array<size_t,3>{0x1ba0,0x1b90,0x1b98}:
            std::array<size_t,3>{0x1ba0,0x1b98,0x1b90}; // 两机 SWP2/3 顺序不同，分别经现场反射核对。
        for(int pool=0;pool<3;++pool) {
            if(f35 && pool==1)continue; // 活动弹仓及其模型仍由原生控制，不进入静态显隐过滤。
            const Addr rack=read<Addr>(command.plane+offsets[pool]); // 原生挂架父组件。
            require(mountName(read<Addr>(rack+0x10))=="StaticMeshComponent","固定挂载：预期静态挂架类型不符");
            require(read<Addr>(rack+0x100)==fixedMount.weaponBase.address,"固定挂载：挂架共同父级不匹配");
            const std::string expected=std::string(f35?"f35c_SWP":"su30sm_SWP")+std::to_string(pool+1)+"_SM"; // 精确资源名。
            require(mountName(read<Addr>(rack+0x728))==expected,"固定挂载：挂架资源被替换");
            const auto flags=read<unsigned char>(rack+0x1bc); // 退出原型时归还的原生状态。
            fixedMount.visuals.push_back({mountIdentity(rack),pool,-1,bool(flags&0x10),bool(flags&0x20)});
            const Addr array=fixedMount.meshManager.address+0x150+pool*16; // 三套持久模型池。
            const int count=read<int>(array+8),expectedCount=f35?(pool==0?2:12):(pool==1?4:6); // 原生持久模型池数量。
            require(count==expectedCount && read<int>(array+12)>=count,"固定挂载：模型池数量不匹配");
            for(int index=0;index<count;++index) {
                const Addr component=read<Addr>(read<Addr>(array)+index*8); // 数组顺序须与 Socket 一致。
                std::string socket=mountNameIndex(read<unsigned>(component+0x108)); // 资源混用 SP/sp 大小写。
                for(char& character:socket)if(character>='A' && character<='Z')character+=32;
                require(socket=="sp"+std::to_string((pool+1)*10+index+1) && read<Addr>(component+0x100)==rack,"固定挂载：Socket 或父链不匹配");
                const std::array<const char*,3> meshes=f35?std::array<const char*,3>{"w_sod_a0_closed_SM","","w_8agm_a0_SM"}:
                    std::array<const char*,3>{"w_4aam_r0_SM","w_qaam_r0_SM","w_lasm_r0_SM"}; // 现场核对的实际模型名。
                require(mountName(read<Addr>(component+0x728))==meshes[pool],"固定挂载：武器模型被替换");
                const auto flags=read<unsigned char>(component+0x1bc); // 原生显隐快照。
                fixedMount.visuals.push_back({mountIdentity(component),pool,index,bool(flags&0x10),bool(flags&0x20)});
                if(f35)fixedMount.physical[(pool==0?0:2)+index]=component; // SOD 与 8AGM 各自原位，一一映射。
                else {
                    if(pool==0 && index<4 && (!hasQaam || index>=2))fixedMount.physical[index]=component;
                    if(hasQaam && pool==1 && index>=2)fixedMount.physical[index-2]=component; // QAAM 的 sp23/24 为原生外翼位置。
                    if(pool==2 && index>=4)fixedMount.physical[index]=component;
                }
            }
        }
        require(std::all_of(fixedMount.physical.begin(),fixedMount.physical.begin()+fixedMount.physicalCount,[](Addr component){return component!=0;}),"固定挂载：预期物理模型未齐备");
        fixedMount.bound=true;
        const char* layout=kindsMask==3?"4x6AAM+2xLAGM":kindsMask==5?"2xQAAM+2x6AAM":
            kindsMask==6?"2xQAAM+2xLAGM":"2xQAAM+2x6AAM+2xLAGM"; // 日志只列实际选中的武器。
        nativeLog(f35?"FIXED_MOUNT_BOUND F-35C 2xSOD+12x8AGM rack=SWP1+SWP3":std::string("FIXED_MOUNT_BOUND Su-30SM ")+layout+" rack=SWP3");
    }
    fixedMount.weaponKinds=kinds;
    for(int index=0;index<session.count;++index)fixedMount.weaponClasses[index]=session.special[index].weaponClass;
    if(command.op=="setup" || command.op=="resupply_setup" || command.op=="resupply")fixedMount.order={0,1,2,3,4,5,6,7,8,9,10,11};
    fixedMount.rebuilding=false;
    fixedMountRefresh(command.time);
}
