// 原生局部机构：只保登记供位架的真实动画与Socket，不接管机身姿态或强制全开。
// 在反射、轻量Socket读取器和旧动画入口定义后包含。
#pragma once

struct F22ClosedSideBone {
    int index{};
    std::array<float,4> rotation{};
    std::array<float,3> translation{};
};
struct LocalMountOwnership {
    MountIdentity plane,base,component,componentClass,mesh,skeleton,anim;
    size_t rackOffset{},meshOffset{},skeletonOffset{},animOffset{};
    int pool{2}; // 既有四机默认SWP3；普通Su-57限定为SWP1。
    MountAnimationReflection reflection;
    LightMountSocketReader sockets;
    MountAnimationReflection::Field refresh;
    MountIdentity refreshEnum;
    unsigned always{};
    bool owned{},nativeNoSkeleton{},nativeTick{};
    unsigned char nativeRefresh{},writtenRefresh{};
    std::map<Addr,MountAnimationReflection::SocketName> nativeSockets;
    std::array<F22ClosedSideBone,14> f22ClosedSide{};
    bool f22SideQualified{},f22SideFault{};
};
bool mountLocalWriting=false; // 本模块的写入不记作新的原生请求。
struct LocalMountWriteScope {
    bool prior{mountLocalWriting};
    LocalMountWriteScope() { mountLocalWriting=true; }
    ~LocalMountWriteScope() { mountLocalWriting=prior; }
};

void mountLocalCurrent(const LocalMountOwnership& lease) {
    const MountIdentityBatch ids;
    require(ids.alive(lease.plane) && ids.alive(lease.base) && ids.alive(lease.component) &&
        ids.alive(lease.componentClass) && ids.alive(lease.mesh) && ids.alive(lease.skeleton) && ids.alive(lease.anim),
        "局部机构：机体、组件或资源身份失效");
    require(ids.read<Addr>(lease.plane.address+lease.rackOffset)==lease.component.address &&
        ids.read<Addr>(lease.plane.address+0x1ae0)==lease.base.address &&
        ids.read<Addr>(lease.component.address+0x10)==lease.componentClass.address &&
        ids.read<Addr>(lease.component.address+0x100)==lease.base.address &&
        ids.read<Addr>(lease.component.address+lease.meshOffset)==lease.mesh.address &&
        ids.read<Addr>(lease.mesh.address+lease.skeletonOffset)==lease.skeleton.address &&
        ids.read<Addr>(lease.component.address+lease.animOffset)==lease.anim.address,
        "局部机构：挂架父链、网格、骨架或动画类改变");
    require(ids.alive(lease.refresh.identity) && ids.alive(lease.refreshEnum) &&
        ids.read<int>(lease.refresh.identity.address+0x44)==lease.refresh.offset &&
        ids.read<int>(lease.refresh.identity.address+0x34)==1,"局部机构：骨刷新元数据改变");
}

