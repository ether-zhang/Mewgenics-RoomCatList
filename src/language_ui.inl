// The language is a UI preference in the existing ImGui ini, independent of
// screening rules and game/save settings. A missing entry defaults to English.
void RegisterLanguageSettings() {
    if (ImGui::FindSettingsHandler("RoomCatLanguage")) return;
    ImGuiSettingsHandler handler;
    handler.TypeName="RoomCatLanguage";
    handler.TypeHash=ImHashStr(handler.TypeName);
    handler.ReadInitFn=[](ImGuiContext*,ImGuiSettingsHandler*) { roomcats::SetLanguage(roomcats::Language::English); };
    handler.ReadOpenFn=[](ImGuiContext*,ImGuiSettingsHandler*,const char* name)->void* {
        return std::strcmp(name,"Selection")==0 ? &roomcats::g_language : nullptr;
    };
    handler.ReadLineFn=[](ImGuiContext*,ImGuiSettingsHandler*,void*,const char* line) {
        if (std::strcmp(line,"code=zh-cn")==0) roomcats::SetLanguage(roomcats::Language::Chinese);
        else if (std::strcmp(line,"code=en")==0) roomcats::SetLanguage(roomcats::Language::English);
    };
    handler.WriteAllFn=[](ImGuiContext*,ImGuiSettingsHandler*,ImGuiTextBuffer* out) {
        out->append("[RoomCatLanguage][Selection]\ncode=");
        out->append(roomcats::Chinese() ? "zh-cn\n\n" : "en\n\n");
    };
    ImGui::AddSettingsHandler(&handler);
}

void RelocalizeSnapshotLabels(roomcats::RoomSnapshot& snapshot) {
    for (auto& location : snapshot.locations)
        location.label=roomcats::LocalizedLocationLabel(location.key,location.label);
    for (auto& cat : snapshot.cats) {
        const auto found=std::find_if(snapshot.locations.begin(),snapshot.locations.end(),
            [&](const auto& location) { return location.key==cat.location_key; });
        cat.location_label=found==snapshot.locations.end() ?
            roomcats::LocalizedLocationLabel(cat.location_key,cat.location_label) : found->label;
    }
}

void SwitchUiLanguage(roomcats::Language language) {
    if (roomcats::NewbornBatchActive() || !roomcats::SetLanguage(language)) return;
    // Review decisions and their manual destinations survive relocalization.
    // Regenerate only the pure plan text with the same seed and rules.
    if (g_screening_plan && !g_screening_submitted) {
        for (const auto& d : g_screening_plan->decisions)
            if (d.discard) g_screening_choices.Remember(*g_screening_plan,d);
        auto source=g_screening_plan->source;
        RelocalizeSnapshotLabels(source);
        auto plan=g_screening_plan->kind==roomcats::ScreeningKind::Adult ?
            roomcats::PlanAdultCats(source,g_screening_plan->seed,g_screening_plan->adult_rules) :
            roomcats::PlanNewbornCats(source,g_screening_plan->seed,g_screening_plan->newborn_rules);
        if (plan.Valid()) {
            g_screening_choices.Prepare(plan);
            g_screening_plan=std::move(plan);
        }
    }
    g_move_feedback.clear();
    g_view_refresh_requested=g_font_scan_pending=g_sort_dirty=true;
    ClearTraitHover();
    ImGui::MarkIniSettingsDirty();
    g_language_save_requested=true;
}

void DrawLanguageSelector() {
    ImGui::SetNextItemWidth(118);
    if (roomcats::ui::BeginCombo("##RoomCatLanguage",roomcats::Chinese() ? "简体中文" : "English")) {
        g_language_popup_drawn=true;
        g_mouse.SetPopupOpen(true);
        if (ImGui::Selectable("English",!roomcats::Chinese())) SwitchUiLanguage(roomcats::Language::English);
        if (ImGui::Selectable("简体中文",roomcats::Chinese())) SwitchUiLanguage(roomcats::Language::Chinese);
        ImGui::EndCombo();
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s",roomcats::Tx("语言"));
}
