// 轻量多挂：复用原生机构，只保明确选中的机外组件。
// 在反射适配器之后包含；任务与预览共享同组件的恢复账本。
#pragma once

const MountSpecification& mountLightSpecification(MountAircraft aircraft) {
    static const MountSpecification dark{"dark",{"plwp_sasm_a0_C","plwp_xsdb_a0_C","plwp_plsl_x0_C"},
        {2,4,2},{},{},{nullptr,nullptr,nullptr},0,{2,4,2}};
    static const MountSpecification zoef{"zoef",{"plwp_tls_x2_C","plwp_4aam_a0_C","plwp_faeb_a0_C"},
        {1,4,2},{},{},{nullptr,"w_4aam_a0_SM","w_faeb_a0_closed_SM"},0,{1,4,2}};
    static const MountSpecification raven{"adf11f",{"plwp_tls_x3_C","plwp_qaam_a0_C","plwp_WeaponUAV_C"},
        {1,4,2},{},{},{nullptr,"w_qaam_a0_SM","w_uav_x0_SM"},0,{1,2,2}};
    static const MountSpecification cfa{"fa44",{"plwp_admm_x0_C","plwp_eml_x2_C","plwp_ecmp_x1_C"},
        {126,2,1},{},{},{nullptr,nullptr,nullptr},0,{12,2,1}};
    switch(aircraft) {
        case MountAircraft::Dark:return dark;
        case MountAircraft::Zoef:return zoef;
        case MountAircraft::Adf11f:return raven;
        case MountAircraft::Fa44:return cfa;
        default:throw std::runtime_error("轻量挂载：机型未登记");
    }
}
bool mountLightNative(MountAircraft aircraft,unsigned mask) {
    require(mask>=1 && mask<=7,"轻量挂载：组合越界");
    return (mask&(mask-1))==0;
}
bool mountLightOwnsPool(MountAircraft aircraft,int pool) {
    return aircraft==MountAircraft::Dark?false:aircraft==MountAircraft::Fa44?true:pool>0;
}
bool mountLightMapped(MountAircraft aircraft,int kind) {
    if(aircraft==MountAircraft::Dark)return false;
    if(aircraft==MountAircraft::Fa44)return kind<2;
    return kind>0;
}
unsigned mountLightRackMask(MountAircraft aircraft,unsigned mask) {
    if(aircraft==MountAircraft::Dark)return mask&4;
    if(aircraft==MountAircraft::Zoef)return (mask&6)==6?2:mask&6;
    if(aircraft==MountAircraft::Fa44)return mask&3;
    return mask&6;
}

// CFA只隐藏Body直接子支路。FName、参数布局、物理操作枚举全部来自当前反射，绝不写骨数组。
struct LightMountBones {
    using Reflection=MountAnimationReflection;
    using Name=Reflection::SocketName;
    using Field=Reflection::Field;
    using Function=Reflection::Function;
    struct Root { Name name;bool wanted{},native{},owned{}; };
    Reflection reflection;
    MountIdentity component,componentClass,mesh,skeleton,physicsEnum;
    Field meshField,skeletonField;
    Function numBones,hide,unhide,isHidden;
    Field countResult,hideName,hidePhysics,unhideName,hiddenName;
    Reflection::BoolField hiddenResult;
    std::vector<Root> roots;
    Name body{};
    unsigned pboNone{};
    int qualifiedPool{-1};
    std::vector<Name> qualifiedSockets;
    bool prepared{},applied{},qualified{};

