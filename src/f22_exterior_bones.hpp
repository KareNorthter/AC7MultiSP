// 在mount_animation_reflection_adapter.hpp之后、产品已有匿名命名空间内包含。
// 只在绑定/换装时调用：Init前release，Init正常结束后重新bind；不放入逐帧更新。
// 原版f22a_SWP3腹15骨的最小根为SWPCover_16..24；不改侧部25..38、Socket、动画或物理体。
#pragma once

struct MountExteriorBones {
    using Reflection=MountAnimationReflection;
    using Name=MountAnimationReflection::SocketName; // 原生8字节FName；来自GetBoneName，绝不构造索引。
    using Function=MountAnimationReflection::Function;
    using Field=MountAnimationReflection::Field;
    struct Bone { Name name; int index{-1}; };
    struct Root { Bone bone; bool nativeHidden{},restoreNeeded{}; };
    struct Expected { const char* name; const char* parent; };
    static constexpr std::array<Expected,30> expectedBones={{
        {"Body","None"},
        {"SWPCover_38","Body"},{"SWPCover_37","Body"},{"SWPCover_36","Body"},{"SWPCover_35","Body"},
        {"SWPCover_34","Body"},{"SWPCover_28","SWPCover_34"},{"SWPCover_27","SWPCover_34"},
        {"SWPCover_33","Body"},{"SWPCover_26","SWPCover_33"},{"SWPCover_25","SWPCover_33"},
        {"SWPCover_32","Body"},{"SWPCover_31","Body"},{"SWPCover_30","Body"},{"SWPCover_29","Body"},
        {"SWPCover_24","Body"},{"SWPCover_10","SWPCover_24"},{"SWPCover_09","SWPCover_24"},
        {"SWPCover_23","Body"},{"SWPCover_22","Body"},
        {"SWPCover_21","Body"},{"SWPCover_08","SWPCover_21"},{"SWPCover_07","SWPCover_21"},
        {"SWPCover_20","Body"},{"SWPCover_19","Body"},
        {"SWPCover_18","Body"},{"SWPCover_06","SWPCover_18"},{"SWPCover_05","SWPCover_18"},
        {"SWPCover_17","Body"},{"SWPCover_16","Body"}
    }};
    static constexpr std::array<const char*,9> rootNames={{
        "SWPCover_16","SWPCover_17","SWPCover_18","SWPCover_19","SWPCover_20",
        "SWPCover_21","SWPCover_22","SWPCover_23","SWPCover_24"
    }};

private:
    Reflection reflection;
    MountIdentity component{},componentClass{},mesh{},skeleton{},physicsEnum{};
    Field meshProperty{},skeletonProperty{};
    Function numBones{},boneName{},parentBone{},hideBone{},unhideBone{},isHidden{};
    Field countResult{},boneIndex{},nameResult{},parentInput{},parentResult{},hideName{},hidePhysics{},unhideName{},hiddenName{};
    Reflection::BoolField hiddenResult{};
    std::array<Root,9> roots{};
    Bone body{};
    unsigned pboNone{};
    bool bound_{},operating_{},applied_{},lastHidden_{},restorePending_{};
    std::string error_;

