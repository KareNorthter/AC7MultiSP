#pragma once

#include "common_bay_animation.hpp"
#include <cstddef>
#include <limits>

// Steam 9855922限定的Parts访问候选；字段来自PlayerPlane原生初始化→引擎包装器→Parts调用链。
// 引擎/Parts不是UObject。此头不安装钩子或读写进程，实际访问使用宿主已有的安全读写封装。
namespace dualsp::common_bay::native_parts {

struct Access {
    virtual ~Access() = default;
    virtual std::uintptr_t moduleBase() const = 0;
    virtual bool onGameThread() const = 0;
    virtual bool ownerAlive(const Binding&) = 0; // 先核对世界、PlayerPlane、激活器等UObject，不访问engine/parts。
    virtual bool inheritsNativePlayerPlane(Identity) = 0; // 完整/Script/Nimbus.PlayerPlane父链。
    virtual bool canonicalAircraft(Identity, Aircraft) = 0; // 再核对该profile的真实原版Pawn包/父类，不接受仅传入枚举。
    virtual bool originalCodeMatches(std::uint32_t rva, std::string_view hex) = 0;
    // 已被本产品钩住的函数须核对自身记录的原始字节/跳板归属；不能接受任意第三方跳转。
    virtual bool moduleReadOnlyPointer(std::uintptr_t) = 0;
    virtual bool read(std::uintptr_t, void*, std::size_t) = 0;
    virtual bool write(std::uintptr_t, const void*, std::size_t) = 0;
    virtual bool writable(std::uintptr_t, std::size_t) = 0; // 预检完整写区，不通过则第一笔也不写。
};

template<class T> inline bool load(Access& api, std::uintptr_t address, T& result) {
    return api.read(address,&result,sizeof(T));
}
template<class T> inline bool store(Access& api, std::uintptr_t address, const T& value) {
    return api.write(address,&value,sizeof(T));
}

// 这些短签名指向已由旧采样完整区段摘要核对的原生指令；不是从F-35C机型名推导的布局。
inline bool codeQualified(Access& api) {
    return api.originalCodeMatches(0x810efa,"488d97400b0000488d8f08140000e843224c01") &&
           api.originalCodeMatches(0x1cd315a,"488b5908") &&
           api.originalCodeMatches(0x1cd3172,"48899368120000") &&
           api.originalCodeMatches(0x1cd3181,"488d8b900f0000") &&
           api.originalCodeMatches(0x1cd55e1,"488d8b900f00004533c04c8d4b600f28cee8a93d0100") &&
           api.originalCodeMatches(0x1ce94a4,"4c8d430c488bcbe8c0d8ffff") &&
           api.originalCodeMatches(0x1ce6bb4,"488d05bd853101488901") &&
           api.originalCodeMatches(0x1ce6d70,"4d85c00f84fe170000488bc455564881ecb8000000") &&
           api.originalCodeMatches(0x81e8f8,"8b874c0b0000");
}

// 只解析，尚不授予写权限。只能用于上述四种白名单机体；调用方仍需Controller的全部资源/布局预检。
inline bool resolveIdentity(Access& api, Aircraft aircraft, Binding& binding, int& nativeConfig) {
    if (!api.onGameThread() || !api.ownerAlive(binding) || !api.inheritsNativePlayerPlane(binding.plane) ||
        !api.canonicalAircraft(binding.plane,aircraft) ||
        !codeQualified(api) || (aircraft != Aircraft::Yf23 && aircraft != Aircraft::Su47 &&
                               aircraft != Aircraft::Su57TopGun && aircraft != Aircraft::F22a)) return false;
    const auto limit=std::numeric_limits<std::uintptr_t>::max();
    if (binding.plane.address > limit-0x1418 || api.moduleBase() > limit-0x2fff178) return false;
    std::uintptr_t engine{}, ownerInterface{}, engineVtable{}, partsVtable{};
    // 原生PlayerPlane用this+1408包装器，包装器+8保存实例；实例+1268回指this+b40接口。
    if (!load(api,binding.plane.address+0x1410,engine) || !engine || (engine&7u) || engine > limit-0x1300 ||
        !load(api,engine+0x1268,ownerInterface) || ownerInterface != binding.plane.address+0xb40 ||
        !load(api,engine,engineVtable) || !api.moduleReadOnlyPointer(engineVtable) ||
        !load(api,engine+0xf90,partsVtable) || partsVtable != api.moduleBase()+0x2fff178) return false;
    const auto parts=engine+0xf90;
    unsigned flags{}, options{};
    if (!load(api,parts+0x27c,flags) || ((flags>>18)&15u) != 1u ||
        !load(api,parts+0xb0,options) || (options&(1u<<20))) return false;
    int planeConfig{};
    if (!load(api,binding.plane.address+0xb4c,planeConfig) || !load(api,parts+0x178,nativeConfig) ||
        planeConfig != nativeConfig || planeConfig < 0 || planeConfig >= 3) return false;
    binding.engine=engine;
    binding.parts=parts;
    return true;
}

inline bool resolve(Access& api, Aircraft aircraft, Binding& binding, PartsSample& sample) {
    if (!resolveIdentity(api,aircraft,binding,sample.nativeConfig)) return false;
    const auto parts=binding.parts;
    if (!load(api,parts+0x58,sample.cover.request) || !load(api,parts+0x274,sample.cover.phase)) return false;
    for (unsigned i=0;i<7;++i) if (!load(api,parts+0x17c+i*4,sample.cover.progress[i])) return false;
    if (!::dualsp::common_bay::valid(sample.cover)) return false;
    return true;
}

inline bool read(Access& api, Aircraft aircraft, const Binding& expected, PartsSample& result) {
    Binding current=expected;
    return resolve(api,aircraft,current,result) && current.engine == expected.engine && current.parts == expected.parts;
}

enum class CoverWriteStatus { Applied, RejectedBeforeWrite, FailedRolledBack, FailedRollbackIncomplete, IdentityChanged };

// 与同步重建窄窗口配合。失败不能假定没写：完整写区先预检，失败后同一身份下尽力归还写前状态。
// Access::ownerAlive必须包含generation；write/read/writable只作安全内存访问，不重入原生事务。
inline CoverWriteStatus writeCover(Access& api, Aircraft aircraft, const Binding& expected, const CoverState& desired) {
    PartsSample before;
    if (!::dualsp::common_bay::valid(desired) || !read(api,aircraft,expected,before) ||
        !api.writable(expected.parts+0x58,sizeof(int)) || !api.writable(expected.parts+0x274,sizeof(int)) ||
        !api.writable(expected.parts+0x17c,7*sizeof(float))) return CoverWriteStatus::RejectedBeforeWrite;
    auto identityStillSame=[&]() {
        Binding current=expected;
        int config{};
        return resolveIdentity(api,aircraft,current,config) && current.engine == expected.engine &&
               current.parts == expected.parts && config == before.nativeConfig;
    };
    auto put=[&](std::uintptr_t address, const void* data, std::size_t size) {
        return identityStillSame() && api.write(address,data,size);
    };
    bool written=put(expected.parts+0x58,&desired.request,sizeof(int));
    if (written) written=put(expected.parts+0x274,&desired.phase,sizeof(int));
    if (written) written=put(expected.parts+0x17c,desired.progress.data(),7*sizeof(float));
    PartsSample after;
    if (written && read(api,aircraft,expected,after) && after.nativeConfig == before.nativeConfig &&
        after.cover.request == desired.request && after.cover.phase == desired.phase && after.cover.progress == desired.progress)
        return CoverWriteStatus::Applied;
    if (!identityStillSame()) return CoverWriteStatus::IdentityChanged; // 不再访问旧engine/parts。
    // 一个字段恢复失败不阻止其他字段的尽力恢复；每笔恢复前重新核对身份，不写配置编号。
    bool restored=put(expected.parts+0x58,&before.cover.request,sizeof(int));
    const bool phaseRestored=put(expected.parts+0x274,&before.cover.phase,sizeof(int));
    const bool progressRestored=put(expected.parts+0x17c,before.cover.progress.data(),7*sizeof(float));
    restored=restored && phaseRestored && progressRestored;
    if (!identityStillSame()) return CoverWriteStatus::IdentityChanged;
    if (restored && read(api,aircraft,expected,after) && after.nativeConfig == before.nativeConfig &&
        after.cover.request == before.cover.request && after.cover.phase == before.cover.phase &&
        after.cover.progress == before.cover.progress) return CoverWriteStatus::FailedRolledBack;
    return CoverWriteStatus::FailedRollbackIncomplete;
}

// 这是原生状态门槛，不能单独证明当前帧的provider骨骼/世界变换已求值，更不能证明扫掠净空。
inline bool nativeFullyOpen(const PartsSample& sample) {
    return ::dualsp::common_bay::valid(sample.cover) && sample.cover.request == 2 && sample.cover.phase == 2 &&
           sample.cover.progress[0] == 1.f;
}

struct EndPoseWitness {
    Identity provider;
    std::uint64_t bindingGeneration{}, frameToken{};
    bool sequenceEndPoseConfirmed{}, bonesCurrent{}, socketWorldCurrent{};
};

struct ComponentSocketPose {
    std::array<float,3> position{}, scale{};
    std::array<float,4> rotation{}; // x/y/z/w，组件空间；不在这里猜资源单位转换。
};
struct PoseTolerance {
    float distance{}, angleRadians{}, scaleDifference{}; // 由几何注册提供经净空核对的容差，不设经验默认值。
};

inline bool matchesComponentEndPose(const ComponentSocketPose& actual, const ComponentSocketPose& expected,
                                    PoseTolerance tolerance) {
    if (!std::isfinite(tolerance.distance) || tolerance.distance < 0 ||
        !std::isfinite(tolerance.angleRadians) || tolerance.angleRadians < 0 || tolerance.angleRadians > 3.141593f ||
        !std::isfinite(tolerance.scaleDifference) || tolerance.scaleDifference < 0) return false;
    double distance2=0, dot=0, firstNorm=0, secondNorm=0;
    for (unsigned i=0;i<3;++i) {
        if (!std::isfinite(actual.position[i]) || !std::isfinite(expected.position[i]) ||
            !std::isfinite(actual.scale[i]) || !std::isfinite(expected.scale[i]) ||
            std::abs(actual.scale[i]-expected.scale[i]) > tolerance.scaleDifference) return false;
        const double delta=double(actual.position[i])-expected.position[i];
        distance2+=delta*delta;
    }
    for (unsigned i=0;i<4;++i) {
        if (!std::isfinite(actual.rotation[i]) || !std::isfinite(expected.rotation[i])) return false;
        dot+=double(actual.rotation[i])*expected.rotation[i];
        firstNorm+=double(actual.rotation[i])*actual.rotation[i];
        secondNorm+=double(expected.rotation[i])*expected.rotation[i];
    }
    if (std::abs(firstNorm-1.0) > .001 || std::abs(secondNorm-1.0) > .001) return false;
    double cosine=std::abs(dot)/std::sqrt(firstNorm*secondNorm); // q和-q描述相同旋转。
    if (cosine > 1) cosine=1;
    return distance2 <= double(tolerance.distance)*tolerance.distance &&
           2*std::acos(cosine) <= tolerance.angleRadians;
}

// native与pose必须来自同一帧原生更新；pose端点确认由经反射核对的GetSocketTransform组件空间取位
// 与matchesComponentEndPose组成。只用于内部弹体；门盖、导轨、机体和原生发射门控不使用此规则。
inline bool mayShowInternalAmmo(const Binding& binding, Identity provider, std::uint64_t frameToken,
                               const PartsSample& native, const EndPoseWitness& pose) {
    return frameToken && pose.frameToken == frameToken && pose.bindingGeneration == binding.generation &&
           same(provider,pose.provider) && nativeFullyOpen(native) && pose.sequenceEndPoseConfirmed &&
           pose.bonesCurrent && pose.socketWorldCurrent;
}

} // namespace dualsp::common_bay::native_parts
