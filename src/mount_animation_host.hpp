// 在产品反射/Parts适配器之后包含；资源与七组合绑定由模型层提供，不自动启用机体。
#pragma once

struct MountAnimationHost final : ::dualsp::common_bay::Host {
    using Binding=::dualsp::common_bay::Binding;
    using Identity=::dualsp::common_bay::Identity;
    using Aircraft=::dualsp::common_bay::Aircraft;
    using Request=::dualsp::common_bay::RackRequest;
    using Parts=::dualsp::common_bay::PartsSample;
    using Cover=::dualsp::common_bay::CoverState;
    using Description=::dualsp::common_bay::ResourceDescription;
    using Error=::dualsp::common_bay::Error;
    using Mode=::dualsp::common_bay::bone_refresh::Setting;
    MountAnimationProductAccess access;
    ::dualsp::common_bay::Controller controller;
    Aircraft aircraft{};
    Binding binding{};
    Description description;
    struct Rack {
        Identity identity;
        MountAnimationReflection::BoolField noSkeleton,pause;
        MountAnimationReflection::TickReader tick;
        Mode mode;
        bool prepared{};
    };
    std::array<Rack,3> racks{};
    bool qualified{};

    bool onGameThread() const override { return access.onGameThread(); }
    bool rackAlive(Identity id) override { return access.alive(id); }
    bool bindingAlive(const Binding& b) override {
        return qualified && ::dualsp::common_bay::same(binding,b) && access.ownerAlive(b);
    }
    bool describe(const Binding& b,Description& result) override {
        if(!bindingAlive(b))return false;
        result=description;return true;
    }
    bool readParts(const Binding& b,Parts& result) override {
        return bindingAlive(b) && ::dualsp::common_bay::native_parts::read(access,aircraft,b,result);
    }
    bool writeCover(const Binding& b,const Cover& wanted) override {
        if(!bindingAlive(b))return false;
        const auto status=::dualsp::common_bay::native_parts::writeCover(access,aircraft,b,wanted);
        if(status!=::dualsp::common_bay::native_parts::CoverWriteStatus::Applied)
            nativeLog(Json{{"event","ANIMATED_COVER_WRITE_REJECTED"},{"status",static_cast<int>(status)}}.dump());
        return status==::dualsp::common_bay::native_parts::CoverWriteStatus::Applied;
    }
    Rack* findRack(Identity id) {
        for(auto& rack:racks)if(rack.prepared && ::dualsp::common_bay::same(rack.identity,id))return &rack;
        return nullptr;
    }
    bool readRack(Identity id,Request& result) override {
        try {
            auto* rack=findRack(id);
            if(!onGameThread() || !rack || !rackAlive(id))return false;
            MountAnimationReflection::alive(rack->noSkeleton.field.identity);
            MountAnimationReflection::alive(rack->pause.field.identity);
            result.noSkeletonUpdate=(read<unsigned char>(id.address+rack->noSkeleton.field.offset)&rack->noSkeleton.fieldMask)!=0;
            result.paused=(read<unsigned char>(id.address+rack->pause.field.offset)&rack->pause.fieldMask)!=0;
            result.tick=access.reflection.tickFast(rack->tick,binding.generation);
            return true;
        } catch(...) { return false; }
    }
    bool applyRack(Identity id,Request wanted) override {
        try {
            Request before;
            if(!readRack(id,before))return false;
            const auto identity=MountAnimationProductAccess::productIdentity(id);
            if(before.noSkeletonUpdate!=wanted.noSkeletonUpdate)
                access.reflection.writeComponentBool(identity,"bNoSkeletonUpdate",wanted.noSkeletonUpdate);
            if(before.paused!=wanted.paused)
                access.reflection.writeComponentBool(identity,"bPauseAnims",wanted.paused);
            if(before.tick!=wanted.tick)access.reflection.setTick(identity,wanted.tick);
            Request after;
            return readRack(id,after) && ::dualsp::common_bay::same(after,wanted);
        } catch(...) { return false; }
    }
    bool acquirePoseRefresh(Identity id) override {
        try {
            auto* rack=findRack(id);
            unsigned actual{};
            // 本版不写刷新模式：原生已是AlwaysTickPoseAndRefreshBones才接入，变化即失败归还。
            // 因为没有接管该字段，退出也不把旧值写回，避免丢失未知原生直接写入的意图。
            return rack && access.descriptorCurrent(rack->mode) && access.read(rack->mode,actual) &&
                   actual==rack->mode.alwaysValue;
        } catch(...) { return false; }
    }
    bool releasePoseRefresh(Identity) override { return onGameThread(); }

