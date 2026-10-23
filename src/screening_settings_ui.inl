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
    ImGui::SetNextWindowSize({std::min(960.0f,display.x-40),std::min(820.0f,display.y-60)},ImGuiCond_Appearing);
    ImGui::SetNextWindowPos({display.x*.5f,display.y*.5f},ImGuiCond_Appearing,{.5f,.5f});
    if(!ImGui::BeginPopupModal(roomcats::Tx("筛选设置###ScreeningSettings"),nullptr,ImGuiWindowFlags_NoSavedSettings)) return;
    ApplyListFontScale();
    roomcats::ui::WindowPaper(false,false,roomcats::Tx("筛选设置"));
    g_rules_popup_drawn=true; g_mouse.SetPopupOpen(true); ClearTraitHover();
    ImGui::TextWrapped(roomcats::Tx("繁育比较遗传总属性，战备比较真实总属性；相同时先比优质突变数量，再看是否有职业。"));
    ImGui::TextWrapped(roomcats::Tx("设置保存后会重新生成受影响的方案；复查并点击“确定处理”后才会执行。"));
    if(ImGui::CollapsingHeader(roomcats::Tx("术语说明"))) {
        ImGui::TextWrapped("%s",roomcats::Tx("遗传总属性是七项遗传数值的总和；真实总属性是七项当前实际数值的总和。"));
        ImGui::TextWrapped("%s",roomcats::Tx("优质突变：只增加属性的好突变，或带功能性效果的好突变。纯数值突变若有减属性，就不计为优质。大块头算优质，侏儒症只算普通好突变。"));
        ImGui::TextWrapped("%s",roomcats::Tx("战备房是一楼右；其他四个房间用于繁育。待处理名单中的猫可以送至NPC、遗弃或保留，确认前不会执行。"));
    }
    if(ImGui::BeginTabBar("RuleGroups")) {
        auto fields=[](auto visit) {
            ImGui::BeginChild("RuleFields",{0,ImGui::GetContentRegionAvail().y-ImGui::GetFrameHeightWithSpacing()*3},ImGuiChildFlags_Borders);
            if(g_pad_frame.scroll_y!=0) ImGui::SetScrollY(ImGui::GetScrollY()+g_pad_frame.scroll_y*650*ImGui::GetIO().DeltaTime);
            visit([](const char* key,const char* label,auto& value,int lo,int hi) {
                ImGui::PushID(key);
                bool hovered=false;
                if constexpr(std::is_same_v<std::decay_t<decltype(value)>,bool>) roomcats::ui::Checkbox(label,&value);
                else {
                    const float input_width=205*ListFontScale();
                    const float input_x=ImGui::GetCursorPosX()+ImGui::GetContentRegionAvail().x-input_width;
                    ImGui::PushTextWrapPos(input_x-ImGui::GetStyle().ItemSpacing.x);
                    ImGui::TextUnformatted(label); hovered=ImGui::IsItemHovered();
                    ImGui::PopTextWrapPos(); ImGui::SameLine(input_x);
                    ImGui::SetNextItemWidth(input_width); roomcats::ui::InputInt("##Value",&value,1,5);
                }
                if(hovered || ImGui::IsItemHovered()) {
                    ImGui::BeginTooltip(); ImGui::PushTextWrapPos(600);
                    ImGui::TextUnformatted(roomcats::ScreeningRuleHelp(key));
                    if constexpr(!std::is_same_v<std::decay_t<decltype(value)>,bool>) ImGui::Text(roomcats::Tx("可填范围：%d 至 %d"),lo,hi);
                    ImGui::PopTextWrapPos(); ImGui::EndTooltip();
                }
                ImGui::PopID();
            });
            ImGui::EndChild();
        };
        if(ImGui::BeginTabItem(roomcats::Tx("新生猫"))) { fields([](auto f) { roomcats::VisitNewbornRules(g_rules_draft.newborn,f); }); ImGui::EndTabItem(); }
        if(ImGui::BeginTabItem(roomcats::Tx("大猫"))) { fields([](auto f) { roomcats::VisitAdultRules(g_rules_draft.adult,f); }); ImGui::EndTabItem(); }
        ImGui::EndTabBar();
    }
    const auto error=roomcats::ValidateScreeningRules(g_rules_draft);
    if(!error.empty()) ImGui::TextColored(roomcats::ui::Warning(),"%s",error.c_str());
    else {
        ImGui::PushStyleColor(ImGuiCol_Text,ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
        ImGui::TextWrapped("%s",roomcats::Tx("房子图标的猫始终保留。新生猫指年龄为1的猫；大猫指年龄大于1的猫。"));
        ImGui::PopStyleColor();
    }
    if(roomcats::ui::Button(roomcats::Tx("恢复默认"))) g_rules_draft={};
    ImGui::SameLine(); ImGui::BeginDisabled(!error.empty() || roomcats::NewbornBatchActive());
    if(roomcats::ui::Button(roomcats::Tx("保存并关闭")) && ApplyScreeningRules(g_rules_draft)) ImGui::CloseCurrentPopup();
    ImGui::EndDisabled(); ImGui::SameLine();
    if(roomcats::ui::Button(roomcats::Tx("取消"))) ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
}