// 骨名与父链只在绑定时核对一次，不逐帧遍历骨骼。
void mountLocalQualifyBones(LocalMountOwnership& lease,const Json& group) {
    auto& api=lease.reflection;const auto object=lease.component;
    const auto count=api.function(object.address,"GetNumBones","SkinnedMeshComponent",1);
    const auto countResult=api.field(count.identity.address,"ReturnValue","IntProperty",4,count.size);
    const auto bone=api.function(object.address,"GetBoneName","SkinnedMeshComponent",2);
    const auto index=api.field(bone.identity.address,"BoneIndex","IntProperty",4,bone.size);
    const auto boneResult=api.field(bone.identity.address,"ReturnValue","NameProperty",8,bone.size);
    const auto parent=api.function(object.address,"GetParentBone","SkinnedMeshComponent",2);
    const auto parentInput=api.field(parent.identity.address,"BoneName","NameProperty",8,parent.size);
    const auto parentResult=api.field(parent.identity.address,"ReturnValue","NameProperty",8,parent.size);
    MountAnimationReflection::separate(index,boneResult);MountAnimationReflection::separate(parentInput,parentResult);
    alignas(16) std::array<unsigned char,256> params{};
    api.invoke(object,count,params.data());int actual{};std::memcpy(&actual,params.data()+countResult.offset,4);
    const auto& expected=group.at("bones");
    require(actual>0 && actual<=64 && actual==static_cast<int>(expected.size()),"局部机构：原生骨数与资源不符");
    std::vector<MountAnimationReflection::SocketName> names;
    for(int i=0;i<actual;++i) {
        params={};std::memcpy(params.data()+index.offset,&i,4);api.invoke(object,bone,params.data());
        MountAnimationReflection::SocketName name{};std::memcpy(&name,params.data()+boneResult.offset,8);
        require(LightMountBones::label(name)==expected.at(i).at("name").get<std::string>(),"局部机构：骨名或骨序改变");
        names.push_back(name);
        params={};std::memcpy(params.data()+parentInput.offset,&name,8);api.invoke(object,parent,params.data());
        MountAnimationReflection::SocketName found{};std::memcpy(&found,params.data()+parentResult.offset,8);
        const int p=expected.at(i).at("parent").get<int>();
        require(p>=-1 && p<i && (p<0?LightMountBones::label(found)=="None":LightMountBones::same(found,names.at(p))),
            "局部机构：原生骨父链改变");
    }
}

