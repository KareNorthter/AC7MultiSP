AC7MultiSP 2.23.0 (candidate)

Changes since official 2.22.0
• Three management tabs, independent controller/keyboard tracking options and Ctrl+Alt+F6 Camera hold. Loadout-cycle/model shortcuts default unbound; NEXT mode is selected in management.
• Gun salvo defaults on; permanent central text and the dot default off. Old settings/profiles are backed up and reset without migration; first launch creates revision 19 defaults.
• F/A-18E TGM uses r2 to fix a cooked Socket reference. All 77 variants require matching offline qualification; the other 152 members are unchanged.


What is new in 2.22.0 (since official 2.21.1)
• NEXT targeting offers Normal (aircraft nose) and VR (current view center) ranking. This does not enable VR hardware. The optional center dot appears beyond 5 degrees.
• Release L3 alone to recenter. Short R3 changes ordinary views; hold for 0.3 seconds to toggle and save Camera hold. Both sticks retain countermeasures.
• Latched tracking defaults off. When enabled, hold Y for 0.2 seconds or tap the game's actual keyboard tracking binding to latch/release. The custom feature shortcut only enables/disables this option.
• Independent mission/hangar model switches and optional D-pad weapon selection. Ordinary notices last 1 second; warnings, errors and important notices last 3 seconds.
• Improved native SP descriptions, purchased-SP filtering, mod-aircraft/skin SP catalogues and paused aircraft switching. Switching closes the native pause menu before restarting the entire mission.
• Integrated F-4E stock/alternating visuals, MiG-35D and additional native-aircraft layouts, local moving racks and native collision preservation. Update core, Lua and all 77 variants/154 members together.
• The local maintenance installer now limits default integrity checking to this mod, its mount PAK, critical loader files and saves. A full audit of other mods is optional; it does not assess compatibility.

The user tested the installed research candidate 2.26.0 and reported no problems. This release adopts official version 2.22.0; gameplay sources, core and resources retain accepted bytes with release labels updated. Intermediate research numbers remain internal history.

Carry one to three special weapons in single-player campaign/free missions, preserving separate ammunition, reloads, HUD and selection. Multiplayer is disabled. Supported native build: Steam 9855922, Windows 10/11 x64.

Installation
1. Exit the game and extract the whole package. Use Steam > Manage > Browse local files to find Ace7Game.exe.
2. Back up the existing loader, ue4ss directory and same-named mount PAK. Preserve ue4ss\Mods\DualSP\profiles.json when upgrading.
3. If needed, run Runtimes\VC_redist.x64.exe and Runtimes\DirectX\DXSETUP.exe with all adjacent CAB files. Existing equal/newer runtimes need no uninstall.
4. Merge the contents of GameFiles into the game root. Do not copy the GameFiles folder itself. Mount PAK: Game\Content\Paks\~mods\AC7MultiSP_Mounts_P.pak.
5. If mods.txt contains other mods, preserve it and add/retain DualSP : 1. This package uses the AC7-specific UE4SS loader; review shared loader files before merging.
6. Launch through Steam. F6 opens management. No Python or compiler is needed. This update backs up and resets old settings and loadouts without migration; first launch creates revision 19 defaults.

