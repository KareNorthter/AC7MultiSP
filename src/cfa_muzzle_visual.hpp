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
std::map<Addr,std::pair<MountIdentity,bool>> cfaMuzzleTemplates;
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
bool mountCfaMuzzleTemplate(Addr particle) {
    if(!particle)return false;
    auto found=cfaMuzzleTemplates.find(particle);
    if(found!=cfaMuzzleTemplates.end() && mountAlive(found->second.first))return found->second.second;
    const auto identity=mountIdentity(particle);
    const bool matches=mountClassIs(particle,"ParticleSystem","/Script/Engine") && mountName(particle)=="FX_EML_Muzzleflash02";
    if(cfaMuzzleTemplates.size()>=32)cfaMuzzleTemplates.clear(); // 只丢资格缓存，不涉及对象归还。
    cfaMuzzleTemplates[particle]={identity,matches};return matches;
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
void mountCfaMuzzleHide(const CfaMuzzleVisual& item) {
    // EML持续取位会多次经过这里；已经不可见就不再通知渲染和传播显隐。
    if(read<unsigned char>(item.component.address+0x1bc)&0x10)
        mountOriginalVisibility(reinterpret_cast<void*>(item.component.address),false,0);
}

bool mountCfaMuzzleScope() {
    return fixedMountEnabled && fixedMount.bound && !fixedMountFailed && fixedMount.light &&
        fixedMount.aircraft==MountAircraft::Fa44 && (fixedMount.kindsMask&3u)==3u;
}
bool mountCfaMuzzleAttached(const CfaMuzzleVisual& item) {
    const MountIdentityBatch identities;
    return identities.alive(item.component) && identities.alive(item.plane) && identities.alive(item.rack) && identities.alive(item.particle) &&
        identities.read<Addr>(item.component.address+0x100)==item.rack.address &&
        identities.read<Addr>(item.component.address+item.templateOffset)==item.particle.address &&
        identities.read<unsigned>(item.component.address+0x10c)==0 &&
        identities.read<unsigned>(item.component.address+0x108)==item.socketIndex;
}
bool mountCfaMuzzleCurrent(const CfaMuzzleVisual& item) {
    return mountSameIdentity(item.plane,fixedMount.plane) &&
        mountSameIdentity(item.rack,fixedMount.light->racks[1].component) && mountCfaMuzzleAttached(item);
}
CfaMuzzleVisual* mountCfaMuzzleFind(Addr object) {
    const auto found=cfaMuzzleVisuals.find(object);
    if(found!=cfaMuzzleVisuals.end()) {
        if(!mountAlive(found->second.component))cfaMuzzleVisuals.erase(found);
        else {
            if(mountCfaMuzzleCurrent(found->second))return &found->second;
            // 旧归属失配后只重新识别当前对象，避免仍符合左炮口条件时漏放本次请求。
            cfaMuzzleVisuals.erase(found);
        }
    }
    const auto& rack=fixedMount.light->racks[1].component;
    {
        const NativeReadBatch memory;
        if(memory.read<Addr>(object+0x100)!=rack.address || memory.read<unsigned>(object+0x10c)!=0 ||
           mountNameIndex(memory.read<unsigned>(object+0x108))!="sp21")return nullptr;
    } // 后续解析元数据及显隐调用不共享这份批次。
    // 非目标模板先快速退出；Outer和反射字段不再随每个持续特效请求从头扫描。
    if((!mountAlive(cfaMuzzleMetadata.componentClass) || read<Addr>(object+0x10)!=cfaMuzzleMetadata.componentClass.address) &&
       !mountClassIs(object,"ParticleSystemComponent","/Script/Engine"))return nullptr;
    const auto& fields=mountCfaMuzzleFields(object,fixedMount.light->reflection);
    const size_t offset=fields.particle.offset;
    const Addr particle=read<Addr>(object+offset);
    if(!mountCfaMuzzleTemplate(particle))return nullptr;
    require(mountAlive(fixedMount.plane) && mountAlive(rack) && mountOuterContains(object,fixedMount.plane.address),
        "CFA炮口：粒子不属于当前玩家机体");
    const auto inserted=cfaMuzzleVisuals.emplace(object,CfaMuzzleVisual{mountIdentity(object),fixedMount.plane,rack,mountIdentity(particle),offset,
        read<unsigned>(object+0x108),
        (read<unsigned char>(object+0x1bc)&0x10)!=0});
    nativeLog("CFA_LEFT_EML_MUZZLE_SUPPRESSED sp21 FX_EML_Muzzleflash02");
    return &inserted.first->second;
}
bool mountCfaMuzzleFilter(void* object,bool requested,unsigned char propagation,bool hidden) {
    CfaMuzzleCostScope cost;
    if(!mountCfaMuzzleScope()) {
        // 归还失败后仍记最新原生请求；此时只直通，不再抑制或用旧请求覆盖原生状态。
        const auto found=cfaMuzzleVisuals.find(reinterpret_cast<Addr>(object));
        if(found!=cfaMuzzleVisuals.end()) {
            if(!mountCfaMuzzleAttached(found->second))cfaMuzzleVisuals.erase(found);
            else if(!hidden)found->second.nativeVisible=requested;
        }
        return false;
    }
    auto* item=mountCfaMuzzleFind(reinterpret_cast<Addr>(object));
    if(!item)return false;
    if(cost.active)++cfaMuzzleCost.sample.hits;
    if(hidden)mountOriginalHidden(object,requested,propagation); // 隐藏请求照常执行，不取得该位的所有权。
    else item->nativeVisible=requested;
    mountCfaMuzzleHide(*item);return true;
}
// 每次模型提交清理一次失效记录；活对象仍保留归还责任，不在每个显隐回调里全表巡检。
void mountCfaMuzzlePrune() {
    if(cfaMuzzleVisuals.empty())return;
    const MountIdentityBatch identities; // 这里只移除已失效的弱记录，不调用粒子或显隐入口。
    for(auto it=cfaMuzzleVisuals.begin();it!=cfaMuzzleVisuals.end();) {
        if(!identities.alive(it->second.component))it=cfaMuzzleVisuals.erase(it);
        else ++it;
    }
}
// 初始化会先生成持久PSC；提交后及实际取发射位置前补查，覆盖初始已可见而未再调用显隐的粒子。
void mountCfaMuzzleSync() {
    if(!mountCfaMuzzleScope())return;
    const auto& rack=fixedMount.light->racks[1].component;
    require(mountAlive(rack),"CFA炮口：右炮原生挂架失效");
    const auto cls=mountIdentity(read<Addr>(rack.address+0x10));
    auto& cached=cfaChildrenMetadata;
    if(!mountMeshIdentityEqual(cls,cached.componentClass) || !mountAlive(cached.componentClass)) {
        auto& api=fixedMount.light->reflection;
        cached={cls,api.field(cls.address,"AttachChildren","ArrayProperty",16,api.typeSize(cls.address),true)};
    }
    mountCfaMuzzleFieldCurrent(cached.children);
    const Addr array=rack.address+cached.children.offset;
    const Addr data=read<Addr>(array);const int count=read<int>(array+8),capacity=read<int>(array+12);
    require(count>=0 && count<=256 && capacity>=count && capacity<=4096 && (!count || data),"CFA炮口：子组件列表无效");
    if(count)checkMemory(data,static_cast<size_t>(count)*8);
    for(int i=0;i<count;++i) {
        const Addr child=read<Addr>(data+static_cast<Addr>(i)*8);if(!child)continue;
        mountIdentity(child);
        if(auto* item=mountCfaMuzzleFind(child))mountCfaMuzzleHide(*item);
    }
    require(read<Addr>(array)==data && read<int>(array+8)==count && read<int>(array+12)==capacity,
        "CFA炮口：检查期间子组件列表改变");
}
bool mountCfaMuzzleRelease() {
    std::map<Addr,CfaMuzzleVisual> pending;
    for(const auto& entry:cfaMuzzleVisuals)try {
        const auto& item=entry.second;
        if(mountCfaMuzzleAttached(item))
            mountOriginalVisibility(reinterpret_cast<void*>(item.component.address),item.nativeVisible,0);
    }catch(const std::exception& error) {pending.emplace(entry);nativeLog(std::string("CFA_MUZZLE_RELEASE_ERROR ")+error.what());}
    cfaMuzzleVisuals=std::move(pending);return cfaMuzzleVisuals.empty();
}
