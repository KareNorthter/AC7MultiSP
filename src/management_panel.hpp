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
    return draftModEnabled!=state.at("mod_enabled").get<bool>() || draftFixedMountEnabled!=state.at("fixed_mount_enabled").get<bool>() ||
        draftTargetGazePadL3!=state.at("target_gaze_pad_l3").get<bool>() || draftCameraHold!=state.at("camera_hold_enabled").get<bool>() ||
        draftCameraR3!=state.at("camera_r3_enabled").get<bool>() || draftTargetGazeKey!=state.at("target_gaze_key").get<int>() ||
        draftCameraCenterKey!=state.at("camera_center_key").get<int>() || draftCameraSensitivity!=state.at("camera_sensitivity").get<int>() ||
        draftAmmoPercentTwo!=state.at("ammo_percent_two").get<int>() || draftAmmoPercentThree!=state.at("ammo_percent_three").get<int>() ||
        draftLanguage!=state.at("language").get<std::string>() || Json(draftKeys)!=state.at("keyboard_keys") || shortcutData(draftShortcuts)!=state.at("shortcuts");
}
void drawFeatureSettings() {
    if(panelCard("MainOptions",tr("飞行设置","Flight settings"))) {
        ImGui::Checkbox(tr("启用模组###ModEnabled","Enable mod###ModEnabled"),&draftModEnabled);
        panelHelp("总开关在主菜单或单人任务选择页应用；在任务/机库保存后，回到这些菜单自动生效，无需重启。管理入口始终可用。",
            "The master switch applies at the main menu or single-player mission selection. Changes saved during a mission or in the hangar apply on returning there. No restart is needed; management stays available.");
        ImGui::Checkbox(tr("多SP挂载模型###FixedMount","Multi-SP weapon models###FixedMount"),&draftFixedMountEnabled);
        panelHelp("同时显示已适配机体的多SP模型，并控制机库/补给预览；部分内舱保持原生。下次出击、任务/检查点重试、补给或调试换装时应用。失败恢复原生外观，不影响兵装选择。适配机体见安装说明。",
            "Shows supported multi-SP models and controls hangar/resupply previews. Some internal bays remain native. Applies on sortie, mission/checkpoint retry, resupply or debug loadout. Visual failures restore native models without affecting selection. See installation notes for aircraft coverage.");
        if(state.at("mod_enabled")!=state.at("runtime_mod_enabled"))ImGui::TextColored({1,.76f,.38f,1},"%s",tr("已保存，返回主菜单后应用总开关。","Saved; return to the main menu to apply the master switch."));
        if(state.at("fixed_mount_enabled")!=state.at("runtime_fixed_mount_enabled"))ImGui::TextColored({1,.76f,.38f,1},"%s",tr("模型设置将在下次配装时应用。","Model settings apply with the next loadout."));
        if(state.contains("mount_warning"))ImGui::TextWrapped("%s",uiMessage(state.at("mount_warning").get<std::string>(),"挂载外观已回退，请查看日志。","Models reverted to native visuals; see Logs.").c_str());
        if(state.contains("master_switch")&&state.at("master_switch").contains("error"))
            ImGui::TextWrapped("%s",uiMessage(state.at("master_switch").at("error").get<std::string>(),"总开关应用失败，请查看日志。","The master switch could not be applied; see Logs.").c_str());
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
    if(ImGui::CollapsingHeader(tr("使用提示###FeatureHelp","Quick guide###FeatureHelp"))) {
        ImGui::BulletText("%s",tr("机库直接选择1至3种SP，任务中用X轮切或十字键直选。","Select 1–3 SPs in the hangar; cycle with X or select with the D-pad."));
        ImGui::BulletText("%s",tr("中央兵装提示及右下HUD跟随游戏HUD显隐。","Weapon hints and the lower-right HUD follow game HUD visibility."));
        ImGui::BulletText("%s",tr("镜头保持、保持注视和所有快捷键在“按键”页。","Find camera hold, toggle tracking and shortcuts in Bindings."));
    }
}
void drawBindingSettings() {
    if(panelCard("TrackingOptions",tr("保持注视","Toggle tracking"))) {
        ImGui::Checkbox(tr("左摇杆按下切换注视###TargetGazePadL3","L3 toggles tracking###TargetGazePadL3"),&draftTargetGazePadL3);
        panelHelp("默认开启。左摇杆短按松开开始/停止注视；双摇杆组合不触发注视。X保留轮切兵装，Y保留选目标和原生长按注视。暂停、管理、失焦、失去目标或离场时停止。第13关也可使用。",
            "On by default. Press and release L3 to start/stop tracking; the two-stick countermeasure gesture never toggles tracking. X cycles weapons; Y selection and native hold-to-track remain. Stops on pause, management, focus loss, target loss or mission exit; also works in Mission 13.");
        ImGui::TextUnformatted(tr("键盘注视键","Keyboard tracking key"));ImGui::SameLine();
        bindingButton(7,draftTargetGazeKey?keyLabel(draftTargetGazeKey):tr("未绑定","Unassigned"),180);
        ImGui::SameLine();if(ImGui::SmallButton(tr("清除###ClearGazeKey","Clear###ClearGazeKey"))){draftTargetGazeKey=0;captureSlot=-1;}
        panelHelp("键盘注视键与左摇杆选项独立，单次按下切换，按住不重复。不要与原生操作或其他模组按键重复。",
            "The keyboard tracking key works independently of L3. Press once to toggle; holding does not repeat. Avoid conflicts with native actions or other mod bindings.");
    }
    ImGui::EndChild();
    if(panelCard("CameraOptions",tr("镜头保持","Camera hold"))) {
        ImGui::Checkbox(tr("启用镜头保持###CameraHold","Keep camera direction###CameraHold"),&draftCameraHold);
        panelHelp("默认开启。松开摇杆/视点模式键、短按切目标或切换普通视角后保持方向；注视从当前方向衔接，停止后返回保持方向。关闭后恢复原生自动回正。暂停、管理、失焦和特殊镜头期间归还原生。",
            "On by default. Retains direction after releasing the stick/viewpoint mode key, briefly selecting a target or changing ordinary views. Tracking starts from and returns to that direction. Disabling restores native recentering. Pause, management, focus loss and special cameras restore native behavior.");
        if(!draftCameraHold&&captureSlot==8)captureSlot=-1;
        if(draftCameraHold) {
            ImGui::Checkbox(tr("R3长按回中###CameraR3","Hold R3 to recenter###CameraR3"),&draftCameraR3);
            panelHelp("默认开启。R3短按松开切换视角；长按满0.3秒立即回中，松开不切视角；双摇杆只释放干扰弹。关闭此项则R3完全使用原生功能。回中会停止模组注视，实际按住Y/F仍可原生注视。",
                "On by default. Release a short R3 press to change view; holding for 0.3 seconds recenters immediately without changing view. Both sticks only release countermeasures. Disabling restores fully native R3 behavior. Recentering stops mod tracking; native Y/F hold-to-track remains.");
            ImGui::TextUnformatted(tr("独立回中键","Recenter shortcut"));ImGui::SameLine();
            bindingButton(8,draftCameraCenterKey?keyLabel(draftCameraCenterKey):tr("未绑定","Unassigned"),180);
            ImGui::SameLine();if(ImGui::SmallButton(tr("中键###ResetCenterKey","Middle mouse###ResetCenterKey"))){draftCameraCenterKey=VK_MBUTTON;captureSlot=-1;}
            ImGui::SameLine();if(ImGui::SmallButton(tr("清除###ClearCenterKey","Clear###ClearCenterKey"))){draftCameraCenterKey=0;captureSlot=-1;}
            panelHelp("点击后按一个键盘键或鼠标中键，Esc取消。按下即回中，按住不重复；与原生键盘换视角是独立功能，请分配不同按键。",
                "Click, then press one keyboard key or middle mouse; Esc cancels. Acts on press without repetition. This is separate from native keyboard view switching; assign different keys.");
            ImGui::SetNextItemWidth(std::min(340.f,ImGui::GetContentRegionAvail().x*.55f));
            ImGui::SliderInt(tr("摇杆灵敏度###CameraSensitivity","Stick sensitivity###CameraSensitivity"),&draftCameraSensitivity,5,200,"%d%%",ImGuiSliderFlags_AlwaysClamp);
            panelHelp("默认100%，范围5%至200%。仅调整右摇杆普通环视移动速度，不扩大角度；回中、注视和单独键鼠操作保留原生速度。",
                "Default 100%, range 5%–200%. Adjusts ordinary right-stick look speed without increasing angles. Recentering, tracking and keyboard/mouse-only look retain native speed.");
        }
    }
    ImGui::EndChild();
    ImGui::TextDisabled("%s",tr("X：轮切兵装    L3 + R3：干扰弹","X: cycle weapons    L3 + R3: countermeasures"));
    if(ImGui::CollapsingHeader(tr("武器直选键###DirectKeys","Direct weapon keys###DirectKeys"))) {
        if(ImGui::BeginTable("DirectBindings",3,ImGuiTableFlags_SizingStretchProp|ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn(tr("兵装","Weapon"));ImGui::TableSetupColumn(tr("键盘","Keyboard"));ImGui::TableSetupColumn(tr("手柄","Controller"));ImGui::TableHeadersRow();
            const char* weapons[]={"MSL","SP1","SP2","SP3"};
            const char* pads[]={tr("十字键左","D-pad Left"),tr("十字键上","D-pad Up"),tr("十字键右","D-pad Right"),tr("十字键下","D-pad Down")};
            for(int i=0;i<4;++i) {
                ImGui::TableNextRow();ImGui::TableNextColumn();ImGui::TextUnformatted(weapons[i]);
                ImGui::TableNextColumn();bindingButton(i,keyLabel(draftKeys[i]),150);
                ImGui::TableNextColumn();ImGui::TextUnformatted(pads[i]);
            }
            ImGui::EndTable();
        }
        if(ImGui::SmallButton(tr("恢复1 / 2 / 3 / 4###ResetWeaponKeys","Reset 1 / 2 / 3 / 4###ResetWeaponKeys"))) {
            const std::array<int,4> defaults{49,50,51,52};
            if(dualsp::validShortcuts(draftShortcuts,defaults)&&dualsp::validTargetGazeKey(draftTargetGazeKey,draftShortcuts,defaults)&&
                dualsp::validCameraCenterKey(draftCameraCenterKey,draftCameraHold,draftTargetGazeKey,draftShortcuts,defaults)){draftKeys=defaults;captureSlot=-1;notice.clear();}
            else notice=tr("默认武器键与当前键位重复，请先调整。","Default weapon keys conflict with a current binding.");
        }
        panelHelp("点击键位后按新键，Esc取消；手柄十字键固定。未搭载或耗尽的SP不切换，装填中的武器可选择。",
            "Click a binding and press a new key; Esc cancels. D-pad bindings are fixed. Empty or depleted SPs keep the current selection; reloading weapons can be selected.");
    }
    if(ImGui::CollapsingHeader(tr("模组快捷键###ModKeys","Mod shortcuts###ModKeys"))) {
        const char* names[]={tr("管理界面","Management"),tr("轮切SP组合","Cycle SP loadouts"),tr("切换挂载模型","Toggle models")};
        for(int i=0;i<3;++i) {
            ImGui::TextUnformatted(names[i]);ImGui::SameLine(std::min(260.f,ImGui::GetContentRegionAvail().x*.45f));
            bindingButton(i+4,shortcutLabel(draftShortcuts[i]));
        }
        if(ImGui::SmallButton(tr("恢复默认快捷键###ResetShortcuts","Reset shortcuts###ResetShortcuts"))) {
            const std::array<dualsp::Shortcut,3> defaults{{{VK_F6,0},{VK_F6,3},{VK_F7,3}}};
            if(dualsp::validShortcuts(defaults,draftKeys)&&dualsp::validTargetGazeKey(draftTargetGazeKey,defaults,draftKeys)&&
                dualsp::validCameraCenterKey(draftCameraCenterKey,draftCameraHold,draftTargetGazeKey,defaults,draftKeys)){draftShortcuts=defaults;captureSlot=-1;notice.clear();}
            else notice=tr("默认快捷键与当前键位重复，请先调整。","Default shortcuts conflict with a current binding.");
        }
        panelHelp("点击后按单键，或按住Ctrl/Alt/Shift再按主键。Tab移动焦点，Enter操作，Ctrl+Tab切页，Esc返回；手柄Back+Y也可开关管理界面。",
            "Click, then press a key, optionally with Ctrl/Alt/Shift. Tab moves focus, Enter activates, Ctrl+Tab changes pages, Esc goes back. Controller Back+Y also toggles management.");
    }
}
void drawManagementErrors() {
    const char* fields[]={"error","menu_error","hud_error","central_error","settings_error"};
    bool errors=false;for(const char* field:fields)errors=errors||state.contains(field);
    for(const char* field:{"target_gaze","camera_hold","management_pause"})errors=errors||(state.contains(field)&&state.at(field).contains("error"));
    if(errors) {
        ImGui::TextColored({1,.65f,.4f,1},"%s",tr("有功能或保存异常，请展开查看。","A feature or save operation needs attention."));
        if(ImGui::CollapsingHeader(tr("查看异常详情###ManagementErrors","View error details###ManagementErrors"))) {
            for(const char* field:fields)if(state.contains(field))ImGui::TextWrapped("%s",uiMessage(state.at(field).get<std::string>()).c_str());
            for(const char* field:{"target_gaze","camera_hold","management_pause"})if(state.contains(field)&&state.at(field).contains("error"))
                ImGui::TextWrapped("%s",uiMessage(state.at(field).at("error").get<std::string>()).c_str());
        }
    }
}
void drawPanel() {
    const auto display=ImGui::GetIO().DisplaySize;
    const float width=std::min(1080.f,std::max(280.f,display.x-32)),height=std::min(830.f,std::max(240.f,display.y-32));
    ImGui::SetNextWindowPos({display.x*.5f,display.y*.5f},ImGuiCond_Always,{.5f,.5f});ImGui::SetNextWindowSize({width,height});
    ImGui::Begin("AC7MultiSP###DualSPManagement",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoSavedSettings);
    ImGui::TextColored({.37f,.84f,.79f,1},"AC7MultiSP");ImGui::SameLine();ImGui::TextDisabled("2.20.1");
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
        if(beginManagementTab(tr("常用###Features","General###Features"),0)) {drawFeatureSettings();ImGui::EndTabItem();}
        if(beginManagementTab(tr("按键###Bindings","Bindings###Bindings"),1)) {drawBindingSettings();ImGui::EndTabItem();}
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
