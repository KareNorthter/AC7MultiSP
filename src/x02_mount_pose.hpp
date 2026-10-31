// X-02S只租用SWP3原序列末姿态，不修改整机spwc或武器取位。
#pragma once

struct X02MountPose {
    using R=MountAnimationReflection;
    using Field=R::Field;
    using Function=R::Function;
    R api;
    MountIdentity component,mesh,animationClass,sequence,modeEnum;
    Field meshField,classField,modeField,instanceField;
    R::BoolField noSkeleton,pause;
    Function setMode,getMode,play,length,setPosition,setRate,setClass,getPosition,getRate;
    Field modeInput,modeResult,playAsset,lengthResult,positionInput,rateInput,classInput,positionResult,rateResult;
    R::BoolField looping,fireNotifies;
    unsigned blueprint{},single{},nativeMode{};
    bool prepared{},owned{},nativeNoSkeleton{},nativePause{},nativeTick{};
    float end{};

    static std::array<unsigned char,256> parameters() {return {};}
    void current() {
        R::alive(component);R::alive(mesh);R::alive(animationClass);R::alive(sequence);
        require(read<Addr>(component.address+meshField.offset)==mesh.address &&
            read<Addr>(component.address+classField.offset)==animationClass.address,
            "X-02S姿态：原挂架网格或动画类改变");
    }
    unsigned mode() {
        auto p=parameters();api.invoke(component,getMode,p.data());return p[modeResult.offset];
    }
    void animationMode(unsigned value) {
        auto p=parameters();p[modeInput.offset]=static_cast<unsigned char>(value);
        api.invoke(component,setMode,p.data());require(mode()==value,"X-02S姿态：动画模式回读不同");
    }
    void bit(R::BoolField field,bool value) {
        R::alive(field.field.identity);
        const Addr address=component.address+field.field.offset;
        require(R::writable(address,1),"X-02S姿态：动画开关不可写");
        const auto prior=read<unsigned char>(address);
        write<unsigned char>(address,static_cast<unsigned char>((prior&~field.fieldMask)|(value?field.byteMask:0)));
        require(((read<unsigned char>(address)&field.fieldMask)!=0)==value,"X-02S姿态：动画开关回读不同");
    }
    // 结构大小也由当前反射取得，不硬编码AnimNode或FlightEngineMainSequence偏移。
    Field structure(Addr owner,const char* name,int limit,const char* type) {
        const Addr field=R::find(owner,name);
        require(field && mountName(read<Addr>(field+0x10))=="StructProperty","X-02S姿态：缺少原动画结构");
        const Addr schema=R::refValue<Addr>(api.structRef,field);
        R::named(schema,type,"/Script/Nimbus");
        return api.field(owner,name,"StructProperty",api.typeSize(schema),limit,true);
    }
    void prepare(MountIdentity rack,MountIdentity originalMesh) {
        if(prepared) {current();return;}
        api.bindSdk();component=rack;mesh=originalMesh;
        require(mountClassIs(component.address,"SkeletalMeshComponent","/Script/Engine"),"X-02S姿态：不是骨架组件");
        const Addr cls=read<Addr>(component.address+0x10);const int size=api.typeSize(cls);
        meshField=api.field(cls,"SkeletalMesh","ObjectProperty",8,size,true);
        classField=api.field(cls,"AnimClass","ClassProperty",8,size,true);
        modeField=api.field(cls,"AnimationMode","ByteProperty",1,size,true);
        animationClass=mountIdentity(read<Addr>(component.address+classField.offset));
        R::named(animationClass.address,"x02s_SWP3_AnimBP_C","/Game/Blueprint/Player/Body/x02s_SWP3_AnimBP");
        noSkeleton=api.boolean(cls,"bNoSkeletonUpdate",size,true);pause=api.boolean(cls,"bPauseAnims",size,true);
        setMode=api.function(component.address,"SetAnimationMode","SkeletalMeshComponent",1);
        modeInput=api.field(setMode.identity.address,"InAnimationMode","ByteProperty",1,setMode.size);
        getMode=api.function(component.address,"GetAnimationMode","SkeletalMeshComponent",1);
        modeResult=api.field(getMode.identity.address,"ReturnValue","ByteProperty",1,getMode.size);
        modeEnum=mountIdentity(R::refValue<Addr>(api.enumRef,modeInput.identity.address));
        require(R::refValue<Addr>(api.enumRef,modeField.identity.address)==modeEnum.address &&
            R::refValue<Addr>(api.enumRef,modeResult.identity.address)==modeEnum.address,"X-02S姿态：动画枚举不一致");
        blueprint=api.enumValue(modeEnum.address,"EAnimationMode","AnimationBlueprint");
        single=api.enumValue(modeEnum.address,"EAnimationMode","AnimationSingleNode");
        nativeMode=mode();require(nativeMode==blueprint,"X-02S姿态：原生没有使用蓝图模式");
        instanceField=api.field(cls,"AnimScriptInstance","ObjectProperty",8,size,true);
        const auto instance=mountIdentity(read<Addr>(component.address+instanceField.offset));R::alive(instance);
        require(read<Addr>(instance.address+0x10)==animationClass.address,"X-02S姿态：原生动画实例与蓝图不同");
        const auto node=structure(animationClass.address,"AnimGraphNode_FlightEngineBlend_4246268146F8E280BC97848C3AC459D9",
            api.typeSize(animationClass.address),"AnimNode_FlightEngineBlend");
        const Addr nodeType=R::refValue<Addr>(api.structRef,node.identity.address);
        const auto config=structure(nodeType,"SequenceConfig",node.size,"FlightEngineMainSequence");
        const Addr configType=R::refValue<Addr>(api.structRef,config.identity.address);
        const auto asset=api.field(configType,"Sequence","ObjectProperty",8,config.size);
        sequence=mountIdentity(read<Addr>(instance.address+node.offset+config.offset+asset.offset));
        R::named(sequence.address,"x02s_SWP3_Anim","/Game/Vehicles/Aircraft/x02s/x02s_SWP3_Anim");
        require(mountClassIs(sequence.address,"AnimSequence","/Script/Engine"),"X-02S姿态：不是原生骨骼序列");
        play=api.function(component.address,"PlayAnimation","SkeletalMeshComponent",2);
        playAsset=api.field(play.identity.address,"NewAnimToPlay","ObjectProperty",8,play.size);
        looping=api.boolean(play.identity.address,"bLooping",play.size);R::separate(playAsset,looping.field);
        length=api.function(sequence.address,"GetPlayLength","AnimSequenceBase",1);
        lengthResult=api.field(length.identity.address,"ReturnValue","FloatProperty",4,length.size);
        auto p=parameters();api.invoke(sequence,length,p.data());std::memcpy(&end,p.data()+lengthResult.offset,4);
        require(std::isfinite(end) && std::abs(end-3.3333332538604736f)<.00001f,"X-02S姿态：原序列长度改变");
        setPosition=api.function(component.address,"SetPosition","SkeletalMeshComponent",2);
        positionInput=api.field(setPosition.identity.address,"InPos","FloatProperty",4,setPosition.size);
        fireNotifies=api.boolean(setPosition.identity.address,"bFireNotifies",setPosition.size);
        R::separate(positionInput,fireNotifies.field);
        setRate=api.function(component.address,"SetPlayRate","SkeletalMeshComponent",1);
        rateInput=api.field(setRate.identity.address,"Rate","FloatProperty",4,setRate.size);
        getPosition=api.function(component.address,"GetPosition","SkeletalMeshComponent",1);
        positionResult=api.field(getPosition.identity.address,"ReturnValue","FloatProperty",4,getPosition.size);
        getRate=api.function(component.address,"GetPlayRate","SkeletalMeshComponent",1);
        rateResult=api.field(getRate.identity.address,"ReturnValue","FloatProperty",4,getRate.size);
        setClass=api.function(component.address,"SetAnimInstanceClass","SkeletalMeshComponent",1);
        classInput=api.field(setClass.identity.address,"NewClass","ClassProperty",8,setClass.size);
        current();prepared=true; // 第一笔修改前，全部接口、资源和参数均已核对。
    }
    void release() {
        if(!owned)return;
        if(!mountAlive(component)) {owned=false;return;}
        current();
        // 每项独立尝试；中途失败仍保存owned，下一次可继续归还。
        std::string errors;
        auto attempt=[&](auto operation) {try {operation();}catch(const std::exception& e) {
            if(!errors.empty())errors+="；";errors+=e.what();}};
        if(owned) {
            attempt([&] {animationMode(nativeMode);});
            attempt([&] {auto p=parameters();std::memcpy(p.data()+classInput.offset,&animationClass.address,8);
                api.invoke(component,setClass,p.data());require(read<Addr>(component.address+classField.offset)==animationClass.address,
                    "X-02S姿态：原蓝图恢复失败");
                const auto instance=mountIdentity(read<Addr>(component.address+instanceField.offset));R::alive(instance);
                require(read<Addr>(instance.address+0x10)==animationClass.address,"X-02S姿态：原蓝图实例未恢复");});
        }
        attempt([&] {bit(noSkeleton,nativeNoSkeleton);});
        attempt([&] {bit(pause,nativePause);});
        attempt([&] {api.setTick(component,nativeTick);});
        require(errors.empty(),errors.c_str());owned=false;
    }
    void captureFlags() {
        nativeNoSkeleton=(read<unsigned char>(component.address+noSkeleton.field.offset)&noSkeleton.fieldMask)!=0;
        nativePause=(read<unsigned char>(component.address+pause.field.offset)&pause.fieldMask)!=0;nativeTick=api.tick(component);
    }
    void enableUpdate() {bit(noSkeleton,false);bit(pause,false);api.setTick(component,true);}
    void maintain() {
        require(prepared,"X-02S姿态：接口尚未绑定");current();
        if(owned) {
            require(read<unsigned char>(component.address+modeField.offset)==single,"X-02S姿态：租约期间动画模式改变");
            return; // 原序列定点保持，不逐帧反射解析或重复播放。
        }
        nativeMode=mode();require(nativeMode==blueprint && read<Addr>(component.address+classField.offset)==animationClass.address,
            "X-02S姿态：原动画控制已改变");
        captureFlags();
        owned=true; // 引擎调用失败也必须归还先前已完成的修改。
        animationMode(single);
        auto p=parameters();std::memcpy(p.data()+playAsset.offset,&sequence.address,8);R::putBool(p.data(),looping,false);
        api.invoke(component,play,p.data());
        p=parameters();std::memcpy(p.data()+positionInput.offset,&end,4);R::putBool(p.data(),fireNotifies,false);
        api.invoke(component,setPosition,p.data());
        p=parameters();const float zero=0;std::memcpy(p.data()+rateInput.offset,&zero,4);api.invoke(component,setRate,p.data());
        enableUpdate();
        require(mode()==single,"X-02S姿态：单节点姿态未生效");
        p=parameters();api.invoke(component,getPosition,p.data());float position{};std::memcpy(&position,p.data()+positionResult.offset,4);
        p=parameters();api.invoke(component,getRate,p.data());float rate{};std::memcpy(&rate,p.data()+rateResult.offset,4);
        require(std::isfinite(position) && std::abs(position-end)<.00001f && rate==0,
            "X-02S姿态：原序列末端或停止速率回读不同");
    }
};
