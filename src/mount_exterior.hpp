// 只保留明显的机外装置；舱内模型、弹舱控制及发射取位均由原生处理。
#pragma once

void mountBindExteriorVisuals(FixedMountState& state,Addr plane) {
    require(mountExterior(state.aircraft) && (state.kindsMask&4u),"机外保留：组合没有目标装置");
    state.physicalCount=0;
    // 三字段仍登记供场景重建核对，但只接管SWP3及F-22A的侧部八枚。
    for(int pool=0;pool<3;++pool) {
        state.rackOffsets[pool]=mountProperty(plane,("SWP"+std::to_string(pool+1)).c_str(),"ObjectProperty",8);
        const Addr array=state.meshManager.address+0x150+pool*16;
        const int count=read<int>(array+8),capacity=read<int>(array+12);
        require(count>=0 && count<=capacity && count<=256,"机外保留：持久池尺寸无效");
        state.poolCounts[pool]=count;
    }
    const bool f22=state.aircraft==MountAircraft::F22a;
    const Addr rack=read<Addr>(plane+state.rackOffsets[2]);state.exteriorRack=mountIdentity(rack);
    require(mountClassIs(rack,f22?"SkeletalMeshComponent":"StaticMeshComponent","/Script/Engine") &&
            read<Addr>(rack+0x100)==state.weaponBase.address,"机外保留：目标挂架类型或父链不符");
    state.exteriorMeshOffset=mountProperty(rack,f22?"SkeletalMesh":"StaticMesh","ObjectProperty",8);
    state.exteriorMesh=mountIdentity(read<Addr>(rack+state.exteriorMeshOffset));
    const auto& spec=mountSpecification(state.aircraft);
    const std::string meshName=std::string(spec.code)+"_SWP3"+(f22?"":"_SM");
    require(mountName(state.exteriorMesh.address)==meshName &&
        mountName(read<Addr>(state.exteriorMesh.address+0x20))=="/Game/Vehicles/Aircraft/"+std::string(spec.code)+"/"+meshName,
        "机外保留：目标挂架资源被替换");
    const auto rackFlags=read<unsigned char>(rack+0x1bc);
    state.visuals.push_back({state.exteriorRack,2,-1,bool(rackFlags&0x10),bool(rackFlags&0x20)});
    if(!f22)return; // PLSL炮体在静态挂架内；不接管其弹体池、效果源或冷却。

    const auto& original=animatedMountData().at("planes").at(0).at("groups").at(2);
    const Addr skeleton=read<Addr>(state.exteriorMesh.address+mountProperty(state.exteriorMesh.address,"Skeleton","ObjectProperty",8));
    const Addr animation=read<Addr>(rack+mountProperty(rack,"AnimClass","ClassProperty",8));
    mountAnimatedResource(skeleton,original.at("skeleton").get<std::string>(),original.at("packages").at("skeleton").get<std::string>(),"Skeleton");
    mountAnimatedResource(animation,original.at("animclass").get<std::string>(),original.at("packages").at("animclass").get<std::string>(),"AnimBlueprintGeneratedClass");
    MountAnimationReflection reflection;reflection.bindSdk();
    const Addr cls=read<Addr>(rack+0x10);const int extent=reflection.typeSize(cls);
    const auto noSkeleton=reflection.boolean(cls,"bNoSkeletonUpdate",extent,true),pause=reflection.boolean(cls,"bPauseAnims",extent,true);
    require(noSkeleton.field.offset==0x9b0 && noSkeleton.fieldMask==2 && pause.field.offset==0x9b0 && pause.fieldMask==4,
        "机外保留：F-22A骨架布尔布局改变");
    const Addr array=state.meshManager.address+0x170,data=read<Addr>(array);
    require(state.poolCounts[2]==14,"机外保留：F-22A原生8AAM池数量改变");
    std::set<std::string> sockets;int lateral=0;
    for(int index=0;index<state.poolCounts[2];++index) {
        const Addr component=read<Addr>(data+static_cast<Addr>(index)*8);const auto identity=mountIdentity(component);
        require(mountClassIs(component,"StaticMeshComponent","/Script/Engine") && read<Addr>(component+0x100)==rack,
            "机外保留：8AAM弹体类型或父链改变");
        auto socket=mountNameIndex(read<unsigned>(component+0x108));
        for(char& c:socket)if(c>='A' && c<='Z')c+=32;
        require(read<unsigned>(component+0x10c)==0 && sockets.insert(socket).second &&
            std::find(original.at("sockets").begin(),original.at("sockets").end(),socket)!=original.at("sockets").end(),
            "机外保留：8AAM原生Socket未知或重复");
        const Addr mesh=read<Addr>(component+0x728);
        require(mesh && mountName(mountIdentity(mesh).address)=="w_4aam_a0_SM","机外保留：8AAM弹体资源改变");
        state.physical[index]=component; // 原生池下标，不重新映射逻辑弹位或发射位置。
        if(socket>="sp31" && socket<="sp38") {
            const auto flags=read<unsigned char>(component+0x1bc);
            state.visuals.push_back({identity,2,index,bool(flags&0x10),bool(flags&0x20)});++lateral;
        }
    }
    require(lateral==8 && sockets.size()==14,"机外保留：8AAM侧部八个位置不完整");
    state.physicalCount=14;
}

