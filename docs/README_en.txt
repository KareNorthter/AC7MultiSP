AC7MultiSP 2.17.0 Test Candidate

Carry several special weapons on one aircraft and switch between them during a mission.
For Windows 10/11 64-bit and the Steam version of ACE COMBAT 7, build 9855922, in single-player campaign and free missions.
Other builds, VR and multiplayer are outside the supported scope. Mod features are automatically disabled in multiplayer.

Features
• Select one to three SP weapons through the native preparation menus. Standard missiles remain available. You can change your SP loadout during resupply.
• Each SP keeps its own ammunition and reload state. One SP uses full ammunition; defaults are 50% each for two SPs and 33% each for three. Adjust these percentages in the management panel.
• HUD weapon and reload information, native weapon cycling, and direct selection of all four weapon slots.
• Experimental simultaneous SP models retain selected external stores where supported. Layouts vary by aircraft; some internal bays retain native model switching.
• Chinese/English management panel, configurable controls, ammunition settings, model toggle and logs.
• Debug controls can temporarily rebuild/refill the SP loadout or change aircraft by restarting the entire mission. Unpurchased and unlicensed DLC aircraft are listed for testing. This does not grant purchases or DLC licenses, and spawning them is not guaranteed.
• Mission 12 uses its assigned weapons with Multi-SP takeover temporarily disabled. Features resume after leaving, without changing saved settings. Free flight on the same map is unaffected.

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
• Optional target tracking toggle: enable controller X replacement in Features (off by default), or assign an independent keyboard key (unbound by default). Press once to track and again to stop. Y retains its native behavior. When X replacement is enabled, use the D-pad or native keyboard cycling for SP selection. Pausing, opening management, losing focus or losing the target stops tracking.
• On first launch, settings migrate after saving profiles.before-target-gaze.json; installation preserves the original personal settings.
• F6 or controller Back+Y: management panel. It pauses an active mission and only releases the pause it created.
• Default Ctrl+Alt+F6 cycles debug SP combinations and refills them; Ctrl+Alt+F7 toggles simultaneous models. These bindings can be changed.
• Saving normal settings does not refill the current mission. Changing aircraft through Debug restarts the entire mission.
• If simultaneous models cause performance problems, disable that option. Multi-SP ammunition and switching remain available.

Known issues and planned improvements
• CFA-44: this candidate includes switching-overhead reductions and ECM mechanism hiding. Improvements over the 2.16 hitch/model exposure still require gameplay verification.
• ADF-01: leaving TLS stops the beam immediately, but the first stop causes a noticeable hitch; later stops may cause smaller hitches.
• SP selection, first-time asset loading and mission entry can still pause briefly. Cursor hitches on the SP2/SP3 pages of CFA-44 and F-22A need further investigation.
• F-2A simultaneous-model performance needs more work. Some aircraft, special mechanisms or limited hardpoints retain native visuals. Su-47, Su-57 TGM and X-02S use native visuals.
• An occasional hangar exit/crash was previously reported without a confirmed cause. Not every aircraft, combination, mission or recovery path has been individually validated.
The other issues remain recorded. Target tracking and the Mission 12 correction also await gameplay verification; this candidate is not a formal release.

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
