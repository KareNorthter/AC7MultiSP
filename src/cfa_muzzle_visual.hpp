// CFA仅ADMM与EML同时选择时抑制左sp21炮口；EML+ECM保留两侧原生效果。
// 在light_mount.hpp之后包含，显隐使用现有引擎入口，并保存最后的原生可见请求。
#pragma once

struct CfaMuzzleVisual {
    MountIdentity component,plane,rack,particle;
    size_t templateOffset{};
    unsigned socketIndex{};
    bool nativeVisible{};
};
std::vector<CfaMuzzleVisual> cfaMuzzleVisuals;

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
void mountCfaMuzzleFieldCurrent(const MountAnimationReflection::Field& field) {
    MountAnimationReflection::alive(field.identity);
    require(read<int>(field.identity.address+0x44)==field.offset && read<int>(field.identity.address+0x34)==field.size,
        "CFA炮口：缓存字段布局改变");
}
const CfaMuzzleMetadata& mountCfaMuzzleFields(Addr object) {
    const auto cls=mountIdentity(read<Addr>(object+0x10));
    auto& cached=cfaMuzzleMetadata;
    if(!mountMeshIdentityEqual(cls,cached.componentClass) || !mountAlive(cached.componentClass)) {
        require(mountClassIs(object,"ParticleSystemComponent","/Script/Engine"),"CFA炮口：粒子组件类型改变");
        auto& api=fixedMount.light->reflection;
        const int size=api.typeSize(cls.address);
        CfaMuzzleMetadata next;next.componentClass=cls;
        next.parent=api.field(cls.address,"AttachParent","ObjectProperty",8,size,true);
        next.socket=api.field(cls.address,"AttachSocketName","NameProperty",8,size,true);
        next.particle=api.field(cls.address,"Template","ObjectProperty",8,size,true);
        const auto visible=api.boolean(cls.address,"bVisible",size,true);next.visible=visible.field;
        require(next.parent.offset==0x100 && next.socket.offset==0x108 &&
            next.visible.offset==0x1bc && visible.byteOffset==0 && visible.fieldMask==0x10,
            "CFA炮口：原生附着或可见字段改变");
        cached=next;
    }
    for(const auto& field:{cached.parent,cached.socket,cached.particle,cached.visible})mountCfaMuzzleFieldCurrent(field);
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
    return mountAlive(item.component) && mountAlive(item.plane) && mountAlive(item.rack) && mountAlive(item.particle) &&
        read<Addr>(item.component.address+0x100)==item.rack.address &&
        read<Addr>(item.component.address+item.templateOffset)==item.particle.address &&
        read<unsigned>(item.component.address+0x10c)==0 &&
        read<unsigned>(item.component.address+0x108)==item.socketIndex;
}
bool mountCfaMuzzleCurrent(const CfaMuzzleVisual& item) {
    return mountSameIdentity(item.plane,fixedMount.plane) &&
        mountSameIdentity(item.rack,fixedMount.light->racks[1].component) && mountCfaMuzzleAttached(item);
}
CfaMuzzleVisual* mountCfaMuzzleFind(Addr object) {
    for(auto it=cfaMuzzleVisuals.begin();it!=cfaMuzzleVisuals.end();) {
        if(!mountAlive(it->component)) {it=cfaMuzzleVisuals.erase(it);continue;}
        if(it->component.address==object) {
            // PSC被池重新分配或改挂时退出该记录，让本次原生显隐请求直接生效。
            if(mountCfaMuzzleCurrent(*it))return &*it;
            cfaMuzzleVisuals.erase(it);return nullptr;
        }
        ++it;
    }
    const auto& rack=fixedMount.light->racks[1].component;
    if(read<Addr>(object+0x100)!=rack.address || read<unsigned>(object+0x10c)!=0 ||
       mountNameIndex(read<unsigned>(object+0x108))!="sp21")return nullptr;
    // 非目标模板先快速退出；Outer和反射字段不再随每个持续特效请求从头扫描。
    if((!mountAlive(cfaMuzzleMetadata.componentClass) || read<Addr>(object+0x10)!=cfaMuzzleMetadata.componentClass.address) &&
       !mountClassIs(object,"ParticleSystemComponent","/Script/Engine"))return nullptr;
    const auto& fields=mountCfaMuzzleFields(object);
    const size_t offset=fields.particle.offset;
    const Addr particle=read<Addr>(object+offset);
    if(!mountCfaMuzzleTemplate(particle))return nullptr;
    require(mountAlive(fixedMount.plane) && mountAlive(rack) && mountOuterContains(object,fixedMount.plane.address),
        "CFA炮口：粒子不属于当前玩家机体");
    cfaMuzzleVisuals.push_back({mountIdentity(object),fixedMount.plane,rack,mountIdentity(particle),offset,
        read<unsigned>(object+0x108),
        (read<unsigned char>(object+0x1bc)&0x10)!=0});
    nativeLog("CFA_LEFT_EML_MUZZLE_SUPPRESSED sp21 FX_EML_Muzzleflash02");
    return &cfaMuzzleVisuals.back();
}
bool mountCfaMuzzleFilter(void* object,bool requested,unsigned char propagation,bool hidden) {
    if(!mountCfaMuzzleScope()) {
        // 归还失败后仍记最新原生请求；此时只直通，不再抑制或用旧请求覆盖原生状态。
        for(auto it=cfaMuzzleVisuals.begin();it!=cfaMuzzleVisuals.end();) {
            if(!mountAlive(it->component)) {it=cfaMuzzleVisuals.erase(it);continue;}
            if(it->component.address==reinterpret_cast<Addr>(object)) {
                if(!mountCfaMuzzleAttached(*it))cfaMuzzleVisuals.erase(it);
                else if(!hidden)it->nativeVisible=requested;
                break;
            }
            ++it;
        }
        return false;
    }
    auto* item=mountCfaMuzzleFind(reinterpret_cast<Addr>(object));
    if(!item)return false;
    if(hidden)mountOriginalHidden(object,requested,propagation); // 隐藏请求照常执行，不取得该位的所有权。
    else item->nativeVisible=requested;
    mountCfaMuzzleHide(*item);return true;
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
    std::vector<CfaMuzzleVisual> pending;
    for(const auto& item:cfaMuzzleVisuals)try {
        if(mountCfaMuzzleAttached(item))
            mountOriginalVisibility(reinterpret_cast<void*>(item.component.address),item.nativeVisible,0);
    }catch(const std::exception& error) {pending.push_back(item);nativeLog(std::string("CFA_MUZZLE_RELEASE_ERROR ")+error.what());}
    cfaMuzzleVisuals=std::move(pending);return cfaMuzzleVisuals.empty();
}