bool mountExteriorDesired(const FixedMountState& state,const MountVisual& visual,bool groupVisible) {
    if(!groupVisible || !(state.kindsMask&4u))return false;
    if(visual.index<0 || state.aircraft==MountAircraft::Pkfa)return true;
    // 当前8AAM仍按原生逐发显隐；切走后保留缓存库存/装填结果。MSL状态也保留侧部。
    const bool preview=mountPreview.active && &state==&mountPreview.layout;
    if(!preview && state.exteriorConfig==2 && read<unsigned char>(state.animationActivator.address+0x2a1)!=0)
        return visual.nativeVisible && !visual.nativeHidden;
    return (state.readyMask&visual.physicalBit)!=0;
}

void mountExteriorRelease(FixedMountState& state) {
    std::string failure;
    if(state.exteriorBones)try { state.exteriorBones->release();state.exteriorBones.reset(); }
    catch(const std::exception& error) { failure=error.what(); }
    if(state.exteriorAnimationOwned)try {
        if(mountAlive(state.exteriorRack)) {
            const Addr rack=state.exteriorRack.address;
            const bool noSkeleton=(read<unsigned char>(rack+0x9b0)&2)!=0,tick=read<unsigned char>(rack+0x3f)!=0;
            if(noSkeleton!=state.exteriorNativeNoSkeleton || tick!=state.exteriorNativeTick)
                mountSetRackAnimation(rack,state.exteriorNativeNoSkeleton,state.exteriorNativeTick);
        }
        state.exteriorAnimationOwned=false;
    }catch(const std::exception& error) { if(!failure.empty())failure+="；";failure+=error.what(); }
    require(failure.empty(),failure.c_str());
}
void mountExteriorBeforeRebuild(FixedMountState& state) {
    auto cache=state.exteriorBones;
    mountExteriorRelease(state);
    state.exteriorBones=std::move(cache); // 只保留未接管的已核元数据；同组件Init后快速重绑，不重复完整反射搜索。
}
void mountExteriorObserveInit(FixedMountState& state) {
    if(state.aircraft!=MountAircraft::F22a || !state.exteriorRack.address)return;
    require(mountAlive(state.exteriorRack),"机外保留：初始化后侧架身份失效");
    state.exteriorNativeNoSkeleton=(read<unsigned char>(state.exteriorRack.address+0x9b0)&2)!=0;
    state.exteriorNativeTick=read<unsigned char>(state.exteriorRack.address+0x3f)!=0;
}
void mountExteriorMaintain(FixedMountState& state) {
    if(!mountExterior(state.aircraft))return;
    require(mountAlive(state.plane) && mountAlive(state.exteriorRack) && mountAlive(state.exteriorMesh) &&
        read<Addr>(state.plane.address+state.rackOffsets[2])==state.exteriorRack.address &&
        read<Addr>(state.exteriorRack.address+0x100)==state.weaponBase.address &&
        read<Addr>(state.exteriorRack.address+state.exteriorMeshOffset)==state.exteriorMesh.address,
        "机外保留：装置、资源或父链改变");
    state.exteriorConfig=read<int>(state.plane.address+0xb4c);
    require(state.exteriorConfig>=0 && state.exteriorConfig<3,"机外保留：原生选择编号无效");
    if(state.aircraft==MountAircraft::Pkfa)return;
    // 仅在非8AAM配置隐藏重复腹架；当前8AAM的14个原生发射点及其骨骼全部恢复。
    if(state.exteriorConfig==2) {
        if(state.exteriorBones)state.exteriorBones->release(); // 原生8AAM必须恢复腹骨；保留已核元数据缓存。
    } else {
        if(!state.exteriorBones || !state.exteriorBones->active()) {
            if(!state.exteriorBones)state.exteriorBones=std::make_shared<MountExteriorBones>();
            state.exteriorBones->bind(state.exteriorRack.address,true); // 同栈完成基线采样和隐藏，逐根仍回读；周期巡检不重复查骨。
        }
    }
    const Addr rack=state.exteriorRack.address;const auto flags=read<unsigned char>(rack+0x9b0);
    const bool noSkeleton=(flags&2)!=0,tick=read<unsigned char>(rack+0x3f)!=0;
    require(!(flags&4),"机外保留：原生正在暂停侧架动画");
    if(!state.exteriorAnimationOwned) {
        state.exteriorNativeNoSkeleton=noSkeleton;state.exteriorNativeTick=tick;state.exteriorAnimationOwned=true;
    } else {
        if(noSkeleton)state.exteriorNativeNoSkeleton=true;
        if(!tick)state.exteriorNativeTick=false;
    }
    if(noSkeleton || !tick)mountSetRackAnimation(rack,false,true); // 沿用F-35C已验证的最小更新方式，不写Parts/舱门进度。
}
void mountExteriorInherit(FixedMountState& next,const FixedMountState& prior) {
    if(!mountExterior(next.aircraft) || !mountMeshIdentityEqual(next.exteriorRack,prior.exteriorRack))return;
    next.exteriorAnimationOwned=prior.exteriorAnimationOwned;
    next.exteriorNativeNoSkeleton=prior.exteriorNativeNoSkeleton;next.exteriorNativeTick=prior.exteriorNativeTick;
    next.exteriorBones=prior.exteriorBones;
}
uint64_t mountExteriorPreviewMask(const FixedMountState& state,const std::array<int,3>& kinds,const std::array<int,3>& ammo,int count) {
    if(state.aircraft!=MountAircraft::F22a)return 0;
    int available=0;for(int slot=0;slot<count;++slot)if(kinds[slot]==2)available=std::min(14,ammo[slot]);
    return available?((uint64_t{1}<<available)-1):0;
}
void mountExteriorRefresh(double time) {
    auto& state=fixedMount;const auto now=GetTickCount64();
    if(now-state.animatedVisualAudit<100)return; // 事件显隐仍立即过滤，库存/装填每100ms聚合一次。
    require(mountAlive(state.plane) && mountAlive(state.manager) && mountAlive(state.meshManager) && mountAlive(state.animationActivator) &&
        read<Addr>(state.animationActivator.address+0x260)==state.manager.address &&
        read<Addr>(state.manager.address+0x150)==state.meshManager.address,"机外保留：兵装管理器身份改变");
    mountExteriorMaintain(state);
    uint64_t mask=0;
    if(state.aircraft==MountAircraft::F22a) {
        for(int weapon=0;weapon<session.count;++weapon)if(state.weaponKinds[weapon]==2) {
            const Group group=weapon==session.active?capture(session.activator,true,time):session.special[weapon];
            const int count=static_cast<int>(group.slots.size()),ammo=scalarInt(group,0),window=scalarInt(group,8),cursor=scalarInt(group,12);
            require(count==14 && window>=0 && window<=count && cursor>=0 && cursor<count && ammo>=-1 &&
                time+.001>=group.capturedTime,"机外保留：8AAM队列或游戏时间改变");
            const int available=ammo<0?count:std::min(count,ammo);const double delta=std::max(0.,time-group.capturedTime);
            uint64_t ready=0,vacant=0;
            for(int distance=0;distance<available;++distance) {
                const auto& slot=group.slots[(cursor+distance)%count];
                require(slot.mountOrdinal>=0 && slot.mountOrdinal<count,"机外保留：8AAM原生池序号无效");
                mask|=uint64_t{1}<<slot.mountOrdinal;
                if(distance<window && dualsp::advanceReload(slot.duration,slot.elapsed,delta,true)>=slot.duration)ready|=uint64_t{1}<<slot.mountOrdinal;
            }
            for(int distance=0;distance<std::min(window,available);++distance) {
                const auto& loading=group.slots[(cursor+distance)%count];
                if(dualsp::advanceReload(loading.duration,loading.elapsed,delta,true)>=loading.duration)continue;
                const auto& fired=group.slots[(cursor+distance+count-window)%count];
                require(fired.mountOrdinal>=0 && fired.mountOrdinal<count,"机外保留：补装池序号无效");
                vacant|=uint64_t{1}<<fired.mountOrdinal;
            }
            mask=(mask&~vacant)|ready;
        }
    }
    state.readyMask=mask;mountApplyVisuals(state);state.animatedVisualAudit=now;
}