void mountLocalBind(FixedMountState& state,Addr plane) {
    const auto& definition=mountExpansionDefinition(state.aircraft);
    require(mountLocal(state.aircraft) && definition.value("local_mechanism",false),"局部机构：机型未登记");
    if(definition.contains("body")) {
        const auto& body=definition.at("body");
        const Addr component=read<Addr>(plane+mountProperty(plane,"PlaneBodyMesh","ObjectProperty",8));
        require(mountClassIs(component,"SkeletalMeshComponent","/Script/Engine"),"局部机构：本体类型不符");
        const Addr mesh=read<Addr>(component+mountProperty(component,"SkeletalMesh","ObjectProperty",8));
        const Addr anim=read<Addr>(component+mountProperty(component,"AnimClass","ClassProperty",8));
        mountAnimatedResource(mesh,body.at("mesh").get<std::string>(),body.at("mesh_package").get<std::string>(),"SkeletalMesh");
        mountAnimatedResource(anim,body.at("anim").get<std::string>(),body.at("anim_package").get<std::string>(),"AnimBlueprintGeneratedClass");
    }
    const int pool=definition.value("local_pool",2);
    require(pool>=0 && pool<3,"局部机构：供位池越界");
    const auto& group=definition.at("groups").at(pool);
    auto lease=std::make_shared<LocalMountOwnership>();
    lease->pool=pool;lease->plane=state.plane;lease->base=state.weaponBase;lease->rackOffset=state.rackOffsets[pool];
    lease->component=mountIdentity(read<Addr>(plane+lease->rackOffset));
    const Addr object=lease->component.address;
    require(mountClassIs(object,"SkeletalMeshComponent","/Script/Engine"),"局部机构：不是原生骨架组件");
    lease->componentClass=mountIdentity(read<Addr>(object+0x10));
    lease->meshOffset=mountProperty(object,"SkeletalMesh","ObjectProperty",8);
    lease->animOffset=mountProperty(object,"AnimClass","ClassProperty",8);
    lease->mesh=mountIdentity(read<Addr>(object+lease->meshOffset));
    lease->skeletonOffset=mountProperty(lease->mesh.address,"Skeleton","ObjectProperty",8);
    lease->skeleton=mountIdentity(read<Addr>(lease->mesh.address+lease->skeletonOffset));
    lease->anim=mountIdentity(read<Addr>(object+lease->animOffset));
    const auto code=definition.at("code").get<std::string>();
    const auto package="/Game/Vehicles/Aircraft/"+code+"/";
    const auto rackName=code+"_SWP"+std::to_string(pool+1);
    mountAnimatedResource(lease->mesh.address,group.at("rack").get<std::string>(),package+rackName,"SkeletalMesh");
    mountAnimatedResource(lease->skeleton.address,group.at("skeleton").get<std::string>(),package+rackName+"_Skeleton","Skeleton");
    // 原生AnimBP在Blueprint/Player/Body，不能按网格目录拼接其所属包。
    mountAnimatedResource(lease->anim.address,group.at("anim").get<std::string>(),
        "/Game/Blueprint/Player/Body/"+rackName+"_AnimBP","AnimBlueprintGeneratedClass");
    auto& api=lease->reflection;api.bindSdk();
    const int extent=api.typeSize(lease->componentClass.address);
    const auto no=api.boolean(lease->componentClass.address,"bNoSkeletonUpdate",extent,true);
    const auto pause=api.boolean(lease->componentClass.address,"bPauseAnims",extent,true);
    require(no.field.offset==0x9b0 && no.fieldMask==2 && pause.field.offset==0x9b0 && pause.fieldMask==4,
        "局部机构：骨架更新位布局改变");
    api.tick(lease->component); // 一次核对原生Tick反射入口。
    const char* property=MountAnimationReflection::find(lease->componentClass.address,"MeshComponentUpdateFlag")?
        "MeshComponentUpdateFlag":"VisibilityBasedAnimTickOption";
    lease->refresh=api.field(lease->componentClass.address,property,"ByteProperty",1,extent,true);
    const Addr enumeration=MountAnimationReflection::refValue<Addr>(api.enumRef,lease->refresh.identity.address);
    lease->refreshEnum=mountIdentity(enumeration);
    lease->always=api.enumValue(enumeration,property==std::string("MeshComponentUpdateFlag")?
        "EMeshComponentUpdateFlag":"EVisibilityBasedAnimTickOption","AlwaysTickPoseAndRefreshBones");
    require(lease->always<=255,"局部机构：骨刷新枚举越界");
    mountLocalQualifyBones(*lease,group);
    if(state.aircraft==MountAircraft::F22a) {
        require(mountProperty(object,"MasterPoseComponent","WeakObjectProperty",8)==0x720 &&
            read<int>(object+0x724)==0 && (read<int>(object+0x720)==0 || read<int>(object+0x720)==-1),
            "F22侧舱：不支持主姿态共享组件");
        const auto& poses=definition.at("closed_side_pose");
        require(pool==2 && poses.size()==lease->f22ClosedSide.size(),"F22侧舱：关闭姿态数量不符");
        std::bitset<30> seen;
        for(size_t i=0;i<poses.size();++i) {
            const auto& pose=poses.at(i);auto& bone=lease->f22ClosedSide[i];
            bone.index=pose.at("index").get<int>();
            require(bone.index>0 && bone.index<30 && !seen.test(bone.index) &&
                group.at("bones").at(bone.index).at("name")==pose.at("name"),"F22侧舱：骨序不符");
            seen.set(bone.index);bone.rotation=pose.at("rotation").get<std::array<float,4>>();
            bone.translation=pose.at("translation").get<std::array<float,3>>();
            float norm=0;for(float v:bone.rotation) {require(std::isfinite(v),"F22侧舱：旋转无效");norm+=v*v;}
            for(float v:bone.translation)require(std::isfinite(v),"F22侧舱：位置无效");
            require(std::abs(norm-1.f)<.001f,"F22侧舱：旋转不是单位四元数");
        }
        lease->f22SideQualified=true;
    }
    lease->sockets.prepare(api,lease->component);
    for(const auto& visual:state.visuals)if(visual.pool==pool && visual.index>=0) {
        MountAnimationReflection::SocketName name{read<unsigned>(visual.identity.address+0x108),read<unsigned>(visual.identity.address+0x10c)};
        require(!name.number && lease->sockets.exists(api,lease->component,name),"局部机构：原生弹体挂点不存在");
        lease->nativeSockets.emplace(visual.identity.address,name);
    }
    mountLocalCurrent(*lease);
    // 同实例的任务/补给预览共用一份原生状态，不把自己强制的值当作恢复基线。
    for(auto* prior:{&fixedMount,&mountPreview.layout})if((state.rackMask&(1u<<pool)) && prior!=&state && prior->local &&
        prior->local->pool==pool &&
        mountMeshIdentityEqual(prior->local->component,lease->component) &&
        mountMeshIdentityEqual(prior->local->mesh,lease->mesh) && mountMeshIdentityEqual(prior->local->anim,lease->anim) &&
        mountMeshIdentityEqual(prior->local->plane,lease->plane)) {
        bool same=prior->local->nativeSockets.size()==lease->nativeSockets.size();
        for(const auto& point:lease->nativeSockets) {
            const auto found=prior->local->nativeSockets.find(point.first);
            same=same && found!=prior->local->nativeSockets.end() && LightMountBones::same(found->second,point.second);
        }
        if(same) { state.local=prior->local;return; }
    }
    state.local=std::move(lease);
}

