// Exercise the shipped capture/restore implementation against synthetic memory.
// No AC7 process, native game function, or executable patch is involved.
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
    fixedMount={};
#endif
    std::cout<<"weapon_group: cache restore, reload and mount boundary checks passed\n";
}
