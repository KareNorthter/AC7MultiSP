#pragma once
#include <initializer_list>
// 原生自由任务设置与出击请求；地址/ABI依据本机20261024原生代码副本，先核指纹再调用。
namespace {
struct QuickMissionString {wchar_t* data{};int count{},capacity{};};
struct QuickMissionName {uint32_t index{},number{};};
DWORD quickMissionThread{};

void quickMissionSignature(Addr base,Addr rva,std::initializer_list<unsigned char> expected) {
    checkMemory(base+rva,expected.size());
    require(std::memcmp(reinterpret_cast<const void*>(base+rva),expected.begin(),expected.size())==0,
        "快速任务原生入口与当前构建不符");
}
bool quickMissionMenuInstance(Addr widget,Addr instance) {
    // MenuBaseWidget的GI缓存不是UPROPERTY。原生Copyright激活直接读取+0x290并以GI调用3cf2b0。
    // MainMenu和Copyright共用同一菜单基类布局；先核对读取指令、继承身份，再比较缓存指针。
    const auto base=reinterpret_cast<Addr>(GetModuleHandleW(nullptr));
    quickMissionSignature(base,0x656a17,{0x48,0x8b,0x8b,0x90,0x02,0,0,0x48,0x8d,0x91,0x48,0x02,0,0});
    return mountClassIs(widget,"MenuBaseWidget","/Script/Nimbus")&&read<Addr>(widget+0x290)==instance;
}
void quickMissionPreflight() {
    verifyVersion();
    const auto base=reinterpret_cast<Addr>(GetModuleHandleW(nullptr));
    quickMissionSignature(base,0x4f6130,{0x48,0x89,0x4c,0x24,0x08,0x55,0x53,0x56,0x57,0x41,0x57});
    quickMissionSignature(base,0x3d0320,{0x40,0x55,0x53,0x57,0x48,0x8d,0x6c,0x24,0xc1,0x48,0x81,0xec,0xf0,0,0,0});
    quickMissionSignature(base,0x3cfce0,{0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x74,0x24,0x10,0x57,0x48,0x83,0xec,0x20});
    quickMissionSignature(base,0x3cfc40,{0x40,0x53,0x48,0x83,0xec,0x20,0x88,0x91,0x20,0x05,0,0});
    quickMissionSignature(base,0x3cfd90,{0x40,0x53,0x48,0x83,0xec,0x20,0x80,0xb9,0x20,0x05,0,0,0x04});
    quickMissionSignature(base,0xa4ade0,{0x40,0x53,0x48,0x83,0xec,0x30,0x48,0x8b,0xd9});
    quickMissionSignature(base,0x99af20,{0x48,0x85,0xc9,0x74,0x1d,0x4c,0x8b,0x05});
    quickMissionSignature(base,0x1798e90,{0x48,0x8b,0x41,0x30,0x48,0x85,0xc0,0x74,0x08,0x48,0x8b,0x80,0x88,0x02,0,0,0xc3});
    // 原生sortie写入阶段4的指令，以及任务/选机getter同时证明相邻非反射字段布局。
    quickMissionSignature(base,0x6aee3f,{0xc6,0x80,0xf1,0x04,0,0,0x04});
    quickMissionSignature(base,0x9248d5,{0x8b,0x81,0xd8,0x04,0,0});
    quickMissionSignature(base,0x924a05,{0x8b,0x81,0xdc,0x04,0,0});
    quickMissionSignature(base,0x924a25,{0x8b,0x81,0xe0,0x04,0,0});
    require(!quickMissionThread || quickMissionThread==GetCurrentThreadId(),"快速任务必须在同一游戏线程执行");
    require(!mountGameThread.load()||mountGameThread.load()==GetCurrentThreadId(),"快速任务不在绑定的游戏线程");
    quickMissionThread=GetCurrentThreadId();
}
Json quickMissionRows(Addr table) {
    require(mountClassIs(table,"DataTable","/Script/Engine"),"快速任务表类型不符");
    // 菜单中尚未安装挂载钩子；这里只绑定元数据getter，不要求飞行上下文或取得写权限。
    MountAnimationReflection reflection;
    const auto loader=GetModuleHandleW(L"UE4SS.dll");require(loader!=nullptr,"快速任务加载器尚未就绪");
    reflection.extent=reinterpret_cast<MountAnimationReflection::Ref>(GetProcAddress(loader,"?GetPropertiesSize@UStruct@Unreal@RC@@QEBAAEBHXZ"));
    reflection.arrayDim=reinterpret_cast<MountAnimationReflection::Ref>(GetProcAddress(loader,"?GetArrayDim@FProperty@Unreal@RC@@QEBAAEBHXZ"));
    const auto rowType=read<Addr>(table+mountProperty(table,"RowStruct","ObjectProperty",8));
    MountAnimationReflection::named(rowType,"MissionDataTable","/Script/Nimbus");
    const auto extent=reflection.typeSize(rowType);
    const auto id=reflection.field(rowType,"MissionID","IntProperty",4,extent);
    const auto code=reflection.field(rowType,"MissionStringID","StrProperty",16,extent);
    const auto dlc=reflection.field(rowType,"DLCID","StrProperty",16,extent);
    Json result=Json::array();std::set<std::string> seen;
    for(const auto row:tableRows(table)) {
        const auto text=unrealString(row+code.offset);
        const bool normal=text.size()==4 && text.rfind("ms",0)==0 && text[2]>='0' && text[2]<='2' && text[3]>='0' && text[3]<='9';
        const bool extra=text=="ex01" || text=="ex02" || text=="ex03";
        const int number=normal?(text[2]-'0')*10+text[3]-'0':0;
        if(!(normal && number>=1 && number<=20) && !extra)continue;
        require(seen.insert(text).second,"快速任务表包含重复任务");
        const int mission=read<int>(row+id.offset);require(mission>0,"快速任务编号无效");
        result.push_back({{"mission_id",mission},{"code",text},{"dlc_id",unrealString(row+dlc.offset)},
            {"label",extra?"SP Mission "+std::to_string(text[3]-'0'):"M"+std::to_string(number)},
            {"fixed_sp",text=="ms13"}});
    }
    require(result.size()==23,"快速任务表未提供完整23关");
    std::sort(result.begin(),result.end(),[](const Json& left,const Json& right){return left.at("mission_id").get<int>()<right.at("mission_id").get<int>();});
    return result;
}
std::wstring quickMissionMap(int mission) {
    QuickMissionString text;
    const auto base=reinterpret_cast<Addr>(GetModuleHandleW(nullptr));
    using Resolve=QuickMissionString*(*)(QuickMissionString*,int);
    reinterpret_cast<Resolve>(base+0x4f6130)(&text,mission);
    std::wstring result;
    try {
        require(text.count>1 && text.count<=1024 && text.capacity>=text.count && text.data,"原生任务地图路径无效");
        checkMemory(reinterpret_cast<Addr>(text.data),static_cast<size_t>(text.count)*sizeof(wchar_t));
        require(text.data[text.count-1]==0,"原生任务地图路径缺少结尾");
        result.assign(text.data,text.count-1);
        require(result.rfind(L"/Game/Maps/Mission/",0)==0 && result.find(L'?')==std::wstring::npos,"原生任务地图不属于单人任务");
    }catch(...) {
        if(text.data)reinterpret_cast<void(*)(void*)>(base+0x99af20)(text.data);
        throw;
    }
    if(text.data)reinterpret_cast<void(*)(void*)>(base+0x99af20)(text.data);
    return result;
}
}