void mountLocalMaintain(FixedMountState& state) {
    if(!mountLocal(state.aircraft) || !state.local)return;
    if(!(state.rackMask&(1u<<state.local->pool))) {
        if(&state==&mountPreview.layout && mountPreviewOwnsTask())mountLocalBeforeRebuild(fixedMount);
        return;
    }
    auto& lease=*state.local;mountLocalCurrent(lease);
    require(!lease.f22SideFault,"F22侧舱：姿态发布校验失败，已退回原生外观");
    mountAnimationInstallHooks(); // 复用按实例过滤的Tick/停用观察入口，不启动旧完整共舱控制器。
    const auto flags=read<unsigned char>(lease.component.address+0x9b0);
    const bool no=(flags&2)!=0,tick=read<unsigned char>(lease.component.address+0x3f)!=0;
    const auto refresh=read<unsigned char>(lease.component.address+lease.refresh.offset);
    if(!lease.owned) {
        lease.nativeNoSkeleton=no;lease.nativeTick=tick;lease.nativeRefresh=refresh;
        lease.writtenRefresh=refresh;lease.owned=true;
    } else if(refresh!=lease.writtenRefresh)lease.nativeRefresh=refresh;
    // bPauseAnims始终保留原生值；暂停/过场请求不会被改成继续播放。
    LocalMountWriteScope writing;
    if(no || !tick)mountSetRackAnimation(lease.component.address,false,true);
    if(refresh!=lease.always) {
        MountAnimationReflection::alive(lease.refresh.identity);MountAnimationReflection::alive(lease.refreshEnum);
        require(MountAnimationReflection::writable(lease.component.address+lease.refresh.offset,1),"局部机构：骨刷新设置不可写");
        write<unsigned char>(lease.component.address+lease.refresh.offset,static_cast<unsigned char>(lease.always));
    }
    lease.writtenRefresh=static_cast<unsigned char>(lease.always);
}

void mountLocalReturn(LocalMountOwnership& lease) {
    if(lease.owned && mountAlive(lease.component)) {
        mountLocalCurrent(lease);LocalMountWriteScope writing;
        const auto no=(read<unsigned char>(lease.component.address+0x9b0)&2)!=0;
        const auto tick=read<unsigned char>(lease.component.address+0x3f)!=0;
        if(no!=lease.nativeNoSkeleton || tick!=lease.nativeTick)
            mountSetRackAnimation(lease.component.address,lease.nativeNoSkeleton,lease.nativeTick);
        const auto refresh=read<unsigned char>(lease.component.address+lease.refresh.offset);
        if(refresh==lease.writtenRefresh && refresh!=lease.nativeRefresh) {
            require(MountAnimationReflection::writable(lease.component.address+lease.refresh.offset,1),"局部机构：骨刷新归还不可写");
            write<unsigned char>(lease.component.address+lease.refresh.offset,lease.nativeRefresh);
        }
        lease.owned=false;
    }
}
void mountLocalBeforeRebuild(FixedMountState& state) {
    if(state.local)mountLocalReturn(*state.local); // 保留资格绑定，Init后重新捕获真实原生状态。
}
void mountLocalRelease(FixedMountState& state) {
    if(!state.local)return;
    if(state.local.use_count()>1) {state.local.reset();return;}
    mountLocalReturn(*state.local);
    state.local.reset(); // 失败在此前抛出，保留租约供既有归还路径重试。
}

