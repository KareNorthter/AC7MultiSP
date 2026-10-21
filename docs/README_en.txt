AC7MultiSP 2.27.2 (source candidate; not built or installed)

2.27.2 source changes (since this branch's 2.27.1; in-game validation pending)
• Management pauses now request muting of this game's audio sessions in a background worker and restore the original unmuted sessions when closed. Previously muted sessions stay muted. A recovery record permits a later startup to attempt recovery after a crash; changed devices/session identifiers may prevent recovery. No game volume settings or other applications are changed.
• Debug groups are displayed as flat cards. Logs alone have an Expand/Collapse button. Management binding is beside the master switch; Gun salvo and Camera hold bindings are beside their checkboxes, labeled Shortcut. Language selection is preceded by 语言/Language. Existing approved Chinese descriptions and notes are retained.
• Infinite ammo now recycles finite-initialization disabled slots only inside the current eligible SP reload window. Pending-fire markers remain intact, and the native engine still creates projectiles. Cache, option changes and checkpoint handling use the same queue rules; switching infinite ammo off resumes finite consumption without refilling. Rapid-fire/continuous SP reload behavior remains unchanged.
• Audio enumeration and recovery-file writes run off the game/render threads. Infinite-only window scans are skipped when the queue has not changed. These are implementation limits, not a verified zero-overhead guarantee. No build, installation, test run or game operation was performed. Management 21 / configuration 19 and resources remain unchanged; matching core/UI/Lua are required.

2.27.1 source changes (since this branch's 2.27.0; in-game validation pending)
• Mod-owned logs use one background writer. Four mod log segments and six native/UI segments are capped at 1,000,000 bytes each, totaling at most 10 MB. Full segments overwrite the oldest fixed archive slot.
• Game and rendering threads only attempt a bounded queue submission; busy queues drop records without waiting for disk. Background CPU/I/O priority, batching, duplicate suppression and rate limits put gameplay responsiveness first. Existing oversized files are trimmed in the background on first use.
• Logs share UTC timestamps, event sequences and thread/source details. Only event-driven settings and debug context are expanded; no additional per-frame game-state sampling is added. Management still reads at most 64 KiB per log stream on demand, including recent archives.
• Queued tail records can be lost on a crash or forced exit. Zero performance overhead has not been verified; this remains unbuilt and untested source. Management protocol 21, configuration revision 19 and resources are unchanged; core/UI/Lua must match.

2.27.0 source changes (since this branch's 2.26.1; in-game validation pending)
• Starting or stopping latched tracking no longer shows an information toast. Existing no-target warnings and errors remain.
• Debug adds Infinite ammo and Fast reload. Both default off and apply immediately when checked, without saving settings. They affect only SPs managed by this mod, not standard missiles, guns or countermeasures.
• Infinite ammo preserves existing stock without refilling it. Fast reload reduces eligible SP reloads to at most 1 second; PLSL, HGP, MGP and other rapid-fire or continuous-effect SPs keep native behavior. Both options reset when the game restarts.
• Management protocol 21 / configuration revision 19. Resources are unchanged. This source candidate has not been built, installed or tested; the installed baseline remains 2.26.0.

2.26.1 source changes (since installed 2.26.0)
• Management follows the user's design with General, Camera and Debug tabs. The master switch is beside the title; disabling it dims the pages. Logs are at the end of Debug.
• Empty groups and reset buttons are removed. SP-combination cycling and mission-model shortcuts are configurable only in profiles.json. NEXT mode is selected only in Management; L3 and keyboard NEXT-mode switching are removed.
• Holding controller Y for 0.2 seconds and pressing the game's actual keyboard tracking binding have independent latched-tracking options, both off by default. Disabled inputs retain native hold-to-track behavior.
• L3 recenters. Short R3 changes view; holding R3 for 0.3 seconds toggles Camera hold. These controller actions are always enabled; both sticks retain countermeasures. Camera hold defaults to Ctrl+Alt+F6; Gun salvo keeps Ctrl+Alt+F5.
• New configurations enable Gun salvo (MGP/PLSL; HGP and simultaneous different SPs remain unsupported). Always-visible central weapon text and the center dot default off. Upgrades retain existing personal display settings.
• Configuration revision 19 / tracking protocol 4. Upgrades back up settings before migration. The 77 resources / 154 PAK members are unchanged. The following entries retain their historical version and installation status.

AC7MultiSP 2.26.0 (candidate)

2.26.0 candidate changes (since 2.25.1; user validation pending)

• Independent mission/hangar model switches and optional D-pad weapon switching. Keyboard selection remains independent.
• Ordinary notices last 1 second; warnings, errors and important messages last 3 seconds.
• Release L3 to recenter. Short R3 changes view; hold for 0.3 seconds to toggle camera hold. Both sticks retain countermeasures.
• Latched tracking defaults off; when enabled, hold Y for 0.2 seconds or tap the actual native keyboard tracking binding to latch/release. NEXT has Normal/VR ranking modes; the center dot hides within 5 degrees. Angle-based auto recenter is removed.
• Configuration upgrades to revision 18 after byte-preserving backups on first launch. Personal bindings, profiles and camera hold remain. Update the complete core/Lua and 77 variants/154 members.

2.25.1 candidate changes (since 2.25.0; validation pending)

• F-4E UGB+SASM alternates sides and follows remaining stock: 9 gives 5+4, 10 gives 5+5, zero clears all; twelve positions and reload gaps remain.
• F/A-18E moves LAGM inboard and 4AGM outboard. TGM moves GPB inboard and air-to-air weapons outboard, borrowing the complete rack. Only the affected dual/triple combinations change.
• Paused aircraft switching closes and resumes the native menu before restarting the mission. Closing F6 alone retains native pause.
• Complete core/Lua and 77 variants/154 members must match; the previous 148 members remain unchanged. Personal configuration, saves and other mods are preserved.

2.25.0 candidate changes (since 2.24.0; user validation pending)
• Added camera-center NEXT selection, off by default, with remappable Ctrl+Alt+F8. NEXT and actual selection use the same native maintenance flow.
• Center dot defaults on and appears beyond 10 degrees from the nose. Camera hold uses 25 ms velocity response and smooth release. Disable the mouse-flight mod F2 HMD before enabling the new target selection; its settings are not changed by this install.
• Custom aircraft use actual table references and skin-specific SP classes. Skin changes rebuild unconfirmed drafts; missing presentation text falls back without relaxing identity, purchase or write guards.
• Nineteen static variants on seven aircraft preserve native collision data and verify settings around mesh changes. Cross-pool projectiles still require NoCollision. Applied aircraft restarts hand off native pause and defer opening mounts until flight input is ready.
• MiG-31B rear SAAM revision r3 moves another 4 units aft from r2, retaining height: native-local X minus 11/Z minus 2. Complete core/Lua and 74 variants/148 members must match.
• Configuration 15/management 16 adds a fifth shortcut. Configuration 14 is backed up to profiles.before-view-target.json; a shortcut conflict leaves only the new binding empty. Personal settings and loadouts remain.

2.24.0 candidate changes (since 2.23.1; user validation pending)
• SP1/SP2/SP3 show the focused weapon native long description in the game language. Preparation and resupply hide unpurchased SPs while retaining native IDs.
• Toasts use AC7MultiSP: description. Open F6 while paused to change aircraft through Debug and restart the whole mission; closing management retains the original pause.
• Corrected four aircraft AnimBP package paths. F-14A uses core NoCollision qualification and failed assets retry only at explicit setup/rebuild boundaries.
• MiG-31B rear SAAM moves one further unit aft with unchanged height: native-local X minus 7/Z minus 2. Revision r2 replaces r1 at the same index; 74 variants/148 members.
• Configuration 14 and personal loadouts are retained. SP1 entry stutter, flashes and transition animations remain research only.

2.23.1 candidate changes (since 2.23.0; observation pending)
• MiG-31B SAAM+PLSL and triple-SP rear SAAM pairs move symmetrically 6 units aft and 2 down in model-local resource units. Y=±4.6, rotation and scale stay unchanged.
• Visible and firing sockets share the offset. Other combinations, both PLSL pods and the four-aircraft runtime additions are retained. One new variant gives 74 variants/148 members; update the complete set.
• Configuration 14 needs no new migration. Settings, loadouts and saves are preserved; clipping and belly clearance need in-game observation.

2.23.0 candidate changes (since 2.22.0; user validation pending)
• Added all seven nonempty SP combinations for F-14D, F/A-18F, Su-33 and F/A-18F Block III; single-SP visuals remain native.
• Retained native SWP3 mechanisms and EML bodies, with shared visual/firing sockets. Block III deployment, clearance and performance need in-game validation.
• Added eight rack variants, totaling 73 variants/146 members. Existing 24 aircraft layouts and 65 variants are preserved; core, Lua and PAK must match.
• Retained 2.22.0 camera and F-4E/MiG-35D changes, personal settings and loadouts. MiG-31B remains deferred.

2.22.0 candidate changes (since 2.21.1; user validation pending)
• Camera hold adds a 0–45 degree auto-center threshold, default 30; 0 disables it. Releasing look input inside the cone starts native smooth recentering.
• Right-stick look uses smoothed relative angular velocity. Short R3 release changes view; 0.2 seconds recenters and 2 seconds toggles and saves Camera hold.
• F-4E UGB+SASM shows six UGB models per side while retaining 50% ammunition and nine actual bombs. MiG-35D puts 4AGM inside and borrows outer stations for SAAM.
• Added F-14A, F/A-18E and TGM mount candidates plus a MiG-31B observation layout with known intersections. Core, Lua and the 65-variant/130-member PAK must match.
• Configuration 13 upgrades to 14 after saving profiles.before-camera-auto-center.json; existing settings and loadouts remain.

Changes in this version (since 2.21.0)
• Added Gun salvo linking the native gun action to MGP/PLSL. Off by default; remappable Ctrl+Alt+F5. HGP and concurrent different SPs are unsupported.
• Single-key actions now work while Shift/Ctrl/Alt is held; exact configured chords take priority to avoid duplicate actions.
• F6 uses General, Camera, Logs and Debug. Tracking and Camera hold are off for fresh settings; upgrades retain saved choices.
• Debug aircraft changes support loaded mod aircraft, with name/code/ID search and source/initial-letter filters.
• Fixed CFA muzzle-record mismatch handling; F-15 S/MTD HPAA+FAEB and triple-SP layouts show two HPAA/two FAEB.

Carry several special weapons on one aircraft and switch between them during a mission.
For Windows 10/11 64-bit and the Steam version of ACE COMBAT 7, build 9855922, in single-player campaign and free missions.
Other builds, VR and multiplayer are outside the supported scope. Mod features are automatically disabled in multiplayer.

Features
• Gun salvo links the native gun action to a loadout’s MGP or PLSL. On for new configurations; release attempts to restore the previous selection. Switching gaps remain possible. Pausing, opening Management, focus loss or manual selection cancels the action.
• Select one to three SP weapons through the native preparation menus. Standard missiles remain available. You can change your SP loadout during resupply.
• Each SP keeps its own ammunition and reload state. One SP uses full ammunition; defaults are 50% each for two SPs and 33% each for three. Adjust these percentages in the management panel.
• HUD weapon and reload information, native weapon cycling, and direct selection of all four weapon slots.
• In Management > General, choose Always visible or Show for 1 second after switching (default) for the central weapon abbreviation. Saving applies the setting. Timed mode starts hidden on mission entry; each actual switch restarts the timer, reselecting the same weapon does not extend it, and pauses do not count. Lower-right weapon information remains available.
• Experimental simultaneous SP models retain selected external stores where supported. Layouts vary by aircraft; some internal bays retain native model switching.
• Chinese/English management panel, configurable controls, ammunition settings, model toggle and logs.
• Debug controls can temporarily rebuild/refill the SP loadout or change aircraft by restarting the entire mission. The aircraft picker includes loaded mods with complete three-SP data, plus unpurchased/unlicensed official aircraft. Search by name/code/ID and filter by source or initial letter. This does not grant purchases or DLC licenses, and spawning them is not guaranteed.
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
• Latched tracking has independent controller and keyboard options in Camera, both off by default. When enabled, hold Y for 0.2 seconds or press the game's actual keyboard tracking binding to start/stop. Short Y presses still change targets. Pausing, management, focus loss or target loss stops tracking.
• Upgrades preserve personal settings and loadouts. Current configuration revision is 19; revision 18 is backed up to profiles.before-management-redesign.json before migration. Old default cycle/model shortcuts are cleared, custom bindings are retained, and a conflicting new camera-hold shortcut is left unassigned.
• Camera hold is off by default. Release L3 or use the recenter binding (middle mouse by default) to recenter. Short R3 changes view; hold R3 for 0.3 seconds to toggle Camera hold. Both sticks retain countermeasures. Sensitivity ranges from 5% to 200%; angle-based auto recenter is removed.
• F6 or controller Back+Y: management panel. It pauses an active mission and only releases the pause it created.
• F6 opens Management, Ctrl+Alt+F5 toggles Gun salvo, and Ctrl+Alt+F6 toggles Camera hold by default. Cycling SP combinations and toggling mission models have no default shortcut; configure them in profiles.json. Direct weapon keys are configured in General.
• Saving normal settings does not refill the current mission. Changing aircraft through Debug restarts the entire mission.
• Mission and hangar multi-SP models have separate switches, both on by default. Disabling either affects visuals only; SP ammunition and switching remain available. D-pad selection is on by default and can be disabled independently of keyboard selection.

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
F-14A | Candidate | Validation pending
F/A-18E | Candidate | Validation pending
F/A-18E TGM | Candidate | Validation pending
MiG-31B | r3 observation | Rear SAAM pair moved 11 aft/2 down from native; support and clearance pending
F-14D | Candidate | Native SWP3 mechanism; validation pending
F/A-18F | Candidate | Native SWP3 mechanism; validation pending
Su-33 | Candidate | Native SWP3 mechanism; validation pending
F/A-18F Block III | Candidate | Native SWP3 mechanism; validation pending
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
• One identified preparation-cache crash is addressed by this mod-side workaround. Other hangar exits, custom-aircraft sorties and recovery paths still need their own evidence; they are not assumed to share that cause.

Compatibility with other mods
• Earlier versions were tested in selected scenarios with AC7MouseAim, FOV and proportional-guidance mods, and PW-MKI/Adler A replacement aircraft. This acceptance does not cover exhaustive testing of those combinations.
• A J-20 replacement was previously checked for model/skin appearance with native single-SP behavior. This does not establish Multi-SP support for all custom aircraft.
• Skin, model, HUD, weapon-table, firing and input mods may modify the same assets or interfaces. Changed skeletons or sockets can make simultaneous models unsupported or trigger a native-visual fallback.
• Conflicting dwmapi.dll files, different UE4SS versions, or mods controlling the same weapon/input paths require individual checks. Rebind F6 if another mod uses it.
• Compatibility reports apply to specific versions and scenarios, not every mod. None of those third-party mods is bundled here.

When reporting a problem, include aircraft, SP combination, mission, steps and approximate time, plus ue4ss\Mods\DualSP\mod.log and native.log. Comparing with simultaneous models disabled can help narrow it down.

Third-party notices
UE4SS, Dear ImGui, MinHook and nlohmann/json licenses are in the mod's licenses folder; json.lua retains its MIT notice.
SP2.wav is a user-supplied, non-original sound. This project grants no additional rights to it. Microsoft runtimes are supplied as their original installers.

Version history (2.xx)
Includes test versions; historical features may have been replaced or withdrawn. Current instructions take precedence.
2.0.0：Unified the AC7MultiSP project, build/install workflow and three-SP baseline.
2.1.0：Intermediate test version.
2.1.1：Intermediate test version.
2.2.0：Added multi-SP model previews in hangars and resupply menus.
2.2.1：Extended previews to other hangar pages and sortie transitions.
2.2.2：Improved model rebuilding, sortie positioning and management options.
2.3.0：Added in-mission loadout cycling and a model-toggle shortcut.
2.3.1：Revised F-15E, F-35C and Su-30SM layouts and model toggling across scenes.
2.4.0：Added custom shortcut chords and integrated mount/continuous-weapon work.
2.5.0：Improved safe master-switch application, brief notifications and performance logging.
2.6.0：Expanded static layouts to ten more aircraft and reduced preview overhead outside hangars.
2.7.0：Fixed mount-asset loading and integrated ADF-11F continuous-weapon handling.
2.7.1：Revised ADF-11F UAV capacity, F-2A switching and Rafale/Typhoon mount points.
2.7.2：Moved F-2A LASM inside and HVAA outside; delivered with a later build.
2.8.0：Trialed full-bay layouts for F-22A, Su-47 and Su-57 TGM; significant performance problems remained.
2.8.1：Reduced repeated full-bay maintenance; these approaches were later archived.
2.9.0：Prioritized external-store retention; restored native visuals for Su-47 and Su-57 TGM.
2.10.0：Improved F-22A switching and added lightweight ADF-01, DarkStar, ADF-11F and CFA-44 layouts.
2.11.0：Expanded ten more layouts and classified support approaches for 47 armed aircraft.
2.12.0：Revised mount counts, lightweight animation and continuous lasers; trialed X-02S visuals.
2.13.0：Added aircraft changes through mission restart, revised layouts and withdrew X-02S visual takeover.
2.14.0：Prioritized three SPs for debug aircraft changes and refined loadouts and Su-30M2/CFA/ADF cleanup.
2.15.0：Loaded assets per combination, reduced repeated parsing and refined loadout titles and preview rebinding.
2.16.0：Integrated loading/preview optimizations, CFA dual layouts, cycling HUD fixes, English text and hangar-back handling.
2.17.0：Added sustained tracking and revised CFA switching and ECM visuals.
2.18.0：Restored native CFA visuals outside missions and corrected assigned-SP bypass to Mission 13.
2.19.0：Optimized CFA muzzle lookup and switching; user feedback confirmed reduced stutter.
2.19.1：Retained CFA opening progress across multi-SP switches and added Camera hold.
2.19.2：Revised stale firing input and R3 routing; blocked switching during FALKEN laser activity.
2.19.3：Cleared stale target registrations, reduced repeated memory checks and refined recenter bindings/sensitivity.
2.19.4：Further reduced repeated CFA work and refined the R3 option and keyboard/mouse Camera hold.
2.19.5：Improved view/tracking direction continuity and reduced controller tracking-toggle notifications.
2.20.0：Released the integrated 2.19-series changes with aircraft support notes.
2.20.1：Added timed weapon abbreviations, revised L3/R3 gestures, F6 cards and XFA-27/MiG-35D mount revisions.
2.20.2：Preserved sustained tracking across view changes and improved local crash-capture tooling.
2.20.3：Retained reflected field-name strings to avoid the identified F-22 preparation crash path.
2.21.0：Released the integrated 2.20.1–2.20.3 changes with updated instructions and aircraft support tables.
2.21.1：Added MGP/PLSL Gun salvo and mod-aircraft search; revised modifier input, F6, defaults, CFA handling and F-15 mounts.

2.22.0: Added threshold recentering, smooth stick look, R3 hold toggle, F-4E/MiG-35D revisions and four mount candidates; validation pending.
2.23.0: Added four aircraft runtime mount candidates and eight variants; validation pending.
2.23.1: Shifted MiG-31B rear SAAM sockets 6 aft/2 down; observation pending.
2.24.0: Added native weapon descriptions, purchased-SP lists and paused F6 aircraft changes; fixed resource qualification and MiG-31B r2. Validation pending.
2.25.0: Added view-center NEXT/dot, smoother hold and mod-aircraft skin-SP support; integrated native collision preservation, pause handoff and MiG-31B r3. Validation pending.

2.25.1: Fixed F-4E stock/alternating visuals, two F/A-18E layouts and native menu pause during aircraft switching; validation pending.

2.26.0: Added independent model/D-pad options and integrated NEXT modes, latched tracking, camera controls and notice durations; validation pending.