    static bool same(Name a,Name b) {return a.index==b.index && a.number==b.number;}
    static std::string label(Name n) {return mountNameIndex(n.index)+(n.number?"_"+std::to_string(n.number-1):"");}
    void componentCurrent() {
        Reflection::thread();const MountIdentityBatch identities;
        require(identities.alive(component) && identities.alive(mesh) && identities.alive(skeleton) &&
            identities.alive(componentClass),"轻量骨显隐：组件或资源身份失效");
        require(read<Addr>(component.address+0x10)==componentClass.address &&
            read<Addr>(component.address+meshField.offset)==mesh.address &&
            read<Addr>(mesh.address+skeletonField.offset)==skeleton.address,"轻量骨显隐：骨架资源被替换");
    }
    void current() {
        componentCurrent();Reflection::alive(meshField.identity);Reflection::alive(skeletonField.identity);Reflection::alive(physicsEnum);
        for(const auto& fn:{numBones,hide,unhide,isHidden}) {
            Reflection::alive(fn.identity);
            require(Reflection::refValue<unsigned short>(reflection.parms,fn.identity.address)==fn.size,
                "轻量骨显隐：缓存函数参数尺寸改变");
        }
        for(const auto& field:{meshField,skeletonField,countResult,hideName,hidePhysics,unhideName,hiddenName,hiddenResult.field}) {
            Reflection::alive(field.identity);
            require(read<int>(field.identity.address+0x44)==field.offset && read<int>(field.identity.address+0x34)==field.size,
                "轻量骨显隐：缓存字段布局改变");
        }
    }
    bool hidden(Name name) {
        alignas(16) std::array<unsigned char,256> params{};
        std::memcpy(params.data()+hiddenName.offset,&name,8);
        reflection.invoke(component,isHidden,params.data());return Reflection::getBool(params.data(),hiddenResult);
    }
    int count() {
        alignas(16) std::array<unsigned char,256> params{};
        reflection.invoke(component,numBones,params.data());int value{};
        std::memcpy(&value,params.data()+countResult.offset,4);return value;
    }
    static bool sameSockets(const std::vector<Name>& a,const std::vector<Name>& b) {
        if(a.size()!=b.size())return false;
        for(size_t i=0;i<a.size();++i)if(!same(a[i],b[i]))return false;
        return true;
    }
    void set(Root& root,bool wanted) {
        componentCurrent();if(hidden(root.name)==wanted)return;
        root.owned=true; // 引擎可能已写入后才报错，先保留恢复责任。
        alignas(16) std::array<unsigned char,256> params{};
        const auto input=wanted?hideName:unhideName;
        std::memcpy(params.data()+input.offset,&root.name,8);
        if(wanted)params[hidePhysics.offset]=static_cast<unsigned char>(pboNone);
        reflection.invoke(component,wanted?hide:unhide,params.data());
        require(hidden(root.name)==wanted,"轻量骨显隐：接口回读失败");
    }
    void prepare(MountIdentity rack,int pool,const std::vector<Name>& visibleSockets) {
        require(!prepared,"轻量骨显隐：旧租约未归还");
        require(pool>=0 && pool<3,"轻量骨显隐：未登记挂架");
        // 只缓存不可变资源资格和实际FName；绝不将native/owned或旧组件隐藏租约带进新绑定。
        static std::array<LightMountBones,3> metadata;
        const auto& cached=metadata[pool];
        if(!qualified && cached.qualified && cached.qualifiedPool==pool && sameSockets(cached.qualifiedSockets,visibleSockets) &&
           mountAlive(cached.componentClass) && read<Addr>(rack.address+0x10)==cached.componentClass.address) {
            *this=cached;component=rack;
        }
        if(qualified && qualifiedPool==pool && mountMeshIdentityEqual(component,rack) && sameSockets(qualifiedSockets,visibleSockets)) {
            bool reusable=false;
            try {current();reusable=count()==(pool==0?8:pool==1?19:7);}catch(...) {}
            if(reusable) {
                require(!hidden(body),"轻量骨显隐：Body已隐藏，不能重绑租约");
                std::vector<bool> baseline;for(const auto& root:roots)baseline.push_back(hidden(root.name));
                componentCurrent();for(size_t i=0;i<roots.size();++i) {roots[i].native=baseline[i];roots[i].owned=false;}
                prepared=true;applied=false;return; // Init前已归还，仅复用同资源不可变元数据及实际FName。
            }
        }
        *this={};
        reflection.bindSdk();component=rack;
        require(mountClassIs(rack.address,"SkeletalMeshComponent","/Script/Engine"),"轻量骨显隐：挂架类型不符");
        const Addr cls=read<Addr>(rack.address+0x10);componentClass=mountIdentity(cls);
        meshField=reflection.field(cls,"SkeletalMesh","ObjectProperty",8,reflection.typeSize(cls),true);
        mesh=mountIdentity(read<Addr>(rack.address+meshField.offset));
        const auto expected=std::string("fa44_SWP")+std::to_string(pool+1);
        Reflection::named(mesh.address,expected.c_str(),("/Game/Vehicles/Aircraft/fa44/"+expected).c_str());
        skeletonField=reflection.field(read<Addr>(mesh.address+0x10),"Skeleton","ObjectProperty",8,
            reflection.typeSize(read<Addr>(mesh.address+0x10)),true);
        skeleton=mountIdentity(read<Addr>(mesh.address+skeletonField.offset));
        Reflection::named(skeleton.address,(expected+"_Skeleton").c_str(),("/Game/Vehicles/Aircraft/fa44/"+expected+"_Skeleton").c_str());
        numBones=reflection.function(rack.address,"GetNumBones","SkinnedMeshComponent",1);
        countResult=reflection.field(numBones.identity.address,"ReturnValue","IntProperty",4,numBones.size);
        const auto get=reflection.function(rack.address,"GetBoneName","SkinnedMeshComponent",2);
        const auto index=reflection.field(get.identity.address,"BoneIndex","IntProperty",4,get.size);
        const auto result=reflection.field(get.identity.address,"ReturnValue","NameProperty",8,get.size);
        Reflection::separate(index,result);
        const auto parent=reflection.function(rack.address,"GetParentBone","SkinnedMeshComponent",2);
        const auto parentInput=reflection.field(parent.identity.address,"BoneName","NameProperty",8,parent.size);
        const auto parentResult=reflection.field(parent.identity.address,"ReturnValue","NameProperty",8,parent.size);
        Reflection::separate(parentInput,parentResult);
        hide=reflection.function(rack.address,"HideBoneByName","SkinnedMeshComponent",2);
        hideName=reflection.field(hide.identity.address,"BoneName","NameProperty",8,hide.size);
        hidePhysics=reflection.field(hide.identity.address,"PhysBodyOption","ByteProperty",1,hide.size);
        Reflection::separate(hideName,hidePhysics);
        physicsEnum=mountIdentity(Reflection::refValue<Addr>(reflection.enumRef,hidePhysics.identity.address));
        pboNone=reflection.enumValue(physicsEnum.address,"EPhysBodyOp","PBO_None");
        unhide=reflection.function(rack.address,"UnHideBoneByName","SkinnedMeshComponent",1);
        unhideName=reflection.field(unhide.identity.address,"BoneName","NameProperty",8,unhide.size);
        isHidden=reflection.function(rack.address,"IsBoneHiddenByName","SkinnedMeshComponent",2);
        hiddenName=reflection.field(isHidden.identity.address,"BoneName","NameProperty",8,isHidden.size);
        hiddenResult=reflection.boolean(isHidden.identity.address,"ReturnValue",isHidden.size);
        Reflection::separate(hiddenName,hiddenResult.field);
        alignas(16) std::array<unsigned char,256> params{};
        const int boneCount=count();
        require(boneCount>0 && boneCount<=256,"轻量骨显隐：原生骨数越界");
        std::map<std::string,Name> names;
        for(int i=0;i<boneCount;++i) {
            params={};std::memcpy(params.data()+index.offset,&i,4);reflection.invoke(component,get,params.data());
            Name name{};std::memcpy(&name,params.data()+result.offset,8);
            require(names.emplace(label(name),name).second,"轻量骨显隐：骨名重复");
        }
        // 三副原版挂架的同名骨父链不同，逐根核对，不从编号推测层级。
        const std::vector<int> parents=pool==0?std::vector<int>{-1,0,1,1,0,4,4,0}:pool==1?
            std::vector<int>{-1,0,1,2,3,4,5,3,7,8,0,10,11,12,13,14,12,16,17}:
            std::vector<int>{-1,0,1,1,0,4,4};
        auto boneLabel=[](int i) {return i==0?std::string("Body"):"SWPCover_"+std::string(i<10?"0":"")+std::to_string(i);};
        require(boneCount==static_cast<int>(parents.size()),"轻量骨显隐：原生骨数与资源证据不同");
        for(int i=0;i<boneCount;++i) {
            const auto expected=boneLabel(i);require(names.count(expected),"轻量骨显隐：原版骨名缺失");
            params={};const auto name=names.at(expected);std::memcpy(params.data()+parentInput.offset,&name,8);
            reflection.invoke(component,parent,params.data());Name actual{};std::memcpy(&actual,params.data()+parentResult.offset,8);
            require(parents[i]<0?label(actual)=="None":same(actual,names.at(boneLabel(parents[i]))),
                "轻量骨显隐：原版骨父链改变");
        }
        // 当前游戏反射目录已确认此函数属于SkinnedMeshComponent；签名仍在绑定时完整核对。
        const auto socketBone=reflection.function(rack.address,"GetSocketBoneName","SkinnedMeshComponent",2);
        const auto socketInput=reflection.field(socketBone.identity.address,"InSocketName","NameProperty",8,socketBone.size);
        const auto socketResult=reflection.field(socketBone.identity.address,"ReturnValue","NameProperty",8,socketBone.size);
        Reflection::separate(socketInput,socketResult);
        for(const auto& socket:visibleSockets) {
            params={};std::memcpy(params.data()+socketInput.offset,&socket,8);
            reflection.invoke(component,socketBone,params.data());Name actual{};std::memcpy(&actual,params.data()+socketResult.offset,8);
            require(same(actual,names.at(pool==0?"SWPCover_01":pool==1?"SWPCover_12":"Body")),
                "轻量骨显隐：发射Socket落入未知或隐藏支路");
        }
        require(names.count("Body") && !hidden(names.at("Body")),"轻量骨显隐：Body缺失或已隐藏");
        body=names.at("Body");
        const std::vector<std::pair<std::string,bool>> rules=pool==0?
            std::vector<std::pair<std::string,bool>>{{"SWPCover_01",false},{"SWPCover_04",true},{"SWPCover_07",true}}:pool==1?
            std::vector<std::pair<std::string,bool>>{{"SWPCover_01",true},{"SWPCover_10",false}}:
            std::vector<std::pair<std::string,bool>>{{"SWPCover_01",true},{"SWPCover_04",true}};
        for(const auto& rule:rules) {
            require(names.count(rule.first),"轻量骨显隐：指定支路缺失");
            const auto name=names.at(rule.first);params={};std::memcpy(params.data()+parentInput.offset,&name,8);
            reflection.invoke(component,parent,params.data());Name actual{};std::memcpy(&actual,params.data()+parentResult.offset,8);
            require(same(actual,names.at("Body")),"轻量骨显隐：指定支路不是Body直接子骨");
            roots.push_back({name,rule.second,hidden(name),false});
        }
        current();qualified=prepared=true; // 所有读取完成后，才允许第一笔显隐写入。
        qualifiedPool=pool;qualifiedSockets=visibleSockets;
        auto& saved=metadata[pool];saved=*this;saved.prepared=saved.applied=false;
        for(auto& root:saved.roots) {root.native=false;root.owned=false;}
    }
    void maintain() {
        if(applied)return;
        // 参数布局在prepare绑定/复用时完整核对；同一资源只重验动态身份。
        // set/invoke仍在每次引擎调用前后检查组件与函数弱身份。
        require(prepared,"轻量骨显隐：尚未绑定");componentCurrent();
        for(auto& root:roots)set(root,root.wanted);
        applied=true;
    }
    void release() {
        if(!prepared)return;
        if(!mountAlive(component)) {*this={};return;}
        current();
        std::string failure;
        for(auto& root:roots)if(root.owned)try {set(root,root.native);root.owned=false;}
            catch(const std::exception& e) {if(!failure.empty())failure+="；";failure+=e.what();}
        require(failure.empty(),failure.c_str());
        prepared=applied=false;for(auto& root:roots) {root.native=false;root.owned=false;}
    }
};

