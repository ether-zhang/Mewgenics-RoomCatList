// Pure list layout and interactions. No native hooks, device access, or file I/O.
namespace {
void DrawCompactValue(const std::string& value);
void ApplyRoomSnapshot(roomcats::RoomSnapshot next, bool force, double read_ms) {
    g_screening_choices.Observe(next);
    const auto old_scene = g_snapshot.scene;
    const auto old_room = g_snapshot.room;
    const auto old_generation = g_snapshot.generation;
    const bool view_changed = g_view_refresh_requested;
    // NPC offices suspend the existing House scene while their native UI is
    // active. Keep the list's intent, scope, and scroll position for return.
    if (next.suspended && next.scene == old_scene && next.generation == g_snapshot.generation) {
        g_has_house = false;
        return;
    }
    const bool same_contents = next.cats.size() == g_snapshot.cats.size() &&
        std::equal(next.cats.begin(), next.cats.end(), g_snapshot.cats.begin(),
            [](const roomcats::Cat& a, const roomcats::Cat& b) {
                return a.id == b.id && a.name == b.name && a.details.revision == b.details.revision &&
                    a.location_key == b.location_key && a.location_label == b.location_label && a.record_only == b.record_only && a.parents.valid == b.parents.valid &&
                    a.parents.ids == b.parents.ids && a.parents.names == b.parents.names;
            });
    g_sort_dirty |= !same_contents;
    g_font_scan_pending |= !same_contents || next.room_id != g_snapshot.room_id || next.error != g_snapshot.error;
    g_snapshot = std::move(next);
    g_has_house = g_snapshot.in_house;
    if (!g_has_house || (old_scene && (old_scene != g_snapshot.scene || old_generation != g_snapshot.generation))) {
        g_open = false;
        ResetListViews();
    }
    if (!view_changed && !g_view.IsParents() && (force || old_room != g_snapshot.room || old_scene != g_snapshot.scene)) g_reset_list_scroll = true;
    g_view_refresh_requested = false;
    if (g_open && (force || old_room != g_snapshot.room)) {
        Log("List snapshot: room=" + (g_snapshot.room ? g_snapshot.room_id : "ALL") + "; cats=" + std::to_string(g_snapshot.cats.size()) +
            "; detail_cats=" + std::to_string(std::count_if(g_snapshot.cats.begin(), g_snapshot.cats.end(),
                [](const roomcats::Cat& c) { return c.details.valid; })) +
            "; valid=" + (g_snapshot.valid ? "yes" : "no") + "; read_ms=" + std::to_string(read_ms));
    }
}

const std::vector<roomcats::Trait>& Traits(const roomcats::Cat& cat, int group) {
    return group == 0 ? cat.details.good : group == 1 ? cat.details.bad : cat.details.diseases;
}

void ClearTraitHover() {
    g_trait_hover = {};
    g_mouse.SetTooltip({});
}

void ClearMovePopup() {
    if ((g_move_popup_cat || g_rules_popup_drawn || g_language_popup_drawn) && ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel))
        ImGui::ClosePopupToLevel(0, true);
    g_move_popup_cat = 0;
    g_move_popup_drawn = g_close_move_popup = false;
    g_rules_popup_drawn=false;
    g_language_popup_drawn=false;
    g_mouse.SetPopupOpen(false);
}

bool RequestListBack() {
    if (g_view_history.empty()) return false;
    g_pending_view_back = true;
    return true;
}

void ResetListViews() {
    if (!g_view_history.empty()) g_sort = g_view_history.front().sort;
    g_view = {}; g_view_history.clear(); g_pending_parent_view.reset();
    g_pending_view_back = g_view_refresh_requested = g_restore_view_scroll = false;
    g_sort_dirty = true;
    g_screening_plan.reset(); g_prepare_screening = g_confirm_screening = g_screening_submitted = false;
}

void ApplyQueuedListView() {
    if (!g_pending_parent_view && !g_pending_view_back) return;
    if (g_pending_view_back && !g_view_history.empty()) {
        g_view = std::move(g_view_history.back()); g_view_history.pop_back();
    } else if (g_pending_parent_view) {
        g_view.sort = g_sort; g_view.scroll = g_list_scroll;
        g_view_history.push_back(std::move(g_view));
        g_view = std::move(*g_pending_parent_view);
    }
    g_pending_parent_view.reset(); g_pending_view_back = false;
    g_sort = g_view.sort; g_list_scroll = g_view.scroll;
    g_sort_dirty = g_view_refresh_requested = g_restore_view_scroll = g_font_scan_pending = true;
    g_reset_list_scroll = false;
    ClearMovePopup(); ClearTraitHover();
    if(!InScreeningReview()) g_screening_plan.reset();
}