Controls
• Choose SP1/SP2/SP3 in the native preparation pages; SP2/SP3 can be NO WEAPON. Unpurchased SPs are excluded from ordinary loadout selection; the lower-left panel shows the native description in the game language.
• Keys 1/2/3/4 select missile/SP1/SP2/SP3. D-pad left/up/right/down does the same and can be disabled separately. X retains native cycling.
• F6 or Back+Y opens General, Camera and Debug; logs expand within Debug. In missions, management borrows pause and releases only its own pause on closing.
• Mission/hangar model options affect visuals only. The mission-model shortcut defaults unbound. Disabling hangar models restores native previews and may reduce hangar stutter.
• Ctrl+Alt+F6 toggles Camera hold. Debug SP cycling defaults unbound. Ctrl+Alt+F5 toggles native-gun linkage to MGP/PLSL; HGP and true concurrent firing of different SP types are unsupported.
• Select Native/VR NEXT ranking in management; its shortcut is removed. Disable the mouse-flight mod's F2 HMD targeting when using VR ranking to avoid competing target selection. Dot, tracking and Camera hold are independent.
• Camera hold defaults off; L3/R3 camera actions are always retained. Release L3 to recenter and release latched tracking. Short R3 changes ordinary views; hold for 0.3 seconds to toggle/save Camera hold, without switching views on release. Both sticks retain countermeasures. Recenter binding and 5%–200% sensitivity are configurable. Angle-based auto recenter and its slider are removed.
• Latched tracking defaults off. With it enabled, hold Y for 0.2 seconds to latch/release; short Y selects a target while tracking follows. Tap the game's actual keyboard tracking binding to latch/release. The custom feature toggle defaults unbound and only enables/disables the feature. Pause, management, focus/target loss, special cameras and mission exit release tracking.
• Saving ordinary settings does not refill ammunition. Debug aircraft switching restarts the whole mission. The catalogue searches official/custom aircraft with complete three-SP data and uses current skin references; it does not grant purchases or DLC licenses.
• Mission 13 (113) retains its assigned SP, restoring multi-SP on leaving. Free flight 213 is unaffected. Missions 12/14 retain multi-SP.

Model support
Implemented layouts are not exhaustive per-aircraft/per-combination gameplay certification. Layout capacity, ammunition and visible/reloading models are separate. F-4E UGB+SASM alternates sides: nine bombs give 5+4, ten give 5+5, zero clears all.

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
F-14A | Implemented | Native mounting positions retained
F/A-18E | Implemented | Native mounting positions retained
F/A-18E TGM | Implemented | Native mounting positions retained
MiG-31B | Implemented | r3 rear SAAM pair moved 11 aft/2 down from native; both PLSL pods retained
F-14D | Implemented | Native SWP3 mechanism; native mounting positions retained
F/A-18F | Implemented | Native SWP3 mechanism; native mounting positions retained
Su-33 | Implemented | Native SWP3 mechanism; native mounting positions retained
F/A-18F Block III | Implemented | Native SWP3 mechanism; native mounting positions retained
F-14A TGM | Adapted |
Gripen E | Adapted |
Su-30M2 | Adapted |
Su-34 | Adapted |
MiG-29A | Adapted |
F-16XL | Adapted |
MiG-35D | Adapted |
F-22A | Partial | Retains side 8AAM models when selected; internal QAAM/XSDB use native switching.
Su-57 (standard) | Partial | Retains PLSL pods; internal 4AAM/GPB use native switching.
ADF-01 FALKEN | Adapted | 4AAM/FAEB share racks. Switches during TLS/trails are discarded; press again afterward.
DarkStar | Partial | Retains both PLSL guns across SP changes; SASM/XSDB follow native current-SP visuals.
ADF-11F | Adapted | QAAM+UAV and triple loadouts use two QAAM positions on the right and one UAV on the left; minor UAV/rail clipping remains.
CFA-44 | Adapted | ADMM+EML and triple loadouts split the sides; ECM models are hidden and out-of-mission visuals remain native.

Su-47, Su-57 TGM and X-02S retain multi-SP gameplay with native SP visuals. Ordinary Su-57 and TGM are distinct.

Known limits
Initial loading, loadout navigation and result-screen arrows may still stutter. F-2A model performance and occasional exits need their own evidence. The F-22 field-name lifetime workaround covers identified paths in this mod, not every loader/custom-aircraft problem. Acceptance is not an all-aircraft, all-combination, all-mod or fixed-performance guarantee. FALKEN switches are ignored during laser/trail activity; press again afterward. MGP/PLSL linkage may have slot-switch gaps.

The package contains no personal settings, saves, logs, dumps or third-party aircraft mods. Remove DualSP and its mount PAK to uninstall; remove shared loaders only when no other mods need them. SP2.wav is user-provided third-party audio with no additional rights granted. Microsoft runtimes remain unchanged; licenses are included.

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
2.22.0: Integrated NEXT modes, tracking/camera controls, independent model/D-pad options, loadout/mod compatibility, paused switching and additional SP layouts; simplified local installation integrity scope.
