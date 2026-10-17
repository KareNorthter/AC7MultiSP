// 使用合成内存检查当前捕获和恢复逻辑。
// 不访问 AC7 进程，不调用原生游戏函数，不修改可执行代码。
#include "../src/weapon_bridge.cpp"
#include <cassert>
#include <iostream>

// 合成数组专用：强制搬迁，检验扩容后是否仍引用旧地址；不接入游戏。
static size_t referenceBytes=0;
static bool failReferenceAllocation=false;
static void* moveReferenceArray(void* old,size_t bytes,unsigned alignment) {
    assert(alignment==0 && bytes>=referenceBytes);
    if(failReferenceAllocation)return nullptr;
    auto next=HeapAlloc(GetProcessHeap(),0,bytes);assert(next);
    if(old){std::memcpy(next,old,referenceBytes);assert(HeapFree(GetProcessHeap(),0,old));}
    referenceBytes=bytes;return next;
}

int main() {
    // 持续对象引用数组只移除指定对象，容量不变，不误删普通导弹。
    std::array<Addr,4> effects{10,20,30,20};
    struct EffectArray { Addr data;int count,capacity; } effectArray{reinterpret_cast<Addr>(effects.data()),4,4};
    continuousRemove(reinterpret_cast<Addr>(&effectArray),20);
    assert(effectArray.count==2&&effectArray.capacity==4&&effects[0]==10&&effects[1]==30&&effects[2]==0&&effects[3]==0);
    continuousRemove(reinterpret_cast<Addr>(&effectArray),99);assert(effectArray.count==2);
    // 保留Num与原元素；成功搬迁公布新data，失败保持原头，已有容量不取得分配器。
    auto arrayMemory=HeapAlloc(GetProcessHeap(),0,2*sizeof(Addr));assert(arrayMemory);
    EffectArray reserveArray{reinterpret_cast<Addr>(arrayMemory),2,2};
    write<Addr>(reserveArray.data,41);write<Addr>(reserveArray.data+8,42);referenceBytes=16;
    int allocatorCalls=0;
    auto allocator=[&](){++allocatorCalls;return &moveReferenceArray;};
    const Addr reserveHeader=reinterpret_cast<Addr>(&reserveArray),oldReferenceData=reserveArray.data;
    continuousReserveUsing(reserveHeader,5,"test",allocator);
    assert(allocatorCalls==1 && reserveArray.data!=oldReferenceData && reserveArray.count==2 && reserveArray.capacity>=5);
    assert(read<Addr>(reserveArray.data)==41 && read<Addr>(reserveArray.data+8)==42);
    continuousReserveUsing(reserveHeader,5,"test",allocator);assert(allocatorCalls==1);
    const auto beforeFailure=reserveArray;failReferenceAllocation=true;
    try{continuousReserveUsing(reserveHeader,20,"test",allocator);assert(false);}catch(const std::runtime_error&){}
    assert(reserveArray.data==beforeFailure.data && reserveArray.count==2 && reserveArray.capacity==beforeFailure.capacity);
    assert(read<Addr>(reserveArray.data)==41 && read<Addr>(reserveArray.data+8)==42);
    try{continuousReserveUsing(reserveHeader,65537,"test",allocator);assert(false);}catch(const std::runtime_error&){}
    assert(allocatorCalls==2);assert(HeapFree(GetProcessHeap(),0,reinterpret_cast<void*>(reserveArray.data)));
    // UAV可能仅在Active中，普通导弹可能覆盖Spawned；两表并集不能遗漏或重复保有。
    std::array<Addr,5> activeEffects{40,20,40,0,10};
    std::array<Addr,3> spawnedEffects{10,30,20};
    ContinuousArray activeArray{0,reinterpret_cast<Addr>(activeEffects.data()),5,5};
    ContinuousArray spawnedArray{0,reinterpret_cast<Addr>(spawnedEffects.data()),3,3};
    assert((continuousCandidates(activeArray,spawnedArray)==std::vector<Addr>{40,20,10,30}));
    assert((activeEffects==std::array<Addr,5>{40,20,40,0,10}));
    // SP回收记录移走，MSL记录原字节保留；未知组先拒绝，原队列不能被部分写入。
    std::array<std::array<unsigned char,16>,4> delayed{};
    for(int i=0;i<4;++i){delayed[i].fill(static_cast<unsigned char>(20+i));delayed[i][0]=i==0?0:i==2?1:2;}
    ContinuousArray delayedArray{0,reinterpret_cast<Addr>(delayed.data()),4,4};
    const auto originalDelayed=delayed;
    const auto kept=ravenKeepMainQueue(delayedArray);
    assert(kept.size()==2 && kept[0]==originalDelayed[0] && kept[1]==originalDelayed[2] && delayed==originalDelayed);
    delayed[3][0]=3;
    try{ravenKeepMainQueue(delayedArray);assert(false);}catch(const std::runtime_error&){}
    assert(delayed[0]==originalDelayed[0] && delayed[1]==originalDelayed[1] && delayed[3][0]==3);
    effectArray.count=5;
    try{continuousArray(reinterpret_cast<Addr>(&effectArray));assert(false);}catch(const std::runtime_error&){}
    // 非换槽事务不得读取不存在的游戏机体或保有对象。
    { Command noSwitch;noSwitch.op="setup";noSwitch.plane=1;ContinuousRebuild skipped(noSwitch);skipped.restoreReferences(); }
    std::vector<unsigned char> activator(0x300),owner(0x200),state(0x200),slots(4*0x60);
    const Addr act=reinterpret_cast<Addr>(activator.data());
    assert(weaponSwitchReady(act));
    write<unsigned char>(act+0x2a0,1);assert(!weaponSwitchReady(act));write<unsigned char>(act+0x2a0,0);
    write<unsigned char>(act+0x2c0,1);write<int>(act+0x2e0,2);assert(!weaponSwitchReady(act));
    write<int>(act+0x2e0,1);assert(weaponSwitchReady(act));write<unsigned char>(act+0x2c0,0);
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
    // 机外保留路线不能误入旧共舱发射映射；舱内原生旁路与固定吊舱分别核对。
    assert(mountNativeOnly(MountAircraft::Su47) && mountNativeOnly(MountAircraft::Su57tg));
    assert(!mountNativeOnly(MountAircraft::F35c));
    FixedMountState exterior;exterior.aircraft=MountAircraft::Pkfa;exterior.kindsMask=7;
    MountVisual pod;pod.pool=2;pod.index=-1;
    assert(mountDesired(exterior,pod,true) && !mountDesired(exterior,pod,false));
    assert(!mountNeedsMappedTransform(exterior,0) && !mountNeedsMappedTransform(exterior,2));
    exterior.kindsMask=3;assert(!mountDesired(exterior,pod,true));
    exterior.aircraft=MountAircraft::F22a;exterior.kindsMask=7;exterior.exteriorConfig=0;
    MountVisual lateral;lateral.pool=2;lateral.index=8;lateral.physicalBit=uint64_t{1}<<8;
    exterior.readyMask=lateral.physicalBit;
    assert(mountDesired(exterior,lateral,true,0)); // 不依赖旧共舱/精确姿态门槛。
    exterior.readyMask=0;assert(!mountDesired(exterior,lateral,true));
    assert(!mountNeedsMappedTransform(exterior,0) && !mountNeedsMappedTransform(exterior,2));
    assert(mountExteriorPreviewMask(exterior,{0,2,1},{20,6,12},3)==63);
    assert(mountExteriorPreviewMask(exterior,{0,1,2},{20,12,0},3)==0);
    // 反向表保留首个同址映射和高位；未映射模型不凭旧位置显示。
    FixedMountState reverseMap;reverseMap.aircraft=MountAircraft::Su30sm;reverseMap.physicalCount=41;
    reverseMap.physical[0]=123;reverseMap.physical[1]=123;reverseMap.physical[40]=456;
    for(Addr address:{Addr{123},Addr{456},Addr{789}}){MountVisual visual;visual.pool=0;visual.index=0;visual.identity.address=address;reverseMap.visuals.push_back(visual);}
    mountBindVisualPositions(reverseMap);
    assert(reverseMap.visuals[0].physicalBit==1 && reverseMap.visuals[1].physicalBit==(uint64_t{1}<<40) && reverseMap.visuals[2].physicalBit==0);
    reverseMap.readyMask=uint64_t{1}<<40;
    assert(!mountDesired(reverseMap,reverseMap.visuals[0],true) && mountDesired(reverseMap,reverseMap.visuals[1],true));
    assert(!mountDesired(reverseMap,reverseMap.visuals[1],false) && !mountDesired(reverseMap,reverseMap.visuals[2],true));
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
        {MountAircraft::Su30sm,4,2,0xf,0x1},
        {MountAircraft::Su30sm,3,4,0x3f,0x11},
        {MountAircraft::Su30sm,5,4,0x3f,0x5},
        {MountAircraft::Su30sm,6,4,0x3f,0x5},
        {MountAircraft::Su30sm,7,4,0x3f,0x15},
        {MountAircraft::F35c,1,1,0x3,0x1},
        {MountAircraft::F35c,2,4,0x3ffc,0x4},
        {MountAircraft::F35c,4,2,0x3c000,0x4000},
        {MountAircraft::F35c,3,5,0x3fff,0x5},
        {MountAircraft::F35c,5,3,0xc0003,0x40001},
        {MountAircraft::F35c,6,6,0x3fffc,0x4004},
        {MountAircraft::F35c,7,7,0xc3fff,0x40005},
        {MountAircraft::F15e,1,1,0x3f,0x1},
        {MountAircraft::F15e,2,2,0x3ffc0,0x40},
        {MountAircraft::F15e,4,4,0x0,0x0},
        {MountAircraft::F15e,3,3,0x3f03f,0x1001},
        {MountAircraft::F15e,5,5,0x3f,0x1},
        {MountAircraft::F15e,6,6,0x3ffc0,0x40},
        {MountAircraft::F15e,7,7,0x3f03f,0x1001},
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
        {MountAircraft::F15e,2,3,1,0,6,12},
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
    // 十机方案完整性、不同武器共用位置隔离，以及超过32位的火箭巢显隐。
    assert(mountExpansionData().at("planes").size()==10 && mountExpansionData().at("assets").size()==16);
    int multiCases=0;
    for(int aircraft=static_cast<int>(MountAircraft::F02a);aircraft<=static_cast<int>(MountAircraft::Typn);++aircraft) {
        const auto type=static_cast<MountAircraft>(aircraft);
        const auto& definition=mountExpansionDefinition(type);
        assert(definition.at("plans").size()==8);
        for(unsigned selection=1;selection<=7;++selection) {
            const auto& plan=mountExpansionPlan(type,selection);
            assert(!plan.is_null());
            if(selection==1 || selection==2 || selection==4)continue; // 单挂原生管理，无新模型写入。
            FixedMountState layout;layout.aircraft=type;layout.kindsMask=selection;
            layout.logicalCounts=mountSpecification(type).logicalCounts;mountExpansionPositions(layout);
            uint64_t occupied=0;
            for(int kind=0;kind<3;++kind) {
                const auto& selected=plan.at("selected").at(kind);
                assert(selected.empty()==((selection&(1u<<kind))==0));
                for(int physical:layout.expansionPositions[kind]) {
                    assert(physical>=0 && physical<64 && !(occupied&(uint64_t{1}<<physical)));
                    occupied|=uint64_t{1}<<physical;
                }
                if(selected.empty())continue;
                for(int ordinal=0;ordinal<layout.logicalCounts[kind];++ordinal) {
                    const int position=mountPhysical(layout,kind,ordinal);
                    assert(std::find(layout.expansionPositions[kind].begin(),layout.expansionPositions[kind].end(),position)!=layout.expansionPositions[kind].end());
                }
                // 窗口位置充足时一轮不重复；减位方案接受共用位置，不减少游戏窗口。
                const int window=std::min<int>(mountSpecification(type).loadedCounts[kind],selected.size());
                uint64_t ready=0;
                for(int ordinal=0;ordinal<window;++ordinal) {
                    const auto bit=uint64_t{1}<<mountPhysical(layout,kind,ordinal);assert(!(ready&bit));ready|=bit;
                }
            }
            if(type==MountAircraft::F02a && (selection&4)) {
                assert(layout.expansionSockets[2].size()==38);
                const int last=mountPhysical(layout,2,37);
                assert(last>=32 && ((uint64_t{1}<<last)>>32)!=0); // 后续轮转到外侧巢后段时不丢高位。
            }
            ++multiCases;
        }
    }
    assert(multiCases==40);
    // 大于旧12槽和64位范围时仍保存原生序号；重建后的数组下标不能冒充原挂点。
    Group rocketQueue;rocketQueue.slots.resize(76);
    for(int i=0;i<76;++i)rocketQueue.slots[i].mountOrdinal=i;
    fixedMountRestoreOrder(rocketQueue,71);
    for(int i=0;i<76;++i)assert(fixedMount.order[i]==(71+i)%76);
    fixedMount.pendingReadyMask.reset();fixedMountRecordReload(true,75,10,9,10);
    assert(fixedMount.pendingReadyMask.test(75) && !fixedMount.pendingReadyMask.test(11));
    fixedMount={};fixedMountEnabled=false;
#endif
    std::cout<<"weapon_group: cache restore, reload, mount layout and preview boundary checks passed\n";
}
