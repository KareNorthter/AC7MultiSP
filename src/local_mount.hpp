// 原生四机局部机构：只保SWP3的真实动画与Socket，不接管机身姿态或强制全开。
// 在反射、轻量Socket读取器和旧动画入口定义后包含。
#pragma once

struct LocalMountOwnership {
    MountIdentity plane,base,component,componentClass,mesh,skeleton,anim;
    size_t rackOffset{},meshOffset{},skeletonOffset{},animOffset{};
    MountAnimationReflection reflection;
    LightMountSocketReader sockets;
    MountAnimationReflection::Field refresh;
    MountIdentity refreshEnum;
    unsigned always{};
    bool owned{},nativeNoSkeleton{},nativeTick{};
    unsigned char nativeRefresh{},writtenRefresh{};
    std::map<Addr,MountAnimationReflection::SocketName> nativeSockets;
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
    const auto& group=definition.at("groups").at(2);
    auto lease=std::make_shared<LocalMountOwnership>();
    lease->plane=state.plane;lease->base=state.weaponBase;lease->rackOffset=state.rackOffsets[2];
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
    mountAnimatedResource(lease->mesh.address,group.at("rack").get<std::string>(),package+code+"_SWP3","SkeletalMesh");
    mountAnimatedResource(lease->skeleton.address,group.at("skeleton").get<std::string>(),package+code+"_SWP3_Skeleton","Skeleton");
    mountAnimatedResource(lease->anim.address,group.at("anim").get<std::string>(),package+code+"_SWP3_AnimBP","AnimBlueprintGeneratedClass");
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
    lease->sockets.prepare(api,lease->component);
    for(const auto& visual:state.visuals)if(visual.pool==2 && visual.index>=0) {
        MountAnimationReflection::SocketName name{read<unsigned>(visual.identity.address+0x108),read<unsigned>(visual.identity.address+0x10c)};
        require(!name.number && lease->sockets.exists(api,lease->component,name),"局部机构：原生弹体挂点不存在");
        lease->nativeSockets.emplace(visual.identity.address,name);
    }
    mountLocalCurrent(*lease);
    // 同实例的任务/补给预览共用一份原生状态，不把自己强制的值当作恢复基线。
    for(auto* prior:{&fixedMount,&mountPreview.layout})if((state.kindsMask&4u) && prior!=&state && prior->local &&
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
    if(!(state.kindsMask&4u)) {
        if(&state==&mountPreview.layout && mountPreviewOwnsTask())mountLocalBeforeRebuild(fixedMount);
        return;
    }
    auto& lease=*state.local;mountLocalCurrent(lease);
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
    if(visual->pool!=2) {
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
