#pragma once
float cameraHoldAxis(void* component,uint64_t name,float native);
void cameraHoldObservePad(DWORD result,const XINPUT_STATE* state);
void cameraHoldFailure(const char* reason);
// 已核对玩家的共用镜头轴入口；目标注视与环视保持按名称隔离，不改目标或镜头坐标。
// 本头文件包含在核心私有命名空间，复用已有弱身份与原生内存访问。
struct TargetGaze {
    ContinuousIdentity plane,input;
    Addr bindings{};
    int count{},capacity{};
    std::atomic<Addr> component{0};
    std::atomic<uint64_t> axis{0};
    std::atomic<DWORD> thread{0};
    std::atomic<bool> allowed{false},active{false};
    std::atomic<ULONGLONG> observed{0};
    std::atomic<ULONGLONG> heartbeat{0}; // 控制器停止调度时自动交还，不能留下永久注视。
    bool padEnabled{},padSeeded{},leftDown{},leftCandidate{},padCombo{},pendingPadToggle{};
    ULONGLONG started{};
    bool ready{};
    void clear() {
        allowed=false;active=false;component=0;axis=0;observed=0;heartbeat=0;
        plane={};input={};bindings=0;count=capacity=0;started=0;
        padEnabled=padSeeded=leftDown=leftCandidate=padCombo=pendingPadToggle=false;
    }
} targetGaze;
using TargetGazeAxis=float(*)(void*,uint64_t);
TargetGazeAxis originalTargetGazeAxis{};
using TargetGazePad=DWORD(WINAPI*)(DWORD,XINPUT_STATE*);
TargetGazePad originalTargetGazePad{};
Addr targetGazeImageBegin{},targetGazeImageEnd{};