// CopyrightWidget承载全部启动Logo。只推进视觉序列至原生结束等待，仍由原生tick等候加载并交接。
extern "C" __declspec(dllexport) int dualsp_startup_logos_v1(void* luaState) {
    const dualsp::LuaApi* api=nullptr;
    try {
        static const dualsp::LuaApi bound;api=&bound;
        require(api->top(luaState)==3 && runtimeEnabled,"片头跳过需要单人许可和完整参数");
        require(!quickMissionThread||quickMissionThread==GetCurrentThreadId(),"片头跳过不在绑定的游戏线程");
        require(!mountGameThread.load()||mountGameThread.load()==GetCurrentThreadId(),"片头跳过不在挂载游戏线程");
        quickMissionThread=GetCurrentThreadId();
        verifyVersion();const auto base=reinterpret_cast<Addr>(GetModuleHandleW(nullptr));
        quickMissionSignature(base,0x656fc0,{0x40,0x53,0x48,0x81,0xec,0x90,0,0,0});
        quickMissionSignature(base,0x65724d,{0x8b,0x83,0x80,0x02,0,0,0x89,0x83,0xcc,0x04,0,0,
            0x66,0xc7,0x83,0xc9,0x04,0,0,0x01,0});
        quickMissionSignature(base,0x657096,{0x80,0xbb,0xc9,0x04,0,0,0,0x74,0x21});
        const Addr instance=api->readInteger(luaState,1),world=api->readInteger(luaState,2),widget=api->readInteger(luaState,3);
        continuousIdentity(instance);continuousIdentity(world);continuousIdentity(widget);
        require(mountClassIs(instance,"NimbusGameInstance","/Script/Nimbus")&&mountClassIs(world,"World","/Script/Engine")&&
            mountClassIs(widget,"CopyrightWidget","/Script/Nimbus"),"片头跳过对象身份不符");
        require(mountName(read<Addr>(world+0x20))=="/Game/UI/Menu/Boot"&&mountName(world)=="Boot","片头跳过只用于启动地图");
        using World=Addr(*)(Addr);
        quickMissionSignature(base,0x1798e90,{0x48,0x8b,0x41,0x30,0x48,0x85,0xc0,0x74,0x08,0x48,0x8b,0x80,0x88,0x02,0,0,0xc3});
        require(read<Addr>(read<Addr>(instance)+0x138)==base+0x1798e90&&
            reinterpret_cast<World>(base+0x1798e90)(instance)==world&&
            quickMissionMenuInstance(widget,instance)&&
            !read<Addr>(world+mountProperty(world,"NetDriver","ObjectProperty",8)),"片头跳过世界已变化");
        require(read<Addr>(read<Addr>(widget)+0x420)==base+0x656fc0,"片头页面tick不属于已确认构建");
        if(!read<unsigned char>(widget+0x3b0)||read<unsigned char>(widget+0x3c5)){api->boolean(luaState,false);return 1;}
        // 与序列播完时的原生赋值一致；不直接调用换图，不跳过加载器等待。
        write<float>(widget+0x4cc,read<float>(widget+0x280));
        write<unsigned short>(widget+0x4c9,1);
        nativeLog("STARTUP_LOGOS_SKIPPED_NATIVE_WAIT");api->boolean(luaState,true);return 1;
    }catch(const std::exception& error) {
        nativeLog(std::string("STARTUP_LOGOS_ERROR ")+error.what());
        if(api){api->boolean(luaState,false);api->string(luaState,error.what(),std::strlen(error.what()));return 2;}return 0;
    }
}