// 发射时只调用已解析的GetSocketTransform，参数、枚举与嵌套变换布局仅在绑定时解析。
struct LightMountSocketReader {
    using Reflection=MountAnimationReflection;
    Reflection::Function function,existsFunction;
    Reflection::Field input,space,result,existsInput;
    Reflection::BoolField existsResult;
    std::array<int,4> rotation{};
    std::array<int,3> translation{},scale{};
    unsigned world{};
    bool prepared{};
    void prepare(Reflection& api,MountIdentity rack) {
        if(prepared)return;
        function=api.function(rack.address,"GetSocketTransform","SceneComponent",3);
        input=api.field(function.identity.address,"InSocketName","NameProperty",8,function.size);
        space=api.field(function.identity.address,"TransformSpace","ByteProperty",1,function.size);
        result=api.field(function.identity.address,"ReturnValue","StructProperty",48,function.size);
        Reflection::separate(input,space);Reflection::separate(input,result);Reflection::separate(space,result);
        world=api.enumValue(Reflection::refValue<Addr>(api.enumRef,space.identity.address),"ERelativeTransformSpace","RTS_World");
        const Addr transform=Reflection::refValue<Addr>(api.structRef,result.identity.address);
        Reflection::named(transform,"Transform","/Script/CoreUObject");require(api.typeSize(transform)==48,"轻量取位：Transform尺寸改变");
        const auto r=api.field(transform,"Rotation","StructProperty",16,48),p=api.field(transform,"Translation","StructProperty",12,48),
            s=api.field(transform,"Scale3D","StructProperty",12,48);
        Reflection::separate(r,p);Reflection::separate(r,s);Reflection::separate(p,s);
        auto fields=[&](Reflection::Field parent,const char* name,auto& offsets) {
            const Addr structure=Reflection::refValue<Addr>(api.structRef,parent.identity.address);
            Reflection::named(structure,name,"/Script/CoreUObject");
            require(api.typeSize(structure)==static_cast<int>(offsets.size()*4),"轻量取位：向量尺寸改变");
            const char* names[]={"X","Y","Z","W"};
            for(size_t i=0;i<offsets.size();++i) {
                const auto part=api.field(structure,names[i],"FloatProperty",4,static_cast<int>(offsets.size()*4));
                offsets[i]=result.offset+parent.offset+part.offset;
                for(size_t j=0;j<i;++j)require(offsets[i]!=offsets[j],"轻量取位：向量参数重叠");
            }
        };
        fields(r,"Quat",rotation);fields(p,"Vector",translation);fields(s,"Vector",scale);
        existsFunction=api.function(rack.address,"DoesSocketExist","SceneComponent",2);
        existsInput=api.field(existsFunction.identity.address,"InSocketName","NameProperty",8,existsFunction.size);
        existsResult=api.boolean(existsFunction.identity.address,"ReturnValue",existsFunction.size);
        Reflection::separate(existsInput,existsResult.field);prepared=true;
    }
    bool exists(Reflection& api,MountIdentity rack,Reflection::SocketName socket) const {
        require(prepared && socket.index,"轻量取位：挂点资格尚未绑定");
        Reflection::alive(existsFunction.identity);
        require(Reflection::refValue<unsigned short>(api.parms,existsFunction.identity.address)==existsFunction.size,
            "轻量取位：挂点查询参数尺寸改变");
        for(const auto& field:{existsInput,existsResult.field}) {
            Reflection::alive(field.identity);
            require(read<int>(field.identity.address+0x44)==field.offset && read<int>(field.identity.address+0x34)==field.size,
                "轻量取位：挂点查询字段布局改变");
        }
        // FName已在同一绑定批次从原生组件读取并核对完整Socket集合；此处仍逐口调用原生查询。
        alignas(16) std::array<unsigned char,256> params{};
        std::memcpy(params.data()+existsInput.offset,&socket,8);api.invoke(rack,existsFunction,params.data());
        return Reflection::getBool(params.data(),existsResult);
    }
    void copy(Reflection& api,MountIdentity rack,Reflection::SocketName socket,void* output) const {
        require(prepared,"轻量取位：尚未绑定接口");
        Reflection::alive(function.identity);
        require(Reflection::refValue<unsigned short>(api.parms,function.identity.address)==function.size,"轻量取位：参数尺寸改变");
        alignas(16) std::array<unsigned char,256> params{};
        std::memcpy(params.data()+input.offset,&socket,8);params[space.offset]=static_cast<unsigned char>(world);
        api.invoke(rack,function,params.data());
        alignas(16) std::array<float,12> transform{};
        auto scalar=[&](int offset) {float value{};std::memcpy(&value,params.data()+offset,4);
            require(std::isfinite(value),"轻量取位：非有限数变换");return value;};
        for(int i=0;i<4;++i)transform[i]=scalar(rotation[i]);
        for(int i=0;i<3;++i) {transform[4+i]=scalar(translation[i]);transform[8+i]=scalar(scale[i]);}
        checkMemory(reinterpret_cast<Addr>(output),48);std::memcpy(output,transform.data(),48);
    }
};

