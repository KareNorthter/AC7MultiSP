// 新静态分区的只读碰撞资格；不修改原生碰撞设置。
#pragma once

void mountRequireNoCollision(Addr component) {
    using Reflection=MountAnimationReflection;
    struct Getter {
        Reflection api;
        Reflection::Function function;
        Reflection::Field result;
        MountIdentity enumeration;
        unsigned noCollision{};
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
        cached=next;
    }
    Reflection::alive(cached.function.identity);Reflection::alive(cached.result.identity);Reflection::alive(cached.enumeration);
    require(Reflection::refValue<unsigned short>(cached.api.parms,cached.function.identity.address)==cached.function.size &&
        read<int>(cached.result.identity.address+0x44)==cached.result.offset && read<int>(cached.result.identity.address+0x34)==1,
        "静态分区：碰撞查询参数布局改变");
    alignas(16) std::array<unsigned char,256> params{};
    cached.api.invoke(identity,cached.function,params.data());
    require(params[cached.result.offset]==cached.noCollision,"静态分区：原生挂架并非NoCollision，已停止外观接管");
}
