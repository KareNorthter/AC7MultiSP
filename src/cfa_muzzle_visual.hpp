// CFA多挂仅抑制左sp21的EML炮口粒子；不改变特效激活、发射、伤害或装填。
// 在light_mount.hpp之后包含，显隐使用现有引擎入口，并保存最后的原生可见请求。
#pragma once

struct CfaMuzzleVisual {
    MountIdentity component,plane,rack,particle;
    size_t templateOffset{};
    bool nativeVisible{};
};
std::vector<CfaMuzzleVisual> cfaMuzzleVisuals;

bool mountCfaMuzzleScope() {
    return fixedMountEnabled && fixedMount.bound && !fixedMountFailed && fixedMount.light &&
        fixedMount.aircraft==MountAircraft::Fa44 && (fixedMount.kindsMask&2u) &&
        (fixedMount.kindsMask&(fixedMount.kindsMask-1))!=0;
}
bool mountCfaMuzzleAttached(const CfaMuzzleVisual& item) {
    return mountAlive(item.component) && mountAlive(item.plane) && mountAlive(item.rack) && mountAlive(item.particle) &&
        read<Addr>(item.component.address+0x100)==item.rack.address &&
        read<Addr>(item.component.address+item.templateOffset)==item.particle.address &&
        read<unsigned>(item.component.address+0x10c)==0 &&
        mountNameIndex(read<unsigned>(item.component.address+0x108))=="sp21";
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
    if(!mountClassIs(object,"ParticleSystemComponent","/Script/Engine"))return nullptr;
    require(mountAlive(fixedMount.plane) && mountAlive(rack) && mountOuterContains(object,fixedMount.plane.address),
        "CFA炮口：粒子不属于当前玩家机体");
    require(mountProperty(object,"AttachParent","ObjectProperty",8)==0x100 &&
        mountProperty(object,"AttachSocketName","NameProperty",8)==0x108,"CFA炮口：附着字段改变");
    const size_t offset=mountProperty(object,"Template","ObjectProperty",8);
    const Addr particle=read<Addr>(object+offset);
    if(!particle || !mountClassIs(particle,"ParticleSystem","/Script/Engine") ||
       mountName(particle)!="FX_EML_Muzzleflash02")return nullptr;
    auto& api=fixedMount.light->reflection;
    const Addr type=read<Addr>(object+0x10);
    const auto visible=api.boolean(type,"bVisible",api.typeSize(type),true);
    require(visible.field.offset==0x1bc && visible.byteOffset==0 && visible.fieldMask==0x10,
        "CFA炮口：原生可见字段改变");
    cfaMuzzleVisuals.push_back({mountIdentity(object),fixedMount.plane,rack,mountIdentity(particle),offset,
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
    mountOriginalVisibility(object,false,0);return true;
}
// 初始化会先生成持久PSC；提交后及实际取发射位置前补查，覆盖初始已可见而未再调用显隐的粒子。
void mountCfaMuzzleSync() {
    if(!mountCfaMuzzleScope())return;
    const auto& rack=fixedMount.light->racks[1].component;
    require(mountAlive(rack),"CFA炮口：右炮原生挂架失效");
    const Addr array=rack.address+mountProperty(rack.address,"AttachChildren","ArrayProperty",16);
    const Addr data=read<Addr>(array);const int count=read<int>(array+8),capacity=read<int>(array+12);
    require(count>=0 && count<=256 && capacity>=count && capacity<=4096 && (!count || data),"CFA炮口：子组件列表无效");
    if(count)checkMemory(data,static_cast<size_t>(count)*8);
    for(int i=0;i<count;++i) {
        const Addr child=read<Addr>(data+static_cast<Addr>(i)*8);if(!child)continue;
        mountIdentity(child);
        if(auto* item=mountCfaMuzzleFind(child))mountOriginalVisibility(reinterpret_cast<void*>(item->component.address),false,0);
    }
}
bool mountCfaMuzzleRelease() {
    std::vector<CfaMuzzleVisual> pending;
    for(const auto& item:cfaMuzzleVisuals)try {
        if(mountCfaMuzzleAttached(item))
            mountOriginalVisibility(reinterpret_cast<void*>(item.component.address),item.nativeVisible,0);
    }catch(const std::exception& error) {pending.push_back(item);nativeLog(std::string("CFA_MUZZLE_RELEASE_ERROR ")+error.what());}
    cfaMuzzleVisuals=std::move(pending);return cfaMuzzleVisuals.empty();
}