struct LightMountOwnership {
    struct Point { MountIdentity component;int pool{},index{};MountAnimationReflection::SocketName socket; };
    struct Rack {
        MountIdentity component,mesh;
        size_t meshOffset{};
        bool skeletal{},animationOwned{},nativeNoSkeleton{},nativeTick{};
        LightMountBones bones;
        std::vector<MountAnimationReflection::SocketName> effectSockets;
    };
    std::array<Rack,3> racks;
    std::array<Point,64> positions;
    MountAnimationReflection reflection;
    LightMountSocketReader socketReader;
};

// 单挂均原生；多挂七集合由mask明确推导，只控制上述白名单内的组件。
void mountBindLightVisuals(FixedMountState& state,Addr plane) {
    require(!mountLightNative(state.aircraft,state.kindsMask),"轻量挂载：本组合应使用原生");
    const auto& spec=mountSpecification(state.aircraft);
    state.light=std::make_shared<LightMountOwnership>();state.physicalCount=0;
    std::array<std::map<std::string,Addr>,3> components;
    std::map<Addr,LightMountOwnership::Point> points;
    std::map<Addr,MountIdentity> componentClasses; // 仅同栈绑定批次复用类资格，不缓存组件、父链或原生显隐。
    for(int pool=0;pool<3;++pool) {
        const bool owned=state.aircraft==MountAircraft::Dark?(pool==2 && (state.kindsMask&4)):
            mountLightOwnsPool(state.aircraft,pool);
        if(!owned)continue; // TLS、SASM、XSDB完全不进入本路线显隐账本。
        auto& record=state.light->racks[pool];
        state.rackOffsets[pool]=mountProperty(plane,("SWP"+std::to_string(pool+1)).c_str(),"ObjectProperty",8);
        const Addr rack=read<Addr>(plane+state.rackOffsets[pool]);record.component=mountIdentity(rack);
        record.skeletal=state.aircraft==MountAircraft::Fa44;
        require(mountClassIs(rack,record.skeletal?"SkeletalMeshComponent":"StaticMeshComponent","/Script/Engine") &&
            read<Addr>(rack+0x100)==state.weaponBase.address,"轻量挂载：原生挂架类型或父链改变");
        record.meshOffset=mountProperty(rack,record.skeletal?"SkeletalMesh":"StaticMesh","ObjectProperty",8);
        record.mesh=mountIdentity(read<Addr>(rack+record.meshOffset));
        const std::string expected=std::string(spec.code)+"_SWP"+std::to_string(pool+1)+(record.skeletal?"":"_SM");
        mountAnimatedResource(record.mesh.address,expected,"/Game/Vehicles/Aircraft/"+std::string(spec.code)+"/"+expected,
            record.skeletal?"SkeletalMesh":"StaticMesh");
        const auto flags=read<unsigned char>(rack+0x1bc);
        state.visuals.push_back({record.component,pool,-1,bool(flags&0x10),bool(flags&0x20)});
        if(record.skeletal && pool<2) {
            // 核对CFA原骨架更新接口，保留真实通道。
            state.light->reflection.bindSdk();
            const Addr cls=read<Addr>(rack+0x10);const int extent=state.light->reflection.typeSize(cls);
            const auto no=state.light->reflection.boolean(cls,"bNoSkeletonUpdate",extent,true);
            const auto pause=state.light->reflection.boolean(cls,"bPauseAnims",extent,true);
            require(no.field.offset==0x9b0 && no.fieldMask==2 && pause.field.offset==0x9b0 && pause.fieldMask==4,
                "轻量挂载：骨架控制字段布局改变");
        }
        if(state.aircraft==MountAircraft::Dark)continue; // PLSL炮体本身就是完整静态架。
        const Addr array=state.meshManager.address+0x150+pool*16;
        const int count=read<int>(array+8),capacity=read<int>(array+12);
        require(count>=0 && count<=256 && capacity>=count,"轻量挂载：原生持久池越界");
        state.poolCounts[pool]=count;
        // 资源Socket数只用于严格核对可见组件集合；运行逻辑队列仍从Group取得。
        const int expectedCount=state.aircraft==MountAircraft::Fa44?(pool==0?126:pool==1?2:1):(pool==1?4:2);
        require(count==expectedCount || (state.aircraft==MountAircraft::Fa44 && pool==2 && count==0),
            "轻量挂载：原生持久池与已核资源Socket集合不符");
        const Addr data=read<Addr>(array);
        if(count)checkMemory(data,static_cast<size_t>(count)*sizeof(Addr));
        for(int index=0;index<count;++index) {
            const Addr component=read<Addr>(data+static_cast<Addr>(index)*8);
            const auto identity=mountIdentity(component);
            const Addr cls=read<Addr>(component+0x10);
            auto known=componentClasses.find(cls);
            if(known==componentClasses.end()) {
                require(mountClassIs(component,"StaticMeshComponent","/Script/Engine"),"轻量挂载：原生弹体组件类型改变");
                known=componentClasses.emplace(cls,mountIdentity(cls)).first;
            }
            require(mountAlive(known->second) && read<Addr>(component+0x100)==rack,
                "轻量挂载：原生弹体组件类或父链改变");
            const MountAnimationReflection::SocketName socketName{read<unsigned>(component+0x108),read<unsigned>(component+0x10c)};
            if(state.aircraft==MountAircraft::Fa44 && pool==2)record.effectSockets.push_back(socketName);
            std::string socket=LightMountBones::label(socketName);
            for(auto& c:socket)if(c>='A' && c<='Z')c+=32;
            require(components[pool].emplace(socket,component).second,
                "轻量挂载：Socket编号异常或重复");
            points.emplace(component,LightMountOwnership::Point{identity,pool,index,socketName});
            const Addr mesh=mountBaselineMesh(identity,read<Addr>(component+0x728));
            if(spec.meshes[pool])require(mesh && mountName(mesh)==spec.meshes[pool],"轻量挂载：原生弹体资源改变");
            else require(!mesh,"轻量挂载：原生固定效果源出现未知弹体网格");
            if(state.aircraft!=MountAircraft::Fa44) {
                const auto componentFlags=read<unsigned char>(component+0x1bc);
                state.visuals.push_back({identity,pool,index,bool(componentFlags&0x10),bool(componentFlags&0x20)});
            } // CFA的空发射组件不参与显隐，保留原生效果源行为。
        }
        require(read<Addr>(array)==data && read<int>(array+8)==count && read<int>(array+12)==capacity,
            "轻量挂载：绑定期间原生持久池改变");
        // 不依赖数组枚举顺序，逐个验证全部原版Socket均存在。
        for(int i=0;i<expectedCount && count;++i) {
            std::string socket;
            if(state.aircraft==MountAircraft::Fa44 && pool==0) {
                socket="sp"+std::string(i+1<10?"00":i+1<100?"0":"")+std::to_string(i+1);
            } else socket="sp"+std::to_string((pool+1)*10+i+1);
            require(components[pool].count(socket),"轻量挂载：原生Socket集合不完整");
        }
    }
    auto select=[&](int kind,const std::string& socket) {
        require(state.physicalCount<64 && components[kind].count(socket),"轻量挂载：物理映射缺失或越界");
        const int position=state.physicalCount++;
        state.physical[position]=components[kind].at(socket);state.expansionPositions[kind].push_back(position);
        state.light->positions[position]=points.at(state.physical[position]);
        const auto& point=state.light->positions[position];
        state.light->socketReader.prepare(state.light->reflection,state.light->racks[kind].component);
        require(state.light->socketReader.exists(state.light->reflection,state.light->racks[kind].component,point.socket),
            "轻量挂载：原生发射Socket不存在");
    };
    if(state.aircraft==MountAircraft::Zoef) {
        if(state.kindsMask&2)for(int i=21;i<=((state.kindsMask&4)?22:24);++i)select(1,"sp"+std::to_string(i));
        if(state.kindsMask&4)for(int i=31;i<=32;++i)select(2,"sp"+std::to_string(i));
    } else if(state.aircraft==MountAircraft::Adf11f) {
        if(state.kindsMask&2) {
            if(state.kindsMask==3)for(int i=21;i<=24;++i)select(1,"sp"+std::to_string(i));
            else {select(1,"sp22");select(1,"sp24");}
        }
        if(state.kindsMask&4) {select(2,"sp31");if(state.kindsMask==5)select(2,"sp32");} // 原生双组件池及回收不变。
    } else if(state.aircraft==MountAircraft::Fa44) {
        const bool split=(state.kindsMask&3u)==3u;
        // 非分侧组合的ADMM保完整126口并走原生发射；这里42位只供既有预览就绪账本使用。
        if(state.kindsMask&1)for(int i=1;i<=124;i+=3)select(0,"sp"+std::string(i<10?"00":i<100?"0":"")+std::to_string(i));
        if(state.kindsMask&2) {if(!split)select(1,"sp21");select(1,"sp22");}
    }
}