bool targetGazeFocused() {
    DWORD process{};
    const auto window=GetForegroundWindow();
    return window && GetWindowThreadProcessId(window,&process) && process==GetCurrentProcessId();
}
bool targetGazeLive() {
    return targetGaze.allowed.load() && GetTickCount64()-targetGaze.heartbeat.load()<=500;
}
// 独立于短按注视开关；只观察原始L3，长按与双键组合会消费本轮短按。
struct ViewTargetPadState {
    Addr plane{};
    std::atomic<DWORD> thread{0};
    ULONGLONG heartbeat{},pressed{};
    bool allowed{},seeded{},leftDown{},combo{},candidate{},fired{},pending{},suppressShort{};
    void clear(){
        thread=0;
        if(allowed){targetGaze.leftCandidate=false;targetGaze.pendingPadToggle=false;}
        plane=0;heartbeat=pressed=0;allowed=seeded=leftDown=combo=candidate=fired=pending=suppressShort=false;
    }
} viewTargetPad;
void viewTargetObservePad(DWORD result,const XINPUT_STATE* state) {
    if(GetCurrentThreadId()!=viewTargetPad.thread.load())return;
    const auto now=GetTickCount64();
    if(!viewTargetPad.allowed||now-viewTargetPad.heartbeat>500||!targetGazeFocused()||result!=ERROR_SUCCESS||!state) {
        viewTargetPad.seeded=false;viewTargetPad.candidate=false;viewTargetPad.pending=false;
        viewTargetPad.suppressShort=true;return;
    }
    const auto buttons=state->Gamepad.wButtons;
    const bool left=(buttons&XINPUT_GAMEPAD_LEFT_THUMB)!=0,right=(buttons&XINPUT_GAMEPAD_RIGHT_THUMB)!=0;
    if(!viewTargetPad.seeded) {
        viewTargetPad.seeded=true;viewTargetPad.leftDown=left;viewTargetPad.combo=left||right;
        viewTargetPad.candidate=false;viewTargetPad.suppressShort=left||right;return;
    }
    if(left&&right) {
        viewTargetPad.combo=true;viewTargetPad.candidate=false;viewTargetPad.pending=false;viewTargetPad.suppressShort=true;
    }
    if(viewTargetPad.combo) {
        if(!left&&!right)viewTargetPad.combo=false;
    }else {
        if(left&&!viewTargetPad.leftDown) {
            viewTargetPad.pressed=now;viewTargetPad.candidate=!right;viewTargetPad.fired=false;viewTargetPad.suppressShort=false;
        }
        if(viewTargetPad.candidate&&!viewTargetPad.fired&&(left||viewTargetPad.leftDown)&&now-viewTargetPad.pressed>=2000) {
            viewTargetPad.fired=true;viewTargetPad.pending=true;viewTargetPad.suppressShort=true;
        }
        if(!left)viewTargetPad.candidate=false;
    }
    viewTargetPad.leftDown=left;
}
float targetGazeAxis(void* object,uint64_t name) {
    const float native=originalTargetGazeAxis(object,name);
    const float look=cameraHoldAxis(object,name,native); // 两种镜头功能共享入口，按轴名称与更新作用域隔离。
    if(!targetGazeLive()){targetGaze.active=false;return look;}
    if(reinterpret_cast<Addr>(object)!=targetGaze.component.load() ||
        name!=targetGaze.axis.load() || GetCurrentThreadId()!=targetGaze.thread.load())return look;
    targetGaze.observed=GetTickCount64();
    if(!targetGaze.active.load())return native;
    try {
        if(!targetGazeFocused() || !continuousAlive(targetGaze.plane) || !continuousAlive(targetGaze.input)) {
            targetGaze.active=false;return native;
        }
        return std::max(native,1.f); // 原生Y/F仍可按住；松开模组切换后立即回到真实轴值。
    }catch(...) {targetGaze.active=false;return native;} // 不让异常越过引擎输入栈。
}
// 只观察真实按键，不过滤X/L3/R3。双摇杆组合锁定到两边都松开，取消本次L3切换。
void targetGazeObservePad(DWORD result,const XINPUT_STATE* state) {
    viewTargetObservePad(result,state); // 必须先判长按，避免释放帧先执行短按注视。
    if(GetCurrentThreadId()!=targetGaze.thread.load())return;
    if(!targetGazeLive() || !targetGazeFocused()) {
        targetGaze.padSeeded=false;targetGaze.leftCandidate=false;targetGaze.pendingPadToggle=false;return;
    }
    if(result!=ERROR_SUCCESS || !state || !targetGaze.padEnabled) {
        targetGaze.padSeeded=false;targetGaze.leftCandidate=false;targetGaze.pendingPadToggle=false;return;
    }
    const auto buttons=state->Gamepad.wButtons;
    const bool left=(buttons&XINPUT_GAMEPAD_LEFT_THUMB)!=0,right=(buttons&XINPUT_GAMEPAD_RIGHT_THUMB)!=0;
    if(!targetGaze.padSeeded) {
        targetGaze.padSeeded=true;targetGaze.leftDown=left;targetGaze.leftCandidate=false;
        targetGaze.padCombo=left || right;return; // 恢复/重连时已按住的边沿不补发。
    }
    if(left && right) {targetGaze.padCombo=true;targetGaze.leftCandidate=false;targetGaze.pendingPadToggle=false;}
    if(targetGaze.padCombo) {
        targetGaze.leftCandidate=false;
        if(!left && !right)targetGaze.padCombo=false;
    }else {
        if(left && !targetGaze.leftDown)targetGaze.leftCandidate=!right;
        if(!left && targetGaze.leftDown && targetGaze.leftCandidate) {
            const bool consumed=GetCurrentThreadId()==viewTargetPad.thread.load()&&viewTargetPad.suppressShort;
            if(!consumed)targetGaze.pendingPadToggle=true;
            targetGaze.leftCandidate=false;
        }
    }
    targetGaze.leftDown=left;
}
DWORD WINAPI targetGazePad(DWORD index,XINPUT_STATE* state) {
    const auto result=originalTargetGazePad(index,state);
    if(index!=0)return result;
    const auto caller=reinterpret_cast<Addr>(__builtin_return_address(0));
    const bool fromGame=caller>=targetGazeImageBegin && caller<targetGazeImageEnd;
    if(fromGame) {
        targetGazeObservePad(result,state);
        try {cameraHoldObservePad(result,state);}catch(const std::exception& error){cameraHoldFailure(error.what());}
    }
    return result; // 原生轮切、干扰弹及UI均获得完整的原始按键。
}
void installTargetGaze() {
    if(targetGaze.ready)return;
    verifyVersion();
    const auto base=continuousBase();
    constexpr unsigned char axis[]={0x4c,0x8b,0xc1,0x0f,0x57,0xc0,0x85,0xd2,0x75,0x0b,0x48,0x8b,0xc2,0x48,0xc1,0xe8,0x20,0x85};
    constexpr unsigned char selected[]={0x48,0x8b,0x81,0x10,0x13,0,0,0x48,0x85,0xc0,0x74,0x08,0x48,0x8b,0x80,0x48,0x02,0,0};
    require(std::memcmp(reinterpret_cast<void*>(base+0x16c03b0),axis,sizeof(axis))==0 &&
        std::memcmp(reinterpret_cast<void*>(base+0x81c100),selected,sizeof(selected))==0,"目标注视接口与当前游戏不匹配");
    const auto module=GetModuleHandleW(L"xinput1_3.dll");
    const auto entry=module?GetProcAddress(module,"XInputGetState"):nullptr;
    require(entry!=nullptr,"目标注视缺少游戏使用的XInput接口");
    const auto status=MH_Initialize();
    require(status==MH_OK || status==MH_ERROR_ALREADY_INITIALIZED,"目标注视输入钩子初始化失败");
    const auto axisAddress=reinterpret_cast<void*>(base+0x16c03b0);
    require(MH_CreateHook(axisAddress,reinterpret_cast<void*>(&targetGazeAxis),
        reinterpret_cast<void**>(&originalTargetGazeAxis))==MH_OK,"目标跟踪轴接入失败");
    const auto padAddress=reinterpret_cast<void*>(entry);
    const auto padStatus=MH_CreateHook(padAddress,reinterpret_cast<void*>(&targetGazePad),reinterpret_cast<void**>(&originalTargetGazePad));
    if(padStatus!=MH_OK){MH_RemoveHook(axisAddress);throw std::runtime_error("手柄观察接口接入失败");}
    const auto dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    const auto pe=reinterpret_cast<const IMAGE_NT_HEADERS64*>(base+dos->e_lfanew);
    targetGazeImageBegin=base;targetGazeImageEnd=base+pe->OptionalHeader.SizeOfImage;
    if(MH_QueueEnableHook(axisAddress)!=MH_OK || MH_QueueEnableHook(padAddress)!=MH_OK || MH_ApplyQueued()!=MH_OK) {
        MH_DisableHook(axisAddress);MH_DisableHook(padAddress);MH_RemoveHook(axisAddress);MH_RemoveHook(padAddress);
        throw std::runtime_error("目标注视输入钩子启用失败");
    }
    targetGaze.ready=true;nativeLog("TARGET_GAZE_HOOKS_READY");
}
void bindTargetGaze(Addr plane) {
    require(mountClassIs(plane,"PlayerPlane","/Script/Nimbus"),"目标注视需要当前玩家飞机");
    require(!(read<unsigned>(plane+8)&0x30u),"目标注视不能使用默认机体或模板");
    const auto input=continuousIdentity(read<Addr>(plane+0x118));
    require(mountClassIs(input.address,"InputComponent","/Script/Engine"),"目标注视输入组件类型不符");
    require(read<Addr>(input.address+0x20)==plane,"目标注视输入组件不属于当前飞机");
    const Addr data=read<Addr>(input.address+0x110);
    const int count=read<int>(input.address+0x118),capacity=read<int>(input.address+0x11c);
    require(data && count>0 && count<=capacity && capacity<=128,"目标注视输入轴列表无效");
    uint64_t axis{};
    for(int index=0;index<count;++index) {
        const auto value=read<uint64_t>(data+index*0x70+8);
        if(mountNameIndex(static_cast<unsigned>(value))=="INP_CameraTrackTarget") {
            require(!axis || axis==value,"目标跟踪轴名称不一致");axis=value;
        }
    }
    require(axis!=0,"当前机体没有原生目标跟踪轴");
    targetGaze.clear();targetGaze.plane=continuousIdentity(plane);targetGaze.input=input;
    targetGaze.bindings=data;targetGaze.count=count;targetGaze.capacity=capacity;
    targetGaze.axis=axis;targetGaze.component=input.address;targetGaze.thread=GetCurrentThreadId();
}

