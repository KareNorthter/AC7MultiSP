#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <string_view>

// 独立的共舱控制模块。只限定YF-23/Su-47/Su-57 Top Gun/F-22A；宿主核对身份与完整布局后显式接入。
// 没有进程偏移、钩子、全局状态或自动注册，不替代整机七组合布局准入。
namespace dualsp::common_bay {

enum class Aircraft { Yf23, Su47, Su57TopGun, F22a };
enum class Error {
    None, WrongThread, InvalidBinding, UnsupportedResources, IncompleteLayout,
    InvalidSelection, InvalidState, Busy, NativeAccess, RestorePending, StaleTransaction,
    NativeTransition // 正常旁路：非同舱切换走原生流程，不调用restoreSwitch。
};

struct Identity {
    std::uintptr_t address{};
    int index{-1};
    int serial{};
};
inline bool same(Identity a, Identity b) {
    return a.address == b.address && a.index == b.index && a.serial == b.serial;
}
inline bool valid(Identity id) { return id.address && id.index >= 0 && id.serial > 0; }

// engine/parts为不透明句柄；只由Host定位和访问。generation在世界/组件重建或一次控制权重绑时递增。
struct Binding {
    Identity world, plane, activator, root, weaponBase;
    std::array<Identity, 3> racks{};
    std::uintptr_t engine{}, parts{};
    std::uint64_t generation{};
    unsigned scene{}; // 由Host区分任务/机库/补给，禁止跨场景继承。
};
inline bool same(const Binding& a, const Binding& b) {
    if (!same(a.world,b.world) || !same(a.plane,b.plane) || !same(a.activator,b.activator) ||
        !same(a.root,b.root) || !same(a.weaponBase,b.weaponBase) || a.engine != b.engine ||
        a.parts != b.parts || a.generation != b.generation || a.scene != b.scene) return false;
    for (unsigned i=0;i<3;++i) if (!same(a.racks[i],b.racks[i])) return false;
    return true;
}
inline bool valid(const Binding& b) {
    if (!valid(b.world) || !valid(b.plane) || !valid(b.activator) || !valid(b.root) ||
        !valid(b.weaponBase) || !b.engine || !b.parts || !b.generation || !b.scene) return false;
    for (unsigned i=0;i<3;++i) {
        if (!valid(b.racks[i])) return false;
        for (unsigned j=0;j<i;++j) if (same(b.racks[i],b.racks[j])) return false;
    }
    return true;
}

struct CoverState {
    int request{}, phase{}; // 原生四态0/1/2/3，不由本模块另造动画时钟。
    std::array<float, 7> progress{}; // 公共进度及六个分通道，只有Host定义它们的实际存储位置。
};
inline bool valid(const CoverState& state) {
    if (state.request < 0 || state.request > 3 || state.phase < 0 || state.phase > 3) return false;
    for (float value: state.progress) if (!std::isfinite(value) || value < -1.f || value > 1.f) return false;
    return true;
}
struct PartsSample { CoverState cover; int nativeConfig{-1}; };

struct RackRequest { bool paused{}, tick{}, noSkeletonUpdate{}; };
inline bool same(RackRequest a, RackRequest b) {
    return a.paused == b.paused && a.tick == b.tick && a.noSkeletonUpdate == b.noSkeletonUpdate;
}

struct ResourceDescription {
    std::string pawnPackage; // 沿真实Pawn继承链找到的原版规范包；不能用当前涂装类名猜测。
    std::array<std::string, 3> meshes, skeletons, animationClasses; // 拥有返回字符串，避免宿主临时字符串悬空。
    unsigned skeletalMask{}, staticMask{}; // 实际组件父链分类，接受引擎组件派生类；TG静态第三组不能读骨架字段。
    bool originalParentChains{};        // SWP→WeaponMeshBase→Root，Body另行保留。
    bool bodyAndRacksUseOnlyPublicSpwc{}; // 实际绑定的原生骨架/AnimClass均已核对，不只读机型白名单。
    bool nativeStateAccessQualified{};   // 已为此实例定位原生包装器/Parts，并核对代码与读取结果。
    bool allSevenLayoutQualified{};      // 包含单一门体/公共支架方案；动画模块不替几何研究作此保证。
    unsigned planEquippedMask{}, providerMask{}, presentationMask{};
    bool hiddenProviderPoseRefreshQualified{}; // 所需provider已准备原生可用/具名反射模式租约，取得后才允许开Tick。
};

inline Error qualify(Aircraft aircraft, const ResourceDescription& d) {
    constexpr std::array<std::string_view,3> yfMeshes={"yf23_SWP1","yf23_SWP2","yf23_SWP3"};
    constexpr std::array<std::string_view,3> suMeshes={"su47_SWP1","su47_SWP2","su47_SWP3"};
    constexpr std::array<std::string_view,3> yfSkels={"yf23_SWP1_Skeleton","yf23_SWP2_Skeleton","yf23_SWP3_Skeleton"};
    constexpr std::array<std::string_view,3> suSkels={"su47_SWP1_Skeleton","su47_SWP2_Skeleton","su47_SWP3_Skeleton"};
    constexpr std::array<std::string_view,3> yfAnims={"yf23_SWP1_AnimBP_C","yf23_SWP2_AnimBP_C","yf23_SWP3_AnimBP_C"};
    constexpr std::array<std::string_view,3> suAnims={"su47_SWP1_AnimBP_C","su47_SWP2_AnimBP_C","su47_SWP3_AnimBP_C"};
    constexpr std::array<std::string_view,3> tgMeshes={"su57tg_SWP1","su57tg_SWP2","su57tg_SWP3_SM"};
    constexpr std::array<std::string_view,3> tgSkels={"su57tg_SWP1_Skeleton","su57tg_SWP2_Skeleton",""};
    constexpr std::array<std::string_view,3> tgAnims={"su57tg_SWP1_AnimBP_C","su57tg_SWP2_AnimBP_C",""};
    constexpr std::array<std::string_view,3> f22Meshes={"f22a_SWP1","f22a_SWP2","f22a_SWP3"};
    constexpr std::array<std::string_view,3> f22Skels={"f22a_SWP1_Skeleton","f22a_SWP2_Skeleton","f22a_SWP3_Skeleton"};
    constexpr std::array<std::string_view,3> f22Anims={"f22a_SWP1_AnimBP_C","f22a_SWP2_AnimBP_C","f22a_SWP3_AnimBP_C"};
    if (aircraft != Aircraft::Yf23 && aircraft != Aircraft::Su47 && aircraft != Aircraft::Su57TopGun && aircraft != Aircraft::F22a)
        return Error::UnsupportedResources;
    const bool yf=aircraft == Aircraft::Yf23;
    const bool tg=aircraft == Aircraft::Su57TopGun;
    const bool f22=aircraft == Aircraft::F22a;
    if (d.pawnPackage != (f22 ? "/Game/Blueprint/Player/Pawn/AcePlayerPawn_f22a" :
                         tg ? "/Game/Blueprint/Player/Pawn/AcePlayerPawn_su57tg" :
                         yf ? "/Game/Blueprint/Player/Pawn/AcePlayerPawn_yf23" :
                              "/Game/Blueprint/Player/Pawn/AcePlayerPawn_su47")) return Error::UnsupportedResources;
    for (unsigned i=0;i<3;++i) {
        if (std::string_view(d.meshes[i]) != (f22?f22Meshes:tg?tgMeshes:yf?yfMeshes:suMeshes)[i] ||
            std::string_view(d.skeletons[i]) != (f22?f22Skels:tg?tgSkels:yf?yfSkels:suSkels)[i] ||
            std::string_view(d.animationClasses[i]) != (f22?f22Anims:tg?tgAnims:yf?yfAnims:suAnims)[i]) return Error::UnsupportedResources;
    }
    if (d.skeletalMask != (tg?3u:7u) || d.staticMask != (tg?4u:0u) || !d.originalParentChains ||
        !d.bodyAndRacksUseOnlyPublicSpwc) return Error::UnsupportedResources;
    if (!d.nativeStateAccessQualified) return Error::InvalidBinding;
    return d.allSevenLayoutQualified ? Error::None : Error::IncompleteLayout;
}

// 全部接口只在游戏线程调用。Host须捕获访问异常并返回false；所有访问前核对UObject身份，死亡对象只丢弃。
// describe须核对完整包路径、Socket白名单/原生池对应、AnimClass和Skeleton；短名仅为再次比对。
struct Host {
    virtual ~Host() = default;
    virtual bool onGameThread() const = 0;
    virtual bool bindingAlive(const Binding&) = 0;
    virtual bool rackAlive(Identity) = 0;
    virtual bool describe(const Binding&, ResourceDescription&) = 0;
    virtual bool readParts(const Binding&, PartsSample&) = 0;
    virtual bool writeCover(const Binding&, const CoverState&) = 0; // 只写请求、phase、七进度；绝不写选择编号。
    virtual bool readRack(Identity, RackRequest&) = 0;
    virtual bool applyRack(Identity, RackRequest) = 0; // 按真实元数据分别归还Pause/NoSkeletonUpdate/Tick。
    virtual bool acquirePoseRefresh(Identity) = 0; // 全预检已prepare；只对required骨架取得/维持刷新模式租约。
    virtual bool releasePoseRefresh(Identity) = 0; // 恢复最新原生模式；死亡对象仅丢弃，失败保留待重试。
};

struct Lease {
    Identity rack;
    RackRequest latestNative;
    bool owned{};
};
struct Snapshot {
    Binding binding;
    CoverState cover;
    std::uint64_t transaction{};
    int targetConfig{-1};
    bool valid{};
};

class Controller {
    Binding binding_{};
    std::array<Lease,3> leases_{};
    unsigned equipped_{};
    unsigned animatedMask_{}, snapshotDomainMask_{};
    std::uint64_t pendingTransaction_{}, lastTransaction_{};
    bool active_{};
    bool hasLeases() const {
        for (const auto& lease: leases_) if (lease.owned) return true;
        return false;
    }
    static bool validMask(unsigned mask) { return mask && !(mask & ~7u); }
    bool selected(int config) const { return config >= 0 && config < 3 && (equipped_ & (1u << config)); }
    // 接管写入共用保护。扫描全部所需架子，不能遇到第一个Pause就漏记其他架子的暂停意图。
    // 只把实际true记作新原生请求；强制后的false不能反推原生意图。
    bool controlWritesAllowed(Host& host) {
        bool allowed=true;
        for (auto& lease: leases_) if (lease.rack.address) {
            RackRequest actual;
            if (!host.rackAlive(lease.rack) || !host.readRack(lease.rack,actual)) { allowed=false; continue; }
            if (actual.paused) { lease.latestNative.paused=true; allowed=false; }
        }
        return allowed;
    }
    Error stopControl(Host& host, bool duringBind=false) {
        // 已进入归还重试时不再把本模块上次恢复的true误当新原生请求。
        if (active_ || duringBind) controlWritesAllowed(host);
        return release(host);
    }

public:
    // 所有资源与基线请求先读完，才允许第一笔控制写入；不改Body、父链、Mesh或AnimClass。
    Error bind(Host& host, Aircraft aircraft, const Binding& binding, unsigned equipped, unsigned requiredRackMask) {
        if (!host.onGameThread()) return Error::WrongThread;
        if (active_ || hasLeases()) return Error::Busy;
        if (!valid(binding) || !host.bindingAlive(binding)) return Error::InvalidBinding;
        if (!validMask(equipped) || !validMask(requiredRackMask)) return Error::InvalidSelection;
        ResourceDescription description;
        if (!host.describe(binding, description)) return Error::NativeAccess;
        const auto error=qualify(aircraft,description);
        if (error != Error::None) return error;
        const unsigned animatedMask=aircraft == Aircraft::Su57TopGun ? 3u : 7u;
        // 武器配置与空间provider分开。未选择第三种武器，也可以借SWP3的骨骼Socket。
        if (description.planEquippedMask != equipped ||
            ((description.providerMask | description.presentationMask) & ~7u) ||
            requiredRackMask != (description.providerMask | description.presentationMask)) return Error::IncompleteLayout;
        if ((requiredRackMask & animatedMask) && !description.hiddenProviderPoseRefreshQualified)
            return Error::UnsupportedResources;
        PartsSample initial;
        if (!host.readParts(binding,initial)) return Error::NativeAccess;
        if (!valid(initial.cover) || initial.nativeConfig < 0 || initial.nativeConfig >= 3 ||
            !(equipped & (1u << initial.nativeConfig))) return Error::InvalidState;
        std::array<Lease,3> pending{};
        for (unsigned i=0;i<3;++i) if ((requiredRackMask & animatedMask) & (1u << i)) {
            if (!host.rackAlive(binding.racks[i]) || !host.readRack(binding.racks[i],pending[i].latestNative))
                return Error::NativeAccess;
            pending[i].rack=binding.racks[i];
        }
        binding_=binding;
        equipped_=equipped;
        animatedMask_=animatedMask;
        snapshotDomainMask_=animatedMask;
        leases_=pending;
        for (unsigned i=0;i<3;++i) if ((requiredRackMask & animatedMask_) & (1u << i)) {
            // 先登记恢复所有权，再调用可能只完成部分操作的原生接口。
            leases_[i].owned=true;
            if (!host.bindingAlive(binding_) || !host.rackAlive(leases_[i].rack) ||
                !controlWritesAllowed(host) || !host.acquirePoseRefresh(leases_[i].rack) ||
                !controlWritesAllowed(host) || !host.applyRack(leases_[i].rack,{false,true})) {
                stopControl(host,true);
                return Error::NativeAccess;
            }
        }
        active_=true;
        return Error::None;
    }

