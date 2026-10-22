AC7MultiSP 2.23.1 alpha2

2.23.1 alpha2 changes since alpha1
• Qualify mod UAV/HPM by actual native classes/callbacks and lifecycle contracts, including S-44 and Arkbird. Improve base-slot weapon names and detect external class changes before reading cached stock.
• Aircraft restart from a native pause menu waits across callbacks for native resume, up to three seconds. Resume/restart are each requested once; timeout or identity changes roll back safely.
• Retain settings/bindings, alpha1 cannon salvo and --- display, accepted mount assets and F-14IA hiding. Gameplay acceptance is pending.


Internal test candidate 2.23.1 alpha1 (since official 2.23.0)
• Skip redundant weapon probing during hangar browsing; inspect only existing, fully loaded default objects.
• F-14IA variants defer to native behavior. Catchable first-admission errors quarantine that aircraft for this session; other aircraft can recover. Existing F-14IA hiding remains; native third-party resource assertions are not guaranteed fixed.
• With an MGP/HGP/PLSL pod equipped, holding the gun fire button fires it together with the gun. Registered Adler A/B, XR302 and other mod cannon SPs are included. Multiple eligible cannon SPs in one loadout remain unlinked.
• Unlimited ammunition displays ---. Stock/reload rules and valid revision 19 settings, loadouts and bindings are retained.
• Build/install verification and user gameplay acceptance are separate. No public package or upload is created.

Delivery: built, all 11 existing checks passed, and 19 runtime files installed. All 77 mount variants/154 members and original revision 19 settings are verified. The game remains closed; gameplay acceptance is pending.

Historical release notes follow.

Changes since 2.22.1
• Commands/results, the catalogue, UI state and action queues now pass through memory. Consecutive pending direct selections are coalesced; duplicate JSON work, weapon-pool copies and HUD sampling are reduced.
• Logs use bounded background queues and batching. Unchanged settings skip saving; validated short selection audio is retained across missions.
• Fixed inactive ADMM pool slots with unlimited ammunition, cached reload markers and checkpoint/mode changes. Native projectile creation and continuous-effect lifetimes remain authoritative.
• Ignore verified inactive bomb remnants when changing combinations; resupply continues to list purchased weapons only.
• Added qualified mod electronic/unlimited-ammo support, SP debug controls, management refinements and CFA startup prewarming/loading feedback. Management muting has been removed.
• CFA44 with three SPs and models enabled: median core switching time 22.74→17.66 ms (about 22% lower), P95 25.87→20.20 ms. One 88.31 ms spike remains. These are uncontrolled log comparisons, not frame-rate guarantees.
• User-accepted alpha1 r3 is the functional baseline. Main is now 2.23.0; installed binaries retain their accepted alpha label. Formal artifacts and packaging are separate delivery steps.

Changes since official 2.22.0
• Three management tabs: General, Camera and Debug; logs expand in Debug. Controller and keyboard tracking have separate options, both off by default.
• Ctrl+Alt+F6 toggles Camera hold. SP-combination cycling and mission-model shortcuts default unbound and can be set in the configuration. NEXT mode is selected in management.
• New settings default to Gun salvo on, center dot off and weapon text for one second after switching. Camera hold defaults off; L3/R3 actions remain available.
• Revision 19 does not migrate older settings/loadouts: back up their original bytes before resetting to new defaults. Valid revision 19 settings are retained normally.
• Fixed the F/A-18E TGM cooked Socket reference with r2 assets. All 77 variants/154 members are bound to offline qualification; the other 152 members and 28-aircraft/196 layouts are unchanged.

2.22.1 historical record: The user accepted that installed candidate. That release used version 2.22.1, retaining accepted core/resource bytes and changing only equal-length UI version text. Research identifiers are not official release numbers.

Carry one to three special weapons in single-player campaign/free missions, preserving separate ammunition, reloads, HUD and selection. Multiplayer is disabled. Supported native build: Steam 9855922, Windows 10/11 x64.

Installation
1. Exit the game and extract the whole package. Use Steam > Manage > Browse local files to find Ace7Game.exe.
2. Back up the existing loader, ue4ss directory and same-named mount PAK. Preserve ue4ss\Mods\DualSP\profiles.json when upgrading.
3. If needed, run Runtimes\VC_redist.x64.exe and Runtimes\DirectX\DXSETUP.exe with all adjacent CAB files. Existing equal/newer runtimes need no uninstall.
4. Merge the contents of GameFiles into the game root. Do not copy the GameFiles folder itself. Mount PAK: Game\Content\Paks\~mods\AC7MultiSP_Mounts_P.pak.
5. If mods.txt contains other mods, preserve it and add/retain DualSP : 1. This package uses the AC7-specific UE4SS loader; review shared loader files before merging.
6. Launch through Steam. F6 opens management. No Python or compiler is needed. Older settings/loadouts are backed up and reset to revision 19 without migration. Valid revision 19 settings are retained.

Controls
• New settings default to Gun salvo on, center dot off and weapon text for one second after switching; controller/keyboard tracking and Camera hold each default off.
• Choose SP1/SP2/SP3 in the native preparation pages; SP2/SP3 can be NO WEAPON. Unpurchased SPs are excluded from ordinary loadout selection; the lower-left panel shows the native description in the game language.
• Keys 1/2/3/4 select missile/SP1/SP2/SP3. D-pad left/up/right/down does the same and can be disabled separately. X retains native cycling.
• F6 or Back+Y opens General, Camera and Debug; logs expand within Debug. In missions, management borrows pause and releases only its own pause on closing.
• Mission/hangar model options affect visuals only. The mission-model shortcut defaults unbound. Disabling hangar models restores native previews and may reduce hangar stutter.
• Ctrl+Alt+F6 toggles Camera hold. Debug SP cycling defaults unbound. Ctrl+Alt+F5 toggles native-gun linkage to registered MGP/HGP/PLSL pods; loadouts with multiple eligible cannon SPs remain unlinked.
• Select Native/VR NEXT ranking in management; its shortcut is removed. Disable the mouse-flight mod's F2 HMD targeting when using VR ranking to avoid competing target selection. Dot, tracking and Camera hold are independent.
• Camera hold defaults off; L3/R3 camera actions are always retained. Release L3 to recenter and release latched tracking. Short R3 changes ordinary views; hold for 0.3 seconds to toggle/save Camera hold, without switching views on release. Both sticks retain countermeasures. Recenter binding and 5%–200% sensitivity are configurable. Angle-based auto recenter and its slider are removed.
• Controller and keyboard latched tracking each default off. Enable the corresponding option, then hold Y for 0.2 seconds to latch/release, or tap the game's actual keyboard tracking binding. Short Y selects a target while tracking follows. The separate feature-toggle shortcut is removed. Pause, management, focus/target loss, special cameras and mission exit release tracking.
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
2.22.1: Refined three-tab management and separate tracking options, shortcuts and defaults; back up/reset older settings; fixed F/A-18E TGM references and offline asset qualification.
2.23.0: Moved runtime IPC to memory, added background logging and reduced repeated sampling; fixed unlimited ADMM queues and inactive-bomb blocking, refined mod compatibility/debug/management, and removed management muting. Comparable CFA median switching time fell by about 22%.
2.23.1 alpha1: Hangar admission/fallback protection, registered mod cannon-SP salvo support and unlimited-ammo display ---.
