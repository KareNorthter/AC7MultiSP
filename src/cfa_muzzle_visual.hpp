// CFA仅ADMM与EML同时选择时抑制左sp21炮口；EML+ECM保留两侧原生效果。
// 在light_mount.hpp之后包含，显隐使用现有引擎入口，并保存最后的原生可见请求。
#pragma once

struct CfaMuzzleVisual {
    MountIdentity component,plane,rack,particle;
    size_t templateOffset{};
    unsigned socketIndex{};
    bool nativeVisible{};
};
// 全局显隐回调只查询本次对象，不能逐条核对历史炮口的UObject身份。
std::map<Addr,CfaMuzzleVisual> cfaMuzzleVisuals;

// 这里只保留资格快照，不持有内存页、实时变换或显隐归还基线。
// 非目标也记录其拒绝条件；父链、Socket、类或模板变化后必须重新发现。
struct CfaMuzzleChild {
    MountIdentity component,componentClass,particle;
    Addr parent{};
    unsigned socketIndex{},socketNumber{};
    MountAnimationReflection::Field particleField;
    bool managed{};
};
struct CfaMuzzleDiscovery {
    bool ready{};
    MountIdentity plane,rack,rackClass;
    MountAnimationReflection::Field childrenField;
    Addr data{};
    int count{},capacity{};
    std::vector<CfaMuzzleChild> children;
} cfaMuzzleDiscovery;

struct CfaMuzzleCost {
    struct Sample { uint64_t ticks{},calls{},hits{}; } sample;
    bool active{};
    unsigned depth{};
    static uint64_t now() { LARGE_INTEGER value{};QueryPerformanceCounter(&value);return value.QuadPart; }
} cfaMuzzleCost;
// 仅切换事务计时；嵌套显隐不重复累计时间，异常退出也能结算。
struct CfaMuzzleCostScope {
    bool active{cfaMuzzleCost.active},outer{};
    uint64_t start{};
    CfaMuzzleCostScope() {
        if(!active)return;
        outer=cfaMuzzleCost.depth++==0;
        ++cfaMuzzleCost.sample.calls;if(outer)start=CfaMuzzleCost::now();
    }
    ~CfaMuzzleCostScope() {
        if(!active)return;
        if(outer)cfaMuzzleCost.sample.ticks+=CfaMuzzleCost::now()-start;
        --cfaMuzzleCost.depth;
    }
};

