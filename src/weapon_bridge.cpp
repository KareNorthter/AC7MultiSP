// DualSP's in-process weapon bridge for Steam AC7 build 9855922.
// Called by UE4SS package.loadlib on the GAME THREAD. No Lua ABI dependency.
// Offsets below were derived from native getters and verified against live shots.
// Early playable version. Reject unsupported executable layouts before access.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <Xinput.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <bitset>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <cwchar>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include "MinHook.h"
#include <map>
#include <memory>
#include <set>
#include "common.hpp"
#include "reload_timer.hpp"
#include "ammo_count.hpp"
#include "ue4ss_lua_api.hpp"
#include "keyboard_shortcuts.hpp"
#include "selection_wave.hpp"
#include "common_bay_animation.hpp"
#include "native_common_bay_parts.hpp"
#include "owned_bone_refresh.hpp"
#include "bounded_log.hpp"

namespace {
using Addr = uintptr_t;
// Lua 每次提交完整方案；配置编号与逻辑槽编号不得混用。
struct Command {
    std::string op; // 初始化、换槽、补给或检查点动作。
    Addr plane{}, activator{}; // 当前玩家及其兵装组件身份。
    int target{}; // 目标逻辑槽，取值 0 至 count-1。
    double time{}; // 游戏时间，用于冷却推进与检查点时间重定位。
    int count{1}; // 实际搭载数量。
    double ratio{1}; // 单种 SP 配弹比例，由协议 4 显式提供。
    std::array<int,3> configs{0,-1,-1}; // 原生配置，未使用槽必须为 -1。
    uint64_t request{}; // 请求序号，防止旧响应被重复消费。
};
std::mutex nativeExecutionMutex;
struct Slot {
    float duration{}, elapsed{}; // 原生装填时长和已过时间。
#ifdef DUALSP_SU30SM_MOUNT_PROTOTYPE
    int mountOrdinal{-1}; // 只在独立原型中保存逻辑挂点身份。
#endif
};
struct Group {
    bool valid{};
    bool fastReloadEligible{};
    Addr weaponClass{};
    std::array<unsigned char, 0x20> scalars{};
    std::vector<Slot> slots;
    double capturedTime{};
};
struct Session {
    Addr plane{}, activator{};
    DWORD gameThread{}; // 出击/重配建立的线程；CFA待发输入只能在该线程归还。
    int active{};
    int count{1}; // 有效逻辑槽数量；空槽不初始化。
    double ratio{1}; // 会话建立时的配弹比例；检查点恢复沿用其原始比例。
    std::array<int,3> configs{0,-1,-1}; // 各逻辑槽映射到机体原生配置。
    int gun{}, flares{};
    std::array<int,3> maximum{}; // 每个逻辑槽独立的配弹上限。
    Group main;
    std::array<Group,3> special; // 每槽的弹药、装填队列和捕获时间。
} session;
Session checkpoint;
bool checkpointValid=false;
double checkpointTime{};
bool runtimeEnabled=false; // 未收到游戏线程的明确单人许可前，原生接口和钩子全部直通/拒绝修改。
bool debugInfiniteAmmo=false,debugFastReload=false;
#ifdef DUALSP_SU30SM_MOUNT_PROTOTYPE
int fixedMountOrdinal(Addr weaponClass,int index); // 原型采样时保留重排前的挂点身份。
float fixedMountElapsed(int index,float duration,float elapsed); // 区分实际冷却与重建用的一次性完成事件。
#endif

void require(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
// 仅CFA切换期间统计实际VirtualQuery次数及耗时，不用估计值代替实测。
struct NativeReadCost {
    bool active{};uint64_t calls{},ticks{},batchReads{},batchReused{};
    static uint64_t now() { LARGE_INTEGER value{};QueryPerformanceCounter(&value);return value.QuadPart; }
};
thread_local NativeReadCost nativeReadCost;
struct CfaWorkCost { uint64_t calls{},ticks{}; };
thread_local std::array<CfaWorkCost,8> cfaWorkCosts;
struct CfaWorkScope {
    unsigned kind;uint64_t start{};
    explicit CfaWorkScope(unsigned value):kind(value) {if(nativeReadCost.active)start=NativeReadCost::now();}
    void stop() {if(start){auto& s=cfaWorkCosts[kind];++s.calls;s.ticks+=NativeReadCost::now()-start;start=0;}}
    ~CfaWorkScope() {stop();}
};
MEMORY_BASIC_INFORMATION readableMemory(Addr address, size_t size) {
    MEMORY_BASIC_INFORMATION info{};
    const auto started=nativeReadCost.active?NativeReadCost::now():0;
    const bool queried=address && VirtualQuery(reinterpret_cast<void*>(address), &info, sizeof(info));
    if(nativeReadCost.active){++nativeReadCost.calls;nativeReadCost.ticks+=NativeReadCost::now()-started;}
    require(queried, "Invalid address");
    require(info.State==MEM_COMMIT && !(info.Protect & (PAGE_GUARD|PAGE_NOACCESS)), "Unreadable memory");
    require(address+size>=address && address+size<=reinterpret_cast<Addr>(info.BaseAddress)+info.RegionSize, "Range outside allocation");
    return info;
}
void checkMemory(Addr address,size_t size) { (void)readableMemory(address,size); }
// 仅在没有引擎调用的短只读块内合并VirtualQuery；不缓存对象字段的值。
// 必须是栈上局部变量，不能跨ProcessEvent、原生调用、帧或线程保留。
class NativeReadBatch {
    struct Region { Addr begin{},end{}; };
    mutable std::array<Region,8> regions_{};
    mutable size_t used_{},next_{};
public:
    NativeReadBatch()=default;
    NativeReadBatch(const NativeReadBatch&)=delete;
    NativeReadBatch& operator=(const NativeReadBatch&)=delete;
    template<class T> T read(Addr address) const {
        bool covered=false;
        if(address && address+sizeof(T)>=address)for(size_t i=0;i<used_;++i) {
            const auto& region=regions_[i];
            if(address>=region.begin && address+sizeof(T)<=region.end) {covered=true;break;}
        }
        if(nativeReadCost.active) {++nativeReadCost.batchReads;if(covered)++nativeReadCost.batchReused;}
        if(!covered) {
            const auto info=readableMemory(address,sizeof(T));
            const Addr begin=reinterpret_cast<Addr>(info.BaseAddress);
            require(begin+info.RegionSize>=begin,"只读批次：内存区间溢出");
            regions_[next_]={begin,begin+info.RegionSize};
            next_=(next_+1)%regions_.size();used_=std::min(used_+1,regions_.size());
        }
        T value;std::memcpy(&value,reinterpret_cast<const void*>(address),sizeof(T));return value;
    }
};
template<class T> T read(Addr address) { checkMemory(address,sizeof(T)); T value; std::memcpy(&value,reinterpret_cast<void*>(address),sizeof(T)); return value; }
template<class T> void write(Addr address, T value) { checkMemory(address,sizeof(T)); std::memcpy(reinterpret_cast<void*>(address),&value,sizeof(T)); }
// 名称读取按可读区分块校验，不为每个字符重复调用VirtualQuery；不缓存跨调用的页或对象。
std::string readBoundedName(Addr address,const char* error) {
    std::string value;
    while(value.size()<240) {
        MEMORY_BASIC_INFORMATION region{};
        require(address && VirtualQuery(reinterpret_cast<void*>(address),&region,sizeof(region)) &&
            region.State==MEM_COMMIT && !(region.Protect&(PAGE_GUARD|PAGE_NOACCESS)),error);
        const auto base=reinterpret_cast<Addr>(region.BaseAddress);
        require(address>=base && address-base<region.RegionSize,error);
        const size_t count=std::min(size_t{240}-value.size(),region.RegionSize-static_cast<size_t>(address-base));
        const auto* data=reinterpret_cast<const char*>(address);
        const auto* end=static_cast<const char*>(std::memchr(data,0,count));
        value.append(data,end?static_cast<size_t>(end-data):count);
        if(end)return value;
        require(address+count>address,error);address+=count;
    }
    throw std::runtime_error(error);
}
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
    if(!runtimeEnabled || !supplyPlane)return false;
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
// 机库双选只排队一个原生决定/返回边沿，跳转、音效、存档校验仍由原生菜单执行。
using MenuQuery=bool(*)(void*,unsigned char);
MenuQuery originalMenuEdge=nullptr,originalMenuRepeat=nullptr; // 两个已核对的菜单输入查询入口。
using PreparationTick=void(*)(void*,float);
PreparationTick originalPreparationTick=nullptr; // 同一原生 Tick 结束时记录接收结果，避免跨帧漏掉跳转。
struct PreparationAction {
    Addr widget{}; // 请求限定到一个 CampaignHangarWeapon 实例。
    unsigned char action{}; // 1 为决定，2 为返回；零表示没有请求。
    bool delivered{}; // 一次边沿只交付一次，不因 Lua 与游戏帧率差异重复决定。
    bool accepted{},rejected{}; // 原生结果留存到 Lua 领取，不要求旧页继续存在。
} preparationAction;
bool preparationHooksReady=false; // 全局钩子不改其他页面的输入。

// 持有请求时隔离此页的真实按键，防止同一帧导航改变已确认的 SP1 行。
bool preparationEdge(void* object,unsigned char key) {
    if(!runtimeEnabled || reinterpret_cast<Addr>(object)!=preparationAction.widget)return originalMenuEdge(object,key);
    if(key!=preparationAction.action || preparationAction.delivered)return false;
    preparationAction.delivered=true;
    return true;
}
// 原生方向重复查询在交接期间暂停；其他页面仍使用游戏的查询结果。
bool preparationRepeat(void* object,unsigned char key) {
    return runtimeEnabled && reinterpret_cast<Addr>(object)==preparationAction.widget ? false : originalMenuRepeat(object,key);
}
// 在决定/返回所属的原生帧内检查结果；返回动作可能在下帧就关闭页面。
void preparationTick(void* object,float delta) {
    originalPreparationTick(object,delta);
    if(!runtimeEnabled || reinterpret_cast<Addr>(object)!=preparationAction.widget || !preparationAction.delivered ||
        preparationAction.accepted || preparationAction.rejected)return;
    unsigned char transition{}; // object 是刚刚执行完 Tick 的原生 UObject，尚在同一调用栈中。
    std::memcpy(&transition,reinterpret_cast<const unsigned char*>(object)+0x3c5,sizeof(transition));
    preparationAction.accepted=transition!=0;
    preparationAction.rejected=transition==0;
}
// 指纹和虚表均来自本 build 的只读现场采集；不在未知版本上尝试菜单动作。
void installPreparationHooks() {
    if(preparationHooksReady)return;
    const auto base=reinterpret_cast<Addr>(GetModuleHandleW(nullptr)); // 当前映像基址。
    constexpr unsigned char edge[]={0x48,0x8b,0x89,0x88,0x02,0,0,0xe9,0x34,0xa7,0,0};
    constexpr unsigned char repeat[]={0x48,0x8b,0x89,0x88,0x02,0,0,0xe9,0x34,0xa7,0,0};
    constexpr unsigned char tick[]={0x40,0x53,0x48,0x83,0xec,0x30,0x0f,0x29,0x74,0x24,0x20};
    require(std::memcmp(reinterpret_cast<void*>(base+0x61e020),edge,sizeof(edge))==0 &&
        std::memcmp(reinterpret_cast<void*>(base+0x61e030),repeat,sizeof(repeat))==0 &&
        std::memcmp(reinterpret_cast<void*>(base+0x6d4640),tick,sizeof(tick))==0,"机库输入查询或 Tick 入口不匹配");
    const auto status=MH_Initialize(); // 与其他核心钩子共用同一 MinHook 实例。
    require(status==MH_OK || status==MH_ERROR_ALREADY_INITIALIZED,"无法初始化机库输入钩子");
    require(MH_CreateHook(reinterpret_cast<void*>(base+0x61e020),reinterpret_cast<void*>(&preparationEdge),
        reinterpret_cast<void**>(&originalMenuEdge))==MH_OK,"无法接入机库决定/返回查询");
    require(MH_CreateHook(reinterpret_cast<void*>(base+0x61e030),reinterpret_cast<void*>(&preparationRepeat),
        reinterpret_cast<void**>(&originalMenuRepeat))==MH_OK,"无法接入机库方向查询");
    require(MH_CreateHook(reinterpret_cast<void*>(base+0x6d4640),reinterpret_cast<void*>(&preparationTick),
        reinterpret_cast<void**>(&originalPreparationTick))==MH_OK,"无法接入机库动作回执");
    require(MH_QueueEnableHook(reinterpret_cast<void*>(base+0x61e020))==MH_OK &&
        MH_QueueEnableHook(reinterpret_cast<void*>(base+0x61e030))==MH_OK &&
        MH_QueueEnableHook(reinterpret_cast<void*>(base+0x6d4640))==MH_OK && MH_ApplyQueued()==MH_OK,
        "无法启用机库输入交接");
    preparationHooksReady=true;
    nativeLog("PREPARATION_ACTION_HOOKS_READY");
}
// 选择音效只接管当前玩家的 NORMAL/SPECIAL，锁定、搜索、警告均保持原生路径。
struct AudioString {const wchar_t* data{};int count{},capacity{};}; // 本 build 的 16 字节 FString。
static_assert(sizeof(AudioString)==16,"FString 布局不匹配");
using WeaponAudio=void(*)(void*,AudioString*);
WeaponAudio originalWeaponAudio=nullptr; // 原生激活器的音效包装器，负责暂停/音量/焦点策略。
using FreeAudioString=void(*)(void*);
FreeAudioString freeAudioString=nullptr; // 原生 FString 所用分配器的配对释放入口。
struct SelectionAudio {Addr plane{},activator{};int last{};} selectionAudio; // 最后已提交的逻辑选择。
bool selectionAudioReady=false; // 挂钩后不再次读取已被改写的入口指纹。
// MSL/SP1 保持原生事件；SP2 独立播放本地 WAV，不替换共享音库媒体。
constexpr const wchar_t* selectionEvents[]={L"HUD_WEAPON_SELECT_NORMAL",L"HUD_WEAPON_SELECT_SPECIAL"};
dualsp::SelectionWave selectionWave; // 游戏线程持有，在解绑或音效错误时明确关闭。
using ToggleWeapon=bool(*)(void*); // 原生完整飞机入口调用的类别切换，保留其返回值与所有行为。
ToggleWeapon originalToggleWeapon=nullptr; // 原生类别切换；多SP用户请求由逻辑选择统一处理。
bool emptySwitchRequested=false; // 当前挂载耗尽，原生无法产生 armed 变化时的单次请求。
bool switchInputAllowed=false; // 使用既有任务、暂停和补给状态，在不可操作时清空请求。
bool cycleSwitchRequested=false; // 多SP循环先交给逻辑选择，不让原生MSL中间态进入HUD。
bool controlledWeaponSwitch=false; // 模组自己的完整飞机切换不再被当成用户循环。
struct ControlledWeaponSwitch {
    bool previous{controlledWeaponSwitch};
    ControlledWeaponSwitch(){controlledWeaponSwitch=true;}
    ~ControlledWeaponSwitch(){controlledWeaponSwitch=previous;}
};

// 只拦截完整飞机输入入口的调用；原生耗尽/回收回调继续执行自己的类别逻辑。
bool observeEmptyWeaponSwitch(void* object) {
    const auto activator=reinterpret_cast<Addr>(object); // 仅监听已绑定的当前玩家。
    const auto caller=reinterpret_cast<Addr>(__builtin_return_address(0));
    const auto inputCaller=reinterpret_cast<Addr>(GetModuleHandleW(nullptr))+0x823999;
    if(runtimeEnabled && switchInputAllowed && !controlledWeaponSwitch &&
        caller==inputCaller &&
        activator==selectionAudio.activator && session.activator==activator &&
        session.plane==selectionAudio.plane && session.count>1) {
        cycleSwitchRequested=true;
        return false; // 类别尚未改变，飞机入口也不应按一次成功切换重绑锁定组件。
    }
    if(runtimeEnabled && switchInputAllowed && !controlledWeaponSwitch && activator==selectionAudio.activator &&
        read<unsigned char>(activator+0x2a1)==0 && read<int>(groupOf(activator,true)+8)==0)
        emptySwitchRequested=true;
    return originalToggleWeapon(object);
}

// 调用方提供原生 FString；只匹配两个完整事件名，拒绝前缀或模糊拦截。
bool isSelectionEvent(const AudioString& event) {
    for(int index=0;index<2;++index) {
        const auto length=std::wcslen(selectionEvents[index])+1; // 包含终止符，排除相似事件。
        if(event.data && event.count==static_cast<int>(length) && event.capacity>=event.count &&
            std::memcmp(event.data,selectionEvents[index],length*sizeof(wchar_t))==0)return true;
    }
    return false;
}
// 原生中间态事件被收回；实际声音由最终逻辑选择提交一次，不能叠加 A/B/C。
void weaponSelectionAudio(void* object,AudioString* event) {
    if(runtimeEnabled && reinterpret_cast<Addr>(object)==selectionAudio.activator && isSelectionEvent(*event)) {
        freeAudioString(const_cast<wchar_t*>(event->data)); // 替代原包装器必须承担其 FString 释放职责。
        *event={};return;
    }
    originalWeaponAudio(object,event);
}
// 原生选择音钩子仍只负责 MSL/SP1 的中间态隔离。
void installSelectionAudio() {
    if(selectionAudioReady)return;
    const auto base=reinterpret_cast<Addr>(GetModuleHandleW(nullptr)); // 固定 build 的原生入口。
    constexpr unsigned char post[]={0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x74,0x24,0x10,0x57,0x48,0x83,0xec,0x40};
    constexpr unsigned char copy[]={0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x74,0x24,0x10,0x57,0x48,0x83,0xec,0x20};
    constexpr unsigned char release[]={0x48,0x85,0xc9,0x74,0x1d,0x4c,0x8b,0x05,0x44,0xa6,0x12,0x03};
    require(std::memcmp(reinterpret_cast<void*>(base+0x885800),post,sizeof(post))==0 &&
        std::memcmp(reinterpret_cast<void*>(base+0x3e4f40),copy,sizeof(copy))==0 &&
        std::memcmp(reinterpret_cast<void*>(base+0x99af20),release,sizeof(release))==0,"选择音效或字符串接口不匹配");
    constexpr unsigned char toggle[]={0x48,0x8b,0xc4,0x55,0x53,0x48,0x8b,0xec,0x48,0x83,0xec,0x68}; // 现场核对的 ToggleSpecial 前导。
    constexpr unsigned char inputCall[]={0xe8,0xb7,0x32,0x06,0}; // 0x823994→0x886c50；回址0x823999。
    require(std::memcmp(reinterpret_cast<void*>(base+0x886c50),toggle,sizeof(toggle))==0 &&
        std::memcmp(reinterpret_cast<void*>(base+0x823994),inputCall,sizeof(inputCall))==0,"原生切换请求入口不匹配");
    freeAudioString=reinterpret_cast<FreeAudioString>(base+0x99af20);
    const auto status=MH_Initialize(); // 与输入钩子使用同一实例。
    require(status==MH_OK || status==MH_ERROR_ALREADY_INITIALIZED,"无法初始化选择音效钩子");
    require(MH_CreateHook(reinterpret_cast<void*>(base+0x885800),reinterpret_cast<void*>(&weaponSelectionAudio),
        reinterpret_cast<void**>(&originalWeaponAudio))==MH_OK,"无法接入玩家选择音效");
    require(MH_EnableHook(reinterpret_cast<void*>(base+0x885800))==MH_OK,"无法启用玩家选择音效");
    require(MH_CreateHook(reinterpret_cast<void*>(base+0x886c50),reinterpret_cast<void*>(&observeEmptyWeaponSwitch),
        reinterpret_cast<void**>(&originalToggleWeapon))==MH_OK,"无法监听耗尽兵装的切换请求");
    require(MH_EnableHook(reinterpret_cast<void*>(base+0x886c50))==MH_OK,"无法启用兵装切换请求监听");
    selectionAudioReady=true;nativeLog("SELECTION_AUDIO_READY");
}

// 通过原生字符串复制器取得游戏分配的缓冲区，由原包装器消费并释放。
void playSelection(int target) {
    if(target==2){selectionWave.play();return;}
    // SP3 明确复用原生 SP 提示音；没有第三份语音素材，不伪造 SP3 语音。
    if(target==3)target=1;
    selectionWave.stop(); // 换回 MSL/SP1 时结束自定义提示尾音。
    const auto base=reinterpret_cast<Addr>(GetModuleHandleW(nullptr)); // 所有调用保持在游戏线程。
    const int length=static_cast<int>(std::wcslen(selectionEvents[target])+1); // FString 的 Num 包含零终止符。
    const AudioString source{selectionEvents[target],length,length}; // 只读的非持有源。
    AudioString owned{}; // 只传持有游戏内存的副本，不能让游戏释放字符串字面量。
    using Copy=AudioString*(*)(AudioString*,const AudioString*);
    reinterpret_cast<Copy>(base+0x3e4f40)(&owned,&source);
    originalWeaponAudio(reinterpret_cast<void*>(selectionAudio.activator),&owned);
}
// 在原生初始化丢弃旧锁定槽之前，逐槽释放其搜索/锁定音引用。
// 不能只清零计数或发送全局静音；这会丢失原生播放与停止的配对关系。
void finishSeekerAudio(Addr plane,Addr activator) {
    Addr seeker{};
    std::array<int,4> before{}; // 四种音效的引用计数，只用于检查和记录。
    {
        const NativeReadBatch memory;
        require(memory.read<Addr>(plane+0x11a8)==activator,"音效收尾的机体与激活器不匹配");
        seeker=memory.read<Addr>(plane+0x1310);
        require(seeker && memory.read<Addr>(seeker+0x378)==plane,"音效收尾缺少当前玩家锁定组件");
        before=memory.read<std::array<int,4>>(seeker+0x4f0);
        for(const int count:before)require(count>=0 && count<=512,"锁定音引用计数无效");
    } // 原生停音会调用引擎，不能保留前面的可读区间。
    const auto base=reinterpret_cast<Addr>(GetModuleHandleW(nullptr)); // 原生逐槽音效收尾函数。
    constexpr unsigned char stop[]={0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x74,0x24,0x10,0x57,0x48,0x83,0xec,0x20,
        0x48,0x8b,0x99,0x68,0x04,0,0};
    require(std::memcmp(reinterpret_cast<void*>(base+0x4c98a0),stop,sizeof(stop))==0,"原生锁定音收尾入口不匹配");
    using Stop=void(*)(void*); // 原生内部按每个槽的播放标记发送对应停止事件。
    reinterpret_cast<Stop>(base+0x4c98a0)(reinterpret_cast<void*>(seeker));
    for(const int count:read<std::array<int,4>>(seeker+0x4f0))
        require(count==0,"旧锁定音引用已失配，请重新开始任务；停止重建兵装");
    if(std::any_of(before.begin(),before.end(),[](int count){return count!=0;}))
        nativeLog(Json{{"event","SEEKER_AUDIO_RELEASED"},{"before",before}}.dump());
}
bool continuousElectronicClass(Addr cls); // 后文持续兵装模块定义，供TLS旧入口排除被重用的编号。
// 只结束当前玩家发出的 TLS，走原生收尾以同时停止伤害、粒子和声源。
void finishPlayerTLS(Addr plane,Addr activator) {
    require(read<Addr>(plane+0x11a8)==activator,"TLS 收尾的玩家身份不匹配");
    const auto base=reinterpret_cast<Addr>(GetModuleHandleW(nullptr)); // 原生自然终止请求入口。
    constexpr unsigned char end[]={0x80,0xb9,0x68,0x03,0,0,0,0x74,0x0d,0x80,0xb9,0xc0,0x05,0,0,0,0x0f,0x84,0x2a,0x14,0,0,0xc3};
    require(std::memcmp(reinterpret_cast<void*>(base+0x894af0),end,sizeof(end))==0,"TLS 原生收尾入口不匹配");
    const auto data=read<Addr>(activator+0x338); // 反射确认的 ActiveWeapons，元素是 8 字节对象指针。
    const int count=read<int>(activator+0x340),capacity=read<int>(activator+0x344); // 不遍历备用容量。
    require(count>=0 && count<=capacity,"活动武器数组无效");
    if(!count)return;
    checkMemory(data,static_cast<size_t>(count)*sizeof(Addr));
    std::vector<Addr> lasers; // 结束回调会移除活动数组元素，必须先固定本轮对象列表。
    for(int index=0;index<count;++index) {
        const auto weapon=read<Addr>(data+index*sizeof(Addr)); // 只读玩家激活器已经登记的武器。
        if(!weapon || read<unsigned char>(weapon+0x3b0)!=25 || read<Addr>(weapon+0x3b8)!=plane)continue;
        if(continuousElectronicClass(read<Addr>(weapon+0x10)))continue;
        if(!read<unsigned char>(weapon+0x368) || read<unsigned char>(weapon+0x5c0))continue;
        // 玩家 plwp_tls_x2 继承 AttachedInstantWeapon，不能用 LaserWeapon 的 Tick 排除它。
        // 两者共享下面三个收尾入口；校验真正要调用的生命周期，不限定不同的更新函数。
        const auto table=read<Addr>(weapon); // 已匹配 TLS 编号和当前玩家归属的活动对象。
        const auto end=read<Addr>(table+0x628),release=read<Addr>(table+0x648),detach=read<Addr>(table+0x680); // 完整收尾链。
        if(end!=base+0x85f1c0 || release!=base+0x894850 || detach!=base+0x861490)
            throw std::runtime_error(Json{{"error","TLS 原生收尾接口不匹配"},{"vtable_rva",table-base},
                {"end_rva",end-base},{"release_rva",release-base},{"detach_rva",detach-base}}.dump());
        lasers.push_back(weapon);
    }
    using End=void(*)(void*); // 与 TLS 丢失所属机体时使用同一个自然终止请求。
    for(const auto weapon:lasers)reinterpret_cast<End>(base+0x894af0)(reinterpret_cast<void*>(weapon));
    if(!lasers.empty())nativeLog("TLS_ENDED_ON_SWITCH "+std::to_string(lasers.size()));
}
// 发射槽是原生动态池，不等于机体挂点数；ADMM 现场为 Num=126、Max=170。
struct WeaponSlotArray { Addr data{}; int count{}; }; // 校验后只暴露有效元素，不读备用容量。
WeaponSlotArray weaponSlotArray(Addr group) {
    const NativeReadBatch memory;
    const auto data=memory.read<Addr>(group+0x58); // 原生 TArray 数据区。
    const int count=memory.read<int>(group+0x60),capacity=memory.read<int>(group+0x64); // 数量与已分配容量。
    const int window=memory.read<int>(group+0x10),next=memory.read<int>(group+0x14); // 允许装填的窗口和发射游标。
    if(!(count>0 && count<=capacity && window>=0 && window<=count && next>=0 && next<count)) {
        // 只在失败时生成诊断，避免 HUD 每次采样都拼接 JSON。
        throw std::runtime_error(Json{{"error","武器槽数组或装填队列无效"},{"count",count},
            {"capacity",capacity},{"window",window},{"next",next}}.dump());
    }
    // 以真实分配区校验完整数组，撤除误把容量限制为 64 的机型假设。
    checkMemory(data,static_cast<size_t>(capacity)*0x60);
    return {data,count};
}
std::string mountNameIndex(unsigned index);
bool debugReloadEligible(Addr group) {
    const auto& cached=session.special[session.active];
    if(cached.valid && cached.weaponClass==read<Addr>(group))return cached.fastReloadEligible;
    const auto weapon=read<Addr>(group+0x40);
    if(!weapon || read<unsigned char>(weapon+0x451))return false;
    const auto id=read<unsigned char>(weapon+0x3b0);
    const auto name=mountNameIndex(read<unsigned>(weapon+0x18));
    return id!=17 && id!=22 && id!=25 && id!=26 && id!=31 &&
        name.find("plwp_hgp")==std::string::npos;
}
float cachedReload(const Group& group,const Slot& slot,double dt,bool inWindow) {
    float elapsed=slot.elapsed;
    if(debugFastReload && group.fastReloadEligible && inWindow && elapsed>=0 && elapsed<slot.duration)
        elapsed=std::max(elapsed,slot.duration-1.f);
    return dualsp::advanceReload(slot.duration,elapsed,dt,inWindow);
}
// 逐槽保存冷却，包含原生未启用槽的负计时；不改变池数量和装填窗口。
Group capture(Addr activator, bool special, double time) {
    const auto address=groupOf(activator,special);
    Group result;
    result.weaponClass=read<Addr>(address);
    result.fastReloadEligible=special && debugReloadEligible(address);
    checkMemory(address+8,result.scalars.size());
    std::memcpy(result.scalars.data(),reinterpret_cast<void*>(address+8),result.scalars.size());
    const auto slots=weaponSlotArray(address); // 所有机型共用结构及内存校验。
    const auto slotData=slots.data;const int num=slots.count; // 不遍历 TArray 的备用容量。
    result.slots.reserve(num);
    for(int i=0;i<num;++i) {
        // 本次同步采样已验证完整数组，循环内不调用引擎；批量读相邻计时，避免每槽重复查页。
        std::array<float,2> timers{};
        std::memcpy(timers.data(),reinterpret_cast<void*>(slotData+static_cast<size_t>(i)*0x60+0x48),sizeof(timers));
        Slot slot{timers[0],timers[1]};
#ifdef DUALSP_SU30SM_MOUNT_PROTOTYPE
        slot.mountOrdinal=i; // 新建逻辑队列按资源顺序编号。
        if(special) {
            slot.mountOrdinal=fixedMountOrdinal(result.weaponClass,i);
            slot.elapsed=fixedMountElapsed(i,slot.duration,slot.elapsed);
        }
#endif
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
    // 原生难度/Mod的负弹药计数均视为无限；真实装填窗口、游标和计时仍须有效。
    require(dualsp::validAmmoCount(ammo) && window>=0 && window<=count && next>=0 && next<count,"HUD 武器队列无效");
    require(std::isfinite(time) && time+0.001>=group.capturedTime,"HUD 游戏时间回退，需先恢复检查点");
    if(ammo==0)return false;
    // 未激活 SP 只推进本来允许装填的窗口；活动组捕获时间就是本次时间。
    const double delta=std::max(0.0,time-group.capturedTime);
    for(int index=0;index<window;++index) {
        const auto& slot=group.slots[(next+index)%count];
        const float elapsed=cachedReload(group,slot,delta,true);
        if(elapsed>=slot.duration)return true;
    }
    return false;
}
#ifdef DUALSP_SU30SM_MOUNT_PROTOTYPE
#include "su30sm_fixed_mount.hpp"
#endif
void restore(Addr activator, bool special, const Group& saved, double time) {
    require(saved.valid,"Uninitialized cached weapon");
    const auto address=groupOf(activator,special);
    require(read<Addr>(address)==saved.weaponClass,"Weapon class changed");
    const auto slots=weaponSlotArray(address); // 恢复前也校验新建池，不能只检查数量相等。
    const auto slotData=slots.data;const int num=slots.count;
    require(num==static_cast<int>(saved.slots.size()),"Weapon slot count changed");
    const int loadedCount=scalarInt(saved,8);   // group+0x10
    const int nextSlot=scalarInt(saved,12);     // group+0x14
    require(loadedCount>=0 && loadedCount<=num && nextSlot>=0 && nextSlot<num,"Invalid cached launch cursor");
    require(time+0.001>=saved.capturedTime,"Game time rewound; checkpoint restoration required");
    const double dt=std::max(0.0,time-saved.capturedTime);
    // 完整数组已核可读范围；本同步批次不调用引擎，集中读取和计算，全部成功后才写回。
    // ADMM有126槽，避免每槽重复查询同一页、重复核对同一装填时长。
    std::vector<std::array<float,2>> reloads;reloads.reserve(num);
    for(int i=0;i<num;++i) {
        const auto& slot=saved.slots[(nextSlot+i)%num];
        std::array<float,2> native{};
        std::memcpy(native.data(),reinterpret_cast<const void*>(slotData+static_cast<size_t>(i)*0x60+0x48),sizeof(native));
        require(std::abs(native[0]-slot.duration)<0.001f,"Reload configuration changed");
        const float logical=cachedReload(saved,slot,dt,i<loadedCount);
        reloads.push_back({logical,dualsp::rebuildReload(slot.duration,logical,native[1])});
    }
    // Only the current load window progresses. Advancing every physical hardpoint
    // would create extra ready missiles and violate the native loaded-count cap.
    checkMemory(address+8,saved.scalars.size());
    std::memcpy(reinterpret_cast<void*>(address+8),saved.scalars.data(),saved.scalars.size());
    // Initialization rebuilds the native projectile pool with hardpoints 0..N-1
    // active. Restore the logical loading queue into that fresh pool, rather than
    // restoring a cursor whose old projectile instances no longer exist.
    write<int>(address+0x14,0);
    for(int i=0;i<num;++i) {
        // 保留原生下一帧跨过装填门槛的行为，不提前将新建槽冒充为已生成弹体。
        const float elapsed=reloads[i][1];
#ifdef DUALSP_SU30SM_MOUNT_PROTOTYPE
        fixedMountRecordReload(special,i,reloads[i][0],elapsed,saved.slots[(nextSlot+i)%num].duration);
#endif
        std::memcpy(reinterpret_cast<void*>(slotData+static_cast<size_t>(i)*0x60+0x4c),&elapsed,sizeof(elapsed));
    }
#ifdef DUALSP_SU30SM_MOUNT_PROTOTYPE
    if(special)fixedMountRestoreOrder(saved,nextSlot); // 与冷却队列使用完全相同的旋转。
#endif
}
#ifdef DUALSP_SU30SM_MOUNT_PROTOTYPE
// 原生换装先同步飞机及飞行引擎编号，再重建武器；只重建池会让舱门仍按旧 SP 响应。
void syncPlaneWeaponSelection(Addr plane,int index) {
    const Addr base=reinterpret_cast<Addr>(GetModuleHandleW(nullptr)); // 本构建只读采样确认的原生赋值入口。
    constexpr unsigned char setter[]={0x48,0x8b,0x41,0x08,0x89,0x90,0x08,0x11,0,0,0xc3}; // 仅更新引擎配置编号，不重置动画。
    require(index>=0 && index<3 && std::memcmp(reinterpret_cast<void*>(base+0x1cd4620),setter,sizeof(setter))==0,"兵装编号同步：原生入口或目标无效");
    const Addr engine=read<Addr>(plane+0x1410); // 飞行引擎包装器 plane+0x1408 内的实例指针。
    const int oldPlane=read<int>(plane+0xb4c),oldEngine=read<int>(engine+0x1108); // PlaneSWP 已由反射确认。
    require(oldPlane>=-1 && oldPlane<3 && oldEngine>=-1 && oldEngine<3,"兵装编号同步：原生状态越界");
    checkMemory(plane+0xb4c,sizeof(int));checkMemory(engine+0x1108,sizeof(int));
    if(oldPlane==index && oldEngine==index)return;
    using SetSelection=void(*)(void*,int); // 与原生换装函数的前两步相同，不调用其补弹或任务恢复分支。
    write<int>(plane+0xb4c,index);
    reinterpret_cast<SetSelection>(base+0x1cd4620)(reinterpret_cast<void*>(plane+0x1408),index);
    require(read<int>(plane+0xb4c)==index && read<int>(engine+0x1108)==index,"兵装编号同步：写入后状态不符");
    nativeLog(Json{{"event","PLANE_SWP_SYNCHRONIZED"},{"old_plane",oldPlane},{"old_engine",oldEngine},{"target",index}}.dump());
}
#endif
#include "continuous_sp.hpp"
#include "seeker_rebuild.hpp"
#include "cfa_fire_transition.hpp"
#include "cfa_weapon_diagnostics.hpp"
#include "raven_sp.hpp"
#include "target_gaze.hpp"
#include "camera_hold.hpp"
#include "view_target.hpp"
void debugWeaponsTick(Addr activator) noexcept;
#include "gun_salvo.hpp"
#include "debug_weapons.hpp"

// 只计当前CFA切换，九个连续区间覆盖核心执行至回执发布；日志在计时结束后写一次。
struct CfaSwitchCost {
    bool active{};uint64_t start{},last{};
    std::array<uint64_t,9> ticks{};
    std::array<bool,9> reached{};
    std::array<std::array<uint64_t,4>,9> reads{};
    int target{-1};bool models{};
#ifdef DUALSP_SU30SM_MOUNT_PROTOTYPE
    std::array<CfaMuzzleCost::Sample,9> muzzle{};
    size_t muzzleRecordsBefore{};
#endif
    static uint64_t now() { LARGE_INTEGER value{};QueryPerformanceCounter(&value);return value.QuadPart; }
    void begin(bool enabled) {
        *this={};active=enabled;
        nativeReadCost={};nativeReadCost.active=active;
        cfaWorkCosts={};
#ifdef DUALSP_SU30SM_MOUNT_PROTOTYPE
        cfaMuzzleCost={};cfaMuzzleCost.active=active;muzzleRecordsBefore=cfaMuzzleVisuals.size();
#endif
        if(active)start=last=now();
    }
    void mark(unsigned stage) {
        if(!active)return;
        const auto time=now();ticks.at(stage)=time-last;reached.at(stage)=true;last=time;
        reads.at(stage)={nativeReadCost.calls,nativeReadCost.ticks,nativeReadCost.batchReads,nativeReadCost.batchReused};
        nativeReadCost.calls=nativeReadCost.ticks=nativeReadCost.batchReads=nativeReadCost.batchReused=0;
#ifdef DUALSP_SU30SM_MOUNT_PROTOTYPE
        muzzle.at(stage)=cfaMuzzleCost.sample;cfaMuzzleCost.sample={};
#endif
    }
    void finish(uint64_t request,bool ok) noexcept {
        if(!active)return;
        const auto end=now();active=false;
        nativeReadCost.active=false;
#ifdef DUALSP_SU30SM_MOUNT_PROTOTYPE
        cfaMuzzleCost.active=false;
#endif
        try {
            LARGE_INTEGER frequency{};QueryPerformanceFrequency(&frequency);
            if(frequency.QuadPart<=0)return;
            const double ms=1000.0/double(frequency.QuadPart);
            const char* names[]={"capture","retain_effects","mount_before","audio_selection","native_init",
                "restore_effects","reload_armed","mount_commit","response_publish"};
            Json stages=Json::object();
            for(unsigned i=0;i<ticks.size();++i)if(reached[i])stages[names[i]]=double(ticks[i])*ms;
            Json result{{"event","CFA_SWITCH_COST"},{"request",request},{"ok",ok},
                {"total_ms",double(end-start)*ms},{"stages_ms",stages},{"target_config",target},{"models",models},
                {"profile_revision",4}};
            Json checks=Json::object();
            for(unsigned i=0;i<reads.size();++i)if(reached[i])checks[names[i]]={
                {"calls",reads[i][0]},{"ms",double(reads[i][1])*ms},
                {"batch_reads",reads[i][2]},{"batch_reused",reads[i][3]}};
            result["memory_checks"]=checks; // 各段的子集，不能与stages_ms相加。
            const char* workNames[]={"layout_audit","bones_return","rack_return","bones_maintain",
                "rack_maintain","commit_classify","parts_bind","muzzle_sync"};
            Json work=Json::object();
            for(unsigned i=0;i<cfaWorkCosts.size();++i)work[workNames[i]]={
                {"calls",cfaWorkCosts[i].calls},{"ms",double(cfaWorkCosts[i].ticks)*ms}};
            result["mount_work"]=work; // 与memory_checks重叠，只用于定位，不能相加。
#ifdef DUALSP_SU30SM_MOUNT_PROTOTYPE
            Json filterStages=Json::object();
            for(unsigned i=0;i<muzzle.size();++i)if(reached[i])filterStages[names[i]]={
                {"ms",double(muzzle[i].ticks)*ms},{"calls",muzzle[i].calls},{"hits",muzzle[i].hits}};
            result["muzzle_filter"]={{"stages",filterStages},{"records_before",muzzleRecordsBefore},
                {"records_after",cfaMuzzleVisuals.size()}};
#endif
            nativeLog(result.dump());
        }catch(...) { } // 计时记录失败不影响已经完成的兵装事务。
    }
} cfaSwitchCost;

void initialize(const Command& command,int index,bool preserveAuxiliary=true) {
    cfaSwitchCost.mark(0);
    ControlledWeaponSwitch controlled;
    ravenBeforeRebuild(command);
    ContinuousRebuild continuous(command); // 电子战不限定机型，TLS/UAV沿用既有真实类限制。
    cfaSwitchCost.mark(1);
#ifdef DUALSP_SU30SM_MOUNT_PROTOTYPE
    // 逻辑槽先映射为原生配置；挂载模块独立保存舱门和重建状态。
    const auto mountRebuild=fixedMountBeforeRebuild(command,index,session);
#endif
    cfaSwitchCost.mark(2);
    prepareSeekerRebuild(command.plane,command.activator); // 先解除旧锁定登记及声音，再丢弃旧槽。
    const int gun=read<int>(stateOf(command.activator)+8);
    const auto owner=read<Addr>(command.activator+0x260);
    const int flares=read<int>(owner+0x10c);
    using Init=void(*)(void*,int);
    const auto base=reinterpret_cast<Addr>(GetModuleHandleW(nullptr));
#ifdef DUALSP_SU30SM_MOUNT_PROTOTYPE
    syncPlaneWeaponSelection(command.plane,index); // 保持原生选择编号与本次真实配置一致，舱门继续由原生更新。
#endif
    continuousRebuildStarted=true;
    continuous.rebuilding=true;
    cfaSwitchCost.mark(3);
    {
#ifdef DUALSP_SU30SM_MOUNT_PROTOTYPE
        MountAnimationNativeRebuild animationRebuild(command.plane);
#endif
        reinterpret_cast<Init>(base+0x81e810)(reinterpret_cast<void*>(command.plane),index);
#ifdef DUALSP_SU30SM_MOUNT_PROTOTYPE
        animationRebuild.returned=true;
#endif
    }
    cfaSwitchCost.mark(4);
    if(command.count>1) {
        const Addr cdo=read<Addr>(groupOf(command.activator,true)+0x40);
        const auto support=continuousSupport(command.plane,cdo);
        if(support.electronic && support.kind==ContinuousKind::None) {
            nativeLog(Json{{"event","MOD_ELECTRONIC_RUNTIME_REJECTED"},{"reason",support.reason},{"config",index}}.dump());
            require(false,"电子战兵装：当前原生回调或装填机制不支持多SP，详见native.log");
        }
        if(read<unsigned char>(cdo+0x3b0)==26 && !support.electronic)
            require(support.kind==ContinuousKind::UAV && continuousRavenLoadout(command.activator),
                "UAV多SP仍只支持已登记的原生配置");
    }
    ravenAfterRebuild(command);
#ifdef DUALSP_SU30SM_MOUNT_PROTOTYPE
    fixedMountAfterRebuild(mountRebuild);
#endif
    // A loadout rebuild also resets the machine gun. Preserve its ammunition,
    // plus flare count, during an SP change; a deliberate resupply uses defaults.
    if(preserveAuxiliary) {
        write<int>(stateOf(command.activator)+8,gun);
        write<int>(read<Addr>(command.activator+0x260)+0x10c,flares);
    }
    continuous.restoreReferences(); // 原生强引用恢复后，同栈释放临时 root。
#ifdef DUALSP_SU30SM_MOUNT_PROTOTYPE
    if(command.op=="setup" && index==1 && fixedMountEnabled && !fixedMountFailed && command.count>=2 &&
        std::find(command.configs.begin(),command.configs.begin()+command.count,0)!=command.configs.begin()+command.count &&
        mountAircraft(command.plane)==MountAircraft::Fa44)
        mountCfaMuzzlePrewarm(command.plane,command.request);
#endif
    cfaSwitchCost.mark(5);
}
void keepArmed(const Command& command,bool armed) {
    ControlledWeaponSwitch controlled;
#ifdef DUALSP_SU30SM_MOUNT_PROTOTYPE
    MountAnimationArmedCompletion animationCompletion(command);
#endif
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
#ifdef DUALSP_SU30SM_MOUNT_PROTOTYPE
    mountAnimationAfterArmed(command,armed);
    mountLightAnimationAfterArmed(command,armed);
    animationCompletion.completed=true;
#endif
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
    for(int i=0;i<session.count;++i) {
        if(i)out<<',';
        require(i==session.active || session.special[i].valid,"SP库存缓存未建立，不能生成弹药快照");
        out<<(i==session.active?scalarInt(special,0):scalarInt(session.special[i],0)); // 缺失缓存不能再用负弹药占位。
    }
    out<<']';
    return out.str();
}
Command readCommand() {
    std::ifstream file(modRoot()/"native-command.txt");
    Command command;
    file>>command.op>>std::hex>>command.plane>>command.activator>>std::dec>>command.target>>command.time;
    require(static_cast<bool>(file),"Malformed command");
    // 协议 4 显式携带配弹比例；旧核心与新 Lua 不能混装。
    int protocol{}; // 独立于管理 UI 协议的核心命令标记。
    file>>protocol>>command.count>>command.configs[0]>>command.configs[1]>>command.configs[2]>>command.request>>command.ratio;
    require(static_cast<bool>(file) && protocol==4,"需要配弹比例核心命令协议 4");
    require(command.count>=1 && command.count<=3,"SP 数量必须为 1 至 3");
    const double minimum=command.count==1?1:command.count==2?.5:.33;
    require(std::isfinite(command.ratio) && command.ratio>=minimum && command.ratio<=1,"SP 配弹比例越界");
    require(std::abs(command.ratio*100-std::round(command.ratio*100))<.000001,"SP 配弹比例必须为整数百分比");
    require(command.target>=0 && command.target<command.count,"目标 SP 不在已选槽范围内");
    for(int index=0;index<3;++index) {
        const int config=command.configs[index]; // 只允许连续、不同的原生兵装配置。
        require(index<command.count ? config>=0 && config<3 : config==-1,"SP 配置与槽数不符");
        for(int prior=0;prior<index && index<command.count;++prior)
            require(config!=command.configs[prior],"SP 配置重复");
    }
    require(std::isfinite(command.time) && command.time>=0,"Invalid game time");
    require(command.request>0,"Missing native request ID");
    return command;
}
// 在原生部件计算之后缩减当前/最大弹药；记录输入输出，便于增弹部件的对照验证。
void scaleAmmo(Addr activator,int count,double ratio) {
    require(count>=1 && count<=3,"配弹槽数无效");
    const double minimum=count==1?1:count==2?.5:.33;
    require(std::isfinite(ratio) && ratio>=minimum && ratio<=1,"SP 配弹比例越界");
    const int percent=static_cast<int>(std::round(ratio*100)); // 界面按整数百分比调节。
    require(std::abs(ratio*100-percent)<.000001,"SP 配弹比例必须为整数百分比");
    const auto group=groupOf(activator,true);
    std::array<int,2> before{},after{}; // 当前弹数与最大弹数，各自按同一比例向上取整。
    size_t index=0; // 对应两个弹药字段，而不是两个逻辑 SP。
    for(auto offset:{8,12}) {
        const int ammo=read<int>(group+offset);
        require(dualsp::validAmmoCount(ammo),"Invalid SP ammunition");
        before[index]=ammo;after[index]=ammo<0?ammo:(ammo*percent+99)/100; // 无限保留原始负值；有限值向上取整，零仍为零。
        write<int>(group+offset,after[index]);++index;
    }
    nativeLog(Json{{"event","SP_AMMO_SCALED"},{"count",count},{"percent",percent},{"native",before},{"scaled",after}}.dump());
}
// 调试换装只能重建当前会话；发射体还在活动数组时要求先恢复游戏等待结束。
void requireDebugLoadoutReady(Addr plane,Addr activator) {
    require(session.plane==plane && session.activator==activator,"调试换装的玩家会话已经改变");
    require(read<Addr>(plane+0x11a8)==activator,"调试换装的武器组件不属于当前机体");
    continuousSweep();
    require(std::none_of(continuousEffects.begin(),continuousEffects.end(),[plane,activator](const auto& e){
        return e.plane.address==plane && e.activator.address==activator;}),"持续效果或 TLS 尾迹尚未退场，请等待后再调试换装");
    const int count=read<int>(activator+0x340),capacity=read<int>(activator+0x344); // 原生 ActiveWeapons 有效数量。
    require(count>=0 && count<=capacity,"调试换装的活动武器数组无效");
    if(continuousRaven(plane) && continuousRavenLoadout(activator)) {
        // 原生 UAV 结束后可留重复项；只豁免已确认不活动的原生 UAV，其余仍拒绝。
        const auto active=continuousArray(activator+0x338);
        for(int i=0;i<active.count;++i) {
            const Addr weapon=read<Addr>(active.data+i*sizeof(Addr));
            require(weapon && read<Addr>(weapon+0x3b8)==plane && read<unsigned char>(weapon+0x3b0)==26 &&
                !read<unsigned char>(weapon+0x368) && continuousSupport(plane,weapon).kind==ContinuousKind::UAV,
                "仍有已发射的活动武器，请等待结束后再调试换装");
        }
    }else require(count==0,"仍有已发射的活动武器，请关闭管理界面、等待武器结束后重试");
    { // 所有机型都检查电子战尾迹，不让未登记机型绕过调试换装的退场门槛。
        const auto spawned=continuousArray(activator+0x2e8);
        for(int i=0;i<spawned.count;++i) {
            const Addr weapon=read<Addr>(spawned.data+i*sizeof(Addr));
            require(!weapon || !read<unsigned char>(weapon+0x368) || !continuousSupported(plane,weapon),
                "持续效果或 TLS 尾迹尚未退场，请等待后再调试换装");
        }
    }
}
// 保留其他机体既有的原生忙碌判断，统一入口另叠加CFA批次边界和Falken退场门槛。
bool weaponSwitchReady(Addr activator) {
    return read<unsigned char>(activator+0x2a0)==0 &&
        !(read<unsigned char>(activator+0x2c0)!=0 && read<int>(activator+0x2e0)>1);
}
// CFA须批次完整收尾，Falken须TLS自然退场；该查询只读，不改发射状态。
const char* weaponSwitchBlockReason(Addr plane,Addr activator) {
    if(continuousFalkenTLSBusy(plane,activator))return "falken_tls_active";
    if(cfaFireTransitionBusy(plane,activator))return "native_busy";
    if(!weaponSwitchReady(activator))return "native_busy";
    return nullptr;
}
void execute(const Command& command) {
    verifyVersion();
    require(continuousRuntimeFailure.empty(),continuousRuntimeFailure.c_str());
    require(read<Addr>(command.plane+0x11a8)==command.activator,"Activator is not owned by this plane");
    if(command.op!="snapshot" && command.op!="checkpoint_save") {
        cfaWeaponDiagnostic.sampled=false;
        if(command.op!="switch")cfaWeaponDiagnostic.admmObserved=false; // 换装/补给/检查点不沿用旧发射历史。
    }
    continuousSweep();
    if(command.op=="setup" || command.op=="resupply_setup" || command.op=="checkpoint_restore" || command.op=="resupply")
        continuousDrain(command.plane,command.activator); // 活动效果不进入弹药/检查点快照。
    // 补给重选需要重建当前组合，同时保留任务已经捕获的检查点。
    // 此新命令只存在于源码候选；正式 DLL 保持不变，不能直接混装。
    if(command.op=="setup" || command.op=="resupply_setup" || command.op=="debug_setup") {
        const bool debugLoadout=command.op=="debug_setup"; // 调试仅补满新的 SP，保留普通兵装与任务检查点。
        Group previousMain; // 初始化前保存普通导弹弹药和装填进度。
        if(debugLoadout) {
            requireDebugLoadoutReady(command.plane,command.activator);
            previousMain=capture(command.activator,false,command.time);
        }
#ifdef DUALSP_SU30SM_MOUNT_PROTOTYPE
        fixedMountRelease(); // 新组合不沿用上一组合的视觉状态。
#endif
        session={}; session.plane=command.plane; session.activator=command.activator; session.active=0;
        session.gameThread=GetCurrentThreadId();
        session.count=command.count;session.configs=command.configs;session.ratio=command.ratio;
        if(command.op=="setup")checkpointValid=false;
        // 第一槽与其余槽走同一初始化入口，包含单挂；重建次数与原 Lua 路径相同。
        initialize(command,session.configs[0],debugLoadout);
        session.main=debugLoadout?previousMain:capture(command.activator,false,command.time);
        scaleAmmo(command.activator,session.count,session.ratio);
        session.special[0]=capture(command.activator,true,command.time);
        session.maximum[0]=scalarInt(session.special[0],0);
        const bool armed=read<unsigned char>(command.activator+0x2a1)!=0;
        // 单挂不重建第二份；多挂依次捕获每个实际配置，再回到 SP1。
        for(int index=1;index<session.count;++index) {
            initialize(command,session.configs[index]);
            scaleAmmo(command.activator,session.count,session.ratio);
            session.special[index]=capture(command.activator,true,command.time);
            session.maximum[index]=scalarInt(session.special[index],0);
        }
        if(session.count>1)initialize(command,session.configs[0]);
        restore(command.activator,false,session.main,command.time);
        restore(command.activator,true,session.special[0],command.time);
        keepArmed(command,armed);
    } else if(command.op=="switch") {
        require(session.plane==command.plane && session.activator==command.activator,"A new plane requires setup");
        require(command.target<session.count && command.configs==session.configs && command.count==session.count,"切换方案与当前会话不符");
        require(command.target!=session.active,"Already using requested SP");
        require(!weaponSwitchBlockReason(command.plane,command.activator),"原生连射或Falken TLS尚未结束，暂不能重建SP");
        session.main=capture(command.activator,false,command.time);
        session.special[session.active]=capture(command.activator,true,command.time);
        const bool armed=read<unsigned char>(command.activator+0x2a1)!=0;
        initialize(command,session.configs[command.target]);
        restore(command.activator,false,session.main,command.time);
        restore(command.activator,true,session.special[command.target],command.time);
        session.active=command.target;
        keepArmed(command,armed);
        cfaSwitchCost.mark(6);
    } else if(command.op=="checkpoint_save") {
        require(session.plane==command.plane && session.activator==command.activator,"A new plane requires setup");
        session.main=capture(command.activator,false,command.time);
        session.special[session.active]=capture(command.activator,true,command.time);
        session.gun=read<int>(stateOf(command.activator)+8);
        session.flares=read<int>(read<Addr>(command.activator+0x260)+0x10c);
        checkpoint=session; checkpointTime=command.time; checkpointValid=true;
    } else if(command.op=="checkpoint_restore") {
        require(checkpointValid,"No captured checkpoint");
        require(command.configs==checkpoint.configs && command.count==checkpoint.count,"检查点方案与控制器不一致");
#ifdef DUALSP_SU30SM_MOUNT_PROTOTYPE
        fixedMountRelease(); // 检查点可能来自补给前的另一组合，重建前先归还旧布局。
#endif
        // Pointer-independent timers and scalar caches can be restored to a newly
        // spawned plane of the same class. restore() validates weapon identities.
        const bool armed=read<unsigned char>(command.activator+0x2a1)!=0;
        session=checkpoint; session.plane=command.plane; session.activator=command.activator;
        const double rebase=command.time-checkpointTime;
        session.main.capturedTime+=rebase;
        for(int index=0;index<session.count;++index)session.special[index].capturedTime+=rebase;
        initialize(command,session.configs[session.active]);
        restore(command.activator,false,session.main,command.time);
        restore(command.activator,true,session.special[session.active],command.time);
        write<int>(stateOf(command.activator)+8,session.gun);
        write<int>(read<Addr>(command.activator+0x260)+0x10c,session.flares);
        keepArmed(command,armed);
    } else if(command.op=="resupply") {
        require(session.plane==command.plane && session.activator==command.activator,"A new plane requires setup");
        const bool armed=read<unsigned char>(command.activator+0x2a1)!=0;
        const int active=session.active;
        // 原生补给不重选时，也只补充当前实际配置的槽。
        for(int index=0;index<session.count;++index) {
            initialize(command,session.configs[index],false);
            scaleAmmo(command.activator,session.count,session.ratio);
            session.special[index]=capture(command.activator,true,command.time);
            session.maximum[index]=scalarInt(session.special[index],0);
        }
        session.main=capture(command.activator,false,command.time);
        if(active!=session.count-1)initialize(command,session.configs[active],false);
        restore(command.activator,false,session.main,command.time);
        restore(command.activator,true,session.special[active],command.time);
        keepArmed(command,armed);
    } else {
        require(command.op=="snapshot","Unknown command");
    }
    if(command.op!="snapshot" && command.op!="checkpoint_save")ravenRestoreReload(command);
#ifdef DUALSP_SU30SM_MOUNT_PROTOTYPE
    try {
        fixedMountCommit(command); // 外观失败独立降级，不把已经完成的兵装事务报告为失败。
        bindF35Cover(command.plane);
    }catch(const std::exception& error) { fixedMountFail(error.what()); }
#endif
    cfaSwitchCost.mark(7);
}
}

// 仅声明候选核心支持新的补给协议，供 Lua 在重建兵装前核对版本。
extern "C" __declspec(dllexport) int dualsp_target_gaze(void* luaState) {return targetGazeLua(luaState);}
extern "C" __declspec(dllexport) int dualsp_gaze_latch_supported(void*) {return 0;}
extern "C" __declspec(dllexport) int dualsp_gaze_inputs_supported(void*) {return 0;}
extern "C" __declspec(dllexport) int dualsp_gun_salvo(void* luaState) {return gunSalvoLua(luaState);}
extern "C" __declspec(dllexport) int dualsp_gun_salvo_supported(void*) {return 0;}
extern "C" __declspec(dllexport) int dualsp_camera_hold(void* luaState) {return cameraHoldLua(luaState);}
extern "C" __declspec(dllexport) int dualsp_view_target(void* luaState) {return viewTargetLua(luaState);}
extern "C" __declspec(dllexport) int dualsp_view_target_modes_supported(void*) {return 0;}
extern "C" __declspec(dllexport) int dualsp_camera_controls_v2_supported(void*) {return 0;}
extern "C" __declspec(dllexport) int dualsp_camera_controls_supported(void*) {return 0;}
extern "C" __declspec(dllexport) int dualsp_camera_input_options_supported(void*) {return 0;}
extern "C" __declspec(dllexport) int dualsp_keyboard_modifier_routing_supported(void*) {return 0;}
extern "C" __declspec(dllexport) int dualsp_controller_clicks_supported(void*) {return 0;}
// 模式改变时清除本模块私有状态；禁止解引用旧玩家，不向多人武器写回单人快照。
extern "C" __declspec(dllexport) int dualsp_runtime_gate(void* luaState) {
    const dualsp::LuaApi* api=nullptr; // 错误显式返回，禁用状态先于清理生效。
    try {
        static const dualsp::LuaApi bound;api=&bound;
        require(api->top(luaState)==1,"Runtime gate expects one argument");
        const auto allowed=api->readInteger(luaState,1); // 只接受明确的单人允许或禁用。
        require(allowed==0 || allowed==1,"Invalid runtime gate value");
        runtimeEnabled=allowed!=0;
        if(!runtimeEnabled) {
            debugWeapons={};debugInfiniteAmmo=false;debugFastReload=false;
            viewTarget.clear();
            salvoNeedRelease=salvoNeedRelease || gunSalvo.gun;salvoStopExtra();gunSalvo={};
#ifdef DUALSP_SU30SM_MOUNT_PROTOTYPE
            mountPreviewRelease(0,false);
            fixedMountRelease(); // 世界仍有效时归还显示；身份失效时不读取旧组件。
#endif
            supplyPlane=0;supplyActivator=0;supplyWidget=0;preparationAction={};
            selectionAudio={};switchInputAllowed=false;emptySwitchRequested=false;cycleSwitchRequested=false;selectionWave.close();
            session={};checkpoint={};checkpointValid=false;checkpointTime=0;ravenVisual={};
        }
        api->boolean(luaState,true);api->string(luaState,"",0);return 2;
    }catch(const std::exception& error) {
        runtimeEnabled=false;nativeLog(std::string("RUNTIME_GATE_ERROR ")+error.what());
        if(api){api->boolean(luaState,false);api->string(luaState,error.what(),std::strlen(error.what()));return 2;}
        return 0;
    }
}

extern "C" __declspec(dllexport) int dualsp_debug_weapons(void* luaState) {return debugWeaponsLua(luaState);}

extern "C" __declspec(dllexport) bool dualsp_log_submit(unsigned source,const char* text,size_t length)noexcept {
    try {return dualsp::asyncLog().submit(source,text,length);}catch(...) {return false;}
}
extern "C" __declspec(dllexport) int dualsp_log(void* luaState) {
    const dualsp::LuaApi* api=nullptr;
    try {
        static const dualsp::LuaApi bound;api=&bound;
        size_t length{};const auto text=api->top(luaState)==1?api->readText(luaState,1,&length):nullptr;
        if(!text)throw std::runtime_error("Invalid log message");
        auto& logger=dualsp::asyncLog();logger.submit(0,text,length); // 压力丢弃由后台汇总，不反向干扰任务控制器。
        api->boolean(luaState,logger.available());return 1;
    }catch(...) {
        if(api){api->boolean(luaState,false);return 1;}return 0;
    }
}

// 具名导出作为安装与 Lua 启动握手；旧核心没有此接口。
extern "C" __declspec(dllexport) int dualsp_three_sp_supported(void*) {return 0;}
extern "C" __declspec(dllexport) int dualsp_cfa_startup_prewarm_supported(void*) {return 0;}
extern "C" __declspec(dllexport) int dualsp_ammo_ratio_supported(void*) {return 0;}
extern "C" __declspec(dllexport) int dualsp_infinite_ammo_supported(void*) {return 0;} // 目录、预览、运行与Lua负数规则须成套。
extern "C" __declspec(dllexport) int dualsp_raven_sp_supported(void*) {return 0;} // 拒绝旧核心混装。
extern "C" __declspec(dllexport) int dualsp_mod_electronic_supported(void*) {return 0;} // 四返回值和通用电子战保有须成套。
// 菜单按实际CDO核对通用电子战和已有TLS/UAV资格；返回原因以便诊断。
extern "C" __declspec(dllexport) int dualsp_continuous_sp_option(void* luaState) {
    const dualsp::LuaApi* api=nullptr;
    try {
        static const dualsp::LuaApi bound;api=&bound;
        require(runtimeEnabled,"Mod disabled outside single-player");
        require(continuousRuntimeFailure.empty(),continuousRuntimeFailure.c_str());
        require(api->top(luaState)==2,"持续兵装：选装检查参数无效");verifyVersion();
        const Addr plane=static_cast<Addr>(api->readInteger(luaState,1));
        const Addr cdo=static_cast<Addr>(api->readInteger(luaState,2));
        continuousIdentity(plane);continuousIdentity(cdo);
        const Addr activator=read<Addr>(plane+0x11a8),cls=read<Addr>(cdo+0x10);
        continuousIdentity(activator);
        bool configured=false;
        for(int i=0;i<3;++i)configured|=read<Addr>(activator+0x158+i*0x30)==cls;
        require(configured,"持续兵装：默认对象不属于当前机体的三个真实配置");
        const auto support=continuousSupport(plane,cdo);
        const bool ravenAllowed=support.kind!=ContinuousKind::UAV || continuousRavenLoadout(activator);
        const bool supported=support.kind!=ContinuousKind::None && ravenAllowed;
        const char* reason=ravenAllowed?support.reason:"uav_loadout_changed";
        api->boolean(luaState,true);api->boolean(luaState,supported);
        api->boolean(luaState,support.electronic);api->string(luaState,reason,std::strlen(reason));return 4;
    }catch(const std::exception& error) {
        if(api){api->boolean(luaState,false);api->string(luaState,error.what(),std::strlen(error.what()));return 2;}
        return 0;
    }
}
extern "C" __declspec(dllexport) int dualsp_resupply_supported(void*) {
    return 0;
}

#ifdef DUALSP_SU30SM_MOUNT_PROTOTYPE
// 派生挂架加载前复用核心的枚举反射校验，不依赖Lua代理枚举与整数直接比较。
extern "C" __declspec(dllexport) int dualsp_mount_collision_guard(void* luaState) {
    const dualsp::LuaApi* api=nullptr;
    try {
        static const dualsp::LuaApi bound;api=&bound;
        require(runtimeEnabled && api->top(luaState)==2,"挂架预检：当前场景或参数无效");
        verifyVersion();mountInstallHooks();
        require(GetCurrentThreadId()==mountGameThread.load(),"挂架预检：不在绑定的游戏线程");
        const Addr plane=static_cast<Addr>(api->readInteger(luaState,1));
        const auto requestedPool=api->readInteger(luaState,2);
        require(requestedPool>=1 && requestedPool<=3,"挂架预检：组件编号无效");
        const auto identity=mountIdentity(plane);
        require(mountExpanded(mountAircraft(plane)),"挂架预检：机体不在扩展白名单");
        const auto offset=mountProperty(plane,("SWP"+std::to_string(requestedPool)).c_str(),"ObjectProperty",8);
        const Addr rack=read<Addr>(plane+offset),base=read<Addr>(plane+0x1ae0);
        const auto rackIdentity=mountIdentity(rack);mountIdentity(base);
        require(read<Addr>(rack+0x100)==base,"挂架预检：共同父链改变");
        mountRequireNoCollision(rack);
        require(mountAlive(identity) && mountAlive(rackIdentity) && read<Addr>(plane+offset)==rack &&
            read<Addr>(plane+0x1ae0)==base,"挂架预检：原生查询期间对象换代");
        api->boolean(luaState,true);api->string(luaState,"",0);return 2;
    }catch(const std::exception& error) {
        if(api){api->boolean(luaState,false);api->string(luaState,error.what(),std::strlen(error.what()));return 2;}
        return 0;
    }
}
// 独立资源仅接受生成白名单的精确包名；只保有纯网格，不保有机体或世界。
extern "C" __declspec(dllexport) int dualsp_mount_asset(void* luaState) {
    const dualsp::LuaApi* api=nullptr;
    try {
        static const dualsp::LuaApi bound;api=&bound;
        require(runtimeEnabled,"扩展挂载：当前场景禁止接入");
        require(api->top(luaState)==2,"扩展挂载：资源登记参数无效");verifyVersion();mountInstallHooks();
        require(GetCurrentThreadId()==mountGameThread.load(),"扩展挂载：资源登记不在游戏线程");
        const int index=static_cast<int>(api->readInteger(luaState,1));
        const Addr object=static_cast<Addr>(api->readInteger(luaState,2));
        const auto& names=mountExpansionData().at("assets");
        require(index>=0 && index<static_cast<int>(names.size()),"扩展挂载：独立资源编号越界");
        const auto identity=mountIdentity(object);const auto name=names.at(index).get<std::string>();
        require(mountName(object)==name && mountName(read<Addr>(object+0x20))=="/Game/AC7MultiSP/Research/"+name &&
            mountName(read<Addr>(object+0x10))=="StaticMesh","扩展挂载：资源包、名称或类型不匹配");
        const Addr base=reinterpret_cast<Addr>(GetModuleHandleW(nullptr));
        const Addr entry=read<Addr>(base+0x3bcfdb0)+static_cast<Addr>(identity.index)*24;
        require(!(read<unsigned>(entry+8)&0x30000000u),"扩展挂载：资源正在销毁");
        const auto found=mountAssets.find(index);
        if(found!=mountAssets.end())require(mountMeshIdentityEqual(found->second.identity,identity),"扩展挂载：已保有资源身份改变");
        else {
            // 只保有当前白名单内纯网格；沿用已核对的FUObjectItem内部RootSet位，不引用世界或玩家。
            const LONG previous=InterlockedOr(reinterpret_cast<volatile LONG*>(entry+8),0x40000000L);
            mountAssets.emplace(index,MountAssetReference{identity,(previous&0x40000000L)==0});
            nativeLog("MOUNT_ASSET_READY "+name);
        }
        api->boolean(luaState,true);api->string(luaState,"",0);return 2;
    }catch(const std::exception& error) {
        if(api){api->boolean(luaState,false);api->string(luaState,error.what(),std::strlen(error.what()));return 2;}
        return 0;
    }
}
// 与其他具名接口一致，Lua 以整数 0/1 传开关；关闭也执行完整的原生外观归还。
extern "C" __declspec(dllexport) int dualsp_set_fixed_mount_enabled(void* luaState) {
    const dualsp::LuaApi* api=nullptr;
    try {
        static const dualsp::LuaApi bound;api=&bound;
        require(api->top(luaState)==1,"固定挂载开关需要一个参数");
        const auto enabled=api->readInteger(luaState,1);
        require(enabled==0 || enabled==1,"固定挂载开关必须为 0 或 1");
        require(!mountHooksReady || GetCurrentThreadId()==mountGameThread.load(),"固定挂载开关必须在游戏线程调用");
        const bool changed=fixedMountEnabled!=(enabled!=0);
        fixedMountEnabled=enabled!=0;
        if(!fixedMountEnabled || changed) {
            mountPreviewRelease(0,false);
            if(fixedMountRelease()) { fixedMountFailed=false;fixedMountWarning.clear(); }
        }
        const std::string warning=mountPreview.warning.empty()?fixedMountWarning:mountPreview.warning;
        api->boolean(luaState,true);api->string(luaState,warning.data(),warning.size());return 2;
    }catch(const std::exception& error) {
        nativeLog(std::string("FIXED_MOUNT_SETTING_ERROR ")+error.what());
        if(api){api->boolean(luaState,false);api->string(luaState,error.what(),std::strlen(error.what()));return 2;}
        return 0;
    }
}

// 仅更新经过校验的展示模型；警告与兵装事务独立。
extern "C" __declspec(dllexport) int dualsp_mount_preview(void* luaState) {
    const dualsp::LuaApi* api=nullptr;
    try {
        static const dualsp::LuaApi bound;api=&bound;
        require(api->top(luaState)==10,"模型预览需要十个整数参数");
        require(!mountHooksReady || GetCurrentThreadId()==mountGameThread.load(),"模型预览必须在游戏线程调用");
        const Addr owner=static_cast<Addr>(api->readInteger(luaState,1));
        const Addr plane=static_cast<Addr>(api->readInteger(luaState,2));
        const Addr activator=static_cast<Addr>(api->readInteger(luaState,3));
        const auto mode=api->readInteger(luaState,4);
        std::array<int,3> configs{},ammo{};
        for(int slot=0;slot<3;++slot) {
            const auto config=api->readInteger(luaState,5+slot),amount=api->readInteger(luaState,8+slot);
            require(config>=-1 && config<=2 && dualsp::validAmmoCount(amount),"模型预览配置或弹药参数越界");
            configs[slot]=static_cast<int>(config);ammo[slot]=static_cast<int>(amount);
        }
        if(mode==0)mountPreviewRelease(owner);
        else if(mode==4) {
            // 补给页已关闭时，仅刷新原先验证过的同一玩家预览，不能创建新绑定。
            if(!runtimeEnabled || !fixedMountEnabled)mountPreviewRelease(0,false);
            else {
                require(mountPreview.active && (mountPreview.mode==2 || mountPreview.mode==5) && mountPreview.layout.plane.address==plane &&
                    mountPreview.activator.address==activator,"补给过渡预览身份不匹配");
                if(GetTickCount64()-mountPreview.lastApply>=100)mountPreviewApply(); // 失败时只重试归还，不恢复接管。
            }
        } else {
            require(mode==1 || mode==2 || mode==3 || mode==5,"模型预览模式无效");
            mountPreviewUpdate(owner,plane,activator,static_cast<int>(mode),configs,ammo);
        }
        const auto& warning=mountPreview.warning;
        api->boolean(luaState,warning.empty() && !mountPreview.pending);api->string(luaState,warning.data(),warning.size());return 2;
    }catch(const std::exception& error) {
        if(mountPreview.active)mountPreviewFail(error.what());
        const std::string warning=mountPreview.warning.empty()?std::string("模型预览：")+error.what():mountPreview.warning;
        if(api) { api->boolean(luaState,false);api->string(luaState,warning.data(),warning.size());return 2; }
        return 0;
    }
}

// 独立原型的视觉调度，使用游戏时间；不把有副作用的更新塞进只读 HUD 接口。
extern "C" __declspec(dllexport) int dualsp_fixed_mount_update(void* luaState) {
    const dualsp::LuaApi* api=nullptr; // 错误回执由既有 Lua 控制器显示。
    try {
        static const dualsp::LuaApi bound;api=&bound;
        require(api->top(luaState)==2,"固定挂载更新需要玩家地址和游戏时间");
        const Addr plane=static_cast<Addr>(api->readInteger(luaState,1)); // 零表示离开本次玩家实例。
        const double time=api->readNumber(luaState,2); // 暂停时不推进。
        if(!plane) {
            mountPreviewRelease(0,false);
            if(fixedMountRelease()) { fixedMountFailed=false;fixedMountWarning.clear(); }
        }
        else {
            require(runtimeEnabled,"固定挂载只允许单人任务");
            require(std::isfinite(time) && time>=0,"固定挂载游戏时间无效");
            try { fixedMountUpdate(plane,session.plane,time); }
            catch(const std::exception& error) { fixedMountFail(error.what()); }
        }
        const std::string warning=mountPreview.warning.empty()?fixedMountWarning:mountPreview.warning;
        api->boolean(luaState,true);api->string(luaState,warning.data(),warning.size());return 2;
    }catch(const std::exception& error) {
        fixedMountFail(error.what());
        if(api){api->boolean(luaState,false);api->string(luaState,error.what(),std::strlen(error.what()));return 2;}
        return 0;
    }
}
#endif

// op=0 绑定或静默同步，1 提交一次真实选择，2 解绑；初始化和检查点恢复不播放提示。
extern "C" __declspec(dllexport) int dualsp_selection_audio(void* luaState) {
    const dualsp::LuaApi* api=nullptr; // 返回音效接入错误，不能悄悄混播旧提示。
    try {
        static const dualsp::LuaApi bound; // 已核对的加载器栈接口。
        api=&bound;
        require(api->top(luaState)==4,"选择音效接口参数数量无效");
        const auto plane=static_cast<Addr>(api->readInteger(luaState,1)); // 玩家身份，解绑时允许为零。
        const auto activator=static_cast<Addr>(api->readInteger(luaState,2)); // 当前玩家激活器。
        const int op=static_cast<int>(api->readInteger(luaState,3)); // 绑定、提交或解绑。
        const int target=static_cast<int>(api->readInteger(luaState,4)); // MSL/SP1/SP2 分别为 0/1/2。
        require(op>=0 && op<=2 && target>=0 && target<=3,"选择音效动作无效");
        if(op==2){selectionAudio={};selectionWave.close();switchInputAllowed=false;emptySwitchRequested=false;cycleSwitchRequested=false;} // 世界销毁时不读取旧玩家对象。
        else {
            require(runtimeEnabled,"Mod disabled outside single-player");
            verifyVersion();
            require(plane && activator && read<Addr>(plane+0x11a8)==activator,"选择音效玩家身份不匹配");
            emptySwitchRequested=false;cycleSwitchRequested=false; // 最终选择覆盖此前尚未领取的旧输入。
            if(op==0) {
                selectionWave.open(modRoot()/"Audio/SP2.wav");
                installSelectionAudio();selectionAudio={plane,activator,target};emptySwitchRequested=false;
            } else {
                require(selectionAudio.plane==plane && selectionAudio.activator==activator,"选择音效尚未绑定本次出击");
                const bool armed=read<unsigned char>(activator+0x2a1)!=0; // 只允许完成后的真实原生类别。
                require(armed==(target>0),"选择音效早于原生武器类别完成");
                require(target==0 || (session.plane==plane && session.activator==activator && session.active==target-1),
                    "选择音效与最终 SP 槽不一致");
                if(target!=selectionAudio.last) {
                    playSelection(target);selectionAudio.last=target;
                    nativeLog(std::string("SELECTION_AUDIO_COMMITTED ")+std::to_string(target));
                }
            }
        }
        api->boolean(luaState,true);api->string(luaState,"",0);return 2;
    }catch(const std::exception& error) {
        selectionAudio={};
        switchInputAllowed=false;emptySwitchRequested=false;cycleSwitchRequested=false;
        selectionWave.close();
        nativeLog(std::string("SELECTION_AUDIO_ERROR ")+error.what());
        if(api){api->boolean(luaState,false);api->string(luaState,error.what(),std::strlen(error.what()));return 2;}
        return 0;
    }
}

// 调试换机只读取当前任务GI所持存档；不合并其他存档，也不写入购买记录。
extern "C" __declspec(dllexport) int dualsp_debug_owned_aircraft(void* luaState) {
    const dualsp::LuaApi* api=nullptr;
    try {
        static const dualsp::LuaApi bound;api=&bound;
        require(api->top(luaState)==1 && runtimeEnabled,"已购机体查询需要当前单人任务和一个GI地址");
        verifyVersion();MountAnimationReflection reflection;reflection.bindSdk();
        const auto instance=static_cast<Addr>(api->readInteger(luaState,1));
        const auto instanceIdentity=continuousIdentity(instance);
        require(mountClassIs(instance,"NimbusGameInstance","/Script/Nimbus"),"已购机体查询：GI类型不符");
        require(session.plane!=0,"已购机体查询：当前任务尚未配置");
        const auto planeIdentity=continuousIdentity(session.plane);
        require(mountClassIs(session.plane,"PlayerPlane","/Script/Nimbus") &&
            read<Addr>(session.plane+mountProperty(session.plane,"GameInstance","ObjectProperty",8))==instance,
            "已购机体查询：GI不属于当前任务机体");
        const auto base=reinterpret_cast<Addr>(GetModuleHandleW(nullptr));
        constexpr unsigned char optionRead[]={0x48,0x8b,0x90,0xf8,0x05,0,0,0x48,0x8b,0x87,0x38,0x04,0,0,0xf3,0x0f,0x10,0x82,0x30,0x01,0,0};
        checkMemory(base+0x766776,sizeof(optionRead));
        require(std::memcmp(reinterpret_cast<void*>(base+0x766776),optionRead,sizeof(optionRead))==0,
            "已购机体查询：当前存档读取指纹改变");
        const Addr save=read<Addr>(instance+0x5f8); // 仅此处沿用原生OptionsSound已证明的当前存档读取。
        const auto saveIdentity=continuousIdentity(save);
        require(!(read<unsigned>(instance+8)&0x30u) && !(read<unsigned>(save+8)&0x30u),"已购机体查询：不能使用默认对象或原型存档");
        require(mountClassIs(save,"SigmaSaveGame","/Script/Nimbus"),"已购机体查询：当前存档类型不符");
        const Addr saveClass=read<Addr>(save+0x10),commonProperty=MountAnimationReflection::find(saveClass,"CommonData");
        require(commonProperty && mountName(read<Addr>(commonProperty+0x10))=="StructProperty",
            "已购机体查询：缺少正式CommonData结构");
        const Addr commonType=MountAnimationReflection::refValue<Addr>(reflection.structRef,commonProperty);
        MountAnimationReflection::named(commonType,"CommonSaveData","/Script/Nimbus");
        const auto common=reflection.field(saveClass,"CommonData","StructProperty",reflection.typeSize(commonType),
            reflection.typeSize(saveClass),true);
        const auto purchased=reflection.field(commonType,"PurchasedAircrafts","ArrayProperty",16,common.size);
        const auto innerGetter=reinterpret_cast<MountAnimationReflection::Ref>(GetProcAddress(GetModuleHandleW(L"UE4SS.dll"),
            "?GetInner@FArrayProperty@Unreal@RC@@QEBAAEAPEBVFProperty@23@XZ"));
        const Addr inner=MountAnimationReflection::refValue<Addr>(innerGetter,purchased.identity.address);
        MountAnimationReflection::alive(mountIdentity(inner));
        require(mountName(read<Addr>(inner+0x10))=="UInt32Property" && read<int>(inner+0x34)==4 &&
            MountAnimationReflection::refValue<int>(reflection.arrayDim,inner)==1 &&
            read<Addr>(inner+0x20)==purchased.identity.address,"已购机体查询：购买列表不是原生uint32数组");
        const Addr array=save+common.offset+purchased.offset,data=read<Addr>(array);
        const int count=read<int>(array+8),capacity=read<int>(array+12);
        require(count>=0 && count<=4096 && capacity>=count && capacity<=65536 && (!count || (data && !(data&3))),
            "已购机体查询：购买列表范围无效");
        Json result=Json::array();
        if(count)checkMemory(data,static_cast<size_t>(count)*sizeof(uint32_t));
        for(int i=0;i<count;++i) {
            const auto id=read<uint32_t>(data+static_cast<Addr>(i)*sizeof(uint32_t));
            require(id<=0x7fffffffu,"已购机体查询：机体编号超出原生IntProperty范围");result.push_back(id);
        }
        require(continuousAlive(instanceIdentity) && continuousAlive(planeIdentity) && continuousAlive(saveIdentity) &&
            read<Addr>(instance+0x5f8)==save && read<Addr>(array)==data && read<int>(array+8)==count &&
            read<int>(array+12)==capacity,"已购机体查询：读取期间当前存档或列表改变");
        const auto text=result.dump();api->boolean(luaState,true);api->string(luaState,text.data(),text.size());return 2;
    }catch(const std::exception& error) {
        if(api){api->boolean(luaState,false);api->string(luaState,error.what(),std::strlen(error.what()));return 2;}
        return 0;
    }
}

// 按游戏线程现有调度更新暂停和音量；不通过扫描多个存档副本猜测当前设置。
extern "C" __declspec(dllexport) int dualsp_custom_audio_update(void* luaState) {
    const dualsp::LuaApi* api=nullptr; // 与选择接口相同的明确错误回执。
    try {
        static const dualsp::LuaApi bound;api=&bound; // 专用加载器栈接口。
        require(api->top(luaState)==2,"自定义选择音状态参数数量无效");
        const auto instance=static_cast<Addr>(api->readInteger(luaState,1)); // 当前 NimbusGameInstance。
        const auto allowed=api->readInteger(luaState,2); // Lua 已核对任务、补给、暂停和面板状态。
        require(allowed==0 || allowed==1,"自定义选择音播放状态无效");
        require(runtimeEnabled || allowed==0,"Mod disabled outside single-player");
        switchInputAllowed=runtimeEnabled && allowed!=0 && selectionAudio.plane!=0;
        if(!switchInputAllowed){emptySwitchRequested=false;cycleSwitchRequested=false;}
        if(selectionAudio.plane) {
            float volume=0; // 禁止播放时无需解引用任务或设置对象。
            if(allowed) {
                const auto base=reinterpret_cast<Addr>(GetModuleHandleW(nullptr)); // OptionsSound 原生读取路径。
                constexpr unsigned char optionRead[]={0x48,0x8b,0x90,0xf8,0x05,0,0,0x48,0x8b,0x87,0x38,0x04,0,0,0xf3,0x0f,0x10,0x82,0x30,0x01,0,0};
                require(std::memcmp(reinterpret_cast<void*>(base+0x766776),optionRead,sizeof(optionRead))==0,
                    "游戏音效音量读取接口不匹配");
                require(instance!=0,"自定义选择音缺少当前游戏实例");
                const auto save=read<Addr>(instance+0x5f8); // 原生声音菜单使用的当前 SigmaSaveGame。
                volume=read<float>(save+0x130); // OptionsData(0x90) + SFXVolume(0xa0)。
            }
            selectionWave.update(allowed!=0,volume);
        }
        api->boolean(luaState,true);api->string(luaState,"",0);return 2;
    } catch(const std::exception& error) {
        selectionWave.close();selectionAudio={};
        switchInputAllowed=false;emptySwitchRequested=false;cycleSwitchRequested=false;
        nativeLog(std::string("CUSTOM_AUDIO_ERROR ")+error.what());
        if(api){api->boolean(luaState,false);api->string(luaState,error.what(),std::strlen(error.what()));return 2;}
        return 0;
    }
}

// 消费耗尽 SP 的真实切换请求；原生已切换的普通请求仍由 Lua 的类别观察处理。
extern "C" __declspec(dllexport) int dualsp_take_empty_switch(void* luaState) {
    const dualsp::LuaApi* api=nullptr; // 与现有核心统一返回明确错误。
    try {
        static const dualsp::LuaApi bound;api=&bound; // 游戏线程调用，无跨线程输入队列。
        require(api->top(luaState)==1,"切换请求接口参数数量无效");
        const auto plane=static_cast<Addr>(api->readInteger(luaState,1)); // 防止旧世界消费新任务请求。
        require(plane && plane==selectionAudio.plane,"切换请求的玩家身份不匹配");
        require(runtimeEnabled,"Mod disabled outside single-player");
        const bool requested=emptySwitchRequested;emptySwitchRequested=false; // 每次按键只消费一次。
        api->boolean(luaState,true);api->boolean(luaState,requested);return 2;
    }catch(const std::exception& error) {
        emptySwitchRequested=false;nativeLog(std::string("SWITCH_REQUEST_ERROR ")+error.what());
        if(api){api->boolean(luaState,false);api->string(luaState,error.what(),std::strlen(error.what()));return 2;}
        return 0;
    }
}

// 多SP真实循环请求只消费一次；最终目标及耗尽跳过仍由Lua统一计算。
extern "C" __declspec(dllexport) int dualsp_take_cycle_switch(void* luaState) {
    const dualsp::LuaApi* api=nullptr;
    try {
        static const dualsp::LuaApi bound;api=&bound;
        require(api->top(luaState)==1 && runtimeEnabled,"循环切换请求需要单人任务和玩家身份");
        const auto plane=static_cast<Addr>(api->readInteger(luaState,1));
        require(plane && plane==selectionAudio.plane && plane==session.plane,"循环切换玩家会话不符");
        const bool requested=cycleSwitchRequested;cycleSwitchRequested=false;
        api->boolean(luaState,true);api->boolean(luaState,requested);return 2;
    }catch(const std::exception& error) {
        cycleSwitchRequested=false;nativeLog(std::string("CYCLE_SWITCH_ERROR ")+error.what());
        if(api){api->boolean(luaState,false);api->string(luaState,error.what(),std::strlen(error.what()));return 2;}
        return 0;
    }
}
// 直接选择与循环选择共同使用完整飞机入口；作用域内不重复拦截自己的调用。
extern "C" __declspec(dllexport) int dualsp_controlled_weapon_switch(void* luaState) {
    const dualsp::LuaApi* api=nullptr;
    try {
        static const dualsp::LuaApi bound;api=&bound;
        require(api->top(luaState)==2 && runtimeEnabled,"类别切换需要单人任务和玩家身份");verifyVersion();
        const auto plane=static_cast<Addr>(api->readInteger(luaState,1));
        const auto activator=static_cast<Addr>(api->readInteger(luaState,2));
        require(session.plane==plane && session.activator==activator && read<Addr>(plane+0x11a8)==activator,
            "类别切换玩家会话不符");
        // 与Lua等待和核心重建共用门槛；暂拒时不按下切换，由实际类别让Lua继续等待。
        if(weaponSwitchBlockReason(plane,activator)){api->boolean(luaState,true);return 1;}
        const auto base=reinterpret_cast<Addr>(GetModuleHandleW(nullptr));
        constexpr unsigned char pressed[]={0x33,0xd2,0xe9};
        constexpr unsigned char released[]={0xc6,0x81,0x51,0x1a,0,0,0,0xc3};
        require(std::memcmp(reinterpret_cast<void*>(base+0x8238d0),pressed,sizeof(pressed))==0 &&
            std::memcmp(reinterpret_cast<void*>(base+0x8238e0),released,sizeof(released))==0,"原生飞机切换入口不匹配");
        ControlledWeaponSwitch controlled;
        using Switch=void(*)(void*);
        reinterpret_cast<Switch>(base+0x8238d0)(reinterpret_cast<void*>(plane));
        reinterpret_cast<Switch>(base+0x8238e0)(reinterpret_cast<void*>(plane));
        api->boolean(luaState,true);return 1;
    }catch(const std::exception& error) {
        nativeLog(std::string("CONTROLLED_SWITCH_ERROR ")+error.what());
        if(api){api->boolean(luaState,false);api->string(luaState,error.what(),std::strlen(error.what()));return 2;}
        return 0;
    }
}

// 调试提交前只读核对；Lua 在触碰 HUD、方案和武器之前调用，拒绝不影响原会话。
extern "C" __declspec(dllexport) int dualsp_debug_loadout_ready(void* luaState) {
    const dualsp::LuaApi* api=nullptr; // 返回明确拒绝原因，不自动延后或重试换装。
    try {
        static const dualsp::LuaApi bound;api=&bound; // 使用专用加载器 ABI。
        require(runtimeEnabled,"Mod disabled outside single-player");
        require(api->top(luaState)==2,"调试换装接口参数数量无效");verifyVersion();
        const auto plane=static_cast<Addr>(api->readInteger(luaState,1)); // 本次玩家身份。
        const auto activator=static_cast<Addr>(api->readInteger(luaState,2)); // 本次武器组件身份。
        requireDebugLoadoutReady(plane,activator);api->boolean(luaState,true);return 1;
    }catch(const std::exception& error) {
        if(api){api->boolean(luaState,false);api->string(luaState,error.what(),std::strlen(error.what()));return 2;}
        return 0;
    }
}

#ifdef DUALSP_SU30SM_MOUNT_PROTOTYPE
// 调试即时切换只重绑模型，保留实际弹药、冷却、当前SP和检查点。
extern "C" __declspec(dllexport) int dualsp_debug_fixed_mount(void* luaState) {
    const dualsp::LuaApi* api=nullptr;
    try {
        static const dualsp::LuaApi bound;api=&bound;
        require(api->top(luaState)==4 && runtimeEnabled,"即时挂载开关需要单人任务与四个参数");
        require(!mountHooksReady || GetCurrentThreadId()==mountGameThread.load(),"即时挂载开关必须在游戏线程调用");
        verifyVersion();
        Command command;command.op="debug_mount";
        command.plane=static_cast<Addr>(api->readInteger(luaState,1));
        command.activator=static_cast<Addr>(api->readInteger(luaState,2));command.time=api->readNumber(luaState,3);
        const auto enabled=api->readInteger(luaState,4);
        require((enabled==0 || enabled==1) && std::isfinite(command.time) && command.time>=0,"即时挂载参数无效");
        // 只切外观，不重建武器或触碰活动发射体，因此不要求输入开放、解除暂停或清空在途武器。
        require(session.plane==command.plane && session.activator==command.activator,"即时挂载的玩家会话已经改变");
        require(read<Addr>(command.plane+0x11a8)==command.activator,"即时挂载的武器组件不属于当前机体");
        fixedMountEnabled=enabled!=0;
        if(fixedMountRelease()) {
            fixedMountFailed=false;fixedMountWarning.clear();
            if(fixedMountEnabled)try { fixedMountCommit(command);bindF35Cover(command.plane); }
                catch(const std::exception& error) { fixedMountFail(error.what()); }
        }
        api->boolean(luaState,true);api->string(luaState,fixedMountWarning.data(),fixedMountWarning.size());return 2;
    }catch(const std::exception& error) {
        if(api){api->boolean(luaState,false);api->string(luaState,error.what(),std::strlen(error.what()));return 2;}
        return 0;
    }
}
#endif
// 仅新选择请求在释放输入后调用；查询/等待接口仍保持只读。
extern "C" __declspec(dllexport) int dualsp_prepare_cfa_switch(void* luaState) {
    const dualsp::LuaApi* api=nullptr;
    try {
        static const dualsp::LuaApi bound;api=&bound;
        require(api->top(luaState)==2 && runtimeEnabled,"CFA切换准备需要单人任务与两个参数");
        const auto plane=static_cast<Addr>(api->readInteger(luaState,1));
        const auto activator=static_cast<Addr>(api->readInteger(luaState,2));
        require(session.plane==plane && session.activator==activator &&
            session.gameThread && session.gameThread==GetCurrentThreadId() &&
            read<Addr>(plane+0x11a8)==activator,"CFA切换准备的玩家会话或线程不符");
        cfaDiscardQueuedFireInput(plane,activator);
        api->boolean(luaState,true);return 1;
    }catch(const std::exception& error) {
        if(api){api->boolean(luaState,false);api->string(luaState,error.what(),std::strlen(error.what()));return 2;}
        return 0;
    }
}
// 暂拒切换是可恢复状态；Lua对普通连射暂存请求，对Falken TLS直接丢弃请求。
extern "C" __declspec(dllexport) int dualsp_switch_ready(void* luaState) {
    const dualsp::LuaApi* api=nullptr;
    try {
        static const dualsp::LuaApi bound;api=&bound;
        require(api->top(luaState)==2 && runtimeEnabled,"切换就绪查询需要单人任务与两个参数");
        verifyVersion();
        const auto plane=static_cast<Addr>(api->readInteger(luaState,1));
        const auto activator=static_cast<Addr>(api->readInteger(luaState,2));
        require(session.plane==plane && session.activator==activator &&
            read<Addr>(plane+0x11a8)==activator,"切换就绪查询的玩家会话已经改变");
        const char* reason=weaponSwitchBlockReason(plane,activator);
        api->boolean(luaState,true);api->boolean(luaState,reason==nullptr);
        api->string(luaState,reason?reason:"",reason?std::strlen(reason):0);return 3;
    }catch(const std::exception& error) {
        if(api){api->boolean(luaState,false);api->boolean(luaState,false);api->string(luaState,error.what(),std::strlen(error.what()));return 3;}
        return 0;
    }
}
// 在保存冷却前选择 TLS 策略；研究机型交给后续保有事务，其余沿用原生结束。
extern "C" __declspec(dllexport) int dualsp_finish_tls(void* luaState) {
    const dualsp::LuaApi* api=nullptr; // 出错就停止本次选择，不继续重建武器。
    try {
        static const dualsp::LuaApi bound;api=&bound; // 与已安装 UE4SS 共享参数接口。
        require(runtimeEnabled,"Mod disabled outside single-player");
        require(continuousRuntimeFailure.empty(),continuousRuntimeFailure.c_str());
        require(api->top(luaState)==2,"TLS 收尾接口参数数量无效");verifyVersion();
        const auto plane=static_cast<Addr>(api->readInteger(luaState,1)); // 本次玩家和激活器。
        const auto activator=static_cast<Addr>(api->readInteger(luaState,2));
        require(read<Addr>(plane+0x11a8)==activator,"TLS 收尾的玩家身份不匹配");
        // Falken只允许自然退场；即使调用旧收尾接口，也不得提前End或隐藏主束。
        if(continuousFalken(plane)) {
            require(!continuousFalkenTLSBusy(plane,activator),"Falken TLS尚未自然结束，请等待后切换");
            api->boolean(luaState,true);return 1;
        }
        // 已登记机型的原生TLS交由换槽事务保留；其余仍沿用原生结束。
        if(!continuousSupported(plane,read<Addr>(groupOf(activator,true)+0x40)))
            finishPlayerTLS(plane,activator);
        api->boolean(luaState,true);return 1;
    }catch(const std::exception& error) {
        nativeLog(std::string("TLS_END_ERROR ")+error.what());
        if(api){api->boolean(luaState,false);api->string(luaState,error.what(),std::strlen(error.what()));return 2;}
        return 0;
    }
}

// 仅补齐模组内部确认步骤的声音；函数 0x61b1d0 会按页面管理器选择原生菜单音。
extern "C" __declspec(dllexport) int dualsp_menu_confirm(void* luaState) {
    const dualsp::LuaApi* api=nullptr; // 失败明确返回，不改菜单或兵装。
    try {
        static const dualsp::LuaApi bound; // 专用加载器 ABI，调用者在游戏线程。
        api=&bound;
        require(runtimeEnabled,"Mod disabled outside single-player");
        require(api->top(luaState)==1,"菜单确认音参数数量无效");
        verifyVersion();
        const auto widget=static_cast<Addr>(api->readInteger(luaState,1)); // 当前已接管的选择页。
        const auto base=reinterpret_cast<Addr>(GetModuleHandleW(nullptr)); // 已核对 build 的游戏映像。
        const auto table=read<Addr>(widget); // 只接受机库兵装页或已绑定的补给页。
        const bool preparation=read<Addr>(table+0x420)==base+0x6d4640 && read<Addr>(table+0x460)==base+0x6d3d00;
        const bool supply=read<Addr>(table+0x408)==base+0x6fa1f0 && read<Addr>(table+0x420)==base+0x6fc6d0;
        require(preparation || supply,"菜单确认音页面类型不匹配");
        require(read<Addr>(widget+0x288)!=0,"菜单确认音缺少原生管理器");
        if(supply)require(supplyWidget==widget && supplyPlane && supplyActivator &&
            read<Addr>(widget+0x5c0)==supplyPlane && read<Addr>(widget+0x5c8)==supplyActivator,
            "补给确认音不属于当前输入事务");
        constexpr unsigned char signature[]={0x48,0x89,0x5c,0x24,0x18,0x48,0x89,0x74,0x24,0x20,0x55,0x57,0x41,0x56};
        require(std::memcmp(reinterpret_cast<void*>(base+0x61b1d0),signature,sizeof(signature))==0,"原生菜单音入口不匹配");
        using PlayMenuSound=int(*)(void*,unsigned char); // 两种原生 SP 确认路径都使用事件编号 3。
        reinterpret_cast<PlayMenuSound>(base+0x61b1d0)(reinterpret_cast<void*>(widget),3);
        nativeLog(preparation?"PREPARATION_CONFIRM_AUDIO":"RESUPPLY_CONFIRM_AUDIO");
        api->boolean(luaState,true);api->string(luaState,"",0);return 2;
    }catch(const std::exception& error) {
        nativeLog(std::string("MENU_CONFIRM_AUDIO_ERROR ")+error.what());
        if(api){api->boolean(luaState,false);api->string(luaState,error.what(),std::strlen(error.what()));return 2;}
        return 0;
    }
}

// 只向模组新增的 Image 绘制原生武器分类，不改原控件或当前兵装。
extern "C" __declspec(dllexport) int dualsp_weapon_icon(void* luaState) {
    const dualsp::LuaApi* api=nullptr; // 明确返回绘制失败，不换用自制图标。
    try {
        static const dualsp::LuaApi bound;api=&bound; // 游戏线程上的加载器接口。
        require(runtimeEnabled,"Mod disabled outside single-player");
        require(api->top(luaState)==3,"兵装图标参数数量无效");verifyVersion();
        const auto image=static_cast<Addr>(api->readInteger(luaState,1)); // Lua 已检查 Image 类型。
        const auto owner=static_cast<Addr>(api->readInteger(luaState,2)); // 当前机库页面。
        const auto category=api->readInteger(luaState,3); // 武器表的对空、对地、其他三类。
        require(category>=0 && category<=2,"未知兵装图标类别");checkMemory(image,0x1d0);
        const auto manager=read<Addr>(owner+0x288); // 原生菜单自己的 UIManager。
        const auto style=read<Addr>(manager+0x388); // 与原生全名图标相同的类别颜色表。
        checkMemory(style,0x290);
        const auto base=reinterpret_cast<Addr>(GetModuleHandleW(nullptr)); // 固定 Steam build。
        constexpr unsigned char signature[]={0x80,0xc2,0x05,0xe9,0x38,0xfe,0xff,0xff};
        require(std::memcmp(reinterpret_cast<void*>(base+0x637c70),signature,sizeof(signature))==0,"原生兵装图标入口不匹配");
        using PaintIcon=void(*)(void*,unsigned char,void*); // 类别内部加 5 后绘制 Offset、颜色与泛光。
        reinterpret_cast<PaintIcon>(base+0x637c70)(reinterpret_cast<void*>(image),static_cast<unsigned char>(category),reinterpret_cast<void*>(style));
        api->boolean(luaState,true);api->string(luaState,"",0);return 2;
    }catch(const std::exception& error) {
        nativeLog(std::string("WEAPON_ICON_ERROR ")+error.what());
        if(api){api->boolean(luaState,false);api->string(luaState,error.what(),std::strlen(error.what()));return 2;}
        return 0;
    }
}

// 原生补给预览的局部修正结构：六类加法/倍率及原生默认尾字段。
struct AmmoModifiers {
    std::array<int,6> add{}; // 部件固定增量。
    std::array<float,6> multiplier{1,1,1,1,1,1}; // 部件倍率，应用于原机体载量。
    std::array<unsigned char,16> tail{0,0,0,0,0,0,0,0,0,0,0,0,0,100,0,0}; // 原生构造写 +0x3c 的 0x6400。
};
static_assert(sizeof(AmmoModifiers)==64);

// HangarStatus 的原生弹药快照；只复制已核对的 0x34 字节，不解释其后的更新标记。
struct PartsAmmoContext {
    int plane{},weapon{},reserved{},candidate{}; // 机体、原生 SP、未使用字段、光标候选。
    std::array<int,8> parts{}; // 原生上下文列表，替换/卸件时不擅自改成 GameInstance 列表。
    int count{}; // 有效部件数，范围 0 至 8。
};
static_assert(sizeof(PartsAmmoContext)==0x34);

// PART 只读查询：复用原生 A/B 修正，既不调用绘制入口，也不临时装备部件。
extern "C" __declspec(dllexport) int dualsp_parts_ammo_preview(void* luaState) {
    const dualsp::LuaApi* api=nullptr; // 结构或签名变化明确报错，禁止返回猜测数量。
    try {
        static const dualsp::LuaApi bound;api=&bound; // 游戏线程调用，沿用专用加载器 ABI。
        require(runtimeEnabled,"Mod disabled outside single-player");
        require(api->top(luaState)==11,"PART 弹药预览参数数量无效");verifyVersion();
        const auto page=static_cast<Addr>(api->readInteger(luaState,1)); // Lua 已核对当前可见 PART 页和世界。
        const auto status=static_cast<Addr>(api->readInteger(luaState,2)); // 页面反射属性 HangarStatus。
        const auto board=static_cast<Addr>(api->readInteger(luaState,3)); // status.BoardWeaponsAmmo。
        const auto instance=static_cast<Addr>(api->readInteger(luaState,4)); // 当前 GameInstance。
        const auto planeId=api->readInteger(luaState,5); // 防止换机期间混用旧上下文。
        const auto base=reinterpret_cast<Addr>(GetModuleHandleW(nullptr)); // 本 build 的原生纯计算函数。
        require(read<Addr>(page+0x4f8)==status && read<Addr>(board+0x28)==status,"PART 预览对象归属改变");
        require(read<Addr>(read<Addr>(status)+0x258)==base+0x8df120 &&
            read<Addr>(read<Addr>(board)+0x258)==base+0x8df100,"PART 预览上下文访问器不匹配");
        require(read<int>(instance+0x4dc)==planeId,"PART 预览与当前所选机体不一致");
        constexpr unsigned char contextSig[]={0x48,0x8d,0x41,0x4c,0xc3};
        constexpr unsigned char partSig[]={0x40,0x53,0x48,0x83,0xec,0x20,0x48,0x8b,0xda,0xe8,0xf2,0x8f,0xca,0xff};
        constexpr unsigned char applySig[]={0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x74,0x24,0x10,0x48,0x89,0x7c,0x24,0x18};
        // Board 更新入口以 +0x58==2 选择模式 3，否则模式 2；核对这一段后才解释字段。
        constexpr unsigned char modeSig[]={0x33,0xc9,0x4c,0x8d,0x43,0x50,0x83,0x7b,0x58,0x02,0x0f,0x94,0xc1,0x83,0xc1,0x02};
        require(std::memcmp(reinterpret_cast<void*>(base+0x8df120),contextSig,sizeof(contextSig))==0 &&
            std::memcmp(reinterpret_cast<void*>(base+0x84e770),partSig,sizeof(partSig))==0 &&
            std::memcmp(reinterpret_cast<void*>(base+0x84dc40),applySig,sizeof(applySig))==0 &&
            std::memcmp(reinterpret_cast<void*>(base+0x6ec5ee),modeSig,sizeof(modeSig))==0,"PART 原生预览入口不匹配");
        const auto context=read<PartsAmmoContext>(status+0x4c); // 同一游戏线程一次复制，不跨页缓存裸指针。
        const int mode=read<int>(board+0x58)==2 ? 3 : 2; // 与 0x6ec5c0 完全相同，不读取可能未刷新的 +0x50。
        Json result={{"pending",context.plane!=planeId},{"plane",context.plane}}; // 正常换机尚未同步时明确等待。
        if(context.plane==planeId) {
            require(context.count>=0 && context.count<=8,"PART 上下文部件数量异常");
            AmmoModifiers installed,candidate,empty; // 每次从原生默认值重建，绝不修改实际配装。
            using BuildPart=void(*)(int,AmmoModifiers*); // 0x84e770 按单个 PartsID 查实际加载的部件表。
            using Apply=bool(*)(unsigned char,int*,const AmmoModifiers*,const AmmoModifiers*); // 返回候选适用标记。
            const auto build=reinterpret_cast<BuildPart>(base+0x84e770); // 栈上修正的唯一来源。
            const auto apply=reinterpret_cast<Apply>(base+0x84dc40); // 保留原生截断和偶数规则。
            bool alreadyInstalled=false; // 原生禁止候选在上下文中重复加成。
            for(int i=0;i<context.count;++i) {
                alreadyInstalled=alreadyInstalled || context.parts[i]==context.candidate;
                build(context.parts[i],&installed);
            }
            if(mode==3 && !alreadyInstalled)build(context.candidate,&candidate);
            Json amounts=Json::array(); // 三种原生配置各自的完整载量，均分仍由 Lua 负责。
            for(int i=0;i<3;++i) {
                const auto weapon=api->readInteger(luaState,6+i*2); // 实际 WeaponID，不能使用 PWDT 配置序号。
                const auto baseline=api->readInteger(luaState,7+i*2); // 不含任何已装或候选修正的基础载量。
                require(weapon>=0 && weapon<102 && dualsp::validAmmoCount(baseline),"PART 兵装参数异常");
                int current=static_cast<int>(baseline),preview=current; // 两次计算分别从基础值开始。
                if(current<0) { // 无限基础载量不参与部件加法、倍率或偶数修正。
                    amounts.push_back({{"current",current},{"preview",preview},{"affects",false}});continue;
                }
                apply(static_cast<unsigned char>(weapon),&current,&installed,&empty);
                const bool affects=apply(static_cast<unsigned char>(weapon),&preview,&installed,&candidate); // 取整相同也可为 true。
                require(dualsp::validAmmoCount(current) && dualsp::validAmmoCount(preview),"PART 原生载量结果异常");
                amounts.push_back({{"current",current},{"preview",preview},{"affects",affects}});
            }
            result["candidate"]=context.candidate;result["mode"]=mode;result["amounts"]=std::move(amounts);
        }
        const auto encoded=result.dump(); // 只返回展示快照，不保存 GameInstance/Flow/任务状态。
        api->boolean(luaState,true);api->string(luaState,encoded.data(),encoded.size());return 2;
    }catch(const std::exception& error) {
        nativeLog(std::string("PARTS_AMMO_PREVIEW_ERROR ")+error.what());
        if(api){api->boolean(luaState,false);api->string(luaState,error.what(),std::strlen(error.what()));return 2;}
        return 0;
    }
}

// 三页共用原生预览算法；仅修改栈上局部数字，不初始化/重建实际武器。
extern "C" __declspec(dllexport) int dualsp_loadout_ammo(void* luaState) {
    const dualsp::LuaApi* api=nullptr; // 失败让 Lua 明确停用此预览，不显示基础数冒充实际值。
    try {
        static const dualsp::LuaApi bound;api=&bound; // 专用加载器 ABI。
        require(runtimeEnabled,"Mod disabled outside single-player");
        require(api->top(luaState)==8,"配弹预览参数数量无效");verifyVersion();
        const auto instance=static_cast<Addr>(api->readInteger(luaState,1)); // 已选机体和部件的唯一来源。
        const auto planeId=api->readInteger(luaState,2); // 防止切换机体时混入上一机体的部件。
        require(read<int>(instance+0x4dc)==planeId,"配弹预览机体与当前选择不一致");
        const auto count=read<int>(instance+0x514); // 原生最多八个部件槽，数组从 +0x4f4 开始。
        require(count>=0 && count<=8,"配弹预览部件槽数量异常");checkMemory(instance+0x4f4,count*4);
        const auto base=reinterpret_cast<Addr>(GetModuleHandleW(nullptr)); // 只接受已核对的纯计算入口。
        constexpr unsigned char buildSig[]={0x48,0x85,0xc9,0x0f,0x84,0xa5,0x01,0,0};
        constexpr unsigned char applySig[]={0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x74,0x24,0x10,0x48,0x89,0x7c,0x24,0x18};
        require(std::memcmp(reinterpret_cast<void*>(base+0x84e050),buildSig,sizeof(buildSig))==0 &&
            std::memcmp(reinterpret_cast<void*>(base+0x84dc40),applySig,sizeof(applySig))==0,"原生弹药预览入口不匹配");
        AmmoModifiers parts,extra; // 部件加成与空的额外加成，初始化与原生补给相同。
        using Build=void(*)(void*,AmmoModifiers*); // 只读 GameInstance 中当前配装。
        using Apply=bool(*)(unsigned char,int*,const AmmoModifiers*,const AmmoModifiers*); // 保留原生取整及成对修正。
        reinterpret_cast<Build>(base+0x84e050)(reinterpret_cast<void*>(instance),&parts);
        Json amounts=Json::array(); // 返回三种候选各自的完整载量，配弹比例仍在 Lua 中应用。
        for(int i=0;i<3;++i) {
            const auto weapon=api->readInteger(luaState,3+i*2); // 原生 WeaponID，不是机体槽号。
            const auto baseline=api->readInteger(luaState,4+i*2); // 原机体数据表的完整载量。
            require(weapon>=0 && weapon<102 && dualsp::validAmmoCount(baseline),"配弹预览兵装参数异常");
            int amount=static_cast<int>(baseline); // 计算只影响这个局部变量。
            if(amount>=0)reinterpret_cast<Apply>(base+0x84dc40)(static_cast<unsigned char>(weapon),&amount,&parts,&extra);
            require(dualsp::validAmmoCount(amount),"原生弹药预览结果异常");amounts.push_back(amount);
        }
        const auto encoded=amounts.dump(); // 小型结果走现有 Lua 字串接口。
        api->boolean(luaState,true);api->string(luaState,encoded.data(),encoded.size());return 2;
    }catch(const std::exception& error) {
        nativeLog(std::string("LOADOUT_AMMO_ERROR ")+error.what());
        if(api){api->boolean(luaState,false);api->string(luaState,error.what(),std::strlen(error.what()));return 2;}
        return 0;
    }
}

// action=1/2 排队决定/返回，3 更新原生光标预览，0 查询回执，-1 取消输入交接。
// 返回成功、是否已收到原生跳转、错误；只由游戏线程调用，不模拟系统按键。
extern "C" __declspec(dllexport) int dualsp_preparation_action(void* luaState) {
    const dualsp::LuaApi* api=nullptr; // 导出层统一向 Lua 返回可见错误。
    try {
        static const dualsp::LuaApi bound; // 已有专用加载器 ABI。
        api=&bound;
        require(api->top(luaState)==3,"机库动作接口参数数量无效");
        const auto widget=static_cast<Addr>(api->readInteger(luaState,1)); // 当前页面身份。
        const int weapon=static_cast<int>(api->readInteger(luaState,2)); // 最终 SP1 或当前预览的原生编号。
        const int action=static_cast<int>(api->readInteger(luaState,3)); // 请求或查询类型。
        require(widget && action>=-1 && action<=3,"机库动作参数无效");
        bool accepted=false; // 只有已交付边沿且原生设置跳转字段才算接收。
        if(action==-1) {
            require(!preparationAction.widget || preparationAction.widget==widget,"取消的机库动作身份不匹配");
            preparationAction={}; // 取消不解引用可能已销毁的页面。
        } else if(action==0) {
            require(runtimeEnabled,"Mod disabled outside single-player");
            require(preparationAction.widget==widget,"机库动作请求已经丢失");
            if(preparationAction.rejected) {
                // 动画或快速返回时原生可以忽略本次边沿；不等于兵装或结构损坏。
                nativeLog("PREPARATION_ACTION_IGNORED");preparationAction={};
                api->boolean(luaState,true);api->boolean(luaState,false);
                api->string(luaState,"native_ignored",14);return 3;
            }
            accepted=preparationAction.accepted; // 领取同一原生帧记录的结果，不读取已关闭页面。
            if(accepted)preparationAction={};
        } else {
            require(runtimeEnabled,"Mod disabled outside single-player");
            verifyVersion();
            const auto base=reinterpret_cast<Addr>(GetModuleHandleW(nullptr)); // 已核对的游戏映像。
            const auto table=read<Addr>(widget); // 必须匹配兵装页的 Tick 与决定/返回回调。
            require(read<Addr>(table+0x420)==base+0x6d4640 && read<Addr>(table+0x460)==base+0x6d3d00 &&
                read<Addr>(table+0x468)==base+0x6d3db0 && read<Addr>(table+0x478)==base+0x6d4590,
                "机库兵装页面类型不匹配");
            {
                require(!preparationAction.widget,"已有待完成的机库动作");
                require(read<unsigned char>(widget+0x3b0)!=0 && read<unsigned char>(widget+0x3c5)==0 &&
                    read<unsigned char>(widget+0x3c6)==0,"机库页面正在切换，不能重复决定");
                if(action!=3)installPreparationHooks();
                if(action==1 || action==3) {
                    require(weapon>=0 && weapon<3,"机库兵装编号无效");
                    const auto choices=read<Addr>(widget+0x4f0); // 原生可选择兵装编号表。
                    const int count=read<int>(widget+0x4f8); // 兵装编号表只含当前机体的可选择行。
                    const int menuCount=read<int>(widget+0x440); // 菜单定义表还包含其他行，现场值为 240，并非三个兵装。
                    require(choices && count>0 && count<=3 && read<int>(widget+0x4fc)>=count &&
                        menuCount>=count && read<int>(widget+0x444)>=menuCount,"机库兵装列表结构不符");
                    int selected=-1; // 从原生编号反查行，不能假定购买状态不影响行序。
                    for(int index=0;index<count;++index)if(read<int>(choices+index*4)==weapon)selected=index;
                    require(selected>=0,"原生机库没有指定的兵装预览行");
                    const auto item=read<Addr>(widget+0x438)+selected*0x720; // 原生行的禁用与目的页字段。
                    if(action==1)require(!read<unsigned char>(item+9) && !read<unsigned char>(item+0xa) && !read<unsigned char>(item+0xb) &&
                        read<unsigned char>(item+0x688)!=0,"原生 SP1 行不可决定");
                    write<int>(widget+0x2f0,selected); // 原生决定回调自行提交此行，不改存档校验字段。
                    if(action==3) {
                        using CursorChanged=void(*)(void*); // 原生导航使用同一虚表回调更新预览与镜头。
                        reinterpret_cast<CursorChanged>(base+0x6d4590)(reinterpret_cast<void*>(widget));
                        // 回调只更新机库临时预览；确认另走 +0x460，返回 +0x468 会恢复旧预览。
                    }
                }
                if(action!=3) {
                    preparationAction={widget,static_cast<unsigned char>(action),false};
                    nativeLog(action==1?"PREPARATION_DECIDE_QUEUED":"PREPARATION_BACK_QUEUED");
                }
            }
        }
        api->boolean(luaState,true);api->boolean(luaState,accepted);api->string(luaState,"",0);return 3;
    }catch(const std::exception& error) {
        preparationAction={};
        nativeLog(std::string("PREPARATION_ACTION_ERROR ")+error.what());
        if(api){api->boolean(luaState,false);api->boolean(luaState,false);api->string(luaState,error.what(),std::strlen(error.what()));return 3;}
        return 0;
    }
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
            require(runtimeEnabled,"Mod disabled outside single-player");
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

// Lua的出击/补给初始化同样先完整解除锁定；所有机体共用，不依赖多SP模型。
extern "C" __declspec(dllexport) int dualsp_prepare_weapon_rebuild(void* luaState) {
    const dualsp::LuaApi* api=nullptr; // 失败返回明确错误，不清零或静音回退。
    try {
        static const dualsp::LuaApi bound; // 复用加载器接口。
        api=&bound;
        require(runtimeEnabled,"Mod disabled outside single-player");
        require(api->top(luaState)==2,"兵装重建收尾接口参数数量无效");
        verifyVersion();
        const auto plane=static_cast<Addr>(api->readInteger(luaState,1)); // 当前玩家。
        const auto activator=static_cast<Addr>(api->readInteger(luaState,2)); // 当前武器组件。
        prepareSeekerRebuild(plane,activator);
        api->boolean(luaState,true);api->string(luaState,"",0);
        return 2;
    }catch(const std::exception& error) {
        nativeLog(std::string("SEEKER_REBUILD_ERROR ")+error.what());
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
        require(runtimeEnabled,"Mod disabled outside single-player");
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

// 兵装事务结束后使原生装填剪影失效；不改弹药、窗口数、冷却或实体模型。
extern "C" __declspec(dllexport) int dualsp_refresh_weapon_icons(void* luaState) {
    const dualsp::LuaApi* api=nullptr; // 错误交给 HUD 模块明确报告，不静默沿用旧剪影。
    try {
        static const dualsp::LuaApi bound;api=&bound; // 调用方必须在游戏线程提交最终选择。
        std::lock_guard<std::mutex> guard(nativeExecutionMutex); // 与兵装重建使用同一事务边界。
        require(runtimeEnabled,"Mod disabled outside single-player");
        require(api->top(luaState)==3,"装填剪影接口参数数量无效");verifyVersion();
        const auto plane=static_cast<Addr>(api->readInteger(luaState,1)); // 当前玩家机体。
        const auto activator=static_cast<Addr>(api->readInteger(luaState,2)); // HUD 与核心共同绑定的组件。
        const auto widget=static_cast<Addr>(api->readInteger(luaState,3)); // Lua 已核对当前世界的 CombinedStatsWidget。
        require(session.plane==plane && session.activator==activator,"装填剪影兵装会话不符");
        require(read<Addr>(plane+0x11a8)==activator && read<Addr>(widget+0x210)==plane &&
            read<Addr>(widget+0x2b0)==activator,"装填剪影 HUD 玩家身份不符");
        const auto icons=widget+0x420; // PersistentUserWidget 内的原生装填图管理器。
        require(read<Addr>(icons+0x98)==activator && read<Addr>(icons+0xa0)==plane,
            "装填剪影内部玩家引用不符");
        const auto base=reinterpret_cast<Addr>(GetModuleHandleW(nullptr)); // 仅支持已研究的 Steam build。
        const auto changed=read<Addr>(read<Addr>(widget)+0x440); // 原生 OnSpecialWeaponChanged 的虚方法。
        constexpr unsigned char method[]={0x48,0x81,0xc1,0x20,0x04,0,0,0xe9};
        constexpr unsigned char refresh[]={0x48,0x83,0xec,0x38,0x4c,0x8b,0x91,0x98,0,0,0};
        constexpr unsigned char dirty[]={0x38,0x47,0x54,0x75,0x0c,0x84,0xc0};
        require(changed==base+0x3f2320 &&
            std::memcmp(reinterpret_cast<void*>(changed),method,sizeof(method))==0 &&
            std::memcmp(reinterpret_cast<void*>(base+0x49f2f0),refresh,sizeof(refresh))==0 &&
            std::memcmp(reinterpret_cast<void*>(base+0x49edc1),dirty,sizeof(dirty))==0,
            "原生装填剪影刷新入口不匹配");
        using Changed=void(*)(void*); // 原生无参回调从当前武器读取背景/填充贴图。
        reinterpret_cast<Changed>(changed)(reinterpret_cast<void*>(widget));
        const auto armed=read<unsigned char>(activator+0x2a1); // 原生重绘条件是该标记等于当前类别。
        require(armed<=1,"装填剪影武器类别无效");
        // SP→SP 的最终类别不变，原生不会自行重新显示先前隐藏的第 3–6 枚图标。
        // 标记下一次原生更新重绑贴图和显隐；数量与进度仍取实际武器装填窗口。
        write<unsigned char>(icons+0x54,armed);
        api->boolean(luaState,true);api->string(luaState,"",0);return 2;
    }catch(const std::exception& error) {
        nativeLog(std::string("HUD_ICONS_ERROR ")+error.what());
        if(api){api->boolean(luaState,false);api->string(luaState,error.what(),std::strlen(error.what()));return 2;}
        return 0;
    }
}

// 游戏线程只读采样，直接返回 Lua 值，不经过命令文件或 JSON。
// 返回：成功、MSL就绪、SP1就绪、SP2就绪、SP3就绪、错误文字；不修改武器或核心缓存。
extern "C" __declspec(dllexport) int dualsp_hud_readiness(void* luaState) {
    const dualsp::LuaApi* api=nullptr;
    try {
        // 接口解析每个进程只做一次；加载器不匹配则明确报错。
        static const dualsp::LuaApi bound;
        api=&bound;
        std::lock_guard<std::mutex> guard(nativeExecutionMutex);
        require(runtimeEnabled,"Mod disabled outside single-player");
        require(continuousRuntimeFailure.empty(),continuousRuntimeFailure.c_str());
        continuousSweep();
        require(api->top(luaState)==4,"HUD 内存接口参数数量无效");
        const auto plane=static_cast<Addr>(api->readInteger(luaState,1));
        const auto activator=static_cast<Addr>(api->readInteger(luaState,2));
        const double time=api->readNumber(luaState,3);
        const int count=static_cast<int>(api->readInteger(luaState,4)); // Lua 提供实际挂载数。
        require(count>=1 && count<=3,"HUD SP 数量无效");
        require(std::isfinite(time) && time>=0,"HUD 游戏时间无效");
        verifyVersion();
        require(read<Addr>(plane+0x11a8)==activator,"HUD 激活器不属于当前玩家机体");
        const auto main=capture(activator,false,time),special=capture(activator,true,time);
        const bool mainReady=hasReadyRound(main,time);
        std::array<bool,3> spReady{}; // 未搭载槽固定不就绪。
        require(session.plane==plane && session.activator==activator && session.count==count,"HUD SP 会话不匹配");
        require(session.active>=0 && session.active<count,"HUD 活动 SP 无效");
        cfaObserveWeapons(plane,activator,main,special,time);
        for(int index=0;index<count;++index)
            spReady[index]=hasReadyRound(index==session.active ? special : session.special[index],time);
        api->boolean(luaState,true);api->boolean(luaState,mainReady);
        for(bool ready:spReady)api->boolean(luaState,ready);
        api->string(luaState,"",0);return 6;
    }catch(const std::exception& error) {
        if(api) {
            api->boolean(luaState,false);
            for(int index=0;index<4;++index)api->boolean(luaState,false);
            api->string(luaState,error.what(),std::strlen(error.what()));
            return 6;
        }
        // Lua 接口本身不能绑定时无法压栈；调度器会因缺少成功值明确显示 HUD 错误。
        nativeLog(std::string("HUD_LUA_API_ERROR ")+error.what());
        return 0;
    }
}

extern "C" __declspec(dllexport) int lab_native_execute(void*) {
    std::lock_guard<std::mutex> guard(nativeExecutionMutex);
    continuousRebuildStarted=false;
    cfaSwitchCost.begin(false);
    uint64_t request=0;
    bool transientSwitch=false,completed=false;
    std::string result;
    try {
        const auto command=readCommand();
        request=command.request;
        require(runtimeEnabled,"Mod disabled outside single-player");
        transientSwitch=command.op=="switch";
        verifyVersion();
        cfaSwitchCost.begin(transientSwitch && continuousCfa44(command.plane));
        cfaSwitchCost.target=command.configs[command.target];
#ifdef DUALSP_SU30SM_MOUNT_PROTOTYPE
        cfaSwitchCost.models=fixedMountEnabled && fixedMount.bound && !fixedMountFailed;
#endif
        if(command.op!="snapshot")nativeLog(Json{{"event","NATIVE_BEGIN"},{"op",command.op},{"request",request},
            {"target",command.target},{"configs",command.configs},{"count",command.count},{"thread",GetCurrentThreadId()}}.dump());
        execute(command);
        Json state=Json::parse("{"+snapshotJson(command)+"}");
        state["count"]=session.count;state["configs"]=session.configs;state["ok"]=true;state["request"]=request;state["maximum"]=session.maximum;
        state["ratio"]=session.ratio;
#ifdef DUALSP_SU30SM_MOUNT_PROTOTYPE
        state["fixed_mount_warning"]=fixedMountWarning; // 降级提示不伪装成兵装事务失败。
#endif
        state["slots"]=Json::array();
        for(int index=0;index<session.count;++index) {
            const auto g=index==session.active?capture(command.activator,true,command.time):session.special[index];
            Json bars=Json::array();
            if(g.valid) {
                const int next=scalarInt(g,12),loaded=scalarInt(g,8);
                require(loaded>=0 && loaded<=static_cast<int>(g.slots.size()) && next>=0 && next<static_cast<int>(g.slots.size()),"Invalid HUD launch cursor");
                for(int k=0;k<loaded;++k) {
                    const auto& s=g.slots[(next+k)%g.slots.size()];
                    const float elapsed=cachedReload(g,s,std::max(0.0,command.time-g.capturedTime),true);
                    bars.push_back(dualsp::reloadProgress(s.duration,elapsed));
                }
            }
            state["slots"].push_back(bars);
        }
        result=state.dump();
        completed=true;
        if(command.op!="snapshot")nativeLog(Json{{"event","NATIVE_DONE"},{"op",command.op},{"request",request},
            {"active",session.active},{"main_ammo",state["main_ammo"]},{"sp",state["cached_sp"]}}.dump());
    } catch(const std::exception& error) {
        completed=false;
        if(runtimeEnabled && continuousRebuildStarted) {
            continuousAbortRetained();ravenAbortRebuild(); // 已重建后失败才清理，前置拒绝不结束在途效果。
        }
#ifdef DUALSP_SU30SM_MOUNT_PROTOTYPE
        fixedMountFail(error.what()); // 原型错误写日志并归还原生显示，既有错误回执继续向用户报告。
#endif
        result=Json{{"ok",false},{"request",request},{"error",error.what()}}.dump();nativeLog(result);
    }
    // A reader must see an entire response. Include the request ID so an I/O
    // failure cannot cause the controller to accept a previous successful reply.
    // 切换回执是同进程即时交接，不需要断电持久化；仍原子替换，并由Lua核对请求编号。
    try {atomicJson(modRoot()/"native-status.json",Json::parse(result),!transientSwitch);}
    catch(const std::exception& e){completed=false;nativeLog(std::string("NATIVE_RESPONSE_WRITE_ERROR ")+e.what());}
    cfaSwitchCost.mark(8);cfaSwitchCost.finish(request,completed);
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
        verifyVersion();Addr planes{},weapons{},skins{};
        std::ifstream cmd(modRoot()/"catalogue-command.txt");cmd>>std::hex>>planes>>weapons;
        require(static_cast<bool>(cmd),"Invalid catalogue command");
        cmd>>skins; // 旧调用仍可读取基础目录；新Lua通过能力标记要求本版核心。
        int dlcOffset=-1;std::string dlcError;
        try {
            require(!mountHooksReady || GetCurrentThreadId()==mountGameThread.load(),"机体目录需要在游戏线程读取");
            require(mountClassIs(planes,"DataTable","/Script/Engine"),"机体目录：机体表类型不符");
            const Addr rowType=read<Addr>(planes+mountProperty(planes,"RowStruct","ObjectProperty",8));
            MountAnimationReflection::named(rowType,"PlayerPlaneDataTable","/Script/Nimbus");
            // 目录早于任务绑定；只绑定只读元数据getter，不安装挂钩或取得写权限。
            MountAnimationReflection metadata;
            const auto loader=GetModuleHandleW(L"UE4SS.dll");require(loader!=nullptr,"机体目录：加载器未就绪");
            metadata.extent=reinterpret_cast<MountAnimationReflection::Ref>(GetProcAddress(loader,
                "?GetPropertiesSize@UStruct@Unreal@RC@@QEBAAEBHXZ"));
            metadata.arrayDim=reinterpret_cast<MountAnimationReflection::Ref>(GetProcAddress(loader,
                "?GetArrayDim@FProperty@Unreal@RC@@QEBAAEBHXZ"));
            dlcOffset=metadata.field(rowType,"DLCID","StrProperty",16,metadata.typeSize(rowType)).offset;
        }catch(const std::exception& error) {
            // 换机扩展不可用时保留原有兵装目录；不能因此停用所有出击/补给功能。
            dlcError=error.what();nativeLog("DEBUG_AIRCRAFT_CATALOGUE_ERROR "+dlcError);
        }
        // 只读接入真实机体/涂装引用，避免由机体代号猜类名。布局不符时不猜偏移。
        int referenceOffset=-1;Json skinReferences=Json::object();
        std::string referenceError;
        auto softReference=[](Addr address) {
            // 本项目限定的UE4.18 FSoftObjectPtr为40字节：弱引用/标记16字节＋FSoftObjectPath。
            checkMemory(address,40);
            const auto index=read<unsigned>(address+16),number=read<unsigned>(address+20);
            if(!index)return std::string{};
            auto name=mountNameIndex(index);
            if(name=="None")return std::string{};
            require(number<100000 && unrealString(address+24).empty(),"机体目录：软引用包含未知子路径");
            if(number)name+="_"+std::to_string(number-1);
            require(name.rfind("/Game/",0)==0 && name.size()<1024,"机体目录：软引用路径无效");
            return name;
        };
        try {
            require(!mountHooksReady || GetCurrentThreadId()==mountGameThread.load(),"机体引用需要在游戏线程读取");
            MountAnimationReflection metadata;
            const auto loader=GetModuleHandleW(L"UE4SS.dll");require(loader!=nullptr,"机体引用：加载器未就绪");
            metadata.extent=reinterpret_cast<MountAnimationReflection::Ref>(GetProcAddress(loader,
                "?GetPropertiesSize@UStruct@Unreal@RC@@QEBAAEBHXZ"));
            metadata.arrayDim=reinterpret_cast<MountAnimationReflection::Ref>(GetProcAddress(loader,
                "?GetArrayDim@FProperty@Unreal@RC@@QEBAAEBHXZ"));
            auto rowType=[&](Addr table,const char* expected) {
                require(mountClassIs(table,"DataTable","/Script/Engine"),"机体引用：数据表类型不符");
                const Addr type=read<Addr>(table+mountProperty(table,"RowStruct","ObjectProperty",8));
                MountAnimationReflection::named(type,expected,"/Script/Nimbus");return type;
            };
            const auto planeType=rowType(planes,"PlayerPlaneDataTable");
            try {referenceOffset=metadata.field(planeType,"Reference","SoftObjectProperty",40,metadata.typeSize(planeType)).offset;}
            catch(const std::exception& error) {referenceError=error.what();nativeLog("MOD_AIRCRAFT_REFERENCE_ERROR "+referenceError);}
            if(skins) {
                const auto skinType=rowType(skins,"SkinDataTable");const int size=metadata.typeSize(skinType);
                const auto code=metadata.field(skinType,"PlaneStringID","StrProperty",16,size);
                const auto number=metadata.field(skinType,"SkinNo","IntProperty",4,size);
                const auto reference=metadata.field(skinType,"PlaneReference","StrProperty",16,size);
                for(const auto row:tableRows(skins)) {
                    std::string planeCode;int skinNo=-1;
                    try {
                        planeCode=unrealString(row+code.offset);skinNo=read<int>(row+number.offset);
                        require(!planeCode.empty() && planeCode.size()<=128 && skinNo>=0 && skinNo<100000,
                                "机体目录：涂装身份无效");
                        const auto path=unrealString(row+reference.offset);
                        require(path.rfind("/Game/",0)==0 && path.size()<1024,"机体目录：涂装引用无效");
                        if(!skinReferences.contains(planeCode))skinReferences[planeCode]=Json::array();
                        skinReferences[planeCode].push_back({{"skin_no",skinNo},{"reference",path}});
                    }catch(const std::exception& error) {
                        // 已知编号的坏引用必须保留拒绝标记，不能伪装成没有此涂装而回退基础类。
                        if(!planeCode.empty() && planeCode.size()<=128 && skinNo>=0 && skinNo<100000) {
                            if(!skinReferences.contains(planeCode))skinReferences[planeCode]=Json::array();
                            skinReferences[planeCode].push_back({{"skin_no",skinNo},{"reference_error",error.what()}});
                        }
                        nativeLog(std::string("MOD_SKIN_ROW_SKIPPED ")+error.what());
                    }
                }
            }
        }catch(const std::exception& error) {
            referenceError=error.what();nativeLog("MOD_AIRCRAFT_REFERENCE_ERROR "+referenceError);
        }
        std::map<std::string,Json> byCode;
        std::set<std::string> duplicateWeaponCodes;
        for(const auto row:tableRows(weapons)) {
            try {
                const auto code=unrealString(row+16);
                require(!code.empty() && code.size()<=128,"机体目录：兵装代号无效");
                require(!duplicateWeaponCodes.count(code),"机体目录：重复兵装代号");
                if(byCode.count(code)) {
                    byCode.erase(code);duplicateWeaponCodes.insert(code);
                    throw std::runtime_error("机体目录：重复兵装代号 "+code);
                }
                Json stats=Json::array();for(int i=0;i<6;++i)stats.push_back(read<unsigned char>(row+40+i));
                // 分类图标和类型键读取原生武器表，避免另维护一份兵装映射。
                byCode[code]={{"weapon_id",read<int>(row+8)},{"stats",stats},
                    {"category",read<unsigned char>(row+32)},{"type_key",unrealString(row+224)}};
            }catch(const std::exception& error) {nativeLog(std::string("MOD_WEAPON_ROW_SKIPPED ")+error.what());}
        }
        Json catalog=Json::object();
        for(const auto row:tableRows(planes)) {
            int id=-1;
            try {
                id=read<int>(row+8);require(id>=0,"机体目录：机体编号无效");
                const auto key=std::to_string(id);
                if(catalog.contains(key)) {
                    catalog[key]["supported"]=false;catalog[key]["compatibility_error"]="重复机体编号";
                    nativeLog("MOD_AIRCRAFT_DUPLICATE_ID "+key);continue;
                }
                const auto planeCode=unrealString(row+16);
                require(!planeCode.empty() && planeCode.size()<=128,"机体目录：机体代号无效");
                Json options=Json::array();bool supported=true,infinite=false;std::array<int,3> maxima{};
                for(int i=0;i<3;++i) {
                    const auto code=unrealString(row+96+i*24);
                    if(code=="-1") {supported=false;continue;}
                    if(!byCode.count(code))throw std::runtime_error("机体目录：兵装引用不存在 "+code);
                    const int maximum=read<int>(row+112+i*24);
                    require(dualsp::validAmmoCount(maximum),"机体目录：槽位基础载量无效");
                    maxima[i]=maximum;infinite=infinite || maximum<0;
                    auto option=byCode.at(code);option["index"]=i;option["code"]=code;option["max"]=maximum;options.push_back(option);
                }
                auto& entry=catalog[key];
                entry={{"id",id},{"code",planeCode},{"main_max",read<int>(row+88)},
                    {"weapons",options},{"supported",supported}};
                if(dlcOffset>=0)entry["dlc_id"]=unrealString(row+dlcOffset);
                else entry["debug_aircraft_error"]=dlcError;
                if(referenceOffset>=0) {
                    try {entry["reference"]=softReference(row+referenceOffset);}
                    catch(const std::exception& error) {entry["reference_error"]=error.what();}
                }
                if(skinReferences.contains(planeCode))entry["skins"]=skinReferences[planeCode];
                if(infinite)nativeLog(Json{{"event","MOD_AIRCRAFT_INFINITE_AMMO"},{"id",id},{"code",planeCode},{"sp_max",maxima}}.dump());
            }catch(const std::exception& error) {
                if(id>=0)catalog[std::to_string(id)]={{"id",id},{"supported",false},{"compatibility_error",error.what()}};
                nativeLog("MOD_AIRCRAFT_ROW_UNAVAILABLE "+std::to_string(id)+" "+error.what());
            }
        }
        const auto planeCount=catalog.size();
        catalog["_compat"]={{"schema",1},{"weapons",byCode},{"reference_error",referenceError}};
        atomicJson(modRoot()/"catalogue.json",catalog);nativeLog("Catalogue loaded: "+std::to_string(planeCount)+" planes");
    }catch(const std::exception& e){nativeLog(std::string("CATALOGUE_ERROR ")+e.what());atomicJson(modRoot()/"catalogue.json",Json{{"error",e.what()}});}
    return 0;
}
// 新目录与Lua必须成套；不允许旧核心缺少真实引用时静默放宽机体身份。
extern "C" __declspec(dllexport) int dualsp_mod_aircraft_catalogue_supported(void*) {return 0;}
