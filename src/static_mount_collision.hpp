// 新静态分区的只读碰撞资格；不修改原生碰撞设置。
#pragma once

unsigned mountStoredCollisionMode(Addr component);
unsigned mountCollisionMode(Addr component,unsigned* noCollision=nullptr,unsigned* queryAndPhysics=nullptr) {
    using Reflection=MountAnimationReflection;
    struct Getter {
        Reflection api;
        Reflection::Function function;
        Reflection::Field result;
        MountIdentity enumeration;
        unsigned noCollision{};
        unsigned queryAndPhysics{};
    };
    static Getter cached;
    const auto identity=mountIdentity(component);
    require(mountClassIs(component,"StaticMeshComponent","/Script/Engine"),"静态分区：碰撞查询对象类型不符");
    if(!cached.function.identity.address) {
        Getter next;
        next.function=next.api.function(component,"GetCollisionEnabled","PrimitiveComponent",1);
        next.result=next.api.field(next.function.identity.address,"ReturnValue","ByteProperty",1,next.function.size);
        next.enumeration=mountIdentity(Reflection::refValue<Addr>(next.api.enumRef,next.result.identity.address));
        next.noCollision=next.api.enumValue(next.enumeration.address,"ECollisionEnabled","NoCollision");
        next.queryAndPhysics=next.api.enumValue(next.enumeration.address,"ECollisionEnabled","QueryAndPhysics");
        cached=next;
    }
    Reflection::alive(cached.function.identity);Reflection::alive(cached.result.identity);Reflection::alive(cached.enumeration);
    require(Reflection::refValue<unsigned short>(cached.api.parms,cached.function.identity.address)==cached.function.size &&
        read<int>(cached.result.identity.address+0x44)==cached.result.offset && read<int>(cached.result.identity.address+0x34)==1,
        "静态分区：碰撞查询参数布局改变");
    alignas(16) std::array<unsigned char,256> params{};
    cached.api.invoke(identity,cached.function,params.data());
    if(noCollision)*noCollision=cached.noCollision;
    if(queryAndPhysics)*queryAndPhysics=cached.queryAndPhysics;
    return params[cached.result.offset];
}
void mountRequireNoCollision(Addr component) {
    unsigned expected{};
    const auto actual=mountCollisionMode(component,&expected);
    if(actual!=expected) {
        const auto identity=mountIdentity(component);
        nativeLog(Json{{"event","MOUNT_COLLISION_MISMATCH"},{"component",component},
            {"component_name",mountName(component)},{"index",identity.index},{"serial",identity.serial},
            {"actual",actual},{"stored",mountStoredCollisionMode(component)},{"expected",expected}}.dump());
        throw std::runtime_error("静态分区：原生挂架并非NoCollision，已停止外观接管");
    }
}
