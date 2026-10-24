#pragma once
// 与管理页共用语言和卡片；玩家正文来自集中生成资源。
void updateDownloadSameLine(const char* label) {
    const float width=ImGui::CalcTextSize(label).x+ImGui::GetStyle().FramePadding.x*2;
    if(ImGui::GetItemRectMax().x+ImGui::GetStyle().ItemSpacing.x+width<=
        ImGui::GetWindowPos().x+ImGui::GetWindowContentRegionMax().x)ImGui::SameLine();
}
void drawVersionUpdatesPage() {
    auto& updates=dualsp::updates::service();
    const auto status=updates.status();
    const auto snapshot=updates.snapshot();
    if(ImGui::BeginChild("UpdateVersions",{0,0},ImGuiChildFlags_Borders|ImGuiChildFlags_AutoResizeY)) {
        ImGui::Checkbox(uiText("updates.check_on_startup"),&draftStartupUpdateCheck);
        ImGui::Text("%s  %s",uiText("current.version.16c8badf"),dualsp::updates::currentVersion);
        const std::string latest=std::string(uiText("latest.version.51f76219"))+"  "+
            (snapshot && !snapshot->latest.empty()?snapshot->latest:"—");
        updateDownloadSameLine(latest.c_str());ImGui::TextUnformatted(latest.c_str());
        updateDownloadSameLine(uiText("check.for.updates.840f7526"));
        ImGui::BeginDisabled(updates.checking());
        if(ImGui::Button(uiText("check.for.updates.840f7526")))updates.requestCheck();
        ImGui::EndDisabled();
        const char* title="—";
        if(status==dualsp::updates::Status::Checking) {
            title=uiText("checking.for.updates.a6e73905");
        }else if(status==dualsp::updates::Status::Current) {
            title=uiText("already.up.to.date.9c218466");
        }else if(status==dualsp::updates::Status::Newer)title=uiText("a.newer.version.is.available.274799a7");
        else if(status==dualsp::updates::Status::Unpublished) {
            title=uiText("update.information.has.not.been.4e698d95");
        }
        else if(status==dualsp::updates::Status::Failed) {
            title=uiText("unable.to.check.for.updates.f4fdc2e6");
        }
        ImGui::TextWrapped("%s",title);
        // 下载区域只在新版有效时出现，R2优先，其余按原清单顺序编号。
        if(status==dualsp::updates::Status::Newer && snapshot && !snapshot->downloads.empty()) {
            ImGui::BeginDisabled(updates.browserStatus()==dualsp::updates::BrowserStatus::Opening);
            bool first=true;size_t backup=0;
            for(bool primary:{true,false})for(const auto& link:snapshot->downloads) {
                if((link.id=="r2")!=primary)continue;
                const std::string label=primary?uiText("primary.download.link.f6096f63"):
                    std::string(uiText("backup.download.link.76fd22fe"))+std::to_string(++backup);
                if(!first)updateDownloadSameLine(label.c_str());first=false;
                ImGui::PushID(link.id.c_str());
                if(ImGui::Button(label.c_str()))updates.openDownload(link.id);
                ImGui::PopID();
            }
            ImGui::EndDisabled();
        }
    }
    ImGui::EndChild();
    const bool chinese=std::string(tr("zh","en"))=="zh";
    // 反馈正文与作者授权同源，合并换行后作为紧凑栏；窄窗口允许自然换行。
    std::string feedback=dualsp::updates::documents::compactFeedback(chinese);
    feedback.erase(std::remove(feedback.begin(),feedback.end(),'\r'),feedback.end());
    std::replace(feedback.begin(),feedback.end(),'\n',' ');
    ImGui::PushFont(nullptr,std::min(ImGui::GetStyle().FontSizeBase,14.f));
    const char* logLabel=uiText("open.mod.log.folder.06bdb703");
    const float logWidth=ImGui::CalcTextSize(logLabel).x+ImGui::GetStyle().FramePadding.x*2;
    const float feedbackWidth=ImGui::GetContentRegionAvail().x-logWidth-ImGui::GetStyle().ItemSpacing.x;
    const bool inlineFeedback=feedbackWidth>=260;
    ImGui::PushTextWrapPos(inlineFeedback?ImGui::GetCursorPosX()+feedbackWidth:0);
    ImGui::TextDisabled("%s",feedback.c_str());ImGui::PopTextWrapPos();
    if(inlineFeedback)ImGui::SameLine(ImGui::GetWindowContentRegionMax().x-logWidth);
    ImGui::BeginDisabled(updates.browserStatus()==dualsp::updates::BrowserStatus::Opening);
    if(ImGui::SmallButton(logLabel))updates.openLogFolder();
    ImGui::EndDisabled();ImGui::PopFont();
    if(updates.browserStatus()==dualsp::updates::BrowserStatus::Failed)
        ImGui::TextWrapped("%s",uiText("unable.to.open.link.or.folder.5705a221"));
    // 本地玩家正文只展示主要更新和作者授权，保持原有展开/收起交互。
    if(panelCard("UpdateChangelog",uiText("changelog.fd31c92d"))) {
        static bool expanded=false;
        if(ImGui::SmallButton(expanded?uiText("collapse.changelogtoggle.6286d892"):
            uiText("expand.changelogtoggle.d2b52d09")))expanded=!expanded;
        if(expanded)ImGui::TextWrapped("%s",dualsp::updates::documents::changelog(std::string(tr("zh","en"))=="zh"));
    }
    ImGui::EndChild();
    if(panelCard("UpdateLicense",uiText("author.and.license.5de4e76d"))) {
        static bool expanded=false;
        if(ImGui::SmallButton(expanded?uiText("collapse.updatelicensetoggle.aa7b68a7"):
            uiText("expand.updatelicensetoggle.ebfd8718")))expanded=!expanded;
        if(expanded)ImGui::TextWrapped("%s",dualsp::updates::documents::credits(std::string(tr("zh","en"))=="zh"));
    }
    ImGui::EndChild();
}
