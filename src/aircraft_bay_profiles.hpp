// 舱门能力的有限资源配置。资源资格仍由运行模块核对，不代表多挂布局资格。
#pragma once

enum class AircraftBayStrategy { Shared01Close, PublicSwitchContinuity };
struct AircraftBayProfile {
    const char* pawn;
    const char* body;
    std::array<const char*,3> weapons; // 完整包路径，按原生配置顺序登记。
    AircraftBayStrategy strategy; // 只能选择已实现的通用策略，不执行任意机型回调。
    int knownFlags; // -1表示未采集现场旗帜；算法不依赖其位义，只绑定并监视本次原生值。
};
const std::array<AircraftBayProfile,7>& aircraftBayProfiles() {
    static const std::array<AircraftBayProfile,7> profiles={{
        {"fa50","fa50",{"/Game/Blueprint/Weapons/Player/Variation/plwp_admm_fa50",
            "/Game/Blueprint/Weapons/Player/Variation/plwp_dualtls_fa50",
            "/Game/Blueprint/Weapons/Player/Variation/plwp_eml_fa50"},AircraftBayStrategy::Shared01Close,11},
        {"pkfa","pkfa",{"/Game/Blueprint/Weapons/Player/Variation/plwp_4aam_r1",
            "/Game/Blueprint/Weapons/Player/Variation/plwp_gpb_r0",
            "/Game/Blueprint/Weapons/Player/Base/plwp_plsl_x0"},AircraftBayStrategy::Shared01Close,-1},
        {"dark","dark",{"/Game/Blueprint/Weapons/Player/Base/plwp_sasm_a0",
            "/Game/Blueprint/Weapons/Player/Base/plwp_xsdb_a0",
            "/Game/Blueprint/Weapons/Player/Base/plwp_plsl_x0"},AircraftBayStrategy::Shared01Close,6},
        {"su30","fenra",{"/Game/Blueprint/Weapons/Player/Base/plwp_lasm_a0",
            "/Game/Blueprint/Weapons/Player/Base/plwp_4aam_a0",
            "/Game/Blueprint/Weapons/Player/Base/plwp_weaponHPM"},AircraftBayStrategy::Shared01Close,-1},
        {"su33","fenrb",{"/Game/Blueprint/Weapons/Player/Base/plwp_xqam_x0",
            "/Game/Blueprint/Weapons/Player/Variation/plwp_laam_x0",
            "/Game/Blueprint/Weapons/Player/Base/plwp_lswm_x0"},AircraftBayStrategy::PublicSwitchContinuity,-1},
        {"su30sm","fenrc",{"/Game/Blueprint/Weapons/Player/Base/plwp_sswb_x0",
            "/Game/Blueprint/Weapons/Player/Base/plwp_meml_x0",
            "/Game/Blueprint/Weapons/Player/Variation/plwp_tls_x5"},AircraftBayStrategy::Shared01Close,-1},
        {"su57tg","su57tg",{"/Game/Blueprint/Weapons/Player/Variation/plwp_4aam_r1",
            "/Game/Blueprint/Weapons/Player/Variation/plwp_hcaa_r1",
            "/Game/Blueprint/Weapons/Player/Variation/plwp_lacm_r0"},AircraftBayStrategy::PublicSwitchContinuity,1}
    }};
    return profiles;
}