// 仅缓存类/字段和资源的弱身份，不保留PSC、显隐基线或租约。类换代后重新完整核对。
struct CfaMuzzleMetadata {
    MountIdentity componentClass;
    MountAnimationReflection::Field parent,socket,particle,visible;
} cfaMuzzleMetadata;
struct CfaChildrenMetadata {
    MountIdentity componentClass;
    MountAnimationReflection::Field children;
} cfaChildrenMetadata;
std::map<std::pair<Addr,bool>,std::pair<MountIdentity,bool>> cfaMuzzleTemplates;
void mountCfaMuzzleFieldCurrent(const MountAnimationReflection::Field& field,const MountIdentityBatch& identities) {
    require(identities.alive(field.identity),"CFA炮口：缓存字段身份失效");
    require(identities.read<int>(field.identity.address+0x44)==field.offset && identities.read<int>(field.identity.address+0x34)==field.size,
        "CFA炮口：缓存字段布局改变");
}
void mountCfaMuzzleFieldCurrent(const MountAnimationReflection::Field& field) {
    const MountIdentityBatch identities;mountCfaMuzzleFieldCurrent(field,identities);
}
// 四个字段共用一次完整遍历；保留父类重名、链长度、身份及字段布局检查。
std::array<MountAnimationReflection::Field,4> mountCfaMuzzleFieldSet(MountAnimationReflection& api,Addr type,int limit) {
    const std::array<const char*,4> names={"AttachParent","AttachSocketName","Template","bVisible"};
    const std::array<const char*,4> kinds={"ObjectProperty","NameProperty","ObjectProperty","BoolProperty"};
    const std::array<int,4> sizes={8,8,8,1};
    std::array<Addr,4> found{};
    {
        const MountIdentityBatch memory; // 纯读取，不跨引擎调用保存可读区间。
        for(int depth=0;type && depth<32;++depth) {
            require(memory.alive(mountIdentity(type)),"CFA预热：类身份失效");
            Addr field=memory.read<Addr>(type+0x38);
            for(int n=0;field && n<1024;++n,field=memory.read<Addr>(field+0x28)) {
                require(memory.alive(mountIdentity(field)),"CFA预热：字段身份失效");
                const auto name=mountName(field);
                for(size_t i=0;i<names.size();++i)if(name==names[i]) {
                    require(!found[i],"CFA预热：字段重名");found[i]=field;
                }
            }
            require(!field,"CFA预热：字段链过长或成环");type=memory.read<Addr>(type+0x30);
        }
        require(!type,"CFA预热：继承链过长或成环");
    }
    std::array<MountAnimationReflection::Field,4> result;
    for(size_t i=0;i<found.size();++i) {
        const Addr p=found[i];
        require(p && mountName(read<Addr>(p+0x10))==kinds[i] &&
            MountAnimationReflection::refValue<int>(api.arrayDim,p)==1,"CFA预热：字段类型或维数不符");
        const int size=read<int>(p+0x34),offset=read<int>(p+0x44);
        require(size==sizes[i] && offset>=0 && offset<=limit && size<=limit-offset,"CFA预热：字段尺寸或范围不符");
        result[i]={mountIdentity(p),offset,size};
    }
    return result;
}
const CfaMuzzleMetadata& mountCfaMuzzleFields(Addr object,MountAnimationReflection& api) {
    const auto cls=mountIdentity(read<Addr>(object+0x10));
    auto& cached=cfaMuzzleMetadata;
    if(!mountMeshIdentityEqual(cls,cached.componentClass) || !mountAlive(cached.componentClass)) {
        require(mountClassIs(object,"ParticleSystemComponent","/Script/Engine"),"CFA炮口：粒子组件类型改变");
        const int size=api.typeSize(cls.address);
        const auto fields=mountCfaMuzzleFieldSet(api,cls.address,size);
        CfaMuzzleMetadata next;next.componentClass=cls;
        next.parent=fields[0];next.socket=fields[1];next.particle=fields[2];next.visible=fields[3];
        const Addr visible=next.visible.identity.address;
        const auto byte=MountAnimationReflection::refValue<unsigned char>(api.byteMask,visible);
        const auto mask=MountAnimationReflection::refValue<unsigned char>(api.fieldMask,visible);
        require(next.parent.offset==0x100 && next.socket.offset==0x108 &&
            next.visible.offset==0x1bc && MountAnimationReflection::refValue<unsigned char>(api.byteOffset,visible)==0 &&
            MountAnimationReflection::refValue<unsigned char>(api.fieldSize,visible)==1 && byte && mask==0x10 && !(byte&~mask),
            "CFA炮口：原生附着或可见字段改变");
        cached=next;
    }
    {
        const MountIdentityBatch identities;
        for(const auto& field:{cached.parent,cached.socket,cached.particle,cached.visible})mountCfaMuzzleFieldCurrent(field,identities);
    }
    return cached;
}
bool mountCfaMuzzleTemplate(Addr particle,bool mgp=false) {
    if(!particle)return false;
    const auto key=std::make_pair(particle,mgp);
    auto found=cfaMuzzleTemplates.find(key);
    if(found!=cfaMuzzleTemplates.end() && mountAlive(found->second.first))return found->second.second;
    const auto identity=mountIdentity(particle);
    const char* expected=mgp?"FX_AutoCannon_Muzzleflash02":"FX_EML_Muzzleflash02";
    const bool matches=mountClassIs(particle,"ParticleSystem","/Script/Engine") && mountName(particle)==expected;
    if(cfaMuzzleTemplates.size()>=32)cfaMuzzleTemplates.clear(); // 只丢资格缓存，不涉及对象归还。
    cfaMuzzleTemplates[key]={identity,matches};return matches;
}
// 复用setup本来就会执行的EML初始化，只预热元数据；不取得显隐所有权、不激活或发射。
void mountCfaMuzzlePrewarm(Addr plane,uint64_t request) noexcept {
    static uint64_t attempted{};
    if(attempted==request)return;
    attempted=request; // 同一setup回到SP1时不重试；失败仍由首次实际使用按原规则识别。
    const auto started=CfaMuzzleCost::now();
    const char* status="no_matching_component";
    int matched=0;std::string error;
    try {
        mountInstallHooks();
        MountAnimationReflection api;api.bindSdk();
        const auto player=mountIdentity(plane);
        const auto rack=mountIdentity(read<Addr>(plane+mountProperty(plane,"SWP2","ObjectProperty",8)));
        require(mountClassIs(rack.address,"SkeletalMeshComponent","/Script/Engine") && mountOuterContains(rack.address,plane),
            "CFA预热：挂架归属或类型不符");
        const auto cls=mountIdentity(read<Addr>(rack.address+0x10));
        const auto children=api.field(cls.address,"AttachChildren","ArrayProperty",16,api.typeSize(cls.address),true);
        const Addr array=rack.address+children.offset,data=read<Addr>(array);
        const int count=read<int>(array+8),capacity=read<int>(array+12);
        require(count>=0 && count<=256 && capacity>=count && capacity<=4096 && (!count || data),"CFA预热：子组件列表无效");
        if(count)checkMemory(data,static_cast<size_t>(count)*8);
        for(int i=0;i<count;++i) {
            const Addr child=read<Addr>(data+static_cast<Addr>(i)*8);if(!child)continue;
            mountIdentity(child);
            if(read<Addr>(child+0x100)!=rack.address || read<unsigned>(child+0x10c)!=0 ||
                mountNameIndex(read<unsigned>(child+0x108))!="sp21" || !mountClassIs(child,"ParticleSystemComponent","/Script/Engine"))continue;
            require(mountOuterContains(child,plane),"CFA预热：粒子不属于当前玩家");
            const auto& fields=mountCfaMuzzleFields(child,api);
            if(mountCfaMuzzleTemplate(read<Addr>(child+fields.particle.offset)))++matched;
        }
        require(mountAlive(player) && mountAlive(rack) && read<Addr>(array)==data && read<int>(array+8)==count &&
            read<int>(array+12)==capacity,"CFA预热：读取期间对象或数组改变");
        cfaChildrenMetadata={cls,children};
        if(matched)status="ready";
    }catch(const std::exception& failure) {status="deferred";error=failure.what();}
    try {
        LARGE_INTEGER frequency{};QueryPerformanceFrequency(&frequency);
        nativeLog(Json{{"event","CFA_MUZZLE_PREWARM"},{"request",request},{"status",status},{"matched",matched},
            {"ms",frequency.QuadPart>0?double(CfaMuzzleCost::now()-started)*1000.0/frequency.QuadPart:0.0},{"error",error}}.dump());
    }catch(...) { } // 可选预热失败不改变既有武器事务，也不反复重试。
}
bool mountCfaMuzzleScope() {
    if(!fixedMountEnabled || !fixedMount.bound || fixedMountFailed)return false;
    if(fixedMount.aircraft==MountAircraft::Fa44)return fixedMount.light && (fixedMount.kindsMask&3u)==3u;
    return fixedMount.aircraft==MountAircraft::M21b && (fixedMount.kindsMask==3 || fixedMount.kindsMask==5);
}
MountIdentity mountMuzzleRack() {
    if(fixedMount.aircraft==MountAircraft::Fa44)return fixedMount.light->racks[1].component;
    for(const auto& visual:fixedMount.visuals)if(visual.pool==0 && visual.index==-1)return visual.identity;
    throw std::runtime_error("MGP炮口：缺少原生SWP1挂架");
}
const char* mountMuzzleSocket() {return fixedMount.aircraft==MountAircraft::M21b?"sp12":"sp21";}
MountAnimationReflection& mountMuzzleReflection() {
    if(fixedMount.aircraft==MountAircraft::Fa44)return fixedMount.light->reflection;
    static MountAnimationReflection api;api.bindSdk();return api;
}
bool mountCfaMuzzleAttached(const CfaMuzzleVisual& item,const MountIdentityBatch& identities) {
    return identities.alive(item.component) && identities.alive(item.plane) && identities.alive(item.rack) && identities.alive(item.particle) &&
        identities.read<Addr>(item.component.address+0x100)==item.rack.address &&
        identities.read<Addr>(item.component.address+item.templateOffset)==item.particle.address &&
        identities.read<unsigned>(item.component.address+0x10c)==0 &&
        identities.read<unsigned>(item.component.address+0x108)==item.socketIndex;
}
bool mountCfaMuzzleAttached(const CfaMuzzleVisual& item) {
    const MountIdentityBatch identities;return mountCfaMuzzleAttached(item,identities);
}
bool mountCfaMuzzleCurrent(const CfaMuzzleVisual& item,const MountIdentityBatch& identities) {
    return mountSameIdentity(item.plane,fixedMount.plane) &&
        mountSameIdentity(item.rack,mountMuzzleRack()) && mountCfaMuzzleAttached(item,identities);
}
bool mountCfaMuzzleCurrent(const CfaMuzzleVisual& item) {
    const MountIdentityBatch identities;return mountCfaMuzzleCurrent(item,identities);
}
void mountCfaMuzzleHide(CfaMuzzleVisual item) {
    if(!mountCfaMuzzleScope())return;
    {
        const MountIdentityBatch identities;
        if(!mountCfaMuzzleCurrent(item,identities)) { cfaMuzzleDiscovery.ready=false;return; }
        if(!(identities.read<unsigned char>(item.component.address+0x1bc)&0x10))return;
    } // 写显隐前重新核对目标，且不把页缓存带入可能重入的引擎调用。
    cfaMuzzleDiscovery.ready=false;
    mountOriginalVisibility(reinterpret_cast<void*>(item.component.address),false,0);
}
CfaMuzzleVisual* mountCfaMuzzleFind(Addr object) {
    const auto found=cfaMuzzleVisuals.find(object);
    if(found!=cfaMuzzleVisuals.end()) {
        if(mountCfaMuzzleCurrent(found->second))return &found->second;
        // 旧归属失配后同次重新识别，且不把失效账本当成已发现目标。
        cfaMuzzleDiscovery.ready=false;cfaMuzzleVisuals.erase(found);
    }
    const auto& rack=mountMuzzleRack();
    {
        const NativeReadBatch memory;
        if(memory.read<Addr>(object+0x100)!=rack.address || memory.read<unsigned>(object+0x10c)!=0 ||
           mountNameIndex(memory.read<unsigned>(object+0x108),memory)!=mountMuzzleSocket())return nullptr;
    } // 后续解析元数据及显隐调用不共享这份批次。
    // 非目标模板先快速退出；Outer和反射字段不再随每个持续特效请求从头扫描。
    if((!mountAlive(cfaMuzzleMetadata.componentClass) || read<Addr>(object+0x10)!=cfaMuzzleMetadata.componentClass.address) &&
       !mountClassIs(object,"ParticleSystemComponent","/Script/Engine"))return nullptr;
    const auto& fields=mountCfaMuzzleFields(object,mountMuzzleReflection());
    const size_t offset=fields.particle.offset;
    const Addr particle=read<Addr>(object+offset);
    if(!mountCfaMuzzleTemplate(particle,fixedMount.aircraft==MountAircraft::M21b))return nullptr;
    require(mountAlive(fixedMount.plane) && mountAlive(rack) && mountOuterContains(object,fixedMount.plane.address),
        "CFA炮口：粒子不属于当前玩家机体");
    const auto inserted=cfaMuzzleVisuals.emplace(object,CfaMuzzleVisual{mountIdentity(object),fixedMount.plane,rack,mountIdentity(particle),offset,
        read<unsigned>(object+0x108),
        (read<unsigned char>(object+0x1bc)&0x10)!=0});
    cfaMuzzleDiscovery.ready=false;
    nativeLog(fixedMount.aircraft==MountAircraft::M21b?"M21B_RIGHT_MGP_MUZZLE_SUPPRESSED sp12":"CFA_LEFT_EML_MUZZLE_SUPPRESSED sp21 FX_EML_Muzzleflash02");
    return &inserted.first->second;
}
bool mountCfaMuzzleFilter(void* object,bool requested,unsigned char propagation,bool hidden) {
    CfaMuzzleCostScope cost;
    if(!mountCfaMuzzleScope()) {
        // 归还失败后仍记最新原生请求；此时只直通，不再抑制或用旧请求覆盖原生状态。
        const auto found=cfaMuzzleVisuals.find(reinterpret_cast<Addr>(object));
        if(found!=cfaMuzzleVisuals.end()) {
            if(!mountCfaMuzzleAttached(found->second)) { cfaMuzzleDiscovery.ready=false;cfaMuzzleVisuals.erase(found); }
            else if(!hidden)found->second.nativeVisible=requested;
        }
        return false;
    }
    auto* item=mountCfaMuzzleFind(reinterpret_cast<Addr>(object));
    if(!item)return false;
    if(cost.active)++cfaMuzzleCost.sample.hits;
    const auto current=*item; // 原生隐藏入口可能重入；不跨调用保留map节点引用。
    if(hidden) {
        cfaMuzzleDiscovery.ready=false;
        mountOriginalHidden(object,requested,propagation); // 隐藏请求照常执行，不取得该位的所有权。
    } else item->nativeVisible=requested;
    mountCfaMuzzleHide(current);return true;
}
// 每次模型提交清理一次失效记录；活对象仍保留归还责任，不在每个显隐回调里全表巡检。
void mountCfaMuzzlePrune() {
    if(cfaMuzzleVisuals.empty())return;
    const MountIdentityBatch identities; // 这里只移除已失效的弱记录，不调用粒子或显隐入口。
    for(auto it=cfaMuzzleVisuals.begin();it!=cfaMuzzleVisuals.end();) {
        if(!identities.alive(it->second.component)) { cfaMuzzleDiscovery.ready=false;it=cfaMuzzleVisuals.erase(it); }
        else ++it;
    }
}
// 稳定取位只读取当前事实，不遍历反射链、不分配容器，也不调用引擎。
// 列表长度不变但某个元素被替换，以及同对象换Socket/模板，也会使快照失效。
bool mountCfaMuzzleCollect(const CfaMuzzleDiscovery& snapshot,std::array<Addr,256>& visible,int& visibleCount) {
    visibleCount=0;
    if(!snapshot.ready || !mountSameIdentity(snapshot.plane,fixedMount.plane) ||
       !mountSameIdentity(snapshot.rack,mountMuzzleRack()))return false;
    const MountIdentityBatch identities;
    if(!identities.alive(snapshot.plane) || !identities.alive(snapshot.rack) || !identities.alive(snapshot.rackClass) ||
       identities.read<Addr>(snapshot.rack.address+0x10)!=snapshot.rackClass.address)return false;
    mountCfaMuzzleFieldCurrent(snapshot.childrenField,identities);
    const Addr array=snapshot.rack.address+snapshot.childrenField.offset;
    auto sameArray=[&] {
        return identities.read<Addr>(array)==snapshot.data && identities.read<int>(array+8)==snapshot.count &&
            identities.read<int>(array+12)==snapshot.capacity;
    };
    if(!sameArray())return false;
    for(int i=0;i<snapshot.count;++i) {
        const auto& child=snapshot.children[i];
        const Addr object=identities.read<Addr>(snapshot.data+static_cast<Addr>(i)*8);
        if(object!=child.component.address)return false;
        if(!object)continue;
        if(!identities.alive(child.component) || !identities.alive(child.componentClass) ||
           identities.read<Addr>(object+0x10)!=child.componentClass.address ||
           identities.read<Addr>(object+0x100)!=child.parent ||
           identities.read<unsigned>(object+0x108)!=child.socketIndex ||
           identities.read<unsigned>(object+0x10c)!=child.socketNumber)return false;
        if(child.particleField.identity.address) {
            mountCfaMuzzleFieldCurrent(child.particleField,identities);
            if(identities.read<Addr>(object+child.particleField.offset)!=child.particle.address ||
               (child.particle.address && !identities.alive(child.particle)))return false;
        }
        const auto found=cfaMuzzleVisuals.find(object);
        if(child.managed!=(found!=cfaMuzzleVisuals.end()))return false;
        if(!child.managed)continue;
        if(!mountCfaMuzzleCurrent(found->second,identities))return false;
        if(identities.read<unsigned char>(object+0x1bc)&0x10)visible[visibleCount++]=object;
    }
    return sameArray();
}
// 绑定/重建或快照失配后才重新分类；所有临时读取批次在反射、分配和引擎调用前结束。
CfaMuzzleDiscovery mountCfaMuzzleDiscover() {
    CfaMuzzleDiscovery next;next.plane=fixedMount.plane;
    const auto& rack=mountMuzzleRack();
    require(mountAlive(rack),"CFA炮口：右炮原生挂架失效");
    const auto cls=mountIdentity(read<Addr>(rack.address+0x10));
    auto& cached=cfaChildrenMetadata;
    if(!mountMeshIdentityEqual(cls,cached.componentClass) || !mountAlive(cached.componentClass)) {
        auto& api=mountMuzzleReflection();
        cached={cls,api.field(cls.address,"AttachChildren","ArrayProperty",16,api.typeSize(cls.address),true)};
    }
    next.rack=rack;next.rackClass=cls;next.childrenField=cached.children;
    {
        const MountIdentityBatch identities;
        mountCfaMuzzleFieldCurrent(next.childrenField,identities);
        const Addr array=rack.address+next.childrenField.offset;
        next.data=identities.read<Addr>(array);next.count=identities.read<int>(array+8);next.capacity=identities.read<int>(array+12);
        require(next.count>=0 && next.count<=256 && next.capacity>=next.count && next.capacity<=4096 &&
            (!next.count || next.data),"CFA炮口：子组件列表无效");
    }
    next.children.resize(next.count); // 仅发现时分配，稳定期不复制或扩容。
    for(int i=0;i<next.count;++i) {
        const Addr object=read<Addr>(next.data+static_cast<Addr>(i)*8);if(!object)continue;
        auto& child=next.children[i];
        child.component=mountIdentity(object);child.componentClass=mountIdentity(read<Addr>(object+0x10));
        {
            const MountIdentityBatch identities;
            child.parent=identities.read<Addr>(object+0x100);
            child.socketIndex=identities.read<unsigned>(object+0x108);child.socketNumber=identities.read<unsigned>(object+0x10c);
        }
        bool expectedManaged=false;
        if(child.parent==rack.address && !child.socketNumber && mountNameIndex(child.socketIndex)==mountMuzzleSocket() &&
           mountClassIs(object,"ParticleSystemComponent","/Script/Engine")) {
            child.particleField=mountCfaMuzzleFields(object,mountMuzzleReflection()).particle;
            const Addr particle=read<Addr>(object+child.particleField.offset);
            if(particle)child.particle=mountIdentity(particle);
            expectedManaged=mountCfaMuzzleTemplate(particle,fixedMount.aircraft==MountAircraft::M21b);
        }
        child.managed=mountCfaMuzzleFind(object)!=nullptr;
        // 分类依据先入快照，再核对账本结果，不能把识别失败后的新Template记成稳定的非目标。
        require(child.managed==expectedManaged,"CFA炮口：检查期间子组件列表改变");
    }
    next.ready=true;return next;
}
// 初始化粒子可能未发出新显隐请求；提交强制发现，显示/发射取位则先尝试轻量维护。
void mountCfaMuzzleSync(bool forceDiscovery) {
    if(!mountCfaMuzzleScope())return;
    std::array<Addr,256> visible;int visibleCount{};
    if(forceDiscovery || !mountCfaMuzzleCollect(cfaMuzzleDiscovery,visible,visibleCount)) {
        cfaMuzzleDiscovery.ready=false;
        auto next=mountCfaMuzzleDiscover();
        require(mountCfaMuzzleCollect(next,visible,visibleCount),"CFA炮口：检查期间子组件列表改变");
        cfaMuzzleDiscovery=std::move(next);
    }
    // 读取批次已经结束。每次引擎调用前再核目标，前一调用可以使后面的候选失效。
    for(int i=0;i<visibleCount;++i) {
        const auto found=cfaMuzzleVisuals.find(visible[i]);
        if(found!=cfaMuzzleVisuals.end())mountCfaMuzzleHide(found->second);
    }
}
bool mountCfaMuzzleRelease() {
    cfaMuzzleTemplates.clear(); // 机型切换后重新核对不同粒子模板。
    cfaMuzzleDiscovery={}; // 资格缓存可丢弃；未归还的原生显隐请求仍留在下面的独立账本。
    std::map<Addr,CfaMuzzleVisual> pending;
    for(const auto& entry:cfaMuzzleVisuals)try {
        const auto& item=entry.second;
        if(mountCfaMuzzleAttached(item))
            mountOriginalVisibility(reinterpret_cast<void*>(item.component.address),item.nativeVisible,0);
    }catch(const std::exception& error) {pending.emplace(entry);nativeLog(std::string("CFA_MUZZLE_RELEASE_ERROR ")+error.what());}
    cfaMuzzleVisuals=std::move(pending);return cfaMuzzleVisuals.empty();
}
