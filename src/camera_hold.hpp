#pragma once
// 当前玩家的相对环视保持及手柄移动响应；不冻结相机、改坐标或吞掉干扰弹输入。
// 本文件在target_gaze.hpp之后包含，共享GetAxisValue与XInput入口。
struct CameraHold {
    ContinuousIdentity plane,input,view,focus;
    std::array<ContinuousIdentity,3> cameras; // 原生槽0=第一人称、1=座舱、2=第三人称。
    Addr bindings{};
    int count{},capacity{},viewType{-1};
    uint64_t right{},up{},track{};
    std::atomic<bool> allowed{false};
    std::atomic<DWORD> thread{0};
    std::atomic<ULONGLONG> heartbeat{0};
    bool ready{},seeded{},rightDown{},nativePress{},longDone{},combo{},pendingView{},centering{},waitNeutral{},armed{};
    bool r3Gesture{},releaseHandled{}; // 整次按压及释放交接都接管，不能只吞第一个原生回调。
    bool centerKeySeeded{},centerKeyDown{};
    bool replaceR3{},padConnected{};
    int centerKey{},sensitivity{100};
    unsigned nativeRequests{};
    bool frozenX{},frozenY{};
    bool poseValid{},viewTransfer{};
    bool stickWas{},axisWas{};
    float heldX{},heldY{},poseX{},poseY{};
    SHORT padX{},padY{};
    ULONGLONG pressedAt{},releasedAt{},buttonReleasedAt{};
    std::string error;
    void resetPose() {armed=false;frozenX=frozenY=false;poseValid=viewTransfer=false;centering=false;waitNeutral=false;stickWas=axisWas=false;releasedAt=0;}
    void cancelGesture() {
        seeded=false;rightDown=nativePress=longDone=combo=pendingView=r3Gesture=releaseHandled=false;
        pressedAt=buttonReleasedAt=0;nativeRequests=0;
    }
    void clear() {
        allowed=false;heartbeat=0;plane={};input={};view={};focus={};cameras={};bindings=0;count=capacity=0;
        right=up=track=0;viewType=-1;cancelGesture();
        centerKeySeeded=centerKeyDown=false;centerKey=0;sensitivity=100;
        replaceR3=padConnected=false;
        padX=padY=0;resetPose();
    }
} cameraHold;
using CameraHoldUpdate=void(*)(void*,float,void*,void*);
using CameraHoldView=void(*)(void*);
std::array<CameraHoldUpdate,3> originalCameraHoldUpdates{};
CameraHoldView originalCameraHoldView{};
CameraHoldUpdate originalCameraHoldSpecial{};
bool cameraHoldNativeSpecial() {return reinterpret_cast<bool(*)()>(continuousBase()+0x3ceac0)();}
struct CameraHoldFrame {bool enabled{};Addr component{};float x{},y{},nativeDelta{};bool delegated{};};
thread_local CameraHoldFrame cameraHoldFrame;

