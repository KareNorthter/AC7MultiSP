// Su-47首批候选只接受离线全LOD复核过的实际原生端点；不设置非零姿态容差。
#pragma once

struct MountAnimationExactPose {
    struct Component {
        MountIdentity object,mesh,skeleton,animClass,animInstance;
        size_t meshOffset{},skeletonOffset{},classOffset{},instanceOffset{},meshSocketsOffset{};
        std::vector<std::array<unsigned,10>> bones;
    };
    struct Local {
        MountIdentity object,parent;
        std::array<size_t,3> offsets{};
        std::array<unsigned,9> expected{};
        size_t parentOffset{},socketOffset{};
    };
    struct Socket {
        MountIdentity object,skeleton;
        size_t arrayOffset{},nativeIndex{};
        std::array<size_t,3> offsets{};
        std::array<unsigned,9> expected{};
        size_t nameOffset{},boneOffset{};
        std::array<unsigned,2> name{},bone{};
        int expectedCount{};
    };
    struct Published { std::uint64_t frame{};int buffer{-1};bool valid{}; };
    std::array<Component,4> components{}; // SWP1/2/3、Body；未使用组不读取骨骼。
    std::array<Published,4> published{};
    std::vector<Local> locals;
    std::vector<Socket> sockets;
    unsigned required{};
    std::uint64_t generation{};
    bool prepared{};

