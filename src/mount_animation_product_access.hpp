// 在mount_animation_reflection_adapter.hpp之后、同一产品匿名命名空间内包含。
// 仅提供具体产品访问实现；不注册机型、不修改旧控制头、不安装任何钩子。
#pragma once

struct MountAnimationProductAccess final : ::dualsp::common_bay::native_parts::Access,
                                          ::dualsp::common_bay::bone_refresh::Access {
    using Binding=::dualsp::common_bay::Binding;
    using Identity=::dualsp::common_bay::Identity;
    using Aircraft=::dualsp::common_bay::Aircraft;
    using Setting=::dualsp::common_bay::bone_refresh::Setting;
    using Lookup=::dualsp::common_bay::bone_refresh::Lookup;
    MountAnimationReflection reflection;
    Binding owner{};
    bool ownerRegistered{};
    std::array<size_t,3> rackOffsets{};
    std::uint64_t metadataGeneration{};
    struct Descriptor { Setting setting; MountIdentity field,enumeration,cls; };
    std::vector<Descriptor> descriptors;
    struct HookReceipt {
        unsigned rva{};
        Addr detour{},trampoline{};
        std::vector<unsigned char> original,installed,trampolineBytes;
        bool installedQualified{};
    };
    std::vector<HookReceipt> hookReceipts;

    static MountIdentity productIdentity(Identity id) { return {id.address,id.index,id.serial}; }
    static bool equal(MountIdentity a,MountIdentity b) {
        return a.address==b.address && a.index==b.index && a.serial==b.serial;
    }
    static std::vector<unsigned char> decode(std::string_view hex) {
        require(!hex.empty() && !(hex.size()&1) && hex.size()<=1024,"动画访问：代码签名长度无效");
        auto digit=[](char c)->unsigned {
            if(c>='0' && c<='9')return c-'0';
            if(c>='a' && c<='f')return c-'a'+10;
            if(c>='A' && c<='F')return c-'A'+10;
            throw std::runtime_error("动画访问：代码签名不是十六进制");
        };
        std::vector<unsigned char> bytes;
        for(size_t i=0;i<hex.size();i+=2)bytes.push_back(static_cast<unsigned char>((digit(hex[i])<<4)|digit(hex[i+1])));
        return bytes;
    }
    static std::vector<unsigned char> bytes(Addr address,size_t size) {
        checkMemory(address,size);
        std::vector<unsigned char> result(size);
        std::memcpy(result.data(),reinterpret_cast<void*>(address),size); return result;
    }
    static Addr hookDestination(Addr target) {
        require(mountAnimationRead<unsigned char>(target)==0xe9,"动画访问：不是预期MinHook入口跳转");
        const Addr relay=target+5+static_cast<std::intptr_t>(mountAnimationRead<int>(target+1));
        if(mountAnimationRead<unsigned short>(relay)!=0x25ff)return relay;
        return mountAnimationRead<Addr>(relay+6+static_cast<std::intptr_t>(mountAnimationRead<int>(relay+2)));
    }
    // 产品安装器须在自己CreateHook之前调用；已存在未知跳转时拒绝记录。
    void beforeOwnHook(unsigned rva,std::string_view expected) {
        MountAnimationReflection::thread();
        const auto original=decode(expected);
        require(bytes(moduleBase()+rva,original.size())==original,"动画访问：安装前原始代码不符");
        for(const auto& receipt:hookReceipts)require(receipt.rva!=rva,"动画访问：钩子回执重复");
        hookReceipts.push_back({rva,0,0,original,{},{},false});
    }
    // 仅接受产品本次安装产生的入口跳转及其MinHook跳板，不接受任意第三方绕转。
    void afterOwnHook(unsigned rva,Addr ownDetour,Addr ownTrampoline) {
        MountAnimationReflection::thread();
        auto it=std::find_if(hookReceipts.begin(),hookReceipts.end(),[&](const HookReceipt& r){return r.rva==rva;});
        require(it!=hookReceipts.end() && !it->installedQualified && ownDetour && ownTrampoline,
                "动画访问：缺少安装前回执");
        const Addr target=moduleBase()+rva;
        require(hookDestination(target)==ownDetour,"动画访问：入口不属于本产品钩子");
        MEMORY_BASIC_INFORMATION m{};
        require(VirtualQuery(reinterpret_cast<void*>(ownTrampoline),&m,sizeof(m)) && m.State==MEM_COMMIT &&
                !(m.Protect&(PAGE_GUARD|PAGE_NOACCESS)) &&
                ((m.Protect&0xff)==PAGE_EXECUTE_READ || (m.Protect&0xff)==PAGE_EXECUTE_READWRITE),
                "动画访问：跳板不是有效可执行区");
        it->installed=bytes(target,it->original.size());
        it->trampolineBytes=bytes(ownTrampoline,64);
        it->detour=ownDetour; it->trampoline=ownTrampoline; it->installedQualified=true;
    }
    std::uintptr_t moduleBase() const override { return reinterpret_cast<Addr>(GetModuleHandleW(nullptr)); }
    bool onGameThread() const override {
        return mountGameThread.load()!=0 && GetCurrentThreadId()==mountGameThread.load();
    }
    bool alive(Identity id) override {
        try { return id.address && id.index>=0 && mountAlive(productIdentity(id)); } catch(...) { return false; }
    }
    bool ownerAlive(const Binding& b) override {
        if(!onGameThread() || !ownerRegistered || !::dualsp::common_bay::same(owner,b))return false;
        try {
            if(!alive(b.world) || !alive(b.plane) || !alive(b.activator) || !alive(b.root) || !alive(b.weaponBase))return false;
            if(mountAnimationRead<Addr>(b.plane.address+0x11a8)!=b.activator.address ||
               mountAnimationRead<Addr>(b.plane.address+0x160)!=b.root.address ||
               mountAnimationRead<Addr>(b.plane.address+0x1ae0)!=b.weaponBase.address ||
               mountAnimationRead<Addr>(b.weaponBase.address+0x100)!=b.root.address ||
               mountAnimationRead<Addr>(b.root.address+0x100)!=0)return false;
            if(mountOuterWorld(b.plane.address)!=b.world.address ||
               mountOuterWorld(b.root.address)!=b.world.address || mountOuterWorld(b.weaponBase.address)!=b.world.address)return false;
            for(auto rack:b.racks)if(!alive(rack) || mountOuterWorld(rack.address)!=b.world.address ||
                mountAnimationRead<Addr>(rack.address+0x100)!=b.weaponBase.address)return false;
            for(unsigned i=0;i<3;++i)if(!rackOffsets[i] || rackOffsets[i]>=0x10000 ||
                mountAnimationRead<Addr>(b.plane.address+rackOffsets[i])!=b.racks[i].address)return false;
            return true;
        } catch(...) { return false; }
    }
    // 生命周期绑定完成后登记；世界/组件/场景重建必须先归还租约，再撤销并递增generation重新登记。
    bool registerOwner(const Binding& b) {
        if(!onGameThread() || ownerRegistered || !::dualsp::common_bay::valid(b))return false;
        owner=b; ownerRegistered=true;
        if(ownerAlive(b))return true;
        owner={};ownerRegistered=false;return false;
    }
    void forgetOwner() { MountAnimationReflection::thread();owner={};ownerRegistered=false;descriptors.clear(); }
    bool inheritsNativePlayerPlane(Identity plane) override {
        try { return alive(plane) && mountClassIs(plane.address,"PlayerPlane","/Script/Nimbus"); }
        catch(...) { return false; }
    }
    bool canonicalAircraft(Identity plane,Aircraft aircraft) override {
        const char* name{};const char* package{};
        switch(aircraft) {
        case Aircraft::F22a:name="AcePlayerPawn_f22a_C";package="/Game/Blueprint/Player/Pawn/AcePlayerPawn_f22a";break;
        case Aircraft::Su47:name="AcePlayerPawn_su47_C";package="/Game/Blueprint/Player/Pawn/AcePlayerPawn_su47";break;
        case Aircraft::Su57TopGun:name="AcePlayerPawn_su57tg_C";package="/Game/Blueprint/Player/Pawn/AcePlayerPawn_su57tg";break;
        default:return false; // 本轮产品接线仅限定主管正在整合的三架；YF-23未自动开放。
        }
        try { return alive(plane) && mountClassIs(plane.address,name,package); } catch(...) { return false; }
    }
    bool originalCodeMatches(std::uint32_t rva,std::string_view expected) override {
        if(!onGameThread())return false;
        try {
            const auto wanted=decode(expected);
            for(const auto& r:hookReceipts)if(r.rva==rva) {
                return r.installedQualified && r.original==wanted &&
                       hookDestination(moduleBase()+rva)==r.detour &&
                       bytes(moduleBase()+rva,r.installed.size())==r.installed &&
                       bytes(r.trampoline,r.trampolineBytes.size())==r.trampolineBytes;
            }
            return bytes(moduleBase()+rva,wanted.size())==wanted;
        } catch(...) { return false; }
    }
    bool moduleReadOnlyPointer(std::uintptr_t address) override {
        MEMORY_BASIC_INFORMATION m{};
        if(!VirtualQuery(reinterpret_cast<void*>(address),&m,sizeof(m)) || m.State!=MEM_COMMIT ||
           m.Type!=MEM_IMAGE || reinterpret_cast<Addr>(m.AllocationBase)!=moduleBase() ||
           (m.Protect&(PAGE_GUARD|PAGE_NOACCESS)))return false;
        return (m.Protect&0xff)==PAGE_READONLY || (m.Protect&0xff)==PAGE_EXECUTE_READ;
    }
    bool read(std::uintptr_t address,void* output,std::size_t size) override {
        if(!onGameThread() || !output || !size)return false;
        try { checkMemory(address,size);std::memcpy(output,reinterpret_cast<void*>(address),size);return true; }
        catch(...) { return false; }
    }
    bool writable(std::uintptr_t address,std::size_t size) override {
        return onGameThread() && MountAnimationReflection::writable(address,size);
    }
    bool write(std::uintptr_t address,const void* input,std::size_t size) override {
        if(!input || !writable(address,size))return false;
        try { std::memcpy(reinterpret_cast<void*>(address),input,size);
            return std::memcmp(reinterpret_cast<void*>(address),input,size)==0; } catch(...) { return false; }
    }
    Lookup inspect(Identity provider,std::string_view property,Setting& result) override {
        if(!onGameThread() || !alive(provider))return Lookup::Invalid;
        try {
            reflection.bindSdk();
            require(mountClassIs(provider.address,"SkeletalMeshComponent","/Script/Engine"),"动画访问：刷新对象不是骨架组件");
            const bool old=property=="MeshComponentUpdateFlag";
            if(!old && property!="VisibilityBasedAnimTickOption")return Lookup::Invalid;
            const Addr cls=mountAnimationRead<Addr>(provider.address+0x10);
            const std::string name(property);
            const Addr candidate=MountAnimationReflection::find(cls,name.c_str());
            if(!candidate)return Lookup::Missing;
            const auto f=reflection.field(cls,name.c_str(),"ByteProperty",1,reflection.typeSize(cls),true);
            const char* enumeration=old?"EMeshComponentUpdateFlag":"EVisibilityBasedAnimTickOption";
            const Addr e=MountAnimationReflection::refValue<Addr>(reflection.enumRef,f.identity.address);
            const unsigned always=reflection.enumValue(e,enumeration,"AlwaysTickPoseAndRefreshBones");
            MountAnimationReflection::named(mountAnimationRead<Addr>(f.identity.address+0x20),"SkinnedMeshComponent","/Script/Engine");
            const auto enumId=mountIdentity(e),classId=mountIdentity(cls);
            for(const auto& d:descriptors)if(::dualsp::common_bay::same(d.setting.provider,provider) && d.setting.property==name &&
                equal(d.field,f.identity) && equal(d.enumeration,enumId) && equal(d.cls,classId) &&
                d.setting.offset==static_cast<unsigned>(f.offset) && d.setting.alwaysValue==always) {
                result=d.setting;return Lookup::Found;
            }
            require(metadataGeneration!=~std::uint64_t{0},"动画访问：元数据代次耗尽");
            result={provider,name,std::string("/Script/Engine.")+enumeration,"AlwaysTickPoseAndRefreshBones",
                    static_cast<unsigned>(f.offset),1,static_cast<unsigned>(reflection.typeSize(cls)),always,++metadataGeneration};
            descriptors.push_back({result,f.identity,enumId,classId});return Lookup::Found;
        } catch(...) { return Lookup::Invalid; }
    }
    bool descriptorCurrent(const Setting& s) override {
        if(!onGameThread() || !::dualsp::common_bay::bone_refresh::valid(s) || !alive(s.provider))return false;
        // 绑定已完整反射核对；热路径只复核持有的元数据身份及实际布局，避免每帧反复遍历字段链。
        try {
            for(const auto& d:descriptors)if(d.setting.metadataGeneration==s.metadataGeneration &&
                ::dualsp::common_bay::same(d.setting.provider,s.provider) && d.setting.property==s.property) {
                MountAnimationReflection::alive(d.field);MountAnimationReflection::alive(d.enumeration);
                MountAnimationReflection::alive(d.cls);
                return mountAnimationRead<Addr>(s.provider.address+0x10)==d.cls.address &&
                    mountAnimationRead<int>(d.field.address+0x44)==static_cast<int>(s.offset) &&
                    mountAnimationRead<int>(d.field.address+0x34)==1 &&
                    MountAnimationReflection::refValue<Addr>(reflection.enumRef,d.field.address)==d.enumeration.address &&
                    d.setting.offset==s.offset && d.setting.width==s.width && d.setting.instanceExtent==s.instanceExtent &&
                    d.setting.enumPath==s.enumPath && d.setting.item==s.item && d.setting.alwaysValue==s.alwaysValue;
            }
        }catch(...) { }
        return false;
    }
    bool read(const Setting& s,unsigned& result) override {
        if(!descriptorCurrent(s))return false;
        unsigned char byte{};
        if(!read(s.provider.address+s.offset,&byte,1))return false;
        result=byte;return true;
    }
    bool write(const Setting& s,unsigned value) override {
        if(value>255 || reflection.ownWrite || !descriptorCurrent(s))return false;
        reflection.ownWrite=true;
        struct Reset { bool& flag;~Reset(){flag=false;} } reset{reflection.ownWrite};
        const auto byte=static_cast<unsigned char>(value);
        return write(s.provider.address+s.offset,&byte,1);
    }
};