    // 原生重建的批量观察只记明确改过的字段；不调用Host读写，不维持、不归还、不过滤。
    // 让全部挂架先完成记账，随后统一maintain/release，避免首个Pause提前归还污染后续采样。
    bool rememberNativeRequest(Host& host, Identity rack, RackRequest wanted, unsigned changedFields) {
        if (!host.onGameThread() || !changedFields || (changedFields & ~7u)) return false;
        for (auto& lease: leases_) if (lease.owned && same(lease.rack,rack)) {
            if (changedFields & 1u) lease.latestNative.paused=wanted.paused;
            if (changedFields & 2u) lease.latestNative.tick=wanted.tick;
            if (changedFields & 4u) lease.latestNative.noSkeletonUpdate=wanted.noSkeletonUpdate;
            return true;
        }
        return false;
    }

    // 放在原生请求的入口过滤；仅真正原生调用进入此函数，本模块自己的applyRack必须用重入标记绕过。
    // 过滤/应用必须同一游戏线程同步完成，不能靠每100ms巡检补救已经错过的动画帧。
    // changedFields位0=Pause，位1=Tick，位2=NoSkeletonUpdate；未改字段沿用已记原生请求。
    // 原生初始化直接写NoSkeletonUpdate须在完成条件赋值后另行报告位2。
    RackRequest nativeRequest(Host& host, Identity rack, RackRequest wanted, unsigned changedFields=7u) {
        if (!host.onGameThread()) return wanted;
        if (!changedFields || (changedFields & ~7u)) return wanted;
        for (auto& lease: leases_) if (lease.owned && same(lease.rack,rack)) {
            const bool allowed=!active_ || controlWritesAllowed(host);
            if (changedFields & 1u) lease.latestNative.paused=wanted.paused;
            if (changedFields & 2u) lease.latestNative.tick=wanted.tick;
            if (changedFields & 4u) lease.latestNative.noSkeletonUpdate=wanted.noSkeletonUpdate;
            if (active_) {
                if (((changedFields & 1u) && wanted.paused) || !allowed || !host.bindingAlive(binding_) || !host.rackAlive(rack)) {
                    // 先采实际Pause、再记本次具名请求。本次Pause=false也须优先于调用前的旧true。
                    release(host);
                    return wanted; // 原生Pause请求不再被过滤成false；归还失败仍保留lease供重试。
                }
                return {false,true};
            }
            break;
        }
        return wanted;
    }

