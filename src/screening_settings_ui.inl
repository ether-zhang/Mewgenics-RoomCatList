// Shared with headless ImGui tests. Persistence uses the existing ui-state.ini
// settings handler; the live rules only change on a valid explicit save.
void RegisterScreeningRulesSettings() {
    if(ImGui::FindSettingsHandler("RoomCatScreening")) return;
    ImGuiSettingsHandler handler;
    handler.TypeName="RoomCatScreening"; handler.TypeHash=ImHashStr(handler.TypeName);
    handler.ReadInitFn=[](ImGuiContext*,ImGuiSettingsHandler*) { g_rules_load_text.clear(); };
    handler.ReadOpenFn=[](ImGuiContext*,ImGuiSettingsHandler*,const char* name)->void* { return std::string(name)=="Rules" ? &g_rules_load_text : nullptr; };
    handler.ReadLineFn=[](ImGuiContext*,ImGuiSettingsHandler*,void* entry,const char* line) { *static_cast<std::string*>(entry)+=std::string(line)+"\n"; };
    handler.ApplyAllFn=[](ImGuiContext*,ImGuiSettingsHandler*) {
        roomcats::ScreeningRules loaded; std::string error;
        if(roomcats::ParseScreeningRules(g_rules_load_text,loaded,error)) g_screening_rules=loaded;
        else { g_screening_rules={}; g_move_feedback=roomcats::Tx("筛选设置无效，已恢复默认值：")+error; g_font_scan_pending=true; }
    };
    handler.WriteAllFn=[](ImGuiContext*,ImGuiSettingsHandler*,ImGuiTextBuffer* out) {
        out->append("[RoomCatScreening][Rules]\n");
        out->append(roomcats::SerializeScreeningRules(g_screening_rules).c_str()); out->append("\n");
    };
    ImGui::AddSettingsHandler(&handler);
}

bool ApplyScreeningRules(const roomcats::ScreeningRules& rules) {
    g_rules_error=roomcats::ValidateScreeningRules(rules);
    if(!g_rules_error.empty() || roomcats::NewbornBatchActive()) return false;
    const bool changed=roomcats::SerializeScreeningRules(rules)!=roomcats::SerializeScreeningRules(g_screening_rules);
    g_screening_rules=rules;
    ImGui::MarkIniSettingsDirty(); g_rules_save_requested=true; g_font_scan_pending=true;
    const bool review_changed=g_screening_plan && (g_screening_plan->kind==roomcats::ScreeningKind::Adult ?
        roomcats::RuleFingerprint(rules.adult)!=roomcats::RuleFingerprint(g_screening_plan->adult_rules) :
        roomcats::RuleFingerprint(rules.newborn)!=roomcats::RuleFingerprint(g_screening_plan->newborn_rules));
    if(changed && InScreeningReview() && review_changed) {
        const auto kind=g_screening_plan->kind;
        ResetListViews(); g_prepare_screening_kind=kind; g_prepare_screening=true;
    }
    g_move_feedback=changed ? roomcats::Tx("筛选设置已保存；下次生成方案使用新规则") : roomcats::Tx("筛选设置已保存");
    return true;
}

void OpenScreeningSettings() {
    if(roomcats::NewbornBatchActive()) return;
    ClearMovePopup(); ClearTraitHover();
    g_rules_draft=g_screening_rules; g_rules_error.clear();
    ImGui::OpenPopup(roomcats::Tx("筛选设置###ScreeningSettings"));
}
void DrawScreeningSettings() {
    const auto display=ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowSize({std::min(780.0f,display.x-40),std::min(740.0f,display.y-60)},ImGuiCond_Appearing);
    ImGui::SetNextWindowPos({display.x*.5f,display.y*.5f},ImGuiCond_Appearing,{.5f,.5f});
    if(!ImGui::BeginPopupModal(roomcats::Tx("筛选设置###ScreeningSettings"),nullptr,ImGuiWindowFlags_NoSavedSettings)) return;
    roomcats::ui::WindowPaper(false,false,roomcats::Tx("筛选设置"));
    g_rules_popup_drawn=true; g_mouse.SetPopupOpen(true); ClearTraitHover();
    ImGui::TextWrapped(roomcats::Tx("排序：属性 > 优质变异 > 有无职业。房子标记始终保护；小猫年龄=1，老猫年龄>1。"));
    ImGui::TextWrapped(roomcats::Tx("修改只影响新生成的方案；保存后仍须复查并确认。数值可直接输入，也可点击加减按钮。"));
    if(ImGui::BeginTabBar("RuleGroups")) {
        auto fields=[](auto visit) {
            ImGui::BeginChild("RuleFields",{0,ImGui::GetContentRegionAvail().y-ImGui::GetFrameHeightWithSpacing()*3},ImGuiChildFlags_Borders);
            if(g_pad_frame.scroll_y!=0) ImGui::SetScrollY(ImGui::GetScrollY()+g_pad_frame.scroll_y*650*ImGui::GetIO().DeltaTime);
            visit([](const char* key,const char* label,auto& value,int lo,int hi) {
                ImGui::PushID(key);
                if constexpr(std::is_same_v<std::decay_t<decltype(value)>,bool>) roomcats::ui::Checkbox(label,&value);
                else {
                    ImGui::TextUnformatted(label); ImGui::SameLine(std::max(ImGui::GetCursorPosX(),ImGui::GetContentRegionAvail().x-215));
                    ImGui::SetNextItemWidth(205); roomcats::ui::InputInt("##Value",&value,1,5);
                    if(ImGui::IsItemHovered()) ImGui::SetTooltip(roomcats::Tx("可填范围：%d 至 %d"),lo,hi);
                }
                ImGui::PopID();
            });
            ImGui::EndChild();
        };
        if(ImGui::BeginTabItem(roomcats::Tx("新生猫"))) { fields([](auto f) { roomcats::VisitNewbornRules(g_rules_draft.newborn,f); }); ImGui::EndTabItem(); }
        if(ImGui::BeginTabItem(roomcats::Tx("老猫"))) { fields([](auto f) { roomcats::VisitAdultRules(g_rules_draft.adult,f); }); ImGui::EndTabItem(); }
        ImGui::EndTabBar();
    }
    const auto error=roomcats::ValidateScreeningRules(g_rules_draft);
    if(!error.empty()) ImGui::TextColored(roomcats::ui::Warning(),"%s",error.c_str());
    else {
        ImGui::PushStyleColor(ImGuiCol_Text,ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
        ImGui::TextWrapped("%s",roomcats::Tx("二楼新生每房最多可设6只；对面出生数量需小于等于每房上限。"));
        ImGui::PopStyleColor();
    }
    if(roomcats::ui::Button(roomcats::Tx("恢复默认"))) g_rules_draft={};
    ImGui::SameLine(); ImGui::BeginDisabled(!error.empty() || roomcats::NewbornBatchActive());
    if(roomcats::ui::Button(roomcats::Tx("保存并关闭")) && ApplyScreeningRules(g_rules_draft)) ImGui::CloseCurrentPopup();
    ImGui::EndDisabled(); ImGui::SameLine();
    if(roomcats::ui::Button(roomcats::Tx("取消"))) ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
}