    static bool sameName(Name a,Name b) { return a.index==b.index && a.number==b.number; }
    static bool sameText(std::string a,const char* b) {
        std::string expected(b);
        auto lower=[](std::string& text) { for(auto& ch:text)if(ch>='A' && ch<='Z')ch=static_cast<char>(ch-'A'+'a'); };
        lower(a);lower(expected);return a==expected;
    }
    static std::string text(Name name) {
        auto result=mountNameIndex(name.index);
        if(name.number)result+="_"+std::to_string(name.number-1);
        return result;
    }
    static void putName(unsigned char* params,Field field,Name name) { std::memcpy(params+field.offset,&name,8); }
    static Name getName(const unsigned char* params,Field field) {
        Name name{};std::memcpy(&name,params+field.offset,8);return name;
    }
    static void resource(MountIdentity id,const char* name,const char* package,const char* kind) {
        Reflection::alive(id);
        Reflection::named(id.address,name,package);
        require(mountName(read<Addr>(id.address+0x10))==kind,"F22骨显隐：资源类型不符");
    }
    void componentCurrent() const {
        Reflection::thread();
        Reflection::alive(component);Reflection::alive(componentClass);Reflection::alive(mesh);Reflection::alive(skeleton);
        require(read<Addr>(component.address+0x10)==componentClass.address &&
                read<Addr>(component.address+meshProperty.offset)==mesh.address &&
                read<Addr>(mesh.address+skeletonProperty.offset)==skeleton.address,
                "F22骨显隐：组件类或骨架资源已更换，旧租约不得写入");
    }
    void current() const {
        componentCurrent();
        Reflection::alive(meshProperty.identity);Reflection::alive(skeletonProperty.identity);Reflection::alive(physicsEnum);
        for(const auto& f:{numBones,boneName,parentBone,hideBone,unhideBone,isHidden}) {
            Reflection::alive(f.identity);
            require(Reflection::refValue<unsigned short>(reflection.parms,f.identity.address)==f.size,
                    "F22骨显隐：缓存函数参数尺寸改变");
        }
        for(const auto& p:{countResult,boneIndex,nameResult,parentInput,parentResult,hideName,hidePhysics,unhideName,hiddenName,hiddenResult.field}) {
            Reflection::alive(p.identity);
            require(read<int>(p.identity.address+0x44)==p.offset && read<int>(p.identity.address+0x34)==p.size,
                    "F22骨显隐：缓存参数布局改变");
        }
    }
    int count() {
        alignas(16) std::array<unsigned char,256> params{};
        reflection.invoke(component,numBones,params.data());
        int value{};std::memcpy(&value,params.data()+countResult.offset,4);return value;
    }
    Name nameAt(int index) {
        alignas(16) std::array<unsigned char,256> params{};
        std::memcpy(params.data()+boneIndex.offset,&index,4);
        reflection.invoke(component,boneName,params.data());return getName(params.data(),nameResult);
    }
    Name parent(Name name) {
        alignas(16) std::array<unsigned char,256> params{};
        putName(params.data(),parentInput,name);
        reflection.invoke(component,parentBone,params.data());return getName(params.data(),parentResult);
    }
    bool hidden(Name name) {
        alignas(16) std::array<unsigned char,256> params{};
        putName(params.data(),hiddenName,name);
        reflection.invoke(component,isHidden,params.data());return Reflection::getBool(params.data(),hiddenResult);
    }
    void setOne(Root& root,bool wanted) {
        // 完整元数据/骨表在批处理开始核对；逐根仅核组件和缓存的实际函数身份。
        componentCurrent();
        if(hidden(root.bone.name)==wanted)return;
        // ProcessEvent可能先改变状态再报失败，必须在第一笔写前保留归还责任。
        root.restoreNeeded=true;
        require(!reflection.ownWrite,"F22骨显隐：重入写入");
        reflection.ownWrite=true;
        struct Reset { bool& flag;~Reset(){flag=false;} } reset{reflection.ownWrite};
        alignas(16) std::array<unsigned char,256> params{};
        if(wanted) {
            putName(params.data(),hideName,root.bone.name);
            params[hidePhysics.offset]=static_cast<unsigned char>(pboNone);
            reflection.invoke(component,hideBone,params.data());
        } else {
            putName(params.data(),unhideName,root.bone.name);
            reflection.invoke(component,unhideBone,params.data());
        }
        componentCurrent();
        require(hidden(root.bone.name)==wanted,"F22骨显隐：原生显隐回读不符");
    }
    void clearBinding(bool discardCache=false) {
        // 正常release保留只读接口/FName缓存；同组件Init后bind仅重采原生hidden。
        for(auto& root:roots) { root.nativeHidden=false;root.restoreNeeded=false; }
        if(discardCache) { component={};componentClass={};mesh={};skeleton={};roots={};body={}; }
        bound_=applied_=lastHidden_=restorePending_=false;
    }
    bool restoreAll() {
        bool complete=true;
        bool needed=false;for(const auto& root:roots)needed=needed || root.restoreNeeded;
        if(!needed) { clearBinding();return true; }
        // 只作一次完整缓存检查；参数布局失效时不能冒险调用旧接口，保留全部归还责任。
        try { current(); }
        catch(...) {
            try { if(!mountAlive(component)) { clearBinding(true);return true; } } catch(...) {}
            restorePending_=true;return false;
        }
        for(auto& root:roots)if(root.restoreNeeded) {
            try {
                if(!mountAlive(component)) { clearBinding(true);return true; } // 死亡对象不再调用原生接口。
                setOne(root,root.nativeHidden);root.restoreNeeded=false;
            } catch(...) { complete=false; } // 一根失败不阻止其他根尽力归还。
        }
        if(complete)clearBinding();else restorePending_=true;
        return complete;
    }
    void prepare(Addr swp3) {
        reflection.bindSdk();component=mountIdentity(swp3);Reflection::alive(component);
        require(mountClassIs(swp3,"SkeletalMeshComponent","/Script/Engine"),"F22骨显隐：要求原生骨架组件");
        componentClass=mountIdentity(read<Addr>(swp3+0x10));
        meshProperty=reflection.field(componentClass.address,"SkeletalMesh","ObjectProperty",8,
                                      reflection.typeSize(componentClass.address),true);
        mesh=mountIdentity(read<Addr>(swp3+meshProperty.offset));
        resource(mesh,"f22a_SWP3","/Game/Vehicles/Aircraft/f22a/f22a_SWP3","SkeletalMesh");
        const Addr meshClass=read<Addr>(mesh.address+0x10);
        skeletonProperty=reflection.field(meshClass,"Skeleton","ObjectProperty",8,reflection.typeSize(meshClass),true);
        skeleton=mountIdentity(read<Addr>(mesh.address+skeletonProperty.offset));
        resource(skeleton,"f22a_SWP3_Skeleton","/Game/Vehicles/Aircraft/f22a/f22a_SWP3_Skeleton","Skeleton");
        numBones=reflection.function(swp3,"GetNumBones","SkinnedMeshComponent",1);
        countResult=reflection.field(numBones.identity.address,"ReturnValue","IntProperty",4,numBones.size);
        boneName=reflection.function(swp3,"GetBoneName","SkinnedMeshComponent",2);
        boneIndex=reflection.field(boneName.identity.address,"BoneIndex","IntProperty",4,boneName.size);
        nameResult=reflection.field(boneName.identity.address,"ReturnValue","NameProperty",8,boneName.size);
        Reflection::separate(boneIndex,nameResult);
        parentBone=reflection.function(swp3,"GetParentBone","SkinnedMeshComponent",2);
        parentInput=reflection.field(parentBone.identity.address,"BoneName","NameProperty",8,parentBone.size);
        parentResult=reflection.field(parentBone.identity.address,"ReturnValue","NameProperty",8,parentBone.size);
        Reflection::separate(parentInput,parentResult);
        hideBone=reflection.function(swp3,"HideBoneByName","SkinnedMeshComponent",2);
        hideName=reflection.field(hideBone.identity.address,"BoneName","NameProperty",8,hideBone.size);
        hidePhysics=reflection.field(hideBone.identity.address,"PhysBodyOption","ByteProperty",1,hideBone.size);
        Reflection::separate(hideName,hidePhysics);
        physicsEnum=mountIdentity(Reflection::refValue<Addr>(reflection.enumRef,hidePhysics.identity.address));
        pboNone=reflection.enumValue(physicsEnum.address,"EPhysBodyOp","PBO_None");
        unhideBone=reflection.function(swp3,"UnHideBoneByName","SkinnedMeshComponent",1);
        unhideName=reflection.field(unhideBone.identity.address,"BoneName","NameProperty",8,unhideBone.size);
        isHidden=reflection.function(swp3,"IsBoneHiddenByName","SkinnedMeshComponent",2);
        hiddenName=reflection.field(isHidden.identity.address,"BoneName","NameProperty",8,isHidden.size);
        hiddenResult=reflection.boolean(isHidden.identity.address,"ReturnValue",isHidden.size);
        Reflection::separate(hiddenName,hiddenResult.field);
        current();
        require(count()==static_cast<int>(expectedBones.size()),"F22骨显隐：骨数不符合原版30骨布局");
        std::array<Bone,30> actual{};
        std::array<bool,30> seen{};
        for(int i=0;i<static_cast<int>(expectedBones.size());++i) {
            const Name name=nameAt(i);const auto label=text(name);
            size_t found=expectedBones.size();
            for(size_t j=0;j<expectedBones.size();++j)if(sameText(label,expectedBones[j].name)) { found=j;break; }
            require(found<expectedBones.size() && !seen[found],"F22骨显隐：缺骨、重名或未知骨");
            seen[found]=true;actual[found]={name,i};
        }
        body=actual[0];
        for(size_t i=0;i<actual.size();++i) {
            const Name actualParent=parent(actual[i].name);
            if(i==0)require(sameText(text(actualParent),"None"),"F22骨显隐：Body不是根骨");
            else {
                size_t expected=actual.size();
                for(size_t j=0;j<actual.size();++j)if(sameText(expectedBones[j].name,expectedBones[i].parent)) { expected=j;break; }
                require(expected<actual.size() && sameName(actualParent,actual[expected].name),"F22骨显隐：腹/侧骨父链不符");
            }
        }
        // Body隐藏会让根的有效hidden与原生独立请求混淆；绑定前保守拒绝，尚无任何写入。
        require(!hidden(body.name),"F22骨显隐：Body骨已隐藏，不能取得根显隐租约");
        for(size_t i=0;i<roots.size();++i) {
            size_t found=actual.size();
            for(size_t j=0;j<actual.size();++j)if(sameText(expectedBones[j].name,rootNames[i])) { found=j;break; }
            require(found<actual.size(),"F22骨显隐：腹根缺失");
            roots[i]={actual[found],hidden(actual[found].name),false};
        }
        current();bound_=true; // 全部接口/骨名/父链/原生hidden完成读取后才允许接管写入。
    }

public:
    MountExteriorBones()=default;
    ~MountExteriorBones()=default; // 析构不调用游戏；主树须显式release，shared_ptr共享同一账本。
    MountExteriorBones(const MountExteriorBones&)=delete;
    MountExteriorBones& operator=(const MountExteriorBones&)=delete;
    MountExteriorBones(MountExteriorBones&&)=delete;
    MountExteriorBones& operator=(MountExteriorBones&&)=delete;