// 允许状态独立于多SP会话，第13关可只用镜头功能；关闭核心兵装时仍须由Lua单独归还。
int targetGazeLua(void* luaState) {
    const dualsp::LuaApi* api=nullptr;
    try {
        static const dualsp::LuaApi bound;api=&bound;
        require(api->top(luaState)==4,"目标注视参数数量无效");
        const auto plane=static_cast<Addr>(api->readInteger(luaState,1));
        const auto allowed=api->readInteger(luaState,2),padEnabled=api->readInteger(luaState,3),toggle=api->readInteger(luaState,4);
        require((allowed==0 || allowed==1) && (padEnabled==0 || padEnabled==1) && (toggle==0 || toggle==1),"目标注视设置无效");
        std::string reason;bool padToggle=false;
        if(!allowed || !plane || !targetGazeFocused())targetGaze.clear();
        else {
            installTargetGaze();
            continuousIdentity(plane);
            const auto pendingInput=read<Addr>(plane+0x118);
            if(!pendingInput || read<int>(pendingInput+0x118)==0) {
                targetGaze.clear();
                api->boolean(luaState,true);api->boolean(luaState,false);api->string(luaState,"waiting_input",13);api->boolean(luaState,false);return 4;
            }
            const bool same=targetGaze.plane.address==plane && continuousAlive(targetGaze.plane) &&
                continuousAlive(targetGaze.input) && read<Addr>(plane+0x118)==targetGaze.input.address &&
                read<Addr>(targetGaze.input.address+0x20)==plane &&
                read<Addr>(targetGaze.input.address+0x110)==targetGaze.bindings &&
                read<int>(targetGaze.input.address+0x118)==targetGaze.count && read<int>(targetGaze.input.address+0x11c)==targetGaze.capacity;
            if(!same)bindTargetGaze(plane);
            const auto now=GetTickCount64();
            if(targetGaze.heartbeat.load() && now-targetGaze.heartbeat.load()>500) {
                targetGaze.padSeeded=false;targetGaze.pendingPadToggle=false;
            }
            targetGaze.heartbeat=now;targetGaze.allowed=true;
            if(targetGaze.padEnabled!=(padEnabled!=0))targetGaze.padSeeded=false;
            targetGaze.padEnabled=padEnabled!=0;
            XINPUT_STATE pad{};const auto result=originalTargetGazePad(0,&pad);targetGazeObservePad(result,&pad);
            padToggle=targetGaze.pendingPadToggle;targetGaze.pendingPadToggle=false;
            const auto seeker=read<Addr>(plane+0x1310);
            const auto selected=seeker?read<Addr>(seeker+0x248):0;
            bool hasTarget=false;
            if(selected)try {hasTarget=continuousAlive(continuousIdentity(selected));}catch(...) {}
            if(!hasTarget){targetGaze.active=false;reason="no_target";}
            else if(toggle || padToggle){targetGaze.active=!targetGaze.active.load();targetGaze.started=GetTickCount64();}
            if(targetGaze.active.load() && GetTickCount64()-targetGaze.started>2000) {
                const auto observed=targetGaze.observed.load();
                require(observed && GetTickCount64()-observed<2000,"未观察到原生目标跟踪轴读取，已停止注视");
            }
        }
        api->boolean(luaState,true);api->boolean(luaState,targetGaze.active.load());api->string(luaState,reason.data(),reason.size());api->boolean(luaState,padToggle);return 4;
    }catch(const std::exception& error) {
        targetGaze.clear();nativeLog(std::string("TARGET_GAZE_ERROR ")+error.what());
        if(api){api->boolean(luaState,false);api->boolean(luaState,false);api->string(luaState,error.what(),std::strlen(error.what()));api->boolean(luaState,false);return 4;}
        return 0;
    }
}
