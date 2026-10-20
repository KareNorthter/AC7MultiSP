#pragma once
// 只在原生NEXT维护的同步作用域中调整候选顺序与参考视线，锁定/发射仍走原生。
struct ViewTargetVector { float x{},y{},z{}; };
struct ViewTargetArray { Addr data{};int count{},capacity{}; };
static_assert(sizeof(ViewTargetVector)==12&&sizeof(ViewTargetArray)==16,"原生视点及候选数组布局必须保持一致");
struct ViewTargetState {
    ContinuousIdentity plane,selection,controller,manager,root,view;
    Addr controllerOffset{};
    DWORD thread{};
    ULONGLONG heartbeat{};
    bool allowed{},enabled{};
    std::string error;
    void clear(){plane=selection=controller=manager=root=view={};controllerOffset=0;thread=0;heartbeat=0;allowed=enabled=false;viewTargetPad.clear();}
} viewTarget;
struct ViewTargetFrame {bool active{};ViewTargetVector position,rotation;};
thread_local ViewTargetFrame viewTargetFrame;
struct ViewTargetFrameScope {
    ViewTargetFrame previous;
    explicit ViewTargetFrameScope(ViewTargetFrame current):previous(viewTargetFrame){viewTargetFrame=current;}
    ~ViewTargetFrameScope(){viewTargetFrame=previous;}
};
using ViewTargetNext=void(*)(void*);
using ViewTargetReference=void(*)(void*,bool,ViewTargetVector*,ViewTargetVector*);
ViewTargetNext originalViewTargetNext{};
ViewTargetReference originalViewTargetReference{};

