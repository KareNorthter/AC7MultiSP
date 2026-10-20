// 七机静态分区的碰撞保真资格：不关闭、不改写原生组件的碰撞配置。
#pragma once

struct MountRackCollisionAccess {
    MountAnimationReflection api;
    MountIdentity componentClass,bodyType;
    MountAnimationReflection::Field body,mode,profile,objectType,responses;
    MountAnimationReflection::BoolField useDefault;
};
MountRackCollisionAccess& mountRackCollisionAccess(Addr component) {
    using R=MountAnimationReflection;
    const auto cls=mountIdentity(read<Addr>(component+0x10));
    static std::map<Addr,MountRackCollisionAccess> layouts; // 仅反射元数据的弱身份，不保有组件。
    auto& cached=layouts[cls.address];
    if(!cached.componentClass.address || !mountMeshIdentityEqual(cls,cached.componentClass)) {
        MountRackCollisionAccess next;next.api.bindSdk();
        require(mountClassIs(component,"StaticMeshComponent","/Script/Engine"),"碰撞保真：组件类型不符");
        const Addr field=R::find(cls.address,"BodyInstance");
        require(field!=0,"碰撞保真：BodyInstance字段缺失");
        next.bodyType=mountIdentity(R::refValue<Addr>(next.api.structRef,field));
        R::named(next.bodyType.address,"BodyInstance","/Script/Engine");
        const int bytes=next.api.typeSize(next.bodyType.address),extent=next.api.typeSize(cls.address);
        next.body=next.api.field(cls.address,"BodyInstance","StructProperty",bytes,extent,true);
        next.mode=next.api.field(next.bodyType.address,"CollisionEnabled","ByteProperty",1,bytes);
        next.profile=next.api.field(next.bodyType.address,"CollisionProfileName","NameProperty",8,bytes);
        next.objectType=next.api.field(next.bodyType.address,"ObjectType","ByteProperty",1,bytes);
        const Addr responses=R::find(next.bodyType.address,"CollisionResponses");
        require(responses!=0,"碰撞保真：响应字段缺失");
        const int size=read<int>(responses+0x34);
        require(size>0 && size<=256,"碰撞保真：响应字段尺寸改变");
        next.responses=next.api.field(next.bodyType.address,"CollisionResponses","StructProperty",size,bytes);
        next.componentClass=cls;cached=next;
    }
    R::alive(cached.componentClass);R::alive(cached.bodyType);
    require(mountMeshIdentityEqual(cls,cached.componentClass),"碰撞保真：组件反射类型变化");
    for(const auto f:{cached.body,cached.mode,cached.profile,cached.objectType,cached.responses}) {
        R::alive(f.identity);
        require(read<int>(f.identity.address+0x44)==f.offset && read<int>(f.identity.address+0x34)==f.size,
            "碰撞保真：反射字段布局改变");
    }
    return cached;
}
unsigned mountStoredCollisionMode(Addr component) {
    const auto id=mountIdentity(component);auto& a=mountRackCollisionAccess(component);
    const auto result=read<unsigned char>(component+a.body.offset+a.mode.offset);
    require(mountAlive(id),"碰撞保真：读取期间组件换代");return result;
}
std::vector<unsigned char> mountPreservedCollisionSettings(Addr component) {
    const auto id=mountIdentity(component);auto& a=mountRackCollisionAccess(component);
    if(!a.useDefault.field.identity.address)a.useDefault=a.api.boolean(a.componentClass.address,"bUseDefaultCollision",
        a.api.typeSize(a.componentClass.address),true);
    MountAnimationReflection::alive(a.useDefault.field.identity);
    require(read<int>(a.useDefault.field.identity.address+0x44)==a.useDefault.field.offset,
        "碰撞保真：默认碰撞字段布局改变");
    require((read<unsigned char>(component+a.useDefault.field.offset)&a.useDefault.fieldMask)==0,
        "碰撞保真：原生组件要求自动覆盖碰撞配置，不能接管");
    std::vector<unsigned char> result;
    for(const auto field:{a.mode,a.profile,a.objectType,a.responses}) {
        const Addr address=component+a.body.offset+field.offset;
        checkMemory(address,static_cast<size_t>(field.size));
        const auto* bytes=reinterpret_cast<const unsigned char*>(address);
        result.insert(result.end(),bytes,bytes+field.size);
    }
    // 响应的动态数组内容也需保持，不能只比较其指针和长度。
    const auto type=MountAnimationReflection::refValue<Addr>(a.api.structRef,a.responses.identity.address);
    const Addr array=MountAnimationReflection::find(type,"ResponseArray");
    require(array && mountName(read<Addr>(array+0x10))=="ArrayProperty","碰撞保真：响应数组类型不符");
    const int offset=read<int>(array+0x44);
    require(offset>=0 && offset+16<=a.responses.size,"碰撞保真：响应数组越界");
    const Addr header=component+a.body.offset+a.responses.offset+offset,data=read<Addr>(header);
    const int count=read<int>(header+8),capacity=read<int>(header+12);
    const Addr inner=read<Addr>(array+0x70);mountIdentity(inner);
    const int stride=read<int>(inner+0x34);
    require(count>=0 && count<=capacity && capacity<=64 && stride>0 && stride<=128,"碰撞保真：响应数组范围改变");
    if(count) {
        const size_t size=static_cast<size_t>(count)*stride;checkMemory(data,size);
        const auto* bytes=reinterpret_cast<const unsigned char*>(data);result.insert(result.end(),bytes,bytes+size);
    }
    require(mountAlive(id),"碰撞保真：采样期间组件换代");return result;
}
bool mountRequireRackCollision(Addr component,Addr original,Addr replacement) {
    using R=MountAnimationReflection;
    unsigned noCollision{},queryAndPhysics{};
    const auto effective=mountCollisionMode(component,&noCollision,&queryAndPhysics),stored=mountStoredCollisionMode(component);
    if(effective==noCollision && stored==noCollision)return false;
    require(stored==queryAndPhysics && (effective==noCollision || effective==queryAndPhysics),
        "碰撞保真：原生碰撞模式不在已核验范围");
    auto& access=mountRackCollisionAccess(component);
    require(mountNameIndex(read<unsigned>(component+access.body.offset+access.profile.offset))=="BlockAllDynamic" &&
        read<unsigned>(component+access.body.offset+access.profile.offset+4)==0,
        "碰撞保真：原生挂架配置不是已核验的BlockAllDynamic");
    mountPreservedCollisionSettings(component);
    const auto originalId=mountIdentity(original),replacementId=mountIdentity(replacement);
    const Json* proof=nullptr;
    for(const auto& entry:mountExpansionData().at("collision_preservation"))if(entry.at("asset").get<std::string>()==mountName(replacement)) {
        require(proof==nullptr,"碰撞保真：资源依据重复");proof=&entry;
    }
    require(proof!=nullptr,"碰撞保真：此资源未登记完整碰撞保持依据");
    require(mountName(original)==proof->at("source").get<std::string>() &&
        mountName(read<Addr>(original+0x20))==proof->at("source_package").get<std::string>() &&
        mountName(read<Addr>(replacement+0x20))=="/Game/AC7MultiSP/Research/"+proof->at("asset").get<std::string>(),
        "碰撞保真：原生或派生资源名称、包改变");
    require(mountClassIs(original,"StaticMesh","/Script/Engine") && mountClassIs(replacement,"StaticMesh","/Script/Engine"),
        "碰撞保真：网格类型不符");
    const auto bodyOf=[&](Addr mesh) {
        const Addr body=read<Addr>(mesh+mountProperty(mesh,"BodySetup","ObjectProperty",8));
        const auto id=mountIdentity(body);
        require(mountClassIs(body,"BodySetup","/Script/Engine") && read<Addr>(body+0x20)==mesh &&
            mountName(body)==proof->at("body_name").get<std::string>() &&
            read<unsigned>(body+0x1c)==proof->at("body_number").get<unsigned>(),"碰撞保真：BodySetup身份或父链改变");
        return id;
    };
    const auto originalBody=bodyOf(original),replacementBody=bodyOf(replacement);
    const auto& layout=mountExpansionData().at("collision_body_layout");
    const int size=layout.at("size").get<int>(),offset=layout.at("guid_offset").get<int>();
    // 本build原生BodySetup布局已通过暂停快照核对，直接读16字节；不搜索对象内存。
    require(size==0x3a0 && offset==0x2e0 &&
        access.api.typeSize(read<Addr>(originalBody.address+0x10))==size &&
        access.api.typeSize(read<Addr>(replacementBody.address+0x10))==size,"碰撞保真：BodySetup尺寸改变");
    std::array<unsigned char,16> guid{};
    require(proof->at("body_guid").size()==guid.size(),"碰撞保真：GUID依据不完整");
    for(size_t i=0;i<guid.size();++i)guid[i]=proof->at("body_guid").at(i).get<unsigned char>();
    R::alive(originalBody);R::alive(replacementBody);
    require(read<std::array<unsigned char,16>>(originalBody.address+offset)==guid &&
        read<std::array<unsigned char,16>>(replacementBody.address+offset)==guid,
        "碰撞保真：原生与派生碰撞GUID不一致");
    require(mountAlive(originalId) && mountAlive(replacementId),"碰撞保真：核验期间资源换代");
    static std::set<std::string> logged; // 19项纯资源名，不保有机体或世界。
    if(logged.insert(proof->at("asset").get<std::string>()).second)nativeLog(Json{{"event","MOUNT_COLLISION_PRESERVED"},{"component_name",mountName(component)},
        {"source",mountName(original)},{"replacement",mountName(replacement)},{"stored",stored},
        {"guid_offset",offset}}.dump());
    return true;
}