bool mountLightDesired(const FixedMountState& state,const MountVisual& visual,bool groupVisible) {
    if(!groupVisible)return false;
    if(visual.index<0)return (state.rackMask&(1u<<visual.pool))!=0;
    if(state.aircraft==MountAircraft::Fa44)return (state.kindsMask&(1u<<visual.pool)) && visual.pool<2 &&
        (visual.pool==1?visual.physicalBit!=0:(state.readyMask&visual.physicalBit)!=0);
    return (state.readyMask&visual.physicalBit)!=0;
}
bool mountLightLayoutChanged(const FixedMountState& state) {
    CfaWorkScope cost(0);
    if(!state.light)return true;
    const MountIdentityBatch identities; // 这段只读循环不调用引擎，结束后不保留表头。
    for(int pool=0;pool<3;++pool) {
        const auto& rack=state.light->racks[pool];if(!rack.component.address)continue;
        if(!identities.alive(rack.component) || !identities.alive(rack.mesh) ||
            read<Addr>(state.plane.address+state.rackOffsets[pool])!=rack.component.address ||
            read<Addr>(rack.component.address+rack.meshOffset)!=rack.mesh.address)return true;
        if(state.aircraft==MountAircraft::Dark)continue;
        const Addr array=state.meshManager.address+0x150+pool*16;
        if(read<int>(array+8)!=state.poolCounts[pool])return true;
        const Addr data=read<Addr>(array);
        for(const auto& visual:state.visuals)if(visual.pool==pool && visual.index>=0 &&
            (!identities.alive(visual.identity) || read<Addr>(data+static_cast<Addr>(visual.index)*8)!=visual.identity.address))return true;
        if(state.aircraft==MountAircraft::Fa44)for(int position=0;position<state.physicalCount;++position) {
            const auto& point=state.light->positions[position];if(point.pool!=pool)continue;
            if(!identities.alive(point.component) || read<Addr>(data+static_cast<Addr>(point.index)*8)!=point.component.address ||
                read<Addr>(point.component.address+0x100)!=rack.component.address)return true;
        }
    }
    return false;
}
void mountLightRelease(FixedMountState& state,bool keepBones) {
    if(!state.light)return;
    std::string failure;
    for(auto& rack:state.light->racks) {
        try {
            CfaWorkScope cost(1);
            if(keepBones) {
                // 同机同布局切换保留租约，避免先显示再隐藏；Init后只复核并补齐被原生重置的支路。
                if(rack.bones.prepared) {rack.bones.componentCurrent();rack.bones.applied=false;}
            } else rack.bones.release();
        }catch(const std::exception& e) {if(!failure.empty())failure+="；";failure+=e.what();}
        if(rack.animationOwned)try {
            CfaWorkScope cost(2);
            if(mountAlive(rack.component) &&
               (((read<unsigned char>(rack.component.address+0x9b0)&2)!=0)!=rack.nativeNoSkeleton ||
                (read<unsigned char>(rack.component.address+0x3f)!=0)!=rack.nativeTick))
                mountSetRackAnimation(rack.component.address,rack.nativeNoSkeleton,rack.nativeTick);
            rack.animationOwned=false;
        }catch(const std::exception& e) {if(!failure.empty())failure+="；";failure+=e.what();}
    }
    require(failure.empty(),failure.c_str());
}
void mountLightBeforeRebuild(const Command& command) {
    auto& state=fixedMount;
    const bool keep=command.op=="switch" && state.bound && state.aircraft==MountAircraft::Fa44 &&
        state.plane.address==command.plane && state.animationActivator.address==command.activator &&
        mountAlive(state.plane) && mountAlive(state.animationActivator) && mountAlive(state.manager) &&
        mountAlive(state.meshManager) && read<Addr>(command.plane+0x11a8)==command.activator &&
        read<Addr>(command.activator+0x260)==state.manager.address &&
        read<Addr>(state.manager.address+0x150)==state.meshManager.address && !mountLightLayoutChanged(state);
    mountLightRelease(state,keep);
}
void mountLightInherit(FixedMountState& next,const FixedMountState& prior) {
    if(!mountLight(next.aircraft) || next.aircraft!=prior.aircraft || next.kindsMask!=prior.kindsMask || !next.light || !prior.light)return;
    bool same=mountMeshIdentityEqual(next.plane,prior.plane) && next.physicalCount==prior.physicalCount;
    for(int pool=0;pool<3;++pool)same=same &&
        mountMeshIdentityEqual(next.light->racks[pool].component,prior.light->racks[pool].component) &&
        mountMeshIdentityEqual(next.light->racks[pool].mesh,prior.light->racks[pool].mesh);
    for(int i=0;same && i<next.physicalCount;++i) {
        const auto& a=next.light->positions[i];const auto& b=prior.light->positions[i];
        same=mountMeshIdentityEqual(a.component,b.component) && a.pool==b.pool && a.index==b.index &&
            a.socket.index==b.socket.index && a.socket.number==b.socket.number && next.physical[i]==prior.physical[i];
    } // 同架重建池时，新point表必须保留，不能继承旧组件身份。
    if(same)next.light=prior.light;
}
void mountLightMaintain(FixedMountState& state) {
    if(!mountLight(state.aircraft))return;
    require(mountAlive(state.plane) && mountAlive(state.manager) && mountAlive(state.meshManager) &&
        mountAlive(state.animationActivator) && read<Addr>(state.animationActivator.address+0x260)==state.manager.address &&
        read<Addr>(state.manager.address+0x150)==state.meshManager.address && !mountLightLayoutChanged(state),
        "轻量挂载：模型池、资源或管理器身份改变");
    if(state.aircraft!=MountAircraft::Fa44)return;
    // SWP3两组机构只控制外形；sp31仍在Body上，不能隐藏根骨或压低公共开盖进度。
    auto& ecm=state.light->racks[2];
    {
        CfaWorkScope cost(3);
        if(!ecm.bones.prepared) {
            // ECM允许没有持久效果源；有源时使用绑定阶段核对过的真实sp31 FName。
            ecm.bones.prepare(ecm.component,2,ecm.effectSockets);
        }
        ecm.bones.maintain();
    }
    const bool split=(state.kindsMask&3u)==3u;
    for(int pool=0;pool<2;++pool) {
        auto& rack=state.light->racks[pool];if(!(state.kindsMask&(1u<<pool)))continue;
        {
            CfaWorkScope cost(3);
            if(split && !rack.bones.prepared) {
                std::vector<MountAnimationReflection::SocketName> sockets;
                for(int p=0;p<state.physicalCount;++p)if(state.light->positions[p].pool==pool)sockets.push_back(state.light->positions[p].socket);
                rack.bones.prepare(rack.component,pool,sockets);
            }
            if(split)rack.bones.maintain();else rack.bones.release(); // 和ECM双挂保整架左右。
        }
        CfaWorkScope cost(4);
        const Addr object=rack.component.address;const auto flags=read<unsigned char>(object+0x9b0);
        const bool noSkeleton=(flags&2)!=0,tick=read<unsigned char>(object+0x3f)!=0;
        require(!(flags&4),"轻量挂载：原生主动暂停挂架动画");
        if(!rack.animationOwned) {
            rack.nativeNoSkeleton=noSkeleton;rack.nativeTick=tick;rack.animationOwned=true;
        }
        if(noSkeleton || !tick)mountSetRackAnimation(object,false,true);
    }
}
bool mountLightCopyTransform(void* output,const FixedMountState& state,int position) {
    if(state.aircraft==MountAircraft::Fa44 && (state.kindsMask&3u)!=3u)return false; // 整架双挂保原生全部发射口。
    require(state.light && position>=0 && position<state.physicalCount,"轻量挂载：发射位置越界");
    const auto& point=state.light->positions[position];const Addr component=point.component.address;
    if(state.aircraft==MountAircraft::Fa44 && point.pool==1)mountCfaMuzzleSync();
    require(component==state.physical[position] && mountAlive(point.component),"轻量挂载：发射组件身份改变");
    // 从原挂架的具名Socket取实时世界位置，避免UAV回收运动或隐藏骨缩放污染发射变换。
    const auto& rack=state.light->racks[point.pool];
    require(mountAlive(rack.component) && read<Addr>(component+0x100)==rack.component.address &&
        read<unsigned>(component+0x108)==point.socket.index && read<unsigned>(component+0x10c)==point.socket.number,
        "轻量挂载：发射Socket或父链改变");
    state.light->socketReader.copy(state.light->reflection,rack.component,point.socket,output);return true;
}
