#pragma once
// 快速任务只提交请求；实际选机、地图交接及兵装确认在游戏线程执行。
Json quickDraft;int quickGeneration=-1,quickRequest=0,quickPending=0;
std::array<char,160> quickSearch{};
void loadQuickMissionDraft() {
    if(!state.contains("quick_mission")){quickDraft=Json();return;}
    const auto& data=state.at("quick_mission");
    if(data.value("protocol",0)!=2){quickDraft=Json();return;}
    quickRequest=std::max(quickRequest,data.at("response").get<int>());
    if(quickPending && data.at("response").get<int>()>=quickPending)quickPending=0;
    const int generation=data.at("generation").get<int>();
    if(quickDraft.is_null()||generation!=quickGeneration)quickDraft=data.at("preset");
    quickGeneration=generation;
}
void submitQuickMission(const char* action) {
    quickPending=++quickRequest;
    enqueue({{"type",action},{"quick_protocol",2},{"request",quickPending},
        {"generation",quickGeneration},{"preset",quickDraft}});
}
std::string quickSkinLabel(int skin) {
    if(skin==0)return uiText("quick.skin_default");
    char label[128]{};std::snprintf(label,sizeof(label),uiText("quick.skin_number"),skin);return label;
}
std::string quickMissionLabel(const Json& task) {
    const auto code=task.value("code",std::string());
    if(code=="ex01"||code=="ex02"||code=="ex03") {
        char label[128]{};std::snprintf(label,sizeof(label),uiText("quick.dlc_task"),code.back()-'0');return label;
    }
    return task.value("label",std::string("--"));
}
void drawQuickMission() {
    if(quickDraft.is_null()){ImGui::TextWrapped("%s",uiText("quick.unavailable"));return;}
    const auto& data=state.at("quick_mission");
    ImGui::TextWrapped("%s",uiText("quick.help"));
    const bool available=data.value("available",false)&&!quickPending;
    const auto& tasks=data.at("tasks");const auto& planes=data.at("planes");
    const Json* task=nullptr;const Json* plane=nullptr;
    for(const auto& row:tasks)if(row.at("mission_id")==quickDraft.at("mission_id"))task=&row;
    for(const auto& row:planes)if(row.at("plane_id")==quickDraft.at("plane_id"))plane=&row;
    ImGui::BeginDisabled(!available);
    ImGui::SetNextItemWidth(std::min(520.f,ImGui::GetContentRegionAvail().x));
    const auto taskPreview=task?quickMissionLabel(*task):std::string("--");
    if(ImGui::BeginCombo(uiText("quick.task"),taskPreview.c_str())) {
        for(const auto& row:tasks) {
            const auto label=quickMissionLabel(row);
            ImGui::BeginDisabled(!row.value("licensed",false));
            if(ImGui::Selectable(label.c_str(),task==&row)) {
                quickDraft["mission_id"]=row.at("mission_id");task=&row;
                if(row.value("fixed_sp",false)&&quickDraft.at("profile").at("a").get<int>()<0)
                    quickDraft["profile"]={{"a",0},{"b",-1},{"c",-1}};
            }
            ImGui::EndDisabled();
        }
        ImGui::EndCombo();
    }
    const auto preview=plane?uiMessage(plane->value("label",std::string())):std::string("--");
    ImGui::SetNextItemWidth(std::min(520.f,ImGui::GetContentRegionAvail().x));
    if(ImGui::BeginCombo(uiText("quick.aircraft"),preview.c_str(),ImGuiComboFlags_HeightLargest)) {
        ImGui::InputTextWithHint("##QuickSearch",uiText("search.name.code.id.a15a68b4"),quickSearch.data(),quickSearch.size());
        const auto query=aircraftSearchKey(quickSearch.data());
        for(const auto& row:planes) {
            const auto label=uiMessage(row.value("label",std::string()))+" ["+std::to_string(row.at("plane_id").get<int>())+"]";
            if(!query.empty()&&aircraftSearchKey(label+row.value("code",std::string())).find(query)==std::string::npos)continue;
            if(ImGui::Selectable(label.c_str(),plane==&row)) {
                quickDraft["plane_id"]=row.at("plane_id");quickDraft["profile"]={{"a",0},{"b",-1},{"c",-1}};
                quickDraft["skin_no"]=0;plane=&row;
            }
        }
        ImGui::EndCombo();
    }
    const Json* skin=nullptr;
    if(plane)for(const auto& row:plane->at("skins"))if(row.at("skin_no")==quickDraft.at("skin_no"))skin=&row;
    const auto skinPreview=skin?quickSkinLabel(skin->at("skin_no").get<int>()):std::string("--");
    ImGui::BeginDisabled(!plane);
    ImGui::SetNextItemWidth(std::min(520.f,ImGui::GetContentRegionAvail().x));
    if(ImGui::BeginCombo(uiText("quick.skin"),skinPreview.c_str())) {
        if(plane)for(const auto& row:plane->at("skins")) {
            const auto label=quickSkinLabel(row.at("skin_no").get<int>());
            ImGui::BeginDisabled(!row.value("available",false)||!row.value("licensed",false));
            if(ImGui::Selectable(label.c_str(),skin==&row)){quickDraft["skin_no"]=row.at("skin_no");skin=&row;}
            ImGui::EndDisabled();
        }
        ImGui::EndCombo();
    }
    ImGui::EndDisabled();
    const bool skinReady=skin&&skin->value("available",false)&&skin->value("licensed",false);
    ImGui::TextWrapped("%s",uiText("quick.skin_help"));
    if(plane&&!skinReady)ImGui::TextWrapped("%s",uiText("quick.skin_unavailable"));
    const bool fixed=task&&task->value("fixed_sp",false);
    ImGui::TextUnformatted(uiText("quick.special_weapons"));
    ImGui::BeginDisabled(fixed||!plane);
    std::array<bool,3> selected{};
    for(const char* field:{"a","b","c"}) {
        const int index=quickDraft.at("profile").at(field).get<int>();
        if(index>=0&&index<3)selected[index]=true;
    }
    std::array<std::string,3> labels;
    bool changed=false;
    for(int i=0;i<3;++i) {
        labels[i]="SP "+std::to_string(i+1);
        if(plane&&plane->at("weapons").size()==3) {
            const auto& weapon=plane->at("weapons").at(i);
            if(weapon.is_string())labels[i]+=" · "+weapon.get<std::string>();
            else if(weapon.is_object()) {
                auto code=weapon.value("code",std::string());
                std::transform(code.begin(),code.end(),code.begin(),[](unsigned char c){return c>='a'&&c<='z'?c-'a'+'A':c;});
                labels[i]+=" · "+code;
            }
        }
        changed=ImGui::Checkbox(labels[i].c_str(),&selected[i])||changed;
    }
    int count=0;for(bool chosen:selected)if(chosen)++count;
    if(changed) {
        std::array<int,3> slots{-1,-1,-1};int next=0;
        for(int i=0;i<3;++i)if(selected[i])slots[next++]=i;
        quickDraft["profile"]={{"a",slots[0]},{"b",slots[1]},{"c",slots[2]}};
    }
    ImGui::EndDisabled();
    if(fixed)ImGui::TextWrapped("%s",uiText("quick.fixed_sp"));
    else ImGui::TextWrapped("%s",uiText(count?"quick.multi_sp_help":"quick.select_sp"));
    ImGui::TextWrapped("%s",uiText("quick.parts_pending"));
    int difficulty=quickDraft.at("difficulty").get<int>();
    ImGui::SetNextItemWidth(240);
    if(ImGui::Combo(uiText("quick.difficulty"),&difficulty,uiText("quick.difficulties")))quickDraft["difficulty"]=difficulty;
    bool automatic=quickDraft.at("auto_launch").get<bool>();
    if(ImGui::Checkbox(uiText("quick.auto"),&automatic))quickDraft["auto_launch"]=automatic;
    ImGui::TextWrapped("%s",uiText("quick.auto_help"));
    bool logos=quickDraft.at("skip_logos").get<bool>();
    if(ImGui::Checkbox(uiText("quick.skip_logos"),&logos))quickDraft["skip_logos"]=logos;
    ImGui::BeginDisabled(count==0||!skinReady);
    if(ImGui::Button(uiText("quick.save")))submitQuickMission("quick_mission_save");
    ImGui::SameLine();ImGui::BeginDisabled(!task||!plane||!task->value("licensed",false));
    if(ImGui::Button(uiText("quick.start")))submitQuickMission("quick_mission_start");
    ImGui::EndDisabled();ImGui::EndDisabled();ImGui::EndDisabled();
    if(data.value("auto_pending",false)&&ImGui::Button(uiText("quick.cancel_auto")))submitQuickMission("quick_mission_cancel_auto");
    const auto reason=data.value("reason_key",std::string());
    if(!reason.empty())ImGui::TextWrapped("%s",uiText(reason.c_str()));
    const auto error=data.value("error_key",std::string());
    if(!error.empty())ImGui::TextWrapped("%s",uiText(error.c_str()));
    const auto status=data.value("status",std::string("idle"));
    if(status!="idle")ImGui::TextWrapped("%s",uiText(("quick.status."+status).c_str()));
}