    void bind(Addr swp3) {
        try {
            Reflection::thread();
            require(!bound_ && !operating_,"F22骨显隐：旧租约尚未归还");
            operating_=true;
            struct Reset { bool& flag;~Reset(){flag=false;} } reset{operating_};
            if(component.address==swp3 && mountAlive(component)) {
                bool reusable=false;
                try { current();reusable=count()==static_cast<int>(expectedBones.size()); } catch(...) { reusable=false; }
                if(reusable) {
                    require(!hidden(body.name),"F22骨显隐：Body骨已隐藏，不能重绑腹根租约");
                    std::array<bool,9> baseline{};
                    for(size_t i=0;i<roots.size();++i)baseline[i]=hidden(roots[i].bone.name);
                    componentCurrent();
                    for(size_t i=0;i<roots.size();++i) { roots[i].nativeHidden=baseline[i];roots[i].restoreNeeded=false; }
                    bound_=true;applied_=lastHidden_=restorePending_=false;error_.clear();return;
                }
            }
            MountExteriorBones next;next.prepare(swp3);
            // 不覆盖当前operating_的RAII引用；复制完成后再用bound_发布绑定。
            component=next.component;componentClass=next.componentClass;mesh=next.mesh;skeleton=next.skeleton;
            physicsEnum=next.physicsEnum;meshProperty=next.meshProperty;skeletonProperty=next.skeletonProperty;
            numBones=next.numBones;boneName=next.boneName;parentBone=next.parentBone;hideBone=next.hideBone;
            unhideBone=next.unhideBone;isHidden=next.isHidden;countResult=next.countResult;boneIndex=next.boneIndex;
            nameResult=next.nameResult;parentInput=next.parentInput;parentResult=next.parentResult;hideName=next.hideName;
            hidePhysics=next.hidePhysics;unhideName=next.unhideName;hiddenName=next.hiddenName;hiddenResult=next.hiddenResult;
            reflection=next.reflection;roots=next.roots;body=next.body;pboNone=next.pboNone;
            bound_=true;applied_=lastHidden_=restorePending_=false;error_.clear();
        } catch(const std::exception& error) { error_=error.what();throw; }
        catch(...) { error_="F22骨显隐：绑定访问失败";throw std::runtime_error(error_); }
    }
    void setAbdomenHidden(bool hide) {
        bool mayRollback=false;
        try {
            Reflection::thread();require(bound_ && !restorePending_ && !operating_,"F22骨显隐：无可用绑定或仍待归还");
            if(!mountAlive(component)) { clearBinding(true);return; }
            mayRollback=true;
            operating_=true;
            struct Reset { bool& flag;~Reset(){flag=false;} } reset{operating_};
            componentCurrent();
            if(applied_ && lastHidden_==hide)return; // 同一请求不重复查骨或调用显隐，不能当逐帧维持器。
            current();
            require(!hidden(body.name),"F22骨显隐：Body骨被隐藏，停止腹根接管");
            // 写前预读全部根，读取失败时第一笔也不写。
            for(const auto& root:roots)hidden(root.bone.name);
            for(auto& root:roots) {
                const bool wanted=hide || root.nativeHidden; // false归还基线，不强行显示原生已隐藏的根。
                setOne(root,wanted);root.restoreNeeded=wanted!=root.nativeHidden;
            }
            applied_=true;lastHidden_=hide;error_.clear();return;
        } catch(const std::exception& error) { error_=error.what(); }
        catch(...) { error_="F22骨显隐：显隐访问失败"; }
        // 不能在重入拒绝时归还外层仍在执行的操作。
        if(mayRollback && !operating_ && bound_) {
            operating_=true;
            struct Reset { bool& flag;~Reset(){flag=false;} } reset{operating_};
            if(!restoreAll())error_+="；部分腹根仍待归还";
        }
        throw std::runtime_error(error_);
    }
    void release() {
        if(!bound_)return;
        try {
            Reflection::thread();require(!operating_,"F22骨显隐：操作尚未结束，不能归还");
            operating_=true;
            struct Reset { bool& flag;~Reset(){flag=false;} } reset{operating_};
            for(const auto& root:roots)if(root.restoreNeeded)restorePending_=true;
            if(!mountAlive(component)) { clearBinding(true);error_.clear();return; }
            require(restoreAll(),"F22骨显隐：部分腹根仍待归还");
            error_.clear();
        } catch(const std::exception& error) { error_=error.what();throw; }
        catch(...) { error_="F22骨显隐：归还访问失败";throw std::runtime_error(error_); }
    }
    bool active() const { return bound_; }
    bool restoring() const { return restorePending_; }
    const std::string& lastError() const { return error_; }
};
