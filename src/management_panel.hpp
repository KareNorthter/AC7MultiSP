#pragma once
// 在overlay的私有命名空间内使用；只绘制草稿、帮助和状态，不操作游戏对象。
void panelHelp(const char* key) {
    ImGui::SameLine();ImGui::PushID(key);
    const bool clicked=ImGui::SmallButton("?");
    if((ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)||ImGui::IsItemFocused()) && ImGui::BeginTooltip()) {
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX()+std::min(520.f,ImGui::GetIO().DisplaySize.x-80));
        ImGui::TextUnformatted(uiText(key));ImGui::PopTextWrapPos();ImGui::EndTooltip();
    }
    if(clicked)ImGui::OpenPopup("Help"); // 键盘/手柄也能打开说明，不只依赖鼠标悬停。
    if(ImGui::BeginPopup("Help")) {
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX()+std::min(520.f,ImGui::GetIO().DisplaySize.x-80));
        ImGui::TextUnformatted(uiText(key));ImGui::PopTextWrapPos();ImGui::EndPopup();
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
    const auto text=captureSlot==slot?std::string(uiText("press.a.key.432290b7")):label;
    if(ImGui::Button((text+"###Binding").c_str(),{std::min(width,ImGui::GetContentRegionAvail().x),0})) {captureSlot=slot;notice.clear();}
    ImGui::PopID();
}
bool managementDirty() {
    if(draftPwWeaponUI!=state.at("pw_weapon_ui_enabled").get<bool>())return true;
    return draftHangarMountEnabled!=state.at("hangar_mount_enabled").get<bool>() || draftDpadWeaponSwitchEnabled!=state.at("dpad_weapon_switch_enabled").get<bool>() || draftPadXWeaponSwitchEnabled!=state.at("pad_x_weapon_switch_enabled").get<bool>() || draftGunSalvo!=state.at("gun_salvo_enabled").get<bool>() || draftViewTargetMode!=state.at("view_target_mode").get<std::string>() || draftViewCenterDot!=state.at("view_center_dot_enabled").get<bool>() || draftCentralWeaponDisplay!=state.at("central_weapon_display").get<std::string>() || draftModEnabled!=state.at("mod_enabled").get<bool>() || draftTaskMountEnabled!=state.at("task_mount_enabled").get<bool>() ||
        draftTargetGazePad!=state.at("target_gaze_pad_enabled").get<bool>() || draftTargetGazeKeyboard!=state.at("target_gaze_keyboard_enabled").get<bool>() || draftCameraHold!=state.at("camera_hold_enabled").get<bool>() ||
        draftCameraCenterKey!=state.at("camera_center_key").get<int>() || draftCameraSensitivity!=state.at("camera_sensitivity").get<int>() ||
        draftAmmoPercentTwo!=state.at("ammo_percent_two").get<int>() || draftAmmoPercentThree!=state.at("ammo_percent_three").get<int>() ||
        draftLanguage!=state.at("language").get<std::string>() || draftStartupUpdateCheck!=state.at("startup_update_check_enabled").get<bool>() || Json(draftKeys)!=state.at("keyboard_keys") || shortcutData(draftShortcuts)!=state.at("shortcuts");
}
void drawBasicBindings();
void drawFeatureSettings() {
    if(panelCard("ModelOptions",uiText("model.display.0bf20428"))) {
        ImGui::TextWrapped("%s",uiText("affects.model.display.only.not.b3a3f2f9"));
        ImGui::Checkbox(uiText("use.multi.sp.models.in.f85ff7cd"),&draftTaskMountEnabled);
        panelHelp("replaces.native.aircraft.models.with.d5984a98");
        ImGui::Checkbox(uiText("use.multi.sp.models.in.b06eb8b1"),&draftHangarMountEnabled);
        panelHelp("uses.multi.sp.models.in.903bf914");
        if(state.at("task_mount_enabled")!=state.at("runtime_task_mount_enabled"))ImGui::TextColored({1,.76f,.38f,1},"%s",uiText("mission.model.settings.apply.with.b4148632"));
        if(state.contains("mount_warning"))ImGui::TextWrapped("%s",uiError(state.at("mount_warning").get<std::string>(),"models.reverted.to.native.visuals.4f908718").c_str());
    }
    ImGui::EndChild();
    if(panelCard("MainOptions",uiText("feature.settings.540ec6f5"))) {
        ImGui::Checkbox(uiText("pw.weapon.selection.ui"),&draftPwWeaponUI);
        bool always=draftCentralWeaponDisplay=="always";
        if(ImGui::Checkbox("##CentralWeaponDisplay",&always))draftCentralWeaponDisplay=always?"always":"switch_1s";
        ImGui::SameLine();ImGui::TextWrapped("%s",uiText("always.show.the.current.weapon.84c597b0"));
        if(ImGui::IsItemClicked())draftCentralWeaponDisplay=always?"switch_1s":"always";
        ImGui::TextWrapped("%s",uiText("ammo.per.weapon.with.two.fe1030b7"));
        ImGui::SetNextItemWidth(std::min(340.f,ImGui::GetContentRegionAvail().x));
        ImGui::SliderInt("##AmmoTwo",&draftAmmoPercentTwo,50,100,"%d%%",ImGuiSliderFlags_AlwaysClamp);
        ImGui::TextWrapped("%s",uiText("ammo.per.weapon.with.three.3eca43e5"));
        ImGui::SetNextItemWidth(std::min(340.f,ImGui::GetContentRegionAvail().x));
        ImGui::SliderInt("##AmmoThree",&draftAmmoPercentThree,33,100,"%d%%",ImGuiSliderFlags_AlwaysClamp);
        ImGui::Checkbox(uiText("gun.salvo.gunsalvo.acda2ffc"),&draftGunSalvo);
        ImGui::SameLine();ImGui::TextUnformatted(uiText("shortcut.51cb85e1"));ImGui::SameLine();
        bindingButton(9,shortcutLabel(draftShortcuts[3]),170);
        ImGui::SameLine();if(ImGui::SmallButton(uiText("clear.clearsalvokey.b6037c97"))){draftShortcuts[3]={0,0};captureSlot=-1;}
        panelHelp("with.an.mgp.hgp.plsl.873e6dbb");
        if(state.contains("gun_salvo")&&state.at("gun_salvo").value("reason",std::string())=="multiple_supported_sp")
            ImGui::TextWrapped("%s",uiText("this.loadout.has.multiple.eligible.ccfbf8de"));
    }
    ImGui::EndChild();
    drawBasicBindings();
}
void drawCameraSettings() {
    if(panelCard("ViewTargetOptions",uiText("target.selection.8ff0d833"))) {
        int mode=draftViewTargetMode=="normal"?0:1;
        ImGui::SetNextItemWidth(std::min(340.f,ImGui::GetContentRegionAvail().x*.55f));
        if(ImGui::Combo(uiText("target.selection.mode.nextmode.160bd5fc"),&mode,uiText("native.aircraft.nose.vr.view.d567d9ac")))draftViewTargetMode=mode==0?"normal":"vr";
        panelHelp("native.mode.prioritizes.targets.near.e00f5e88");
        ImGui::Checkbox(uiText("show.the.view.center.dot.80e497f3"),&draftViewCenterDot);
    }
    ImGui::EndChild();
    if(panelCard("TrackingOptions",uiText("latched.target.tracking.11a27755"))) {
        ImGui::Checkbox(uiText("hold.controller.y.to.latch.8a3a68d5"),&draftTargetGazePad);
        panelHelp("when.enabled.hold.controller.y.3329cb01");
        ImGui::Checkbox(uiText("press.the.native.keyboard.tracking.21d621bc"),&draftTargetGazeKeyboard);
        panelHelp("uses.the.game.s.actual.c7dce5e6");
    }
    ImGui::EndChild();
    if(panelCard("CameraOptions",uiText("camera.hold.202916ec"))) {
        ImGui::Checkbox(uiText("keep.camera.direction.camerahold.70008ef1"),&draftCameraHold);
        ImGui::SameLine();ImGui::TextUnformatted(uiText("shortcut.51cb85e1"));ImGui::SameLine();
        bindingButton(10,shortcutLabel(draftShortcuts[4]),170);
        ImGui::SameLine();if(ImGui::SmallButton(uiText("clear.clearcameraholdkey.f782cd6c"))){draftShortcuts[4]={0,0};captureSlot=-1;}
        panelHelp("when.enabled.the.view.retains.446f23c3");
        ImGui::SetNextItemWidth(std::min(340.f,ImGui::GetContentRegionAvail().x*.55f));
        ImGui::SliderInt(uiText("view.sensitivity.camerasensitivity.6d7158ee"),&draftCameraSensitivity,5,200,"%d%%",ImGuiSliderFlags_AlwaysClamp);
        ImGui::TextUnformatted(uiText("keyboard.recenter.shortcut.27495edb"));ImGui::SameLine();
        bindingButton(8,draftCameraCenterKey?keyLabel(draftCameraCenterKey):uiText("unassigned.5dc2dd7c"),180);
        ImGui::SameLine();if(ImGui::SmallButton(uiText("clear.clearcenterkey.d7b5b5b8"))){draftCameraCenterKey=0;captureSlot=-1;}
    }
    ImGui::EndChild();
}
void drawBasicBindings() {
    if(panelCard("DirectKeys",uiText("additional.weapon.selection.3cf00ef8"))) {
        ImGui::TextWrapped("%s",uiText("the.native.weapon.action.cycles.48a65ba7"));
        ImGui::Checkbox(uiText("switch.weapons.with.the.controller.80495bbe"),&draftDpadWeaponSwitchEnabled);
        ImGui::Checkbox(uiText("pad.x.weapon.switch"),&draftPadXWeaponSwitchEnabled);
        if(ImGui::BeginTable("DirectBindings",3,ImGuiTableFlags_SizingStretchProp|ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn(uiText("weapon.f84ecfb3"));ImGui::TableSetupColumn(uiText("keyboard.1725fafc"));ImGui::TableSetupColumn(uiText("controller.90ab65bf"));ImGui::TableHeadersRow();
            const char* weapons[]={"MSL","SP1","SP2","SP3"};
            const char* pads[]={uiText("d.pad.left.c0c98dc7"),uiText("d.pad.up.27c3b621"),uiText("d.pad.right.df4b76a1"),uiText("d.pad.down.42a7c73b")};
            for(int i=0;i<4;++i) {
                ImGui::TableNextRow();ImGui::TableNextColumn();ImGui::TextUnformatted(weapons[i]);
                ImGui::TableNextColumn();bindingButton(i,keyLabel(draftKeys[i]),150);
                ImGui::TableNextColumn();if(draftDpadWeaponSwitchEnabled)ImGui::TextUnformatted(pads[i]);else ImGui::TextDisabled("%s",uiText("not.assigned.51fe00e3"));
            }
            ImGui::EndTable();
        }
    }
    ImGui::EndChild();
}
std::string weaponCatalogueReason(const Json& issue) {
    const auto key=issue.value("reason_key",std::string());
    auto reason=key.empty()?uiError(issue.value("reason",std::string())):std::string(uiText(key.c_str()));
    const auto at=reason.find("{weapon}");
    if(at!=std::string::npos)reason.replace(at,8,issue.value("weapon",std::string()));
    return reason;
}
void drawManagementErrors() {
    std::string recoveryDetail;
    if(state.contains("recovery")&&state.at("recovery").value("disabled",false)) {
        const auto& recovery=state.at("recovery");
        ImGui::TextWrapped("%s",recoveryText(recovery.value("severe",false)?"recovery.severe":
            recovery.value("must_leave",false)?"recovery.leave_mission":"recovery.aircraft").c_str());
        const auto reason=recovery.value("reason",std::string());
        if(!recovery.value("severe",false)&&recovery.contains("issue"))
            ImGui::TextWrapped("%s",weaponCatalogueReason(recovery.at("issue")).c_str());
        else if(!reason.empty())recoveryDetail=uiError(reason);
    }
    const char* fields[]={"error","menu_error","hud_error","central_error","settings_error"};
    bool errors=!recoveryDetail.empty();for(const char* field:fields)errors=errors||state.contains(field);
    for(const char* field:{"target_gaze","camera_hold","management_pause","gun_salvo","view_target","debug_weapons"})errors=errors||(state.contains(field)&&state.at(field).contains("error"));
    if(errors) {
        ImGui::TextColored({1,.65f,.4f,1},"%s",uiText("a.feature.or.save.operation.2aa58920"));
        if(ImGui::CollapsingHeader(uiText("view.error.details.managementerrors.3daf489c"))) {
            bool recoveryShown=false;
            const auto showDetail=[&](const std::string& raw) {
                const auto detail=uiError(raw);
                ImGui::TextWrapped("%s",detail.c_str());
                recoveryShown=recoveryShown||detail==recoveryDetail;
            };
            for(const char* field:fields)if(state.contains(field))showDetail(state.at(field).get<std::string>());
            for(const char* field:{"target_gaze","camera_hold","management_pause","gun_salvo","view_target","debug_weapons"})if(state.contains(field)&&state.at(field).contains("error"))
                showDetail(state.at(field).at("error").get<std::string>());
            // 仅有恢复原因、没有其他错误字段时，仍可在详情中查看；与已有详情相同时不重复。
            if(!recoveryDetail.empty()&&!recoveryShown)ImGui::TextWrapped("%s",recoveryDetail.c_str());
        }
    }
}
void drawWeaponCatalogueIssues() {
    if(!state.contains("recovery"))return;
    const auto& recovery=state.at("recovery");
    const int count=recovery.value("issue_count",0);
    if(count<=0)return;
    const std::string title=std::string(uiText("recovery.catalogue_title"))+"###WeaponCatalogueIssues";
    if(ImGui::Button(recoveryText("recovery.catalogue_count",std::to_string(count)).c_str()))
        ImGui::OpenPopup(title.c_str());
    const auto display=ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowSize({std::min(760.f,std::max(260.f,display.x-48)),
        std::min(570.f,std::max(180.f,display.y-48))},ImGuiCond_Appearing);
    if(ImGui::BeginPopupModal(title.c_str(),nullptr,ImGuiWindowFlags_NoSavedSettings)) {
        ImGui::TextWrapped("%s",uiText("recovery.catalogue_help"));
        ImGui::Separator();
        ImGui::BeginChild("WeaponCatalogueRows",{0,-ImGui::GetFrameHeightWithSpacing()},ImGuiChildFlags_Borders);
        if(recovery.contains("issues")&&recovery.at("issues").is_array())for(const auto& issue:recovery.at("issues")) {
            std::string identity="#"+std::to_string(issue.value("id",-1))+" / "+issue.value("code",std::string());
            if(issue.contains("slot"))identity+=" / SP"+std::to_string(issue.at("slot").get<int>());
            if(issue.contains("weapon"))identity+=" / "+issue.at("weapon").get<std::string>();
            ImGui::TextWrapped("%s",identity.c_str());
            ImGui::TextWrapped("%s",weaponCatalogueReason(issue).c_str());
            ImGui::Separator();
        }
        ImGui::EndChild();
        if(ImGui::Button(uiText("recovery.close")))ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
}
#include "version_updates_panel.hpp"
struct ManagementWatermark {
    struct Part {std::string label;int author;int action=-1;};
    std::vector<std::vector<Part>> rows;
    float font{},noticeHeight{},height{},width{};
    bool chinese{};
};
float watermarkPartWidth(const ManagementWatermark::Part& part) {
    return ImGui::CalcTextSize(part.label.c_str()).x+(part.action>=0?ImGui::GetStyle().FramePadding.x*2:0);
}
float watermarkRowHeight(const std::vector<ManagementWatermark::Part>& row) {
    for(const auto& part:row)if(part.action>=0)return ImGui::GetFrameHeight();
    return ImGui::GetTextLineHeight();
}
ManagementWatermark managementWatermark(float width) {
    namespace documents=dualsp::updates::documents;
    // 作者区使用正文基础字号；按钮和作者链接按实测尺寸换行，页脚预留真实高度。
    ManagementWatermark result;result.width=width;result.font=ImGui::GetStyle().FontSizeBase;
    result.chinese=std::string(tr("zh","en"))=="zh";
    ImGui::PushFont(nullptr,result.font);
    std::vector<ManagementWatermark::Part> parts={{documents::compactAuthorHeading(result.chinese),-1}};
    for(size_t i=0;i<documents::compactAuthorCount(result.chinese);++i)
        parts.push_back({std::string("• ")+documents::compactAuthorLabel(i,result.chinese),static_cast<int>(i)});
    parts.push_back({uiText("author.homepage.44eb1411"),-1,0});
    parts.push_back({uiText("author.coffee.f1fa8278"),-1,1});
    float longest=0;
    for(const auto& part:parts)longest=std::max(longest,watermarkPartWidth(part));
    if(longest>width) {
        result.font*=std::max(1.f,width-ImGui::GetStyle().FramePadding.x*2)/longest;
        ImGui::PopFont();ImGui::PushFont(nullptr,result.font);
    }
    const float gap=ImGui::GetStyle().ItemSpacing.x;
    result.noticeHeight=ImGui::CalcTextSize(documents::compactNotice(result.chinese),nullptr,false,width).y;
    result.rows.emplace_back();float rowWidth=0;
    for(auto& part:parts) {
        const float itemWidth=watermarkPartWidth(part);
        if(rowWidth && rowWidth+gap+itemWidth>width) {result.rows.emplace_back();rowWidth=0;}
        if(rowWidth)rowWidth+=gap;
        result.rows.back().push_back(std::move(part));rowWidth+=itemWidth;
    }
    result.height=result.noticeHeight;
    for(const auto& row:result.rows)result.height+=ImGui::GetStyle().ItemSpacing.y+watermarkRowHeight(row);
    ImGui::PopFont();return result;
}
void drawManagementWatermark(const ManagementWatermark& mark,float right,float top) {
    namespace documents=dualsp::updates::documents;
    ImGui::PushFont(nullptr,mark.font);
    const char* notice=documents::compactNotice(mark.chinese);
    const float noticeWidth=std::min(mark.width,ImGui::CalcTextSize(notice).x);
    ImGui::SetCursorPos({right-noticeWidth,top});ImGui::PushTextWrapPos(right);
    ImGui::TextDisabled("%s",notice);ImGui::PopTextWrapPos();
    float y=top+mark.noticeHeight+ImGui::GetStyle().ItemSpacing.y;
    for(const auto& row:mark.rows) {
        float width=0;const float rowHeight=watermarkRowHeight(row);
        for(const auto& part:row)width+=watermarkPartWidth(part);
        if(!row.empty())width+=(row.size()-1)*ImGui::GetStyle().ItemSpacing.x;
        ImGui::SetCursorPos({right-width,y});bool first=true;
        for(const auto& part:row) {
            if(!first)ImGui::SameLine();first=false;
            if(part.action>=0) {
                ImGui::PushID(100+part.action);
                if(ImGui::Button(part.label.c_str()))dualsp::updates::service().openAuthorAction(static_cast<size_t>(part.action));
                ImGui::PopID();
            }else {
                if(rowHeight>ImGui::GetTextLineHeight())ImGui::AlignTextToFramePadding();
                if(part.author<0)ImGui::TextDisabled("%s",part.label.c_str());
                else {
                    ImGui::PushID(part.author);
                    if(ImGui::TextLink(part.label.c_str()))dualsp::updates::service().openAuthor(static_cast<size_t>(part.author),mark.chinese);
                    ImGui::PopID();
                }
            }
        }
        y+=rowHeight+ImGui::GetStyle().ItemSpacing.y;
    }
    ImGui::PopFont();
}
void drawPanel() {
    const auto display=ImGui::GetIO().DisplaySize;
    const float width=std::min(1080.f,std::max(280.f,display.x-32)),height=std::min(830.f,std::max(240.f,display.y-32));
    ImGui::SetNextWindowPos({display.x*.5f,display.y*.5f},ImGuiCond_Always,{.5f,.5f});ImGui::SetNextWindowSize({width,height});
    ImGui::Begin("AC7MultiSP###DualSPManagement",nullptr,ImGuiWindowFlags_NoDecoration|
        ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::TextColored({.37f,.84f,.79f,1},"AC7MultiSP");ImGui::SameLine();ImGui::TextDisabled("%s",dualsp::updates::currentVersion);
    if(state.value("management_protocol",0)!=managementProtocol||!managementError.empty()) {
        ImGui::TextWrapped("%s",uiText("management.components.do.not.match.9ae559c4"));
        if(!managementError.empty())ImGui::TextWrapped("%s",uiError(managementError).c_str());
        if(ImGui::Button(uiText("close.f09d5099")))togglePanel(false);
        const auto watermark=managementWatermark(ImGui::GetWindowContentRegionMax().x-ImGui::GetCursorPosX());
        drawManagementWatermark(watermark,ImGui::GetWindowContentRegionMax().x,
            ImGui::GetWindowContentRegionMax().y-watermark.height);
        ImGui::End();return;
    }
    // 标题单独一行；主开关、管理改键及语言依次排列，窄窗口允许语言换行。
    ImGui::PushStyleColor(ImGuiCol_Button,draftModEnabled?ImVec4{.13f,.43f,.39f,1}:ImVec4{.37f,.20f,.20f,1});
    if(focusManagement){ImGui::SetKeyboardFocusHere();focusManagement=false;}
    if(ImGui::Button(draftModEnabled?uiText("mod.on.modenabled.bbe2f274"):uiText("mod.off.modenabled.000955e4"),{176,38})) {draftModEnabled=!draftModEnabled;captureSlot=-1;}
    ImGui::PopStyleColor();
    panelHelp("save.to.apply.at.the.3d03eef2");
    ImGui::SameLine();ImGui::TextUnformatted(uiText("management.6009aa3c"));ImGui::SameLine();bindingButton(4,shortcutLabel(draftShortcuts[0]),150);
    panelHelp("back.y.also.toggles.management.b588258e");
    const float languageWidth=ImGui::CalcTextSize(uiText("language.bc9f0319")).x+ImGui::GetStyle().ItemSpacing.x+150;
    const float languageStart=ImGui::GetWindowContentRegionMax().x-languageWidth;
    if(ImGui::GetItemRectMax().x-ImGui::GetWindowPos().x+ImGui::GetStyle().ItemSpacing.x<languageStart)ImGui::SameLine(languageStart);
    ImGui::TextUnformatted(uiText("language.bc9f0319"));ImGui::SameLine();
    int language=draftLanguage=="auto"?0:draftLanguage=="zh"?1:2;
    ImGui::SetNextItemWidth(150);
    if(ImGui::Combo("##Language",&language,uiText("game.language.chinese.english.c0b892a6")))draftLanguage=language==0?"auto":language==1?"zh":"en";
    if(state.at("mod_enabled")!=state.at("runtime_mod_enabled"))ImGui::TextColored({1,.76f,.38f,1},"%s",uiText("saved.return.to.the.main.4b8dc61e"));
    if(state.contains("master_switch")&&state.at("master_switch").contains("error"))ImGui::TextWrapped("%s",uiError(state.at("master_switch").at("error").get<std::string>()).c_str());
    if(state.contains("management_pause")) {
        const auto& pause=state.at("management_pause");
        ImGui::TextDisabled("%s",pause.value("existing",false)?uiText("game.is.paused.48376114"):pause.value("owned",false)?uiText("close.to.resume.flight.ddb47cd0"):uiText("save.to.apply.settings.222ac713"));
        panelHelp("management.releases.only.its.own.d4015e85");
    }
    ImGui::Separator();
    // 正文与固定页脚分开；保留左侧保存/关闭顺序，水印贴右，窄窗口移到固定下行。
    const float footerRight=ImGui::GetWindowContentRegionMax().x;
    const float footerLeft=ImGui::GetCursorPosX(),footerWidth=footerRight-footerLeft;
    const bool stackedFooter=footerWidth<760;
    const float saveWidth=std::min(168.f,std::max(100.f,footerWidth*.5f));
    const float closeWidth=std::min(100.f,std::max(58.f,footerWidth*.25f));
    const float gap=ImGui::GetStyle().ItemSpacing.x;
    const auto watermark=managementWatermark(stackedFooter?footerWidth:
        footerWidth-saveWidth-closeWidth-gap*4-30);
    const float statusHeight=ImGui::GetFrameHeightWithSpacing();
    const float actionHeight=std::max(36.f,stackedFooter?36.f:watermark.height);
    const float footerHeight=statusHeight+ImGui::GetTextLineHeightWithSpacing()+actionHeight+
        (stackedFooter?watermark.height+ImGui::GetStyle().ItemSpacing.y:0)+ImGui::GetStyle().ItemSpacing.y*2;
    const float footerTop=ImGui::GetWindowContentRegionMax().y-footerHeight;
    ImGui::BeginChild("ManagementBody",{0,std::max(1.f,footerTop-ImGui::GetCursorPosY()-ImGui::GetStyle().ItemSpacing.y)},ImGuiChildFlags_None);
    drawManagementErrors();
    if(state.value("mission_fixed_sp",false))ImGui::TextColored({1,.76f,.38f,1},"%s",uiText("mission.13.assigned.sp.camera.6fa3a5e0"));
    // 总开关不锁分页与诊断；功能动作继续按已发布的实际可用状态禁用。
    if(ImGui::BeginTabBar("ManagementTabs")) {
        if(beginManagementTab(uiText("general.features.d6795dc0"),0)) {drawWeaponCatalogueIssues();drawFeatureSettings();ImGui::EndTabItem();}
        if(beginManagementTab(uiText("camera.bindings.291ded16"),1)) {drawCameraSettings();ImGui::EndTabItem();}
        if(beginManagementTab(uiText("debug.debug.3211ed36"),2)) {
            if(panelCard("DebugLoadout",uiText("mission.loadout.682b8e9c"))) {
                const auto& options=state.at("debug_weapons");
                bool infinite=pendingDebugWeapons>0?pendingInfiniteAmmo:options.at("infinite").get<bool>();
                bool fast=pendingDebugWeapons>0?pendingFastReload:options.at("fast").get<bool>();
                ImGui::BeginDisabled(!options.at("available").get<bool>()||pendingDebugWeapons>0);
                const bool changedInfinite=ImGui::Checkbox(uiText("infinite.ammo.infiniteammo.6a8db67a"),&infinite);
                const bool changedFast=ImGui::Checkbox(uiText("fast.reload.fastreload.645bd7ab"),&fast);
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
            if(panelCard("DebugAircraft",uiText("change.aircraft.6b159313")))drawDebugAircraft();
            ImGui::EndChild();
            if(panelCard("QuickMission",uiText("quick.title")))drawQuickMission();
            ImGui::EndChild();
            if(panelCard("Logs",uiText("logs.2c322bb4"))) {
                if(ImGui::SmallButton(logsExpanded?uiText("collapse.togglelogs.01eaf7d4"):uiText("expand.togglelogs.85549964")))logsExpanded=!logsExpanded;
                if(logsExpanded) {
                    if(!logsLoaded)refreshLogs();
                    if(ImGui::Button(uiText("refresh.refreshlogs.423e6a9d")))refreshLogs();
                    panelHelp("shows.the.last.64.kib.4ded2bc8");
                    if(!logError.empty())ImGui::TextWrapped("%s",uiError(logError).c_str());
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
        if(beginManagementTab(uiText("version.and.updates.versionupdates.f6da021a"),3)) {drawVersionUpdatesPage();ImGui::EndTabItem();}
        ImGui::EndTabBar();if(requestedTab==managementTab)requestedTab=-1;
    }
    ImGui::EndChild();ImGui::SetCursorPosY(footerTop);ImGui::Separator();
    ImGui::TextDisabled("%s",captureSlot>=0?uiText("waiting.for.a.key.esc.0bd06836"):pendingSave>0?uiText("saving.2a26d205"):managementDirty()?uiText("unsaved.changes.82a5cd03"):uiText("settings.saved.d104ef7d"));
    if(captureSlot>=0){ImGui::SameLine();if(ImGui::SmallButton(uiText("cancel.capture.1c5192e2")))captureSlot=-1;}
    ImGui::BeginChild("ManagementFooterMessage",{0,ImGui::GetTextLineHeight()},ImGuiChildFlags_None,
        ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse);
    if(!bindingsValid())ImGui::TextColored({1,.65f,.4f,1},"%s",uiText("a.binding.is.invalid.or.8eccbcf5"));
    else if(!notice.empty()&&notice!=dualsp::user_texts::get("settings.saved.ce3bf5a0",true)&&notice!=dualsp::user_texts::get("settings.saved.ce3bf5a0",false)&&notice!=dualsp::user_texts::get("saving.2a26d205",true)&&notice!=dualsp::user_texts::get("saving.2a26d205",false))
        ImGui::TextUnformatted(uiMessage(notice).c_str()); // 通常保存状态已在上行展示，避免修改草稿后留下旧成功提示。
    else if(state.contains("input_notice"))ImGui::TextDisabled("%s",state.at("input_notice").get<std::string>()=="empty_sp"?
        uiText("this.sp.is.empty.selection.92f6b119"):uiText("this.sp.is.depleted.selection.c47a85ad"));
    if(ImGui::IsItemHovered() && ImGui::BeginTooltip()) {
        ImGui::PushTextWrapPos(ImGui::GetFontSize()*24);
        if(!bindingsValid())ImGui::TextUnformatted(uiText("a.binding.is.invalid.or.8eccbcf5"));
        else if(!notice.empty())ImGui::TextUnformatted(uiMessage(notice).c_str());
        ImGui::PopTextWrapPos();ImGui::EndTooltip();
    }
    ImGui::EndChild();
    const float actionTop=footerTop+statusHeight+ImGui::GetTextLineHeightWithSpacing()+ImGui::GetStyle().ItemSpacing.y;
    const float watermarkTop=actionTop+(stackedFooter?36.f+ImGui::GetStyle().ItemSpacing.y:0);
    drawManagementWatermark(watermark,footerRight,watermarkTop);
    ImGui::SetCursorPos({footerLeft,actionTop});
    ImGui::BeginDisabled(captureSlot>=0||!bindingsValid()||pendingSave>0);
    if(ImGui::Button(uiText("save.settings.savesettings.b8bcac13"),{saveWidth,36}))saveManagementSettings();
    ImGui::EndDisabled();ImGui::SameLine();
    if(ImGui::Button(uiText("close.closemanagement.542d61d9"),{closeWidth,36}))togglePanel(false);
    panelHelp("closing.discards.unsaved.changes.language.585b36b5");
    ImGui::End();
}
