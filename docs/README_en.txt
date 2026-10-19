AC7MultiSP 2.20.3 Test Candidate

Addresses a crash when entering weapon preparation: fixed field names remain referenced for as long as the mod uses them, avoiding dangling strings in the loader's member-name cache. Covers preparation, review, parts, resupply, weapon configurations and mount queries. Layouts, models, animations and settings are unchanged.
This candidate has been built, passed 11 existing checks and been installed over 2.20.2. Gameplay verification is pending. Fully restart the game; hot reload cannot repair an existing dangling cache. Settings, saves and other mods are preserved. The released version remains 2.20.0.

Installed 2.20.2 candidate notes:

With Camera hold enabled, ordinary first-person, cockpit and third-person view changes now carry sustained tracking progress and direction across views. This addresses recentering before returning to the tracked target. Applies to L3 and custom keyboard tracking toggles. Controls, settings and defaults remain unchanged. Disabling Camera hold retains native behavior.
This candidate has been built, passed 11 existing checks and been installed over 2.20.1. Gameplay verification is pending. Controls, profiles, saves and other mods are preserved. The released version remains 2.20.0.

Installed 2.20.1 candidate features:

This candidate integrates always-visible/one-second weapon labels, the four-page management UI, L3 tracking/R3 0.3-second recenter and XFA-27/MiG-35D mount changes. Management protocol 13/configuration 12; first launch backs up before migration. Gameplay verification is pending.

This candidate has been built, passed the 11 existing checks and been installed; gameplay validation is pending. The released version remains 2.20.0.
• X retains native weapon cycling. Press and release L3 to toggle sustained target tracking.
• With Camera hold and Hold R3 to recenter enabled, release a short R3 press to change view, or hold 0.3 seconds to recenter immediately without also changing view. Both sticks only release native countermeasures.
• Fresh configurations enable L3 tracking, Camera hold and R3 hold-to-recenter by default. All remain optional in the second Bindings tab. Middle-mouse recenter, custom bindings and 5%–200% sensitivity remain.
• F6 now uses General, Bindings, Logs and Debug pages with dark cards. Read details through hover/focus/click on ?, expand less-used bindings or aircraft controls, and use the fixed Save footer beneath scrollable content.
• Migration backs up profiles.before-controller-clicks.json then backs up profiles.before-central-weapon-display.json before moving to configuration 12. Both backups retain the original old bytes. Central weapon text defaults to always visible. X replacement is retired; the new L3 and R3 behavior starts enabled. Existing Camera hold choices, bindings and loadouts remain. Matching core/UI/Lua files are required: management protocol 13, tracking 2, camera 4.

The following 2.20.0 release notes are historical; this candidate's defaults and R3/X behavior follow the rules above.

What is new in 2.20.0 (since 2.19.0)
• Camera direction hold, off by default in the second Bindings tab, supports controller release, mouse viewpoint-mode release, short target selection and ordinary view changes. Tracking turns from the current direction and returns to it afterward. Native auto-recentering is suppressed only while Camera hold is enabled.
• Independent recenter defaults to middle mouse and can be rebound. Right-stick sensitivity is 5%–200%, default 100%. Optional R3 replacement is off by default: when enabled, release a short press to recenter, or hold 0.5 seconds to change view immediately; both sticks retain countermeasures and recenter together.
• CFA-44 dual/triple-SP changes retain deployment progress. Reduced repeated muzzle, model, lock and persistent-effect checks lower switching hitches. Initial loading can still be slower; no fixed 10 ms or hitch-free guarantee is made.
• Shared weapon rebuilding now releases old lock registrations to prevent stale targets causing extra MSL/EML shots, while preserving normal multi-target salvos. The user confirmed the latest changes work; this is not exhaustive validation of every aircraft/combination.
• ADF-01 switch requests during TLS/trails are discarded; press again after they end. The beam is not forcibly stopped and requests are not queued.
• Normal controller-X target-tracking toggles are silent; errors, no-target and keyboard notifications remain. Personal settings are retained, with a backup before any required migration.

Carry several special weapons on one aircraft and switch between them during a mission.
For Windows 10/11 64-bit and the Steam version of ACE COMBAT 7, build 9855922, in single-player campaign and free missions.
Other builds, VR and multiplayer are outside the supported scope. Mod features are automatically disabled in multiplayer.