void mountLocalRemember(void* component,unsigned fields,bool tick) noexcept {
    if(mountLocalWriting || GetCurrentThreadId()!=mountGameThread.load())return;
    for(auto* state:{&fixedMount,&mountPreview.layout})if(state->local && state->local->owned &&
        state->local->component.address==reinterpret_cast<Addr>(component))try {
        auto& lease=*state->local;mountLocalCurrent(lease);
        if(fields&1u)lease.nativeTick=tick;
        if(fields&2u)lease.nativeNoSkeleton=(read<unsigned char>(lease.component.address+0x9b0)&2)!=0;
        // 停用函数尚有后续直接写入；最外层返回后再补齐，不在Tick中间递归原生停用。
        if(!mountAnimationNativeDepth && state->bound && !state->rebuilding && !mountPreviewRestoring)
            mountLocalMaintain(*state);
        return;
    }catch(const std::exception& error) {
        if(state==&mountPreview.layout)mountPreviewFail(error.what());else fixedMountFail(error.what());return;
    }
}

bool mountLocalCopyTransform(void* output,const FixedMountState& state,int position) {
    require(state.local && position>=0 && position<state.physicalCount,"局部机构：发射位置未绑定");
    const Addr component=state.physical[position];
    const auto visual=std::find_if(state.visuals.begin(),state.visuals.end(),[&](const MountVisual& v){return v.identity.address==component;});
    require(visual!=state.visuals.end() && mountAlive(visual->identity),"局部机构：发射弹体身份失效");
    if(visual->pool!=state.local->pool) {
        checkMemory(component+0x180,48);checkMemory(reinterpret_cast<Addr>(output),48);
        std::memcpy(output,reinterpret_cast<void*>(component+0x180),48);return true;
    }
    auto& lease=*state.local;mountLocalCurrent(lease);
    require(read<Addr>(component+0x100)==lease.component.address,"局部机构：发射挂点父级改变");
    // 原生骨骼尚未发布完成时旁路本次映射；不等候、不补帧、不改变发射门槛。
    const auto frame=read<std::uint64_t>(reinterpret_cast<Addr>(GetModuleHandleW(nullptr))+0x3ac5520);
    if(read<unsigned>(lease.component.address+0xd24)!=static_cast<unsigned>(frame) ||
       read<unsigned char>(lease.component.address+0x828)!=0)return false;
    MountAnimationReflection::SocketName socket{read<unsigned>(component+0x108),read<unsigned>(component+0x10c)};
    const auto found=lease.nativeSockets.find(component);
    require(!socket.number && found!=lease.nativeSockets.end() && LightMountBones::same(found->second,socket),
        "局部机构：发射挂点身份改变");
    lease.sockets.copy(lease.reflection,lease.component,socket,output);return true;
}

