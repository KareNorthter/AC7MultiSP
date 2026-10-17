// 在mount_animation_product_access.hpp之后包含。只观察真实原生发布，不主动求值动画或补帧。
#pragma once

struct MountAnimationPublicationTracker {
    using Binding=::dualsp::common_bay::Binding;
    using Identity=::dualsp::common_bay::Identity;
    using PartsSample=::dualsp::common_bay::PartsSample;
    using Aircraft=::dualsp::common_bay::Aircraft;
    using Pose=::dualsp::common_bay::native_parts::ComponentSocketPose;
    using Tolerance=::dualsp::common_bay::native_parts::PoseTolerance;
    using Witness=::dualsp::common_bay::native_parts::EndPoseWitness;
    using SocketName=MountAnimationReflection::SocketName;
    using Original=void(*)(void*);
    static constexpr unsigned publishRva=0x16e5de0;
    static constexpr const char* publishCode=
        "48895c240848896c24104889742418574883ec2033db488bf13899280800007430889928080000389950070000741b"
        "8b9148070000b8010000002bc289914c070000898148070000eb0748899948070000e87ac1ffff488bbec808000048"
        "6386d0080000488d0cc7488be9482bef4883c50748c1ed03483bf9480f47eb4885ed7414488b0fe89613f0ff48ffc3"
        "488d7f08483bdd75ec488b8ec00800004885c97405e87913f0ff488b5c2430488b6c2438488b7424404883c4205fc3";
    struct Endpoint {
        Binding binding;
        Aircraft aircraft{};
        Identity provider;
        MountIdentity mesh,skeleton,animationClass,animationInstance;
        SocketName socket;
        Pose expectedComponent;
        Tolerance tolerance;
    };
    struct Result {
        Witness witness;
        PartsSample native;
        Pose component,world;
        int publishedBuffer{-1};
        std::uint64_t nativeFrame{};
    };
    bool observing{};

    static bool resourcesCurrent(MountAnimationProductAccess& api,const Endpoint& e) {
        auto& r=api.reflection;
        r.bindSdk();
        for(auto id:{e.mesh,e.skeleton,e.animationClass,e.animationInstance})MountAnimationReflection::alive(id);
        const Addr cls=::read<Addr>(e.provider.address+0x10);
        const int extent=r.typeSize(cls);
        const auto mesh=r.field(cls,"SkeletalMesh","ObjectProperty",8,extent,true);
        const auto animClass=r.field(cls,"AnimClass","ClassProperty",8,extent,true);
        const auto instance=r.field(cls,"AnimScriptInstance","ObjectProperty",8,extent,true);
        const Addr meshClass=::read<Addr>(e.mesh.address+0x10);
        const auto skeleton=r.field(meshClass,"Skeleton","ObjectProperty",8,r.typeSize(meshClass),true);
        return ::read<Addr>(e.provider.address+mesh.offset)==e.mesh.address &&
               ::read<Addr>(e.provider.address+animClass.offset)==e.animationClass.address &&
               ::read<Addr>(e.provider.address+instance.offset)==e.animationInstance.address &&
               ::read<Addr>(e.mesh.address+skeleton.offset)==e.skeleton.address;
    }