    bool prepare(Aircraft selected,const Binding& owner,Description verified) {
        if(!onGameThread() || qualified || controller.active() || controller.restoring() || access.ownerRegistered)return false;
        aircraft=selected;binding=owner;description=std::move(verified);
        if(!access.registerOwner(binding)) { binding={};return false; }
        try {
            access.reflection.bindSdk();
            Parts sample;
            require(::dualsp::common_bay::native_parts::read(access,aircraft,binding,sample),"动画宿主：原生实例或代码资格不符");
            description.nativeStateAccessQualified=true; // 完整九处代码、owner回指和原生配置刚已实际核对。
            const unsigned needed=description.providerMask|description.presentationMask;
            const unsigned skeletal=description.skeletalMask;
            for(unsigned i=0;i<3;++i)if((needed&skeletal)&(1u<<i)) {
                auto& rack=racks[i];rack.identity=binding.racks[i];
                const auto identity=MountAnimationProductAccess::productIdentity(rack.identity);
                const Addr cls=read<Addr>(identity.address+0x10);
                const int extent=access.reflection.typeSize(cls);
                rack.noSkeleton=access.reflection.boolean(cls,"bNoSkeletonUpdate",extent,true);
                rack.pause=access.reflection.boolean(cls,"bPauseAnims",extent,true);
                rack.tick=access.reflection.bindTickReader(identity,binding.generation);
                require(rack.noSkeleton.field.offset!=rack.pause.field.offset ||
                        !(rack.noSkeleton.fieldMask&rack.pause.fieldMask),"动画宿主：两个布尔位重叠");
                require(access.inspect(rack.identity,"MeshComponentUpdateFlag",rack.mode)==
                        ::dualsp::common_bay::bone_refresh::Lookup::Found,"动画宿主：缺少实际骨骼刷新模式");
                rack.prepared=true;
                require(acquirePoseRefresh(rack.identity),"动画宿主：原生模式不保证隐藏骨骼刷新");
                Request baseline;
                require(readRack(rack.identity,baseline),"动画宿主：原生状态读取失败");
                require(!baseline.paused,"动画宿主：原生正在暂停动画，保留原生流程");
            }
            description.hiddenProviderPoseRefreshQualified=true; // 以上逐个实际读回，未写模式或填默认枚举。
            require(::dualsp::common_bay::qualify(aircraft,description)==Error::None,"动画宿主：资源或七组合资格不符");
            qualified=true;
            const auto status=controller.bind(*this,aircraft,binding,description.planEquippedMask,needed);
            require(status==Error::None,"动画宿主：取得组件更新控制失败");
            return true;
        } catch(const std::exception& error) {
            nativeLog(std::string("ANIMATED_HOST_BIND_REJECTED ")+error.what());
            release();return false;
        }
    }
    bool release() {
        if(!onGameThread())return false;
        const auto status=controller.release(*this);
        if(status!=Error::None)return false; // 保留待归还组件及原生请求，不能被新绑定覆盖。
        access.forgetOwner();qualified=false;binding={};description={};racks={};
        return true;
    }
    bool maintain() {
        if(!controller.active())return false;
        // 未主动强制原生Pause；发现真正的Pause请求先记账再回退，不清除暂停意图。
        for(auto& rack:racks)if(rack.prepared) {
            Request state;
            if(!readRack(rack.identity,state)) { release();return false; }
            if(state.paused) {
                controller.nativeRequest(*this,rack.identity,state,1u);
                release();
                return false;
            }
            if(!acquirePoseRefresh(rack.identity)) { release();return false; }
        }
        return controller.maintain(*this)==Error::None;
    }
};