Features
• Select one to three SP weapons through the native preparation menus. Standard missiles remain available. You can change your SP loadout during resupply.
• Each SP keeps its own ammunition and reload state. One SP uses full ammunition; defaults are 50% each for two SPs and 33% each for three. Adjust these percentages in the management panel.
• HUD weapon and reload information, native weapon cycling, and direct selection of all four weapon slots.
• In Management > Features & Parameters, choose Always visible (default) or Show for 1 second after switching for the central weapon abbreviation. Saving applies the setting. Timed mode starts hidden on mission entry; each actual switch restarts the timer, reselecting the same weapon does not extend it, and pauses do not count. Lower-right weapon information remains available.
• Experimental simultaneous SP models retain selected external stores where supported. Layouts vary by aircraft; some internal bays retain native model switching.
• Chinese/English management panel, configurable controls, ammunition settings, model toggle and logs.
• Debug controls can temporarily rebuild/refill the SP loadout or change aircraft by restarting the entire mission. Unpurchased and unlicensed DLC aircraft are listed for testing. This does not grant purchases or DLC licenses, and spawning them is not guaranteed.
• Mission 13 uses its assigned weapons with Multi-SP takeover temporarily disabled. Features resume after leaving, without changing saved settings. Free flight on the same map is unaffected.

Installation
1. Extract the entire archive and close the game. In Steam, use Manage > Browse local files. Locate the folder containing Ace7Game.exe.
2. Back up any existing dwmapi.dll, ue4ss folder, and the matching mount PAK in Game\Content\Paks\~mods.
   Existing personal settings are in ue4ss\Mods\DualSP\profiles.json. Keep that file when upgrading; do not delete the entire DualSP folder.
3. If needed, run Runtimes\VC_redist.x64.exe and Runtimes\DirectX\DXSETUP.exe.
   Do not uninstall an equal or newer VC++ runtime. Keep DXSETUP.exe together with its CAB files.
4. Merge the dwmapi.dll, ue4ss and Game items INSIDE GameFiles into the game root. Do not copy the GameFiles folder itself.
   The model package must end up at Game\Content\Paks\~mods\AC7MultiSP_Mounts_P.pak.
5. The supplied mods.txt enables DualSP : 1 for a fresh installation. If your existing ue4ss\Mods\mods.txt lists other mods, keep it and add or retain that line instead of overwriting the list.
6. Launch normally through Steam. F6 opens the management panel by default; upgrades retain your previously configured key.
   Keep the internal directory and enable name DualSP unchanged. Python and development tools are not required.

Check existing UE4SS installations or proxy DLLs before merging files. This package uses an AC7-specific loader; a generic UE4SS build is not a direct replacement.
Personal settings, saves, logs and other mods are not included. To uninstall, move out the DualSP folder and the mount PAK, and remove DualSP : 1. Only remove shared loader files if no other mod uses them.

Controls
• Choose SP1, SP2 and SP3 in sequence; the latter two can be NO WEAPON. Use keyboard or controller in native preparation menus.
• Default keyboard 1/2/3/4: standard missiles / SP1 / SP2 / SP3.
• Controller D-pad left/up/right/down: standard missiles / SP1 / SP2 / SP3. Native weapon cycling remains available.
• Optional toggle tracking: L3 is enabled by default in the second Bindings tab; press and release to start/stop. An independent keyboard key is unbound by default. X cycles weapons; Y retains native actions. Both-stick countermeasures never toggle tracking. Pausing, management, focus loss or target loss stops tracking.
• Upgrades retain personal settings. Older configurations are backed up before each required migration; current configuration revision is 10. Existing revision-10 settings need no further migration.
• Camera hold and Hold R3 to recenter are enabled by default for fresh configurations. Short R3 release changes view; holding 0.3 seconds recenters, while both sticks only release countermeasures. Independent middle-mouse recenter and 5%–200% sensitivity remain. Disabling Camera hold restores native recentering; existing saved Camera hold choices are retained.
• F6 or controller Back+Y: management panel. It pauses an active mission and only releases the pause it created.
• Default Ctrl+Alt+F6 cycles debug SP combinations and refills them; Ctrl+Alt+F7 toggles simultaneous models. These bindings can be changed.
• Saving normal settings does not refill the current mission. Changing aircraft through Debug restarts the entire mission.
• If simultaneous models cause performance problems, disable that option. Multi-SP ammunition and switching remain available.