bool cameraHoldLive() {
    return cameraHold.allowed.load() && GetCurrentThreadId()==cameraHold.thread.load() &&
        GetTickCount64()-cameraHold.heartbeat.load()<=500 && targetGazeFocused();
}
void cameraHoldFailure(const char* reason) {
    const std::string message=reason;
    cameraHold.clear();cameraHold.error=message;
    try {nativeLog(std::string("CAMERA_HOLD_ERROR ")+message);}catch(...){}
}
bool cameraHoldIdentities() {
    return continuousAlive(cameraHold.plane) && continuousAlive(cameraHold.input) && continuousAlive(cameraHold.view) && continuousAlive(cameraHold.focus) &&
        read<Addr>(cameraHold.plane.address+0x118)==cameraHold.input.address &&
        read<Addr>(cameraHold.plane.address+0x1250)==cameraHold.view.address &&
        read<Addr>(cameraHold.input.address+0x20)==cameraHold.plane.address && read<Addr>(cameraHold.view.address+0x20)==cameraHold.plane.address &&
        read<Addr>(cameraHold.plane.address+0x1370)==cameraHold.focus.address && read<Addr>(cameraHold.focus.address+0x20)==cameraHold.plane.address;
}
bool cameraHoldCameraIdentities() {
    for(int i=0;i<3;++i) {
        const auto& camera=cameraHold.cameras[i];
        if(!continuousAlive(camera) || read<Addr>(cameraHold.view.address+0x318+i*0x10)!=camera.address ||
            read<Addr>(camera.address+0x20)!=cameraHold.plane.address || read<Addr>(camera.address+0x330)!=cameraHold.plane.address ||
            read<Addr>(camera.address+0x338)!=cameraHold.focus.address)return false;
    }
    return true;
}
// 普通视角之间传递最近一帧的实际环视姿态；许可由调用方的游戏线程/前台/身份门槛保证。
void cameraHoldChangeView(int view) {
    if(cameraHold.viewType==view)return;
    const bool transfer=cameraHold.poseValid && cameraHold.armed && !cameraHold.centering &&
        cameraHold.viewType>=0 && cameraHold.viewType<=2 && view>=0 && view<=2;
    const float x=cameraHold.poseX,y=cameraHold.poseY;
    const bool centering=cameraHold.centering,waitNeutral=cameraHold.waitNeutral;
    cameraHold.resetPose();cameraHold.viewType=view;
    if(transfer) {
        const float scale=view==1?1.f:.95f;
        const bool reversed=read<unsigned char>(cameraHold.plane.address+0xe17)!=0;
        cameraHold.armed=true;cameraHold.frozenX=cameraHold.frozenY=true;
        cameraHold.poseValid=cameraHold.viewTransfer=true;cameraHold.poseX=x;cameraHold.poseY=y;
        cameraHold.heldX=x*scale;cameraHold.heldY=y*scale*(reversed?-1.f:1.f);
    }else if(centering) {cameraHold.centering=true;cameraHold.waitNeutral=waitNeutral;}
    if(cameraHold.r3Gesture) {
        // 已切换视角不能再执行本次手势，但不能提前放行剩余原生回调。
        cameraHold.longDone=true;cameraHold.pendingView=false;cameraHold.releaseHandled=true;
    }else cameraHold.cancelGesture();
}
void cameraHoldCenter() {
    cameraHold.resetPose();cameraHold.centering=true;cameraHold.waitNeutral=true;
    // 明确回正会结束模组保持的目标注视；真实按住Y/F仍按原生轴优先。
    targetGaze.active=false;
}
void cameraHoldResolveRelease() {
    if(!cameraHold.r3Gesture || cameraHold.rightDown || !cameraHold.buttonReleasedAt || cameraHold.releaseHandled)return;
    cameraHold.releaseHandled=true;
    if(cameraHold.longDone || cameraHold.combo)return;
    if(cameraHold.buttonReleasedAt-cameraHold.pressedAt<300)cameraHold.pendingView=true;
    else {cameraHold.longDone=true;cameraHoldCenter();} // 跨过阈值才松开时补回中，不能兼触短按。
}
void cameraHoldObserveCenterKey(int key) {
    if(cameraHold.centerKey!=key){cameraHold.centerKey=key;cameraHold.centerKeySeeded=false;}
    const bool down=key && (GetAsyncKeyState(key)&0x8000)!=0;
    const bool modifiers=(GetAsyncKeyState(VK_CONTROL)&0x8000) || (GetAsyncKeyState(VK_MENU)&0x8000) || (GetAsyncKeyState(VK_SHIFT)&0x8000);
    if(cameraHold.centerKeySeeded && down && !cameraHold.centerKeyDown && !modifiers)cameraHoldCenter();
    cameraHold.centerKeyDown=down;cameraHold.centerKeySeeded=true; // 恢复时先播种，不能把已按住的键当新按下。
}
void cameraHoldObservePad(DWORD result,const XINPUT_STATE* state) {
    if(!cameraHoldLive())return;
    if(result!=ERROR_SUCCESS || !state) {
        if(cameraHold.padConnected)cameraHold.resetPose(); // 断连仅清一次，不能反复取消独立键鼠回正。
        cameraHold.padConnected=false;
        cameraHold.cancelGesture();
        cameraHold.padX=cameraHold.padY=0;return;
    }
    const WORD buttons=state->Gamepad.wButtons;
    const bool right=(buttons&XINPUT_GAMEPAD_RIGHT_THUMB)!=0;
    const bool both=right && (buttons&XINPUT_GAMEPAD_LEFT_THUMB)!=0;
    cameraHold.padX=state->Gamepad.sThumbRX;cameraHold.padY=state->Gamepad.sThumbRY;
    cameraHold.padConnected=true;
    if(!cameraHold.replaceR3){cameraHold.cancelGesture();return;} // 保持仍可用，仅归还R3原生动作。
    const auto now=GetTickCount64();
    if(!cameraHold.seeded) {
        cameraHold.seeded=true;cameraHold.rightDown=right;
        cameraHold.nativePress=false;cameraHold.longDone=right; // 已按住的旧边沿不补发。
        cameraHold.r3Gesture=right;cameraHold.releaseHandled=right;cameraHold.combo=right || (buttons&XINPUT_GAMEPAD_LEFT_THUMB);
        return;
    }
    if(right && !cameraHold.rightDown) {
        cameraHold.pressedAt=now;cameraHold.nativePress=false;cameraHold.longDone=false;cameraHold.pendingView=false;
        cameraHold.r3Gesture=true;cameraHold.releaseHandled=false;cameraHold.buttonReleasedAt=0;cameraHold.nativeRequests=0;
    }
    if(both && !cameraHold.combo) {
        cameraHold.combo=true;cameraHold.pendingView=false;cameraHold.releaseHandled=true; // 双摇杆只交给原生干扰弹，不切视角、不回中。
    }
    if(!right && cameraHold.rightDown)cameraHold.buttonReleasedAt=now;
    cameraHold.rightDown=right;
    cameraHoldResolveRelease(); // 释放后的重复回调仍属于同次按压。
    if(!right && !(buttons&XINPUT_GAMEPAD_LEFT_THUMB))cameraHold.combo=false; // 组合锁定至两边都松开。
}
// 整次R3按压的视角请求隔离；短按松开只派发一次原生换视角，长按满300毫秒只回中。
// 原生动作没有FKey，按压期间及短暂释放交接中的混合换视角输入也归本次R3处理。
void cameraHoldViewPressed(void* object) {
    if(!cameraHoldLive() || !cameraHold.replaceR3 || reinterpret_cast<Addr>(object)!=cameraHold.plane.address) {
        originalCameraHoldView(object);return;
    }
    try {
        if(!cameraHoldIdentities() || cameraHoldNativeSpecial()){cameraHold.clear();originalCameraHoldView(object);return;}
        // 已有按压只用缓存边沿；无记录时补采，兼容原生动作先于游戏线程输入观察到达。
        if(!cameraHold.r3Gesture) {
            XINPUT_STATE pad{};const auto result=originalTargetGazePad(0,&pad);
            cameraHoldObservePad(result,&pad);
        }
        if(cameraHold.r3Gesture) {
            cameraHold.nativePress=true;++cameraHold.nativeRequests;
            if(cameraHold.nativeRequests<=2)nativeLog("CAMERA_HOLD_R3_VIEW_BLOCK callbacks="+std::to_string(cameraHold.nativeRequests)+
                " released="+std::to_string(!cameraHold.rightDown));
            cameraHoldResolveRelease();return;
        }
        // 原生换视角保留姿态缓存，目标视角的首次输入更新负责接续。
    }catch(const std::exception& error){cameraHoldFailure(error.what());}
    originalCameraHoldView(object);
}
float cameraHoldAxis(void* component,uint64_t name,float native) {
    if(!cameraHoldFrame.enabled || !cameraHoldLive() || reinterpret_cast<Addr>(component)!=cameraHoldFrame.component)return native;
    if(name==cameraHold.right)return cameraHoldFrame.x;
    if(name==cameraHold.up)return cameraHoldFrame.y;
    return native;
}
void cameraHoldUpdate(int index,void* component,float delta,void* actor,void* camera) {
    const auto previous=cameraHoldFrame;
    cameraHoldFrame={};
    float cameraDelta=delta;
    try {
        if(cameraHoldLive() && (index==cameraHold.viewType || (continuousAlive(cameraHold.view) && read<unsigned char>(cameraHold.view.address+0x350)==index)) &&
            reinterpret_cast<Addr>(component)==cameraHold.cameras[index].address &&
            reinterpret_cast<Addr>(actor)==cameraHold.plane.address && cameraHoldIdentities() &&
            continuousAlive(cameraHold.cameras[index])) {
            const int view=read<unsigned char>(cameraHold.view.address+0x350);
            if(view==index && read<unsigned char>(cameraHold.plane.address+0xe12) && !cameraHoldNativeSpecial()) {
                cameraHoldChangeView(view); // 在目标视角的首帧接续，不等待20毫秒Lua调度。
                const float rawX=originalTargetGazeAxis(reinterpret_cast<void*>(cameraHold.input.address),cameraHold.right);
                const float rawY=originalTargetGazeAxis(reinterpret_cast<void*>(cameraHold.input.address),cameraHold.up);
                // 原生实际注视状态2/3及模组持续注视都使用原生输出插值，不应用环视灵敏度。
                // 此判据与0x420120一致，接入时校验其完整16字节；不另设长按等待时间。
                const unsigned focusState=read<unsigned>(cameraHold.focus.address+0x108);
                const bool targetTracking=(focusState-2u<=1u) || (targetGazeLive() && targetGaze.active.load());
                const bool stick=std::abs(static_cast<int>(cameraHold.padX))>8192 || std::abs(static_cast<int>(cameraHold.padY))>8192;
                if(!std::isfinite(rawX) || !std::isfinite(rawY))throw std::runtime_error("镜头保持收到无效环视轴");
                const bool axes=std::abs(rawX)>.001f || std::abs(rawY)>.001f;
                const auto now=GetTickCount64();
                if((!stick && cameraHold.stickWas) || (!axes && cameraHold.axisWas))cameraHold.releasedAt=now;
                cameraHold.stickWas=stick;cameraHold.axisWas=axes;
                // 第一/第三人称状态是[-1,1]，原生输入先夹到0.95再归一化；普通座舱没有这一步。
                const Addr state=reinterpret_cast<Addr>(component)+(index==1?0x38c:0x3d4);
                const float x=read<float>(state),y=read<float>(state+4);
                require(std::isfinite(x) && std::isfinite(y) && std::abs(x)<=1.01f && std::abs(y)<=1.01f,"镜头保持状态超出原生范围");
                {
                    // 原生输出将环视姿态与目标方向插值。保留环视基底，避免先归零再转向目标。
                    // 实际注视仍由原生输出优先，不覆盖跟踪轴、目标方向或原生混合权重。
                    if(!stick && !axes)cameraHold.waitNeutral=false;
                    if((stick || axes) && !cameraHold.waitNeutral) {cameraHold.centering=false;cameraHold.armed=true;}
                    // 摇杆回中或键鼠轴归零有短暂交接，先保持实际平滑姿态，不保存回弹尾值。
                    const bool releaseTail=!stick && cameraHold.armed && cameraHold.releasedAt && now-cameraHold.releasedAt<=50;
                    // 键鼠也从原生环视轴建立保持；模式键松开、轴归零后保存实际平滑姿态，不写死Alt。
                    float outX=rawX,outY=rawY;
                    if(cameraHold.centering) {
                        outX=outY=0;
                        if(std::abs(x)<.0001f && std::abs(y)<.0001f && !cameraHold.waitNeutral)cameraHold.centering=false;
                    }else if(cameraHold.viewTransfer) {
                        outX=cameraHold.heldX;outY=cameraHold.heldY;
                        cameraDelta=1.f/7.f; // 仅首帧让原生delta*7到达保持方向，不从零重新渐进。
                    }else if(cameraHold.armed) {
                        const float scale=index==1?1.f:.95f;
                        const bool reversed=read<unsigned char>(cameraHold.plane.address+0xe17)!=0;
                        if(std::abs(rawX)<=.001f || releaseTail) {
                            if(!cameraHold.frozenX){cameraHold.heldX=x*scale;cameraHold.frozenX=true;}
                            outX=cameraHold.heldX;
                        }else cameraHold.frozenX=false;
                        if(std::abs(rawY)<=.001f || releaseTail) {
                            if(!cameraHold.frozenY){cameraHold.heldY=y*scale*(reversed?-1.f:1.f);cameraHold.frozenY=true;}
                            outY=cameraHold.heldY;
                        }else cameraHold.frozenY=false;
                    }
                    cameraHoldFrame={true,cameraHold.input.address,outX,outY,delta};
                    // 原生普通环视按delta*7平滑；只调手柄移动响应，不放大角度、保持值、回正或目标注视。
                    if(stick && cameraHold.armed && !cameraHold.centering && !cameraHold.viewTransfer && !targetTracking && cameraHold.sensitivity!=100 && std::isfinite(delta) && delta>0)
                        cameraDelta=std::min(delta*(cameraHold.sensitivity/100.f),1.f/7.f);
                }
            }
        }
    }catch(const std::exception& error){cameraHoldFailure(error.what());cameraHoldFrame={};cameraDelta=delta;}
    // 不在原生调用外复制/写回相机姿态，原生边界、动画和插值继续执行。
    originalCameraHoldUpdates[index](component,cameraDelta,actor,camera);
    try {
        if(cameraHoldFrame.enabled) {
            if(cameraHoldFrame.delegated)cameraHold.resetPose(); // 特殊座舱不使用普通归一化姿态。
            else {
                cameraHold.viewTransfer=false;
                if(cameraHold.armed && !cameraHold.centering && cameraHoldLive() && cameraHoldIdentities() &&
                    continuousAlive(cameraHold.cameras[index]) && read<unsigned char>(cameraHold.view.address+0x350)==index) {
                    const Addr state=reinterpret_cast<Addr>(component)+(index==1?0x38c:0x3d4);
                    const float x=read<float>(state),y=read<float>(state+4);
                    require(std::isfinite(x) && std::isfinite(y) && std::abs(x)<=1.01f && std::abs(y)<=1.01f,"镜头保持状态超出原生范围");
                    cameraHold.poseX=std::clamp(x,-1.f,1.f);cameraHold.poseY=std::clamp(y,-1.f,1.f);cameraHold.poseValid=true;
                }
            }
        }
    }catch(const std::exception& error){cameraHoldFailure(error.what());}
    cameraHoldFrame=previous;
}
void cameraHoldFirst(void* c,float d,void* a,void* v){cameraHoldUpdate(0,c,d,a,v);}
void cameraHoldCockpit(void* c,float d,void* a,void* v){cameraHoldUpdate(1,c,d,a,v);}
void cameraHoldThird(void* c,float d,void* a,void* v){cameraHoldUpdate(2,c,d,a,v);}
void cameraHoldSpecial(void* c,float d,void* a,void* v) {
    const auto previous=cameraHoldFrame;cameraHoldFrame={};
    originalCameraHoldSpecial(c,previous.enabled?previous.nativeDelta:d,a,v);cameraHoldFrame=previous;
    cameraHoldFrame.delegated=true; // 特殊/委托分支归还原delta，不缓存普通环视姿态。
}

