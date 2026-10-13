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
    std::cout<<"weapon_group: empty ammo, zero load window, queue rotation, cooldown and validation passed\n";
}