Simultaneous SP-model support
Adapted means a simultaneous-visual layout is implemented, not that every aircraft and combination has been individually playtested. Visible positions follow hardpoints, ammunition and reload state; they do not equal total ammunition. Aircraft without adapted visuals retain Multi-SP weapon functionality. Aircraft not listed below use native visuals.

Aircraft | Status | Necessary notes
F-15E | Adapted |
F-35C | Adapted | 4AAM has fewer visible positions when SOD is selected; ammunition and lock windows are unchanged.
Su-30SM | Adapted |
F-2A | Adapted | Simultaneous-model performance still needs work.
F-15C | Adapted |
F-15J | Adapted |
F-15 S/MTD | Adapted |
F-16C | Adapted |
XFA-27 | Adapted |
Mirage 2000-5 | Adapted |
ADFX-01 | Adapted |
Rafale M | Adapted |
Typhoon | Adapted |
Su-35S | Adapted |
Su-37 | Adapted |
F-4E | Adapted |
F-14A TGM | Adapted |
Gripen E | Adapted |
Su-30M2 | Adapted |
Su-34 | Adapted |
MiG-29A | Adapted |
F-16XL | Adapted |
MiG-35D | Adapted |
F-22A | Partial | Retains side 8AAM models when selected; internal QAAM/XSDB use native switching.
Su-57 (standard) | Partial | Retains PLSL pods; internal 4AAM/GPB remain native.
ADF-01 FALKEN | Adapted | 4AAM/FAEB share racks. Switches during TLS/trails are discarded; press again afterward.
DarkStar | Partial | Retains both PLSL guns across SP changes; SASM/XSDB follow native current-SP visuals.
ADF-11F | Adapted | QAAM+UAV and triple loadouts use two QAAM positions on the right and one UAV on the left; minor UAV/rail clipping remains.
CFA-44 | Adapted | ADMM+EML and triple loadouts split the sides; ECM models are hidden and out-of-mission visuals remain native.

Su-47, Su-57 TGM and X-02S retain Multi-SP weapon functionality but use native current-SP models/animations, without full simultaneous mounting. Standard Su-57 and Su-57 TGM are separate cases.

Known issues and planned improvements
• SP selection, first-time resource loading and mission entry can still pause. CFA-44/F-22A SP2/SP3 selection cursors and F-2A simultaneous-model performance remain improvement areas.
• Mission-results arrows stutter noticeably less, but slight stuttering remains.
• An occasional hangar exit/crash was reported without a confirmed cause. Not every aircraft, combination, mission or recovery path has been individually validated.

Compatibility with other mods
• Earlier versions were tested in selected scenarios with AC7MouseAim, FOV and proportional-guidance mods, and PW-MKI/Adler A replacement aircraft. Those combinations have not received a complete 2.16 retest.
• A J-20 replacement was previously checked for model/skin appearance with native single-SP behavior. This does not establish Multi-SP support for all custom aircraft.
• Skin, model, HUD, weapon-table, firing and input mods may modify the same assets or interfaces. Changed skeletons or sockets can make simultaneous models unsupported or trigger a native-visual fallback.
• Conflicting dwmapi.dll files, different UE4SS versions, or mods controlling the same weapon/input paths require individual checks. Rebind F6 if another mod uses it.
• Compatibility reports apply to specific versions and scenarios, not every mod. None of those third-party mods is bundled here.

When reporting a problem, include aircraft, SP combination, mission, steps and approximate time, plus ue4ss\Mods\DualSP\mod.log and native.log. Comparing with simultaneous models disabled can help narrow it down.

Third-party notices
UE4SS, Dear ImGui, MinHook and nlohmann/json licenses are in the mod's licenses folder; json.lua retains its MIT notice.
SP2.wav is a user-supplied, non-original sound. This project grants no additional rights to it. Microsoft runtimes are supplied as their original installers.