    // 新代码已证实request1会递增进度，达到端点时phase变2但request仍可保持1。
    // request3是正在收拢请求，即使尚未更新旧phase也不能放行显示。
    static bool nativeOpen(const PartsSample& s) {
        return ::dualsp::common_bay::valid(s.cover) && (s.cover.request==1 || s.cover.request==2) &&
               s.cover.phase==2 && s.cover.progress[0]==1.f;
    }
    static bool sameParts(const PartsSample& a,const PartsSample& b) {
        return a.nativeConfig==b.nativeConfig && a.cover.request==b.cover.request && a.cover.phase==b.cover.phase &&
               a.cover.progress==b.cover.progress;
    }
    static bool unitRotation(const Pose& pose) {
        double norm=0;
        for(float v:pose.rotation) { if(!std::isfinite(v))return false;norm+=double(v)*v; }
        return std::abs(norm-1.)<=.001;
    }
    static bool supportedBinding(const Endpoint& e) {
        if(!::dualsp::common_bay::valid(e.binding))return false;
        const unsigned skeletalMask=e.aircraft==Aircraft::Su57TopGun?3u:7u;
        if(e.aircraft!=Aircraft::F22a && e.aircraft!=Aircraft::Su47 && e.aircraft!=Aircraft::Su57TopGun)return false;
        for(unsigned i=0;i<3;++i)if((skeletalMask&(1u<<i)) && ::dualsp::common_bay::same(e.binding.racks[i],e.provider))return true;
        return false;
    }
    struct Snapshot {
        std::uint64_t frame{};
        unsigned animationFrame{};
        unsigned char pending{},doubleBuffered{};
        int writeIndex{},readIndex{};
        std::array<unsigned char,48> componentToWorld{};
    };
    static Snapshot snapshot(MountAnimationProductAccess& api,Identity provider) {
        MountAnimationReflection::thread();
        require(api.alive(provider),"动画发布：组件身份失效");
        const Addr p=provider.address;
        Snapshot s;
        s.frame=::read<std::uint64_t>(api.moduleBase()+0x3ac5520);
        s.animationFrame=::read<unsigned>(p+0xd24);
        s.pending=::read<unsigned char>(p+0x828);
        s.doubleBuffered=::read<unsigned char>(p+0x750);
        s.writeIndex=::read<int>(p+0x748);s.readIndex=::read<int>(p+0x74c);
        // 原生取位先尝试MasterPose，再走自己的+74c；本版本仅接受没有MasterPose的原版独立挂架。
        auto& r=api.reflection;
        r.bindSdk();
        const Addr cls=::read<Addr>(p+0x10);
        const auto master=r.field(cls,"MasterPoseComponent","WeakObjectProperty",8,r.typeSize(cls),true);
        require(master.offset==0x720 && ::read<int>(p+master.offset+4)==0 &&
                (::read<int>(p+master.offset)==0 || ::read<int>(p+master.offset)==-1),
                "动画发布：MasterPose挂架不能使用独立缓冲见证");
        checkMemory(p+0x180,48);std::memcpy(s.componentToWorld.data(),reinterpret_cast<void*>(p+0x180),48);
        require(s.pending<=1 && s.doubleBuffered<=1 && s.writeIndex>=0 && s.writeIndex<=1 &&
                s.readIndex>=0 && s.readIndex<=1,"动画发布：双缓冲字段异常");
        return s;
    }
    static bool codeQualified(MountAnimationProductAccess& api,Identity provider,Original original) {
        bool receipt=false;
        for(const auto& r:api.hookReceipts)if(r.rva==publishRva && r.installedQualified &&
            r.trampoline==reinterpret_cast<Addr>(original))receipt=true;
        if(!receipt || !api.originalCodeMatches(publishRva,publishCode) || !api.alive(provider))return false;
        const Addr vtable=::read<Addr>(provider.address);
        if(!api.moduleReadOnlyPointer(vtable) || ::read<Addr>(vtable+0x890)!=api.moduleBase()+publishRva ||
           ::read<Addr>(vtable+0x3d0)!=api.moduleBase()+0x16eafd0)return false;
        // 原生当帧标记的写端和比较端；不把本地计数器当游戏帧。
        return api.originalCodeMatches(0x16fb6d6,"8b05449e3c028986240d0000") &&
               api.originalCodeMatches(0x16fa02b,"8b81240d0000483905e8b43c02") &&
               api.originalCodeMatches(0x16e7f75,"4863974c070000") &&
               api.originalCodeMatches(0x16eb137,"e894c8ffff");
    }
    // 产品的publishRva钩子直接调用此方法；原函数恰好调用一次，包括所有拒绝/异常路径。
    // 成功结果仅供当前钩子尾部同步消费，不缓存成后续帧的放行许可。
    bool aroundNativePublication(MountAnimationProductAccess& api,void* component,Original original,
                                 const Endpoint& e,Result& output) {
        require(original!=nullptr,"动画发布：缺少原生跳板");
        output={};
        bool admitted=false;
        Snapshot before;
        PartsSample prior;
        if(!observing && api.onGameThread())try {
            admitted=supportedBinding(e) && reinterpret_cast<Addr>(component)==e.provider.address && api.ownerAlive(e.binding) &&
                     resourcesCurrent(api,e) && codeQualified(api,e.provider,original);
            if(admitted) {
                before=snapshot(api,e.provider);
                const auto state=api.reflection.rackState(MountAnimationProductAccess::productIdentity(e.provider));
                ::dualsp::common_bay::bone_refresh::Setting mode;
                unsigned actualMode{};
                admitted=before.pending==1 && before.frame!=~std::uint64_t{0} &&
                         before.animationFrame==static_cast<unsigned>(before.frame) &&
                         !state.noSkeletonUpdate && !state.pauseAnims && state.tickEnabled &&
                         api.inspect(e.provider,"MeshComponentUpdateFlag",mode)==::dualsp::common_bay::bone_refresh::Lookup::Found &&
                         api.read(mode,actualMode) && actualMode==mode.alwaysValue &&
                         ::dualsp::common_bay::native_parts::read(api,e.aircraft,e.binding,prior) && nativeOpen(prior);
            }
        } catch(...) { admitted=false; }
        if(observing) { original(component);return false; }
        observing=true;
        struct Reset { bool& flag;~Reset(){flag=false;} } reset{observing};
        original(component); // 先完成原生双缓冲发布和通知；不主动调用骨骼刷新函数。
        if(!admitted)return false;
        try {
            if(!api.ownerAlive(e.binding) || !resourcesCurrent(api,e))return false;
            const auto published=snapshot(api,e.provider);
            if(published.frame!=before.frame || published.animationFrame!=before.animationFrame || published.pending ||
               published.doubleBuffered!=before.doubleBuffered ||
               published.readIndex!=(before.doubleBuffered?before.writeIndex:0) ||
               published.writeIndex!=(before.doubleBuffered?1-before.writeIndex:0))return false;
            Result result;
            const auto identity=MountAnimationProductAccess::productIdentity(e.provider);
            result.component=api.reflection.socketTransform(identity,e.socket,MountAnimationReflection::Space::Component);
            result.world=api.reflection.socketTransform(identity,e.socket,MountAnimationReflection::Space::World);
            const auto after=snapshot(api,e.provider);
            if(after.frame!=published.frame || after.animationFrame!=published.animationFrame || after.pending ||
               after.writeIndex!=published.writeIndex || after.readIndex!=published.readIndex ||
               after.componentToWorld!=published.componentToWorld || !api.ownerAlive(e.binding) || !resourcesCurrent(api,e) ||
               !::dualsp::common_bay::native_parts::read(api,e.aircraft,e.binding,result.native) ||
               !sameParts(prior,result.native) || !nativeOpen(result.native) || !unitRotation(result.world) ||
               !::dualsp::common_bay::native_parts::matchesComponentEndPose(result.component,e.expectedComponent,e.tolerance))return false;
            result.nativeFrame=published.frame;result.publishedBuffer=published.readIndex;
            result.witness={e.provider,e.binding.generation,published.frame+1,true,true,true};
            output=result;return true;
        } catch(...) { return false; }
    }
};
