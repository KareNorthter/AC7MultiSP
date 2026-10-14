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
#include "MinHook.h"
#include <map>
#include "common.hpp"
#include "reload_timer.hpp"
#include "ue4ss_lua_api.hpp"

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
// 原生补给页直接读取 UIManager 按键，SetIsEnableControl 不会阻止这两个入口。
// 只隔离本轮玩家补给页；页面 Tick、动画、原生 YES 与其他菜单保持正常执行。
using SupplyInput = void(*)(void*);
SupplyInput originalSupplyReview=nullptr,originalSupplyWeapons=nullptr; // 两个输入函数的原生入口。
Addr supplyPlane{},supplyActivator{},supplyWidget{}; // 当前补给身份，仅由游戏线程维护。
bool supplyHooksReady=false; // 钩子只安装一次，事务结束后通过身份解绑停止隔离。

// 入口已经限定为 MissionResupplyWidget；只比较当前补给的飞机/激活器身份。
bool ownsSupplyInput(void* object) {
    if(!supplyPlane)return false;
    const auto widget=reinterpret_cast<Addr>(object); // 当前正在读取菜单输入的页面。
    if(read<Addr>(widget+0x5c0)!=supplyPlane || read<Addr>(widget+0x5c8)!=supplyActivator)return false;
    if(!supplyWidget) {
        supplyWidget=widget;
        nativeLog("RESUPPLY_INPUT_OWNED");
    }
    return widget==supplyWidget;
}
// 模组接管的页面不再处理原生单 SP 的 YES/NO 按键。
void supplyReviewInput(void* object) {
    if(!ownsSupplyInput(object))originalSupplyReview(object);
}
// 同一事务的原生单 SP 列表也不能处理双 SP 页正在使用的决定/方向键。
void supplyWeaponsInput(void* object) {
    if(!ownsSupplyInput(object))originalSupplyWeapons(object);
}
// 首次补给前核对函数指纹并安装两个独立钩子；失败明确返回，不继续混用输入。
void installSupplyHooks() {
    if(supplyHooksReady)return;
    const auto base=reinterpret_cast<Addr>(GetModuleHandleW(nullptr)); // 已核对的游戏映像。
    constexpr unsigned char review[]={0x48,0x89,0x5c,0x24,0x08,0x57,0x48,0x83,0xec,0x20,0x8b,0xb9,0xb8,0x05,0,0};
    constexpr unsigned char weapons[]={0x48,0x89,0x5c,0x24,0x08,0x57,0x48,0x83,0xec,0x20,0x8b,0xb9,0xbc,0x05,0,0};
    require(std::memcmp(reinterpret_cast<void*>(base+0x6fccf0),review,sizeof(review))==0 &&
        std::memcmp(reinterpret_cast<void*>(base+0x6fd2d0),weapons,sizeof(weapons))==0,"补给输入入口不匹配");
    const auto status=MH_Initialize(); // 本核心的 MinHook 实例，与 UI DLL 分开链接。
    require(status==MH_OK || status==MH_ERROR_ALREADY_INITIALIZED,"无法初始化补给输入钩子");
    require(MH_CreateHook(reinterpret_cast<void*>(base+0x6fccf0),reinterpret_cast<void*>(&supplyReviewInput),
        reinterpret_cast<void**>(&originalSupplyReview))==MH_OK,"无法接入补给确认输入");
    require(MH_CreateHook(reinterpret_cast<void*>(base+0x6fd2d0),reinterpret_cast<void*>(&supplyWeaponsInput),
        reinterpret_cast<void**>(&originalSupplyWeapons))==MH_OK,"无法接入补给兵装输入");
    require(MH_QueueEnableHook(reinterpret_cast<void*>(base+0x6fccf0))==MH_OK &&
        MH_QueueEnableHook(reinterpret_cast<void*>(base+0x6fd2d0))==MH_OK && MH_ApplyQueued()==MH_OK,
        "无法启用补给输入隔离");
    supplyHooksReady=true;
    nativeLog("RESUPPLY_INPUT_HOOKS_READY");
}
// 在原生初始化丢弃旧锁定槽之前，逐槽释放其搜索/锁定音引用。
// 不能只清零计数或发送全局静音；这会丢失原生播放与停止的配对关系。
void finishSeekerAudio(Addr plane,Addr activator) {
    require(read<Addr>(plane+0x11a8)==activator,"音效收尾的机体与激活器不匹配");
    const auto seeker=read<Addr>(plane+0x1310); // 当前玩家的锁定组件。
    require(seeker && read<Addr>(seeker+0x378)==plane,"音效收尾缺少当前玩家锁定组件");
    const auto base=reinterpret_cast<Addr>(GetModuleHandleW(nullptr)); // 原生逐槽音效收尾函数。
    constexpr unsigned char stop[]={0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x74,0x24,0x10,0x57,0x48,0x83,0xec,0x20,
        0x48,0x8b,0x99,0x68,0x04,0,0};
    require(std::memcmp(reinterpret_cast<void*>(base+0x4c98a0),stop,sizeof(stop))==0,"原生锁定音收尾入口不匹配");
    std::array<int,4> before{}; // 四种音效的引用计数，只用于检查和记录。
    for(size_t i=0;i<before.size();++i) {
        before[i]=read<int>(seeker+0x4f0+i*4);
        require(before[i]>=0 && before[i]<=512,"锁定音引用计数无效");
    }
    using Stop=void(*)(void*); // 原生内部按每个槽的播放标记发送对应停止事件。
    reinterpret_cast<Stop>(base+0x4c98a0)(reinterpret_cast<void*>(seeker));
    for(size_t i=0;i<before.size();++i)
        require(read<int>(seeker+0x4f0+i*4)==0,"旧锁定音引用已失配，请重新开始任务；停止重建兵装");
    if(std::any_of(before.begin(),before.end(),[](int count){return count!=0;}))
        nativeLog(Json{{"event","SEEKER_AUDIO_RELEASED"},{"before",before}}.dump());
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

// 判断允许装填的逻辑窗口内是否至少一发就绪；装填窗口数量不是就绪数量。
bool hasReadyRound(const Group& group,double time) {
    require(group.valid,"HUD 武器缓存未建立");
    const int ammo=scalarInt(group,0),window=scalarInt(group,8),next=scalarInt(group,12);
    const int count=static_cast<int>(group.slots.size());
    // CASUAL-EASY 的原生无限弹药使用 -1；它仍需按真实装填队列判断就绪。
    // 仅接受此已实机核对的特殊值，其他负数及无效队列继续明确报错。
    require(ammo>=-1 && ammo<=100000 && window>=0 && window<=count && next>=0 && next<count,"HUD 武器队列无效");
    require(std::isfinite(time) && time+0.001>=group.capturedTime,"HUD 游戏时间回退，需先恢复检查点");
    if(ammo==0)return false;
    // 未激活 SP 只推进本来允许装填的窗口；活动组捕获时间就是本次时间。
    const double delta=std::max(0.0,time-group.capturedTime);
    for(int index=0;index<window;++index) {
        const auto& slot=group.slots[(next+index)%count];
        const float elapsed=dualsp::advanceReload(slot.duration,slot.elapsed,delta,true);
        if(elapsed>=slot.duration)return true;
    }
    return false;
}
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
    finishSeekerAudio(command.plane,command.activator); // 先释放旧槽音效，再重建锁定槽和武器池。
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
        // 重建后必须经过飞机完整切换流程。仅 ToggleSpecial 会留下 MSL
        // 的锁定组件参数；原生飞机入口还会重绑 seeker 并应用 PART 修正。
        using Switch=void(*)(void*);
        const auto base=reinterpret_cast<Addr>(GetModuleHandleW(nullptr));
        constexpr unsigned char pressed[]={0x33,0xd2,0xe9};
        constexpr unsigned char released[]={0xc6,0x81,0x51,0x1a,0,0,0,0xc3};
        require(std::memcmp(reinterpret_cast<void*>(base+0x8238d0),pressed,sizeof(pressed))==0 &&
            std::memcmp(reinterpret_cast<void*>(base+0x8238e0),released,sizeof(released))==0,
            "原生飞机切换入口不匹配");
        reinterpret_cast<Switch>(base+0x8238d0)(reinterpret_cast<void*>(command.plane));
        reinterpret_cast<Switch>(base+0x8238e0)(reinterpret_cast<void*>(command.plane));
        require(read<unsigned char>(command.activator+0x2a1)!=0,"重建后无法恢复原生 SP 类别");
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
    // 补给重选需要重建当前组合，同时保留任务已经捕获的检查点。
    // 此新命令只存在于源码候选；正式 DLL 保持不变，不能直接混装。
    if(command.op=="setup" || command.op=="resupply_setup") {
        session={}; session.plane=command.plane; session.activator=command.activator; session.active=0;
        session.secondConfig=command.secondConfig;
        session.firstConfig=command.firstConfig;
        session.ratio=command.ratio;
        if(command.op=="setup")checkpointValid=false;
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

// 仅声明候选核心支持新的补给协议，供 Lua 在重建兵装前核对版本。
extern "C" __declspec(dllexport) int dualsp_resupply_supported(void*) {
    return 0;
}

// 补给开始时取得原生输入所有权；成功、离开任务或检查点恢复时用 0,0 解绑。
extern "C" __declspec(dllexport) int dualsp_resupply_control(void* luaState) {
    const dualsp::LuaApi* api=nullptr; // Lua 绑定失败仍保留原生日志。
    try {
        static const dualsp::LuaApi bound; // 当前加载器的稳定栈接口。
        api=&bound;
        require(api->top(luaState)==2,"补给输入接口参数数量无效");
        const auto plane=static_cast<Addr>(api->readInteger(luaState,1)); // 零表示解绑。
        const auto activator=static_cast<Addr>(api->readInteger(luaState,2)); // 与机体共同核对身份。
        require((plane==0)==(activator==0),"补给输入身份不完整");
        if(plane) {
            verifyVersion();
            require(!supplyPlane,"上一轮补给输入尚未释放");
            require(read<Addr>(plane+0x11a8)==activator,"补给输入机体不匹配");
            installSupplyHooks();
        }
        supplyPlane=plane;supplyActivator=activator;supplyWidget=0;
        nativeLog(plane?"RESUPPLY_INPUT_ARMED":"RESUPPLY_INPUT_RELEASED");
        api->boolean(luaState,true);api->string(luaState,"",0);
        return 2;
    }catch(const std::exception& error) {
        nativeLog(std::string("RESUPPLY_INPUT_ERROR ")+error.what());
        if(api){api->boolean(luaState,false);api->string(luaState,error.what(),std::strlen(error.what()));return 2;}
        return 0;
    }
}

// Lua 的出击/补给初始化也必须在丢弃旧槽前完成音效收尾。
extern "C" __declspec(dllexport) int dualsp_prepare_weapon_rebuild(void* luaState) {
    const dualsp::LuaApi* api=nullptr; // 失败返回明确错误，不清零或静音回退。
    try {
        static const dualsp::LuaApi bound; // 复用加载器接口。
        api=&bound;
        require(api->top(luaState)==2,"兵装重建收尾接口参数数量无效");
        verifyVersion();
        const auto plane=static_cast<Addr>(api->readInteger(luaState,1)); // 当前玩家。
        const auto activator=static_cast<Addr>(api->readInteger(luaState,2)); // 当前武器组件。
        finishSeekerAudio(plane,activator);
        api->boolean(luaState,true);api->string(luaState,"",0);
        return 2;
    }catch(const std::exception& error) {
        nativeLog(std::string("SEEKER_AUDIO_ERROR ")+error.what());
        if(api){api->boolean(luaState,false);api->string(luaState,error.what(),std::strlen(error.what()));return 2;}
        return 0;
    }
}

// 候选 5 的实际补给 YES 入口。只接受 Lua 在游戏线程已确认的页面与
// 玩家绑定，不调用 MiniGamePhase 的结束通知，不替换原生关闭/成功流程。
extern "C" __declspec(dllexport) int dualsp_resupply_takeoff(void* luaState) {
    const dualsp::LuaApi* api=nullptr; // 绑定失败时只能通过 native.log 报错。
    try {
        static const dualsp::LuaApi bound; // 复用专用加载器的 Lua 参数接口。
        api=&bound;
        require(api->top(luaState)==3,"补给起飞接口参数数量无效");
        const auto widget=static_cast<Addr>(api->readInteger(luaState,1)); // 当前原生补给页。
        const auto plane=static_cast<Addr>(api->readInteger(luaState,2)); // 本次补给的玩家机体。
        const auto activator=static_cast<Addr>(api->readInteger(luaState,3)); // 当前武器激活器。
        const auto base=reinterpret_cast<Addr>(GetModuleHandleW(nullptr)); // 已核对的游戏映像。
        verifyVersion();
        const auto table=read<Addr>(widget); // 用页面虚表核对类型，拒绝其他菜单对象。
        require(read<Addr>(table+0x408)==base+0x6fa1f0 && read<Addr>(table+0x420)==base+0x6fc6d0,
            "补给页面原生类型不匹配");
        require(read<Addr>(widget+0x5c0)==plane && read<Addr>(widget+0x5c8)==activator &&
            read<Addr>(plane+0x11a8)==activator,"补给页面与玩家武器不匹配");
        require(supplyPlane==plane && supplyActivator==activator && supplyWidget==widget,
            "补给 YES 页面没有独占原生输入");
        const auto manager=read<Addr>(widget+0x288); // 原生 YES 会由此关闭菜单栈。
        require(manager && read<int>(manager+0xf28)>0 && read<int>(manager+0xf28)<=64,
            "补给页面没有活动的原生菜单栈");
        constexpr unsigned char takeoff[]={0x48,0x89,0x5c,0x24,0x08,0x57,0x48,0x83,0xec,0x20,
            0x48,0x8b,0xd9,0xb2,0x01,0x48,0x8b,0x89,0xc8,0x05,0,0,0xe8};
        require(std::memcmp(reinterpret_cast<void*>(base+0x6fc680),takeoff,sizeof(takeoff))==0,
            "补给原生 YES 入口不匹配");
        nativeLog(Json{{"event","RESUPPLY_NATIVE_YES_BEGIN"}}.dump());
        // 原生 YES 依次清理武器状态、关闭 UI 栈并通知任务；不持有核心互斥锁，
        // 因为关闭过程可以同步回调 Lua。返回值只代表调用返回，不能冒充补给成功。
        using Takeoff=void(*)(void*);
        reinterpret_cast<Takeoff>(base+0x6fc680)(reinterpret_cast<void*>(widget));
        nativeLog(Json{{"event","RESUPPLY_NATIVE_YES_RETURNED"}}.dump());
        api->boolean(luaState,true);api->string(luaState,"",0);
        return 2;
    }catch(const std::exception& error) {
        nativeLog(std::string("RESUPPLY_NATIVE_YES_ERROR ")+error.what());
        if(api) {
            api->boolean(luaState,false);api->string(luaState,error.what(),std::strlen(error.what()));
            return 2;
        }
        return 0;
    }
}

// 游戏线程只读采样，直接返回 Lua 值，不经过命令文件或 JSON。
// 返回：成功、MSL就绪、SP1就绪、SP2就绪、错误文字；不修改武器或核心缓存。
extern "C" __declspec(dllexport) int dualsp_hud_readiness(void* luaState) {
    const dualsp::LuaApi* api=nullptr;
    try {
        // 接口解析每个进程只做一次；加载器不匹配则明确报错。
        static const dualsp::LuaApi bound;
        api=&bound;
        std::lock_guard<std::mutex> guard(nativeExecutionMutex);
        require(api->top(luaState)==4,"HUD 内存接口参数数量无效");
        const auto plane=static_cast<Addr>(api->readInteger(luaState,1));
        const auto activator=static_cast<Addr>(api->readInteger(luaState,2));
        const double time=api->readNumber(luaState,3);
        const int dual=static_cast<int>(api->readInteger(luaState,4));
        require(dual==0 || dual==1,"HUD 双 SP 标志无效");
        require(std::isfinite(time) && time>=0,"HUD 游戏时间无效");
        verifyVersion();
        require(read<Addr>(plane+0x11a8)==activator,"HUD 激活器不属于当前玩家机体");
        const auto main=capture(activator,false,time),special=capture(activator,true,time);
        const bool mainReady=hasReadyRound(main,time);
        std::array<bool,2> spReady={hasReadyRound(special,time),false};
        if(dual) {
            require(session.plane==plane && session.activator==activator,"HUD 双 SP 会话不匹配");
            require(session.active==0 || session.active==1,"HUD 活动 SP 无效");
            spReady[session.active]=hasReadyRound(special,time);
            spReady[1-session.active]=hasReadyRound(session.special[1-session.active],time);
        }
        api->boolean(luaState,true);api->boolean(luaState,mainReady);
        api->boolean(luaState,spReady[0]);api->boolean(luaState,spReady[1]);api->string(luaState,"",0);
        return 5;
    }catch(const std::exception& error) {
        if(api) {
            api->boolean(luaState,false);
            for(int index=0;index<3;++index)api->boolean(luaState,false);
            api->string(luaState,error.what(),std::strlen(error.what()));
            return 5;
        }
        // Lua 接口本身不能绑定时无法压栈；调度器会因缺少成功值明确显示 HUD 错误。
        nativeLog(std::string("HUD_LUA_API_ERROR ")+error.what());
        return 0;
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
