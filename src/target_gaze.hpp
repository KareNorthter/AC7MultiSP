#pragma once
float cameraHoldAxis(void* component,uint64_t name,float native);
void cameraHoldObservePad(DWORD result,const XINPUT_STATE* state);
void cameraHoldFailure(const char* reason);
bool cameraHoldNativeSpecial();
// 只替换当前玩家的注视轴；Y长按与原生键盘注视轴共同控制镜头持续注视。
struct TargetGaze {
    ContinuousIdentity plane,input,selection,view;
    Addr bindings{};
    int count{},capacity{};
    std::atomic<Addr> component{0};
    std::atomic<uint64_t> axis{0};
    std::atomic<DWORD> thread{0};
    std::atomic<bool> allowed{false},active{false};
    std::atomic<ULONGLONG> observed{0},heartbeat{0};
    bool padSeeded{},padConnected{},yDown{},yCandidate{},yConsumed{},keyboardSeeded{},keyboardDown{},keyboardBlocked{},pendingToggle{};
    ULONGLONG yPressed{},yReleased{},started{};
    bool ready{};
    int inputs{}; // 位1：手柄Y，位2：游戏实际键盘注视键；关闭的来源继续使用原生按住注视。
    std::string error,notice;
    void cancelGesture() {
        padSeeded=padConnected=yDown=yCandidate=yConsumed=keyboardSeeded=keyboardDown=keyboardBlocked=pendingToggle=false;
        yPressed=yReleased=0;
    }
    void clear() {
        allowed=false;active=false;component=0;axis=0;observed=0;heartbeat=0;
        plane=input=selection=view={};bindings=0;count=capacity=0;started=0;inputs=0;cancelGesture();notice.clear();
    }
} targetGaze;
using TargetGazeAxis=float(*)(void*,uint64_t);
TargetGazeAxis originalTargetGazeAxis{};
using TargetGazePad=DWORD(WINAPI*)(DWORD,XINPUT_STATE*);
TargetGazePad originalTargetGazePad{};
using TargetGazeSelect=Addr(*)(void*,bool);
TargetGazeSelect originalTargetGazeSelect{};
Addr targetGazeImageBegin{},targetGazeImageEnd{};

