// 所有机体共用的锁定槽重建收尾；在continuous_sp.hpp之后包含。
// 原生Init直接换掉LockonSeekers，不能只停音效后丢弃仍登记着目标的旧槽。
#pragma once

void qualifySeekerRebuild() {
    static bool qualified=false;
    if(qualified)return;
    verifyVersion();
    const Addr base=continuousBase();
    auto code=[base](size_t offset,const auto& bytes) {
        checkMemory(base+offset,sizeof(bytes));
        require(std::memcmp(reinterpret_cast<const void*>(base+offset),bytes,sizeof(bytes))==0,
            "锁定收尾：原生入口不匹配");
    };
    constexpr unsigned char reset[]={0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x74,0x24,0x10,0x57,0x48,0x83,0xec,0x40,
        0x48,0x8b,0x99,0x68,0x04,0,0};
    constexpr unsigned char prune[]={0x40,0x56,0x48,0x83,0xec,0x30,0x8b,0xb1,0xa0,0x03,0,0,0x83,0xee,0x01};
    constexpr unsigned char remove[]={0x48,0x85,0xd2,0x0f,0x84,0x85,0,0,0,0x48,0x89,0x5c,0x24,0x08};
    // 原生重建只清槽数组Num；它没有配对清除LockedTargets，这正是重复记录的来源。
    constexpr unsigned char discard[]={0x8b,0x46,0x0c,0x33,0xed,0x89,0x6e,0x08};
    code(0x4c8ee0,reset);code(0x4c3cb0,prune);code(0x4c7210,remove);code(0x4c6d33,discard);
    qualified=true;
}

void prepareSeekerRebuild(Addr plane,Addr activator) {
    qualifySeekerRebuild();
    const auto playerId=continuousIdentity(plane),activatorId=continuousIdentity(activator);
    require(read<Addr>(plane+0x11a8)==activator,"锁定收尾：玩家武器组件不匹配");
    const Addr seeker=read<Addr>(plane+0x1310);
    const auto seekerId=continuousIdentity(seeker);
    require(read<Addr>(seeker+0x378)==plane,"锁定收尾：组件不属于当前玩家");
    if(session.plane==plane && session.activator==activator && session.gameThread)
        require(session.gameThread==GetCurrentThreadId(),"锁定收尾：不在会话游戏线程");
    const auto slots=continuousArray(seeker+0x468);
    const int lockedBefore=continuousArray(seeker+0x398).count;
    // 只在重建边界检查旧槽；不增加帧钩子，不缓存跨帧发射状态。
    for(int i=0;i<slots.count;++i) {
        const Addr slot=read<Addr>(slots.data+i*sizeof(Addr));
        continuousIdentity(slot);
        require(read<Addr>(slot+0x28)==plane && read<Addr>(slot+0x30)==seeker &&
            read<unsigned char>(slot+0x40)<=6,"锁定收尾：旧槽归属或状态无效");
    }
    using Reset=void(*)(void*,float,float);
    // 逐槽原生Reset进入状态0，解除目标登记并配对停音；0参数沿用原有锁定时间。
    // 不调用会清SelectedTarget和候选表的整组件Reset，保留用户选中的目标。
    reinterpret_cast<Reset>(continuousBase()+0x4c8ee0)(reinterpret_cast<void*>(seeker),0.f,0.f);
    auto alive=[&] {
        require(continuousAlive(playerId) && continuousAlive(activatorId) && continuousAlive(seekerId) &&
            read<Addr>(plane+0x1310)==seeker && read<Addr>(plane+0x11a8)==activator,
            "锁定收尾：原生回调期间玩家已换代");
    };
    alive();
    const auto after=continuousArray(seeker+0x468);
    require(after.data==slots.data && after.count==slots.count,"锁定收尾：旧槽数组已换代");
    for(int i=0;i<after.count;++i) {
        const Addr slot=read<Addr>(after.data+i*sizeof(Addr));
        require(read<unsigned char>(slot+0x40)==0 && !read<Addr>(slot+0x38),"锁定收尾：旧槽尚未解除");
    }
    finishSeekerAudio(plane,activator); // 保留原有音效引用归零核验，兼容状态0仍有声音的旧现场。
    alive();
    // 兼容旧版本已经遗留、没有任何现存槽负责的记录。先沿原生过滤死亡/空目标，
    // 再调用原生解除登记通知；不直接清Num，不对正常多重锁定去重或压成单发。
    using Prune=void(*)(void*);
    using Remove=void(*)(void*,void*,bool);
    int orphaned=continuousArray(seeker+0x398).count;
    if(orphaned) {
        reinterpret_cast<Prune>(continuousBase()+0x4c3cb0)(reinterpret_cast<void*>(seeker));
        alive();
        auto remaining=continuousArray(seeker+0x398);
        while(remaining.count) {
            const Addr target=read<Addr>(remaining.data);
            continuousIdentity(target);
            reinterpret_cast<Remove>(continuousBase()+0x4c7210)(reinterpret_cast<void*>(seeker),reinterpret_cast<void*>(target),true);
            alive();
            const auto next=continuousArray(seeker+0x398);
            require(next.count<remaining.count,"锁定收尾：原生解除登记未减少旧记录");
            remaining=next;
        }
    }
    if(lockedBefore || orphaned)
        nativeLog(Json{{"event","SEEKER_REBUILD_RELEASED"},{"locked_before",lockedBefore},
            {"orphaned_after_slot_reset",orphaned},{"locked_after",0},{"slots",slots.count}}.dump());
}