    static std::array<unsigned,10> transformBits(Addr address) {
        std::array<unsigned,12> raw{};checkMemory(address,48);
        std::memcpy(raw.data(),reinterpret_cast<void*>(address),48);
        return {raw[0],raw[1],raw[2],raw[3],raw[4],raw[5],raw[6],raw[8],raw[9],raw[10]};
    }
    static bool vectorBits(Addr object,const std::array<size_t,3>& offsets,const std::array<unsigned,9>& expected) {
        for(unsigned i=0;i<3;++i) {
            std::array<unsigned,3> value{};checkMemory(object+offsets[i],12);
            std::memcpy(value.data(),reinterpret_cast<void*>(object+offsets[i]),12);
            if(!std::equal(value.begin(),value.end(),expected.begin()+i*3))return false;
        }
        return true;
    }
    void clear() { *this={}; }
    void bindComponent(unsigned slot,Addr object,const Json& reference) {
        auto& c=components.at(slot);c.object=mountIdentity(object);
        c.meshOffset=mountProperty(object,"SkeletalMesh","ObjectProperty",8);
        c.classOffset=mountProperty(object,"AnimClass","ClassProperty",8);
        c.instanceOffset=mountProperty(object,"AnimScriptInstance","ObjectProperty",8);
        c.mesh=mountIdentity(read<Addr>(object+c.meshOffset));c.animClass=mountIdentity(read<Addr>(object+c.classOffset));
        c.animInstance=mountIdentity(read<Addr>(object+c.instanceOffset));
        c.skeletonOffset=mountProperty(c.mesh.address,"Skeleton","ObjectProperty",8);
        c.skeleton=mountIdentity(read<Addr>(c.mesh.address+c.skeletonOffset));
        c.meshSocketsOffset=mountProperty(c.mesh.address,"Sockets","ArrayProperty",16); // 本机9855922真实反射名，已只读核对。
        require(reference.at("reference_kind")=="native_observed_geometry_checked" &&
            mountName(c.mesh.address)==reference.at("mesh").get<std::string>() &&
            mountName(c.skeleton.address)==reference.at("skeleton").get<std::string>(),"Su-47：严格姿态资源未核对");
        const auto& bones=reference.at("bones");
        require(bones.size()==(slot==3?38u:6u),"Su-47：完整骨数改变");
        c.bones.resize(bones.size());
        std::vector<bool> seen(bones.size());
        for(const auto& b:bones) {
            const unsigned index=b.at("mesh_bone_index").get<unsigned>();
            require(index<bones.size() && !seen[index],"Su-47：骨索引不双射");seen[index]=true;
            c.bones[index]=b.at("component").at("uint32").get<std::array<unsigned,10>>();
        }
    }
    void bindLocal(Addr object,MountIdentity parent,const Json& expected) {
        Local l;l.object=mountIdentity(object);l.parent=parent;
        l.offsets={mountProperty(object,"RelativeLocation","StructProperty",12),
            mountProperty(object,"RelativeRotation","StructProperty",12),mountProperty(object,"RelativeScale3D","StructProperty",12)};
        l.parentOffset=mountProperty(object,"AttachParent","ObjectProperty",8);
        l.socketOffset=mountProperty(object,"AttachSocketName","NameProperty",8);
        l.expected=expected.at("local_position_rotation_scale").at("uint32").get<std::array<unsigned,9>>();
        require(expected.at("required_identity_local_transform").get<bool>(),"Su-47：严格端点缺相对变换依据");
        locals.push_back(l);
    }
    void bindSockets(unsigned slot,const Json& expected) {
        const auto& c=components[slot];
        const auto offset=mountProperty(c.skeleton.address,"Sockets","ArrayProperty",16);
        const Addr array=c.skeleton.address+offset;
        const int count=read<int>(array+8),capacity=read<int>(array+12);
        require(count>=0 && count<=capacity && count==static_cast<int>(expected.size()),"Su-47：严格端点Socket集合改变");
        const Addr data=read<Addr>(array);
        std::set<std::string> seen;
        for(int i=0;i<count;++i) {
            const Addr object=read<Addr>(data+i*8);Socket s;s.object=mountIdentity(object);s.skeleton=c.skeleton;
            require(mountClassIs(object,"SkeletalMeshSocket","/Script/Engine"),"Su-47：Socket类不符");
            s.arrayOffset=offset;s.nativeIndex=i;s.expectedCount=count;
            s.nameOffset=mountProperty(object,"SocketName","NameProperty",8);s.boneOffset=mountProperty(object,"BoneName","NameProperty",8);
            s.name={read<unsigned>(object+s.nameOffset),read<unsigned>(object+s.nameOffset+4)};
            s.bone={read<unsigned>(object+s.boneOffset),read<unsigned>(object+s.boneOffset+4)};
            auto name=mountNameIndex(s.name[0]);for(char& v:name)if(v>='A' && v<='Z')v+=32;
            require(!s.name[1] && !s.bone[1] && expected.contains(name) && seen.insert(name).second,
                "Su-47：Socket名字或编号不符");
            const auto& item=expected.at(name);
            require(mountNameIndex(s.bone[0])==item.at("bone").get<std::string>(),"Su-47：Socket依附骨改变");
            s.offsets={mountProperty(object,"RelativeLocation","StructProperty",12),
                mountProperty(object,"RelativeRotation","StructProperty",12),mountProperty(object,"RelativeScale","StructProperty",12)};
            s.expected=item.at("local").at("uint32").get<std::array<unsigned,9>>();sockets.push_back(s);
        }
    }
    void prepare(const FixedMountState& state,MountAnimationHost& host) {
        clear();const auto& reference=animatedMountData().at("su47_native_reference");
        require(reference.at("geometry_passed_at_reference").get<bool>(),"Su-47：真实端点几何未通过");
        required=(state.animationDescription.providerMask|state.animationDescription.presentationMask)|8u;
        const Addr body=read<Addr>(state.plane.address+mountProperty(state.plane.address,"PlaneBodyMesh","ObjectProperty",8));
        bindComponent(3,body,reference.at("body"));
        for(unsigned i=0;i<3;++i)if(required&(1u<<i)) {
            const auto key="SWP"+std::to_string(i+1);
            bindComponent(i,state.animationRacks[i].address,reference.at("providers").at(key));
            bindSockets(i,reference.at("socket_expectations").at(key));
        }
        const auto& r=reference.at("relative_components");
        bindLocal(body,state.root,r.at("body"));bindLocal(state.weaponBase.address,state.root,r.at("weapon_base"));
        for(unsigned i=0;i<3;++i)if(required&(1u<<i))bindLocal(state.animationRacks[i].address,state.weaponBase,r.at("SWP"+std::to_string(i+1)));
        generation=host.binding.generation;prepared=true;
    }
    bool resourcesCurrent(const Component& c) const {
        return mountAlive(c.object) && mountAlive(c.mesh) && mountAlive(c.skeleton) && mountAlive(c.animClass) && mountAlive(c.animInstance) &&
            read<Addr>(c.object.address+c.meshOffset)==c.mesh.address && read<Addr>(c.object.address+c.classOffset)==c.animClass.address &&
            read<Addr>(c.object.address+c.instanceOffset)==c.animInstance.address && read<Addr>(c.mesh.address+c.skeletonOffset)==c.skeleton.address &&
            read<int>(c.mesh.address+c.meshSocketsOffset+8)==0;
    }
    bool matches(MountAnimationHost& host) const {
        if(!prepared || generation!=host.binding.generation || !host.controller.active() || !host.bindingAlive(host.binding))return false;
        ::dualsp::common_bay::PartsSample parts;
        if(!host.readParts(host.binding,parts) || !MountAnimationPublicationTracker::nativeOpen(parts))return false;
        const auto frame=read<std::uint64_t>(host.access.moduleBase()+0x3ac5520);
        const auto root=transformBits(host.binding.root.address+0x180);
        for(unsigned i=0;i<4;++i)if(required&(1u<<i)) {
            const auto& c=components[i];const auto& p=published[i];
            if(!p.valid || p.frame!=frame || !resourcesCurrent(c) || read<unsigned>(c.object.address+0xd24)!=static_cast<unsigned>(frame) ||
               read<unsigned char>(c.object.address+0x828) || read<int>(c.object.address+0x74c)!=p.buffer ||
               transformBits(c.object.address+0x180)!=root)return false;
            const Addr array=c.object.address+0x728+p.buffer*16;
            const int count=read<int>(array+8),capacity=read<int>(array+12);
            if(count!=static_cast<int>(c.bones.size()) || count>capacity)return false;
            const Addr data=read<Addr>(array);checkMemory(data,count*48);
            for(int bone=0;bone<count;++bone) {
                std::array<unsigned,12> v{};std::memcpy(v.data(),reinterpret_cast<void*>(data+bone*48),48);
                const std::array<unsigned,10> bits={v[0],v[1],v[2],v[3],v[4],v[5],v[6],v[8],v[9],v[10]};
                if(bits!=c.bones[bone])return false;
            }
        }
        for(const auto& l:locals)if(!mountAlive(l.object) || !mountAlive(l.parent) ||
            read<Addr>(l.object.address+l.parentOffset)!=l.parent.address || read<std::uint64_t>(l.object.address+l.socketOffset)!=0 ||
            !vectorBits(l.object.address,l.offsets,l.expected))return false;
        if(transformBits(host.binding.weaponBase.address+0x180)!=root)return false;
        for(const auto& s:sockets) {
            if(!mountAlive(s.object) || !mountAlive(s.skeleton))return false;
            const Addr array=s.skeleton.address+s.arrayOffset;
            if(read<int>(array+8)!=s.expectedCount || read<Addr>(read<Addr>(array)+s.nativeIndex*8)!=s.object.address ||
               read<unsigned>(s.object.address+s.nameOffset)!=s.name[0] || read<unsigned>(s.object.address+s.nameOffset+4)!=s.name[1] ||
               read<unsigned>(s.object.address+s.boneOffset)!=s.bone[0] || read<unsigned>(s.object.address+s.boneOffset+4)!=s.bone[1] ||
               !vectorBits(s.object.address,s.offsets,s.expected))return false;
        }
        return read<std::uint64_t>(host.access.moduleBase()+0x3ac5520)==frame;
    }
};
