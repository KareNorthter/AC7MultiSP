// 持续效果只登记原生契约和有限资源例外；新机体优先复用类型契约，不增加机型控制器。
#pragma once

struct ContinuousNativeContract {
    ContinuousKind kind;
    const char* nativeClass;
    const char* accepted;
    const char* reloadRejected;
    const char* callbacksRejected;
    struct Callback {size_t offset,rva;};
    std::array<Callback,5> callbacks;
    size_t callbackCount;
};
const std::array<ContinuousNativeContract,3>& continuousNativeContracts() {
    static const std::array<ContinuousNativeContract,3> contracts={{
        {ContinuousKind::Electronic,"ElectronicSupportMeasures","native_electronic",
            "electronic_reload_contract_changed","electronic_native_callbacks_changed",
            {{{0x648,0x894850},{0x628,0x895fd0},{0x680,0x86bf90},{},{}}},3},
        {ContinuousKind::UAV,"WeaponUAV","native_uav",
            "uav_reload_contract_changed","uav_native_callbacks_changed",
            {{{0x618,0x8a6bf0},{0x628,0x8779a0},{0x648,0x8a5260},{0x668,0x878d40},{0x680,0x878eb0}}},5},
        {ContinuousKind::Interceptor,"LaserInterceptor","native_interceptor",
            "interceptor_reload_contract_changed","interceptor_native_callbacks_changed",
            {{{0x618,0x873730},{0x628,0x8736e0},{0x648,0x894850},{0x678,0x873700},{0x680,0x861490}}},5}
    }};
    return contracts;
}
struct ContinuousTLSProfile {
    const char* pawnClass;
    const char* pawnPackage;
    const char* weaponClass;
    const char* weaponPackage;
};
const std::array<ContinuousTLSProfile,3>& continuousTLSProfiles() {
    static const std::array<ContinuousTLSProfile,3> profiles={{
        {"AcePlayerPawn_mrgn_C","/Game/Blueprint/Player/Pawn/AcePlayerPawn_mrgn",
            "plwp_tls_x1_C","/Game/Blueprint/Weapons/Player/Variation/plwp_tls_x1"},
        {"AcePlayerPawn_adf11f_C","/Game/Blueprint/Player/Pawn/AcePlayerPawn_adf11f",
            "plwp_tls_x3_C","/Game/Blueprint/Weapons/Player/Variation/plwp_tls_x3"},
        {"AcePlayerPawn_su37_C","/Game/Blueprint/Player/Pawn/AcePlayerPawn_su37",
            "plwp_tls_x0_C","/Game/Blueprint/Weapons/Player/Base/plwp_tls_x0"}
    }};
    return profiles;
}
