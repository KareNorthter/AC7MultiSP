AC7MultiSP 2.24.1 — Official release

Changes since 2.23.1
• Adds bilingual Version and updates, author contacts, manual checking and Lanzou/R2 browser download links.
• Fixes Backspace triggering native pause in management search and explains when an aircraft has no model layout.
• Places transient notices at the top center with the ACMultiSP: prefix and stacking; preparing manifests show that update information is not yet published.
• Retains valid revision 19 settings, bindings, profiles and existing assets. DirectX/Visual C++ runtimes are not bundled.

This guide describes official 2.24.1. The update check and both download workflows still need later in-game acceptance.

Carry one to three special weapons in single-player campaign/free missions, preserving separate ammunition, reloads, HUD and selection. Multiplayer is disabled. Supported native build: Steam 9855922, Windows 10/11 x64. Other game builds and native VR gameplay are outside the compatibility guarantee.

Installation and configuration reference: 安装说明.txt. Delivery methods will be revised separately.

Controls
• New settings enable the mod, mission models, hangar models, D-pad selection and Gun salvo. Center dot, controller/keyboard tracking and Camera hold default off; weapon text appears for one second after switching and NEXT uses native ranking. The interface supports Chinese and English, with automatic language selection by default. Valid revision 19 settings retain personal choices.
• Each SP retains its own stock and reload state. A single SP uses full ammunition; two SPs default to 50% each and three to 33% each, adjustable in General. Native unlimited stock displays ---.
• Save panel changes before closing; closing discards unsaved edits. Language and bindings apply after saving, the master switch waits for a safe menu, and ammunition ratios apply at the next loadout allocation.
• Choose SP1/SP2/SP3 in the native preparation pages; SP2/SP3 can be NO WEAPON. Unpurchased SPs are excluded from ordinary loadout selection; the lower-left panel shows the native description in the game language.
• Keys 1/2/3/4 select missile/SP1/SP2/SP3. D-pad left/up/right/down does the same and can be disabled separately. X retains native cycling. Single-key keyboard/mouse bindings work while modifiers are held; explicitly configured complete shortcut chords take priority.
• F6 or Back+Y opens four tabs: General, Camera, Debug, and Version and updates; logs expand within Debug. In missions, management borrows pause and releases only its own pause on closing.
• Mission/hangar model options are independent and affect visuals only. The mission-model shortcut defaults unbound. Disabling hangar models restores native previews and may reduce hangar stutter.
• Ctrl+Alt+F6 toggles Camera hold. Debug SP cycling defaults unbound. Ctrl+Alt+F5 toggles native-gun linkage to registered MGP/HGP/PLSL pods; loadouts with multiple eligible cannon SPs remain unlinked.
• Select Native/VR NEXT ranking in management; its shortcut is removed. Disable the mouse-flight mod's F2 HMD targeting when using VR ranking to avoid competing target selection. Dot, tracking and Camera hold are independent.
• Camera hold defaults off; L3/R3 camera actions are always retained. Release L3 to recenter and release latched tracking. Short R3 changes ordinary views; hold for 0.3 seconds to toggle/save Camera hold, without switching views on release. Both sticks retain countermeasures. Recenter defaults to the middle mouse button and is configurable; sensitivity defaults to 100%, adjustable from 5% to 200%. Angle-based auto recenter and its slider are removed.
• Controller and keyboard latched tracking each default off. Enable the corresponding option, then hold Y for 0.2 seconds to latch/release, or tap the game's actual keyboard tracking binding. Short Y selects a target while tracking follows. The separate feature-toggle shortcut is removed. Pause, management, focus/target loss, special cameras and mission exit release tracking.
• Debug can temporarily change SP loadouts and refill SP ammunition. Infinite ammo and Fast reload default off, last only for the current game session and affect only SPs managed by this mod. Infinite ammo does not refill stock; Fast reload reduces eligible SP waits to at most one second, excluding cannon and continuous-effect SPs.
• Saving ordinary settings does not refill ammunition. Debug aircraft switching restarts the whole mission. The catalogue searches official/custom aircraft with complete three-SP data and uses current skin references; it does not grant purchases or DLC licenses.
• Mission 13 (113) retains its assigned SP, restoring multi-SP on leaving. Free flight 213 is unaffected. Missions 12/14 retain multi-SP.
• Version and updates shows the author, email karenorthterofchina@gmail.com, test-feedback QQ group 1072924682 and licenses. Automatic checking runs once after the first main menu; Manual check can retry a failure and is disabled while checking. New-version notices appear outside missions. Clicking Lanzou or R2 opens the default Windows browser; exit the game and follow the downloaded package instructions. Preparing manifests show that update information is not yet published. Check failures do not interrupt weapon control.
• Transient notices appear at the top center with the ACMultiSP: prefix and stack when simultaneous. Ordinary notices last about one second; warnings, errors and important notices about three seconds. Aircraft without a model layout receive an explanatory notice and can still use multi-SP weapons.

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
Initial loading, loadout navigation and result-screen arrows may still stutter. F-2A model performance and occasional exits need their own evidence. The F-22 field-name lifetime workaround covers identified paths in this mod, not every loader/custom-aircraft problem. Existing F-14IA hiding remains; its native resource-dependency crash is not listed as fixed. Acceptance is not an all-aircraft, all-combination, all-mod or fixed-performance guarantee. FALKEN switches are ignored during laser/trail activity; press again afterward. MGP/PLSL linkage may have slot-switch gaps.

The package contains no personal settings, saves, logs, dumps or third-party aircraft mods. Remove DualSP and its mount PAK to uninstall; remove shared loaders only when no other mods need them. SP2.wav is user-provided third-party audio with no additional rights granted. DirectX/Visual C++ runtimes are not bundled; mod/dependency licenses are included.

The complete version history is maintained once in the repository: docs/releases/版本更新说明.md. This guide contains the current summary and controls; future delivery summaries should be generated from the shared history.