bool viewTargetFinite(ViewTargetVector v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);}
ViewTargetVector viewTargetForward(ViewTargetVector rotation) {
    constexpr double radians=3.14159265358979323846/180.;
    const double pitch=rotation.x*radians,yaw=rotation.y*radians;
    return {static_cast<float>(std::cos(pitch)*std::cos(yaw)),static_cast<float>(std::cos(pitch)*std::sin(yaw)),static_cast<float>(std::sin(pitch))};
}
bool viewTargetIdentities() {
    const ContinuousReadBatch memory; // 只在本次纯读取内复用查页，不跨原生调用或写入缓存身份结论。
    return memory.alive(viewTarget.plane)&&memory.alive(viewTarget.selection)&&memory.alive(viewTarget.controller)&&
        memory.alive(viewTarget.manager)&&memory.alive(viewTarget.root)&&memory.alive(viewTarget.view)&&
        memory.read<Addr>(viewTarget.plane.address+0x1310)==viewTarget.selection.address&&
        memory.read<Addr>(viewTarget.selection.address+0x20)==viewTarget.plane.address&&
        memory.read<Addr>(viewTarget.selection.address+0x378)==viewTarget.plane.address&&
        memory.read<Addr>(viewTarget.plane.address+viewTarget.controllerOffset)==viewTarget.controller.address&&
        memory.read<Addr>(viewTarget.controller.address+0x370)==viewTarget.plane.address&&
        memory.read<Addr>(viewTarget.controller.address+0x400)==viewTarget.manager.address&&
        memory.read<Addr>(viewTarget.plane.address+0x160)==viewTarget.root.address&&
        memory.read<Addr>(viewTarget.plane.address+0x1250)==viewTarget.view.address;
}
bool viewTargetLive() {
    return viewTarget.allowed&&viewTarget.enabled&&GetCurrentThreadId()==viewTarget.thread&&
        GetTickCount64()-viewTarget.heartbeat<=500&&targetGazeFocused();
}
bool viewTargetOrdinary() {
    if(read<unsigned char>(viewTarget.view.address+0x350)>2||cameraHoldNativeSpecial())return false;
    if(read<unsigned char>(viewTarget.view.address+0x350)==1) {
        const auto input=read<Addr>(viewTarget.view.address+0x328);
        if(!input)return false;
        continuousIdentity(input);
        if(read<unsigned char>(input+0x370)||read<Addr>(input+0x378))return false;
    }
    return read<unsigned char>(viewTarget.plane.address+0xe12)!=0;
}
void viewTargetPose(ViewTargetVector& position,ViewTargetVector& rotation) {
    position=read<ViewTargetVector>(viewTarget.manager.address+0x400);
    rotation=read<ViewTargetVector>(viewTarget.manager.address+0x40c);
    require(viewTargetFinite(position)&&viewTargetFinite(rotation),"视角选目标的相机数据无效");
}
void viewTargetReference(void* plane,bool camera,ViewTargetVector* position,ViewTargetVector* rotation) {
    originalViewTargetReference(plane,camera,position,rotation);
    if(viewTargetFrame.active&&reinterpret_cast<Addr>(plane)==viewTarget.plane.address) {
        // 仅NEXT调用期间替换输出，不改缓存视点、兵装属性或原生机头姿态。
        *position=viewTargetFrame.position;*rotation=viewTargetFrame.rotation;
    }
}
struct ViewTargetRanked {Addr address{};double cosine{},distance{};int order{};};
struct ViewTargetScratch {std::vector<Addr> original,reordered;std::vector<ViewTargetRanked> ranked;};
thread_local std::array<ViewTargetScratch,2> viewTargetScratch; // 只复用容量，内容每次重新读取，不缓存跨帧目标身份。
// 原生0x4c5e00只读取这两张候选表。保留引擎分配与数组头，只临时重排指针，退出立即归还。
struct ViewTargetOrder {
    Addr field{};
    ViewTargetArray array{};
    ContinuousIdentity owner;
    std::vector<Addr>& original;
    bool changed{};
    ViewTargetOrder(int slot,Addr address,ViewTargetVector position,ViewTargetVector forward):field(address),owner(viewTarget.selection),original(viewTargetScratch[slot].original) {
        array=read<ViewTargetArray>(field);
        require(array.count>=0&&array.count<=array.capacity&&array.capacity<=4096&&(!array.count||array.data),"视角选目标的候选表无效");
        if(!array.count)return;
        checkMemory(array.data,array.count*sizeof(Addr));
        original.resize(array.count);std::memcpy(original.data(),reinterpret_cast<void*>(array.data),array.count*sizeof(Addr));
        auto& ranked=viewTargetScratch[slot].ranked;ranked.clear();ranked.reserve(array.count);
        {const ContinuousReadBatch memory;
        for(int i=0;i<array.count;++i) {
            ViewTargetRanked item{original[i],-2.,1.e300,i};
            try {
                if(item.address) {
                    memory.identity(item.address);
                    const auto root=memory.read<Addr>(item.address+0x160);
                    if(root) {
                        memory.identity(root);
                        const auto p=memory.read<ViewTargetVector>(root+0x190);
                        require(viewTargetFinite(p),"视角选目标的目标位置无效");
                        const double x=static_cast<double>(p.x)-position.x,y=static_cast<double>(p.y)-position.y,z=static_cast<double>(p.z)-position.z;
                        item.distance=x*x+y*y+z*z;
                        if(item.distance>1.e-8)item.cosine=(x*forward.x+y*forward.y+z*forward.z)/std::sqrt(item.distance);
                    }
                }
            }catch(...) {} // 失效目标放末尾，资格仍由原生弱身份/存活条件判定，不解引用旧目标。
            ranked.push_back(item);
        }
        }
        const auto before=[](const ViewTargetRanked& a,const ViewTargetRanked& b){
            if(a.cosine!=b.cosine)return a.cosine>b.cosine;
            if(a.distance!=b.distance)return a.distance<b.distance;
            return a.order<b.order;
        };
        // 普通原生分支只检查前4项，无需为剩余候选做全排序；原生相机兵装仍排完整表。
        if(!read<unsigned char>(viewTarget.selection.address+0x45a)&&array.count>4)
            std::partial_sort(ranked.begin(),ranked.begin()+4,ranked.end(),before);
        else std::sort(ranked.begin(),ranked.end(),before);
        auto& reordered=viewTargetScratch[slot].reordered;reordered.clear();reordered.reserve(array.count);
        for(const auto& item:ranked)reordered.push_back(item.address);
        // 全部准备成功后一次提交，构造失败不留下部分重排。
        changed=true;std::memcpy(reinterpret_cast<void*>(array.data),reordered.data(),array.count*sizeof(Addr));
    }
    ViewTargetOrder(const ViewTargetOrder&)=delete;
    ~ViewTargetOrder() {
        if(!changed)return;
        try {
            if(!continuousAlive(owner))return;
            const auto current=read<ViewTargetArray>(field);
            require(current.data==array.data&&current.count==array.count&&current.capacity==array.capacity,"视角选目标的候选表在调用期间改变");
            checkMemory(array.data,array.count*sizeof(Addr));
            std::memcpy(reinterpret_cast<void*>(array.data),original.data(),array.count*sizeof(Addr));
        }catch(const std::exception& error){viewTarget.allowed=false;viewTarget.error=error.what();}
    }
};
void viewTargetNext(void* object) {
    if(!viewTargetLive()||viewTargetFrame.active||reinterpret_cast<Addr>(object)!=viewTarget.selection.address){originalViewTargetNext(object);return;}
    bool called=false;
    try {
        require(viewTargetIdentities(),"视角选目标的玩家身份已变化");
        if(!viewTargetOrdinary()){called=true;originalViewTargetNext(object);return;}
        ViewTargetVector position,rotation;viewTargetPose(position,rotation);
        const auto forward=viewTargetForward(rotation);
        ViewTargetOrder regular(0,viewTarget.selection.address+0x3b0,position,forward);
        ViewTargetOrder special(1,viewTarget.selection.address+0x400,position,forward);
        const ViewTargetFrameScope frame({true,position,rotation});
        called=true;originalViewTargetNext(object);
    }catch(const std::exception& error) {
        viewTarget.allowed=false;viewTarget.error=error.what();
        if(!called)originalViewTargetNext(object); // 原生每次只执行一次，准备失败时直接归还。
    }
}
void installViewTarget() {
    static bool installed=false;if(installed)return;
    verifyVersion();const auto base=continuousBase();
    // 采集来源：target-selection-research.json；不接管已被鼠标飞控Hook的释放入口。
    constexpr unsigned char next[]={0x48,0x8b,0xc4,0x48,0x89,0x58,0x18,0x55,0x56,0x57,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57};
    constexpr unsigned char reference[]={0x48,0x89,0x5c,0x24,0x08,0x57,0x48,0x83,0xec,0x40,0x49,0x8b,0xf9,0x49,0x8b,0xd8};
    const auto verify=[base](Addr offset,const auto& bytes){checkMemory(base+offset,sizeof(bytes));
        require(!std::memcmp(reinterpret_cast<void*>(base+offset),bytes,sizeof(bytes)),"视角选目标的原生入口不匹配");};
    verify(0x4c5e00,next);verify(0x81bae0,reference);
    // 候选表的只读使用结论依赖完整函数体；除入口外核对此前采集原件的FNV-1a指纹。
    const auto fingerprint=[base](Addr offset,size_t size,uint64_t expected) {
        checkMemory(base+offset,size);uint64_t value=14695981039346656037ull;
        const auto* bytes=reinterpret_cast<const unsigned char*>(base+offset);
        for(size_t i=0;i<size;++i)value=(value^bytes[i])*1099511628211ull;
        require(value==expected,"视角选目标的原生入口不匹配");
    };
    fingerprint(0x4c5e00,2674,0x5f7cc9639f287f48ull);
    fingerprint(0x81bae0,309,0xb5fe8309a209f5fbull);
    const auto status=MH_Initialize();require(status==MH_OK||status==MH_ERROR_ALREADY_INITIALIZED,"视角选目标钩子初始化失败");
    const Addr entries[]={base+0x4c5e00,base+0x81bae0};
    void* hooks[]={reinterpret_cast<void*>(&viewTargetNext),reinterpret_cast<void*>(&viewTargetReference)};
    void** originals[]={reinterpret_cast<void**>(&originalViewTargetNext),reinterpret_cast<void**>(&originalViewTargetReference)};
    int created=0;
    try {
        for(int i=0;i<2;++i){require(MH_CreateHook(reinterpret_cast<void*>(entries[i]),hooks[i],originals[i])==MH_OK,"视角选目标钩子创建失败");++created;}
        for(const auto entry:entries)require(MH_QueueEnableHook(reinterpret_cast<void*>(entry))==MH_OK,"视角选目标钩子排队失败");
        require(MH_ApplyQueued()==MH_OK,"视角选目标钩子启用失败");installed=true;
    }catch(...) {for(int i=0;i<created;++i){MH_DisableHook(reinterpret_cast<void*>(entries[i]));MH_RemoveHook(reinterpret_cast<void*>(entries[i]));}throw;}
}
bool bindViewTarget(Addr plane) {
    viewTarget.clear();viewTarget.plane=continuousIdentity(plane);
    require(mountClassIs(plane,"PlayerPlane","/Script/Nimbus"),"视角选目标需要当前玩家飞机");
    viewTarget.controllerOffset=mountProperty(plane,"Controller","ObjectProperty",8);
    const auto selection=read<Addr>(plane+0x1310),controller=read<Addr>(plane+viewTarget.controllerOffset);
    if(!selection||!controller||!read<Addr>(plane+0x160)||!read<Addr>(plane+0x1250)){viewTarget.clear();return false;}
    continuousIdentity(controller);
    require(mountClassIs(controller,"PlayerController","/Script/Engine"),"视角选目标的玩家身份已变化");
    if(!read<Addr>(controller+0x400)||read<Addr>(controller+0x370)!=plane){viewTarget.clear();return false;}
    continuousIdentity(selection);
    if(!read<Addr>(selection+0x378)){viewTarget.clear();return false;}
    viewTarget.selection=continuousIdentity(read<Addr>(plane+0x1310));
    viewTarget.controller=continuousIdentity(read<Addr>(plane+viewTarget.controllerOffset));
    viewTarget.manager=continuousIdentity(read<Addr>(viewTarget.controller.address+0x400));
    viewTarget.root=continuousIdentity(read<Addr>(plane+0x160));
    viewTarget.view=continuousIdentity(read<Addr>(plane+0x1250));
    require(mountClassIs(viewTarget.selection.address,"TargetSelectionComponent","/Script/Nimbus")&&
        mountClassIs(viewTarget.manager.address,"PlayerCameraManager","/Script/Engine")&&
        mountClassIs(viewTarget.root.address,"SceneComponent","/Script/Engine")&&
        mountClassIs(viewTarget.view.address,"CameraViewComponent","/Script/Nimbus")&&
        mountProperty(viewTarget.controller.address,"Pawn","ObjectProperty",8)==0x370&&
        mountProperty(viewTarget.controller.address,"PlayerCameraManager","ObjectProperty",8)==0x400&&
        mountProperty(viewTarget.manager.address,"CameraCache","StructProperty",1504)==0x3f0&&
        mountProperty(viewTarget.selection.address,"TargetCandidates","ArrayProperty",16)==0x3b0&&
        mountProperty(viewTarget.selection.address,"TGTPTargetCandidates","ArrayProperty",16)==0x400&&
        mountProperty(viewTarget.selection.address,"NextTarget","ObjectProperty",8)==0x258&&viewTargetIdentities(),"视角选目标的玩家身份已变化");
    viewTarget.thread=GetCurrentThreadId();
    return true;
}
int viewTargetLua(void* luaState) {
    const dualsp::LuaApi* api=nullptr;
    try {
        static const dualsp::LuaApi bound;api=&bound;
        require(api->top(luaState)==5,"视角选目标参数数量无效");
        const auto plane=static_cast<Addr>(api->readInteger(luaState,1));
        const auto allowed=api->readInteger(luaState,2),enabled=api->readInteger(luaState,3),dot=api->readInteger(luaState,4);
        const auto pad=api->readInteger(luaState,5);
        require((allowed==0||allowed==1)&&(enabled==0||enabled==1)&&(dot==0||dot==1)&&(pad==0||pad==1),"视角选目标参数无效");
        bool available=false,visible=false,padToggle=false;
        if(!plane||!allowed||!targetGazeFocused()){viewTarget.clear();viewTarget.error.clear();}
        else {
            require(viewTarget.error.empty(),viewTarget.error.c_str());verifyVersion();
            if((viewTarget.plane.address!=plane||!viewTargetIdentities())&&!bindViewTarget(plane)) {
                api->boolean(luaState,true);api->boolean(luaState,false);api->boolean(luaState,false);api->string(luaState,"waiting_input",13);api->boolean(luaState,false);return 5;
            }
            require(GetCurrentThreadId()==viewTarget.thread,"视角选目标不在玩家游戏线程");
            available=viewTargetOrdinary();
            if(enabled&&available)installViewTarget();
            viewTarget.allowed=available;viewTarget.enabled=enabled!=0;viewTarget.heartbeat=GetTickCount64();
            if(pad&&available) {
                installTargetGaze(); // 共用现有XInput观察入口，保持原生按键返回值。
                const auto now=GetTickCount64();
                if(!viewTargetPad.allowed||viewTargetPad.plane!=plane||now-viewTargetPad.heartbeat>500)viewTargetPad.clear();
                viewTargetPad.plane=plane;viewTargetPad.heartbeat=now;viewTargetPad.allowed=true;viewTargetPad.thread=GetCurrentThreadId();
                XINPUT_STATE state{};const auto result=originalTargetGazePad(0,&state);viewTargetObservePad(result,&state);
                padToggle=viewTargetPad.pending;viewTargetPad.pending=false;
            }else viewTargetPad.clear();
            if(dot&&available) {
                ViewTargetVector position,rotation;viewTargetPose(position,rotation);const auto forward=viewTargetForward(rotation);
                const auto q=read<std::array<float,4>>(viewTarget.root.address+0x180);
                const double x=q[0],y=q[1],z=q[2],w=q[3],length=x*x+y*y+z*z+w*w;
                require(std::isfinite(length)&&length>.9&&length<1.1,"视角选目标的机头方向无效");
                const double cosine=((1.-2.*(y*y+z*z)/length)*forward.x+2.*(x*y+w*z)/length*forward.y+2.*(x*z-w*y)/length*forward.z);
                visible=cosine<std::cos(15.*3.14159265358979323846/180.); // 15度边界也隐藏，不用归一化环视轴代替实际画面方向。
            }
        }
        api->boolean(luaState,true);api->boolean(luaState,available);api->boolean(luaState,visible);api->string(luaState,"",0);api->boolean(luaState,padToggle);return 5;
    }catch(const std::exception& error) {
        viewTarget.allowed=false;viewTarget.error=error.what();viewTargetPad.clear();
        if(api){api->boolean(luaState,false);api->boolean(luaState,false);api->boolean(luaState,false);api->string(luaState,error.what(),std::strlen(error.what()));api->boolean(luaState,false);return 5;}
        return 0;
    }
}
