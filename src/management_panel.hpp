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
        panelHelp("任务中使用挂载多个SP武器的机体模型替代游戏原本机体模型，避免切换SP武器时，模型突变。\n提供单SP、双SP、三SP 共7种挂载方案。已适配大部分游戏本体和DLC机体，Mod机体、部分内置弹舱及RKT组合按现有适配范围生效。\n可能会造成卡顿、帧数下降。",
            "Replaces native aircraft models with models carrying multiple SPs to avoid abrupt model changes when switching SPs.\nProvides seven single-, dual- and triple-SP loadouts. Most base-game and DLC aircraft are supported; mod aircraft, some internal bays and RKT combinations depend on existing support.\nMay cause stuttering or reduce frame rate.");
        ImGui::Checkbox(tr("机库中使用多SP模型挂载###HangarMount","Use multi-SP models in the hangar###HangarMount"),&draftHangarMountEnabled);
        panelHelp("出击前选择机体和SP时的机体预览也使用多SP模型挂载。\n可能造成卡顿、帧数下降。",
            "Uses multi-SP models in aircraft previews when choosing an aircraft and SPs before a sortie.\nMay cause stuttering or reduce frame rate.");
        if(state.at("task_mount_enabled")!=state.at("runtime_task_mount_enabled"))ImGui::TextColored({1,.76f,.38f,1},"%s",tr("任务模型设置将在下次配装时应用。","Mission model settings apply with the next loadout."));
        if(state.contains("mount_warning"))ImGui::TextWrapped("%s",uiMessage(state.at("mount_warning").get<std::string>(),"挂载外观已回退，请查看日志。","Models reverted to native visuals; see Logs.").c_str());
    }
    ImGui::EndChild();
    if(panelCard("MainOptions",tr("功能设置","Feature settings"))) {
        bool always=draftCentralWeaponDisplay=="always";
        if(ImGui::Checkbox("##CentralWeaponDisplay",&always))draftCentralWeaponDisplay=always?"always":"switch_1s";
        ImGui::SameLine();ImGui::TextWrapped("%s",tr("HUD中央始终显示当前武器名称","Always show the current weapon at HUD center"));
        if(ImGui::IsItemClicked())draftCentralWeaponDisplay=always?"switch_1s":"always";
        ImGui::TextWrapped("%s",tr("双SP时每个武器的弹药量","Ammo per weapon with two SPs"));
        ImGui::SetNextItemWidth(std::min(340.f,ImGui::GetContentRegionAvail().x));
        ImGui::SliderInt("##AmmoTwo",&draftAmmoPercentTwo,50,100,"%d%%",ImGuiSliderFlags_AlwaysClamp);
        ImGui::TextWrapped("%s",tr("三SP时每个武器的弹药量","Ammo per weapon with three SPs"));
        ImGui::SetNextItemWidth(std::min(340.f,ImGui::GetContentRegionAvail().x));
        ImGui::SliderInt("##AmmoThree",&draftAmmoPercentThree,33,100,"%d%%",ImGuiSliderFlags_AlwaysClamp);
        ImGui::Checkbox(tr("机炮齐射###GunSalvo","Gun salvo###GunSalvo"),&draftGunSalvo);
        ImGui::SameLine();ImGui::TextUnformatted(tr("快捷键","Shortcut"));ImGui::SameLine();
        bindingButton(9,shortcutLabel(draftShortcuts[3]),170);
        ImGui::SameLine();if(ImGui::SmallButton(tr("清除###ClearSalvoKey","Clear###ClearSalvoKey"))){draftShortcuts[3]={0,0};captureSlot=-1;}
        panelHelp("携带MGP/HGP/PLSL吊舱时，按住机炮发射键会联动该武器齐射。",
            "With an MGP/HGP/PLSL pod equipped, holding the gun fire button fires it together with the gun.");
        if(state.contains("gun_salvo")&&state.at("gun_salvo").value("reason",std::string())=="multiple_supported_sp")
            ImGui::TextWrapped("%s",tr("当前方案含多种合格SP，暂不支持它们同时发射。","This loadout has multiple eligible SPs; simultaneous firing is unsupported."));
    }
    ImGui::EndChild();
    drawBasicBindings();
}
void drawCameraSettings() {
    if(panelCard("ViewTargetOptions",tr("目标选择方式","Target selection"))) {
        int mode=draftViewTargetMode=="normal"?0:1;
        ImGui::SetNextItemWidth(std::min(340.f,ImGui::GetContentRegionAvail().x*.55f));
        if(ImGui::Combo(tr("目标选择模式###NextMode","Target selection mode###NextMode"),&mode,tr("原生模式 / 机头附近\0VR模式 / 视角中心\0","Native / aircraft nose\0VR / view center\0")))draftViewTargetMode=mode==0?"normal":"vr";
        panelHelp("原生模式优先选择机头附近；\nVR模式优先选择当前视角中心附近。\n可能与鼠标飞控F2 HMD冲突。",
            "Native mode prioritizes targets near the aircraft nose;\nVR mode prioritizes targets near the current view center.\nMay conflict with the mouse-flight mod F2 HMD.");
        ImGui::Checkbox(tr("显示视角中心定位白点###ViewCenterDot","Show the view-center dot###ViewCenterDot"),&draftViewCenterDot);
    }
    ImGui::EndChild();
    if(panelCard("TrackingOptions",tr("保持注视","Latched target tracking"))) {
        ImGui::Checkbox(tr("长按手柄Y键 保持注视###TargetGazePad","Hold controller Y to latch tracking###TargetGazePad"),&draftTargetGazePad);
        panelHelp("启用后长按手柄Y键切换到持续注视，再长按一次停止；期间可短按Y切目标。",
            "When enabled, hold controller Y to latch tracking; hold it again to stop. Short Y presses can still change targets while tracking.");
        ImGui::Checkbox(tr("按下键盘注视键 保持注视###TargetGazeKeyboard","Press the native keyboard tracking key to latch###TargetGazeKeyboard"),&draftTargetGazeKeyboard);
        panelHelp("使用游戏中实际绑定的注视键，按一下持续注视，再按一下停止。",
            "Uses the game's actual tracking binding: press once to latch tracking and again to stop.");
    }
    ImGui::EndChild();
    if(panelCard("CameraOptions",tr("视角保持","Camera hold"))) {
        ImGui::Checkbox(tr("启用视角保持###CameraHold","Keep camera direction###CameraHold"),&draftCameraHold);
        ImGui::SameLine();ImGui::TextUnformatted(tr("快捷键","Shortcut"));ImGui::SameLine();
        bindingButton(10,shortcutLabel(draftShortcuts[4]),170);
        ImGui::SameLine();if(ImGui::SmallButton(tr("清除###ClearCameraHoldKey","Clear###ClearCameraHoldKey"))){draftShortcuts[4]={0,0};captureSlot=-1;}
        panelHelp("启用后松开环视输入仍保持方向。\nL3按下并松开或键盘回中键回正。长按R3可以开启/关闭此功能。",
            "When enabled, the view retains its direction after look input is released.\nPress and release L3 or use the keyboard recenter binding to recenter. Hold R3 to turn this feature on or off.");
        ImGui::SetNextItemWidth(std::min(340.f,ImGui::GetContentRegionAvail().x*.55f));
        ImGui::SliderInt(tr("视角灵敏度###CameraSensitivity","View sensitivity###CameraSensitivity"),&draftCameraSensitivity,5,200,"%d%%",ImGuiSliderFlags_AlwaysClamp);
        ImGui::TextUnformatted(tr("视角回中 键盘快捷键","Keyboard recenter shortcut"));ImGui::SameLine();
        bindingButton(8,draftCameraCenterKey?keyLabel(draftCameraCenterKey):tr("未绑定","Unassigned"),180);
        ImGui::SameLine();if(ImGui::SmallButton(tr("清除###ClearCenterKey","Clear###ClearCenterKey"))){draftCameraCenterKey=0;captureSlot=-1;}
    }
    ImGui::EndChild();
}
void drawBasicBindings() {
    if(panelCard("DirectKeys",tr("额外的武器切换方式","Additional weapon selection"))) {
        ImGui::TextWrapped("%s",tr("游戏原本的武器切换键会依次切换每个武器。可使用手柄十字键快速选择武器，或自定义键盘快捷键",
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
}
void drawManagementErrors() {
    const char* fields[]={"error","menu_error","hud_error","central_error","settings_error"};
    bool errors=false;for(const char* field:fields)errors=errors||state.contains(field);
    for(const char* field:{"target_gaze","camera_hold","management_pause","gun_salvo","view_target","debug_weapons"})errors=errors||(state.contains(field)&&state.at(field).contains("error"));
    if(errors) {
        ImGui::TextColored({1,.65f,.4f,1},"%s",tr("有功能或保存异常，请展开查看。","A feature or save operation needs attention."));
        if(ImGui::CollapsingHeader(tr("查看异常详情###ManagementErrors","View error details###ManagementErrors"))) {
            for(const char* field:fields)if(state.contains(field))ImGui::TextWrapped("%s",uiMessage(state.at(field).get<std::string>()).c_str());
            for(const char* field:{"target_gaze","camera_hold","management_pause","gun_salvo","view_target","debug_weapons"})if(state.contains(field)&&state.at(field).contains("error"))
                ImGui::TextWrapped("%s",uiMessage(state.at(field).at("error").get<std::string>()).c_str());
        }
    }
}
void drawPanel() {
    const auto display=ImGui::GetIO().DisplaySize;
    const float width=std::min(1080.f,std::max(280.f,display.x-32)),height=std::min(830.f,std::max(240.f,display.y-32));
    ImGui::SetNextWindowPos({display.x*.5f,display.y*.5f},ImGuiCond_Always,{.5f,.5f});ImGui::SetNextWindowSize({width,height});
    ImGui::Begin("AC7MultiSP###DualSPManagement",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoSavedSettings);
    ImGui::TextColored({.37f,.84f,.79f,1},"AC7MultiSP");ImGui::SameLine();ImGui::TextDisabled("2.23.1");
    if(state.value("management_protocol",0)!=managementProtocol||!managementError.empty()) {
        ImGui::TextWrapped("%s",tr("管理组件版本不匹配，请完整安装同一版本。","Management components do not match. Install a complete matching version."));
        if(!managementError.empty())ImGui::TextWrapped("%s",uiMessage(managementError).c_str());
        if(ImGui::Button(tr("关闭","Close")))togglePanel(false);
        ImGui::End();return;
    }
    // 标题单独一行；主开关、管理改键及语言依次排列，窄窗口允许语言换行。
    ImGui::PushStyleColor(ImGuiCol_Button,draftModEnabled?ImVec4{.13f,.43f,.39f,1}:ImVec4{.37f,.20f,.20f,1});
    if(focusManagement){ImGui::SetKeyboardFocusHere();focusManagement=false;}
    if(ImGui::Button(draftModEnabled?tr("模组：开启###ModEnabled","Mod: ON###ModEnabled"):tr("模组：关闭###ModEnabled","Mod: OFF###ModEnabled"),{176,38})) {draftModEnabled=!draftModEnabled;captureSlot=-1;}
    ImGui::PopStyleColor();
    panelHelp("总开关在保存后，于主菜单或单人任务选择页生效。关闭时下方页面变灰；标题开关、保存和关闭始终可用。",
        "Save to apply at the main menu or single-player mission selection. Disabled pages are dimmed; this switch, Save and Close remain available.");
    ImGui::SameLine();ImGui::TextUnformatted(tr("管理界面","Management"));ImGui::SameLine();bindingButton(4,shortcutLabel(draftShortcuts[0]),150);
    panelHelp("手柄Back+Y也可开关管理界面。键盘Tab选项，Enter操作，Ctrl+Tab切页，Esc返回。",
        "Back+Y also toggles management. Tab navigates, Enter activates, Ctrl+Tab changes pages and Esc returns.");
    const float languageWidth=ImGui::CalcTextSize("语言/Language").x+ImGui::GetStyle().ItemSpacing.x+150;
    const float languageStart=ImGui::GetWindowContentRegionMax().x-languageWidth;
    if(ImGui::GetItemRectMax().x-ImGui::GetWindowPos().x+ImGui::GetStyle().ItemSpacing.x<languageStart)ImGui::SameLine(languageStart);
    ImGui::TextUnformatted("语言/Language");ImGui::SameLine();
    int language=draftLanguage=="auto"?0:draftLanguage=="zh"?1:2;
    ImGui::SetNextItemWidth(150);
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
            if(panelCard("DebugLoadout",tr("任务兵装","Mission loadout"))) {
                const auto& options=state.at("debug_weapons");
                bool infinite=pendingDebugWeapons>0?pendingInfiniteAmmo:options.at("infinite").get<bool>();
                bool fast=pendingDebugWeapons>0?pendingFastReload:options.at("fast").get<bool>();
                ImGui::BeginDisabled(!options.at("available").get<bool>()||pendingDebugWeapons>0);
                const bool changedInfinite=ImGui::Checkbox(tr("无限弹药###InfiniteAmmo","Infinite ammo###InfiniteAmmo"),&infinite);
                const bool changedFast=ImGui::Checkbox(tr("快速装填###FastReload","Fast reload###FastReload"),&fast);
                if(changedInfinite || changedFast) {
                    pendingDebugWeapons=++debugWeaponsRequest;
                    pendingInfiniteAmmo=infinite;pendingFastReload=fast;
                    enqueue({{"type","debug_weapons"},{"protocol",1},{"request",pendingDebugWeapons},
                        {"session",state.at("debug_loadout").at("session")},{"revision",state.at("debug_loadout").at("revision")},
                        {"infinite",infinite},{"fast",fast}});
                }
                ImGui::EndDisabled();
                drawDebugLoadout(requestedTab==2);
            }
            ImGui::EndChild();
            if(panelCard("DebugAircraft",tr("更换机体","Change aircraft")))drawDebugAircraft();
            ImGui::EndChild();
            if(panelCard("Logs",tr("日志","Logs"))) {
                if(ImGui::SmallButton(logsExpanded?tr("收起###ToggleLogs","Collapse###ToggleLogs"):tr("展开###ToggleLogs","Expand###ToggleLogs")))logsExpanded=!logsExpanded;
                if(logsExpanded) {
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
            }
            ImGui::EndChild();
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
