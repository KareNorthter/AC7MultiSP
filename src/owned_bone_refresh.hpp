#pragma once

#include "common_bay_animation.hpp"

// 只管理反射核对后的骨架可见性更新模式，不猜字段偏移或枚举数字；宿主提供安全元数据/标量访问。
namespace dualsp::common_bay::bone_refresh {

enum class Lookup { Missing, Found, Invalid };
struct Setting {
    Identity provider;
    std::string property, enumPath, item;
    unsigned offset{}, width{}, instanceExtent{}, alwaysValue{};
    std::uint64_t metadataGeneration{};
};
inline bool valid(const Setting& s) {
    const bool old=s.property == "MeshComponentUpdateFlag" &&
                   s.enumPath == "/Script/Engine.EMeshComponentUpdateFlag";
    const bool current=s.property == "VisibilityBasedAnimTickOption" &&
                       s.enumPath == "/Script/Engine.EVisibilityBasedAnimTickOption";
    return ::dualsp::common_bay::valid(s.provider) && (old || current) && s.item == "AlwaysTickPoseAndRefreshBones" &&
           (s.width == 1 || s.width == 4) && s.offset >= 0x28 && s.offset <= s.instanceExtent &&
           s.width <= s.instanceExtent-s.offset && (s.width != 1 || s.alwaysValue <= 255) && s.metadataGeneration;
}
struct Access {
    virtual ~Access() = default;
    virtual bool onGameThread() const = 0;
    virtual bool alive(Identity) = 0;
    // 真正遍历反射父链；核对Byte/EnumProperty、底层宽度、所属Engine类、枚举路径与唯一具名枚举项。
    // 未知类型/歧义返回Invalid，不能把缺字段、未知枚举或lookup失败都当作默认0。
    virtual Lookup inspect(Identity, std::string_view property, Setting&) = 0;
    virtual bool descriptorCurrent(const Setting&) = 0;
    virtual bool read(const Setting&, unsigned&) = 0;
    virtual bool write(const Setting&, unsigned) = 0; // 按实际宽度写入并回读；禁止碰bNoSkeletonUpdate。
};

class Lease {
    Setting setting_{};
    unsigned latestNative_{};
    bool prepared_{}, owned_{};
public:
    // 全部所需provider先完成prepare，再允许任何模式/Pause/Tick修改；跨控制器字段所有权由宿主排他登记。
    bool prepare(Access& api, Identity provider) {
        if (!api.onGameThread() || prepared_ || owned_ || !api.alive(provider)) return false;
        Setting old, current;
        const auto a=api.inspect(provider,"MeshComponentUpdateFlag",old);
        const auto b=api.inspect(provider,"VisibilityBasedAnimTickOption",current);
        if (a == Lookup::Invalid || b == Lookup::Invalid || (a == Lookup::Found) == (b == Lookup::Found)) return false;
        Setting selected=a == Lookup::Found ? old : current;
        unsigned native{};
        if (!same(provider,selected.provider) || !valid(selected) || !api.descriptorCurrent(selected) ||
            !api.read(selected,native) || (selected.width == 1 && native > 255)) return false;
        setting_=selected;
        latestNative_=native;
        prepared_=true;
        return true;
    }
    bool acquireOrMaintain(Access& api) {
        if (!api.onGameThread() || !prepared_ || !api.alive(setting_.provider) || !api.descriptorCurrent(setting_)) return false;
        owned_=true; // 可能部分写入失败，恢复责任必须先保留。
        unsigned actual{};
        return api.read(setting_,actual) && (actual == setting_.alwaysValue || api.write(setting_,setting_.alwaysValue));
    }
    // 仅原生/受支持的外部设置请求进入；本模块自己的write绕过此观察，实际强制值不能覆盖原生意图。
    unsigned nativeRequest(Access& api, unsigned wanted) {
        if (!api.onGameThread() || !prepared_) return wanted;
        if (setting_.width == 1 && wanted > 255) return wanted;
        latestNative_=wanted;
        return owned_ && api.alive(setting_.provider) && api.descriptorCurrent(setting_) ? setting_.alwaysValue : wanted;
    }
    bool release(Access& api) {
        if (!api.onGameThread()) return false;
        if (!prepared_) return true;
        if (!api.alive(setting_.provider)) { *this={}; return true; }
        if (owned_ && (!api.descriptorCurrent(setting_) || !api.write(setting_,latestNative_))) return false;
        *this={};
        return true;
    }
    bool owned() const { return owned_; }
};

} // namespace dualsp::common_bay::bone_refresh
