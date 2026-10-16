// 使用合成内存检查当前捕获和恢复逻辑。
// 不访问 AC7 进程，不调用原生游戏函数，不修改可执行代码。
#include "../src/weapon_bridge.cpp"
#include <cassert>
#include <iostream>

int main() {
    std::vector<unsigned char> activator(0x300),owner(0x200),state(0x200),slots(4*0x60);
    const Addr act=reinterpret_cast<Addr>(activator.data());
    const Addr own=reinterpret_cast<Addr>(owner.data());
    const Addr st=reinterpret_cast<Addr>(state.data());
    const Addr pool=reinterpret_cast<Addr>(slots.data());
    const Addr group=st+0x110;
    write<Addr>(act+0x260,own);write<Addr>(own+0xf0,st);
    write<Addr>(group,0x1234);write<Addr>(group+0x58,pool);
    write<int>(group+0x60,4);write<int>(group+0x64,4);
    write<int>(group+8,0);write<int>(group+12,0);
    write<int>(group+0x10,0);write<int>(group+0x14,2);
    for(int i=0;i<4;++i){write<float>(pool+i*0x60+0x48,10);write<float>(pool+i*0x60+0x4c,float(i+1));}
    auto empty=capture(act,true,100);
    for(int i=0;i<4;++i)write<float>(pool+i*0x60+0x4c,0);
    restore(act,true,empty,105);
    assert(read<int>(group+8)==0 && read<int>(group+12)==0);
    assert(read<int>(group+0x10)==0 && read<int>(group+0x14)==0);
    const float expectedEmpty[]={3,4,1,2};
    for(int i=0;i<4;++i)assert(read<float>(pool+i*0x60+0x4c)==expectedEmpty[i]);
    auto cooling=empty;
    const int ammo=6,loaded=2;
    std::memcpy(cooling.scalars.data(),&ammo,4);
    std::memcpy(cooling.scalars.data()+8,&loaded,4);
    for(int i=0;i<4;++i)write<float>(pool+i*0x60+0x4c,0);
    restore(act,true,cooling,105);
    const float expectedCooling[]={8,9,1,2};
    assert(read<int>(group+8)==6);
    for(int i=0;i<4;++i)assert(read<float>(pool+i*0x60+0x4c)==expectedCooling[i]);
    write<float>(pool+0x48,11);
    try{restore(act,true,cooling,105);assert(false);}catch(const std::runtime_error&){}
    write<float>(pool+0x48,10);
    try{restore(act,true,cooling,99);assert(false);}catch(const std::runtime_error&){}
#ifdef DUALSP_SU30SM_MOUNT_PROTOTYPE
    // 挂载边界只操作合成内存；不调用引擎或验证真实动画。
    Command command;command.op="setup";
    Session noSelection;noSelection.active=100; // setup 必须短路，不读取不存在的当前槽。
    fixedMount.pendingReadyMask=7;
    assert(!fixedMountEnabled && !fixedMountFailed); // 核心默认关闭，Lua 显式启用前不接管挂载。
    const auto disabledMount=fixedMountBeforeRebuild(command,0,noSelection);
    assert(!disabledMount.cover && !fixedMount.rebuilding && fixedMount.pendingReadyMask==7);
    MountRebuildSnapshot disabledRestore;disabledRestore.cover=1; // 禁用时不能访问此无效地址。
    fixedMountAfterRebuild(disabledRestore);
    bindF35Cover(1); // 禁用时不读取机体类型或动画地址。
    fixedMountCommit(command);
    fixedMount.bound=true;fixedMount.plane.address=1; // 会话不匹配也必须先走禁用清理。
    fixedMountUpdate(1,2,0); // 禁用时不核对玩家会话，且清理待接管状态。
    assert(!fixedMount.bound && !fixedMount.rebuilding && fixedMount.pendingReadyMask==0 && !fixedMountFailed);
    fixedMountEnabled=true;fixedMount.pendingReadyMask=7; // 旧合成挂载用例显式启用。
    const auto emptyMount=fixedMountBeforeRebuild(command,0,noSelection);
    assert(!emptyMount.cover && fixedMount.rebuilding && fixedMount.pendingReadyMask==0);
    fixedMountAfterRebuild(emptyMount);
    std::vector<unsigned char> coverMemory(0x300);
    MountRebuildSnapshot mount;
    mount.cover=reinterpret_cast<Addr>(coverMemory.data());mount.request=2;mount.state=3;
    for(int i=0;i<7;++i)mount.progress[i]=float(i)/10;
    fixedMountAfterRebuild(mount);
    assert(read<int>(mount.cover+0x58)==2 && read<int>(mount.cover+0x274)==3);
    for(int i=0;i<7;++i)assert(read<float>(mount.cover+0x17c+i*4)==mount.progress[i]);
    fixedMountRecordReload(false,0,10,9,10);assert(fixedMount.pendingReadyMask==0);
    fixedMountRecordReload(true,1,9,9,10);assert(fixedMount.pendingReadyMask==0);
    fixedMountRecordReload(true,2,10,9,10);assert(fixedMount.pendingReadyMask==4);
    fixedMountRecordReload(true,3,10,10,10);assert(fixedMount.pendingReadyMask==4);
    // 独立预览布局必须使用传入机型/组合，不能借用当前任务的映射和库存。
    session.plane=0x1234;session.activator=act;session.active=1;session.count=3;
    session.configs={2,0,1};session.maximum={29,41,17};session.ratio=.67;
    session.special={empty,cooling,empty};
    const Session beforePreview=session;
    fixedMount.aircraft=MountAircraft::F15e;fixedMount.kindsMask=5;
    fixedMount.order={5,4,3,2,1,0,6,7,8,9,10,11};
    fixedMount.readyMask=0x55;fixedMount.rackMask=5;
    const auto taskOrder=fixedMount.order;
    struct PreviewCase { MountAircraft aircraft; unsigned kinds,racks,ready,oneRound; };
    // 预期值按已登记物理位置列出，备用弹显示与可发射窗口分别核对。
    const PreviewCase previews[]={
        {MountAircraft::Su30sm,1,1,0x3f,0x1},
        {MountAircraft::Su30sm,2,4,0x3,0x1},
        {MountAircraft::Su30sm,4,2,0x3,0x1},
        {MountAircraft::Su30sm,3,4,0x3f,0x11},
        {MountAircraft::Su30sm,5,4,0x3f,0x5},
        {MountAircraft::Su30sm,6,4,0x3f,0x5},
        {MountAircraft::Su30sm,7,4,0x3f,0x15},
        {MountAircraft::F35c,1,1,0x3,0x1},
        {MountAircraft::F35c,2,4,0x3fc,0x4},
        {MountAircraft::F35c,4,2,0x3c000,0x4000},
        {MountAircraft::F35c,3,5,0x3ff,0x5},
        {MountAircraft::F35c,5,3,0xc0003,0x40001},
        {MountAircraft::F35c,6,6,0x3c3fc,0x4004},
        {MountAircraft::F35c,7,7,0xc03ff,0x40005},
        {MountAircraft::F15e,1,1,0x3f,0x1},
        {MountAircraft::F15e,2,2,0x3ffc0,0x40},
        {MountAircraft::F15e,4,4,0x0,0x0},
        {MountAircraft::F15e,3,2,0x3ffc3,0x41},
        {MountAircraft::F15e,5,5,0x3f,0x1},
        {MountAircraft::F15e,6,6,0x3ffc0,0x40},
        {MountAircraft::F15e,7,6,0x3ffc3,0x41},
    };
    for(const auto& expected:previews) {
        FixedMountState preview;
        preview.aircraft=expected.aircraft;preview.kindsMask=expected.kinds;
        preview.logicalCounts=mountSpecification(expected.aircraft).logicalCounts;
        std::array<int,3> kinds{-1,-1,-1},ammo{};int count=0;
        for(int kind=0;kind<3;++kind)if(expected.kinds&(1u<<kind)) {
            kinds[count]=kind;ammo[count]=99;++count;
        }
        assert(mountRackMask(expected.aircraft,expected.kinds)==expected.racks);
        assert(mountPreviewReadyMask(preview,kinds,ammo,count)==expected.ready);
        // 选择次序不改变物理布局；草稿库存为零时不生成导弹/炸弹模型。
        std::reverse(kinds.begin(),kinds.begin()+count);
        assert(mountPreviewReadyMask(preview,kinds,ammo,count)==expected.ready);
        ammo.fill(0);assert(mountPreviewReadyMask(preview,kinds,ammo,count)==0);
        ammo.fill(1);assert(mountPreviewReadyMask(preview,kinds,ammo,count)==expected.oneRound);
    }
    // 三机型的单/多挂反复切换不能继承上一组合的位置限制。
    struct SelectionCase { MountAircraft aircraft; unsigned single,multiple; int kind,ordinal,singlePosition,multiplePosition; };
    const SelectionCase selections[]={
        {MountAircraft::Su30sm,1,7,0,0,0,2},
        {MountAircraft::F35c,4,5,2,0,14,18},
        {MountAircraft::F15e,1,3,0,5,5,1},
    };
    for(const auto& expected:selections) {
        FixedMountState preview;preview.aircraft=expected.aircraft;
        preview.logicalCounts=mountSpecification(expected.aircraft).logicalCounts;
        preview.kindsMask=expected.multiple;
        assert(mountPhysical(preview,expected.kind,expected.ordinal)==expected.multiplePosition);
        preview.kindsMask=expected.single;
        assert(mountPhysical(preview,expected.kind,expected.ordinal)==expected.singlePosition);
        preview.kindsMask=expected.multiple;
        assert(mountPhysical(preview,expected.kind,expected.ordinal)==expected.multiplePosition);
        try{mountPhysical(preview,expected.kind,preview.logicalCounts[expected.kind]);assert(false);}catch(const std::runtime_error&){}
    }
    // 换涂装的部分组件复用：只继承同一序列号的原生请求，新祖先/动画组件不可继承旧隐藏状态。
    FixedMountState priorVisuals,nextVisuals;
    priorVisuals.root={100,1,10};priorVisuals.weaponBase={101,2,10};
    priorVisuals.rootVisible=false;priorVisuals.rootHidden=true;priorVisuals.ancestorVisible=false;
    priorVisuals.visuals={{{200,3,10},0,0,false,true},{{201,4,10},1,-1,true,false}};
    priorVisuals.animatedRack={201,4,10};priorVisuals.nativeRackPaused=true;priorVisuals.nativeRackTick=false;
    nextVisuals.root=priorVisuals.root;nextVisuals.weaponBase={101,2,11}; // 地址复用也不是同一组件。
    nextVisuals.visuals={{{200,3,10},0,0,true,false},{{201,4,11},1,-1,false,true}};
    mountPreviewInheritNative(nextVisuals,priorVisuals);
    assert(!nextVisuals.rootVisible && nextVisuals.rootHidden && nextVisuals.ancestorVisible);
    assert(!nextVisuals.visuals[0].nativeVisible && nextVisuals.visuals[0].nativeHidden);
    assert(!nextVisuals.visuals[1].nativeVisible && nextVisuals.visuals[1].nativeHidden && !nextVisuals.animatedRack.address);
    nextVisuals.visuals[1].identity=priorVisuals.animatedRack;
    mountPreviewInheritNative(nextVisuals,priorVisuals);
    assert(nextVisuals.animatedRack.address==201 && nextVisuals.nativeRackPaused && !nextVisuals.nativeRackTick);
    // 空池等待与失败归还不得继续解引用尚未就绪或失效的展示对象。
    mountPreview.active=true;mountPreview.pending=true;mountPreview.layout.plane.address=1;
    mountPreviewApply();assert(mountPreview.pending && !mountPreview.failed);
    mountPreview.pending=false;mountPreview.failed=true;
    mountPreviewApply();assert(mountPreview.failed && !mountPreview.restorePending);
    assert(!mountPreviewMatchesManager(1));mountPreview={};
    // 无真实组件的预览仍须遵守owner归属；失败保留原生回退，直到正确owner归还。
    mountPreview.active=true;mountPreview.owner.address=0x5555;mountPreview.layout.plane.address=0x6666;
    assert(mountPreviewRelease(0x7777,false) && mountPreview.active && mountPreview.owner.address==0x5555);
    mountPreviewFail("合成预览失败");
    assert(mountPreview.active && mountPreview.failed && mountPreview.nativeOnly && !mountPreview.warning.empty());
    const auto firstWarning=mountPreview.warning;
    mountPreviewFail("后续错误不能重新接管");assert(mountPreview.warning==firstWarning);
    assert(mountPreviewRelease(0x5555,false) && !mountPreview.active && !mountPreview.failed && !mountPreview.nativeOnly);
    assert(mountPreview.layout.visuals.empty());
    // 关闭模型或运行许可必须先清理空上下文，不能读取这些刻意无效的引擎地址。
    for(int disabled=0;disabled<2;++disabled) {
        mountPreview.active=true;mountPreview.owner.address=0x5555;mountPreview.layout.plane.address=0x6666;
        fixedMountEnabled=disabled!=0;runtimeEnabled=disabled==0;
        mountPreviewUpdate(1,2,3,1,{0,1,2},{1,1,1});
        assert(!mountPreview.active && !mountPreview.failed && mountPreview.layout.visuals.empty());
    }
    runtimeEnabled=false;
    assert(fixedMount.order==taskOrder && fixedMount.readyMask==0x55 && fixedMount.rackMask==5);
    assert(session.plane==beforePreview.plane && session.activator==beforePreview.activator);
    assert(session.active==beforePreview.active && session.count==beforePreview.count);
    assert(session.configs==beforePreview.configs && session.maximum==beforePreview.maximum && session.ratio==beforePreview.ratio);
    for(size_t weapon=0;weapon<session.special.size();++weapon) {
        const auto& actual=session.special[weapon];const auto& prior=beforePreview.special[weapon];
        assert(actual.scalars==prior.scalars && actual.capturedTime==prior.capturedTime && actual.slots.size()==prior.slots.size());
        for(size_t slot=0;slot<actual.slots.size();++slot) {
            assert(actual.slots[slot].duration==prior.slots[slot].duration && actual.slots[slot].elapsed==prior.slots[slot].elapsed);
            assert(actual.slots[slot].mountOrdinal==prior.slots[slot].mountOrdinal);
        }
    }
    fixedMount={};fixedMountEnabled=false;
#endif
    std::cout<<"weapon_group: cache restore, reload, mount layout and preview boundary checks passed\n";
}
