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
    if(panelCard("UpdateAuthor",tr("作者信息","Author information"))) {
        ImGui::TextUnformatted(tr("NexusMod：Northter","NexusMod: Northter"));
        ImGui::TextUnformatted(tr("bilibili：班德慕","bilibili: 班德慕"));
        ImGui::TextWrapped("%s",tr("邮箱：karenorthterofchina@gmail.com","Email: karenorthterofchina@gmail.com"));
        ImGui::TextWrapped("%s",tr("测试反馈QQ群：1072924682","Test feedback QQ group: 1072924682"));
    }
    ImGui::EndChild();
    if(ImGui::BeginChild("UpdateVersions",{0,0},ImGuiChildFlags_Borders|ImGuiChildFlags_AutoResizeY)) {
        ImGui::Text("%s  %s",tr("当前版本","Current version"),dualsp::updates::currentVersion);
        ImGui::Text("%s  %s",tr("最新版本","Latest version"),snapshot?snapshot->latest.c_str():"—");
        const char* title="—",*hint="";
        if(status==dualsp::updates::Status::Checking) {
            title=tr("正在检查更新…","Checking for updates...");hint=tr("你可以继续游戏。","You can continue playing.");
        }else if(status==dualsp::updates::Status::Current) {
            title=tr("已是最新版本","Already up to date");hint=tr("无需更新。","No update is needed.");
        }else if(status==dualsp::updates::Status::Newer)title=tr("发现新版本","A newer version is available");
        else if(status==dualsp::updates::Status::Failed) {
            title=tr("暂时无法检查更新","Unable to check for updates at the moment");
            hint=tr("本次检查已结束，不影响继续游戏。","This check has ended. You can continue playing.");
        }
        ImGui::Separator();ImGui::TextWrapped("%s",title);
        if(status==dualsp::updates::Status::Newer && snapshot)
            ImGui::TextWrapped("%s",tr(snapshot->notesZh.c_str(),snapshot->notesEn.c_str()));
        else if(*hint)ImGui::TextWrapped("%s",hint);
    }
    ImGui::EndChild();
    if(panelCard("UpdateDownloads",tr("下载更新","Download update"))) {
        const bool opening=updates.browserStatus()==dualsp::updates::BrowserStatus::Opening;
        ImGui::BeginDisabled(!snapshot || opening);
        if(snapshot) {
            for(size_t i=0;i<snapshot->downloads.size();++i) {
                const auto& link=snapshot->downloads[i];
                const char* label=link.id=="lanzou"?tr("蓝奏云下载","Lanzou download"):
                    link.id=="r2"?tr("CloudFlareR2 下载","CloudFlareR2 download"):tr(link.zh.c_str(),link.en.c_str());
                if(i)updateDownloadSameLine(label);
                ImGui::PushID(link.id.c_str());
                if(ImGui::Button(label))updates.openDownload(link.id);
                ImGui::PopID();
            }
        }else {
            ImGui::Button(tr("蓝奏云下载","Lanzou download"));updateDownloadSameLine(tr("CloudFlareR2 下载","CloudFlareR2 download"));
            ImGui::Button(tr("CloudFlareR2 下载","CloudFlareR2 download"));
        }
        ImGui::EndDisabled();
        ImGui::TextWrapped("%s",tr("点击下载按钮，将使用 Windows 默认浏览器打开对应入口。",
            "Download buttons open the corresponding link in your default Windows browser."));
        ImGui::TextWrapped("%s",tr("下载完成后，请先退出游戏，再按压缩包内的说明解压安装。",
            "After downloading, exit the game, then extract and install the update as described in the archive."));
    }
    ImGui::EndChild();
    if(panelCard("UpdateLicense",tr("授权信息","License information"))) {
        static bool expanded=false;
        if(ImGui::SmallButton(expanded?tr("收起###UpdateLicenseToggle","Collapse###UpdateLicenseToggle"):
            tr("展开###UpdateLicenseToggle","Expand###UpdateLicenseToggle")))expanded=!expanded;
        if(expanded)ImGui::TextWrapped("%s",tr(dualsp::updates::creditsZh,dualsp::updates::creditsEn));
    }
    ImGui::EndChild();
}
