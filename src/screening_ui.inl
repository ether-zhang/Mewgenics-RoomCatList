// Included inside the shared list UI namespace; preview choices only queue a
// confirmation request. No movement, marker write or disposal happens here.
int TableColumnIndex(int column) {
    if(!g_view.screening) return column;
    if(column==roomcats::Name) return 1;
    return column<roomcats::Name ? column+2 : column+1;
}
roomcats::NewbornDecision* ScreeningDecision(std::uint64_t id) {
    if(g_screening_plan) for(auto& d:g_screening_plan->decisions) if(d.cat.id==id) return &d;
    return nullptr;
}
roomcats::RoomSnapshot ScreeningSnapshot(const roomcats::RoomSnapshot& meta) {
    if(!g_screening_plan || !meta.in_house || !meta.valid || meta.scene!=g_screening_plan->source.scene || meta.generation!=g_screening_plan->source.generation) return meta;
    auto out=g_screening_plan->source; out.room=0; out.room_id.clear(); out.cats.clear();
    for(const auto& d:g_screening_plan->decisions) if(d.discard) out.cats.push_back(d.cat);
    return out;
}
void SetupScreeningColumns() {
    using namespace roomcats;
    auto setup=[](int col) {
        float width=kCatColumnWidths[col]*ListFontScale();
        if(col==SexOrientation && !roomcats::Chinese())
            width=std::max(width,ImGui::CalcTextSize("None/H/None").x+ImGui::GetStyle().CellPadding.x*2);
        ImGui::TableSetupColumn(roomcats::Tx(kCatColumnLabels[col]),ImGuiTableColumnFlags_WidthFixed,width,col);
    };
    if(!g_view.screening) { for(int col=0;col<CatColumnCount;++col) setup(col); return; }
    ImGui::TableSetupColumn(roomcats::Tx("处理方式"),ImGuiTableColumnFlags_WidthFixed,210*ListFontScale());
    setup(Name); setup(Position); setup(SendTo);
    for(int col=Parents;col<CatColumnCount;++col) setup(col);
    ImGui::TableSetupColumn(roomcats::Tx("筛选原因"),ImGuiTableColumnFlags_WidthFixed,420*ListFontScale());
}
std::string ScreeningNpcLabel(const roomcats::NpcChoice& choice) {
    // Put status first so it remains visible in a narrow destination column.
    return std::string(roomcats::Tx(choice.enabled ? "送至 " : choice.transient ? "等待：" : "不可用："))+choice.label;
}
std::string ScreeningChoiceLabel(const roomcats::NewbornDecision& d,const std::vector<roomcats::NpcChoice>* known_choices=nullptr) {
    using namespace roomcats;
    if(d.choice==ReviewChoice::Keep) return roomcats::Tx("不遗弃（原地保留）");
    if(d.choice==ReviewChoice::Location) {
        for(const auto& c:g_screening_plan->source.locations) if(c.key==d.alternative_location) return roomcats::Tx("移至 ")+c.label;
        return roomcats::Tx("移至 ")+(d.selection_label.empty() ? d.alternative_location : roomcats::LocalizedLocationLabel(d.alternative_location,d.selection_label));
    }
    if(d.choice==ReviewChoice::Npc) {
        const auto queried=known_choices ? std::vector<NpcChoice>{} : CatNpcChoices(g_screening_plan->source,d.cat.id);
        const auto& choices=known_choices ? *known_choices : queried;
        for(const auto& c:choices) if(c.npc==d.npc) return ScreeningNpcLabel(c);
        return roomcats::Tx("不可用：")+std::string(roomcats::NpcLabel(d.npc));
    }
    return roomcats::Tx("遗弃");
}
void DrawScreeningChoice(const roomcats::Cat& cat) {
    using namespace roomcats;
    auto* decision=ScreeningDecision(cat.id);
    if(!decision) return;
    std::vector<NpcChoice> npc_choices;
    if(decision->choice==ReviewChoice::Npc) {
        auto& preview=g_npc_previews[cat.id];
        const double now=ImGui::GetTime();
        if(preview.time<0 || now<preview.time || now-preview.time>=0.25) {
            preview.choices=CatNpcChoices(g_screening_plan->source,cat.id); preview.time=now;
        }
        npc_choices=preview.choices;
    }
    auto selected=std::find_if(npc_choices.begin(),npc_choices.end(),[&](const auto& c) { return c.npc==decision->npc; });
    bool unavailable=decision->choice==ReviewChoice::Npc && (selected==npc_choices.end() || !selected->enabled);
    ImGui::PushID(reinterpret_cast<void*>(static_cast<std::uintptr_t>(cat.id)));
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::BeginDisabled(g_screening_submitted || NewbornBatchActive());
    if(unavailable) ImGui::PushStyleColor(ImGuiCol_Text,ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    const bool expanded=roomcats::ui::BeginCombo("##ScreeningChoice",ScreeningChoiceLabel(*decision,&npc_choices).c_str(),ImGuiComboFlags_HeightLarge);
    if(unavailable) ImGui::PopStyleColor();
    const bool hovered=ImGui::IsItemHovered();
    if(expanded) {
        // Menus and the action queue revalidate live; only the closed preview
        // is cached, so changing NPC capacity cannot authorize a stale send.
        npc_choices=CatNpcChoices(g_screening_plan->source,cat.id);
        selected=std::find_if(npc_choices.begin(),npc_choices.end(),[&](const auto& c) { return c.npc==decision->npc; });
        unavailable=decision->choice==ReviewChoice::Npc && (selected==npc_choices.end() || !selected->enabled);
        g_move_popup_cat=cat.id; g_move_popup_drawn=true; g_mouse.SetPopupOpen(true); ClearTraitHover();
        bool changed=false;
        if(ImGui::Selectable(roomcats::Tx("遗弃"),decision->choice==ReviewChoice::Discard)) { decision->choice=ReviewChoice::Discard; changed=true; }
        if(ImGui::Selectable(roomcats::Tx("不遗弃（原地保留）"),decision->choice==ReviewChoice::Keep)) { decision->choice=ReviewChoice::Keep; changed=true; }
        ImGui::Separator();
        if(unavailable && selected==npc_choices.end()) {
            ImGui::BeginDisabled();
            ImGui::Selectable(ScreeningChoiceLabel(*decision,&npc_choices).c_str(),true);
            ImGui::EndDisabled();
        }
        for(const auto& choice:npc_choices) {
            if(choice.npc==8) continue;
            ImGui::PushID(choice.npc); ImGui::BeginDisabled(!choice.enabled);
            if(ImGui::Selectable(ScreeningNpcLabel(choice).c_str(),decision->choice==ReviewChoice::Npc && decision->npc==choice.npc)) {
                decision->choice=ReviewChoice::Npc; decision->npc=choice.npc;
                decision->selection_label=choice.label; changed=true;
            }
            if(!choice.reason.empty() && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("%s",choice.reason.c_str());
            ImGui::EndDisabled(); ImGui::PopID();
        }
        ImGui::Separator();
        for(const auto& choice:CatMoveChoices(g_screening_plan->source,cat.id)) {
            ImGui::PushID(choice.destination.key.c_str()); ImGui::BeginDisabled(!choice.enabled);
            if(ImGui::Selectable((roomcats::Tx("移至 ")+choice.destination.label).c_str(),decision->choice==ReviewChoice::Location && decision->alternative_location==choice.destination.key)) {
                decision->choice=ReviewChoice::Location; decision->alternative_location=choice.destination.key;
                decision->selection_label=choice.destination.label; changed=true;
            }
            if(!choice.reason.empty() && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("%s",choice.reason.c_str());
            ImGui::EndDisabled(); ImGui::PopID();
        }
        if(changed) g_screening_choices.Remember(*g_screening_plan,*decision);
        ImGui::EndCombo();
    } else if(hovered && !g_screening_submitted && unavailable) {
        ImGui::SetTooltip(roomcats::Tx("已保留原选择：%s。请等待或手动修改去向。"),selected==npc_choices.end() ? roomcats::Tx("此NPC当前不可用") : selected->reason.c_str());
    }
    ImGui::EndDisabled(); ImGui::PopID();
}
void DrawScreeningToolbar() {
    const auto progress=roomcats::GetNewbornBatchStatus();
    const bool recheck=g_view.screening && g_screening_submitted && progress.stopped;
    const char* label=progress.running ? roomcats::Tx("停止处理") : g_view.screening && !g_screening_submitted ? roomcats::Tx("确定处理") : recheck ? roomcats::Tx("重新复查") : roomcats::Tx("新生猫筛选");
    const bool choosing=!progress.running && !recheck && (!g_view.screening || g_screening_submitted);
    const char* adult=roomcats::Tx("大猫数量控制");
    const float settings_width=ImGui::CalcTextSize(roomcats::Tx("筛选设置")).x+ImGui::GetStyle().FramePadding.x*2+ImGui::GetStyle().ItemSpacing.x;
    const float group_width=ImGui::CalcTextSize(CatGroupButtonLabel().c_str()).x+ImGui::GetStyle().FramePadding.x*2;
    const float language_width=LanguageSelectorWidth();
    const float width=group_width+ImGui::GetStyle().ItemSpacing.x+language_width+ImGui::GetStyle().ItemSpacing.x+settings_width+ImGui::CalcTextSize(label).x+ImGui::GetStyle().FramePadding.x*2 +
        (choosing ? ImGui::CalcTextSize(adult).x+ImGui::GetStyle().FramePadding.x*2+ImGui::GetStyle().ItemSpacing.x : 0);
    const float right=ImGui::GetWindowContentRegionMax().x;
    const float after_label=ImGui::GetItemRectMax().x-ImGui::GetWindowPos().x+ImGui::GetStyle().ItemSpacing.x;
    const auto same_line_if_fits=[&](const char* next) {
        const float end=ImGui::GetItemRectMax().x-ImGui::GetWindowPos().x+ImGui::GetStyle().ItemSpacing.x;
        if(end+ImGui::CalcTextSize(next).x+ImGui::GetStyle().FramePadding.x*2<=right) ImGui::SameLine();
    };
    if(right-width>=after_label) ImGui::SameLine(right-width);
    else ImGui::NewLine();
    DrawCatGroupButton();
    const float after_group=ImGui::GetItemRectMax().x-ImGui::GetWindowPos().x+ImGui::GetStyle().ItemSpacing.x;
    if(after_group+language_width<=right) ImGui::SameLine();
    ImGui::BeginDisabled(progress.running);
    DrawLanguageSelector();
    ImGui::EndDisabled(); same_line_if_fits(roomcats::Tx("筛选设置"));
    ImGui::BeginDisabled(progress.running);
    if(roomcats::ui::Button(roomcats::Tx("筛选设置"))) OpenScreeningSettings();
    ImGui::EndDisabled(); same_line_if_fits(choosing ? adult : label);
    if(choosing) {
        if(roomcats::ui::Button(adult)) { g_prepare_screening_kind=roomcats::ScreeningKind::Adult; g_prepare_screening=true; }
        same_line_if_fits(label);
    }
    if(roomcats::ui::Button(label)) {
        if(progress.running) roomcats::StopNewbornBatch();
        else if(g_view.screening && !g_screening_submitted) g_confirm_screening=true;
        else if(recheck && g_screening_plan) { g_prepare_screening_kind=g_screening_plan->kind; g_prepare_screening=true; }
        else { g_prepare_screening_kind=roomcats::ScreeningKind::Newborn; g_prepare_screening=true; }
    }
}
void DrawEmptyScreeningReview() {
    if(g_screening_submitted) {
        ImGui::TextDisabled(roomcats::Tx("无可遗弃。本轮处理进度见下方。"));
        return;
    }
    int moves=0,marks=0;
    if(g_screening_plan) for(const auto& d:g_screening_plan->decisions) {
        moves+=!d.discard && d.destination!=d.cat.location_key;
        marks+=!d.marker.empty() && d.marker!=d.cat.details.marker;
    }
    if(moves>0) {
        ImGui::TextColored(roomcats::ui::Warning(),roomcats::Tx("无可遗弃，存在调房：%d 只猫咪。"),moves);
        ImGui::TextWrapped(roomcats::Tx("请点击右上角“确定处理”后执行调房。悬停下方“调房”数量可查看名单。"));
    } else if(marks>0) {
        ImGui::TextWrapped(roomcats::Tx("无可遗弃，无需调房；有 %d 只猫咪需要打标，点击“确定处理”后执行。"),marks);
    } else ImGui::TextDisabled(roomcats::Tx("无可遗弃，也无需调房或打标。"));
}
void DrawScreeningSummary() {
    if(!g_screening_plan) return;
    int marks=0,moves=0,discard=0,npcs=0,keep=0;
    for(const auto& d:g_screening_plan->decisions) {
        marks+=!d.marker.empty() && d.marker!=d.cat.details.marker;
        if(!d.discard) moves+=d.destination!=d.cat.location_key;
        else if(d.choice==roomcats::ReviewChoice::Discard) ++discard;
        else if(d.choice==roomcats::ReviewChoice::Npc) ++npcs;
        else if(d.choice==roomcats::ReviewChoice::Location) moves+=d.alternative_location!=d.cat.location_key;
        else ++keep;
    }
    ImGui::Text(roomcats::Tx("打标 %d · 调房 %d · NPC %d · 遗弃 %d · 原地保留 %d"),marks,moves,npcs,discard,keep);
    if(ImGui::IsItemHovered()) {
        ImGui::BeginTooltip(); ImGui::PushTextWrapPos(640);
        for(const auto& d:g_screening_plan->decisions)
            if(d.destination!=d.cat.location_key && !d.discard)
                ImGui::Text(roomcats::Tx("%s：%s → %s（遗传 %lld，优质突变 %d）"),d.cat.name.c_str(),d.cat.location_label.c_str(),
                    roomcats::RoomLabel(d.destination.substr(5)).c_str(),roomcats::TotalStats(d.cat.details.genetic),roomcats::QualityMutations(d.cat.details));
        for(const auto& note:g_screening_plan->notes) ImGui::TextWrapped("%s",note.c_str());
        ImGui::PopTextWrapPos(); ImGui::EndTooltip();
    }
    if(g_screening_plan->kind==roomcats::ScreeningKind::Adult) {
        const auto& rules=g_screening_plan->adult_rules;
        const auto projected=roomcats::ProjectPopulation(*g_screening_plan);
        const auto count=std::count_if(g_screening_plan->source.cats.begin(),g_screening_plan->source.cats.end(),[](const auto& c) { return c.active && !c.record_only; });
        ImGui::Text(roomcats::Tx("预计总数 %d → %d / %d · 房子保护 %d · 舒适度：顶楼 %.0f / 二楼左 %.0f / 二楼右 %.0f / 一楼左 %.0f"),
            int(count),projected.total,rules.population_target,projected.protected_cats,std::round(projected.comfort[0]),std::round(projected.comfort[1]),std::round(projected.comfort[2]),std::round(projected.comfort[3]));
        bool below=false;
        for(int r=0;r<4;++r) below|=projected.comfort[r]+1e-6<rules.comfort_min[r];
        if(projected.total>rules.population_target || below)
            ImGui::TextColored(roomcats::ui::Warning(),roomcats::Tx("保留限制或手动选择使目标未达成。目标舒适度：%d / %d / %d / %d。"),rules.comfort_min[0],rules.comfort_min[1],rules.comfort_min[2],rules.comfort_min[3]);
        else ImGui::TextDisabled(roomcats::Tx("年龄>1 · 繁育遗传总属性（顶楼/二楼/一楼左）：%d/%d/%d · 战备真实总属性至少 %d"),rules.breeding.genetic_min[0],rules.breeding.genetic_min[1],rules.breeding.genetic_min[2],rules.battle.real_min);
    } else {
        const auto& rules=g_screening_plan->newborn_rules;
        ImGui::TextDisabled(roomcats::Tx("遗传总属性（顶楼/二楼/一楼左）：%d/%d/%d · 每房新生上限（顶/二楼）：%d/%d · 战备总属性至少 %d"),
            rules.breeding.genetic_min[0],rules.breeding.genetic_min[1],rules.breeding.genetic_min[2],rules.attic_limit,rules.second_limit,rules.battle.real_min);
    }
    const auto progress=roomcats::GetNewbornBatchStatus();
    if(g_screening_submitted) ImGui::Text(roomcats::Tx("%zu / %zu 步：%s"),progress.completed,progress.total,progress.message.c_str());
    else ImGui::TextDisabled(roomcats::Tx("确认前不修改猫咪。悬停上方数量可查看调房摘要。"));
}