bool targetGazeFocused() {
    DWORD process{};const auto window=GetForegroundWindow();
    return window && GetWindowThreadProcessId(window,&process) && process==GetCurrentProcessId();
}
bool targetGazeLive() {
    return targetGaze.allowed.load() && GetCurrentThreadId()==targetGaze.thread.load() &&
        GetTickCount64()-targetGaze.heartbeat.load()<=500 && targetGazeFocused();
}
bool targetGazeIdentities() {
    const ContinuousReadBatch memory;
    return memory.alive(targetGaze.plane)&&memory.alive(targetGaze.input)&&memory.alive(targetGaze.selection)&&memory.alive(targetGaze.view)&&
        memory.read<Addr>(targetGaze.plane.address+0x118)==targetGaze.input.address&&
        memory.read<Addr>(targetGaze.plane.address+0x1310)==targetGaze.selection.address&&
        memory.read<Addr>(targetGaze.plane.address+0x1250)==targetGaze.view.address&&
        memory.read<Addr>(targetGaze.input.address+0x20)==targetGaze.plane.address&&
        memory.read<Addr>(targetGaze.selection.address+0x20)==targetGaze.plane.address&&
        memory.read<Addr>(targetGaze.view.address+0x20)==targetGaze.plane.address;
}
bool targetGazeOrdinary() {return read<unsigned char>(targetGaze.view.address+0x350)<=2&&!cameraHoldNativeSpecial();}
bool targetGazeHasTarget() {
    const auto selected=read<Addr>(targetGaze.selection.address+0x248);
    if(selected)try {continuousIdentity(selected);return true;}catch(...) {} // identity已核对地址、序列号与销毁标记。
    return false;
}
void targetGazeToggle() {
    require(targetGazeIdentities(),"锁定注视的玩家身份已变化");
    targetGaze.pendingToggle=true;
    if(targetGazeHasTarget()) {targetGaze.active=!targetGaze.active.load();targetGaze.started=GetTickCount64();targetGaze.notice.clear();}
    else {targetGaze.active=false;targetGaze.notice="no_target";}
}
void targetGazeFailure(const char* reason) {targetGaze.clear();targetGaze.error=reason;}
void targetGazeObservePad(DWORD result,const XINPUT_STATE* state) {
    if(GetCurrentThreadId()!=targetGaze.thread.load())return;
    if(!targetGazeLive()) {targetGaze.active=false;targetGaze.cancelGesture();return;}
    if(result!=ERROR_SUCCESS||!state) {
        if(targetGaze.padConnected){targetGaze.active=false;targetGaze.pendingToggle=false;}
        if(targetGaze.yDown||targetGaze.yConsumed)targetGaze.keyboardBlocked=true;
        targetGaze.padConnected=targetGaze.padSeeded=targetGaze.yCandidate=targetGaze.yDown=false;
        targetGaze.yConsumed=false;targetGaze.yReleased=0;return;
    }
    targetGaze.padConnected=true;
    const bool y=(state->Gamepad.wButtons&XINPUT_GAMEPAD_Y)!=0;
    const auto now=GetTickCount64();
    if(!targetGaze.padSeeded) {
        targetGaze.padSeeded=true;targetGaze.yDown=y;targetGaze.yCandidate=false;targetGaze.yConsumed=y;
        targetGaze.keyboardBlocked=y;return; // 恢复后已按住的Y不能补发。
    }
    if(y&&!targetGaze.yDown) {
        targetGaze.yPressed=now;targetGaze.yReleased=0;targetGaze.yCandidate=true;targetGaze.yConsumed=false;
    }
    if(y)targetGaze.keyboardBlocked=true;
    if(targetGaze.yCandidate&&(y||targetGaze.yDown)&&now-targetGaze.yPressed>=200) {
        targetGaze.yCandidate=false;
        if(targetGaze.inputs&1){targetGaze.yConsumed=true;targetGazeToggle();}
    }
    if(!y&&targetGaze.yDown) {targetGaze.yReleased=now;targetGaze.yCandidate=false;}
    targetGaze.yDown=y;
}
float targetGazeTrackValue(float native) {
    const bool down=std::isfinite(native)&&native>.5f;
    if(!targetGaze.keyboardSeeded) {targetGaze.keyboardSeeded=true;targetGaze.keyboardDown=down;}
    else {
        if((targetGaze.inputs&2)&&!targetGaze.yDown&&!targetGaze.keyboardBlocked&&down&&!targetGaze.keyboardDown)targetGazeToggle();
        targetGaze.keyboardDown=down;
    }
    // 游戏自行解析实际键盘绑定；Y与键盘共用原生轴时，先等Y释放且原轴归零再接收键盘新边沿。
    if(!targetGaze.yDown&&!down)targetGaze.keyboardBlocked=false;
    const bool nativePad=!(targetGaze.inputs&1)&&targetGaze.yDown&&down;
    const bool nativeKeyboard=!(targetGaze.inputs&2)&&!targetGaze.yDown&&!targetGaze.keyboardBlocked&&down;
    return targetGaze.active.load()||nativePad||nativeKeyboard?1.f:0.f;
}
float targetGazeAxis(void* object,uint64_t name) {
    const float native=originalTargetGazeAxis(object,name);
    const float look=cameraHoldAxis(object,name,native);
    if(reinterpret_cast<Addr>(object)!=targetGaze.component.load()||name!=targetGaze.axis.load()||
        GetCurrentThreadId()!=targetGaze.thread.load())return look;
    if(!targetGazeLive()){targetGaze.active=false;targetGaze.cancelGesture();return native;}
    try {
        require(targetGazeIdentities(),"锁定注视的玩家身份已变化");
        if(!targetGazeOrdinary()){targetGaze.active=false;targetGaze.cancelGesture();return native;}
        XINPUT_STATE pad{};const auto result=originalTargetGazePad(0,&pad);targetGazeObservePad(result,&pad);
        targetGaze.observed=GetTickCount64();return targetGazeTrackValue(native);
    }catch(const std::exception& error){targetGazeFailure(error.what());return native;}
}
// 原生在Y释放时按0.5秒判断短按。已消费的0.2秒手势须避免再次切目标，不改释放入口或任何目标指针。
Addr targetGazeSelect(void* object,bool force) {
    if(targetGazeLive()&&(targetGaze.inputs&1)&&!force&&reinterpret_cast<Addr>(object)==targetGaze.selection.address)try {
        require(targetGazeIdentities(),"锁定注视的玩家身份已变化");
        if(targetGazeOrdinary()) {
            XINPUT_STATE pad{};const auto result=originalTargetGazePad(0,&pad);targetGazeObservePad(result,&pad);
            if(targetGaze.yConsumed&&(targetGaze.yDown||(targetGaze.yReleased&&GetTickCount64()-targetGaze.yReleased<=150)))
                return read<Addr>(targetGaze.selection.address+0x248);
        }
    }catch(const std::exception& error){targetGazeFailure(error.what());}
    return originalTargetGazeSelect(object,force);
}
DWORD WINAPI targetGazePad(DWORD index,XINPUT_STATE* state) {
    const auto result=originalTargetGazePad(index,state);
    if(index!=0)return result;
    const auto caller=reinterpret_cast<Addr>(__builtin_return_address(0));
    if(caller>=targetGazeImageBegin&&caller<targetGazeImageEnd) {
        try {
            if(targetGazeLive()&&targetGazeIdentities()&&targetGazeOrdinary())targetGazeObservePad(result,state);
            else if(GetCurrentThreadId()==targetGaze.thread.load()){targetGaze.active=false;targetGaze.cancelGesture();}
        }catch(const std::exception& error){targetGazeFailure(error.what());}
        try {cameraHoldObservePad(result,state);}catch(const std::exception& error){cameraHoldFailure(error.what());}
    }
    return result; // 不过滤Y/L3/R3，原生干扰弹和UI仍得到完整按键。
}
void installTargetGaze() {
    if(targetGaze.ready)return;
    verifyVersion();const auto base=continuousBase();
    constexpr unsigned char special[]={0x40,0x53,0x48,0x83,0xec,0x20,0x48,0x8b,0x05,0x5b,0xd1,0x8e,0x03};
    checkMemory(base+0x3ceac0,sizeof(special));
    require(std::memcmp(reinterpret_cast<void*>(base+0x3ceac0),special,sizeof(special))==0,"镜头保持特殊视角判据不匹配");
    constexpr unsigned char axis[]={0x4c,0x8b,0xc1,0x0f,0x57,0xc0,0x85,0xd2,0x75,0x0b,0x48,0x8b,0xc2,0x48,0xc1,0xe8,0x20,0x85};
    constexpr unsigned char selected[]={0x48,0x8b,0x81,0x10,0x13,0,0,0x48,0x85,0xc0,0x74,0x08,0x48,0x8b,0x80,0x48,0x02,0,0};
    constexpr unsigned char select[]={0x40,0x57,0x48,0x83,0xec,0x30,0xf3,0x0f,0x10,0x81,0xe0,0x01,0,0,0x48,0x8b,0xf9};
    require(std::memcmp(reinterpret_cast<void*>(base+0x16c03b0),axis,sizeof(axis))==0&&
        std::memcmp(reinterpret_cast<void*>(base+0x81c100),selected,sizeof(selected))==0&&
        std::memcmp(reinterpret_cast<void*>(base+0x4c90b0),select,sizeof(select))==0,"目标注视接口与当前游戏不匹配");
    // 下层选择入口完整函数体来自此前只读采集；不接管鼠标飞控已改写的释放入口。
    checkMemory(base+0x4c90b0,629);uint64_t fingerprint=14695981039346656037ull;
    for(size_t i=0;i<629;++i)fingerprint=(fingerprint^read<unsigned char>(base+0x4c90b0+i))*1099511628211ull;
    require(fingerprint==0xaecb3aa29797d7a0ull,"锁定注视的选择入口不匹配");
    const auto module=GetModuleHandleW(L"xinput1_3.dll");
    const auto entry=module?GetProcAddress(module,"XInputGetState"):nullptr;
    require(entry!=nullptr,"目标注视缺少游戏使用的XInput接口");
    const auto status=MH_Initialize();require(status==MH_OK||status==MH_ERROR_ALREADY_INITIALIZED,"目标注视输入钩子初始化失败");
    void* entries[]={reinterpret_cast<void*>(base+0x16c03b0),reinterpret_cast<void*>(entry),reinterpret_cast<void*>(base+0x4c90b0)};
    void* hooks[]={reinterpret_cast<void*>(&targetGazeAxis),reinterpret_cast<void*>(&targetGazePad),reinterpret_cast<void*>(&targetGazeSelect)};
    void** originals[]={reinterpret_cast<void**>(&originalTargetGazeAxis),reinterpret_cast<void**>(&originalTargetGazePad),reinterpret_cast<void**>(&originalTargetGazeSelect)};
    const auto dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    const auto pe=reinterpret_cast<const IMAGE_NT_HEADERS64*>(base+dos->e_lfanew);
    targetGazeImageBegin=base;targetGazeImageEnd=base+pe->OptionalHeader.SizeOfImage;
    int created=0;
    try {
        for(int i=0;i<3;++i){require(MH_CreateHook(entries[i],hooks[i],originals[i])==MH_OK,"目标注视输入接入失败");++created;}
        for(const auto address:entries)require(MH_QueueEnableHook(address)==MH_OK,"目标注视输入钩子启用失败");
        require(MH_ApplyQueued()==MH_OK,"目标注视输入钩子启用失败");targetGaze.ready=true;
    }catch(...) {for(int i=0;i<created;++i){MH_DisableHook(entries[i]);MH_RemoveHook(entries[i]);}throw;}
    nativeLog("TARGET_GAZE_HOOKS_READY");
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
    targetGaze.selection=continuousIdentity(read<Addr>(plane+0x1310));
    targetGaze.view=continuousIdentity(read<Addr>(plane+0x1250));
    require(mountClassIs(targetGaze.selection.address,"TargetSelectionComponent","/Script/Nimbus") &&
        read<Addr>(targetGaze.selection.address+0x20)==plane &&
        mountClassIs(targetGaze.view.address,"CameraViewComponent","/Script/Nimbus") &&
        read<Addr>(targetGaze.view.address+0x20)==plane,"锁定注视组件不属于当前飞机");
    targetGaze.bindings=data;targetGaze.count=count;targetGaze.capacity=capacity;
    targetGaze.axis=axis;targetGaze.component=input.address;targetGaze.thread=GetCurrentThreadId();
}