    // 兵装重建后立即重新保证非当前挂架可更新；周期巡检只作兜底，不改变原生动画求值时序。
    Error maintain(Host& host) {
        if (!host.onGameThread()) return Error::WrongThread;
        if (!active_) return hasLeases() ? Error::RestorePending : Error::InvalidBinding;
        if (!host.bindingAlive(binding_)) { stopControl(host); return Error::InvalidBinding; }
        if (!controlWritesAllowed(host)) { stopControl(host); return Error::NativeAccess; }
        for (auto& lease: leases_) if (lease.owned) {
            RackRequest actual;
            if (!host.rackAlive(lease.rack) || !host.acquirePoseRefresh(lease.rack) || !host.readRack(lease.rack,actual) ||
                (!same(actual,{false,true}) && (!controlWritesAllowed(host) || !host.applyRack(lease.rack,{false,true})))) {
                stopControl(host);
                return Error::NativeAccess;
            }
        }
        return Error::None;
    }

    // 仅同一组合内真正的SP→SP同步重建；MSL普通切换、补给、检查点和调试重建不得调用。
    Error captureSwitch(Host& host, const Binding& current, int fromConfig, int toConfig,
                        std::uint64_t transaction, Snapshot& result) {
        result={};
        if (!host.onGameThread()) return Error::WrongThread;
        if (!active_ || !same(current,binding_) || !host.bindingAlive(binding_)) return Error::InvalidBinding;
        if (!transaction || fromConfig == toConfig || !selected(fromConfig) || !selected(toConfig))
            return Error::InvalidSelection;
        // TG的LACM是静态外置组：仅0/1互切继承同舱状态；出入配置2不继承旧开门请求。
        if (!(snapshotDomainMask_ & (1u << fromConfig)) || !(snapshotDomainMask_ & (1u << toConfig)))
            return Error::NativeTransition;
        if (pendingTransaction_ || transaction <= lastTransaction_) return Error::StaleTransaction;
        PartsSample sample;
        if (!host.readParts(binding_,sample)) return Error::NativeAccess;
        if (sample.nativeConfig != fromConfig || !valid(sample.cover)) return Error::InvalidState;
        result={binding_,sample.cover,transaction,toConfig,true};
        pendingTransaction_=transaction;
        return Error::None;
    }

