// 用加载器公开的ProcessEvent接口替换弹体网格；不直接写渲染资源指针。
#pragma once

bool mountRequireRackCollision(Addr component,Addr original,Addr replacement);
std::vector<unsigned char> mountPreservedCollisionSettings(Addr component);

bool mountMeshIdentityEqual(const MountIdentity& a,const MountIdentity& b) {
    return a.address==b.address && a.index==b.index && a.serial==b.serial;
}
// 接管中的组件再次绑定时，仍按首次记录的原版资源验证。
Addr mountBaselineMesh(const MountIdentity& component,Addr current) {
    const auto from=[&](const FixedMountState& state)->Addr {
        for(const auto& item:state.meshOverrides)if(mountMeshIdentityEqual(item.component,component)) {
            require(mountAlive(item.original) && mountAlive(item.replacement),"弹体换位：原网格或替换网格失效");
            require(current==item.original.address || current==item.replacement.address,"弹体换位：组件网格被其他逻辑改变");
            return item.original.address;
        }
        return 0;
    };
    const Addr preview=from(mountPreview.layout);
    const Addr task=from(fixedMount);
    require(!preview || !task || preview==task,"弹体换位：任务与预览原网格不一致");
    return preview?preview:task?task:current;
}
struct MountMeshSetter {
    using Process=void(*)(void*,void*,void*);
    using Size=const unsigned short*(*)(const void*);
    using Count=const unsigned char*(*)(const void*);
    using Available=bool(*)();
    Process process{};
    Size size{};
    Count count{};
    Available available{};
    MountIdentity function;
    int parameterSize{},meshOffset{},resultOffset{};
};
MountMeshSetter mountMeshSetter;
bool mountMeshChanging=false; // 原生重建渲染状态时的同步回调不能再次替换网格。