// 只修改原生即将发布的组件空间侧骨；原生发布负责后续附着、通知与渲染。
// 仅任务内已绑定的原版F-22有效；开火取位和腹舱骨继续使用同一原生发布。
void mountLocalBeforePublish(void* component) noexcept {
    if(GetCurrentThreadId()!=mountGameThread.load() || !runtimeEnabled || !fixedMountEnabled || fixedMountFailed ||
       controlledWeaponSwitch || !fixedMount.bound || fixedMount.rebuilding || fixedMount.aircraft!=MountAircraft::F22a ||
       !fixedMount.local || mountPreviewOwnsTask())return;
    auto& lease=*fixedMount.local;
    if(!lease.owned || !lease.f22SideQualified || lease.f22SideFault ||
       lease.component.address!=reinterpret_cast<Addr>(component))return;
    try {
        const MountIdentityBatch ids;
        require(ids.alive(lease.plane) && ids.alive(lease.component) && ids.alive(lease.mesh) && ids.alive(lease.anim) &&
            ids.alive(fixedMount.animationActivator),"F22侧舱：实例身份失效");
        require(session.plane==lease.plane.address && session.activator==fixedMount.animationActivator.address &&
            ids.read<Addr>(lease.plane.address+lease.rackOffset)==lease.component.address &&
            ids.read<Addr>(lease.component.address+lease.meshOffset)==lease.mesh.address &&
            ids.read<Addr>(lease.component.address+lease.animOffset)==lease.anim.address,"F22侧舱：会话或资源改变");
        const Addr engine=ids.read<Addr>(lease.plane.address+0x1410),parts=engine+0xf90;
        const Addr base=reinterpret_cast<Addr>(GetModuleHandleW(nullptr));
        require(engine && ids.read<Addr>(engine+0x1268)==lease.plane.address+0xb40 &&
            ids.read<Addr>(parts)==base+0x2fff178,"F22侧舱：引擎身份改变");
        const int config=ids.read<int>(parts+0x178);
        const auto armed=ids.read<unsigned char>(session.activator+0x2a1);
        require(config>=0 && config<3 && armed<=1,"F22侧舱：选择状态无效");
        if(armed && config==2)return; // 选中8AAM时完整保留原生活动姿态。
        const Addr object=lease.component.address;
        if(ids.read<unsigned char>(object+0x828)!=1)return; // 没有待发布姿态时不触碰缓冲。
        const auto frame=ids.read<std::uint64_t>(base+0x3ac5520);
        if(ids.read<unsigned>(object+0xd24)!=static_cast<unsigned>(frame))return;
        const auto doubleBuffered=ids.read<unsigned char>(object+0x750);
        const int index=ids.read<int>(object+0x748);
        const int published=ids.read<int>(object+0x74c);
        require(doubleBuffered==1 && index>=0 && index<=1 && published>=0 && published<=1 && index!=published &&
            ids.read<int>(object+0x724)==0 && (ids.read<int>(object+0x720)==0 || ids.read<int>(object+0x720)==-1),
            "F22侧舱：要求独立双缓冲及未共享的待发布姿态");
        const Addr array=object+0x728+static_cast<Addr>(index)*16;
        const Addr data=ids.read<Addr>(array);const int count=ids.read<int>(array+8),capacity=ids.read<int>(array+12);
        require(count==30 && capacity>=count && capacity<=1000 && data &&
            MountAnimationReflection::writable(data,static_cast<size_t>(count)*48),"F22侧舱：待发布姿态不可写");
        require(ids.read<unsigned char>(object+0x828)==1 && ids.read<int>(object+0x748)==index &&
            ids.read<int>(object+0x74c)==published && ids.read<Addr>(array)==data,
            "F22侧舱：写入前发布状态改变");
        for(const auto& bone:lease.f22ClosedSide) {
            const Addr transform=data+static_cast<Addr>(bone.index)*48;
            // 仅旋转与位移被动画驱动；原生缩放和填充字节保持不动。
            if(std::memcmp(reinterpret_cast<void*>(transform),bone.rotation.data(),sizeof(bone.rotation))!=0)
                std::memcpy(reinterpret_cast<void*>(transform),bone.rotation.data(),sizeof(bone.rotation));
            if(std::memcmp(reinterpret_cast<void*>(transform+16),bone.translation.data(),sizeof(bone.translation))!=0)
                std::memcpy(reinterpret_cast<void*>(transform+16),bone.translation.data(),sizeof(bone.translation));
        }
    }catch(...) {lease.f22SideFault=true;} // 下次维护统一退回原生，不在引擎发布栈中归还资源。
}
