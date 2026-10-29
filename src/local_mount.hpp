// 原生局部机构：只保登记供位架的真实动画与Socket，不接管机身姿态或强制全开。
// 在反射、轻量Socket读取器和旧动画入口定义后包含。
#pragma once

struct F22ClosedSideBone {
    int index{};
    std::array<float,4> rotation{};
    std::array<float,3> translation{};
};
struct LocalPoseKey {
    float alpha{};
    std::array<float,4> rotation{};
    std::array<float,3> position{};
};
struct F22AnimatedBone {
    int index{};
    std::array<float,4> closedRotation{},openRotation{};
    std::array<float,3> closedPosition{},openPosition{};
    std::array<LocalPoseKey,5> keys{};
    bool hasKeys{};
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
    MountIdentity f22Body,f22BodyMesh,f22BodyAnim;
    size_t f22BodyOffset{},f22BodyMeshOffset{},f22BodyAnimOffset{};
    std::array<F22AnimatedBone,14> f22SideAnimation{};
    std::array<F22AnimatedBone,15> f22RackBellyAnimation{};
    std::array<F22AnimatedBone,9> f22BodyAnimation{};
    std::array<F22AnimatedBone,10> pkfaRackAnimation{};
    bool pkfaRackQualified{},pkfaRackFault{};
    double f22LastTime{};
    float f22SideProgress{},f22BellyProgress{};
    bool f22ClockReady{};
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
void mountLocalQualifyBones(LocalMountOwnership& lease,const Json& group,MountIdentity object={}) {
    auto& api=lease.reflection;if(!object.address)object=lease.component;
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

template<size_t N>
void mountLocalParsePose(const Json& rows,std::array<F22AnimatedBone,N>& target,const Json& bones) {
            require(rows.size()==target.size(),"内舱：端点数量不符");
            std::bitset<64> assigned;
            for(size_t i=0;i<rows.size();++i) {
                const auto& row=rows.at(i);auto& dst=target[i];dst.index=row.at("index").get<int>();
                require(dst.index>0 && dst.index<static_cast<int>(bones.size()) && !assigned.test(dst.index) &&
                    bones.at(dst.index).at("name")==row.at("name"),"内舱：端点骨序不符");
                assigned.set(dst.index);
                const auto& a=row.at("closed");const auto& b=row.at("open");
                dst.closedRotation=a.at("rotation").get<std::array<float,4>>();dst.openRotation=b.at("rotation").get<std::array<float,4>>();
                dst.closedPosition=a.at("translation").get<std::array<float,3>>();dst.openPosition=b.at("translation").get<std::array<float,3>>();
                for(const auto& q:{dst.closedRotation,dst.openRotation}) {
                    float norm=0;for(float v:q) {require(std::isfinite(v),"内舱：端点旋转无效");norm+=v*v;}
                    require(std::abs(norm-1.f)<.001f,"内舱：端点旋转不是单位四元数");
                }
                for(const auto& pos:{dst.closedPosition,dst.openPosition})for(float v:pos)require(std::isfinite(v),"内舱：端点位置无效");
                if(row.contains("keys")) {
                    const auto& keys=row.at("keys");require(keys.size()==dst.keys.size(),"内舱：动画关键点数量不符");
                    float previous=-1.f;
                    for(size_t k=0;k<keys.size();++k) {
                        auto& key=dst.keys[k];key.alpha=keys.at(k).at("alpha").get<float>();
                        key.rotation=keys.at(k).at("rotation").get<std::array<float,4>>();
                        key.position=keys.at(k).at("translation").get<std::array<float,3>>();
                        require(std::isfinite(key.alpha) && key.alpha>=0.f && key.alpha<=1.f && key.alpha>previous,"内舱：动画关键点顺序无效");
                        previous=key.alpha;float norm=0;
                        for(float v:key.rotation) {require(std::isfinite(v),"内舱：关键点旋转无效");norm+=v*v;}
                        for(float v:key.position)require(std::isfinite(v),"内舱：关键点位置无效");
                        require(std::abs(norm-1.f)<.001f,"内舱：关键点旋转不是单位四元数");
                    }
                    require(dst.keys.front().alpha==0.f && dst.keys.back().alpha==1.f,"内舱：动画缺少两端点");
                    dst.hasKeys=true;
                }
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
        lease->f22BodyOffset=mountProperty(plane,"PlaneBodyMesh","ObjectProperty",8);
        lease->f22Body=mountIdentity(read<Addr>(plane+lease->f22BodyOffset));
        lease->f22BodyMeshOffset=mountProperty(lease->f22Body.address,"SkeletalMesh","ObjectProperty",8);
        lease->f22BodyAnimOffset=mountProperty(lease->f22Body.address,"AnimClass","ClassProperty",8);
        lease->f22BodyMesh=mountIdentity(read<Addr>(lease->f22Body.address+lease->f22BodyMeshOffset));
        lease->f22BodyAnim=mountIdentity(read<Addr>(lease->f22Body.address+lease->f22BodyAnimOffset));
        require(mountProperty(lease->f22Body.address,"MasterPoseComponent","WeakObjectProperty",8)==0x720 &&
            read<int>(lease->f22Body.address+0x724)==0 &&
            (read<int>(lease->f22Body.address+0x720)==0 || read<int>(lease->f22Body.address+0x720)==-1),
            "F22腹舱：不支持主姿态共享组件");
        mountLocalQualifyBones(*lease,definition.at("body"),lease->f22Body);
        mountLocalParsePose(definition.at("feedback_side_pose"),lease->f22SideAnimation,group.at("bones"));
        mountLocalParsePose(definition.at("feedback_rack_belly_pose"),lease->f22RackBellyAnimation,group.at("bones"));
        mountLocalParsePose(definition.at("body").at("feedback_pose"),lease->f22BodyAnimation,definition.at("body").at("bones"));
        lease->f22SideQualified=true;
    } else if(state.aircraft==MountAircraft::Pkfa) {
        require(pool==0 && mountProperty(object,"MasterPoseComponent","WeakObjectProperty",8)==0x720 &&
            read<int>(object+0x724)==0 && (read<int>(object+0x720)==0 || read<int>(object+0x720)==-1),
            "Su-57供位架：不支持主姿态共享组件");
        mountLocalParsePose(definition.at("feedback_rack_pose"),lease->pkfaRackAnimation,group.at("bones"));
        lease->pkfaRackQualified=true;
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
    require(!lease.f22SideFault && !lease.pkfaRackFault,"内舱：姿态发布校验失败，已退回原生外观");
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
    // 发射读取原生已发布缓冲；待发布任务不影响旧缓冲，不因帧号差异退回错误的原生站位。
    const Addr rack=lease.component.address;
    if(state.aircraft!=MountAircraft::F22a && state.aircraft!=MountAircraft::Pkfa) {
        const auto frame=read<std::uint64_t>(reinterpret_cast<Addr>(GetModuleHandleW(nullptr))+0x3ac5520);
        if(read<unsigned>(rack+0xd24)!=static_cast<unsigned>(frame) || read<unsigned char>(rack+0x828)!=0)return false;
    }
    const int published=read<int>(rack+0x74c),writing=read<int>(rack+0x748);
    require(published>=0 && published<=1,"局部机构：已发布缓冲索引无效");
    if(state.aircraft==MountAircraft::F22a || state.aircraft==MountAircraft::Pkfa)
        require(read<unsigned char>(rack+0x750)==1 && writing>=0 && writing<=1 && writing!=published,
            "局部机构：发射取位需要独立已发布缓冲");
    const Addr array=rack+0x728+static_cast<Addr>(published)*16;
    const Addr pose=read<Addr>(array);const int count=read<int>(array+8),capacity=read<int>(array+12);
    require(pose && count>0 && count<=capacity && capacity<=1000,"局部机构：已发布姿态无效");
    MountAnimationReflection::SocketName socket{read<unsigned>(component+0x108),read<unsigned>(component+0x10c)};
    const auto found=lease.nativeSockets.find(component);
    require(!socket.number && found!=lease.nativeSockets.end() && LightMountBones::same(found->second,socket),
        "局部机构：发射挂点身份改变");
    alignas(16) std::array<unsigned char,48> transform{};
    lease.sockets.copy(lease.reflection,lease.component,socket,transform.data());
    require(read<int>(rack+0x74c)==published && read<Addr>(array)==pose &&
        ((state.aircraft!=MountAircraft::F22a && state.aircraft!=MountAircraft::Pkfa) ||
         (read<unsigned char>(rack+0x750)==1 && read<int>(rack+0x748)==writing)),"局部机构：取位期间姿态换代");
    checkMemory(reinterpret_cast<Addr>(output),transform.size());std::memcpy(output,transform.data(),transform.size());return true;
}

template<size_t N>
void mountLocalApplyPose(Addr data,const std::array<F22AnimatedBone,N>& bones,float alpha) {
            for(const auto& bone:bones) {
                std::array<float,4> rotation{};std::array<float,3> position{};
                const auto* first=&bone.closedRotation;const auto* last=&bone.openRotation;
                const auto* firstPosition=&bone.closedPosition;const auto* lastPosition=&bone.openPosition;
                float blend=alpha;
                if(bone.hasKeys) {
                    size_t segment=0;while(segment+1<bone.keys.size()-1 && alpha>bone.keys[segment+1].alpha)++segment;
                    const auto& a=bone.keys[segment];const auto& b=bone.keys[segment+1];
                    first=&a.rotation;last=&b.rotation;firstPosition=&a.position;lastPosition=&b.position;
                    blend=std::max(0.f,std::min(1.f,(alpha-a.alpha)/(b.alpha-a.alpha)));
                }
                if(blend<=0.f) {rotation=*first;position=*firstPosition;}
                else if(blend>=1.f) {rotation=*last;position=*lastPosition;}
                else {
                    float dot=0,norm=0;for(int i=0;i<4;++i)dot+=(*first)[i]*(*last)[i];
                    const float sign=dot<0.f?-1.f:1.f;
                    for(int i=0;i<4;++i) {rotation[i]=(1.f-blend)*(*first)[i]+blend*sign*(*last)[i];norm+=rotation[i]*rotation[i];}
                    require(std::isfinite(norm) && norm>.0001f,"F22舱门：插值旋转无效");
                    const float inv=1.f/std::sqrt(norm);for(float& v:rotation)v*=inv;
                    for(int i=0;i<3;++i)position[i]=(1.f-blend)*(*firstPosition)[i]+blend*(*lastPosition)[i];
                }
                const Addr transform=data+static_cast<Addr>(bone.index)*48;
                if(std::memcmp(reinterpret_cast<void*>(transform),rotation.data(),sizeof(rotation))!=0)
                    std::memcpy(reinterpret_cast<void*>(transform),rotation.data(),sizeof(rotation));
                if(std::memcmp(reinterpret_cast<void*>(transform+16),position.data(),sizeof(position))!=0)
                    std::memcpy(reinterpret_cast<void*>(transform+16),position.data(),sizeof(position));
            }
}

// 使用游戏时间驱动两舱独立开合；暂停不推进，不触碰原生发射门槛。
void mountLocalUpdateF22(FixedMountState& state,double time) {
    if(state.aircraft!=MountAircraft::F22a || !state.local || state.rebuilding || controlledWeaponSwitch)return;
    auto& lease=*state.local;
    require(std::isfinite(time) && mountAlive(lease.plane) && session.plane==lease.plane.address &&
        session.activator==state.animationActivator.address,"F22：舱门计时会话无效");
    const Addr engine=read<Addr>(lease.plane.address+0x1410),parts=engine+0xf90;
    const Addr base=reinterpret_cast<Addr>(GetModuleHandleW(nullptr));
    require(engine && read<Addr>(engine+0x1268)==lease.plane.address+0xb40 && read<Addr>(parts)==base+0x2fff178,
        "F22：舱门计时引擎无效");
    const int config=read<int>(parts+0x178);const auto armed=read<unsigned char>(session.activator+0x2a1);
    const float native=read<float>(parts+0x17c);const int request=read<int>(parts+0x58);
    const bool held=(read<unsigned>(parts+0xb0)&(1u<<20))!=0;
    require(config>=0 && config<3 && armed<=1 && request>=0 && request<=3 &&
        std::isfinite(native) && native>=-1.f && native<=1.f,"F22：舱门计时状态无效");
    const float current=std::max(0.f,std::min(1.f,native));
    const bool mayOpen=armed && !held && request!=3;
    const bool sideSelected=mayOpen && config==2,bellySelected=mayOpen && config!=2;
    if(!lease.f22ClockReady || time<lease.f22LastTime) {
        lease.f22ClockReady=true;lease.f22LastTime=time;
        lease.f22SideProgress=sideSelected?current:0.f;
        lease.f22BellyProgress=bellySelected?current:0.f;
        return;
    }
    const float step=static_cast<float>(std::min(.4,std::max(0.0,time-lease.f22LastTime))/.4);
    lease.f22LastTime=time;
    lease.f22SideProgress=sideSelected?std::min(1.f,lease.f22SideProgress+step):std::max(0.f,lease.f22SideProgress-step);
    lease.f22BellyProgress=bellySelected?std::min(1.f,lease.f22BellyProgress+step):std::max(0.f,lease.f22BellyProgress-step);
}

// 原生双缓冲发布前同步F-22两舱及Su-57供位架，不改武器队列与原生发布。
void mountLocalBeforePublish(void* component) noexcept {
    if(GetCurrentThreadId()!=mountGameThread.load() || !runtimeEnabled || !fixedMountEnabled || fixedMountFailed ||
       controlledWeaponSwitch || !fixedMount.bound || fixedMount.rebuilding ||
       (fixedMount.aircraft!=MountAircraft::F22a && fixedMount.aircraft!=MountAircraft::Pkfa) ||
       !fixedMount.local || mountPreviewOwnsTask())return;
    auto& lease=*fixedMount.local;const Addr object=reinterpret_cast<Addr>(component);
    const bool pkfa=fixedMount.aircraft==MountAircraft::Pkfa;
    const bool side=object==lease.component.address,belly=!pkfa && object==lease.f22Body.address;
    if(!lease.owned || (!side && !belly))return;
    if(pkfa ? (!lease.pkfaRackQualified || lease.pkfaRackFault) :
       (!lease.f22SideQualified || lease.f22SideFault || !lease.f22ClockReady))return;
    try {
        const MountIdentityBatch ids;
        require(ids.alive(lease.plane) && ids.alive(lease.component) && ids.alive(lease.mesh) && ids.alive(lease.anim) &&
            ids.alive(fixedMount.animationActivator),"内舱：实例身份失效");
        require(session.plane==lease.plane.address && session.activator==fixedMount.animationActivator.address &&
            ids.read<Addr>(lease.plane.address+lease.rackOffset)==lease.component.address &&
            ids.read<Addr>(lease.component.address+lease.meshOffset)==lease.mesh.address &&
            ids.read<Addr>(lease.component.address+lease.animOffset)==lease.anim.address,"内舱：会话或资源改变");
        if(!pkfa)require(ids.alive(lease.f22Body) && ids.alive(lease.f22BodyMesh) && ids.alive(lease.f22BodyAnim) &&
            ids.read<Addr>(lease.plane.address+lease.f22BodyOffset)==lease.f22Body.address &&
            ids.read<Addr>(lease.f22Body.address+lease.f22BodyMeshOffset)==lease.f22BodyMesh.address &&
            ids.read<Addr>(lease.f22Body.address+lease.f22BodyAnimOffset)==lease.f22BodyAnim.address,
            "F22舱门：机身资源改变");
        const Addr engine=ids.read<Addr>(lease.plane.address+0x1410),parts=engine+0xf90;
        const Addr base=reinterpret_cast<Addr>(GetModuleHandleW(nullptr));
        require(engine && ids.read<Addr>(engine+0x1268)==lease.plane.address+0xb40 &&
            ids.read<Addr>(parts)==base+0x2fff178,"F22舱门：引擎身份改变");
        if(!mountLocalBayQualified(fixedMount,engine))return; // 舱门会话未就绪时保留原生姿态。
        const int config=ids.read<int>(parts+0x178);const auto armed=ids.read<unsigned char>(session.activator+0x2a1);
        require(config>=0 && config<3 && armed<=1,"F22舱门：选择状态无效");
        const float native=ids.read<float>(parts+0x17c);
        const float rawBay=pkfa?ids.read<float>(parts+0x194):0.f;
        require(std::isfinite(native) && std::isfinite(rawBay) && native>=-1.f && native<=1.f &&
            (!pkfa || (rawBay>=-1.f && rawBay<=1.f)),"内舱：开度无效");
        const float sideAlpha=pkfa?0.f:lease.f22SideProgress;
        const float bellyAlpha=pkfa?std::max(0.f,std::min(1.f,rawBay)):lease.f22BellyProgress;
        require(std::isfinite(sideAlpha) && std::isfinite(bellyAlpha) && sideAlpha>=0.f && sideAlpha<=1.f &&
            bellyAlpha>=0.f && bellyAlpha<=1.f,"内舱：独立开度无效");
        const auto sameEndpoint=[&](float alpha) {return (alpha<=0.f && native<=0.f) || (alpha>=1.f && native>=1.f);};
        const bool fixSide=!pkfa && side && !sameEndpoint(sideAlpha);
        const bool fixBelly=!sameEndpoint(bellyAlpha);
        if((side && !fixSide && !fixBelly) || (belly && !fixBelly))return;
        if(ids.read<unsigned char>(object+0x828)!=1)return;
        const auto frame=ids.read<std::uint64_t>(base+0x3ac5520);
        if(ids.read<unsigned>(object+0xd24)!=static_cast<unsigned>(frame))return;
        const auto doubleBuffered=ids.read<unsigned char>(object+0x750);
        const int index=ids.read<int>(object+0x748),published=ids.read<int>(object+0x74c);
        require(doubleBuffered==1 && index>=0 && index<=1 && published>=0 && published<=1 && index!=published &&
            ids.read<int>(object+0x724)==0 && (ids.read<int>(object+0x720)==0 || ids.read<int>(object+0x720)==-1),
            "F22舱门：要求独立双缓冲及未共享的待发布姿态");
        const Addr array=object+0x728+static_cast<Addr>(index)*16;
        const Addr data=ids.read<Addr>(array);const int count=ids.read<int>(array+8),capacity=ids.read<int>(array+12);
        require(count==(pkfa?11:side?30:50) && capacity>=count && capacity<=1000 && data &&
            MountAnimationReflection::writable(data,static_cast<size_t>(count)*48),"F22舱门：待发布姿态不可写");
        require(ids.read<unsigned char>(object+0x828)==1 && ids.read<int>(object+0x748)==index &&
            ids.read<int>(object+0x74c)==published && ids.read<Addr>(array)==data,"F22舱门：写入前发布状态改变");
        if(pkfa)mountLocalApplyPose(data,lease.pkfaRackAnimation,bellyAlpha);
        else if(side) {
            if(fixSide)mountLocalApplyPose(data,lease.f22SideAnimation,sideAlpha);
            if(fixBelly)mountLocalApplyPose(data,lease.f22RackBellyAnimation,bellyAlpha);
        } else mountLocalApplyPose(data,lease.f22BodyAnimation,bellyAlpha);
    }catch(...) {if(pkfa)lease.pkfaRackFault=true;else lease.f22SideFault=true;} // 下次维护统一退回原生，不在引擎发布栈中归还资源。
}
