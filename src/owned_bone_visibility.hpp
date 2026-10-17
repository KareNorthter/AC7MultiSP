#pragma once

#include "common_bay_animation.hpp"
#include <utility>
#include <vector>

// 原生骨支路的可见性所有权候选；仅供研究方案接入，不安装钩子或注册新机体。
// HideBoneByName会影响隐藏支路的缩放及其子Socket，发射点必须属于未隐藏的独立支路。
namespace dualsp::bone_visibility {

struct Rule { std::string bone; bool hidden{}; };
struct NativeState { Rule desired; bool latestNative{}, owned{}; };
struct Access {
    virtual ~Access() = default;
    virtual bool onGameThread() const = 0;
    virtual bool alive(common_bay::Identity, std::uint64_t generation) = 0;
    // 沿实际反射父链核对三函数完整签名及EPhysBodyOp::PBO_None；核对原生请求观察与自身重入隔离。
    virtual bool nativeApiAndRequestObservationQualified() = 0;
    virtual bool directChildOfBody(common_bay::Identity, std::string_view bone) = 0;
    virtual bool readHidden(common_bay::Identity, std::string_view bone, bool&) = 0;
    // 经引擎Hide/UnHide接口执行，不碰物理终止、不猜BoneVisibilityStates偏移，调用后回读。
    virtual bool writeHidden(common_bay::Identity, std::string_view bone, bool hidden) = 0;
};

class Lease {
    common_bay::Identity component_{};
    std::uint64_t generation_{};
    std::vector<NativeState> states_;
    bool prepared_{};
public:
    bool prepare(Access& api, common_bay::Identity component, std::uint64_t generation,
                 const std::vector<Rule>& rules) {
        if (!api.onGameThread() || prepared_ || !generation || rules.empty() ||
            !common_bay::valid(component) || !api.alive(component,generation) ||
            !api.nativeApiAndRequestObservationQualified()) return false;
        // 只允许Body直接子支路；根本身隐藏时，IsBoneHidden不能区分子支路显式状态，拒绝取得所有权。
        bool parentHidden{};
        if (!api.readHidden(component,"Body",parentHidden) || parentHidden) return false;
        std::vector<NativeState> pending;
        for (const auto& rule: rules) {
            if (rule.bone.empty() || rule.bone == "Body" || !api.directChildOfBody(component,rule.bone)) return false;
            for (const auto& state: pending) if (state.desired.bone == rule.bone) return false;
            bool hidden{};
            if (!api.readHidden(component,rule.bone,hidden)) return false;
            pending.push_back({rule,hidden,false});
        }
        component_=component; generation_=generation; states_=std::move(pending); prepared_=true;
        return true;
    }
    bool maintain(Access& api) {
        if (!api.onGameThread() || !prepared_ || !api.alive(component_,generation_) ||
            !api.nativeApiAndRequestObservationQualified()) return false;
        bool parentHidden{};
        if (!api.readHidden(component_,"Body",parentHidden) || parentHidden) return false;
        for (auto& state: states_) {
            if (!api.alive(component_,generation_)) return false;
            state.owned=true; // 先登记恢复责任，接口失败也可能已完成部分操作。
            bool actual{};
            if (!api.readHidden(component_,state.desired.bone,actual) ||
                (actual != state.desired.hidden && !api.writeHidden(component_,state.desired.bone,state.desired.hidden))) return false;
        }
        return true;
    }
    // 只接真正原生请求；本模块write使用重入标记绕过。不能从强制状态倒推最新原生意图。
    bool nativeRequest(Access& api, common_bay::Identity component, std::string_view bone, bool wanted) {
        if (!api.onGameThread() || !prepared_ || !common_bay::same(component,component_) ||
            !api.alive(component_,generation_)) return wanted;
        for (auto& state: states_) if (state.desired.bone == bone) {
            state.latestNative=wanted;
            return state.owned ? state.desired.hidden : wanted;
        }
        return wanted;
    }
    bool release(Access& api) {
        if (!api.onGameThread()) return false;
        if (!prepared_) return true;
        if (!api.alive(component_,generation_)) { *this={}; return true; }
        bool complete=true;
        for (auto& state: states_) if (state.owned) {
            if (!api.alive(component_,generation_)) { *this={}; return true; }
            if (api.writeHidden(component_,state.desired.bone,state.latestNative)) state.owned=false;
            else complete=false; // 一支归还失败不阻断其他支路；保留失败项，禁止新绑定。
        }
        if (complete) *this={};
        return complete;
    }
};

// 座舱朝前定义左右；正式接入还须核对原生组件变换/镜像，不能只看骨名数字。
inline std::vector<Rule> cfa44AdmmLeft() {
    return {{"SWPCover_01",false},{"SWPCover_04",true},{"SWPCover_07",true}};
}
inline std::vector<Rule> cfa44EmlRight() {
    return {{"SWPCover_01",true},{"SWPCover_10",false}};
}

} // namespace dualsp::bone_visibility