void installCameraHold() {
    if(cameraHold.ready)return;
    installTargetGaze();
    constexpr std::array<Addr,5> rvas{0x41e150,0x424cf0,0x43df90,0x821430,0x421320};
    constexpr unsigned char signatures[5][16]={
        {0x48,0x89,0x5c,0x24,0x08,0x57,0x48,0x83,0xec,0x60,0x48,0x8b,0x81,0x30,0x03,0x00},
        {0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x6c,0x24,0x10,0x48,0x89,0x74,0x24,0x18,0x57},
        {0x48,0x89,0x5c,0x24,0x08,0x57,0x48,0x83,0xec,0x50,0x0f,0x29,0x74,0x24,0x40,0x48},
        {0x40,0x53,0x48,0x83,0xec,0x20,0x48,0x83,0xb9,0x50,0x12,0x00,0x00,0x00,0x48,0x8b},
        {0x4c,0x8b,0xdc,0x49,0x89,0x6b,0x10,0x49,0x89,0x73,0x18,0x57,0x48,0x81,0xec,0x90}};
    const auto base=continuousBase();
    constexpr unsigned char special[]={0x40,0x53,0x48,0x83,0xec,0x20,0x48,0x8b,0x05,0x5b,0xd1,0x8e,0x03};
    checkMemory(base+0x3ceac0,sizeof(special));
    require(std::memcmp(reinterpret_cast<void*>(base+0x3ceac0),special,sizeof(special))==0,"镜头保持特殊视角判据不匹配");
    constexpr unsigned char focused[]={0x8b,0x81,0x08,0x01,0x00,0x00,0x83,0xe8,0x02,0x83,0xf8,0x01,0x0f,0x96,0xc0,0xc3};
    checkMemory(base+0x420120,sizeof(focused));
    require(std::memcmp(reinterpret_cast<void*>(base+0x420120),focused,sizeof(focused))==0,"镜头保持实际注视判据不匹配");
    for(size_t i=0;i<rvas.size();++i) {
        checkMemory(base+rvas[i],sizeof(signatures[i]));
        require(std::memcmp(reinterpret_cast<void*>(base+rvas[i]),signatures[i],sizeof(signatures[i]))==0,"镜头保持接口与当前游戏不匹配");
    }
    std::array<void*,5> hooks{reinterpret_cast<void*>(&cameraHoldFirst),reinterpret_cast<void*>(&cameraHoldCockpit),
        reinterpret_cast<void*>(&cameraHoldThird),reinterpret_cast<void*>(&cameraHoldViewPressed),reinterpret_cast<void*>(&cameraHoldSpecial)};
    size_t created=0;
    try {
        for(size_t i=0;i<rvas.size();++i) {
            void** original=i<3?reinterpret_cast<void**>(&originalCameraHoldUpdates[i]):i==3?reinterpret_cast<void**>(&originalCameraHoldView):reinterpret_cast<void**>(&originalCameraHoldSpecial);
            require(MH_CreateHook(reinterpret_cast<void*>(base+rvas[i]),hooks[i],original)==MH_OK,"镜头保持原生入口接入失败");++created;
        }
        for(auto rva:rvas)require(MH_QueueEnableHook(reinterpret_cast<void*>(base+rva))==MH_OK,"镜头保持钩子排队失败");
        require(MH_ApplyQueued()==MH_OK,"镜头保持钩子启用失败");
    }catch(...) {
        for(size_t i=0;i<created;++i){MH_DisableHook(reinterpret_cast<void*>(base+rvas[i]));MH_RemoveHook(reinterpret_cast<void*>(base+rvas[i]));}
        throw;
    }
    cameraHold.ready=true;nativeLog("CAMERA_HOLD_HOOKS_READY");
}
void bindCameraHold(Addr plane) {
    cameraHold.clear();
    require(mountClassIs(plane,"PlayerPlane","/Script/Nimbus") && !(read<unsigned>(plane+8)&0x30u),"镜头保持需要当前玩家飞机");
    cameraHold.plane=continuousIdentity(plane);
    cameraHold.input=continuousIdentity(read<Addr>(plane+0x118));
    cameraHold.view=continuousIdentity(read<Addr>(plane+0x1250));
    cameraHold.focus=continuousIdentity(read<Addr>(plane+0x1370));
    require(mountClassIs(cameraHold.input.address,"InputComponent","/Script/Engine") &&
        mountClassIs(cameraHold.view.address,"CameraViewComponent","/Script/Nimbus"),"镜头保持组件类型不符");
    require(read<Addr>(cameraHold.input.address+0x20)==plane && read<Addr>(cameraHold.view.address+0x20)==plane,"镜头保持组件不属于当前飞机");
    require(mountClassIs(cameraHold.focus.address,"CameraFocusTargetComponent","/Script/Nimbus") &&
        read<Addr>(cameraHold.focus.address+0x20)==plane,"镜头保持注视组件身份不符");
    const char* classes[]={"FirstPersonCameraInputComponent","CockpitCameraInputComponent","ThirdPersonCameraInputComponent"};
    for(int i=0;i<3;++i) {
        cameraHold.cameras[i]=continuousIdentity(read<Addr>(cameraHold.view.address+0x318+i*0x10));
        const auto address=cameraHold.cameras[i].address;
        require(mountClassIs(address,classes[i],"/Script/Nimbus") && read<Addr>(address+0x20)==plane && read<Addr>(address+0x330)==plane &&
            read<Addr>(address+0x338)==cameraHold.focus.address,"镜头保持视角输入身份不符");
    }
    cameraHold.bindings=read<Addr>(cameraHold.input.address+0x110);
    cameraHold.count=read<int>(cameraHold.input.address+0x118);cameraHold.capacity=read<int>(cameraHold.input.address+0x11c);
    require(cameraHold.bindings && cameraHold.count>0 && cameraHold.count<=cameraHold.capacity && cameraHold.capacity<=128,"镜头保持输入轴列表无效");
    for(int i=0;i<cameraHold.count;++i) {
        const auto name=read<uint64_t>(cameraHold.bindings+i*0x70+8);
        const auto label=mountNameIndex(static_cast<unsigned>(name));
        uint64_t* slot=label=="INP_CameraLookRight"?&cameraHold.right:label=="INP_CameraLookUp"?&cameraHold.up:label=="INP_CameraTrackTarget"?&cameraHold.track:nullptr;
        if(slot){require(!*slot || *slot==name,"镜头保持轴名称不一致");*slot=name;}
    }
    require(cameraHold.right && cameraHold.up && cameraHold.track,"镜头保持缺少原生环视轴");
    cameraHold.thread=GetCurrentThreadId();
}
int cameraHoldLua(void* luaState) {
    const dualsp::LuaApi* api=nullptr;
    try {
        static const dualsp::LuaApi bound;api=&bound;
        require(api->top(luaState)==5,"镜头保持参数数量无效");
        const auto plane=static_cast<Addr>(api->readInteger(luaState,1));const auto allowed=api->readInteger(luaState,2);
        const auto centerKey=api->readInteger(luaState,3),sensitivity=api->readInteger(luaState,4);
        const auto replaceR3=api->readInteger(luaState,5);
        require(allowed==0 || allowed==1,"镜头保持设置无效");
        require(centerKey==0 || centerKey==VK_MBUTTON || dualsp::supportedShortcutKey(static_cast<int>(centerKey)),"镜头回正键位无效");
        require(centerKey>=0 && centerKey<=255 && sensitivity>=5 && sensitivity<=200 && (replaceR3==0 || replaceR3==1),"镜头保持参数超出范围");
        std::string reason;
        if(!allowed || !plane || !targetGazeFocused()){cameraHold.clear();cameraHold.error.clear();}
        else {
            require(cameraHold.error.empty(),cameraHold.error.c_str());
            installCameraHold();continuousIdentity(plane);
            const auto input=read<Addr>(plane+0x118);
            const auto viewObject=read<Addr>(plane+0x1250);
            bool pending=!input || !viewObject || !read<Addr>(plane+0x1370) || !read<int>(input+0x118);
            if(!pending) {
                continuousIdentity(viewObject);
                require(mountClassIs(viewObject,"CameraViewComponent","/Script/Nimbus"),"镜头保持组件类型不符");
                for(int i=0;i<3;++i) {
                    const auto camera=read<Addr>(viewObject+0x318+i*0x10);
                    if(!camera){pending=true;break;}
                    continuousIdentity(camera);
                    if(!read<Addr>(camera+0x330) || !read<Addr>(camera+0x338)){pending=true;break;}
                }
            }
            if(pending) {
                cameraHold.clear();reason="waiting_input";
            }else {
                const bool same=cameraHold.plane.address==plane && cameraHoldIdentities() && cameraHoldCameraIdentities() &&
                    read<Addr>(input+0x110)==cameraHold.bindings && read<int>(input+0x118)==cameraHold.count && read<int>(input+0x11c)==cameraHold.capacity;
                if(!same)bindCameraHold(plane);
                const int view=read<unsigned char>(cameraHold.view.address+0x350);
                if(view>2 || cameraHoldNativeSpecial()) {cameraHold.resetPose();cameraHold.cancelGesture();cameraHold.centerKeySeeded=false;reason="special_view";cameraHold.allowed=false;}
                else {
                    const auto now=GetTickCount64();
                    if(cameraHold.heartbeat.load() && now-cameraHold.heartbeat.load()>500) {
                        cameraHold.resetPose();cameraHold.cancelGesture();cameraHold.centerKeySeeded=false;
                    }
                    cameraHoldChangeView(view);
                    cameraHold.heartbeat=now;cameraHold.allowed=true;
                    cameraHold.sensitivity=static_cast<int>(sensitivity);
                    if(cameraHold.replaceR3!=(replaceR3==1))cameraHold.cancelGesture();
                    cameraHold.replaceR3=replaceR3==1;
                    XINPUT_STATE state{};const auto result=originalTargetGazePad(0,&state);cameraHoldObservePad(result,&state);
                    cameraHoldObserveCenterKey(static_cast<int>(centerKey));
                    // 300毫秒按实际时间计，回中在游戏线程立即执行一次，松开不再换视角。
                    if(cameraHold.replaceR3 && cameraHold.rightDown && !cameraHold.longDone && !cameraHold.combo && GetTickCount64()-cameraHold.pressedAt>=300) {
                        cameraHold.longDone=true;cameraHoldCenter();
                    }
                    if(cameraHold.replaceR3 && cameraHold.pendingView && !cameraHold.combo) {
                        cameraHold.pendingView=false;cameraHold.longDone=true;
                        nativeLog("CAMERA_HOLD_R3_SHORT_VIEW");
                        originalCameraHoldView(reinterpret_cast<void*>(plane));
                    }
                    if(cameraHold.r3Gesture && !cameraHold.rightDown && cameraHold.buttonReleasedAt && !cameraHold.pendingView) {
                        const auto sinceRelease=GetTickCount64()-cameraHold.buttonReleasedAt;
                        // 已确认动作保留100毫秒交接；尚未交付的原生动作最多等500毫秒，避免快点漏放或无限占键。
                        if(sinceRelease>=(cameraHold.nativePress?100:500)) {
                            cameraHold.r3Gesture=false;cameraHold.nativePress=false;
                        }
                    }
                }
            }
        }
        api->boolean(luaState,true);api->boolean(luaState,cameraHold.allowed.load());api->string(luaState,reason.data(),reason.size());return 3;
    }catch(const std::exception& error) {
        const std::string reason=error.what();cameraHoldFailure(reason.c_str());
        if(api){api->boolean(luaState,false);api->boolean(luaState,false);api->string(luaState,reason.data(),reason.size());return 3;}
        return 0;
    }
}