    // 同步InitializeWeaponSystem之后、keepArmed之前调用。快照一次使用；不写当前配置编号。
    Error restoreSwitch(Host& host, const Binding& current, std::uint64_t transaction, Snapshot& saved) {
        if (!host.onGameThread()) return Error::WrongThread;
        const Snapshot copy=saved;
        saved.valid=false;
        if (!copy.valid || !transaction || copy.transaction != transaction || pendingTransaction_ != transaction)
            return Error::StaleTransaction;
        pendingTransaction_=0;
        lastTransaction_=transaction;
        if (!active_ || !same(current,binding_) || !same(copy.binding,binding_) ||
            !host.bindingAlive(binding_)) { stopControl(host); return Error::InvalidBinding; }
        if (!controlWritesAllowed(host)) { stopControl(host); return Error::NativeAccess; }
        PartsSample after;
        if (!host.readParts(binding_,after)) { stopControl(host); return Error::NativeAccess; }
        if (after.nativeConfig != copy.targetConfig || !valid(copy.cover) || !valid(after.cover)) {
            stopControl(host);
            return Error::InvalidState;
        }
        if (!controlWritesAllowed(host) || !host.writeCover(binding_,copy.cover)) { stopControl(host); return Error::NativeAccess; }
        // 回读确认本模块没误改选择编号；异常由宿主停止本绑定并独立回退视觉，不回滚弹药事务。
        if (!host.readParts(binding_,after) || after.nativeConfig != copy.targetConfig ||
            after.cover.request != copy.cover.request || after.cover.phase != copy.cover.phase ||
            after.cover.progress != copy.cover.progress) { stopControl(host); return Error::NativeAccess; }
        return maintain(host);
    }

    // 不用旧engine/parts访问做恢复；按组件各自身份归还最新原生请求，一个失败不妨碍其他组件。
    // 生存组件恢复失败时保留lease，宿主稍后重试；新绑定在lease清空前会拒绝。
    Error release(Host& host) {
        if (!host.onGameThread()) return Error::WrongThread;
        active_=false; // 归还只应用latestNative；不走接管保护或stopControl，避免递归归还。
        pendingTransaction_=0;
        for (auto& lease: leases_) if (lease.owned) {
            if (!host.rackAlive(lease.rack)) { host.releasePoseRefresh(lease.rack); lease={}; continue; }
            if (host.applyRack(lease.rack,lease.latestNative) && host.releasePoseRefresh(lease.rack)) lease={};
        }
        if (hasLeases()) return Error::RestorePending;
        binding_={};
        equipped_=0;
        animatedMask_=snapshotDomainMask_=0;
        lastTransaction_=0;
        return Error::None;
    }

    bool active() const { return active_; }
    bool restoring() const { return !active_ && hasLeases(); }
};

} // namespace dualsp::common_bay
