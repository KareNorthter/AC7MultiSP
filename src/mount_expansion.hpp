// 常规挂载接入；保留原生组件、父链和兵装队列，借位只替换弹体网格。
#pragma once

size_t mountProperty(Addr object,const char* name,const char* kind,int size);
void mountRequireNoCollision(Addr component);
struct MountAssetReference { MountIdentity identity;bool rooted{}; };
std::map<int,MountAssetReference> mountAssets; // 只保有生成白名单内的纯网格，不引用世界或玩家。

const Json& mountExpansionPlan(MountAircraft aircraft,unsigned mask) {
    require(mask>=1 && mask<=7,"扩展挂载：组合无效");
    return mountExpansionDefinition(aircraft).at("plans").at(mask);
}
bool mountFixedEmitter(MountAircraft aircraft,int kind) {
    if(mountLight(aircraft))return !mountLightMapped(aircraft,kind) || (aircraft==MountAircraft::Fa44 && kind==1);
    if(mountExterior(aircraft))return aircraft==MountAircraft::Pkfa && kind==2;
    return mountExpanded(aircraft) && mountExpansionDefinition(aircraft).at("groups").at(kind).at("fixed").get<bool>();
}
bool mountNeedsMappedTransform(const FixedMountState& state,int kind) {
    // CFA仅ADMM/EML共存才分侧；与ECM双挂保完整原生左右口，126口不挤入64位映射。
    if(state.aircraft==MountAircraft::Fa44 && (state.kindsMask&3u)!=3u)return false;
    if(mountLight(state.aircraft))return mountLightMapped(state.aircraft,kind);
    if(mountExterior(state.aircraft))return false; // 机外外观保留不换原生发射点。
    return !mountFixedEmitter(state.aircraft,kind);
}
// 根据登记Socket列表建立周期表；逻辑槽保留原序号，登记序号不等于运行池下标。
void mountExpansionPositions(FixedMountState& state) {
    const auto& plan=mountExpansionPlan(state.aircraft,state.kindsMask);
    const auto& selected=plan.at("selected");
    state.physicalCount=0;
    for(int kind=0;kind<3;++kind) {
        state.expansionPositions[kind].clear();state.expansionSockets[kind].clear();state.expansionPools[kind].clear();
        for(const auto& socket:selected.at(kind)) {
            require(state.physicalCount<64,"扩展挂载：物理位置超过显隐容量");
            state.expansionPositions[kind].push_back(state.physicalCount++);
            state.expansionSockets[kind].push_back(socket.get<int>());
            state.expansionPools[kind].push_back(kind);
        }
    }
    if(plan.contains("borrowed_positions"))for(const auto& borrowed:plan.at("borrowed_positions")) {
        const int kind=borrowed.at("kind").get<int>(),pool=borrowed.at("pool").get<int>(),socket=borrowed.at("socket").get<int>();
        require(kind>=0 && kind<3 && pool>=0 && pool<3 && kind!=pool && (state.kindsMask&(1u<<kind)),
            "扩展借位：逻辑兵装或供位池无效");
        const bool fullBorrow=(plan.value("full_borrowed_kinds",0u)&(1u<<kind))!=0;
        // 旧借位保持原门槛；整组迁位须显式登记，供位池可选中，但每个Socket只能分配一次。
        require(fullBorrow?selected.at(kind).empty():
            (!(state.kindsMask&(1u<<pool)) && !state.expansionPositions[kind].empty()),
            "扩展借位：未登记整组迁位或原生来源位置缺失");
        const auto& provider=mountExpansionDefinition(state.aircraft).at("groups").at(pool);
        require(!provider.at("fixed").get<bool>() && !mountExpansionDefinition(state.aircraft).at("groups").at(kind).at("fixed").get<bool>() &&
            socket>=0 && socket<static_cast<int>(provider.at("sockets").size()),
            "扩展借位：供位池或原生Socket无效");
        for(int prior=0;prior<3;++prior)for(size_t i=0;i<state.expansionSockets[prior].size();++i)
            require(state.expansionPools[prior][i]!=pool || state.expansionSockets[prior][i]!=socket,"扩展借位：同一原生位置被重复分配");
        require(state.physicalCount<64,"扩展借位：物理位置超过显隐容量");
        state.expansionPositions[kind].push_back(state.physicalCount++);
        state.expansionSockets[kind].push_back(socket);state.expansionPools[kind].push_back(pool);
    }
}
// 新飞机字段逐项反射验证，不套用三架参考飞机的派生类偏移。
void mountBindExpansionVisuals(FixedMountState& state,Addr plane) {
    const auto& definition=mountExpansionDefinition(state.aircraft);
    const auto& groups=definition.at("groups");
    const auto code=definition.at("code").get<std::string>();
    const auto& plan=mountExpansionPlan(state.aircraft,state.kindsMask);
    mountExpansionPositions(state);
    for(int pool=0;pool<3;++pool) {
        const auto& group=groups.at(pool);
        const std::string context=" [aircraft="+code+", pool=SWP"+std::to_string(pool+1)+"]";
        state.rackOffsets[pool]=mountProperty(plane,("SWP"+std::to_string(pool+1)).c_str(),"ObjectProperty",8);
        const Addr rack=read<Addr>(plane+state.rackOffsets[pool]);
        const auto rackIdentity=mountIdentity(rack);
        require(mountName(read<Addr>(rack+0x10))=="StaticMeshComponent" &&
            read<Addr>(rack+0x100)==state.weaponBase.address,("扩展挂载：挂架类型或父链改变"+context).c_str());
        const Addr original=mountBaselineMesh(rackIdentity,read<Addr>(rack+0x728));
        require(mountName(mountIdentity(original).address)==group.at("rack").get<std::string>(),("扩展挂载：原生挂架资源不匹配"+context).c_str());
        const auto flags=read<unsigned char>(rack+0x1bc);
        state.visuals.push_back({rackIdentity,pool,-1,bool(flags&0x10),bool(flags&0x20)});
        const int variant=plan.at("variants").at(pool).get<int>();
        if(variant>=0) {
            if(definition.value("requires_no_collision",false))mountRequireNoCollision(rack);
            const auto asset=mountAssets.find(variant);
            require(asset!=mountAssets.end() && mountAlive(asset->second.identity),("扩展挂载：独立挂架未加载或已失效"+context+", variant="+std::to_string(variant)).c_str());
            state.meshOverrides.push_back({rackIdentity,mountIdentity(original),asset->second.identity});
        }
        const Addr array=state.meshManager.address+0x150+pool*16;
        const int count=read<int>(array+8),expected=static_cast<int>(group.at("sockets").size());
        // 固定效果源可能没有可视弹体池；其发射变换仍由原生处理。
        require((count==expected || (count==0 && group.at("fixed").get<bool>())) &&
            read<int>(array+12)>=count,("扩展挂载：持久池与原版Socket数量不符"+context+", expected="+
                std::to_string(expected)+", actual="+std::to_string(count)).c_str());
        state.poolCounts[pool]=count;
        // 资源枚举顺序可能不同于原生池；只接受完整白名单内无重复的一一对应。
        std::map<std::string,int> socketOrdinals;
        std::vector<bool> socketSeen(expected,false);
        for(int ordinal=0;ordinal<expected;++ordinal) {
            std::string socket=group.at("sockets").at(ordinal).get<std::string>();
            for(char& c:socket)if(c>='A' && c<='Z')c+=32;
            require(!socket.empty() && socketOrdinals.emplace(socket,ordinal).second,
                ("扩展挂载：登记Socket为空或重复"+context+", socket="+socket).c_str());
        }
        for(int index=0;index<count;++index) {
            const std::string componentContext=context+", native_index="+std::to_string(index);
            const Addr component=read<Addr>(read<Addr>(array)+index*8);
            const auto identity=mountIdentity(component);
            require(mountName(read<Addr>(component+0x10))=="StaticMeshComponent" && read<Addr>(component+0x100)==rack,
                ("扩展挂载：武器组件类型或父链改变"+componentContext).c_str());
            std::string socket=mountNameIndex(read<unsigned>(component+0x108));
            for(char& c:socket)if(c>='A' && c<='Z')c+=32;
            const auto found=socketOrdinals.find(socket);
            require(found!=socketOrdinals.end(),("扩展挂载：原生Socket不在白名单"+componentContext+", actual="+socket).c_str());
            const int ordinal=found->second;
            require(!socketSeen[ordinal],("扩展挂载：原生Socket重复"+componentContext+", actual="+socket).c_str());
            socketSeen[ordinal]=true;
            const Addr mesh=mountBaselineMesh(identity,read<Addr>(component+0x728));
            if(!group.at("mesh").is_null())require(mesh && mountName(mesh)==group.at("mesh").get<std::string>(),("扩展挂载：原生弹体资源改变"+componentContext+", socket="+socket).c_str());
            else require(!mesh || mountName(read<Addr>(mountIdentity(mesh).address+0x10))=="StaticMesh",("扩展挂载：固定效果源网格类型改变"+componentContext+", socket="+socket).c_str());
            const auto componentFlags=read<unsigned char>(component+0x1bc);
            // 生命周期校验仍使用原生池下标，不能改成登记Socket序号。
            state.visuals.push_back({identity,pool,index,bool(componentFlags&0x10),bool(componentFlags&0x20)});
            for(int kind=0;kind<3;++kind)for(size_t selected=0;selected<state.expansionSockets[kind].size();++selected)
                if(state.expansionPools[kind][selected]==pool && state.expansionSockets[kind][selected]==ordinal)
                    state.physical[state.expansionPositions[kind][selected]]=component;
        }
        if(count>0)for(int ordinal=0;ordinal<expected;++ordinal)
            require(socketSeen[ordinal],("扩展挂载：原生Socket缺失"+context+", expected="+
                group.at("sockets").at(ordinal).get<std::string>()).c_str());
    }
    // 全部原生池核对完成后才处理跨池位置，不能在供位池尚未遍历时误判缺件。
    for(int kind=0;kind<3;++kind)if((state.kindsMask&(1u<<kind)) && !groups.at(kind).at("fixed").get<bool>())
        for(int physical:state.expansionPositions[kind])require(state.physical[physical]!=0,"扩展挂载：预期发射组件缺失");
    for(int kind=0;kind<3;++kind)for(size_t index=0;index<state.expansionPositions[kind].size();++index) {
        if(state.expansionPools[kind][index]==kind)continue;
        const Addr target=state.physical[state.expansionPositions[kind][index]];
        const auto identity=mountIdentity(target);
        mountRequireNoCollision(target); // 只读守卫；不改借用组件的碰撞设置。
        Addr source=0;
        if((plan.value("full_borrowed_kinds",0u)&(1u<<kind))!=0) {
            // 原生池仍完整绑定核验；即使本组不占显示位，也从其真实组件取原弹体网格。
            for(const auto& visual:state.visuals)if(visual.pool==kind && visual.index>=0) {
                source=visual.identity.address;break;
            }
            require(source!=0,"扩展借位：整组迁位缺少原生弹体网格来源");
        } else {
            source=state.physical[state.expansionPositions[kind].front()];
            require(state.expansionPools[kind].front()==kind,"扩展借位：缺少本兵装原生弹体作为网格来源");
        }
        const auto replacement=mountIdentity(mountBaselineMesh(mountIdentity(source),read<Addr>(source+0x728)));
        const auto original=mountIdentity(mountBaselineMesh(identity,read<Addr>(target+0x728)));
        require(!groups.at(kind).at("mesh").is_null() && mountName(replacement.address)==groups.at(kind).at("mesh").get<std::string>(),
            "扩展借位：弹体网格与已核兵装不符");
        state.meshOverrides.push_back({identity,original,replacement}); // 与F-15E相同账本，关闭/换组/失败逐项归还。
    }
}