void mountBindMeshSetter(Addr component) {
    if(mountMeshSetter.function.address) {
        require(mountAlive(mountMeshSetter.function),"弹体换位：SetStaticMesh接口失效");
        return;
    }
    const auto module=GetModuleHandleW(L"UE4SS.dll");
    require(module!=nullptr,"弹体换位：加载器未就绪");
    MountMeshSetter next;
    next.process=reinterpret_cast<MountMeshSetter::Process>(GetProcAddress(module,"?ProcessEvent@UObject@Unreal@RC@@QEAAXPEAVUFunction@23@PEAX@Z"));
    next.size=reinterpret_cast<MountMeshSetter::Size>(GetProcAddress(module,"?GetParmsSize@UFunction@Unreal@RC@@QEBAAEBGXZ"));
    next.count=reinterpret_cast<MountMeshSetter::Count>(GetProcAddress(module,"?GetNumParms@UFunction@Unreal@RC@@QEBAAEBEXZ"));
    next.available=reinterpret_cast<MountMeshSetter::Available>(GetProcAddress(module,"?IsProcessEventAvailable@UE4SSRuntime@RC@@SA_NXZ"));
    require(next.process && next.size && next.count && next.available,"弹体换位：加载器缺少公开反射接口");
    require(next.available(),"弹体换位：ProcessEvent尚不可用");
    Addr function=0,cls=read<Addr>(component+0x10);
    for(int depth=0;cls && depth<32 && !function;++depth,cls=read<Addr>(cls+0x30)) {
        mountIdentity(cls);
        Addr field=read<Addr>(cls+0x38);
        for(int fields=0;field && fields<1024;++fields,field=read<Addr>(field+0x28)) {
            mountIdentity(field);
            if(mountName(field)=="SetStaticMesh" && mountName(read<Addr>(field+0x10))=="Function") { function=field;break; }
        }
        require(!field || function,"弹体换位：反射字段链过长");
    }
    require(function && mountName(read<Addr>(function+0x20))=="StaticMeshComponent" &&
        mountName(read<Addr>(read<Addr>(function+0x20)+0x20))=="/Script/Engine","弹体换位：SetStaticMesh所属类不符");
    next.function=mountIdentity(function);
    next.parameterSize=read<unsigned short>(reinterpret_cast<Addr>(next.size(reinterpret_cast<void*>(function))));
    require(read<unsigned char>(reinterpret_cast<Addr>(next.count(reinterpret_cast<void*>(function))))==2 && next.parameterSize>0 && next.parameterSize<=64,
        "弹体换位：SetStaticMesh参数布局不符");
    bool mesh=false,result=false;
    int fields=0;
    Addr field=read<Addr>(function+0x38);
    for(;field && fields<3;++fields,field=read<Addr>(field+0x28)) {
        mountIdentity(field);
        const auto name=mountName(field),kind=mountName(read<Addr>(field+0x10));
        const int bytes=read<int>(field+0x34),offset=read<int>(field+0x44);
        require(offset>=0 && bytes>0 && offset+bytes<=next.parameterSize,"弹体换位：参数越界");
        if(name=="NewMesh" && kind=="ObjectProperty" && bytes==8 && !mesh) { next.meshOffset=offset;mesh=true; }
        else if(name=="ReturnValue" && kind=="BoolProperty" && bytes==1 && !result) { next.resultOffset=offset;result=true; }
        else throw std::runtime_error("弹体换位：SetStaticMesh参数类型改变");
    }
    require(!field && fields==2 && mesh && result &&
        (next.resultOffset<next.meshOffset || next.resultOffset>=next.meshOffset+8),"弹体换位：参数缺失或重叠");
    mountMeshSetter=next;
}
void mountApplyMesh(MountMeshOverride item,bool replacement) {
    if(!mountAlive(item.component))return; // 已销毁组件不再归还或解引用。
    require(mountAlive(item.original) && mountAlive(item.replacement),"弹体换位：资源身份失效");
    const Addr current=read<Addr>(item.component.address+0x728);
    require(current==item.original.address || current==item.replacement.address,"弹体换位：组件已被其他逻辑接管");
    const Addr target=replacement?item.replacement.address:item.original.address;
    if(current==target)return;
    // 稳定网格仍核对三份对象身份及接管权；仅真正替换时再次解码资源类型名。
    require(mountName(read<Addr>(item.original.address+0x10))=="StaticMesh" &&
        mountName(read<Addr>(item.replacement.address+0x10))=="StaticMesh","弹体换位：资源类型不符");
    require(!mountMeshChanging,"弹体换位：正在完成上一项网格替换");
    require(GetCurrentThreadId()==mountGameThread.load(),"弹体换位：只能在绑定的游戏线程执行");
    mountBindMeshSetter(item.component.address);
    require(mountMeshSetter.available(),"弹体换位：ProcessEvent不可用");
    alignas(16) std::array<unsigned char,64> params{};
    std::memcpy(params.data()+mountMeshSetter.meshOffset,&target,sizeof(target));
    mountMeshChanging=true;
    struct ResetChanging { ~ResetChanging() { mountMeshChanging=false; } } resetChanging;
    std::vector<unsigned char> collisionBefore;
    if(item.preserveCollision) {
        mountRequireRackCollision(item.component.address,item.original.address,item.replacement.address);
        collisionBefore=mountPreservedCollisionSettings(item.component.address);
    }
    mountMeshSetter.process(reinterpret_cast<void*>(item.component.address),reinterpret_cast<void*>(mountMeshSetter.function.address),params.data());
    require(mountAlive(item.component) && params[mountMeshSetter.resultOffset]!=0 &&
        read<Addr>(item.component.address+0x728)==target,"弹体换位：原生SetStaticMesh拒绝替换");
    if(item.preserveCollision)require(collisionBefore==mountPreservedCollisionSettings(item.component.address),
        "静态挂架：原生换网格期间碰撞配置变化，已停止接管");
}
// 切换候选或重建模型池时，先归还新布局不再接管的组件；失败保留旧上下文重试。
void mountRestoreUnusedMeshes(const FixedMountState& prior,const FixedMountState& next) {
    std::string errors;
    for(const auto& item:prior.meshOverrides) {
        const bool reused=std::any_of(next.meshOverrides.begin(),next.meshOverrides.end(),[&](const MountMeshOverride& target) {
            return mountMeshIdentityEqual(item.component,target.component) && mountMeshIdentityEqual(item.original,target.original) &&
                mountMeshIdentityEqual(item.replacement,target.replacement);
        });
        if(reused)continue;
        try { mountApplyMesh(item,false); }
        catch(const std::exception& error) { if(!errors.empty())errors+="；";errors+=error.what(); }
    }
    require(errors.empty(),errors.c_str());
}
