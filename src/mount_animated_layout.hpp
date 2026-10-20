// 三机动画布局只绑定已有组件；逻辑owner、Socket provider与呈现挂架分别校验。
#pragma once

const Json& mountAnimatedDefinition(MountAircraft aircraft) {
    require(mountAnimated(aircraft),"动画挂载：机型未登记");
    return animatedMountData().at("planes").at(static_cast<int>(aircraft)-static_cast<int>(MountAircraft::F22a));
}
const Json& mountAnimatedPlan(MountAircraft aircraft,unsigned mask) {
    require(mask>=1 && mask<=7,"动画挂载：组合越界");
    return mountAnimatedDefinition(aircraft).at("plans").at(mask);
}
const MountSpecification& mountAnimatedSpecification(MountAircraft aircraft) {
    static const auto specifications=[] {
        std::vector<MountSpecification> result;
        for(const auto& plane:animatedMountData().at("planes")) {
            MountSpecification spec{};spec.code=plane.at("code").get_ref<const std::string&>().c_str();
            for(int i=0;i<3;++i) {
                const auto& group=plane.at("groups").at(i);
                spec.weaponClasses[i]=group.at("class").get_ref<const std::string&>().c_str();
                spec.meshes[i]=group.at("mesh").get_ref<const std::string&>().c_str();
                spec.loadedCounts[i]=group.at("window").get<int>();
                // 此处不把资源Socket数填成原生逻辑队列或持久池长度；绑定和capture分别读取。
            }
            result.push_back(spec);
        }
        return result;
    }();
    return specifications.at(static_cast<int>(aircraft)-static_cast<int>(MountAircraft::F22a));
}
void mountAnimatedResource(Addr object,const std::string& expected,const std::string& package,const char* type) {
    mountIdentity(object);
    const auto actualType=mountName(read<Addr>(object+0x10)),actualName=mountName(object),
        actualPackage=mountName(read<Addr>(object+0x20));
    if(actualType==type && actualName==expected && actualPackage==package)return;
    // 只在失败时记录差异，界面提示保持简短；类型、名称和包的严格门槛不放宽。
    nativeLog(Json{{"event","MOUNT_RESOURCE_MISMATCH"},{"expected_type",type},{"expected_name",expected},
        {"expected_package",package},{"actual_type",actualType},{"actual_name",actualName},
        {"actual_package",actualPackage}}.dump());
    throw std::runtime_error("动画挂载：原生资源类型、名称或包不符");
}
// 每个方案均核对owner/provider/Socket集合，不能只因为头文件里有七行就填资格通过。
void mountAnimatedQualifyPlans(const Json& definition) {
    const auto& groups=definition.at("groups");const auto& plans=definition.at("plans");
    require(groups.size()==3 && plans.size()==8,"动画挂载：三组或七组合数据不完整");
    for(unsigned mask=1;mask<=7;++mask) {
        const auto& plan=plans.at(mask);
        require(plan.at("selected").size()==3 && plan.at("providers").size()==3 &&
                plan.at("native").get<bool>()==((mask&(mask-1))==0),"动画挂载：单挂旁路或组合结构改变");
        std::set<std::pair<int,int>> occupied;
        for(int owner=0;owner<3;++owner) {
            const auto& selected=plan.at("selected").at(owner);
            const int provider=plan.at("providers").at(owner).get<int>();
            if(!(mask&(1u<<owner))) { require(selected.empty(),"动画挂载：未选武器仍占弹位");continue; }
            require(provider>=0 && provider<3 && !selected.empty(),"动画挂载：有效兵装缺少provider");
            for(const auto& item:selected) {
                const int socket=item.get<int>();
                require(socket>=0 && socket<static_cast<int>(groups.at(provider).at("sockets").size()) &&
                        occupied.emplace(provider,socket).second,"动画挂载：未知或重复物理Socket");
            }
        }
        const unsigned racks=plan.at("racks").get<unsigned>(),required=plan.at("required").get<unsigned>();
        require(racks && !(racks&~7u) && !(required&~7u),"动画挂载：挂架集合无效");
    }
}
void mountBindAnimatedVisuals(FixedMountState& state,Addr plane) {
    const auto& definition=mountAnimatedDefinition(state.aircraft);
    mountAnimatedQualifyPlans(definition);
    const auto& groups=definition.at("groups");
    const auto& plan=mountAnimatedPlan(state.aircraft,state.kindsMask);
    require(!plan.at("native").get<bool>(),"动画挂载：单挂应保留原生流程");
    std::array<std::vector<Addr>,3> components;
    std::array<Addr,3> ownerMeshes{};
    auto& description=state.animationDescription;
    description.pawnPackage="/Game/Blueprint/Player/Pawn/AcePlayerPawn_"+definition.at("code").get<std::string>();
    for(int pool=0;pool<3;++pool) {
        const auto& group=groups.at(pool);
        const bool skeletal=group.at("componentkind")=="skeletal";
        const auto field="SWP"+std::to_string(pool+1);
        state.rackOffsets[pool]=mountProperty(plane,field.c_str(),"ObjectProperty",8);
        const Addr rack=read<Addr>(plane+state.rackOffsets[pool]);
        state.animationRacks[pool]=mountIdentity(rack);
        require(mountClassIs(rack,skeletal?"SkeletalMeshComponent":"StaticMeshComponent","/Script/Engine") &&
                read<Addr>(rack+0x100)==state.weaponBase.address,"动画挂载：挂架类或共同父链不符");
        const auto meshField=mountProperty(rack,skeletal?"SkeletalMesh":"StaticMesh","ObjectProperty",8);
        const Addr mesh=read<Addr>(rack+meshField);
        const auto& packages=group.at("packages");
        mountAnimatedResource(mesh,group.at("rack").get<std::string>(),packages.at("rack").get<std::string>(),skeletal?"SkeletalMesh":"StaticMesh");
        description.meshes[pool]=group.at("rack").get<std::string>();
        if(skeletal) {
            const Addr skeleton=read<Addr>(mesh+mountProperty(mesh,"Skeleton","ObjectProperty",8));
            const Addr anim=read<Addr>(rack+mountProperty(rack,"AnimClass","ClassProperty",8));
            mountAnimatedResource(skeleton,group.at("skeleton").get<std::string>(),packages.at("skeleton").get<std::string>(),"Skeleton");
            mountAnimatedResource(anim,group.at("animclass").get<std::string>(),packages.at("animclass").get<std::string>(),"AnimBlueprintGeneratedClass");
            description.skeletons[pool]=group.at("skeleton").get<std::string>();
            description.animationClasses[pool]=group.at("animclass").get<std::string>();description.skeletalMask|=1u<<pool;
        } else description.staticMask|=1u<<pool;
        const auto flags=read<unsigned char>(rack+0x1bc);
        state.visuals.push_back({state.animationRacks[pool],pool,-1,bool(flags&0x10),bool(flags&0x20)});
        const Addr array=state.meshManager.address+0x150+pool*16;
        const Addr data=read<Addr>(array);const int count=read<int>(array+8),capacity=read<int>(array+12);
        const auto& sockets=group.at("sockets");
        require(count>0 && count<=capacity && count==static_cast<int>(sockets.size()),"动画挂载：实际持久池与完整Socket集合不符");
        components[pool].resize(sockets.size());state.poolCounts[pool]=count;
        std::map<std::string,int> ordinals;
        for(size_t i=0;i<sockets.size();++i)
            require(ordinals.emplace(sockets.at(i).get<std::string>(),static_cast<int>(i)).second,"动画挂载：资源Socket重复");
        for(int index=0;index<count;++index) {
            const Addr component=read<Addr>(data+static_cast<Addr>(index)*8);
            const auto id=mountIdentity(component);
            require(mountClassIs(component,"StaticMeshComponent","/Script/Engine") && read<Addr>(component+0x100)==rack,
                    "动画挂载：原生弹体类或父链改变");
            auto socket=mountNameIndex(read<unsigned>(component+0x108));
            for(char& c:socket)if(c>='A' && c<='Z')c+=32;
            const auto found=ordinals.find(socket);
            require(found!=ordinals.end() && read<unsigned>(component+0x10c)==0 && !components[pool][found->second],
                    "动画挂载：原生Socket不在白名单或重复");
            const Addr original=mountBaselineMesh(id,read<Addr>(component+0x728));
            require(original && mountName(original)==group.at("mesh").get<std::string>() &&
                    mountName(read<Addr>(original+0x10))=="StaticMesh","动画挂载：原生弹体资源不符");
            if(!ownerMeshes[pool])ownerMeshes[pool]=original;
            else require(ownerMeshes[pool]==original,"动画挂载：同一原生池混用弹体资源");
            const auto componentFlags=read<unsigned char>(component+0x1bc);
            state.visuals.push_back({id,pool,index,bool(componentFlags&0x10),bool(componentFlags&0x20)});
            components[pool][found->second]=component;
        }
    }
    // Body也必须仍使用已核对的原版骨架/动画资源，不能只检查挂架后假定公共通道成立。
    const auto& body=definition.at("body");const auto& bodyPackages=body.at("packages");
    const Addr bodyComponent=read<Addr>(plane+mountProperty(plane,"PlaneBodyMesh","ObjectProperty",8));
    mountIdentity(bodyComponent);
    require(mountClassIs(bodyComponent,"SkeletalMeshComponent","/Script/Engine") &&
            read<Addr>(bodyComponent+0x100)==state.root.address,"动画挂载：机体骨架父链改变");
    const Addr bodyMesh=read<Addr>(bodyComponent+mountProperty(bodyComponent,"SkeletalMesh","ObjectProperty",8));
    const Addr bodySkeleton=read<Addr>(bodyMesh+mountProperty(bodyMesh,"Skeleton","ObjectProperty",8));
    const Addr bodyAnim=read<Addr>(bodyComponent+mountProperty(bodyComponent,"AnimClass","ClassProperty",8));
    mountAnimatedResource(bodyMesh,body.at("mesh").get<std::string>(),bodyPackages.at("mesh").get<std::string>(),"SkeletalMesh");
    mountAnimatedResource(bodySkeleton,body.at("skeleton").get<std::string>(),bodyPackages.at("skeleton").get<std::string>(),"Skeleton");
    mountAnimatedResource(bodyAnim,body.at("animclass").get<std::string>(),bodyPackages.at("animclass").get<std::string>(),"AnimBlueprintGeneratedClass");
    description.bodyAndRacksUseOnlyPublicSpwc=true; // 上述真实资源均匹配本批已逐轨核对的三机原版绑定。
    description.originalParentChains=true;description.allSevenLayoutQualified=true;
    description.planEquippedMask=state.kindsMask;description.presentationMask=plan.at("racks").get<unsigned>();
    state.physicalCount=0;
    for(int owner=0;owner<3;++owner) {
        const int provider=plan.at("providers").at(owner).get<int>();
        for(const auto& item:plan.at("selected").at(owner)) {
            const int socket=item.get<int>();
            require(provider>=0 && provider<3 && state.physicalCount<64,"动画挂载：provider或物理表越界");
            const Addr component=components[provider].at(socket);
            require(component!=0,"动画挂载：provider实际组件缺失");
            const int position=state.physicalCount++;
            state.expansionPositions[owner].push_back(position);state.expansionSockets[owner].push_back(socket);
            state.physical[position]=component;state.animationProvider[position]=provider;
            state.animationSocket[position]={read<unsigned>(component+0x108),read<unsigned>(component+0x10c)};
            description.providerMask|=1u<<provider;
            const auto id=mountIdentity(component);const Addr original=mountBaselineMesh(id,read<Addr>(component+0x728));
            if(original!=ownerMeshes[owner])state.meshOverrides.push_back({id,mountIdentity(original),mountIdentity(ownerMeshes[owner])});
        }
    }
    require(((description.providerMask|description.presentationMask)&description.skeletalMask)==plan.at("required").get<unsigned>(),
            "动画挂载：所需更新集合与provider/呈现架不符");
}
