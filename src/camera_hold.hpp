#pragma once
// 当前玩家的相对环视保持及连续环绕；不冻结机体、改世界坐标或吞掉干扰弹输入。
// 本文件在target_gaze.hpp之后包含，共享GetAxisValue与XInput入口。
struct CameraHold {
    ContinuousIdentity plane,input,view,focus,viewClass;
    std::array<ContinuousIdentity,3> cameras; // 原生槽0=第一人称、1=座舱、2=第三人称。
    Addr bindings{};
    int count{},capacity{},viewType{-1};
    uint64_t right{},up{},track{};
    std::atomic<bool> allowed{false};
    std::atomic<DWORD> thread{0};
    std::atomic<ULONGLONG> heartbeat{0};
    bool ready{},seeded{},rightDown{},nativePress{},longDone{},combo{},pendingView{},centering{},waitNeutral{},armed{};
    bool leftDown{},leftCandidate{},pendingCenter{};
    bool r3Gesture{},releaseHandled{}; // 整次按压及释放交接都接管，不能只吞第一个原生回调。
    bool centerKeySeeded{},centerKeyDown{};
    bool replaceR3{},padConnected{},holdEnabled{},toggleDone{},pendingToggle{};
    int centerKey{},sensitivity{100};
    unsigned nativeRequests{};
    bool frozenX{},frozenY{};
    bool poseValid{},viewTransfer{};
    ContinuousIdentity gazeTarget,gazeTransferTarget;
    bool gazeTransfer{};
    float gazeWeight{};
    ULONGLONG gazeTransferAt{};
    bool stickWas{},axisWas{};
    float heldX{},heldY{},poseX{},poseY{};
    double stickVelocityX{},stickVelocityY{}; // 统一环视参考角的度/秒。
    double stickYaw{},stickPitch{}; // 独立双精度角度；只在交给原生时把水平角折回一圈。
    bool stickOrbit{},linearCenter{};
    ULONGLONG stickSeenAt{};
    void resetStick() {
        stickOrbit=false;stickYaw=stickPitch=stickVelocityX=stickVelocityY=0;stickSeenAt=0;
    }
    SHORT padX{},padY{};
    ULONGLONG pressedAt{},releasedAt{},buttonReleasedAt{};
    std::string error;
    void resetPose() {
        armed=false;frozenX=frozenY=false;poseValid=viewTransfer=false;centering=false;waitNeutral=false;stickWas=axisWas=false;releasedAt=0;
        gazeTarget=gazeTransferTarget={};gazeTransfer=false;gazeWeight=0;gazeTransferAt=0;
        resetStick();linearCenter=false;
    }
    void cancelGesture() {
        seeded=false;rightDown=nativePress=longDone=combo=pendingView=r3Gesture=releaseHandled=false;
        toggleDone=pendingToggle=leftDown=leftCandidate=pendingCenter=false;
        pressedAt=buttonReleasedAt=0;nativeRequests=0;
    }
    void clear() {
        allowed=false;heartbeat=0;plane={};input={};view={};focus={};viewClass={};cameras={};bindings=0;count=capacity=0;
        right=up=track=0;viewType=-1;cancelGesture();
        centerKeySeeded=centerKeyDown=false;centerKey=0;sensitivity=100;
        replaceR3=padConnected=holdEnabled=false;
        padX=padY=0;resetPose();
    }
} cameraHold;
using CameraHoldUpdate=void(*)(void*,float,void*,void*);
using CameraHoldView=void(*)(void*);
std::array<CameraHoldUpdate,3> originalCameraHoldUpdates{};
CameraHoldView originalCameraHoldView{};
CameraHoldUpdate originalCameraHoldSpecial{};
// 两个输出入口均返回调用方的输出指针；公共第一/第三人称入口另有9个栈参数。
using CameraHoldOutput=void*(*)(void*,void*,float,void*,const void*,const void*,const void*,const void*,const void*,bool,const void*,const void*,const void*);
using CameraHoldCockpitOutput=void*(*)(void*,void*,float,void*,int,const void*,const void*);
CameraHoldOutput originalCameraHoldOutput{};
CameraHoldCockpitOutput originalCameraHoldCockpitOutput{};
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
bool cameraHoldIdentities(const ContinuousReadBatch& memory) {
    return memory.alive(cameraHold.plane) && memory.alive(cameraHold.input) && memory.alive(cameraHold.view) && memory.alive(cameraHold.focus) &&
        memory.read<Addr>(cameraHold.plane.address+0x118)==cameraHold.input.address &&
        memory.read<Addr>(cameraHold.plane.address+0x1250)==cameraHold.view.address &&
        memory.read<Addr>(cameraHold.input.address+0x20)==cameraHold.plane.address && memory.read<Addr>(cameraHold.view.address+0x20)==cameraHold.plane.address &&
        memory.read<Addr>(cameraHold.plane.address+0x1370)==cameraHold.focus.address && memory.read<Addr>(cameraHold.focus.address+0x20)==cameraHold.plane.address;
}
bool cameraHoldIdentities() {const ContinuousReadBatch memory;return cameraHoldIdentities(memory);}
bool cameraHoldCameraIdentities(const ContinuousReadBatch& memory) {
    for(int i=0;i<3;++i) {
        const auto& camera=cameraHold.cameras[i];
        if(!memory.alive(camera) || memory.read<Addr>(cameraHold.view.address+0x318+i*0x10)!=camera.address ||
            memory.read<Addr>(camera.address+0x20)!=cameraHold.plane.address || memory.read<Addr>(camera.address+0x330)!=cameraHold.plane.address ||
            memory.read<Addr>(camera.address+0x338)!=cameraHold.focus.address)return false;
    }
    return true;
}
// 只复用同一绑定的类型资格；七个对象及输入列表的当前身份/链接仍在每次调度核对。
bool cameraHoldBindingCurrent(Addr plane) {
    if(cameraHold.plane.address!=plane)return false;
    const ContinuousReadBatch memory;
    return cameraHoldIdentities(memory) &&
        memory.alive(cameraHold.viewClass) && memory.read<Addr>(cameraHold.view.address+0x10)==cameraHold.viewClass.address &&
        cameraHoldCameraIdentities(memory) &&
        memory.read<Addr>(cameraHold.input.address+0x110)==cameraHold.bindings &&
        memory.read<int>(cameraHold.input.address+0x118)==cameraHold.count &&
        memory.read<int>(cameraHold.input.address+0x11c)==cameraHold.capacity;
}
bool cameraHoldSameTarget(const ContinuousIdentity& a,const ContinuousIdentity& b) {
    return a.address && a.address==b.address && a.index==b.index && a.serial==b.serial;
}
// 只接续已经进入原生注视的模组保持注视；目标消失/换代属于正常退出，不停整个镜头功能。
ContinuousIdentity cameraHoldGazeTarget() {
    if(!targetGazeLive() || !targetGaze.active.load() || targetGaze.plane.address!=cameraHold.plane.address ||
        targetGaze.component.load()!=cameraHold.input.address || GetCurrentThreadId()!=targetGaze.thread.load() ||
        read<unsigned>(cameraHold.focus.address+0x108)-2u>1u)return {};
    const auto target=read<Addr>(cameraHold.focus.address+0x100);
    if(target)try {return continuousIdentity(target);}catch(...){}
    return {};
}
// 普通视角之间传递最近一帧的实际环视姿态；许可由调用方的游戏线程/前台/身份门槛保证。
void cameraHoldChangeView(int view) {
    if(cameraHold.viewType==view)return;
    const int previousView=cameraHold.viewType;
    const auto target=cameraHoldGazeTarget();
    const bool gaze=cameraHold.holdEnabled && !cameraHold.centering && previousView>=0 && previousView<=2 && view>=0 && view<=2 &&
        cameraHoldSameTarget(cameraHold.gazeTarget,target) && continuousAlive(cameraHold.cameras[previousView]);
    const float weight=gaze?(cameraHold.gazeTransfer?cameraHold.gazeWeight:
        read<float>(cameraHold.cameras[previousView].address+(previousView==1?0x39c:0x3e0))):0;
    require(!gaze || (std::isfinite(weight) && weight>=0 && weight<=1.001f),"镜头注视交接进度超出原生范围");
    const bool transfer=cameraHold.holdEnabled && cameraHold.poseValid && cameraHold.armed && !cameraHold.centering &&
        cameraHold.viewType>=0 && cameraHold.viewType<=2 && view>=0 && view<=2;
    const float x=cameraHold.poseX,y=cameraHold.poseY;
    const bool centering=cameraHold.centering,waitNeutral=cameraHold.waitNeutral,linearCenter=cameraHold.linearCenter;
    const bool orbit=cameraHold.stickOrbit;
    const double yaw=cameraHold.stickYaw,pitch=cameraHold.stickPitch;
    const double velocityX=cameraHold.stickVelocityX,velocityY=cameraHold.stickVelocityY;
    const auto stickSeenAt=cameraHold.stickSeenAt;
    cameraHold.resetPose();cameraHold.viewType=view;
    if(transfer) {
        const float scale=view==1?1.f:.95f;
        const bool reversed=read<unsigned char>(cameraHold.plane.address+0xe17)!=0;
        cameraHold.armed=true;cameraHold.frozenX=cameraHold.frozenY=true;
        cameraHold.poseValid=cameraHold.viewTransfer=true;cameraHold.poseX=x;cameraHold.poseY=y;
        cameraHold.heldX=x*scale;cameraHold.heldY=y*scale*(reversed?-1.f:1.f);
        // 普通换视角不制造一次松杆，也不重启速度滤波。
        cameraHold.stickOrbit=orbit;cameraHold.stickYaw=yaw;cameraHold.stickPitch=pitch;
        cameraHold.stickVelocityX=velocityX;cameraHold.stickVelocityY=velocityY;cameraHold.stickSeenAt=stickSeenAt;
    }else if(centering) {
        cameraHold.centering=true;cameraHold.waitNeutral=waitNeutral;cameraHold.linearCenter=linearCenter;
    }
    if(gaze) {
        cameraHold.gazeTarget=cameraHold.gazeTransferTarget=target;cameraHold.gazeTransfer=true;
        cameraHold.gazeWeight=std::clamp(weight,0.f,1.f);cameraHold.gazeTransferAt=GetTickCount64();
    }
    if(cameraHold.r3Gesture) {
        // 已切换视角不能再执行本次手势，但不能提前放行剩余原生回调。
        cameraHold.longDone=cameraHold.toggleDone=true;cameraHold.pendingView=false;cameraHold.releaseHandled=true;
    }else cameraHold.cancelGesture();
}
void cameraHoldCenter() {
    const bool linear=cameraHold.linearCenter || (cameraHold.holdEnabled && cameraHold.armed && cameraHold.viewType==1);
    cameraHold.resetPose();cameraHold.centering=true;cameraHold.waitNeutral=true;cameraHold.linearCenter=linear;
    // 明确回正结束模组锁定注视；功能关闭时原生按住注视仍由游戏处理。
    targetGaze.active=false;
}
// 右摇杆在保持模式下控制相对角速度，不能继续把偏转量当作绝对目标角。
// 第一/第三人称继续由原生消费等价角；普通座舱的连续朝向由下方输出作用域接续。
void cameraHoldMoveStick(int index,float rawX,float rawY,float x,float y,float delta,float& outX,float& outY) {
    if(!cameraHold.stickOrbit) {
        cameraHold.stickYaw=static_cast<double>(x)*180.;cameraHold.stickPitch=static_cast<double>(y)*90.;
        cameraHold.stickOrbit=true;
    }
    const bool reversed=read<unsigned char>(cameraHold.plane.address+0xe17)!=0;
    double commandX=std::clamp(static_cast<double>(rawX),-1.,1.);
    double commandY=std::clamp(static_cast<double>(rawY),-1.,1.)*(reversed?-1.:1.);
    // 复用游戏的映射、死区和响应曲线；只限制合成速度，防止斜向满推快sqrt(2)倍。
    const double length=std::sqrt(commandX*commandX+commandY*commandY);
    if(length>1.){commandX/=length;commandY/=length;}
    const double speed=180.*cameraHold.sensitivity/100.;
    commandX*=speed;commandY*=speed;
    // 25毫秒速度响应降低转向拖曳；松杆也用同一解析积分收尾，不在死区边缘突然清零。
    // 普通波动按完整游戏delta积分，避免超过50毫秒即变慢；大于100毫秒视为停顿，不补走。
    constexpr double response=.025;
    if(delta>.1f){cameraHold.stickVelocityX=cameraHold.stickVelocityY=0;}
    const double step=delta>.1f?0.:static_cast<double>(delta);
    const double blend=-std::expm1(-step/response);
    const auto integrate=[&](double command,double& velocity) {
        const double difference=velocity-command;
        const double distance=command*step+difference*response*blend;
        velocity=command+difference*(1.-blend);
        return distance;
    };
    cameraHold.stickYaw+=integrate(commandX,cameraHold.stickVelocityX);
    const double nextPitch=cameraHold.stickPitch+integrate(commandY,cameraHold.stickVelocityY);
    if(nextPitch<-90. || nextPitch>90.)cameraHold.stickVelocityY=0;
    cameraHold.stickPitch=std::clamp(nextPitch,-90.,90.);
    // 跨后方只改变同一方向的表示，不清水平速度；多圈后重置整圈数以保持数值精度。
    if(std::abs(cameraHold.stickYaw)>360.*1024.)cameraHold.stickYaw=std::remainder(cameraHold.stickYaw,360.);
    const float scale=index==1?1.f:.95f;
    outX=static_cast<float>(std::remainder(cameraHold.stickYaw,360.)/180.)*scale;
    outY=static_cast<float>(cameraHold.stickPitch/90.)*scale*(reversed?-1.f:1.f);
    if(commandX==0 && std::abs(cameraHold.stickVelocityX)<.001)cameraHold.stickVelocityX=0;
    if(commandY==0 && std::abs(cameraHold.stickVelocityY)<.001)cameraHold.stickVelocityY=0;
    cameraHold.frozenX=cameraHold.frozenY=false;
}
// R3满0.3秒只切换保持，不回正；本轮按住不重复。
void cameraHoldResolveHold(ULONGLONG duration) {
    if(!cameraHold.combo&&duration>=300&&!cameraHold.toggleDone) {
        cameraHold.toggleDone=cameraHold.longDone=true;cameraHold.pendingToggle=true;
    }
}
void cameraHoldResolveRelease() {
    if(!cameraHold.r3Gesture || cameraHold.rightDown || !cameraHold.buttonReleasedAt || cameraHold.releaseHandled)return;
    cameraHold.releaseHandled=true;
    if(cameraHold.combo)return;
    const auto duration=cameraHold.buttonReleasedAt-cameraHold.pressedAt;
    if(duration<300 && !cameraHold.longDone)cameraHold.pendingView=true;
    else cameraHoldResolveHold(duration); // 跨过阈值才松开时补触发，不能兼触短按。
}
void cameraHoldObserveCenterKey(int key,unsigned shortcutMask) {
    if(cameraHold.centerKey!=key){cameraHold.centerKey=key;cameraHold.centerKeySeeded=false;}
    const bool down=key && (GetAsyncKeyState(key)&0x8000)!=0;
    const int mods=((GetAsyncKeyState(VK_CONTROL)&0x8000)?1:0)|((GetAsyncKeyState(VK_MENU)&0x8000)?2:0)
        |((GetAsyncKeyState(VK_SHIFT)&0x8000)?4:0);
    // 回中在游戏线程观察，必须独立避开同主键的组合快捷键，不能只靠窗口消息被消费。
    const bool eligible=dualsp::singleKeyMatches(key,key,mods)&&!(shortcutMask&(1u<<mods));
    if(cameraHold.centerKeySeeded && down && !cameraHold.centerKeyDown && eligible)cameraHoldCenter();
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
    const bool left=(buttons&XINPUT_GAMEPAD_LEFT_THUMB)!=0;
    const bool both=right&&left;
    cameraHold.padX=state->Gamepad.sThumbRX;cameraHold.padY=state->Gamepad.sThumbRY;
    cameraHold.padConnected=true;
    if(!cameraHold.replaceR3){cameraHold.cancelGesture();return;} // 保持仍可用，L3/R3均归还原生。
    const auto now=GetTickCount64();
    if(!cameraHold.seeded) {
        cameraHold.seeded=true;cameraHold.rightDown=right;cameraHold.leftDown=left;cameraHold.leftCandidate=false;
        cameraHold.nativePress=false;cameraHold.longDone=cameraHold.toggleDone=right; // 已按住的旧边沿不补发。
        cameraHold.r3Gesture=right;cameraHold.releaseHandled=right;cameraHold.combo=right || (buttons&XINPUT_GAMEPAD_LEFT_THUMB);
        return;
    }
    if(right && !cameraHold.rightDown) {
        cameraHold.pressedAt=now;cameraHold.nativePress=false;cameraHold.longDone=cameraHold.toggleDone=false;cameraHold.pendingView=false;
        cameraHold.r3Gesture=true;cameraHold.releaseHandled=false;cameraHold.buttonReleasedAt=0;cameraHold.nativeRequests=0;
    }
    if(left&&!cameraHold.leftDown&&!cameraHold.combo)cameraHold.leftCandidate=!right;
    if(both && !cameraHold.combo) {
        cameraHold.combo=true;cameraHold.leftCandidate=cameraHold.pendingCenter=false;cameraHold.pendingView=cameraHold.pendingToggle=false;cameraHold.releaseHandled=true; // 双摇杆只交给原生干扰弹，不切视角、不回中。
    }
    if(!left&&cameraHold.leftDown&&cameraHold.leftCandidate&&!cameraHold.combo) {
        cameraHold.pendingCenter=true;cameraHold.leftCandidate=false;
    }
    cameraHold.leftDown=left;
    if(!right && cameraHold.rightDown)cameraHold.buttonReleasedAt=now;
    cameraHold.rightDown=right;
    cameraHoldResolveRelease(); // 释放后的重复回调仍属于同次按压。
    if(!right && !(buttons&XINPUT_GAMEPAD_LEFT_THUMB))cameraHold.combo=false; // 组合锁定至两边都松开。
}
// 整次R3按压的视角请求隔离；短按松开派发一次原生换视角，满300毫秒只切换保持。
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
        if(cameraHoldLive() && (cameraHold.holdEnabled || cameraHold.centering) && (index==cameraHold.viewType || (continuousAlive(cameraHold.view) && read<unsigned char>(cameraHold.view.address+0x350)==index)) &&
            reinterpret_cast<Addr>(component)==cameraHold.cameras[index].address &&
            reinterpret_cast<Addr>(actor)==cameraHold.plane.address && cameraHoldIdentities() &&
            continuousAlive(cameraHold.cameras[index])) {
            const int view=read<unsigned char>(cameraHold.view.address+0x350);
            if(view==index && read<unsigned char>(cameraHold.plane.address+0xe12) && !cameraHoldNativeSpecial()) {
                cameraHoldChangeView(view); // 在目标视角的首帧接续，不等待20毫秒Lua调度。
                // 仅当前普通视角查询一次物理状态，减少对20毫秒补采缓存的依赖。
                // 查询仍非引擎输入的原子快照；真正轴值和死区继续使用原生结果。
                XINPUT_STATE pad{};const auto padResult=originalTargetGazePad(0,&pad);cameraHoldObservePad(padResult,&pad);
                const float rawX=originalTargetGazeAxis(reinterpret_cast<void*>(cameraHold.input.address),cameraHold.right);
                const float rawY=originalTargetGazeAxis(reinterpret_cast<void*>(cameraHold.input.address),cameraHold.up);
                // 原生实际注视状态2/3及模组持续注视都使用原生输出插值，不应用环视灵敏度。
                // 此判据与0x420120一致，接入时校验其完整16字节；不另设长按等待时间。
                const unsigned focusState=read<unsigned>(cameraHold.focus.address+0x108);
                const bool targetTracking=(focusState-2u<=1u) || (targetGazeLive() && targetGaze.active.load());
                // 圆形活动门槛只用于识别输入来源；真正死区仍由游戏原生轴负责。
                const double padX=cameraHold.padX,padY=cameraHold.padY;
                const bool stick=cameraHold.padConnected && padX*padX+padY*padY>8192.*8192.;
                if(!std::isfinite(rawX) || !std::isfinite(rawY))throw std::runtime_error("镜头保持收到无效环视轴");
                const bool axes=std::abs(rawX)>.001f || std::abs(rawY)>.001f;
                const auto now=GetTickCount64();
                if(stick)cameraHold.stickSeenAt=now;
                // 已接管右杆后用较低来源门槛和短交接窗防抖；不修改玩家的游戏死区。
                // 松杆后的零输入继续由同一积分收尾，不能立即切回绝对角追近。
                const bool stickPresent=cameraHold.padConnected && padX*padX+padY*padY>4096.*4096.;
                const bool stickSource=stick || (cameraHold.stickOrbit && cameraHold.padConnected &&
                    (stickPresent || !axes || (cameraHold.stickSeenAt && now-cameraHold.stickSeenAt<=50)));
                if((!stick && cameraHold.stickWas) || (!axes && cameraHold.axisWas))cameraHold.releasedAt=now;
                cameraHold.stickWas=stick;cameraHold.axisWas=axes;
                // 第一/第三人称状态是[-1,1]，原生输入先夹到0.95再归一化；普通座舱没有这一步。
                const Addr state=reinterpret_cast<Addr>(component)+(index==1?0x38c:0x3d4);
                const float x=read<float>(state),y=read<float>(state+4);
                require(std::isfinite(x) && std::isfinite(y) && std::abs(x)<=1.01f && std::abs(y)<=1.01f,"镜头保持状态超出原生范围");
                {
                    // 原生输出将环视姿态与目标方向插值。保留环视基底，避免先归零再转向目标。
                    // 输入轴处理不覆盖跟踪轴；换视角时的注视缓存交接由下方输出入口单独处理。
                    if(!stick && !axes)cameraHold.waitNeutral=false;
                    if(cameraHold.holdEnabled && (stick || axes) && !cameraHold.waitNeutral) {
                        cameraHold.centering=cameraHold.linearCenter=false;cameraHold.armed=true;
                    }
                    // 摇杆回中或键鼠轴归零有短暂交接，先保持实际平滑姿态，不保存回弹尾值。
                    const bool releaseTail=!stick && cameraHold.armed && cameraHold.releasedAt && now-cameraHold.releasedAt<=50;
                    // 键鼠也从原生环视轴建立保持；模式键松开、轴归零后保存实际平滑姿态，不写死Alt。
                    float outX=rawX,outY=rawY; // 保留原生轴作为默认输出，手动回正与保持按需覆盖。
                    if(cameraHold.centering) {
                        outX=outY=0;
                        if(std::abs(x)<.0001f && std::abs(y)<.0001f && !cameraHold.waitNeutral)
                            cameraHold.centering=cameraHold.linearCenter=false;
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
                    const bool specialCockpit=index==1 && (read<unsigned char>(reinterpret_cast<Addr>(component)+0x370) ||
                        read<Addr>(reinterpret_cast<Addr>(component)+0x378));
                    const float trackingWeight=read<float>(reinterpret_cast<Addr>(component)+(index==1?0x39c:0x3e0));
                    if(cameraHold.holdEnabled && stickSource && cameraHold.armed && !cameraHold.centering &&
                        !targetTracking && !specialCockpit && std::isfinite(trackingWeight) && trackingWeight>=0 && trackingWeight<=.0001f &&
                        std::isfinite(delta) && delta>0) {
                        cameraHoldMoveStick(index,axes?rawX:0.f,axes?rawY:0.f,
                            cameraHold.viewTransfer?cameraHold.poseX:x,cameraHold.viewTransfer?cameraHold.poseY:y,delta,outX,outY);
                        // 位移已经按真实delta积分；原生delta*7取1，只消费本帧姿态一次，避免再次追近。
                        // 此分支没有目标注视/退出插值；特殊委托仍通过CameraHoldFrame归还真实delta。
                        cameraDelta=1.f/7.f;
                    }else cameraHold.resetStick();
                    cameraHoldFrame={true,cameraHold.input.address,outX,outY,delta};
                }
            }
        }
    }catch(const std::exception& error){cameraHoldFailure(error.what());cameraHoldFrame={};cameraDelta=delta;}
    // 第一/第三人称的等价角直接交给原生；特殊路径仍使用真实delta和原生边界。
    originalCameraHoldUpdates[index](component,cameraDelta,actor,camera);
    try {
        if(cameraHoldFrame.enabled) {
            if(cameraHoldFrame.delegated)cameraHold.resetPose(); // 特殊座舱不使用普通归一化姿态。
            else {
                cameraHold.viewTransfer=false;
                cameraHold.gazeTarget=cameraHoldGazeTarget();
                if(!cameraHoldSameTarget(cameraHold.gazeTarget,cameraHold.gazeTransferTarget))cameraHold.gazeTransfer=false;
                if(cameraHold.armed && !cameraHold.centering && cameraHoldLive() && cameraHoldIdentities() &&
                    continuousAlive(cameraHold.cameras[index]) && read<unsigned char>(cameraHold.view.address+0x350)==index) {
                    const Addr state=reinterpret_cast<Addr>(component)+(index==1?0x38c:0x3d4);
                    const float x=read<float>(state),y=read<float>(state+4);
                    require(std::isfinite(x) && std::isfinite(y) && std::abs(x)<=1.01f && std::abs(y)<=1.01f,"镜头保持状态超出原生范围");
                    cameraHold.poseX=std::clamp(x,-1.f,1.f);cameraHold.poseY=std::clamp(y,-1.f,1.f);cameraHold.poseValid=true;
                    // 原生飞行状态仍可能限制俯仰，只同步真实钳位，不逐帧损失双精度积分尾数。
                    if(cameraHold.stickOrbit && std::abs(cameraHold.stickPitch-static_cast<double>(y)*90.)>.001)
                        cameraHold.stickPitch=static_cast<double>(y)*90.;
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

// 仅在当前普通视角的输出入口接续姿态；不能把后台更新的其他相机当成当前视角。
bool cameraHoldOrbitOutputReady(Addr component,bool cockpit) {
    if((!cameraHold.holdEnabled && !cameraHold.linearCenter) || !cameraHoldLive() ||
        (!cameraHold.armed && !(cameraHold.centering && cameraHold.linearCenter)))return false;
    const ContinuousReadBatch memory;
    if(!cameraHoldIdentities(memory))return false;
    const int view=memory.read<unsigned char>(cameraHold.view.address+0x350);
    if(view>2 || (view==1)!=cockpit || component!=cameraHold.cameras[view].address ||
        !memory.alive(cameraHold.cameras[view]) || memory.read<Addr>(cameraHold.view.address+0x318+view*0x10)!=component ||
        memory.read<Addr>(component+0x330)!=cameraHold.plane.address || memory.read<Addr>(component+0x338)!=cameraHold.focus.address ||
        !memory.read<unsigned char>(cameraHold.plane.address+0xe12) || cameraHoldNativeSpecial())return false;
    if(cockpit && (memory.read<unsigned char>(component+0x370) || memory.read<Addr>(component+0x378)))return false;
    cameraHoldChangeView(view);
    return cameraHold.armed || (cameraHold.centering && cameraHold.linearCenter);
}
void cameraHoldSeedViewOutput(Addr component,bool cockpit) {
    if(!cameraHold.viewTransfer || !cameraHold.poseValid || !cameraHold.armed)return;
    const std::array<float,2> pose{cameraHold.poseX,cameraHold.poseY};
    for(const auto value:pose)require(std::isfinite(value) && std::abs(value)<=1.01f,"镜头保持状态超出原生范围");
    // 输出可能早于目标视角的首个输入更新；只接续当前组件的环视状态，不改机体或相机世界坐标。
    const Addr state=component+(cockpit?0x38c:0x3d4);
    checkMemory(state,sizeof(pose));write(state,pose);
}
struct alignas(16) CameraHoldTransform {
    std::array<float,4> rotation{},translation{},scale{};
};
static_assert(sizeof(CameraHoldTransform)==48,"连续环视变换必须保持原生48字节布局");
std::array<float,4> cameraHoldRotateLook(const std::array<float,4>& base,float x,float y) {
    // UE的正俯仰绕-Y轴，正水平角绕+Z轴；先俯仰再水平，最后接到本帧中立眼位旋转。
    constexpr double halfRadians=3.14159265358979323846/360.;
    const double sh=std::sin(static_cast<double>(x)*180.*halfRadians),ch=std::cos(static_cast<double>(x)*180.*halfRadians);
    const double sp=std::sin(static_cast<double>(y)*90.*halfRadians),cp=std::cos(static_cast<double>(y)*90.*halfRadians);
    const std::array<double,4> turn{sh*sp,-ch*sp,sh*cp,ch*cp};
    for(const auto value:base)require(std::isfinite(value),"镜头保持状态超出原生范围");
    const double norm=static_cast<double>(base[0])*base[0]+static_cast<double>(base[1])*base[1]+
        static_cast<double>(base[2])*base[2]+static_cast<double>(base[3])*base[3];
    require(std::isfinite(norm) && norm>.5 && norm<1.5,"镜头保持状态超出原生范围");
    std::array<float,4> result{
        static_cast<float>(base[3]*turn[0]+base[0]*turn[3]+base[1]*turn[2]-base[2]*turn[1]),
        static_cast<float>(base[3]*turn[1]-base[0]*turn[2]+base[1]*turn[3]+base[2]*turn[0]),
        static_cast<float>(base[3]*turn[2]+base[0]*turn[1]-base[1]*turn[0]+base[2]*turn[3]),
        static_cast<float>(base[3]*turn[3]-base[0]*turn[0]-base[1]*turn[1]-base[2]*turn[2])};
    const double length=std::sqrt(norm);
    for(auto& value:result)value=static_cast<float>(value/length);
    return result;
}
// 只在一次普通座舱原生输出调用内把环视轴置零，让原生以提供的朝向作为基底继续混合注视。
// 保存后直接还原同一8字节；析构不查询游戏对象、不抛异常，也不会覆盖注视缓存或输出坐标。
struct CameraHoldNeutralInput {
    Addr state{};
    std::array<float,2> saved{};
    CameraHoldNeutralInput()=default;
    CameraHoldNeutralInput(const CameraHoldNeutralInput&)=delete;
    CameraHoldNeutralInput& operator=(const CameraHoldNeutralInput&)=delete;
    void begin(Addr address) {
        checkMemory(address,sizeof(saved));saved=read<std::array<float,2>>(address);
        const std::array<float,2> neutral{};
        write(address,neutral);state=address;
    }
    void restore() noexcept {
        if(state){std::memcpy(reinterpret_cast<void*>(state),saved.data(),sizeof(saved));state=0;}
    }
    ~CameraHoldNeutralInput(){restore();}
};

// 输出钩子也可在首次可见帧发现换视角，不能等待20毫秒Lua调度或假定输入更新先到。
bool cameraHoldGazeOutputReady(Addr component,bool cockpit) {
    if(!cameraHold.holdEnabled)return false;
    if(!cameraHold.gazeTransfer) {
        if(!cameraHold.gazeTarget.address)return false;
        if(cameraHold.viewType>=0 && cameraHold.viewType<=2 && component==cameraHold.cameras[cameraHold.viewType].address)return false;
    }
    if(!cameraHoldLive() || !cameraHoldIdentities())return false;
    const int view=read<unsigned char>(cameraHold.view.address+0x350);
    if(view>2 || (view==1)!=cockpit || component!=cameraHold.cameras[view].address || !continuousAlive(cameraHold.cameras[view]) ||
        read<Addr>(cameraHold.view.address+0x318+view*0x10)!=component ||
        read<Addr>(component+0x330)!=cameraHold.plane.address || read<Addr>(component+0x338)!=cameraHold.focus.address ||
        !read<unsigned char>(cameraHold.plane.address+0xe12) || cameraHoldNativeSpecial())return false;
    cameraHoldChangeView(view);
    if(!cameraHold.gazeTransfer)return false;
    if(GetTickCount64()-cameraHold.gazeTransferAt>250 || cameraHold.centering ||
        !cameraHoldSameTarget(cameraHold.gazeTransferTarget,cameraHoldGazeTarget())) {
        cameraHold.gazeTransfer=false;return false;
    }
    return true;
}
void* cameraHoldOutput(void* component,void* output,float delta,void* context,const void* a5,const void* a6,
    const void* origin,const void* a8,const void* a9,bool a10,const void* a11,const void* a12,const void* a13) {
    try {
        const auto address=reinterpret_cast<Addr>(component);
        if(cameraHold.holdEnabled && cameraHold.poseValid && cameraHold.armed &&
            (cameraHold.viewTransfer || cameraHold.viewType<0 || cameraHold.viewType>2 ||
                address!=cameraHold.cameras[cameraHold.viewType].address) && cameraHoldOrbitOutputReady(address,false))
            cameraHoldSeedViewOutput(address,false);
        if(cameraHoldGazeOutputReady(address,false)) {
            // 原生输出使用同一个世界位置参数。只初始化输入组件的注视方向缓存，不写输出坐标/四元数。
            std::array<float,3> direction{};float lengthSquared=0;
            for(size_t i=0;i<direction.size();++i) {
                direction[i]=read<float>(cameraHold.focus.address+0x120+i*4)-read<float>(reinterpret_cast<Addr>(origin)+i*4);
                require(std::isfinite(direction[i]),"镜头注视交接方向无效");lengthSquared+=direction[i]*direction[i];
            }
            if(std::isfinite(lengthSquared) && lengthSquared>1.e-8f) {
                const float length=std::sqrt(lengthSquared);
                for(auto& value:direction)value/=length;
                checkMemory(address+0x3bc,sizeof(direction));checkMemory(address+0x3e0,sizeof(float));
                write(address+0x3bc,direction);write(address+0x3e0,cameraHold.gazeWeight);
            }
            cameraHold.gazeTransfer=false;
        }
    }catch(const std::exception& error){cameraHoldFailure(error.what());}
    return originalCameraHoldOutput(component,output,delta,context,a5,a6,origin,a8,a9,a10,a11,a12,a13);
}
void* cameraHoldCockpitOutput(void* component,void* output,float delta,void* context,int mode,const void* a6,const void* a7) {
    CameraHoldNeutralInput neutral;
    CameraHoldTransform look;
    const void* base=a7;
    try {
        const auto address=reinterpret_cast<Addr>(component);
        if(cameraHoldGazeOutputReady(address,true)) {
            // 委托镜头没有普通座舱字段的语义，继续走原生；普通座舱沿用原生范围计算及缓动函数。
            if(!read<unsigned char>(address+0x370) && !read<Addr>(address+0x378)) {
                float negative{},positive{};
                reinterpret_cast<void(*)(void*,int,float*,float*)>(continuousBase()+0x424200)(context,mode,&negative,&positive);
                const float pitch=read<float>(address+0x394),rawYaw=read<float>(address+0x398);
                const float extent=rawYaw<0?negative:positive;
                const float scale=read<float>(continuousBase()+(rawYaw<0?0x25744b8:0x257449c));
                if(std::isfinite(pitch) && std::abs(pitch)<=1.01f && std::isfinite(rawYaw) && std::isfinite(scale) &&
                    std::isfinite(extent) && std::abs(extent)>1.e-6f) {
                    const float yaw=std::clamp(rawYaw*scale/extent,-1.f,1.f);
                    const float eased=reinterpret_cast<float(*)(float)>(continuousBase()+0x41fa10)(cameraHold.gazeWeight);
                    require(std::isfinite(yaw) && std::isfinite(eased),"镜头注视交接方向无效");
                    if(cameraHoldGazeOutputReady(address,true)) {
                        checkMemory(address+0x39c,4*sizeof(float));
                        write(address+0x39c,cameraHold.gazeWeight);write(address+0x3a0,eased);
                        write(address+0x3a4,pitch);write(address+0x3a8,yaw);
                    }
                }
            }
            cameraHold.gazeTransfer=false;
        }
        if(cameraHoldOrbitOutputReady(address,true)) {
            cameraHoldSeedViewOutput(address,true);
            const auto pose=read<std::array<float,2>>(address+0x38c);
            for(const auto value:pose)require(std::isfinite(value) && std::abs(value)<=1.01f,"镜头保持状态超出原生范围");
            // a7是原生中立眼位FTransform；只替换传入副本的旋转，保留平移、缩放及本帧动画基底。
            // 不再用受限的左右环视轨迹表示360度；注视目标仍由同一次原生输出按真实delta混合。
            look=read<CameraHoldTransform>(reinterpret_cast<Addr>(a7));
            look.rotation=cameraHoldRotateLook(look.rotation,pose[0],pose[1]);
            neutral.begin(address+0x38c);base=&look;
        }
    }catch(const std::exception& error){neutral.restore();base=a7;cameraHoldFailure(error.what());}
    return originalCameraHoldCockpitOutput(component,output,delta,context,mode,a6,base);
}

void installCameraHold() {
    if(cameraHold.ready)return;
    installTargetGaze();
    constexpr std::array<Addr,7> rvas{0x41e150,0x424cf0,0x43df90,0x821430,0x421320,0x41e350,0x424610};
    constexpr unsigned char signatures[7][16]={
        {0x48,0x89,0x5c,0x24,0x08,0x57,0x48,0x83,0xec,0x60,0x48,0x8b,0x81,0x30,0x03,0x00},
        {0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x6c,0x24,0x10,0x48,0x89,0x74,0x24,0x18,0x57},
        {0x48,0x89,0x5c,0x24,0x08,0x57,0x48,0x83,0xec,0x50,0x0f,0x29,0x74,0x24,0x40,0x48},
        {0x40,0x53,0x48,0x83,0xec,0x20,0x48,0x83,0xb9,0x50,0x12,0x00,0x00,0x00,0x48,0x8b},
        {0x4c,0x8b,0xdc,0x49,0x89,0x6b,0x10,0x49,0x89,0x73,0x18,0x57,0x48,0x81,0xec,0x90},
        {0x48,0x8b,0xc4,0x55,0x53,0x56,0x57,0x41,0x56,0x48,0x8d,0xa8,0xe8,0xfe,0xff,0xff},
        {0x40,0x55,0x53,0x56,0x57,0x41,0x56,0x41,0x57,0x48,0x8d,0xac,0x24,0x48,0xff,0xff}};
    const auto base=continuousBase();
    constexpr unsigned char special[]={0x40,0x53,0x48,0x83,0xec,0x20,0x48,0x8b,0x05,0x5b,0xd1,0x8e,0x03};
    checkMemory(base+0x3ceac0,sizeof(special));
    require(std::memcmp(reinterpret_cast<void*>(base+0x3ceac0),special,sizeof(special))==0,"镜头保持特殊视角判据不匹配");
    constexpr unsigned char focused[]={0x8b,0x81,0x08,0x01,0x00,0x00,0x83,0xe8,0x02,0x83,0xf8,0x01,0x0f,0x96,0xc0,0xc3};
    checkMemory(base+0x420120,sizeof(focused));
    require(std::memcmp(reinterpret_cast<void*>(base+0x420120),focused,sizeof(focused))==0,"镜头保持实际注视判据不匹配");
    constexpr unsigned char bounds[]={0x48,0x8b,0xc4,0x53,0x55,0x56,0x57,0x48,0x81,0xec,0xf8,0,0,0,0x0f,0x29};
    constexpr unsigned char ease[]={0x0f,0x57,0xe4,0x0f,0x2f,0xc4,0x77,0x04,0x0f,0x28,0xc4,0xc3,0xf3,0x0f,0x10,0x0d};
    checkMemory(base+0x424200,sizeof(bounds));checkMemory(base+0x41fa10,sizeof(ease));
    require(std::memcmp(reinterpret_cast<void*>(base+0x424200),bounds,sizeof(bounds))==0 &&
        std::memcmp(reinterpret_cast<void*>(base+0x41fa10),ease,sizeof(ease))==0,"镜头注视交接接口不匹配");
    // 连续座舱朝向还依赖第7参数及48字节变换布局，不能只核对函数入口。
    constexpr unsigned char cockpitBase[]={0x48,0x8b,0x9d,0x20,0x01,0x00,0x00,0x48,0x8b,0xf2,0x48,0x8b,0xf9};
    constexpr unsigned char cockpitLayout[]={0x44,0x0f,0x29,0x6e,0x10,0x44,0x0f,0x28,0xac,0x24,0x30,0x01,0x00,0x00,0x44,0x0f,0x29,0x76,0x20};
    checkMemory(base+0x424652,sizeof(cockpitBase));checkMemory(base+0x424ca8,sizeof(cockpitLayout));
    require(std::memcmp(reinterpret_cast<void*>(base+0x424652),cockpitBase,sizeof(cockpitBase))==0 &&
        std::memcmp(reinterpret_cast<void*>(base+0x424ca8),cockpitLayout,sizeof(cockpitLayout))==0,"镜头保持接口与当前游戏不匹配");
    for(size_t i=0;i<rvas.size();++i) {
        checkMemory(base+rvas[i],sizeof(signatures[i]));
        require(std::memcmp(reinterpret_cast<void*>(base+rvas[i]),signatures[i],sizeof(signatures[i]))==0,"镜头保持接口与当前游戏不匹配");
    }
    std::array<void*,7> hooks{reinterpret_cast<void*>(&cameraHoldFirst),reinterpret_cast<void*>(&cameraHoldCockpit),
        reinterpret_cast<void*>(&cameraHoldThird),reinterpret_cast<void*>(&cameraHoldViewPressed),reinterpret_cast<void*>(&cameraHoldSpecial),
        reinterpret_cast<void*>(&cameraHoldOutput),reinterpret_cast<void*>(&cameraHoldCockpitOutput)};
    size_t created=0;
    try {
        for(size_t i=0;i<rvas.size();++i) {
            void** original=i<3?reinterpret_cast<void**>(&originalCameraHoldUpdates[i]):i==3?reinterpret_cast<void**>(&originalCameraHoldView):
                i==4?reinterpret_cast<void**>(&originalCameraHoldSpecial):i==5?reinterpret_cast<void**>(&originalCameraHoldOutput):reinterpret_cast<void**>(&originalCameraHoldCockpitOutput);
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
    cameraHold.viewClass=continuousIdentity(read<Addr>(cameraHold.view.address+0x10));
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
        require(api->top(luaState)==7,"镜头保持参数数量无效");
        const auto plane=static_cast<Addr>(api->readInteger(luaState,1));const auto allowed=api->readInteger(luaState,2);
        const auto centerKey=api->readInteger(luaState,3),sensitivity=api->readInteger(luaState,4);
        const auto replaceR3=api->readInteger(luaState,5);
        const auto shortcutMask=api->readInteger(luaState,6);
        const auto holdEnabled=api->readInteger(luaState,7);
        require(holdEnabled==0 || holdEnabled==1,"镜头保持设置无效");
        require(allowed==0 || allowed==1,"镜头保持设置无效");
        require(centerKey==0 || centerKey==VK_MBUTTON || dualsp::supportedShortcutKey(static_cast<int>(centerKey)),"镜头回正键位无效");
        require(centerKey>=0 && centerKey<=255 && sensitivity>=5 && sensitivity<=200 && (replaceR3==0 || replaceR3==1),"镜头保持参数超出范围");
        require(shortcutMask>=0&&shortcutMask<=255,"镜头回中组合键占用信息无效");
        std::string reason;
        if(!allowed || !plane || !targetGazeFocused()){cameraHold.clear();cameraHold.error.clear();}
        else {
            require(cameraHold.error.empty(),cameraHold.error.c_str());
            installCameraHold();
            const bool same=cameraHoldBindingCurrent(plane);
            bool pending=false;
            if(!same) {
                continuousIdentity(plane);
                const auto input=read<Addr>(plane+0x118);
                const auto viewObject=read<Addr>(plane+0x1250);
                pending=!input || !viewObject || !read<Addr>(plane+0x1370) || !read<int>(input+0x118);
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
            }
            if(pending) {
                cameraHold.clear();reason="waiting_input";
            }else {
                if(!same)bindCameraHold(plane);
                const int view=read<unsigned char>(cameraHold.view.address+0x350);
                if(view>2 || cameraHoldNativeSpecial()) {cameraHold.resetPose();cameraHold.cancelGesture();cameraHold.centerKeySeeded=false;reason="special_view";cameraHold.allowed=false;}
                else {
                    const auto now=GetTickCount64();
                    if(cameraHold.heartbeat.load() && now-cameraHold.heartbeat.load()>500) {
                        cameraHold.resetPose();cameraHold.cancelGesture();cameraHold.centerKeySeeded=false;
                    }
                    if(cameraHold.holdEnabled!=(holdEnabled==1)) {
                        // 座舱从连续朝向平滑回中后再交还动画轨迹；不顺带关闭独立锁定注视。
                        const bool linear=cameraHold.linearCenter || (cameraHold.holdEnabled && cameraHold.armed && view==1);
                        cameraHold.resetPose();
                        if(linear){cameraHold.centering=true;cameraHold.waitNeutral=true;cameraHold.linearCenter=true;}
                    } // 保留本次R3手势，避免切换后重复计时。
                    cameraHold.holdEnabled=holdEnabled==1;
                    cameraHoldChangeView(view);
                    cameraHold.heartbeat=now;cameraHold.allowed=true;
                    cameraHold.sensitivity=static_cast<int>(sensitivity);
                    if(cameraHold.replaceR3!=(replaceR3==1))cameraHold.cancelGesture();
                    cameraHold.replaceR3=replaceR3==1;
                    XINPUT_STATE state{};const auto result=originalTargetGazePad(0,&state);cameraHoldObservePad(result,&state);
                    if(cameraHold.pendingCenter&&!cameraHold.combo) {cameraHold.pendingCenter=false;cameraHoldCenter();}
                    if(cameraHold.holdEnabled)cameraHoldObserveCenterKey(static_cast<int>(centerKey),static_cast<unsigned>(shortcutMask));
                    else cameraHold.centerKeySeeded=false;
                    if(cameraHold.replaceR3 && cameraHold.rightDown && !cameraHold.combo)
                        cameraHoldResolveHold(GetTickCount64()-cameraHold.pressedAt);
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
        const bool toggle=cameraHold.pendingToggle;cameraHold.pendingToggle=false;
        api->boolean(luaState,true);api->boolean(luaState,cameraHold.allowed.load() && cameraHold.holdEnabled);
        api->string(luaState,reason.data(),reason.size());api->boolean(luaState,toggle);return 4;
    }catch(const std::exception& error) {
        const std::string reason=error.what();cameraHoldFailure(reason.c_str());
        if(api){api->boolean(luaState,false);api->boolean(luaState,false);api->string(luaState,reason.data(),reason.size());api->boolean(luaState,false);return 4;}
        return 0;
    }
}
