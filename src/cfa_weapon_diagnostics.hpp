// CFA双发调查：复用HUD的只读库存采样，不安装发射钩子、不改变发射参数。
#pragma once

struct CfaWeaponDiagnostic {
    ContinuousIdentity plane;
    Addr activator{},mainClass{},specialClass{};
    int mainAmmo{},specialAmmo{},slot{-1};
    bool cfa{},sampled{},armed{},admmObserved{};
    double time{},lastAdmmTime{};
    int lastAdmmConsumed{};
} cfaWeaponDiagnostic;

void cfaObserveWeapons(Addr plane,Addr activator,const Group& main,const Group& special,double time) noexcept {
    try {
        auto& last=cfaWeaponDiagnostic;
        if(last.plane.address!=plane || last.activator!=activator || !continuousAlive(last.plane)) {
            last={};last.plane=continuousIdentity(plane);last.activator=activator;
            last.cfa=continuousCfa44(plane);
        }
        if(!last.cfa)return;
        const int mainAmmo=scalarInt(main,0),specialAmmo=scalarInt(special,0);
        const bool armed=read<unsigned char>(activator+0x2a1)!=0;
        if(time<last.time)last.admmObserved=false; // 检查点回退不沿用未来的发射记录。
        const bool same=last.sampled && last.mainClass==main.weaponClass && last.specialClass==special.weaponClass &&
            last.slot==session.active && time>=last.time;
        // 只记最近一次ADMM扣弹，不为齐射每枚另写日志；跨重建不推断发射。
        if(same && session.configs[session.active]==0 && specialAmmo>=0 && last.specialAmmo>specialAmmo) {
            last.admmObserved=true;last.lastAdmmTime=time;last.lastAdmmConsumed=last.specialAmmo-specialAmmo;
        }
        const bool changed=!same || armed!=last.armed || mainAmmo!=last.mainAmmo ||
            (session.configs[session.active]==1 && specialAmmo!=last.specialAmmo);
        if(changed) {
            auto group=[&](bool sp,const Group& saved) {
                const Addr live=groupOf(activator,sp),weapon=read<Addr>(live+0x40);
                std::array<int,8> scalars{};std::memcpy(scalars.data(),saved.scalars.data(),saved.scalars.size());
                Json row{{"class",continuousName(saved.weaponClass)},{"ammo",scalarInt(saved,0)},
                    {"window",scalarInt(saved,8)},{"cursor",scalarInt(saved,12)},
                    {"slots",saved.slots.size()},{"scalars_i32",scalars}};
                if(weapon)row["native_weapon"]={{"class",continuousName(read<Addr>(weapon+0x10))},
                    {"id",read<unsigned char>(weapon+0x3b0)},
                    {"fire_all",read<unsigned char>(weapon+0x450)},
                    {"auto_fire",read<unsigned char>(weapon+0x451)}};
                return row;
            };
            Json record{{"event","CFA_WEAPON_SAMPLE"},{"game_time",time},{"same_selection",same},
                {"interval_ms",same?(time-last.time)*1000:0},{"slot",session.active},
                {"armed",armed},{"previous_armed",last.armed},{"target_config",session.configs[session.active]},
                {"admm_consumption_observed",last.admmObserved},
                {"switch_gate_raw",{{"byte_2a0",read<unsigned char>(activator+0x2a0)},
                    {"byte_2a5",read<unsigned char>(activator+0x2a5)},{"queued_inputs",read<int>(activator+0x2b8)},
                    {"byte_2c0",read<unsigned char>(activator+0x2c0)},{"byte_2c1",read<unsigned char>(activator+0x2c1)},
                    {"volley_fired",read<int>(activator+0x2dc)},{"volley_total",read<int>(activator+0x2e0)},
                    {"tick_weapon_group",read<unsigned char>(activator+0x2e4)}}},
                {"main",group(false,main)},{"special",group(true,special)}};
            if(same) {record["main_consumed"]=last.mainAmmo-mainAmmo;record["special_consumed"]=last.specialAmmo-specialAmmo;}
            if(last.admmObserved) {
                record["since_admm_consumption_ms"]=(time-last.lastAdmmTime)*1000;
                record["last_admm_interval_consumed"]=last.lastAdmmConsumed;
            }
#ifdef DUALSP_SU30SM_MOUNT_PROTOTYPE
            record["models"]=fixedMountEnabled && fixedMount.bound && !fixedMountFailed;
#endif
            nativeLog(record.dump());
        }
        last.mainClass=main.weaponClass;last.specialClass=special.weaponClass;last.slot=session.active;
        last.mainAmmo=mainAmmo;last.specialAmmo=specialAmmo;last.time=time;last.armed=armed;last.sampled=true;
    }catch(const std::exception& error) {nativeLog(std::string("CFA_WEAPON_SAMPLE_ERROR ")+error.what());}
    // 这是采样间隔的扣弹量，不是单次按键发射计数；不能据此直接判定根因。
}
