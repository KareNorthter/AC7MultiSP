#pragma once
// 与管理页共用语言和卡片；中文按用户2026-10-22回传稿保存，#意见不进入显示。
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
        ImGui::Text("%s  %s",uiText("current.version.16c8badf"),dualsp::updates::currentVersion);
        ImGui::Text("%s  %s",uiText("latest.version.51f76219"),snapshot && !snapshot->latest.empty()?snapshot->latest.c_str():"—");
        ImGui::BeginDisabled(updates.checking());
        if(ImGui::Button(uiText("check.for.updates.840f7526")))updates.requestCheck();
        ImGui::EndDisabled();
        const char* title="—",*hint="";
        if(status==dualsp::updates::Status::Checking) {
            title=uiText("checking.for.updates.a6e73905");hint=uiText("you.can.continue.playing.3acf66f6");
        }else if(status==dualsp::updates::Status::Current) {
            title=uiText("already.up.to.date.9c218466");hint=uiText("no.update.is.needed.f6dc0c26");
        }else if(status==dualsp::updates::Status::Newer)title=uiText("a.newer.version.is.available.274799a7");
        else if(status==dualsp::updates::Status::Unpublished) {
            title=uiText("update.information.has.not.been.4e698d95");
            hint=uiText("you.can.continue.playing.and.5cfb85ee");
        }
        else if(status==dualsp::updates::Status::Failed) {
            title=uiText("unable.to.check.for.updates.f4fdc2e6");
            hint=uiText("this.check.has.ended.you.137d7d95");
        }
        ImGui::Separator();ImGui::TextWrapped("%s",title);
        if(status==dualsp::updates::Status::Newer && snapshot)
            ImGui::TextWrapped("%s",tr(snapshot->notesZh.c_str(),snapshot->notesEn.c_str()));
        else if(*hint)ImGui::TextWrapped("%s",hint);
    }
    ImGui::EndChild();
    if(panelCard("UpdateDownloads",uiText("download.update.22da3701"))) {
        const bool opening=updates.browserStatus()==dualsp::updates::BrowserStatus::Opening;
        const bool available=snapshot && !snapshot->downloads.empty();
        ImGui::BeginDisabled(!available || opening);
        if(available) {
            for(size_t i=0;i<snapshot->downloads.size();++i) {
                const auto& link=snapshot->downloads[i];
                const char* label=link.id=="lanzou"?uiText("lanzou.download.edd106d4"):
                    link.id=="r2"?uiText("cloudflarer2.download.1a0af643"):tr(link.zh.c_str(),link.en.c_str());
                if(i)updateDownloadSameLine(label);
                ImGui::PushID(link.id.c_str());
                if(ImGui::Button(label))updates.openDownload(link.id);
                ImGui::PopID();
            }
        }else {
            ImGui::Button(uiText("lanzou.download.edd106d4"));updateDownloadSameLine(uiText("cloudflarer2.download.1a0af643"));
            ImGui::Button(uiText("cloudflarer2.download.1a0af643"));
        }
        ImGui::EndDisabled();
        ImGui::TextWrapped("%s",uiText("download.buttons.open.the.corresponding.687ea42f"));
        ImGui::TextWrapped("%s",uiText("after.downloading.exit.the.game.aaa43341"));
    }
    ImGui::EndChild();
    // 本地玩家正文只展示主要更新和作者授权；远端更新摘要仍作为外部数据显示。
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
