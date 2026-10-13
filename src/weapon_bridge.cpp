// DualSP's in-process weapon bridge for Steam AC7 build 9855922.
// Called by UE4SS package.loadlib on the GAME THREAD. No Lua ABI dependency.
// Offsets below were derived from native getters and verified against live shots.
// Early playable version. Reject unsupported executable layouts before access.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <map>
#include "common.hpp"
#include "reload_timer.hpp"

namespace {
using Addr = uintptr_t;
struct Command { std::string op; Addr plane{}, activator{}; int target{}; double time{}; int secondConfig{1}, firstConfig{}; double ratio{1}; uint64_t request{}; };
std::mutex nativeExecutionMutex;
struct Slot { float duration{}, elapsed{}; };
struct Group {
    bool valid{};
    Addr weaponClass{};
    std::array<unsigned char, 0x20> scalars{};
    std::vector<Slot> slots;
    double capturedTime{};
};
struct Session {
    Addr plane{}, activator{};
    int active{};
    int secondConfig{1};
    int firstConfig{};
    int gun{}, flares{};
    std::array<int,2> maximum{};
    double ratio{1};
    Group main;
    std::array<Group,2> special;
} session;
Session checkpoint;
bool checkpointValid=false;
double checkpointTime{};

void require(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
void checkMemory(Addr address, size_t size) {
    MEMORY_BASIC_INFORMATION info{};
    require(address && VirtualQuery(reinterpret_cast<void*>(address), &info, sizeof(info)), "Invalid address");
    require(info.State==MEM_COMMIT && !(info.Protect & (PAGE_GUARD|PAGE_NOACCESS)), "Unreadable memory");
    require(address+size>=address && address+size<=reinterpret_cast<Addr>(info.BaseAddress)+info.RegionSize, "Range outside allocation");
}
template<class T> T read(Addr address) { checkMemory(address,sizeof(T)); T value; std::memcpy(&value,reinterpret_cast<void*>(address),sizeof(T)); return value; }
template<class T> void write(Addr address, T value) { checkMemory(address,sizeof(T)); std::memcpy(reinterpret_cast<void*>(address),&value,sizeof(T)); }
Addr stateOf(Addr activator) {
    const auto owner=read<Addr>(activator+0x260);
    return read<Addr>(owner+0xf0);
}
Addr groupOf(Addr activator, bool special) { return stateOf(activator)+(special ? 0x110 : 0x88); }
void verifyVersion() {
    const auto base=reinterpret_cast<Addr>(GetModuleHandleW(nullptr));
    // GetSpecialWeaponCount: owner -> state -> SP ammunition. Reject other builds.
    constexpr unsigned char signature[]={0x48,0x8b,0x81,0x60,0x02,0,0,0x48,0x8b,0x88,0xf0,0,0,0,0x8b,0x81,0x18,0x01,0,0,0xc3};
    checkMemory(base+0x882390,sizeof(signature));
    require(std::memcmp(reinterpret_cast<void*>(base+0x882390),signature,sizeof(signature))==0,"Unsupported executable layout");
}
Group capture(Addr activator, bool special, double time) {
    const auto address=groupOf(activator,special);
    Group result;
    result.weaponClass=read<Addr>(address);
    checkMemory(address+8,result.scalars.size());
    std::memcpy(result.scalars.data(),reinterpret_cast<void*>(address+8),result.scalars.size());
    const auto slotData=read<Addr>(address+0x58);
    const auto num=read<int>(address+0x60);
    const auto capacity=read<int>(address+0x64);
    require(num>0 && num<=capacity && capacity<=64,"Unexpected weapon slot array");
    for(int i=0;i<num;++i) {
        Slot slot{read<float>(slotData+i*0x60+0x48),read<float>(slotData+i*0x60+0x4c)};
        try { dualsp::validateReload(slot.duration,slot.elapsed); }
        catch(const std::exception& e) {
            std::ostringstream detail;
            detail<<e.what()<<" group="<<(special?"SP":"MSL")<<" slot="<<i
                  <<" duration="<<slot.duration<<" elapsed="<<slot.elapsed;
            throw std::runtime_error(detail.str());
        }
        // Record each kind of previously rejected timer once per session. These
        // diagnostics distinguish overshoot from signed offsets in real play.
        static bool loggedNegative=false,loggedOvershoot=false;
        bool& logged=slot.elapsed<0?loggedNegative:loggedOvershoot;
        if((slot.elapsed<0 || slot.elapsed>slot.duration+0.1f) && !logged) {
            logged=true;
            nativeLog(Json{{"event","RELOAD_TIMER_OUTSIDE_PROGRESS_RANGE"},{"group",special?"SP":"MSL"},
                {"slot",i},{"duration",slot.duration},{"elapsed",slot.elapsed}}.dump());
        }
        result.slots.push_back(slot);
    }
    result.valid=true;
    result.capturedTime=time;
    return result;
}
int scalarInt(const Group& group, size_t offset) { int value; std::memcpy(&value,group.scalars.data()+offset,sizeof(value)); return value; }
void restore(Addr activator, bool special, const Group& saved, double time) {
    require(saved.valid,"Uninitialized cached weapon");
    const auto address=groupOf(activator,special);
    require(read<Addr>(address)==saved.weaponClass,"Weapon class changed");
    const auto slotData=read<Addr>(address+0x58);
    const auto num=read<int>(address+0x60);
    require(num==static_cast<int>(saved.slots.size()),"Weapon slot count changed");
    const int loadedCount=scalarInt(saved,8);   // group+0x10
    const int nextSlot=scalarInt(saved,12);     // group+0x14
    require(loadedCount>=0 && loadedCount<=num && nextSlot>=0 && nextSlot<num,"Invalid cached launch cursor");
    require(time+0.001>=saved.capturedTime,"Game time rewound; checkpoint restoration required");
    const double dt=std::max(0.0,time-saved.capturedTime);
    for(int i=0;i<num;++i)require(std::abs(read<float>(slotData+i*0x60+0x48)-saved.slots[(nextSlot+i)%num].duration)<0.001f,"Reload configuration changed");
    // Only the current load window progresses. Advancing every physical hardpoint
    // would create extra ready missiles and violate the native loaded-count cap.
    checkMemory(address+8,saved.scalars.size());
    std::memcpy(reinterpret_cast<void*>(address+8),saved.scalars.data(),saved.scalars.size());
    // Initialization rebuilds the native projectile pool with hardpoints 0..N-1
    // active. Restore the logical loading queue into that fresh pool, rather than
    // restoring a cursor whose old projectile instances no longer exist.
    write<int>(address+0x14,0);
    for(int i=0;i<num;++i) {
        const auto& slot=saved.slots[(nextSlot+i)%num];
        require(std::abs(read<float>(slotData+i*0x60+0x48)-slot.duration)<0.001f,"Reload configuration changed");
        const int distance=i;
        float elapsed=dualsp::advanceReload(slot.duration,slot.elapsed,dt,distance<loadedCount);
        // A newly rebuilt hardpoint may not yet own a loaded projectile. Writing
        // a completed timer skips the native reload-completion event. Let the next
        // game tick cross the threshold and perform the actual native reload.
        const float rebuiltElapsed=read<float>(slotData+i*0x60+0x4c);
        elapsed=dualsp::rebuildReload(slot.duration,elapsed,rebuiltElapsed);
        write<float>(slotData+i*0x60+0x4c,elapsed);
    }
}
void initialize(const Command& command,int index,bool preserveAuxiliary=true) {
    const int gun=read<int>(stateOf(command.activator)+8);
    const auto owner=read<Addr>(command.activator+0x260);
    const int flares=read<int>(owner+0x10c);
    using Init=void(*)(void*,int);
    const auto base=reinterpret_cast<Addr>(GetModuleHandleW(nullptr));
    reinterpret_cast<Init>(base+0x81e810)(reinterpret_cast<void*>(command.plane),index);
    // A loadout rebuild also resets the machine gun. Preserve its ammunition,
    // plus flare count, during an SP change; a deliberate resupply uses defaults.
    if(preserveAuxiliary) {
        write<int>(stateOf(command.activator)+8,gun);
        write<int>(read<Addr>(command.activator+0x260)+0x10c,flares);
    }
}
void keepArmed(const Command& command,bool armed) {
    if(armed) {
        using Toggle=bool(*)(void*);
        const auto base=reinterpret_cast<Addr>(GetModuleHandleW(nullptr));
        reinterpret_cast<Toggle>(base+0x886c50)(reinterpret_cast<void*>(command.activator));
    }
}
std::string snapshotJson(const Command& command) {
    const auto main=capture(command.activator,false,command.time);
    const auto special=capture(command.activator,true,command.time);
    std::ostringstream out;
    out << std::setprecision(9) << "\"active\":" << session.active << ",\"main_ammo\":" << scalarInt(main,0)
        << ",\"special_ammo\":" << scalarInt(special,0) << ",\"reload\":[";
    for(size_t i=0;i<special.slots.size();++i) { if(i)out<<','; out<<'['<<special.slots[i].duration<<','<<special.slots[i].elapsed<<']'; }
    out<<']';
    out<<",\"gun_ammo\":"<<read<int>(stateOf(command.activator)+8)
       <<",\"flares\":"<<read<int>(read<Addr>(command.activator+0x260)+0x10c)
       <<",\"armed\":"<<(read<unsigned char>(command.activator+0x2a1)!=0 ? "true":"false")
       <<",\"cached_sp\":[";
    for(int i=0;i<2;++i) { if(i)out<<','; out<<(i==session.active ? scalarInt(special,0) : (session.special[i].valid ? scalarInt(session.special[i],0) : -999)); }
    out<<']';
    return out.str();
}
Command readCommand() {
    std::ifstream file(modRoot()/"native-command.txt");
    Command command;
    file>>command.op>>std::hex>>command.plane>>command.activator>>std::dec>>command.target>>command.time;
    require(static_cast<bool>(file),"Malformed command");
    file>>command.secondConfig>>command.firstConfig;
    require(static_cast<bool>(file),"Missing weapon configurations");
    require(command.target==0 || command.target==1,"Prototype supports two logical SP slots");
    require(command.secondConfig>=0 && command.secondConfig<3 && command.firstConfig>=0 && command.firstConfig<3,"Unsupported SP configuration");
    require(command.firstConfig!=command.secondConfig,"SP selections must be different");
    require(std::isfinite(command.time) && command.time>=0,"Invalid game time");
    file>>command.ratio;require(static_cast<bool>(file) && command.ratio>=0.1 && command.ratio<=1,"Invalid ammo ratio");
    file>>command.request;require(static_cast<bool>(file) && command.request>0,"Missing native request ID");
    return command;
}
void scaleAmmo(Addr activator,double ratio) {
    const auto group=groupOf(activator,true);
    for(auto offset:{8,12}) {
        const int ammo=read<int>(group+offset);
        require(ammo>=0 && ammo<=100000,"Invalid SP ammunition");
        write<int>(group+offset,std::max(1,static_cast<int>(std::floor(ammo*ratio))));
    }
}
void execute(const Command& command) {
    verifyVersion();
    require(read<Addr>(command.plane+0x11a8)==command.activator,"Activator is not owned by this plane");
    if(command.op=="setup") {
        session={}; session.plane=command.plane; session.activator=command.activator; session.active=0;
        session.secondConfig=command.secondConfig;
        session.firstConfig=command.firstConfig;
        session.ratio=command.ratio;
        checkpointValid=false;
        session.main=capture(command.activator,false,command.time);
        scaleAmmo(command.activator,session.ratio);
        session.special[0]=capture(command.activator,true,command.time);
        session.maximum[0]=scalarInt(session.special[0],0);
        const bool armed=read<unsigned char>(command.activator+0x2a1)!=0;
        initialize(command,session.secondConfig);
        scaleAmmo(command.activator,session.ratio);
        session.special[1]=capture(command.activator,true,command.time);
        session.maximum[1]=scalarInt(session.special[1],0);
        initialize(command,session.firstConfig);
        restore(command.activator,false,session.main,command.time);
        restore(command.activator,true,session.special[0],command.time);
        keepArmed(command,armed);
    } else if(command.op=="switch") {
        require(session.plane==command.plane && session.activator==command.activator,"A new plane requires setup");
        require(command.target!=session.active,"Already using requested SP");
        session.main=capture(command.activator,false,command.time);
        session.special[session.active]=capture(command.activator,true,command.time);
        const bool armed=read<unsigned char>(command.activator+0x2a1)!=0;
        initialize(command,command.target==0 ? session.firstConfig : session.secondConfig);
        restore(command.activator,false,session.main,command.time);
        restore(command.activator,true,session.special[command.target],command.time);
        session.active=command.target;
        keepArmed(command,armed);
    } else if(command.op=="checkpoint_save") {
        require(session.plane==command.plane && session.activator==command.activator,"A new plane requires setup");
        session.main=capture(command.activator,false,command.time);
        session.special[session.active]=capture(command.activator,true,command.time);
        session.gun=read<int>(stateOf(command.activator)+8);
        session.flares=read<int>(read<Addr>(command.activator+0x260)+0x10c);
        checkpoint=session; checkpointTime=command.time; checkpointValid=true;
    } else if(command.op=="checkpoint_restore") {
        require(checkpointValid,"No captured checkpoint");
        // Pointer-independent timers and scalar caches can be restored to a newly
        // spawned plane of the same class. restore() validates weapon identities.
        const bool armed=read<unsigned char>(command.activator+0x2a1)!=0;
        session=checkpoint; session.plane=command.plane; session.activator=command.activator;
        const double rebase=command.time-checkpointTime;
        session.main.capturedTime+=rebase;
        for(auto& sp:session.special)sp.capturedTime+=rebase;
        initialize(command,session.active==0 ? session.firstConfig : session.secondConfig);
        restore(command.activator,false,session.main,command.time);
        restore(command.activator,true,session.special[session.active],command.time);
        write<int>(stateOf(command.activator)+8,session.gun);
        write<int>(read<Addr>(command.activator+0x260)+0x10c,session.flares);
        keepArmed(command,armed);
    } else if(command.op=="resupply") {
        require(session.plane==command.plane && session.activator==command.activator,"A new plane requires setup");
        const bool armed=read<unsigned char>(command.activator+0x2a1)!=0;
        const int active=session.active;
        initialize(command,session.firstConfig,false);
        scaleAmmo(command.activator,session.ratio);
        session.main=capture(command.activator,false,command.time);
        session.special[0]=capture(command.activator,true,command.time);
        initialize(command,session.secondConfig,false);
        scaleAmmo(command.activator,session.ratio);
        session.special[1]=capture(command.activator,true,command.time);
        if(active==0) {initialize(command,session.firstConfig,false);scaleAmmo(command.activator,session.ratio);}
        keepArmed(command,armed);
    } else {
        require(command.op=="snapshot","Unknown command");
    }
}
}

extern "C" __declspec(dllexport) int lab_native_execute(void*) {
    std::lock_guard<std::mutex> guard(nativeExecutionMutex);
    uint64_t request=0;
    std::string result;
    try {
        const auto command=readCommand();
        request=command.request;
        if(command.op!="snapshot")nativeLog(Json{{"event","NATIVE_BEGIN"},{"op",command.op},{"request",request},
            {"target",command.target},{"a",command.firstConfig},{"b",command.secondConfig},{"thread",GetCurrentThreadId()}}.dump());
        execute(command);
        Json state=Json::parse("{"+snapshotJson(command)+"}");
        state["ok"]=true;state["request"]=request;state["maximum"]=session.maximum;
        state["slots"]=Json::array();
        for(int index=0;index<2;++index) {
            const auto g=index==session.active?capture(command.activator,true,command.time):session.special[index];
            Json bars=Json::array();
            if(g.valid) {
                const int next=scalarInt(g,12),loaded=scalarInt(g,8);
                require(loaded>=0 && loaded<=static_cast<int>(g.slots.size()) && next>=0 && next<static_cast<int>(g.slots.size()),"Invalid HUD launch cursor");
                for(int k=0;k<loaded;++k) {
                    const auto& s=g.slots[(next+k)%g.slots.size()];
                    const float elapsed=dualsp::advanceReload(s.duration,s.elapsed,std::max(0.0,command.time-g.capturedTime),true);
                    bars.push_back(dualsp::reloadProgress(s.duration,elapsed));
                }
            }
            state["slots"].push_back(bars);
        }
        result=state.dump();
        if(command.op!="snapshot")nativeLog(Json{{"event","NATIVE_DONE"},{"op",command.op},{"request",request},
            {"active",session.active},{"main_ammo",state["main_ammo"]},{"sp",state["cached_sp"]}}.dump());
    } catch(const std::exception& error) {
        result=Json{{"ok",false},{"request",request},{"error",error.what()}}.dump();nativeLog(result);
    }
    // A reader must see an entire response. Include the request ID so an I/O
    // failure cannot cause the controller to accept a previous successful reply.
    try {atomicJson(modRoot()/"native-status.json",Json::parse(result));}
    catch(const std::exception& e){nativeLog(std::string("NATIVE_RESPONSE_WRITE_ERROR ")+e.what());}
    return 0;
}

// Read immutable native UE4.18 data tables on the game thread. The reflected row
// offsets were checked in the Steam build; require a dense, validated row map.
std::vector<Addr> tableRows(Addr table) {
    const auto data=read<Addr>(table+0x30);
    const int count=read<int>(table+0x38),capacity=read<int>(table+0x3c),free=read<int>(table+0x64);
    require(count>0 && count<=capacity && capacity<=4096 && free==0,"Unsupported data-table map");
    std::vector<Addr> rows;
    for(int i=0;i<count;++i) rows.push_back(read<Addr>(data+i*24+8));
    return rows;
}
std::string unrealString(Addr address) {
    const auto data=read<Addr>(address);const int n=read<int>(address+8),capacity=read<int>(address+12);
    require(n>=0 && n<=capacity && capacity<=4096,"Invalid Unreal string");
    if(!n)return {};
    checkMemory(data,n*2);
    const auto wide=reinterpret_cast<const wchar_t*>(data);
    const int size=WideCharToMultiByte(CP_UTF8,0,wide,n-1,nullptr,0,nullptr,nullptr);
    std::string result(size,0);WideCharToMultiByte(CP_UTF8,0,wide,n-1,result.data(),size,nullptr,nullptr);return result;
}
extern "C" __declspec(dllexport) int dualsp_catalogue(void*) {
    try {
        verifyVersion();Addr planes{},weapons{};
        std::ifstream cmd(modRoot()/"catalogue-command.txt");cmd>>std::hex>>planes>>weapons;
        require(static_cast<bool>(cmd),"Invalid catalogue command");
        std::map<std::string,Json> byCode;
        for(const auto row:tableRows(weapons)) {
            Json stats=Json::array();for(int i=0;i<6;++i)stats.push_back(read<unsigned char>(row+40+i));
            byCode[unrealString(row+16)]={{"weapon_id",read<int>(row+8)},{"stats",stats}};
        }
        Json catalog=Json::object();
        for(const auto row:tableRows(planes)) {
            const int id=read<int>(row+8);Json options=Json::array();bool supported=true;
            for(int i=0;i<3;++i) {
                const auto code=unrealString(row+96+i*24);
                if(code=="-1") {supported=false;continue;}
                require(byCode.count(code),"Unknown weapon data reference");
                auto option=byCode.at(code);option["index"]=i;option["code"]=code;option["max"]=read<int>(row+112+i*24);options.push_back(option);
            }
            catalog[std::to_string(id)]={{"id",id},{"code",unrealString(row+16)},{"main_max",read<int>(row+88)},{"weapons",options},{"supported",supported}};
        }
        atomicJson(modRoot()/"catalogue.json",catalog);nativeLog("Catalogue loaded: "+std::to_string(catalog.size())+" planes");
    }catch(const std::exception& e){nativeLog(std::string("CATALOGUE_ERROR ")+e.what());atomicJson(modRoot()/"catalogue.json",Json{{"error",e.what()}});}
    return 0;
}
