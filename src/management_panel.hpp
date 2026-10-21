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
        draftTargetGazePad!=state.at("target_gaze_pad_enabled").get<bool>() || draftTargetGazeKeyboard!=state.at("target_gaze_keyboard_enabled").get<bool>() || draftCameraHold!=state.at("camera_hold_enabled").get<bool>() ||
        draftCameraCenterKey!=state.at("camera_center_key").get<int>() || draftCameraSensitivity!=state.at("camera_sensitivity").get<int>() ||
        draftAmmoPercentTwo!=state.at("ammo_percent_two").get<int>() || draftAmmoPercentThree!=state.at("ammo_percent_three").get<int>() ||
        draftLanguage!=state.at("language").get<std::string>() || Json(draftKeys)!=state.at("keyboard_keys") || shortcutData(draftShortcuts)!=state.at("shortcuts");
}
void drawBasicBindings();
void drawFeatureSettings() {
    if(panelCard("ModelOptions",tr("模型显示","Model display"))) {
        ImGui::TextWrapped("%s",tr("只影响模型显示，不影响游戏功能。","Affects model display only, not gameplay features."));
        ImGui::Checkbox(tr("任务中使用多SP模型挂载###TaskMount","Use multi-SP models in missions###TaskMount"),&draftTaskMountEnabled);
        panelHelp("任务中用同时挂载多个SP的模型，减少切换SP时外观突变。已适配机体提供单、双、三SP共7种组合；Mod机体、部分内置弹舱及RKT组合按现有适配范围生效。可能造成卡顿、帧数下降。覆盖飞行、补给及补给过渡；下次配装时应用。",
            "Uses models carrying multiple SPs to reduce visual changes when switching weapons. Supported aircraft have seven single/double/triple-SP combinations; mod aircraft, some internal bays and RKT combinations depend on existing coverage. May reduce performance. Covers flight and resupply transitions; applies with the next loadout.");
        ImGui::Checkbox(tr("机库中使用多SP模型挂载###HangarMount","Use multi-SP models in the hangar###HangarMount"),&draftHangarMountEnabled);
        panelHelp("出击前选择机体和SP时，机体预览也使用多SP模型。可能造成卡顿、帧数下降。关闭后展示游戏原本的机体和SP模型，可改善机库中的低帧卡顿；与任务模型开关独立。",
            "Uses multi-SP models in aircraft and SP previews before a sortie. May reduce frame rate. Disabling restores native hangar models and may improve performance. Independent of mission models.");
        if(state.at("task_mount_enabled")!=state.at("runtime_task_mount_enabled"))ImGui::TextColored({1,.76f,.38f,1},"%s",tr("任务模型设置将在下次配装时应用。","Mission model settings apply with the next loadout."));
        if(state.contains("mount_warning"))ImGui::TextWrapped("%s",uiMessage(state.at("mount_warning").get<std::string>(),"挂载外观已回退，请查看日志。","Models reverted to native visuals; see Logs.").c_str());
    }
    ImGui::EndChild();
    if(panelCard("MainOptions",tr("功能设置","Feature settings"))) {
        ImGui::Checkbox(tr("机炮齐射###GunSalvo","Gun salvo###GunSalvo"),&draftGunSalvo);
        panelHelp("新配置默认开启。携带MGP或PLSL时，按住游戏实际机炮键会联动该吊舱，松开后恢复先前选择。HGP及不同SP真正并发暂不支持；切槽可能有间隙。暂停、管理、失焦或人工切武器会结束联动。",
            "On for new configurations. Holding the native gun action also fires an equipped MGP or PLSL; release restores the previous selection. HGP and simultaneous different SPs remain unsupported; slot changes can cause gaps. Pause, management, focus loss or manual selection cancels it.");
        if(state.contains("gun_salvo")&&state.at("gun_salvo").value("reason",std::string())=="multiple_supported_sp")
            ImGui::TextWrapped("%s",tr("当前方案含多种合格SP，暂不支持它们同时发射。","This loadout has multiple eligible SPs; simultaneous firing is unsupported."));
        bool always=draftCentralWeaponDisplay=="always";
        const bool narrow=ImGui::GetContentRegionAvail().x<500;
        if(ImGui::Checkbox(narrow?tr("中央常显当前武器###CentralWeaponDisplay","Always show weapon at center###CentralWeaponDisplay"):tr("HUD中央始终显示当前武器名称###CentralWeaponDisplay","Always show the current weapon at HUD center###CentralWeaponDisplay"),&always))draftCentralWeaponDisplay=always?"always":"switch_1s";
        ImGui::TextWrapped("%s",tr("双SP时每个武器的弹药量","Ammo per weapon with two SPs"));
        ImGui::SetNextItemWidth(std::min(340.f,ImGui::GetContentRegionAvail().x));
        ImGui::SliderInt("##AmmoTwo",&draftAmmoPercentTwo,50,100,"%d%%",ImGuiSliderFlags_AlwaysClamp);
        ImGui::TextWrapped("%s",tr("三SP时每个武器的弹药量","Ammo per weapon with three SPs"));
        ImGui::SetNextItemWidth(std::min(340.f,ImGui::GetContentRegionAvail().x));
        ImGui::SliderInt("##AmmoThree",&draftAmmoPercentThree,33,100,"%d%%",ImGuiSliderFlags_AlwaysClamp);
    }
    ImGui::EndChild();
    drawBasicBindings();
}
void drawCameraSettings() {
    if(panelCard("ViewTargetOptions",tr("目标选择方式","Target selection"))) {
        int mode=draftViewTargetMode=="normal"?0:1;
        ImGui::SetNextItemWidth(std::min(340.f,ImGui::GetContentRegionAvail().x*.55f));
        if(ImGui::Combo(tr("目标选择模式###NextMode","Target selection mode###NextMode"),&mode,tr("原生模式 / 机头附近\0VR模式 / 视角中心\0","Native / aircraft nose\0VR / view center\0")))draftViewTargetMode=mode==0?"normal":"vr";
        panelHelp("默认原生模式，切换目标时优先选择机头附近；VR模式优先选择当前视角中心附近。只改变NEXT排序参考，不启用VR设备，不扩大导弹锁定范围。可能与鼠标飞控的F2 HMD冲突。模式仅在本页选择，L3不切换NEXT。",
            "Native mode prioritizes the aircraft nose; VR mode prioritizes the current view center. Changes NEXT ranking only, without enabling VR hardware or extending missile lock range. May conflict with the mouse-flight mod's F2 HMD. Choose the mode here; L3 never changes NEXT.");
        ImGui::Checkbox(tr("显示视角中心定位白点###ViewCenterDot","Show the view-center dot###ViewCenterDot"),&draftViewCenterDot);
    }
    ImGui::EndChild();
    if(panelCard("TrackingOptions",tr("保持注视","Latched target tracking"))) {
        ImGui::Checkbox(tr("长按手柄Y键保持注视###TargetGazePad","Hold controller Y to latch tracking###TargetGazePad"),&draftTargetGazePad);
        panelHelp("默认关闭。游戏原本需要一直按住Y才注视；开启后按满0.2秒即可持续注视，再长按一次停止。期间短按Y仍可切目标，镜头跟随新目标；长按松开不会额外切目标。",
            "Off by default. Instead of holding Y throughout, hold it for 0.2 seconds to latch tracking and hold again to release. Short Y presses still select targets and tracking follows; releasing a long press does not select another target.");
        ImGui::Checkbox(tr("按下键盘注视键保持注视###TargetGazeKeyboard","Press the native keyboard tracking key to latch###TargetGazeKeyboard"),&draftTargetGazeKeyboard);
        panelHelp("默认关闭。沿用游戏中实际绑定的注视键，按一下持续注视，再按一下停止，无需一直按住。游戏的切换目标键保持原功能。两种注视选项独立；关闭的输入来源保留原生按住注视。暂停、管理、失焦、失去目标或离场时结束注视。",
            "Off by default. Uses the game's actual keyboard tracking binding: press once to latch and again to release. Native target selection is unchanged. Independent of the controller option; a disabled source keeps native hold-to-track. Stops on pause, management, focus loss, target loss or mission exit.");
    }
    ImGui::EndChild();
    if(panelCard("CameraOptions",tr("视角保持","Camera hold"))) {
        ImGui::Checkbox(tr("启用视角保持###CameraHold","Keep camera direction###CameraHold"),&draftCameraHold);
        panelHelp("启用后松开环视输入仍保持方向；可按下并松开L3，或使用键盘回中键回正。R3短按仍切第一/第三人称视角，长按满0.3秒切换视角保持；长按松开不再切视角。L3+R3只释放干扰弹，不切换NEXT。",
            "Retains direction after releasing look input. Press and release L3 or use the recenter key to return to center. Short R3 changes native views; hold for 0.3 seconds to toggle camera hold without changing view on release. Both sticks only release countermeasures; NEXT is unchanged.");
        ImGui::SetNextItemWidth(std::min(340.f,ImGui::GetContentRegionAvail().x*.55f));
        ImGui::SliderInt(tr("视角灵敏度###CameraSensitivity","View sensitivity###CameraSensitivity"),&draftCameraSensitivity,5,200,"%d%%",ImGuiSliderFlags_AlwaysClamp);
        panelHelp("默认100%，范围5%至200%。控制保持视角时右摇杆的转动速度，不扩大观察范围；座舱仍受原生边界和动画影响。",
            "Default 100%, range 5% to 200%. Controls stick turn speed during camera hold without extending view limits; cockpit limits and animations remain.");
        ImGui::TextUnformatted(tr("视角回中键盘快捷键","Keyboard recenter shortcut"));ImGui::SameLine();
        bindingButton(8,draftCameraCenterKey?keyLabel(draftCameraCenterKey):tr("未绑定","Unassigned"),180);
        ImGui::SameLine();if(ImGui::SmallButton(tr("清除###ClearCenterKey","Clear###ClearCenterKey"))){draftCameraCenterKey=0;captureSlot=-1;}
        ImGui::TextUnformatted(tr("启用视角保持快捷键","Camera hold toggle shortcut"));ImGui::SameLine();
        bindingButton(10,shortcutLabel(draftShortcuts[4]),210);
        ImGui::SameLine();if(ImGui::SmallButton(tr("清除###ClearCameraHoldKey","Clear###ClearCameraHoldKey"))){draftShortcuts[4]={0,0};captureSlot=-1;}
    }
    ImGui::EndChild();
}
void drawBasicBindings() {
    if(panelCard("DirectKeys",tr("额外的武器切换方式","Additional weapon selection"))) {
        ImGui::TextWrapped("%s",tr("游戏原本的武器切换键会依次切换每个武器。可使用手柄十字键快速选择武器，或自定义键盘快捷键。",
            "The native weapon action cycles through weapons. Use the D-pad or custom keyboard bindings for direct selection."));
        ImGui::Checkbox(tr("手柄十字键切换武器###DpadWeaponSwitch","Switch weapons with the controller D-pad###DpadWeaponSwitch"),&draftDpadWeaponSwitchEnabled);
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
    }
    ImGui::EndChild();
    if(panelCard("ModKeys",tr("模组快捷键","Mod shortcuts"))) {
        ImGui::TextUnformatted(tr("管理界面","Management"));ImGui::SameLine();bindingButton(4,shortcutLabel(draftShortcuts[0]));
        panelHelp("手柄Back+Y也可开关管理界面。键盘Tab选项，Enter操作，Ctrl+Tab切页，Esc返回。",
            "Back+Y also toggles management. Tab navigates, Enter activates, Ctrl+Tab changes pages and Esc returns.");
        ImGui::TextUnformatted(tr("机炮齐射","Gun salvo"));ImGui::SameLine();bindingButton(9,shortcutLabel(draftShortcuts[3]));
        ImGui::SameLine();if(ImGui::SmallButton(tr("清除###ClearSalvoKey","Clear###ClearSalvoKey"))){draftShortcuts[3]={0,0};captureSlot=-1;}
    }
    ImGui::EndChild();
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
    ImGui::TextColored({.37f,.84f,.79f,1},"AC7MultiSP");ImGui::SameLine();ImGui::TextDisabled("2.26.1");
    if(state.value("management_protocol",0)!=managementProtocol||!managementError.empty()) {
        ImGui::TextWrapped("%s",tr("管理组件版本不匹配，请完整安装同一版本。","Management components do not match. Install a complete matching version."));
        if(!managementError.empty())ImGui::TextWrapped("%s",uiMessage(managementError).c_str());
        if(ImGui::Button(tr("关闭","Close")))togglePanel(false);
        ImGui::End();return;
    }
    if(width>550)ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button,draftModEnabled?ImVec4{.13f,.43f,.39f,1}:ImVec4{.37f,.20f,.20f,1});
    if(focusManagement){ImGui::SetKeyboardFocusHere();focusManagement=false;}
    if(ImGui::Button(draftModEnabled?tr("模组：开启###ModEnabled","Mod: ON###ModEnabled"):tr("模组：关闭###ModEnabled","Mod: OFF###ModEnabled"),{176,38})) {draftModEnabled=!draftModEnabled;captureSlot=-1;}
    ImGui::PopStyleColor();
    panelHelp("总开关在保存后，于主菜单或单人任务选择页生效。关闭时下方页面变灰；标题开关、保存和关闭始终可用。",
        "Save to apply at the main menu or single-player mission selection. Disabled pages are dimmed; this switch, Save and Close remain available.");
    if(width>700)ImGui::SameLine(width-240);
    int language=draftLanguage=="auto"?0:draftLanguage=="zh"?1:2;
    ImGui::SetNextItemWidth(210);
    if(ImGui::Combo("##Language",&language,tr("跟随游戏\0中文\0English\0","Game language\0Chinese\0English\0")))draftLanguage=language==0?"auto":language==1?"zh":"en";
    if(state.at("mod_enabled")!=state.at("runtime_mod_enabled"))ImGui::TextColored({1,.76f,.38f,1},"%s",tr("已保存，返回主菜单后应用总开关。","Saved; return to the main menu to apply the master switch."));
    if(state.contains("master_switch")&&state.at("master_switch").contains("error"))ImGui::TextWrapped("%s",uiMessage(state.at("master_switch").at("error").get<std::string>()).c_str());
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
    ImGui::BeginDisabled(!draftModEnabled);
    if(ImGui::BeginTabBar("ManagementTabs")) {
        if(beginManagementTab(tr("基础功能###Features","General###Features"),0)) {drawFeatureSettings();ImGui::EndTabItem();}
        if(beginManagementTab(tr("视角###Bindings","Camera###Bindings"),1)) {drawCameraSettings();ImGui::EndTabItem();}
        if(beginManagementTab(tr("调试###Debug","Debug###Debug"),2)) {
            if(ImGui::CollapsingHeader(tr("任务兵装###DebugLoadout","Mission loadout###DebugLoadout"),ImGuiTreeNodeFlags_DefaultOpen))drawDebugLoadout(requestedTab==2);
            if(ImGui::CollapsingHeader(tr("更换机体###DebugAircraft","Change aircraft###DebugAircraft")))drawDebugAircraft();
            if(ImGui::CollapsingHeader(tr("日志###Logs","Logs###Logs"))) {
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
            }
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();if(requestedTab==managementTab)requestedTab=-1;
    }
    ImGui::EndDisabled();
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