void DrawMoveSelector(const roomcats::Cat& cat) {
    if(InScreeningReview() || roomcats::NewbornBatchActive()) { DrawCompactValue(cat.location_label); return; }
    ImGui::PushID(reinterpret_cast<void*>(static_cast<std::uintptr_t>(cat.id)));
    ImGui::SetNextItemWidth(-FLT_MIN);
    const bool pending = roomcats::HasPendingCatMove() || roomcats::HasPendingCatAction();
    ImGui::BeginDisabled(pending || cat.record_only);
    const bool opened = roomcats::ui::BeginCombo("##MoveCat", pending ? roomcats::Tx("移动中…") : cat.location_label.empty() ? roomcats::Tx("位置") : cat.location_label.c_str(), ImGuiComboFlags_HeightLarge);
    if (opened) {
        g_move_popup_cat = cat.id;
        g_move_popup_drawn = true;
        g_mouse.SetPopupOpen(true);
        ClearTraitHover();
        for (const auto& choice : roomcats::CatMoveChoices(g_snapshot, cat.id)) {
            ImGui::BeginDisabled(!choice.enabled);
            if (ImGui::Selectable(choice.destination.label.c_str(), choice.current)) {
                if (roomcats::QueueCatMove(g_snapshot, cat.id, choice.destination.key)) g_move_feedback = roomcats::Tx("正在移送…");
                ImGui::CloseCurrentPopup();
            }
            if (!choice.reason.empty() && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                ImGui::SetTooltip("%s", choice.reason.c_str());
            ImGui::EndDisabled();
        }
        ImGui::EndCombo();
    }
    ImGui::EndDisabled();
    if (cat.record_only && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip(roomcats::Tx("此猫不在家园，无法移动或投送。"));
    ImGui::PopID();
}

void DrawNpcSelector(const roomcats::Cat& cat) {
    if(InScreeningReview() || roomcats::NewbornBatchActive()) { ImGui::TextDisabled("—"); return; }
    ImGui::PushID(reinterpret_cast<void*>(static_cast<std::uintptr_t>(cat.id)));
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::BeginDisabled(cat.record_only || roomcats::NewbornBatchActive() || roomcats::HasPendingCatMove() || roomcats::HasPendingCatAction());
    if (roomcats::ui::BeginCombo("##SendCat", roomcats::Tx("选择去向"), ImGuiComboFlags_HeightLarge)) {
        g_move_popup_cat = cat.id;
        g_move_popup_drawn = true;
        g_mouse.SetPopupOpen(true);
        ClearTraitHover();
        const auto choices = roomcats::CatNpcChoices(g_snapshot, cat.id);
        if (choices.empty()) ImGui::TextDisabled(roomcats::Tx("暂无已解锁角色"));
        for (const auto& choice : choices) {
            ImGui::PushID(choice.npc);
            ImGui::BeginDisabled(!choice.enabled);
            if (ImGui::Selectable(choice.label.c_str())) {
                if (roomcats::QueueCatAction(g_snapshot, cat.id, roomcats::CatActionKind::Donate, choice.npc))
                    g_move_feedback = roomcats::Tx("正在移送…");
                ImGui::CloseCurrentPopup();
            }
            if (!choice.reason.empty() && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                ImGui::SetTooltip("%s", choice.reason.c_str());
            ImGui::EndDisabled();
            ImGui::PopID();
        }
        ImGui::EndCombo();
    }
    ImGui::EndDisabled();
    ImGui::PopID();
}

void DrawCatName(const roomcats::Cat& cat) {
    ImGui::PushID(reinterpret_cast<void*>(static_cast<std::uintptr_t>(cat.id)));
    const auto position = ImGui::GetCursorScreenPos();
    const float width = ImGui::GetContentRegionAvail().x;
    const auto name = roomcats::MarkerGlyph(cat.details.marker) + cat.name;
    ImGui::BeginDisabled(cat.record_only || roomcats::NewbornBatchActive() || (InScreeningReview() && g_screening_submitted) || roomcats::HasPendingCatMove() || roomcats::HasPendingCatAction());
    if (ImGui::Selectable("##InspectCat", false, 0, {width, ImGui::GetTextLineHeight()*2 + ImGui::GetStyle().ItemSpacing.y}))
        roomcats::QueueCatAction(g_snapshot, cat.id, roomcats::CatActionKind::Inspect);
    const float icon_width = cat.details.valid ? 28.0f : 0.0f;
    ImGui::RenderTextClipped(position, {position.x+width-icon_width, position.y+ImGui::GetTextLineHeight()+3},
        name.data(), name.data()+name.size(), nullptr);
    if (cat.details.valid) {
        const auto icon = roomcats::ClassGlyph(cat.details.collar);
        ImGui::RenderTextClipped({position.x+width-icon_width,position.y},
            {position.x+width,position.y+28},icon.c_str(),nullptr,nullptr);
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip(roomcats::Tx("%s\n职业：%s\n%s"), cat.name.c_str(),
        cat.details.valid ? roomcats::ClassLabel(cat.details.collar).c_str() : roomcats::Tx("未知"),
        cat.record_only ? roomcats::Tx("此猫不在家园，无法打开家园详情。") : roomcats::Tx("点击打开猫咪界面"));
    ImGui::EndDisabled();
    ImGui::PopID();
}

void DrawParents(const roomcats::Cat& cat) {
    const auto& parents = cat.parents;
    if (!parents.valid) { ImGui::TextDisabled("—"); return; }
    if (!parents.ids[0] && !parents.ids[1]) { ImGui::TextDisabled(roomcats::Tx("未知")); return; }
    const float width = ImGui::GetContentRegionAvail().x;
    const auto position = ImGui::GetCursorScreenPos();
    ImGui::PushID(reinterpret_cast<void*>(static_cast<std::uintptr_t>(cat.id)));
    ImGui::BeginDisabled(g_view_history.size() >= 32);
    if (ImGui::Selectable("##Parents",false,0,{width,ImGui::GetTextLineHeight()*2+ImGui::GetStyle().ItemSpacing.y})) {
        ListView next; next.ids = parents.ids; next.names = parents.names; next.child_name = cat.name;
        g_pending_parent_view = std::move(next);
    }
    for (int i=0;i<2;++i) {
        const auto& name = parents.names[i];
        const ImVec2 line{position.x,position.y+i*ImGui::GetTextLineHeightWithSpacing()};
        ImGui::RenderTextClipped(line,{line.x+width,line.y+ImGui::GetTextLineHeight()},name.empty() ? roomcats::Tx("未知") : name.c_str(),nullptr,nullptr);
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip(roomcats::Tx("%s\n%s\n点击查看父母表格"),parents.names[0].c_str(),parents.names[1].c_str());
    ImGui::EndDisabled(); ImGui::PopID();
}

void DrawCompactValue(const std::string& value) {
    const float width = ImGui::GetContentRegionAvail().x;
    ImGui::TextUnformatted(value.c_str());
    if (g_mouse.CapturesPointer() && ImGui::IsItemHovered() && ImGui::CalcTextSize(value.c_str()).x > width)
        ImGui::SetTooltip("%s", value.c_str());
}

ImVec4 StatValueColor(double value, double baseline) {
    const float t = static_cast<float>(std::clamp(std::abs(value-baseline)/5.0,0.0,1.0));
    if(roomcats::ui::Active()) {
        const auto from=roomcats::ui::Ink();
        const ImVec4 to=value>=baseline ? ImVec4{.09f,.42f,.10f,1} : ImVec4{.67f,.08f,.07f,1};
        return {from.x+(to.x-from.x)*t,from.y+(to.y-from.y)*t,from.z+(to.z-from.z)*t,1};
    }
    return value >= baseline ? ImVec4{1-0.75f*t,1,1-0.65f*t,1} : ImVec4{1,1-0.7f*t,1-0.7f*t,1};
}

void DrawStatValues(const roomcats::CatDetails& details, int stat) {
    if (!details.valid) { ImGui::TextDisabled("—"); return; }
    const auto real = stat < 0 ? roomcats::TotalStats(details.real) : details.real[stat];
    const auto genetic = stat < 0 ? roomcats::TotalStats(details.genetic) : details.genetic[stat];
    const double count = stat < 0 ? 7.0 : 1.0;
    const std::string parts[] = {std::to_string(real),"(",std::to_string(genetic),")"};
    const ImVec4 text=ImGui::GetStyleColorVec4(ImGuiCol_Text);
    const ImVec4 colors[] = {StatValueColor(real/count,7),text,StatValueColor(genetic/count,5),text};
    const auto pos = ImGui::GetCursorScreenPos();
    const float width = ImGui::GetContentRegionAvail().x;
    ImGui::Dummy({width,ImGui::GetTextLineHeight()});
    ImGui::PushClipRect(pos,{pos.x+width,pos.y+ImGui::GetTextLineHeight()},true);
    float x = pos.x;
    for (int i=0;i<4;++i) {
        ImGui::GetWindowDrawList()->AddText({x,pos.y},ImGui::GetColorU32(colors[i]),parts[i].c_str());
        x += ImGui::CalcTextSize(parts[i].c_str()).x;
    }
    ImGui::PopClipRect();
    if (g_mouse.CapturesPointer() && ImGui::IsItemHovered())
        ImGui::SetTooltip(roomcats::Tx("%s\n基线：真实 7 / 遗传 5%s"),(parts[0]+parts[1]+parts[2]+parts[3]).c_str(),stat < 0 ? roomcats::Tx("（总属性按七项平均）") : "");
}

void TrackTraitHover(const roomcats::Cat& cat, int group, ImVec2 cell_min, ImVec2 cell_max) {
    // The whole visible cell is the target, including space around a one-digit
    // mutation count. Respect table clipping and any window above the table.
    if (!g_mouse.CapturesPointer() || !ImGui::IsWindowHovered() ||
        !ImGui::IsMouseHoveringRect(cell_min, cell_max)) return;
    if (g_trait_hover.cat != cat.id || g_trait_hover.group != group) g_trait_hover.reset_scroll = true;
    g_trait_hover.cat = cat.id;
    g_trait_hover.group = group;
    g_trait_hover.source_min = cell_min;
    g_trait_hover.source_max = cell_max;
    g_trait_hover.last_hover = ImGui::GetTime();
}

void DrawTraitDescriptions(const std::vector<roomcats::Trait>& traits) {
    ImGui::PushTextWrapPos(0);
    if (traits.empty()) ImGui::TextDisabled(roomcats::Tx("无"));
    for (std::size_t i = 0; i < traits.size(); ++i) {
        if (i) { ImGui::Spacing(); ImGui::Separator(); }
        ImGui::TextColored(roomcats::ui::TraitTitle(), "%s", roomcats::TraitName(traits[i]).c_str());
        ImGui::TextUnformatted(roomcats::TraitEffect(traits[i]).c_str());
    }
    ImGui::PopTextWrapPos();
}

void DrawMutationImpact(const roomcats::CatDetails& details) {
    ImGui::TextUnformatted(roomcats::Tx("全部突变"));
    ImGui::TextDisabled(roomcats::Tx("固定属性合计"));
    ImGui::Separator();
    const auto impact = roomcats::MutationStatImpact(details);
    constexpr const char* names[] = {"总计", "力量", "敏捷", "体质", "智力", "速度", "魅力", "幸运"};
    for (int i = 0; i < 8; ++i) {
        const auto value = i ? impact[i - 1] : roomcats::TotalStats(impact);
        const ImVec4 color = roomcats::ui::Active() ? StatValueColor(static_cast<double>(value),0) : value > 0 ? ImVec4{0.50f, 0.85f, 0.52f, 1} :
            value < 0 ? ImVec4{0.95f, 0.48f, 0.44f, 1} : ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);
        ImGui::TextColored(color, "%s %s", roomcats::Tx(names[i]), roomcats::FormatSignedStat(value).c_str());
    }
    ImGui::Spacing();
    ImGui::TextDisabled(roomcats::Tx("条件效果见左侧"));
}

void DrawTraitHover() {
    if (!g_trait_hover.cat || !g_open) { ClearTraitHover(); return; }
    const auto mouse = ImGui::GetIO().MousePos;
    const bool over_tip = ImGui::IsMousePosValid() && g_trait_hover.bounds.Contains(static_cast<int>(mouse.x), static_cast<int>(mouse.y));
    if (over_tip || (g_trait_hover.bounds.right && g_mouse.ListButtons())) g_trait_hover.last_hover = ImGui::GetTime();
    if (ImGui::GetTime() - g_trait_hover.last_hover > 0.18) { ClearTraitHover(); return; }
    const auto found = std::find_if(g_snapshot.cats.begin(), g_snapshot.cats.end(),
        [](const roomcats::Cat& c) { return c.id == g_trait_hover.cat; });
    if (found == g_snapshot.cats.end() || !found->details.valid) { ClearTraitHover(); return; }
    const auto display = ImGui::GetIO().DisplaySize;
    const bool mutation = g_trait_hover.group < 2;
    const float summary_width = 180.0f;
    const float width = std::min(mutation ? 760.0f : 540.0f, display.x - 32.0f);
    const auto& traits = Traits(*found, g_trait_hover.group);
    const auto& style = ImGui::GetStyle();
    const float wrap = width - style.WindowPadding.x * 2 - style.ScrollbarSize -
        (mutation ? summary_width + style.ItemSpacing.x : 0);
    float content_height = style.WindowPadding.y * 2 + ImGui::GetTextLineHeight() + style.ItemSpacing.y * 2 + 4;
    for (const auto& trait : traits)
        content_height += ImGui::CalcTextSize(roomcats::TraitName(trait).c_str(), nullptr, false, wrap).y +
            ImGui::CalcTextSize(roomcats::TraitEffect(trait).c_str(), nullptr, false, wrap).y + style.ItemSpacing.y * 4 + 2;
    if (traits.empty()) content_height += ImGui::GetTextLineHeightWithSpacing();
    if (mutation) content_height = std::max(content_height,
        style.WindowPadding.y * 2 + ImGui::GetTextLineHeightWithSpacing() * 15);
    // Place above/below the source row. A sideways popup could cover all the
    // other trait cells, making it impossible to move between their targets.
    const float below = std::max(1.0f, display.y - g_trait_hover.source_max.y - 20);
    const float above = std::max(1.0f, g_trait_hover.source_min.y - 20);
    const bool use_below = below >= std::min(content_height, 680.0f) || below >= above;
    const float height = std::min(std::min(content_height, 680.0f), use_below ? below : above);
    const float x = std::max(8.0f, std::min(g_trait_hover.source_min.x, display.x - width - 12));
    const float y = use_below ? g_trait_hover.source_max.y + 8 : g_trait_hover.source_min.y - height - 8;
    ImGui::SetNextWindowPos({x, y});
    ImGui::SetNextWindowSize({width, height});
    ImGui::SetNextWindowBgAlpha(1.0f);
    // A scrollable hover panel keeps long mutation descriptions readable.
    // Its exact bounds participate in the same mouse routing as the main list.
    if (ImGui::Begin("##RoomCatsTraitHover", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_Tooltip | ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav |
            (mutation ? ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse : 0))) {
        // Tooltip draw layers alone do not reorder ImGui's mouse hit testing.
        roomcats::ui::WindowPaper(false,true);
        // Keep this interactive tooltip above the main window for both paths,
        // without taking keyboard focus away from the list.
        ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());
        const bool reset_scroll = g_trait_hover.reset_scroll;
        if (reset_scroll) { ImGui::SetScrollY(0); g_trait_hover.reset_scroll = false; }
        if (!mutation && g_pad_frame.scroll_y != 0) ImGui::SetScrollY(ImGui::GetScrollY()+g_pad_frame.scroll_y*650*ImGui::GetIO().DeltaTime);
        ImGui::TextUnformatted(found->name.c_str());
        ImGui::SameLine();
        ImGui::TextDisabled("· %s (%zu)", g_trait_hover.group == 0 ? roomcats::Tx("好突变") : g_trait_hover.group == 1 ? roomcats::Tx("坏突变") : roomcats::Tx("疾病"), traits.size());
        ImGui::Separator();
        if (mutation) {
            const auto available = ImGui::GetContentRegionAvail();
            if (ImGui::BeginChild("##MutationDescriptions", {available.x - summary_width - style.ItemSpacing.x, available.y})) {
                if (reset_scroll) ImGui::SetScrollY(0);
                if (g_pad_frame.scroll_y != 0) ImGui::SetScrollY(ImGui::GetScrollY()+g_pad_frame.scroll_y*650*ImGui::GetIO().DeltaTime);
                DrawTraitDescriptions(traits);
            }
            ImGui::EndChild();
            ImGui::SameLine();
            if (ImGui::BeginChild("##MutationImpact", {0, available.y})) DrawMutationImpact(found->details);
            ImGui::EndChild();
        } else {
            DrawTraitDescriptions(traits);
        }
        const auto pos = ImGui::GetWindowPos(), size = ImGui::GetWindowSize();
        g_trait_hover.bounds = {static_cast<int>(pos.x), static_cast<int>(pos.y),
            static_cast<int>(pos.x + size.x), static_cast<int>(pos.y + size.y)};
        g_mouse.SetTooltip(g_trait_hover.bounds);
    }
    ImGui::End();
}

#include "language_ui.inl"
#include "screening_settings_ui.inl"
#include "screening_ui.inl"

void DrawColumnHeaders() {
    using namespace roomcats;
    ImGui::TableNextRow(ImGuiTableRowFlags_Headers);
    if(g_view.screening) {
        if(ImGui::TableSetColumnIndex(0)) ImGui::TextUnformatted(roomcats::Tx("处理方式"));
        if(ImGui::TableSetColumnIndex(CatColumnCount+1)) ImGui::TextUnformatted(roomcats::Tx("筛选原因"));
    }
    for (int col = 0; col < CatColumnCount; ++col) {
        if (!ImGui::TableSetColumnIndex(TableColumnIndex(col))) continue;
        if (!SortableCatColumn(col)) { ImGui::TextUnformatted(roomcats::Tx(kCatColumnLabels[col])); continue; }
        ImGui::PushID(col);
        const auto position = ImGui::GetCursorScreenPos();
        const float width = ImGui::GetContentRegionAvail().x;
        const float line_height = ImGui::GetTextLineHeight();
        if (ImGui::Selectable("##SortColumn", g_sort.column == col, 0, {width, line_height * 2 + 4})) {
            g_sort.Click(col);
            g_sort_dirty = true;
            g_reset_list_scroll = true;
            ClearTraitHover();
        }
        const bool selected = g_sort.column == col;
        std::string title = roomcats::Tx(kCatColumnLabels[col]);
        if (selected) title += g_sort.Ascending() ? "↑" : "↓";
        ImGui::RenderTextClipped(position, {position.x+width, position.y+line_height}, title.data(), title.data()+title.size(), nullptr);
        if (col == Age) {
            const ImVec2 lower{position.x, position.y+line_height+2};
            ImGui::RenderTextClipped(lower, {lower.x+width, lower.y+line_height}, roomcats::Tx("排序"), nullptr, nullptr);
        } else {
            float x = position.x;
            int part = 0;
            for (const auto* text : {roomcats::Tx("实"), "/", roomcats::Tx("遗")}) {
                const bool active_basis = selected && part == (g_sort.Genetic() ? 2 : 0);
                ImGui::PushStyleColor(ImGuiCol_Text, active_basis ? (roomcats::ui::Active() ? roomcats::ui::Ink() : ImVec4{1.0f,0.85f,0.48f,1.0f}) : ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
                const ImVec2 lower{x, position.y+line_height+2};
                ImGui::RenderTextClipped(lower, {position.x+width, lower.y+line_height}, text, nullptr, nullptr);
                ImGui::PopStyleColor();
                x += ImGui::CalcTextSize(text).x;
                ++part;
            }
        }
        if (g_mouse.CapturesPointer() && ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", col == Age ? roomcats::Tx("年龄在降序和升序之间切换。") : roomcats::Tx("点击表头依次切换：真实降序、真实升序、遗传降序、遗传升序。"));
        ImGui::PopID();
    }
}

void DrawUi() {
    using namespace roomcats;
    auto& io = ImGui::GetIO();
    if (!g_open) { g_panel_rect = {}; ClearTraitHover(); ClearMovePopup(); ResetListViews(); return; }
    if (g_close_move_popup) ClearMovePopup();
    g_rules_popup_drawn=false;
    g_language_popup_drawn=false;
    if (g_move_popup_cat && std::none_of(g_snapshot.cats.begin(), g_snapshot.cats.end(),
        [](const roomcats::Cat& c) { return c.id == g_move_popup_cat; })) ClearMovePopup();
    g_move_popup_drawn = false;
    const bool pad_popup = g_mouse.PopupOpen() && GImGui->OpenPopupStack.Size > 0;
    if (pad_popup) {
        auto* popup = GImGui->OpenPopupStack.back().Window;
        if (popup && g_pad_frame.scroll_y != 0) ImGui::SetScrollY(popup,popup->Scroll.y+g_pad_frame.scroll_y*650*io.DeltaTime);
    }
    const float height = std::min(780.0f, io.DisplaySize.y - 80.0f);
    const float width = std::min(1420.0f, io.DisplaySize.x - 64.0f);
    ImGui::SetNextWindowSize({width, height}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos({io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f}, ImGuiCond_FirstUseEver, {0.5f, 0.5f});
    ImGui::SetNextWindowSizeConstraints({620, 350}, {std::max(620.0f, io.DisplaySize.x - 32), std::max(350.0f, io.DisplaySize.y - 32)});
    if (ImGui::Begin(roomcats::Tx("猫咪列表###RoomCatsPanel"), &g_open, ImGuiWindowFlags_NoCollapse)) {
        roomcats::ui::WindowPaper(true,false,roomcats::Tx("猫咪列表"));
        const auto position = ImGui::GetWindowPos();
        const auto size = ImGui::GetWindowSize();
        // ImGui's resize border extends four pixels outside the window.
        g_panel_rect = {static_cast<int>(position.x) - 4, static_cast<int>(position.y) - 4,
            static_cast<int>(position.x + size.x) + 4, static_cast<int>(position.y + size.y) + 4};
        if (g_view.IsParents() || g_view.screening) {
            if (roomcats::ui::Button(g_view.screening ? roomcats::Tx("返回名单##ReviewBack") : roomcats::Tx("返回上一级##ParentBack"))) RequestListBack();
            ImGui::SameLine();
        }
        if (!g_snapshot.valid) {
            ImGui::TextWrapped("%s", g_snapshot.error.empty() ? roomcats::Tx("房间数据暂时不可用，请稍后重试。") : g_snapshot.error.c_str());
        } else {
            const auto label = g_view.screening ? std::string(g_screening_plan && g_screening_plan->kind==roomcats::ScreeningKind::Adult ? roomcats::Tx("老猫数量控制复查") : roomcats::Tx("新生猫筛选复查")) : g_view.IsParents() ? g_view.child_name+roomcats::Tx(" 的父母") : roomcats::RoomLabel(g_snapshot.room_id);
            ImGui::TextUnformatted(label.c_str());
            ImGui::SameLine();
            ImGui::TextDisabled(roomcats::Tx("  %zu 只猫咪"), g_snapshot.cats.size());
            DrawScreeningToolbar();
            ImGui::Separator();
            if (g_snapshot.cats.empty()) {
                if(g_view.screening) DrawEmptyScreeningReview();
                else ImGui::TextDisabled("%s", g_snapshot.room ? roomcats::Tx("此房间暂无猫咪。") : roomcats::Tx("家园暂无猫咪。"));
            }
            else if (ImGui::BeginTable(g_view.screening ? "RoomCatsScreeningRowsV1" : "RoomCatsDetailsRowsV6", CatColumnCount+(g_view.screening ? 2 : 0),
                    ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_ScrollY |
                    ImGuiTableFlags_ScrollX | ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingFixedFit,
                    {0, ImGui::GetContentRegionAvail().y - ImGui::GetTextLineHeightWithSpacing() * ((g_move_feedback.empty() ? 2 : 3) + (g_pad_frame.connected ? 1 : 0) + (g_view.screening ? (g_screening_plan && g_screening_plan->kind==roomcats::ScreeningKind::Adult ? 4 : 3) : 0))})) {
                if (g_restore_view_scroll) {
                    ImGui::SetScrollX(g_view.scroll.x); ImGui::SetScrollY(g_view.scroll.y);
                    g_restore_view_scroll = false; g_reset_list_scroll = false;
                } else if (g_reset_list_scroll) { ImGui::SetScrollY(0); g_reset_list_scroll = false; }
                if (!pad_popup && g_pad_frame.scroll_x != 0) ImGui::SetScrollX(ImGui::GetScrollX()+g_pad_frame.scroll_x*650*io.DeltaTime);
                if (!pad_popup && g_pad_frame.scroll_y != 0 && !g_trait_hover.cat) ImGui::SetScrollY(ImGui::GetScrollY()+g_pad_frame.scroll_y*650*io.DeltaTime);
                SetupScreeningColumns();
                ImGui::TableSetupScrollFreeze(g_view.screening ? 2 : 3, 1);
                DrawColumnHeaders();
                if (g_sort_dirty || g_sorted_rows.size() != g_snapshot.cats.size()) {
                    g_sorted_rows = roomcats::SortCatRows(g_snapshot.cats, g_sort);
                    g_sort_dirty = false;
                }
                ImGuiListClipper clipper;
                clipper.Begin(static_cast<int>(g_snapshot.cats.size()));
                if (g_move_popup_cat)
                    for (int row = 0; row < static_cast<int>(g_sorted_rows.size()); ++row)
                        if (g_snapshot.cats[g_sorted_rows[row]].id == g_move_popup_cat) { clipper.IncludeItemByIndex(row); break; }
                const float row_height = ImGui::GetTextLineHeight() * 2 + ImGui::GetStyle().ItemSpacing.y + ImGui::GetStyle().CellPadding.y * 2;
                while (clipper.Step()) {
                    for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
                        const auto& cat = g_snapshot.cats[g_sorted_rows[i]];
                        ImGui::TableNextRow(0, row_height);
                        if(g_view.screening && ImGui::TableSetColumnIndex(0)) DrawScreeningChoice(cat);
                        ImGui::TableSetColumnIndex(TableColumnIndex(Position));
                        DrawMoveSelector(cat);
                        if (ImGui::TableSetColumnIndex(TableColumnIndex(SendTo))) DrawNpcSelector(cat);
                        if (ImGui::TableSetColumnIndex(TableColumnIndex(Name))) DrawCatName(cat);
                        if (ImGui::TableSetColumnIndex(TableColumnIndex(Parents))) DrawParents(cat);
                        if (ImGui::TableSetColumnIndex(TableColumnIndex(Age))) {
                            if (cat.details.valid) ImGui::Text("%d", cat.details.age);
                            else ImGui::TextDisabled("—");
                        }
                        if (ImGui::TableSetColumnIndex(TableColumnIndex(SexOrientation))) DrawCompactValue(roomcats::FormatSexOrientation(cat.details));
                        if (ImGui::TableSetColumnIndex(TableColumnIndex(Total))) DrawStatValues(cat.details,-1);
                        for (int stat = 0; stat < 7; ++stat)
                            if (ImGui::TableSetColumnIndex(TableColumnIndex(stat + Strength))) DrawStatValues(cat.details,stat);
                        for (int group = 0; group < 3; ++group) {
                            if (!ImGui::TableSetColumnIndex(TableColumnIndex(group + GoodMutations))) continue;
                            if (!cat.details.valid) {
                                ImGui::TextDisabled("—");
                                if (g_mouse.CapturesPointer() && ImGui::IsItemHovered()) ImGui::SetTooltip(roomcats::Tx("详情暂不可用。"));
                                continue;
                            }
                            const auto cell_min = ImGui::GetCursorScreenPos();
                            const ImVec2 cell_max{cell_min.x + ImGui::GetContentRegionAvail().x,
                                cell_min.y + row_height - ImGui::GetStyle().CellPadding.y * 2};
                            const auto& traits = Traits(cat, group);
                            if (group < 2) {
                                if(group==0) ImGui::Text(roomcats::Tx("%zu / 优%d"),traits.size(),roomcats::QualityMutations(cat.details));
                                else ImGui::Text("%zu", traits.size());
                            } else if (traits.empty()) ImGui::TextDisabled(roomcats::Tx("无"));
                            else for (const auto& trait : traits) {
                                ImGui::TextUnformatted(roomcats::TraitName(trait).c_str());
                            }
                            TrackTraitHover(cat, group, cell_min, cell_max);
                        }
                        if(g_view.screening && ImGui::TableSetColumnIndex(CatColumnCount+1))
                            if(const auto* d=ScreeningDecision(cat.id)) DrawCompactValue(d->reason);
                    }
                }
                g_list_scroll = {ImGui::GetScrollX(),ImGui::GetScrollY()};
                ImGui::EndTable();
            }
        }
        ImGui::Spacing();
        if(g_view.screening) DrawScreeningSummary();
        if (!g_move_feedback.empty()) ImGui::TextUnformatted(g_move_feedback.c_str());
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
        ImGui::TextWrapped(roomcats::Tx("滚轮上下 · Shift+滚轮左右 · 点击表头排序 · %s"),g_view.IsParents() || g_view.screening ? roomcats::Tx("Esc 返回上一级") : roomcats::Tx("Esc 关闭"));
        ImGui::PopStyleColor();
        if (g_pad_frame.connected) ImGui::TextDisabled(roomcats::Tx("手柄：左摇杆移动光标 · A 点击 · B 返回 · 右摇杆滚动"));
    }
    DrawScreeningSettings();
    ImGui::End();
    if (g_move_popup_cat && !g_move_popup_drawn) ClearMovePopup();
    const bool popup_open = (g_move_popup_drawn || g_rules_popup_drawn || g_language_popup_drawn) && ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel);
    g_mouse.SetPopupOpen(popup_open);
    if (!popup_open) g_move_popup_cat = 0;
    DrawTraitHover();
    ApplyQueuedListView();
}

void HandleGamepadBack(bool popup_before_frame) {
    if (g_pad_frame.back_pressed && g_open && g_has_house) {
        if (popup_before_frame) g_close_move_popup = true;
        else if (!RequestListBack()) g_open = false;
    }
    g_pad_frame.back_pressed = false;
}

}