extern "C" __declspec(dllexport) int dualsp_quick_mission_v1(void* luaState) {
    const dualsp::LuaApi* api=nullptr;
    try {
        static const dualsp::LuaApi bound;api=&bound;
        require(api->top(luaState)==9 && runtimeEnabled,"快速任务需要已启用的单人入口与完整参数");
        quickMissionPreflight();
        const int op=static_cast<int>(api->readInteger(luaState,1));
        const Addr instance=api->readInteger(luaState,2),world=api->readInteger(luaState,3),table=api->readInteger(luaState,4);
        const Addr menu=api->readInteger(luaState,5);
        const auto identity=continuousIdentity(instance),worldIdentity=continuousIdentity(world),tableIdentity=continuousIdentity(table);
        require(mountClassIs(instance,"NimbusGameInstance","/Script/Nimbus") && !(read<unsigned>(instance+8)&0x30u),"快速任务GI身份不符");
        require(mountClassIs(world,"World","/Script/Engine"),"快速任务世界身份不符");
        const auto package=mountName(read<Addr>(world+0x20));
        require(package=="/Game/UI/Menu/Menu" && mountName(world)=="Menu","快速任务只允许主菜单启动");
        using World=Addr(*)(Addr);
        const auto module=reinterpret_cast<Addr>(GetModuleHandleW(nullptr));
        require(read<Addr>(read<Addr>(instance)+0x138)==module+0x1798e90&&
            reinterpret_cast<World>(module+0x1798e90)(instance)==world,"快速任务GI世界已变化");
        require(!read<Addr>(world+mountProperty(world,"NetDriver","ObjectProperty",8)),"快速任务不允许网络世界");
        const auto info=read<Addr>(instance+mountProperty(instance,"GameInformation","ObjectProperty",8));
        continuousIdentity(info);
        require(read<Addr>(info+mountProperty(info,"MissionDataTable","ObjectProperty",8))==table,"快速任务表不属于当前GI");
        auto tasks=quickMissionRows(table);
        Json result={{"protocol",1},{"tasks",tasks}};
        if(op==1) {
            const int mission=static_cast<int>(api->readInteger(luaState,6)),plane=static_cast<int>(api->readInteger(luaState,7));
            const int swp=static_cast<int>(api->readInteger(luaState,8)),difficulty=static_cast<int>(api->readInteger(luaState,9));
            require(plane>=0 && swp>=0 && swp<3 && difficulty>=0 && difficulty<4,"快速任务选机或难度无效");
            require(std::any_of(tasks.begin(),tasks.end(),[&](const Json& row){return row.at("mission_id")==mission;}),"目标不属于23个测试任务");
            const auto menuIdentity=continuousIdentity(menu);
            require(mountClassIs(menu,"MainMenuWidget","/Script/Nimbus") && !(read<unsigned>(menu+8)&0x30u) &&
                quickMissionMenuInstance(menu,instance),"快速任务主菜单身份已变化");
            require(read<int>(instance+0x4dc)==plane && read<int>(instance+0x4e0)==swp,"快速任务机体选择已变化");
            const auto map=quickMissionMap(mission);
            const auto base=reinterpret_cast<Addr>(GetModuleHandleW(nullptr));QuickMissionName mapName;
            using MakeName=QuickMissionName*(*)(QuickMissionName*,const wchar_t*,int);
            reinterpret_cast<MakeName>(base+0xa4ade0)(&mapName,map.c_str(),1);
            require(mapName.index!=0,"原生任务地图名称无效");
            require(continuousAlive(identity) && continuousAlive(worldIdentity) && continuousAlive(tableIdentity) && continuousAlive(menuIdentity),
                "快速任务准备期间对象身份已变化");
            const auto oldMode=read<unsigned char>(instance+0x522),oldDifficulty=read<unsigned char>(instance+0x520);
            const bool oldCasual=read<unsigned char>(instance+0x521)!=0;
            const auto oldStage=read<unsigned char>(instance+0x4f1);const int oldMission=read<int>(instance+0x4d8);
            using Mode=void(*)(Addr,unsigned char);using Difficulty=void(*)(Addr,unsigned char,bool);using Mission=void(*)(Addr,int);
            // CampaignMain自由任务选择走模式2，难度页设置520/521，Sortie激活使用阶段4。
            reinterpret_cast<Mode>(base+0x3cfce0)(instance,2);
            reinterpret_cast<Difficulty>(base+0x3cfc40)(instance,static_cast<unsigned char>(difficulty),false);
            reinterpret_cast<Mission>(base+0x3cfd90)(instance,mission);
            write<unsigned char>(instance+0x4f1,4);
            // 与原生Sortie最终请求一致：目标地图、使用过渡世界、空回调、允许加载。
            using Travel=int(*)(Addr,const QuickMissionName*,bool,void*,bool);
            const int request=reinterpret_cast<Travel>(base+0x3d0320)(instance,&mapName,true,nullptr,true);
            if(request<0) {
                write<unsigned char>(instance+0x4f1,oldStage);
                reinterpret_cast<Mode>(base+0x3cfce0)(instance,oldMode);
                reinterpret_cast<Difficulty>(base+0x3cfc40)(instance,oldDifficulty,oldCasual);
                write<int>(instance+0x4d8,oldMission); // 难度4的setter会再换算自由飞行ID；回滚保留原始编号。
                throw std::runtime_error("原生加载器拒绝快速任务请求");
            }
            result={{"protocol",1},{"accepted",true},{"native_request",request}};
            nativeLog("QUICK_MISSION_NATIVE_ACCEPTED mission="+std::to_string(mission)+" request="+std::to_string(request));
        } else require(op==0,"快速任务操作无效");
        const auto text=result.dump();api->boolean(luaState,true);api->string(luaState,text.data(),text.size());return 2;
    }catch(const std::exception& error) {
        nativeLog(std::string("QUICK_MISSION_NATIVE_ERROR ")+error.what());
        if(api){api->boolean(luaState,false);api->string(luaState,error.what(),std::strlen(error.what()));return 2;}return 0;
    }
}