// 第三参数为两种输入的独立许可位；第四参数仅用于立即结束注视。
int targetGazeLua(void* luaState) {
    const dualsp::LuaApi* api=nullptr;
    try {
        static const dualsp::LuaApi bound;api=&bound;
        require(api->top(luaState)==4,"目标注视参数数量无效");
        const auto plane=static_cast<Addr>(api->readInteger(luaState,1));
        const auto allowed=api->readInteger(luaState,2),inputs=api->readInteger(luaState,3),stop=api->readInteger(luaState,4);
        require((allowed==0||allowed==1)&&inputs>=0&&inputs<=3&&(stop==0||stop==1),"目标注视设置无效");
        std::string reason;bool toggled=false;
        if(!allowed||!inputs||!plane||!targetGazeFocused()){targetGaze.clear();targetGaze.error.clear();}
        else {
            require(targetGaze.error.empty(),targetGaze.error.c_str());installTargetGaze();continuousIdentity(plane);
            const auto input=read<Addr>(plane+0x118);
            if(!input||!read<int>(input+0x118)||!read<Addr>(plane+0x1310)||!read<Addr>(plane+0x1250)) {
                targetGaze.clear();reason="waiting_input";
            }else {
                const bool same=targetGaze.plane.address==plane&&targetGazeIdentities()&&
                    read<Addr>(input+0x110)==targetGaze.bindings&&read<int>(input+0x118)==targetGaze.count&&read<int>(input+0x11c)==targetGaze.capacity;
                if(!same)bindTargetGaze(plane);
                if(targetGaze.inputs!=inputs){targetGaze.active=false;targetGaze.cancelGesture();targetGaze.inputs=static_cast<int>(inputs);}
                if(!targetGazeOrdinary()){targetGaze.active=false;targetGaze.cancelGesture();targetGaze.allowed=false;reason="special_view";}
                else {
                    const auto now=GetTickCount64();
                    if(targetGaze.heartbeat.load()&&now-targetGaze.heartbeat.load()>500){targetGaze.active=false;targetGaze.cancelGesture();}
                    targetGaze.heartbeat=now;targetGaze.allowed=true;
                    XINPUT_STATE pad{};const auto result=originalTargetGazePad(0,&pad);targetGazeObservePad(result,&pad);
                    targetGazeTrackValue(originalTargetGazeAxis(reinterpret_cast<void*>(input),targetGaze.axis.load()));
                    if(stop)targetGaze.active=false;
                    if(!targetGazeHasTarget())targetGaze.active=false;
                    toggled=targetGaze.pendingToggle;targetGaze.pendingToggle=false;reason=targetGaze.notice;targetGaze.notice.clear();
                    const auto checkedAt=GetTickCount64();
                    if(targetGaze.active.load()&&checkedAt-targetGaze.started>2000) {
                        const auto observed=targetGaze.observed.load();
                        require(observed&&checkedAt-observed<2000,"未观察到原生目标跟踪轴读取，已停止注视");
                    }
                }
            }
        }
        api->boolean(luaState,true);api->boolean(luaState,targetGaze.active.load());api->string(luaState,reason.data(),reason.size());api->boolean(luaState,toggled);return 4;
    }catch(const std::exception& error) {
        const std::string message=error.what();targetGazeFailure(message.c_str());nativeLog(std::string("TARGET_GAZE_ERROR ")+message);
        if(api){api->boolean(luaState,false);api->boolean(luaState,false);api->string(luaState,message.data(),message.size());api->boolean(luaState,false);return 4;}
        return 0;
    }
}
