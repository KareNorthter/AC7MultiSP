#pragma once
// 在overlay的私有命名空间内使用；只绘制草稿、帮助和状态，不操作游戏对象。
void panelHelp(const char* zh,const char* en) {
    ImGui::SameLine();ImGui::PushID(zh);
    const bool clicked=ImGui::SmallButton("?");
    if((ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)||ImGui::IsItemFocused()) && ImGui::BeginTooltip()) {
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX()+std::min(520.f,ImGui::GetIO().DisplaySize.x-80));
        ImGui::TextUnformatted(tr(zh,en));ImGui::PopTextWrapPos();ImGui::EndTooltip();
    }
    if(clicked)ImGui::OpenPopup("Help"); // 键盘/手柄也能打开说明，不只依赖鼠标悬停。
    if(ImGui::BeginPopup("Help")) {
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX()+std::min(520.f,ImGui::GetIO().DisplaySize.x-80));
        ImGui::TextUnformatted(tr(zh,en));ImGui::PopTextWrapPos();ImGui::EndPopup();
    }
    ImGui::PopID();
}
bool panelCard(const char* id,const char* title) {
    const bool visible=ImGui::BeginChild(id,{0,0},ImGuiChildFlags_Borders|ImGuiChildFlags_AutoResizeY);
    if(visible)ImGui::SeparatorText(title);
    return visible; // 无论可见与否，调用方都须EndChild。
}
void bindingButton(int slot,const std::string& label,float width=210) {
    ImGui::PushID(slot);
    const auto text=captureSlot==slot?std::string(tr("请按键…","Press a key...")):label;
    if(ImGui::Button((text+"###Binding").c_str(),{std::min(width,ImGui::GetContentRegionAvail().x),0})) {captureSlot=slot;notice.clear();}
    ImGui::PopID();
}
bool managementDirty() {
    return draftHangarMountEnabled!=state.at("hangar_mount_enabled").get<bool>() || draftDpadWeaponSwitchEnabled!=state.at("dpad_weapon_switch_enabled").get<bool>() || draftGunSalvo!=state.at("gun_salvo_enabled").get<bool>() || draftViewTargetMode!=state.at("view_target_mode").get<std::string>() || draftViewCenterDot!=state.at("view_center_dot_enabled").get<bool>() || draftCentralWeaponDisplay!=state.at("central_weapon_display").get<std::string>() || draftModEnabled!=state.at("mod_enabled").get<bool>() || draftTaskMountEnabled!=state.at("task_mount_enabled").get<bool>() ||
        draftTargetGazeEnabled!=state.at("target_gaze_enabled").get<bool>() || draftCameraHold!=state.at("camera_hold_enabled").get<bool>() ||
        draftCameraR3!=state.at("camera_r3_enabled").get<bool>() || draftTargetGazeKey!=state.at("target_gaze_key").get<int>() ||
        draftCameraCenterKey!=state.at("camera_center_key").get<int>() || draftCameraSensitivity!=state.at("camera_sensitivity").get<int>() ||
        draftAmmoPercentTwo!=state.at("ammo_percent_two").get<int>() || draftAmmoPercentThree!=state.at("ammo_percent_three").get<int>() ||
        draftLanguage!=state.at("language").get<std::string>() || Json(draftKeys)!=state.at("keyboard_keys") || shortcutData(draftShortcuts)!=state.at("shortcuts");
}
void drawBasicBindings();
void drawFeatureSettings() {
    if(panelCard("MainOptions",tr("飞行设置","Flight settings"))) {
        ImGui::Checkbox(tr("启用模组###ModEnabled","Enable mod###ModEnabled"),&draftModEnabled);
        panelHelp("总开关在主菜单或单人任务选择页应用；在任务/机库保存后，回到这些菜单自动生效，无需重启。管理入口始终可用。",
            "The master switch applies at the main menu or single-player mission selection. Changes saved during a mission or in the hangar apply on returning there. No restart is needed; management stays available.");
        ImGui::Checkbox(tr("机炮齐射###GunSalvo","Gun salvo###GunSalvo"),&draftGunSalvo);
        if(state.contains("gun_salvo")&&state.at("gun_salvo").value("reason",std::string())=="multiple_supported_sp")
            ImGui::TextWrapped("%s",tr("当前方案含多种合格SP，本候选不支持它们同时发射。","This loadout has multiple eligible SPs; this candidate cannot fire them simultaneously."));
        panelHelp("默认关闭。按住实际原生机枪键联动方案中的MGP或PLSL，松开后恢复先前选择；自定义机枪键同样生效。暂停、管理页、失焦或人工换武器会取消联动。不支持HGP及多种不同SP并发。",
            "Off by default. Hold your native gun action to also fire the selected loadout’s MGP or PLSL; release to restore the prior selection. Native remapping is respected. Pause, management, focus loss or manual weapon selection cancels the action. HGP and simultaneous different SPs are unsupported.");
        if(state.at("mod_enabled")!=state.at("runtime_mod_enabled"))ImGui::TextColored({1,.76f,.38f,1},"%s",tr("已保存，返回主菜单后应用总开关。","Saved; return to the main menu to apply the master switch."));
        if(state.contains("master_switch")&&state.at("master_switch").contains("error"))
            ImGui::TextWrapped("%s",uiMessage(state.at("master_switch").at("error").get<std::string>(),"总开关应用失败，请查看日志。","The master switch could not be applied; see Logs.").c_str());
    }
    ImGui::EndChild();
    if(panelCard("ModelOptions",tr("模型显示","Model display"))) {
        ImGui::TextWrapped("%s",tr("两个开关独立生效，只影响模型显示，不影响多SP选择、切换、弹药或注视功能。",
            "These switches are independent and affect models only, not SP selection, switching, ammunition or target tracking."));
        ImGui::Checkbox(tr("任务中显示多SP模型###TaskMount","Show multi-SP models in missions###TaskMount"),&draftTaskMountEnabled);
        panelHelp("默认开启，控制飞行、补给及补给过渡中的多SP模型；部分内舱保持原生。下次出击、任务/检查点重试、补给或调试配装时应用。模型快捷键仅切换这一项，不影响机库。",
            "On by default. Controls multi-SP models in flight, resupply and resupply transitions; some internal bays remain native. Applies at the next sortie, mission/checkpoint retry, resupply or debug loadout. The model shortcut changes this option only, leaving hangar models unchanged.");
        ImGui::Checkbox(tr("机库中显示多SP模型###HangarMount","Show multi-SP models in the hangar###HangarMount"),&draftHangarMountEnabled);
        ImGui::TextWrapped("%s",tr("若关闭，则机库中展示游戏原本的机体和SP模型，可改善机库中的低帧卡顿。",
            "When disabled, the hangar shows the game's original aircraft and SP models, which may improve low frame rates and stuttering in the hangar."));
        if(state.at("task_mount_enabled")!=state.at("runtime_task_mount_enabled"))ImGui::TextColored({1,.76f,.38f,1},"%s",tr("任务模型设置将在下次配装时应用。","Mission model settings apply with the next loadout."));
        if(state.contains("mount_warning"))ImGui::TextWrapped("%s",uiMessage(state.at("mount_warning").get<std::string>(),"挂载外观已回退，请查看日志。","Models reverted to native visuals; see Logs.").c_str());
    }
    ImGui::EndChild();
    if(panelCard("HudOptions",tr("中央武器简称","Central weapon abbreviation"))) {
        int display=draftCentralWeaponDisplay=="always"?0:1;
        ImGui::SetNextItemWidth(std::min(340.f,ImGui::GetContentRegionAvail().x*.65f));
        if(ImGui::Combo("##CentralWeaponDisplay",&display,tr("始终显示\0切换武器后显示1秒\0","Always visible\0Show for 1 second after switching\0")))
            draftCentralWeaponDisplay=display==0?"always":"switch_1s";
        panelHelp("默认始终显示，保存后立即生效。短显只在实际切换成功后计时1秒，重复选择不延长，暂停不计时；隐藏最多约100毫秒刷新误差，右下HUD持续保留。",
            "Always visible by default; saving applies immediately. Timed mode shows for 1 second after an actual switch; reselecting does not extend it and pauses do not count. Hiding can lag by one 100 ms update; lower-right weapon information stays available.");
    }
    ImGui::EndChild();
    if(panelCard("AmmoOptions",tr("SP弹药量","SP ammunition"))) {
        ImGui::SetNextItemWidth(std::min(340.f,ImGui::GetContentRegionAvail().x*.55f));
        ImGui::SliderInt(tr("双SP，各自###AmmoTwo","Two SPs, each###AmmoTwo"),&draftAmmoPercentTwo,50,100,"%d%%",ImGuiSliderFlags_AlwaysClamp);
        panelHelp("默认双SP各50%、三SP各33%，单SP为100%。先计部件再按比例向上取整；下次出击、补给或调试配弹时生效，保存不会立即补弹。",
            "Defaults: 50% each with two SPs, 33% with three, 100% with one. Parts apply first, then quantities round up. Applies at the next sortie, resupply or debug loadout; saving does not refill current ammunition.");
        ImGui::SetNextItemWidth(std::min(340.f,ImGui::GetContentRegionAvail().x*.55f));
        ImGui::SliderInt(tr("三SP，各自###AmmoThree","Three SPs, each###AmmoThree"),&draftAmmoPercentThree,33,100,"%d%%",ImGuiSliderFlags_AlwaysClamp);
    }
    ImGui::EndChild();
    drawBasicBindings(); // 基础设置、兵装键位和模组快捷键集中在第一页。
    if(ImGui::CollapsingHeader(tr("使用提示###FeatureHelp","Quick guide###FeatureHelp"))) {
        ImGui::BulletText("%s",tr("机库直接选择1至3种SP，任务中用X轮切；启用十字键后可直选。","Select 1–3 SPs in the hangar; cycle with X or enable D-pad selection."));
        ImGui::BulletText("%s",tr("中央兵装提示及右下HUD跟随游戏HUD显隐。","Weapon hints and the lower-right HUD follow game HUD visibility."));
        ImGui::BulletText("%s",tr("视角保持、锁定注视及相关键位在“视角”页。","Find camera hold, toggle tracking and their bindings in Camera."));
    }
}
void drawCameraSettings() {
    if(panelCard("ViewTargetOptions",tr("NEXT选目标模式","NEXT targeting mode"))) {
        if(ImGui::RadioButton(tr("普通模式：机头附近优先###NextNormal","Normal: near aircraft nose###NextNormal"),draftViewTargetMode=="normal"))draftViewTargetMode="normal";
        if(ImGui::RadioButton(tr("VR模式：视角中心优先###NextVR","VR: near view center###NextVR"),draftViewTargetMode=="vr"))draftViewTargetMode="vr";
        panelHelp("默认普通模式。普通模式按机头附近优先，VR模式按当前视野中心优先；这里只切换NEXT的排序参考，不会启用VR设备。保留原生候选、目标资格和近期历史，按原生切目标键才选中。仅管理界面或默认Ctrl+Alt+F8切换，可在基础页改键；L3不切模式。可能与鼠标飞控F2 HMD功能冲突，使用VR模式时请关闭该HMD。",
            "Normal mode is the default and prioritizes the aircraft nose. VR mode prioritizes the current view center; it changes NEXT ranking without enabling VR hardware. Native candidates, eligibility and history remain; use the native target key to select. Switch here or with Ctrl+Alt+F8, rebindable in General. L3 never changes mode. May conflict with the mouse-flight mod's F2 HMD; disable that HMD for VR mode.");
        ImGui::Checkbox(tr("显示视野中心白点###ViewCenterDot","Show view-center dot###ViewCenterDot"),&draftViewCenterDot);
        panelHelp("白点默认开启，与NEXT模式和视角保持独立。镜头偏离机头超过5°时显示放大的荧光白点，5°以内及边界隐藏。跟随游戏HUD显隐；暂停、管理、失焦及特殊镜头时隐藏。",
            "On by default, independent of NEXT mode and camera hold. The enlarged glowing dot appears beyond 5 degrees from the aircraft nose and is hidden at or within 5 degrees. Follows the game HUD; hidden during pause, management, focus loss or special cameras.");
    }
    ImGui::EndChild();
    if(panelCard("TrackingOptions",tr("锁定注视目标","Latched target tracking"))) {
        ImGui::TextWrapped("%s",tr("让镜头持续跟随当前选中的目标，按一次保持、再按一次停止，无需一直按住；不会自动更换目标或修改武器锁定规则。",
            "Keeps the camera following the selected target. Press once to start and again to stop, without holding the key. It does not select another target or modify weapon lock-on rules."));
        ImGui::Checkbox(tr("启用锁定注视目标###TargetGazeEnabled","Enable latched target tracking###TargetGazeEnabled"),&draftTargetGazeEnabled);
        panelHelp("默认关闭。启用后Y按满0.2秒开始持续注视，再长按一次结束；按住只触发一次，松开不切目标。短按Y仍切目标，持续注视随新目标移动。键盘沿用游戏实际绑定的注视目标键，按一下开始、再按一下结束；原生切目标键保持。关闭后恢复按住才注视。L3不切注视，R3长按只切视角保持。暂停、管理、失焦、目标消失或离场时结束注视。",
            "Off by default. Hold Y for 0.2 seconds to latch tracking, then hold again to release. Fires once per hold; release does not select another target. Short Y presses switch targets while tracking follows. The game's actual keyboard tracking binding toggles tracking on each press; the native selection binding remains. Disabled restores hold-to-track. L3 never toggles tracking; long R3 only toggles camera hold. Tracking stops on pause, management, focus loss, target loss or mission exit.");
        ImGui::TextUnformatted(tr("功能启闭快捷键","Feature toggle shortcut"));ImGui::SameLine();
        bindingButton(7,draftTargetGazeKey?keyLabel(draftTargetGazeKey):tr("未绑定","Unassigned"),180);
        ImGui::SameLine();if(ImGui::SmallButton(tr("清除###ClearGazeKey","Clear###ClearGazeKey"))){draftTargetGazeKey=0;captureSlot=-1;}
        panelHelp("此自定义键只切换本功能是否启用并保存，不直接开始注视；请与游戏原生注视键使用不同按键。默认未绑定，功能默认关闭。Shift/Ctrl/Alt可同时按住，已设置的完整组合快捷键优先。",
            "This shortcut only enables/disables and saves the feature; it does not start tracking. Use a different key from the game's tracking binding. Unassigned by default; the feature starts disabled. Works with Shift/Ctrl/Alt held, while exact configured mod chords take priority.");
    }
    ImGui::EndChild();
    if(panelCard("CameraOptions",tr("视角保持","Camera hold"))) {
        ImGui::Checkbox(tr("启用视角保持###CameraHold","Keep camera direction###CameraHold"),&draftCameraHold);
        panelHelp("默认关闭。启用后松开摇杆/视点模式键、短按切目标或切换普通视角可保持方向；注视从当前方向衔接，停止后返回保持方向。关闭后恢复原生自动回正。暂停、管理、失焦和特殊镜头期间归还原生。",
            "Off by default. When enabled, retains direction after releasing the stick/viewpoint mode key, briefly selecting a target or changing ordinary views. Tracking starts from and returns to that direction. Disabling restores native recentering. Pause, management, focus loss and special cameras restore native behavior.");
        ImGui::Checkbox(tr("手柄镜头快捷操作###CameraR3","Controller camera shortcuts###CameraR3"),&draftCameraR3);
        panelHelp("默认开启，视角保持关闭时也可操作。L3单独按下并松开：视角回正并结束锁定注视。R3短按松开：原生切换第一/第三人称等普通视角；R3按满0.3秒：切换视角保持并保存，松开不再换视角，也不回正。按住不重复。双摇杆组合只释放干扰弹。关闭此项则摇杆按键使用原生功能。",
            "On by default, including while camera hold is off. Press and release L3 alone to recenter and release latched tracking. Short R3 switches native ordinary views; hold R3 for 0.3 seconds to toggle and save camera hold without view switching or recentering on release. No repeats while held. Both sticks only release countermeasures. Disabling restores native stick clicks.");
        if(!draftCameraHold&&captureSlot==8)captureSlot=-1;
        if(draftCameraHold) {
            ImGui::TextUnformatted(tr("独立回中键","Recenter shortcut"));ImGui::SameLine();
            bindingButton(8,draftCameraCenterKey?keyLabel(draftCameraCenterKey):tr("未绑定","Unassigned"),180);
            ImGui::SameLine();if(ImGui::SmallButton(tr("中键###ResetCenterKey","Middle mouse###ResetCenterKey"))){draftCameraCenterKey=VK_MBUTTON;captureSlot=-1;}
            ImGui::SameLine();if(ImGui::SmallButton(tr("清除###ClearCenterKey","Clear###ClearCenterKey"))){draftCameraCenterKey=0;captureSlot=-1;}
            panelHelp("点击后按单键或鼠标中键，Esc取消；按下回中、按住不重复。可同时按住Shift/Ctrl/Alt，已配置组合优先；与原生换视角请用不同键。",
                "Click, then press a key or middle mouse; Esc cancels. Recenters once on press, including while Shift/Ctrl/Alt is held; exact configured chords take priority. Use a different key for native view switching.");
            ImGui::SetNextItemWidth(std::min(340.f,ImGui::GetContentRegionAvail().x*.55f));
            ImGui::SliderInt(tr("摇杆灵敏度###CameraSensitivity","Stick sensitivity###CameraSensitivity"),&draftCameraSensitivity,5,200,"%d%%",ImGuiSliderFlags_AlwaysClamp);
            panelHelp("默认100%，范围5%至200%。右摇杆按偏转量控制转动速度，低延迟平滑加速和转向，松开后短暂平滑收尾再保持方向；不扩大观察范围。回中、注视和单独键鼠操作沿用原有响应；座舱仍受原生边界和动画影响。",
                "Default 100%, range 5%–200%. Stick deflection controls turn speed with low-latency smooth acceleration and direction changes; release briefly eases to a stop, then holds direction. View limits stay unchanged. Recentering, tracking and keyboard/mouse-only look retain their existing response; cockpit output retains native limits and animation.");
        }
    }
    ImGui::EndChild();
    ImGui::TextDisabled("%s",tr("L3 + R3：干扰弹","L3 + R3: countermeasures"));
}
void drawBasicBindings() {
    if(ImGui::CollapsingHeader(tr("武器直选键###DirectKeys","Direct weapon keys###DirectKeys"),ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox(tr("手柄十字键切换武器###DpadWeaponSwitch","Switch weapons with the controller D-pad###DpadWeaponSwitch"),&draftDpadWeaponSwitchEnabled);
        panelHelp("默认开启。关闭并保存后，模组不响应十字键切换武器；键盘直选、X原生轮切和管理菜单导航保留。",
            "On by default. Disable and save to stop mod weapon selection with the D-pad. Keyboard selection, native X cycling and menu navigation remain available.");
        ImGui::TextDisabled("%s",tr("X：原生轮切兵装","X: native weapon cycling"));
        if(ImGui::BeginTable("DirectBindings",3,ImGuiTableFlags_SizingStretchProp|ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn(tr("兵装","Weapon"));ImGui::TableSetupColumn(tr("键盘","Keyboard"));ImGui::TableSetupColumn(tr("手柄","Controller"));ImGui::TableHeadersRow();
            const char* weapons[]={"MSL","SP1","SP2","SP3"};
            const char* pads[]={tr("十字键左","D-pad Left"),tr("十字键上","D-pad Up"),tr("十字键右","D-pad Right"),tr("十字键下","D-pad Down")};
            for(int i=0;i<4;++i) {
                ImGui::TableNextRow();ImGui::TableNextColumn();ImGui::TextUnformatted(weapons[i]);
                ImGui::TableNextColumn();bindingButton(i,keyLabel(draftKeys[i]),150);
                ImGui::TableNextColumn();if(draftDpadWeaponSwitchEnabled)ImGui::TextUnformatted(pads[i]);else ImGui::TextDisabled("%s",tr("未占用","Not assigned"));
            }
            ImGui::EndTable();
        }
        if(ImGui::SmallButton(tr("恢复1 / 2 / 3 / 4###ResetWeaponKeys","Reset 1 / 2 / 3 / 4###ResetWeaponKeys"))) {
            const std::array<int,4> defaults{49,50,51,52};
            if(dualsp::validShortcuts(draftShortcuts,defaults)&&dualsp::validTargetGazeKey(draftTargetGazeKey,draftShortcuts,defaults)&&
                dualsp::validCameraCenterKey(draftCameraCenterKey,draftCameraHold,draftTargetGazeKey,draftShortcuts,defaults)){draftKeys=defaults;captureSlot=-1;notice.clear();}
            else notice=tr("默认武器键与当前键位重复，请先调整。","Default weapon keys conflict with a current binding.");
        }
        panelHelp("点击后按新键，Esc取消；手柄十字键固定。按住Shift/Ctrl/Alt也可直选，明确设置的组合快捷键优先。未搭载或耗尽的SP不切换，装填中可选。",
            "Click, then press a new key; Esc cancels. D-pad bindings are fixed. Direct selection works while Shift/Ctrl/Alt is held; exact configured mod chords take priority. Empty or depleted SPs cannot be selected; reloading weapons can.");
    }
    if(ImGui::CollapsingHeader(tr("模组快捷键###ModKeys","Mod shortcuts###ModKeys"),ImGuiTreeNodeFlags_DefaultOpen)) {
        const char* names[]={tr("管理界面","Management"),tr("轮切SP组合","Cycle SP loadouts"),tr("切换任务模型","Toggle mission models"),tr("机炮齐射","Gun salvo"),tr("NEXT选目标模式","NEXT targeting mode")};
        for(int i=0;i<5;++i) {
            ImGui::TextUnformatted(names[i]);ImGui::SameLine(std::min(260.f,ImGui::GetContentRegionAvail().x*.45f));
            bindingButton(i==4?10:(i==3?9:i+4),shortcutLabel(draftShortcuts[i]));
            if(i>=3){ImGui::SameLine();ImGui::PushID(i);if(ImGui::SmallButton(tr("清除###ClearOptionalShortcut","Clear###ClearOptionalShortcut"))){draftShortcuts[i]={0,0};captureSlot=-1;}ImGui::PopID();}
        }
        if(ImGui::SmallButton(tr("恢复默认快捷键###ResetShortcuts","Reset shortcuts###ResetShortcuts"))) {
            const std::array<dualsp::Shortcut,5> defaults{{{VK_F6,0},{VK_F6,3},{VK_F7,3},{VK_F5,3},{VK_F8,3}}};
            if(dualsp::validShortcuts(defaults,draftKeys)&&dualsp::validTargetGazeKey(draftTargetGazeKey,defaults,draftKeys)&&
                dualsp::validCameraCenterKey(draftCameraCenterKey,draftCameraHold,draftTargetGazeKey,defaults,draftKeys)){draftShortcuts=defaults;captureSlot=-1;notice.clear();}
            else notice=tr("默认快捷键与当前键位重复，请先调整。","Default shortcuts conflict with a current binding.");
        }
        panelHelp("点击后按单键或Ctrl/Alt/Shift组合。运行时先匹配完整组合，再匹配单键，只执行一个Mod动作。Tab选项，Enter操作，Ctrl+Tab切页，Esc返回；Back+Y开关管理界面。",
            "Click, then press a key or Ctrl/Alt/Shift chord. Exact chords take priority over single-key actions; one mod action per press. Tab navigates, Enter activates, Ctrl+Tab changes pages, Esc goes back; Back+Y toggles management.");
    }
}
void drawManagementErrors() {
    const char* fields[]={"error","menu_error","hud_error","central_error","settings_error"};
    bool errors=false;for(const char* field:fields)errors=errors||state.contains(field);
    for(const char* field:{"target_gaze","camera_hold","management_pause","gun_salvo","view_target"})errors=errors||(state.contains(field)&&state.at(field).contains("error"));
    if(errors) {
        ImGui::TextColored({1,.65f,.4f,1},"%s",tr("有功能或保存异常，请展开查看。","A feature or save operation needs attention."));
        if(ImGui::CollapsingHeader(tr("查看异常详情###ManagementErrors","View error details###ManagementErrors"))) {
            for(const char* field:fields)if(state.contains(field))ImGui::TextWrapped("%s",uiMessage(state.at(field).get<std::string>()).c_str());
            for(const char* field:{"target_gaze","camera_hold","management_pause","gun_salvo","view_target"})if(state.contains(field)&&state.at(field).contains("error"))
                ImGui::TextWrapped("%s",uiMessage(state.at(field).at("error").get<std::string>()).c_str());
        }
    }
}
void drawPanel() {
    const auto display=ImGui::GetIO().DisplaySize;
    const float width=std::min(1080.f,std::max(280.f,display.x-32)),height=std::min(830.f,std::max(240.f,display.y-32));
    ImGui::SetNextWindowPos({display.x*.5f,display.y*.5f},ImGuiCond_Always,{.5f,.5f});ImGui::SetNextWindowSize({width,height});
    ImGui::Begin("AC7MultiSP###DualSPManagement",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoSavedSettings);
    ImGui::TextColored({.37f,.84f,.79f,1},"AC7MultiSP");ImGui::SameLine();ImGui::TextDisabled("2.22.0");
    if(state.value("management_protocol",0)!=managementProtocol||!managementError.empty()) {
        ImGui::TextWrapped("%s",tr("管理组件版本不匹配，请完整安装同一版本。","Management components do not match. Install a complete matching version."));
        if(!managementError.empty())ImGui::TextWrapped("%s",uiMessage(managementError).c_str());
        if(ImGui::Button(tr("关闭","Close")))togglePanel(false);
        ImGui::End();return;
    }
    ImGui::SameLine();ImGui::TextDisabled("%s",tr("飞行控制中心","Flight controls"));
    if(width>700)ImGui::SameLine(width-310);
    int language=draftLanguage=="auto"?0:draftLanguage=="zh"?1:2;
    ImGui::SetNextItemWidth(210);
    if(focusManagement){ImGui::SetKeyboardFocusHere();focusManagement=false;}
    if(ImGui::Combo("##Language",&language,tr("跟随游戏\0中文\0English\0","Game language\0Chinese\0English\0")))draftLanguage=language==0?"auto":language==1?"zh":"en";
    if(state.contains("management_pause")) {
        const auto& pause=state.at("management_pause");
        ImGui::TextDisabled("%s",pause.value("existing",false)?tr("游戏暂停中","Game is paused"):pause.value("owned",false)?tr("关闭管理后继续飞行","Close to resume flight"):tr("设置将在保存后应用","Save to apply settings"));
        panelHelp("管理暂停在关闭后自动解除；打开前已有的游戏暂停会保留。Tab移动焦点，Ctrl+Tab切页；?支持悬停、键盘聚焦或点击查看说明。",
            "Management releases only its own pause when closed; any pre-existing game pause remains. Tab moves focus; Ctrl+Tab changes pages. Hover, focus or activate ? to read help.");
    }
    ImGui::Separator();
    // 内容独立滚动，底部操作始终可见；小窗口不会把保存按钮挤出屏幕。
    ImGui::BeginChild("ManagementBody",{0,-104},ImGuiChildFlags_None);
    drawManagementErrors();
    if(state.value("mission_fixed_sp",false))ImGui::TextColored({1,.76f,.38f,1},"%s",tr("第13关：使用任务指定SP；镜头功能仍可用。","Mission 13: assigned SP; camera features remain available."));
    if(ImGui::BeginTabBar("ManagementTabs")) {
        if(beginManagementTab(tr("基础###Features","General###Features"),0)) {drawFeatureSettings();ImGui::EndTabItem();}
        if(beginManagementTab(tr("视角###Bindings","Camera###Bindings"),1)) {drawCameraSettings();ImGui::EndTabItem();}
        if(beginManagementTab(tr("日志###Logs","Logs###Logs"),2)) {
            if(!logsLoaded)refreshLogs();
            if(ImGui::Button(tr("刷新###RefreshLogs","Refresh###RefreshLogs")))refreshLogs();
            panelHelp("每份只显示末尾64 KiB，点击刷新更新；日志正文保留原始语言。","Shows the last 64 KiB of each log. Refresh to update; log contents retain their original language.");
            if(!logError.empty())ImGui::TextWrapped("%s",uiMessage(logError).c_str());
            if(ImGui::BeginTabBar("LogFiles")) {
                const char* files[]={"mod.log","native.log"};const std::string* text[]={&modLogText,&nativeLogText};
                for(int i=0;i<2;++i)if(ImGui::BeginTabItem(files[i])) {
                    ImGui::BeginChild(files[i],{0,std::max(160.f,height-330)},ImGuiChildFlags_Borders,ImGuiWindowFlags_HorizontalScrollbar);
                    ImGui::TextUnformatted(text[i]->c_str());ImGui::EndChild();ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }
            ImGui::EndTabItem();
        }
        if(beginManagementTab(tr("调试###Debug","Debug###Debug"),3)) {
            if(ImGui::CollapsingHeader(tr("任务兵装###DebugLoadout","Mission loadout###DebugLoadout"),ImGuiTreeNodeFlags_DefaultOpen))drawDebugLoadout(requestedTab==3);
            if(ImGui::CollapsingHeader(tr("更换机体###DebugAircraft","Change aircraft###DebugAircraft")))drawDebugAircraft();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();if(requestedTab==managementTab)requestedTab=-1;
    }
    ImGui::EndChild();ImGui::Separator();
    ImGui::TextDisabled("%s",captureSlot>=0?tr("等待新按键 · Esc取消","Waiting for a key · Esc cancels"):pendingSave>0?tr("正在保存…","Saving..."):managementDirty()?tr("有未保存的更改","Unsaved changes"):tr("设置已保存","Settings saved"));
    if(captureSlot>=0){ImGui::SameLine();if(ImGui::SmallButton(tr("取消改键","Cancel capture")))captureSlot=-1;}
    ImGui::BeginDisabled(captureSlot>=0||!bindingsValid()||pendingSave>0);
    if(ImGui::Button(tr("保存设置###SaveSettings","Save settings###SaveSettings"),{168,36}))saveManagementSettings();
    ImGui::EndDisabled();ImGui::SameLine();
    if(ImGui::Button(tr("关闭###CloseManagement","Close###CloseManagement"),{100,36}))togglePanel(false);
    panelHelp("关闭会丢弃未保存的更改。语言和按键保存后应用；总开关等待安全菜单，弹药比例等待下一次配弹。",
        "Closing discards unsaved changes. Language and bindings apply after saving; the master switch waits for safe menus and ammunition percentages for the next loadout.");
    if(!bindingsValid())ImGui::TextColored({1,.65f,.4f,1},"%s",tr("键位有重复或无效，请调整。","A binding is invalid or conflicts; adjust it before saving."));
    else if(!notice.empty()&&notice!="设置已保存。"&&notice!="Settings saved."&&notice!="正在保存…"&&notice!="Saving...")
        ImGui::TextWrapped("%s",uiMessage(notice).c_str()); // 通常保存状态已在上行展示，避免修改草稿后留下旧成功提示。
    else if(state.contains("input_notice"))ImGui::TextDisabled("%s",state.at("input_notice").get<std::string>()=="empty_sp"?
        tr("此SP未配置，仍使用当前兵装。","This SP is empty; selection unchanged."):tr("此SP已耗尽，仍使用当前兵装。","This SP is depleted; selection unchanged."));
    ImGui::End();
}
