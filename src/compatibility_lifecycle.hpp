// 兼容能力共用的故障账本。只管理范围和诊断，不读取引擎、不决定资源资格或执行清理。
// 包含于核心私有命名空间，复用宿主已引入的标准库及nativeLog/Json。
#pragma once

struct CompatibilityIdentity {
    uintptr_t address{};
    int index{},serial{};
};
bool compatibilitySameIdentity(const CompatibilityIdentity& a,const CompatibilityIdentity& b) noexcept {
    return a.address==b.address && a.index==b.index && a.serial==b.serial;
}
template<class Identity> CompatibilityIdentity compatibilityIdentity(const Identity& id) noexcept {
    return {id.address,id.index,id.serial};
}
struct CompatibilityContext {
    CompatibilityIdentity player,activator,owner; // owner由能力定义：持续效果为Level，舱门为World。
};
struct CompatibilityFault {
    CompatibilityContext context;
    const char* stage{}; // 仅使用静态阶段代号，不能借用临时字符串。
    std::array<char,768> message{};
    unsigned repeats{};
    explicit operator bool() const noexcept {return stage!=nullptr;}
};
class CompatibilityFailures {
    // 每种能力独立实例。容量耗尽只关闭该能力，不能覆盖旧故障或扩大到别的能力。
    std::array<CompatibilityFault,64> local_{};
    CompatibilityFault global_{};
    unsigned localCount_{};
    static void remember(CompatibilityFault& fault,const CompatibilityContext& context,
                         const char* stage,const char* message) noexcept {
        if(fault) {if(fault.repeats<UINT_MAX)++fault.repeats;return;}
        fault.context=context;fault.stage=stage?stage:"unknown";fault.repeats=1;
        if(!message)return;
        size_t length=0;
        while(length+1<fault.message.size() && message[length])++length;
        // 截断诊断时不拆开UTF-8字符；原生错误必须是有效的UTF-8文本。
        if(message[length])while(length && (static_cast<unsigned char>(message[length])&0xc0)==0x80)--length;
        std::memcpy(fault.message.data(),message,length);fault.message[length]=0;
    }
public:
    bool empty() const noexcept {return !global_ && !localCount_;}
    const CompatibilityFault* global() const noexcept {return global_?&global_:nullptr;}
    const CompatibilityFault* find(const CompatibilityIdentity& player) const noexcept {
        if(global_)return &global_;
        if(!localCount_)return nullptr;
        for(const auto& fault:local_)if(fault && compatibilitySameIdentity(fault.context.player,player))return &fault;
        return nullptr;
    }
    const CompatibilityFault* find(const CompatibilityContext& context) const noexcept {
        if(const auto* fault=find(context.player))return fault;
        if(context.activator.address)for(const auto& fault:local_)if(fault &&
            compatibilitySameIdentity(fault.context.activator,context.activator))return &fault;
        return nullptr; // 新玩家也不能借用仍有故障的同一个激活器绕过保护。
    }
    void recordGlobal(const CompatibilityContext& context,const char* stage,const char* message) noexcept {
        remember(global_,context,stage,message);
    }
    void recordLocal(const CompatibilityContext& context,const char* stage,const char* message) noexcept {
        if(!context.player.address) {recordGlobal(context,stage,message);return;}
        for(auto& fault:local_)if(fault && compatibilitySameIdentity(fault.context.player,context.player)) {
            remember(fault,context,stage,message);return;
        }
        for(auto& fault:local_)if(!fault) {remember(fault,context,stage,message);++localCount_;return;}
        recordGlobal(context,"fault_capacity",message);
    }
    template<class Destroyed> void forgetDestroyedContexts(Destroyed destroyed) {
        // 调用方必须证明相关对象表身份已消失；PendingKill、失去控制或换机均不等于销毁。
        // 这里只收回故障记录容量，不承担效果、原生引用或Root的归还责任。
        for(auto& fault:local_)if(fault && destroyed(fault.context)) {fault={};--localCount_;}
    }
};

void compatibilityLogFault(const char* capability,const CompatibilityFault& fault,bool global) noexcept {
    try {
        nativeLog(Json{{"event","COMPATIBILITY_FALLBACK"},{"capability",capability},
            {"scope",global?"process":"player"},{"stage",fault.stage},{"reason",fault.message.data()},
            {"player",fault.context.player.address},{"index",fault.context.player.index},
            {"serial",fault.context.player.serial},{"activator",fault.context.activator.address},
            {"owner",fault.context.owner.address},{"repeats",fault.repeats}}.dump());
    }catch(...) { } // 故障已经锁存，诊断失败不能从引擎回调或析构再次抛出。
}
