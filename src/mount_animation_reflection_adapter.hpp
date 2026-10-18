// 在weapon_bridge.cpp匿名命名空间内、mount_preview.hpp之后包含。
// 三个common_bay头须先在全局包含。本文件不自动注册、不安装钩子、不调用游戏。
#pragma once

struct MountAnimationReflection {
    using Ref=const void*(*)(const void*);
    using Process=void(*)(void*,void*,void*);
    using Available=bool(*)();
    using EnumCount=int(*)(void*);
    Ref parms{}, count{}, extent{}, arrayDim{}, enumRef{}, structRef{}, enumNames{};
    Ref byteOffset{}, byteMask{}, fieldMask{}, fieldSize{};
    Process process{};
    Available available{};
    EnumCount enumCount{};
    bool ownWrite{};

    static void thread() {
        require(mountGameThread.load()!=0 && GetCurrentThreadId()==mountGameThread.load(),
                "动画访问：不在绑定的游戏线程");
    }
    static void alive(const MountIdentity& id) {
        // 静态反射对象允许serial=0，但仍必须通过实际UObjectArray身份复核。
        require(id.address && id.index>=0 && mountAlive(id),"动画访问：对象身份失效");
    }
    template<class T> static T refValue(Ref getter,Addr object) {
        require(getter!=nullptr,"动画访问：加载器缺少反射getter");
        const auto result=reinterpret_cast<Addr>(getter(reinterpret_cast<void*>(object)));
        require(result!=0,"动画访问：getter返回空引用");
        return read<T>(result);
    }
    void bindSdk() {
        thread();
        if(process) { require(available && available(),"动画访问：ProcessEvent不可用"); return; }
        verifyVersion(); // 基础UObject/UField链也只接受本项目已核对的Steam构建。
        const auto dll=GetModuleHandleW(L"UE4SS.dll");
        require(dll!=nullptr,"动画访问：UE4SS未加载");
        auto get=[&](const char* symbol) { const auto p=GetProcAddress(dll,symbol);
            require(p!=nullptr,"动画访问：当前加载器缺少必要导出"); return p; };
        parms=reinterpret_cast<Ref>(get("?GetParmsSize@UFunction@Unreal@RC@@QEBAAEBGXZ"));
        count=reinterpret_cast<Ref>(get("?GetNumParms@UFunction@Unreal@RC@@QEBAAEBEXZ"));
        extent=reinterpret_cast<Ref>(get("?GetPropertiesSize@UStruct@Unreal@RC@@QEBAAEBHXZ"));
        arrayDim=reinterpret_cast<Ref>(get("?GetArrayDim@FProperty@Unreal@RC@@QEBAAEBHXZ"));
        enumRef=reinterpret_cast<Ref>(get("?GetEnum@FByteProperty@Unreal@RC@@QEBAAEBV?$TObjectPtr@VUEnum@Unreal@RC@@@23@XZ"));
        structRef=reinterpret_cast<Ref>(get("?GetStruct@FStructProperty@Unreal@RC@@QEBAAEBV?$TObjectPtr@VUScriptStruct@Unreal@RC@@@23@XZ"));
        // 这是本项目现用UE4SS的4.16+ ABI导出；不声称是跨加载器版本的公开接口。
        enumNames=reinterpret_cast<Ref>(get("?GetNames416@UEnum@Unreal@RC@@AEBAAEBV?$TArray@U?$TTuple@VFName@Unreal@RC@@_J@Unreal@RC@@V?$TSizedDefaultAllocator@$0CA@@23@@23@XZ"));
        enumCount=reinterpret_cast<EnumCount>(get("?NumEnums@UEnum@Unreal@RC@@QEAAHXZ"));
        byteOffset=reinterpret_cast<Ref>(get("?GetByteOffset@FBoolProperty@Unreal@RC@@QEBAAEBEXZ"));
        byteMask=reinterpret_cast<Ref>(get("?GetByteMask@FBoolProperty@Unreal@RC@@QEBAAEBEXZ"));
        fieldMask=reinterpret_cast<Ref>(get("?GetFieldMask@FBoolProperty@Unreal@RC@@QEBAAEBEXZ"));
        fieldSize=reinterpret_cast<Ref>(get("?GetFieldSize@FBoolProperty@Unreal@RC@@QEBAAEBEXZ"));
        available=reinterpret_cast<Available>(get("?IsProcessEventAvailable@UE4SSRuntime@RC@@SA_NXZ"));
        const auto candidate=reinterpret_cast<Process>(get("?ProcessEvent@UObject@Unreal@RC@@QEAAXPEAVUFunction@23@PEAX@Z"));
        require(available(),"动画访问：ProcessEvent尚未就绪");
        process=candidate; // 所有导出完成才发布已初始化标记。
    }
    static void named(Addr object,const char* name,const char* package) {
        alive(mountIdentity(object));
        require(mountName(object)==name && mountName(read<Addr>(object+0x20))==package,
                "动画访问：元数据所属包或名称不符");
    }
    static Addr find(Addr type,const char* name,bool inherited=true) {
        Addr found{};
        for(int depth=0;type && depth<32;++depth) {
            alive(mountIdentity(type));
            Addr field=read<Addr>(type+0x38);
            int n=0;
            for(;field && n<1024;++n,field=read<Addr>(field+0x28)) {
                alive(mountIdentity(field));
                if(mountName(field)==name) {
                    require(!found,"动画访问：反射成员重名"); found=field;
                }
            }
            require(!field,"动画访问：字段链过长或成环");
            if(!inherited)return found;
            type=read<Addr>(type+0x30);
        }
        require(!type,"动画访问：继承链过长或成环");
        return found;
    }
    struct Field { MountIdentity identity; int offset{},size{}; };
    Field field(Addr owner,const char* name,const char* kind,int bytes,int limit,bool inherited=false) {
        const Addr p=find(owner,name,inherited);
        require(p && mountName(read<Addr>(p+0x10))==kind && refValue<int>(arrayDim,p)==1,
                "动画访问：字段缺失、类型或数组维数不符");
        const int size=read<int>(p+0x34),offset=read<int>(p+0x44);
        require(size==bytes && offset>=0 && offset<=limit && size<=limit-offset,
                "动画访问：字段尺寸或边界不符");
        return {mountIdentity(p),offset,size};
    }
    int typeSize(Addr type) {
        alive(mountIdentity(type));
        const int bytes=refValue<int>(extent,type);
        require(bytes>0 && bytes<=0x10000,"动画访问：结构尺寸无效"); return bytes;
    }
    struct Function { MountIdentity identity; int size{}; };
    Function function(Addr object,const char* name,const char* owner,unsigned expectedCount) {
        bindSdk(); alive(mountIdentity(object));
        const Addr f=find(read<Addr>(object+0x10),name);
        require(f && mountName(read<Addr>(f+0x10))=="Function","动画访问：缺少反射函数");
        named(read<Addr>(f+0x20),owner,"/Script/Engine");
        const int size=refValue<unsigned short>(parms,f);
        require(refValue<unsigned char>(count,f)==expectedCount && size>0 && size<=256,
                "动画访问：函数参数数目或尺寸不符");
        // 此批引擎函数没有局部变量；完整遍历，不能默许额外参数。
        int children=0; Addr p=read<Addr>(f+0x38);
        for(;p && children<32;p=read<Addr>(p+0x28)) { alive(mountIdentity(p)); ++children; }
        require(!p && children==static_cast<int>(expectedCount),"动画访问：函数字段数目不符");
        return {mountIdentity(f),size};
    }
    static void separate(Field a,Field b) {
        require(a.offset+a.size<=b.offset || b.offset+b.size<=a.offset,"动画访问：参数重叠");
    }
    void invoke(const MountIdentity& object,Function f,void* parameters) {
        thread();
        {
            const MountIdentityBatch identities;
            require(identities.alive(object) && identities.alive(f.identity),"动画访问：调用前对象或函数身份失效");
        } // 调用前结束只读批次，引擎可以在ProcessEvent中改变对象表或内存分配。
        require(process && available(),"动画访问：反射调用不可用");
        process(reinterpret_cast<void*>(object.address),reinterpret_cast<void*>(f.identity.address),parameters);
        {
            const MountIdentityBatch identities; // 调用后重新查页、读取表头与当前条目。
            require(identities.alive(object) && identities.alive(f.identity),"动画访问：调用后对象或函数身份失效");
        }
    }
    unsigned enumValue(Addr enumeration,const char* enumName,const char* item) {
        named(enumeration,enumName,"/Script/Engine");
        require(mountName(read<Addr>(enumeration+0x10))=="Enum","动画访问：枚举类型不符");
        const Addr array=reinterpret_cast<Addr>(enumNames(reinterpret_cast<void*>(enumeration)));
        const Addr data=read<Addr>(array);
        const int n=read<int>(array+8),capacity=read<int>(array+12);
        require(n>0 && n<=256 && capacity>=n && capacity<=4096 && data &&
                enumCount(reinterpret_cast<void*>(enumeration))==n,"动画访问：枚举数组无效");
        checkMemory(data,static_cast<size_t>(n)*16);
        unsigned result{}; int matches=0;
        for(int i=0;i<n;++i) {
            const Addr entry=data+static_cast<Addr>(i)*16;
            const auto name=mountNameIndex(read<unsigned>(entry));
            require(read<unsigned>(entry+4)==0,"动画访问：枚举FName编号异常");
            if(name==item || name==std::string(enumName)+"::"+item) {
                const auto value=read<long long>(entry+8);
                require(value>=0 && value<=255,"动画访问：枚举值越界");
                result=static_cast<unsigned>(value); ++matches;
            }
        }
        require(matches==1,"动画访问：具名枚举项缺失或重复"); return result;
    }
    struct BoolField { Field field; unsigned byteOffset{},byteMask{},fieldMask{}; };
    BoolField boolean(Addr type,const char* name,int limit,bool inherited=false) {
        const auto f=field(type,name,"BoolProperty",1,limit,inherited);
        const Addr p=f.identity.address;
        BoolField b{f,refValue<unsigned char>(byteOffset,p),refValue<unsigned char>(byteMask,p),
                      refValue<unsigned char>(fieldMask,p)};
        require(refValue<unsigned char>(fieldSize,p)==1 && b.byteOffset==0 && b.byteMask && b.fieldMask &&
                !(b.byteMask&~b.fieldMask),"动画访问：布尔字段布局不符");
        return b;
    }
    static bool getBool(const unsigned char* data,BoolField b) {
        return (data[b.field.offset+b.byteOffset]&b.fieldMask)!=0;
    }
    static void putBool(unsigned char* data,BoolField b,bool value) {
        auto& byte=data[b.field.offset+b.byteOffset];
        byte=static_cast<unsigned char>((byte&~b.fieldMask)|(value?b.byteMask:0));
    }
    static bool writable(Addr address,size_t size) {
        if(!address || !size || address+size<address)return false;
        MEMORY_BASIC_INFORMATION m{};
        if(!VirtualQuery(reinterpret_cast<void*>(address),&m,sizeof(m)) || m.State!=MEM_COMMIT ||
           (m.Protect&(PAGE_GUARD|PAGE_NOACCESS)))return false;
        const DWORD protection=m.Protect&0xff;
        return (protection==PAGE_READWRITE || protection==PAGE_WRITECOPY ||
                protection==PAGE_EXECUTE_READWRITE || protection==PAGE_EXECUTE_WRITECOPY) &&
               address>=reinterpret_cast<Addr>(m.BaseAddress) &&
               size<=m.RegionSize-(address-reinterpret_cast<Addr>(m.BaseAddress));
    }
    bool readComponentBool(MountIdentity object,const char* property) {
        bindSdk(); alive(object);
        require(mountClassIs(object.address,"SkeletalMeshComponent","/Script/Engine"),"动画访问：不是骨架组件");
        const Addr cls=read<Addr>(object.address+0x10);
        const auto b=boolean(cls,property,typeSize(cls),true);
        named(read<Addr>(b.field.identity.address+0x20),"SkeletalMeshComponent","/Script/Engine");
        require(b.field.offset>=0x28,"动画访问：组件布尔偏移无效");
        return (read<unsigned char>(object.address+b.field.offset)&b.fieldMask)!=0;
    }
    void writeComponentBool(MountIdentity object,const char* property,bool value) {
        bindSdk(); alive(object);
        require(!ownWrite && mountClassIs(object.address,"SkeletalMeshComponent","/Script/Engine"),"动画访问：递归写入或组件不符");
        require(std::string(property)=="bPauseAnims" || std::string(property)=="bNoSkeletonUpdate",
                "动画访问：未允许的布尔字段");
        const Addr cls=read<Addr>(object.address+0x10);
        const auto b=boolean(cls,property,typeSize(cls),true);
        named(read<Addr>(b.field.identity.address+0x20),"SkeletalMeshComponent","/Script/Engine");
        const Addr address=object.address+b.field.offset;
        require(b.field.offset>=0x28 && writable(address,1),"动画访问：布尔字段不可写");
        const auto before=read<unsigned char>(address);
        const auto desired=static_cast<unsigned char>((before&~b.fieldMask)|(value?b.byteMask:0));
        alive(object); alive(b.field.identity);
        ownWrite=true;
        struct Reset { bool& flag; ~Reset(){flag=false;} } reset{ownWrite};
        write<unsigned char>(address,desired);
        require(read<unsigned char>(address)==desired,"动画访问：布尔写入回读不符");
    }
    bool tick(MountIdentity object) {
        const auto f=function(object.address,"IsComponentTickEnabled","ActorComponent",1);
        const auto b=boolean(f.identity.address,"ReturnValue",f.size);
        alignas(16) std::array<unsigned char,256> params{};
        invoke(object,f,params.data()); return getBool(params.data(),b);
    }
    // 原生wrapper完整0x2b字节和getter完整8字节均已由本轮只读采样核对。
    static constexpr const char* tickWrapperCode="40534883ec20488b42204533c94885c0498bd8410f95c14c03c84c894a20e87d95b5ff88034883c4205bc3";
    static constexpr const char* tickGetterCode="80793f000f95c0c3";
    static void tickCodeCurrent() {
        verifyVersion();
        const Addr base=reinterpret_cast<Addr>(GetModuleHandleW(nullptr));
        constexpr unsigned char wrapper[]={0x40,0x53,0x48,0x83,0xec,0x20,0x48,0x8b,0x42,0x20,0x45,0x33,0xc9,
            0x48,0x85,0xc0,0x49,0x8b,0xd8,0x41,0x0f,0x95,0xc1,0x4c,0x03,0xc8,0x4c,0x89,0x4a,0x20,
            0xe8,0x7d,0x95,0xb5,0xff,0x88,0x03,0x48,0x83,0xc4,0x20,0x5b,0xc3};
        constexpr unsigned char getter[]={0x80,0x79,0x3f,0x00,0x0f,0x95,0xc0,0xc3};
        checkMemory(base+0x1b2ab30,sizeof(wrapper));checkMemory(base+0x16840d0,sizeof(getter));
        require(std::memcmp(reinterpret_cast<void*>(base+0x1b2ab30),wrapper,sizeof(wrapper))==0 &&
                std::memcmp(reinterpret_cast<void*>(base+0x16840d0),getter,sizeof(getter))==0,
                "动画访问：Tick原生wrapper或getter代码改变");
    }
    struct TickReader {
        MountIdentity component,cls,function,resultProperty;
        std::uint64_t bindingGeneration{};
    };
    TickReader bindTickReader(MountIdentity object,std::uint64_t generation) {
        bindSdk();alive(object);require(generation!=0,"动画访问：缺少Tick绑定代次");
        tickCodeCurrent();
        const auto f=function(object.address,"IsComponentTickEnabled","ActorComponent",1);
        const auto b=boolean(f.identity.address,"ReturnValue",f.size);
        const Addr base=reinterpret_cast<Addr>(GetModuleHandleW(nullptr));
        require(read<Addr>(f.identity.address+0xb0)==base+0x1b2ab30 && b.field.offset==0 &&
                b.byteMask==1 && b.fieldMask==255,"动画访问：Tick反射入口或返回布尔布局改变");
        // 绑定时只做一次真实反射交叉核对；之后读取严格复用上述原生getter语义。
        require(tick(object)==(read<unsigned char>(object.address+0x3f)!=0),"动画访问：Tick反射与原生getter不一致");
        return {object,mountIdentity(read<Addr>(object.address+0x10)),f.identity,b.field.identity,generation};
    }
    bool tickFast(const TickReader& reader,std::uint64_t generation) {
        thread();
        require(generation && generation==reader.bindingGeneration,"动画访问：Tick绑定代次失效");
        alive(reader.component);alive(reader.cls);alive(reader.function);alive(reader.resultProperty);
        const Addr base=reinterpret_cast<Addr>(GetModuleHandleW(nullptr));
        require(read<Addr>(reader.component.address+0x10)==reader.cls.address &&
                read<Addr>(reader.function.address+0xb0)==base+0x1b2ab30,
                "动画访问：Tick对象类或原生入口改变");
        tickCodeCurrent();
        return read<unsigned char>(reader.component.address+0x3f)!=0; // cmp byte[rcx+3f],0/setne。
    }
    void setTick(MountIdentity object,bool enabled) {
        require(!ownWrite,"动画访问：递归设置Tick");
        const auto f=function(object.address,"SetComponentTickEnabled","ActorComponent",1);
        const auto b=boolean(f.identity.address,"bEnabled",f.size);
        alignas(16) std::array<unsigned char,256> params{};
        putBool(params.data(),b,enabled);
        ownWrite=true;
        struct Reset { bool& flag; ~Reset(){flag=false;} } reset{ownWrite};
        invoke(object,f,params.data());
        require(tick(object)==enabled,"动画访问：Tick设置回读不符");
    }
    struct RackState { bool noSkeletonUpdate{},pauseAnims{},tickEnabled{}; };
    RackState rackState(MountIdentity object) {
        return {readComponentBool(object,"bNoSkeletonUpdate"),readComponentBool(object,"bPauseAnims"),tick(object)};
    }
    RackState rackState(const TickReader& reader,std::uint64_t generation) {
        return {readComponentBool(reader.component,"bNoSkeletonUpdate"),readComponentBool(reader.component,"bPauseAnims"),
                tickFast(reader,generation)};
    }
    struct SocketName { unsigned index{},number{}; }; // 必须来自已校验的原生挂点映射，不按字符串猜FName。
    bool socketExists(MountIdentity object,SocketName name) {
        require(name.index && !mountNameIndex(name.index).empty(),"动画访问：挂点FName无效");
        const auto f=function(object.address,"DoesSocketExist","SceneComponent",2);
        const auto n=field(f.identity.address,"InSocketName","NameProperty",8,f.size);
        const auto b=boolean(f.identity.address,"ReturnValue",f.size);
        separate(n,b.field);
        alignas(16) std::array<unsigned char,256> params{};
        std::memcpy(params.data()+n.offset,&name,8);
        invoke(object,f,params.data()); return getBool(params.data(),b);
    }
    enum class Space { Component,World };
    using Pose=::dualsp::common_bay::native_parts::ComponentSocketPose;
    Pose socketTransform(MountIdentity object,SocketName name,Space space) {
        require(socketExists(object,name),"动画访问：原生挂点不存在");
        const auto f=function(object.address,"GetSocketTransform","SceneComponent",3);
        const auto n=field(f.identity.address,"InSocketName","NameProperty",8,f.size);
        const auto s=field(f.identity.address,"TransformSpace","ByteProperty",1,f.size);
        const auto r=field(f.identity.address,"ReturnValue","StructProperty",48,f.size);
        separate(n,s); separate(n,r); separate(s,r);
        const Addr enumeration=refValue<Addr>(enumRef,s.identity.address);
        const unsigned value=enumValue(enumeration,"ERelativeTransformSpace",space==Space::World?"RTS_World":"RTS_Component");
        const Addr transform=refValue<Addr>(structRef,r.identity.address);
        named(transform,"Transform","/Script/CoreUObject");
        require(typeSize(transform)==48,"动画访问：Transform尺寸改变");
        // 连嵌套字段也按实际元数据取位，避免把FTransform的对齐填充当Vector内容。
        const auto rotation=field(transform,"Rotation","StructProperty",16,48);
        const auto translation=field(transform,"Translation","StructProperty",12,48);
        const auto scale=field(transform,"Scale3D","StructProperty",12,48);
        separate(rotation,translation); separate(rotation,scale); separate(translation,scale);
        auto componentFields=[&](Field parent,const char* type,int width) {
            std::array<int,4> offsets{};
            const Addr structure=refValue<Addr>(structRef,parent.identity.address);
            named(structure,type,"/Script/CoreUObject");
            require(typeSize(structure)==width*4,"动画访问：向量或四元数尺寸改变");
            const char* names[]={"X","Y","Z","W"};
            for(int i=0;i<width;++i) {
                const auto part=field(structure,names[i],"FloatProperty",4,width*4);
                offsets[i]=r.offset+parent.offset+part.offset;
                for(int j=0;j<i;++j)require(offsets[j]!=offsets[i],"动画访问：向量分量重叠");
            }
            return offsets;
        };
        const auto q=componentFields(rotation,"Quat",4),p=componentFields(translation,"Vector",3),
                   z=componentFields(scale,"Vector",3);
        alignas(16) std::array<unsigned char,256> params{};
        std::memcpy(params.data()+n.offset,&name,8); params[s.offset]=static_cast<unsigned char>(value);
        invoke(object,f,params.data());
        Pose result;
        auto scalar=[&](int offset) { float v{}; std::memcpy(&v,params.data()+offset,4);
            require(std::isfinite(v),"动画访问：变换分量非有限数"); return v; };
        for(int i=0;i<3;++i) { result.position[i]=scalar(p[i]);result.scale[i]=scalar(z[i]); }
        for(int i=0;i<4;++i)result.rotation[i]=scalar(q[i]);
        return result; // 仅实际缓存变换；不生成frameToken或bonesCurrent证据。
    }
};
