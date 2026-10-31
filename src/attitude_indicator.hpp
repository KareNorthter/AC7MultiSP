#pragma once
// 在continuous_sp/target_gaze之后、camera_hold之前包含；只更新本功能自建的UMG图形。
void installCameraHold();
bool cameraHoldNativeSpecial();

struct AttitudeSetter {
    ContinuousIdentity function;
    int bytes{};
};
struct AttitudeIndicator {
    ContinuousIdentity plane,root,view,hud,horizon,cursor,nativeHolder,failureOwner;
    std::array<ContinuousIdentity,3> cameras;
    AttitudeSetter angle,translation,visibility;
    using Ref=const void*(*)(const void*);
    using Process=void(*)(void*,void*,void*);
    using Available=bool(*)();
    Ref parms{},numParms{};
    Process process{};Available processAvailable{};
    std::atomic<bool> enabled{false};
    DWORD thread{};
    ULONGLONG heartbeat{};
    ULONGLONG sampledAt{};
    bool visible{},sampled{},inside{},nativeHidden{};
    unsigned viewMask{};
    float pitch{},bank{},lastPitch{1000},lastBank{1000};
    float travel{24};
    int shownPitch{1000},shownBank{1000};
    std::string error;

    void bindSdk() {
        if(process)return;
        const auto module=GetModuleHandleW(L"UE4SS.dll");
        require(module!=nullptr,"Attitude: UE4SS is unavailable");
        auto get=[&](const char* name){auto result=GetProcAddress(module,name);
            require(result!=nullptr,"Attitude: UMG SDK export is unavailable");return result;};
        parms=reinterpret_cast<Ref>(get("?GetParmsSize@UFunction@Unreal@RC@@QEBAAEBGXZ"));
        numParms=reinterpret_cast<Ref>(get("?GetNumParms@UFunction@Unreal@RC@@QEBAAEBEXZ"));
        processAvailable=reinterpret_cast<Available>(get("?IsProcessEventAvailable@UE4SSRuntime@RC@@SA_NXZ"));
        const auto candidate=reinterpret_cast<Process>(get("?ProcessEvent@UObject@Unreal@RC@@QEAAXPEAVUFunction@23@PEAX@Z"));
        require(processAvailable(),"Attitude: ProcessEvent is not ready");process=candidate;
    }
    AttitudeSetter setter(Addr address,const char* name,int bytes,const char* kind) {
        const auto id=continuousIdentity(address);
        require(continuousName(address)==name && continuousName(read<Addr>(address+0x10))=="Function",
            "Attitude: setter name/type mismatch");
        const auto owner=read<Addr>(address+0x20);
        require(continuousName(owner)=="Widget" && continuousName(read<Addr>(owner+0x20))=="/Script/UMG",
            "Attitude: setter is not owned by UMG.Widget");
        const auto size=reinterpret_cast<const unsigned short*>(parms(reinterpret_cast<void*>(address)));
        const auto count=reinterpret_cast<const unsigned char*>(numParms(reinterpret_cast<void*>(address)));
        require(size && count && read<unsigned short>(reinterpret_cast<Addr>(size))==bytes &&
            read<unsigned char>(reinterpret_cast<Addr>(count))==1,"Attitude: setter parameter layout mismatch");
        const auto field=read<Addr>(address+0x38);continuousIdentity(field);
        require(!read<Addr>(field+0x28) && read<int>(field+0x34)==bytes && read<int>(field+0x44)==0 &&
            continuousName(read<Addr>(field+0x10))==kind,"Attitude: setter parameter type mismatch");
        return {id,bytes};
    }
    template<class T> void invoke(ContinuousIdentity widget,AttitudeSetter method,const T& value) {
        require(sizeof(T)==static_cast<size_t>(method.bytes),"Attitude: setter value size mismatch");
        {const ContinuousReadBatch memory;
            require(memory.alive(widget)&&memory.alive(method.function),"Attitude: UMG identity expired");}
        require(process && processAvailable(),"Attitude: ProcessEvent is unavailable");
        // setter可能触发布局失效；调用后不沿用前一只读批次的查页结果。
        T parameter=value;
        process(reinterpret_cast<void*>(widget.address),reinterpret_cast<void*>(method.function.address),&parameter);
        {const ContinuousReadBatch memory;
            require(memory.alive(widget)&&memory.alive(method.function),"Attitude: UMG identity changed during setter");}
    }
    void show(bool value,bool replaceNative=false) {
        if(visible!=value) {invoke(hud,visibility,static_cast<unsigned char>(value?3:1));visible=value;}
        const bool hideNative=value&&replaceNative;
        if(nativeHidden!=hideNative) {
            if(continuousAlive(nativeHolder))invoke(nativeHolder,visibility,static_cast<unsigned char>(hideNative?1:3));
            else require(!hideNative,"Attitude: native pitch holder expired");
            nativeHidden=hideNative;
        }
    }
    void stop() {
        enabled.store(false,std::memory_order_release);
        if(!continuousAlive(hud))visible=false;
        show(false); // 也归还原生大姿态尺，不能只折叠简易图形。
        heartbeat=sampledAt=0;sampled=false;lastPitch=lastBank=1000;shownPitch=shownBank=1000;
    }
    bool current(Addr player,Addr indicator,Addr roll,Addr pointer,unsigned mask,Addr holder) {
        const ContinuousReadBatch memory;
        bool source=memory.alive(plane)&&memory.alive(root)&&memory.alive(view);
        for(size_t i=0;i<cameras.size();++i)source=source&&memory.alive(cameras[i])&&
            memory.read<Addr>(view.address+0x318+i*0x10)==cameras[i].address;
        return source&&viewMask==mask&&nativeHolder.address==holder&&(!holder||memory.alive(nativeHolder))&&
            plane.address==player&&hud.address==indicator&&horizon.address==roll&&cursor.address==pointer &&
            memory.alive(hud)&&memory.alive(horizon)&&memory.alive(cursor)&&
            memory.read<Addr>(player+0x160)==root.address&&memory.read<Addr>(player+0x1250)==view.address;
    }
    void bind(Addr player,Addr indicator,Addr roll,Addr pointer,Addr angleFunction,Addr moveFunction,Addr visibilityFunction,float range,unsigned mask,Addr holder) {
        stop();verifyVersion();bindSdk();installCameraHold();
        const auto owner=continuousIdentity(player);
        require(!(failureOwner.address==owner.address&&failureOwner.index==owner.index&&failureOwner.serial==owner.serial),
            "Attitude: this player instance is disabled after an earlier failure");
        require(mountClassIs(player,"PlayerPlane","/Script/Nimbus"),"Attitude: player class mismatch");
        plane=owner;thread=GetCurrentThreadId();
        root=continuousIdentity(read<Addr>(player+0x160));view=continuousIdentity(read<Addr>(player+0x1250));
        require(mountClassIs(root.address,"SceneComponent","/Script/Engine")&&
            mountClassIs(view.address,"CameraViewComponent","/Script/Nimbus"),"Attitude: source component type mismatch");
        require(read<Addr>(root.address+0x20)==player&&read<Addr>(view.address+0x20)==player,"Attitude: source ownership mismatch");
        const char* classes[]={"FirstPersonCameraInputComponent","CockpitCameraInputComponent","ThirdPersonCameraInputComponent"};
        for(size_t i=0;i<cameras.size();++i) {
            cameras[i]=continuousIdentity(read<Addr>(view.address+0x318+i*0x10));
            require(mountClassIs(cameras[i].address,classes[i],"/Script/Nimbus")&&
                read<Addr>(cameras[i].address+0x20)==player&&
                (!read<Addr>(cameras[i].address+0x330)||read<Addr>(cameras[i].address+0x330)==player),
                "Attitude: camera ownership mismatch");
        }
        hud=continuousIdentity(indicator);horizon=continuousIdentity(roll);cursor=continuousIdentity(pointer);
        for(auto object:{indicator,roll,pointer})require(mountClassIs(object,"CanvasPanel","/Script/UMG"),"Attitude: target must be a CanvasPanel");
        require(continuousName(indicator).rfind("DualSPAttitudeRoot",0)==0&&
            continuousName(roll).rfind("DualSPAttitudeHorizon",0)==0&&continuousName(pointer).rfind("DualSPAttitudeCursor",0)==0,
            "Attitude: target is not an owned indicator widget");
        // 仅接受Lua交付的自建控件和确定的三种setter，不解析任意函数签名。
        angle=setter(angleFunction,"SetRenderAngle",4,"FloatProperty");
        translation=setter(moveFunction,"SetRenderTranslation",8,"StructProperty");
        visibility=setter(visibilityFunction,"SetVisibility",1,"EnumProperty");
        require(mask==3||mask==4||mask==7,"Attitude: invalid view mask");
        nativeHolder={};nativeHidden=false;viewMask=mask;
        if(mask&3) {
            nativeHolder=continuousIdentity(holder);
            require(mountClassIs(holder,"CanvasPanel","/Script/UMG")&&continuousName(holder).rfind("DualSPAttitudeNativeHolder",0)==0,
                "Attitude: native pitch holder is not owned by this indicator");
        }else require(!holder,"Attitude: unexpected native pitch holder");
        require(std::isfinite(range)&&range>0&&range<=96,"Attitude: pitch cursor range invalid");
        travel=range;error.clear();sampled=false;lastPitch=lastBank=1000;shownPitch=shownBank=1000;
    }
    void frame(Addr component) noexcept {
        if(!enabled.load(std::memory_order_acquire)||GetCurrentThreadId()!=thread||inside)return;
        inside=true;
        try {
            const auto now=GetTickCount64();
            if(!runtimeEnabled||now-heartbeat>150) {show(false);sampled=false;inside=false;return;}
            std::array<float,4> q{};
            bool draw=false,sourceAlive=false,uiAlive=false;
            unsigned type=3;
            {const ContinuousReadBatch memory;
                uiAlive=memory.alive(hud)&&memory.alive(horizon)&&memory.alive(cursor)&&(!nativeHolder.address||memory.alive(nativeHolder));
                sourceAlive=memory.alive(plane)&&memory.alive(root)&&memory.alive(view)&&
                    memory.read<Addr>(plane.address+0x160)==root.address&&memory.read<Addr>(plane.address+0x1250)==view.address;
                for(size_t i=0;i<cameras.size();++i)sourceAlive=sourceAlive&&memory.alive(cameras[i])&&
                    memory.read<Addr>(view.address+0x318+i*0x10)==cameras[i].address;
                if(sourceAlive)type=memory.read<unsigned char>(view.address+0x350);
                // 后台相机也可能计算输出；不能由它覆盖当前视角的同帧显隐。
                if(sourceAlive&&uiAlive&&type<3&&component!=cameras[type].address){inside=false;return;}
                draw=sourceAlive&&uiAlive&&type<3&&(viewMask&(1u<<type))&&
                    memory.read<Addr>(cameras[type].address+0x330)==plane.address&&memory.read<unsigned char>(plane.address+0xe12)!=0;
                if(draw)q=memory.read<std::array<float,4>>(root.address+0x180);
            }
            // HUD/玩家正常销毁或重建只等待重新绑定，不把过渡状态记为机体故障。
            if(!sourceAlive||!uiAlive) {
                enabled.store(false,std::memory_order_release);sampled=false;
                if(!continuousAlive(hud))visible=false;
                show(false);
                inside=false;return;
            }
            if(!draw||cameraHoldNativeSpecial()){show(false);sampled=false;inside=false;return;}
            const double x=q[0],y=q[1],z=q[2],w=q[3],norm=x*x+y*y+z*z+w*w;
            require(std::isfinite(norm)&&norm>.9&&norm<1.1,"Attitude: invalid player quaternion");
            constexpr double degrees=180./3.14159265358979323846;
            const double forwardZ=std::clamp(2.*(x*z-w*y)/norm,-1.,1.);
            pitch=static_cast<float>(std::asin(forwardZ)*degrees);
            // UE正俯仰绕-Y，正滚转使右机翼向下。近垂直时保留最后一个可定义的滚转角。
            if(1.-forwardZ*forwardZ>1.e-6)
                bank=static_cast<float>(std::atan2(-2.*(y*z+w*x)/norm,1.-2.*(x*x+y*y)/norm)*degrees);
            else if(!sampled)bank=0;
            // 固定倒T表示机体；地平圈反向转动，正B右倾时水平线向左转。
            if(bank!=lastBank){invoke(horizon,angle,-bank);lastBank=bank;}
            if(pitch!=lastPitch){invoke(cursor,translation,std::array<float,2>{0,-pitch/90.f*travel});lastPitch=pitch;}
            show(true,type<2);sampled=true;sampledAt=now;
        }catch(const std::exception& exception) {
            enabled.store(false,std::memory_order_release);failureOwner=plane;error=exception.what();
            try {show(false);}catch(...) {} // Lua诊断与清理仍会再次隐藏/释放自有根容器。
            nativeLog(std::string("ATTITUDE_INDICATOR_ERROR ")+error);
        }
        inside=false;
    }
} attitudeIndicator;

void attitudeCameraFrame(Addr component) noexcept {attitudeIndicator.frame(component);}
