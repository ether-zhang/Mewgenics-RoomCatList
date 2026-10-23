void RegisterDisplaySettings() {
    if (ImGui::FindSettingsHandler("RoomCatDisplay")) return;
    ImGuiSettingsHandler handler;
    handler.TypeName="RoomCatDisplay"; handler.TypeHash=ImHashStr(handler.TypeName);
    handler.ReadInitFn=[](ImGuiContext*,ImGuiSettingsHandler*) { g_font_percent=100; g_group=roomcats::CatGroup::None; };
    handler.ReadOpenFn=[](ImGuiContext*,ImGuiSettingsHandler*,const char* name)->void* {
        return std::strcmp(name,"Preferences")==0 ? &g_font_percent : nullptr;
    };
    handler.ReadLineFn=[](ImGuiContext*,ImGuiSettingsHandler*,void*,const char* line) {
        const std::string_view text=line;
        const auto equals=text.find('=');
        if(equals==std::string_view::npos) return;
        const auto key=text.substr(0,equals), number=text.substr(equals+1);
        int value=0;
        const auto result=std::from_chars(number.data(),number.data()+number.size(),value);
        if(result.ec!=std::errc{} || result.ptr!=number.data()+number.size()) return;
        if (key=="font_percent" && value>=kMinFontPercent && value<=kMaxFontPercent) g_font_percent=value;
        if (key=="group" && value>=0 && value<=3) g_group=static_cast<roomcats::CatGroup>(value);
    };
    handler.ApplyAllFn=[](ImGuiContext*,ImGuiSettingsHandler*) { g_sort_dirty=g_resize_for_font=true; };
    handler.WriteAllFn=[](ImGuiContext*,ImGuiSettingsHandler*,ImGuiTextBuffer* out) {
        out->appendf("[RoomCatDisplay][Preferences]\nfont_percent=%d\ngroup=%d\n\n",g_font_percent,int(g_group));
    };
    ImGui::AddSettingsHandler(&handler);
}
void SetListFontPercent(int value) {
    value=std::clamp(value,kMinFontPercent,kMaxFontPercent);
    if(value==g_font_percent) return;
    g_font_percent=value;
    ClearMovePopup(); ClearTraitHover();
    g_reset_list_scroll=g_resize_for_font=g_display_save_requested=true;
    ImGui::MarkIniSettingsDirty();
}
void SetCatGroup(roomcats::CatGroup group) {
    if(group==g_group) return;
    g_group=group; g_sort_dirty=g_reset_list_scroll=g_display_save_requested=true;
    ClearTraitHover(); ImGui::MarkIniSettingsDirty();
}
std::string CatGroupLabel(const roomcats::Cat& cat) {
    if(g_group==roomcats::CatGroup::Location) return cat.location_label.empty() ? roomcats::Tx("未分配") : cat.location_label;
    if(g_group==roomcats::CatGroup::Class) return roomcats::ClassLabel(cat.details.collar);
    const auto glyph=roomcats::MarkerGlyph(cat.details.marker);
    return glyph.empty() ? (cat.details.marker.empty() ? roomcats::Tx("未标记") : cat.details.marker) : glyph;
}
void RebuildTableRows() {
    g_sorted_rows=roomcats::SortCatRows(g_snapshot.cats,g_sort,g_group);
    g_table_rows.clear();
    std::string previous;
    int header=-1;
    for(int index:g_sorted_rows) {
        const auto& cat=g_snapshot.cats[index];
        const auto key=roomcats::CatGroupKey(cat,g_group);
        if(g_group!=roomcats::CatGroup::None && (header<0 || key!=previous)) {
            header=int(g_table_rows.size()); previous=key;
            g_table_rows.push_back({-1,CatGroupLabel(cat),0});
        }
        if(header>=0) ++g_table_rows[header].count;
        g_table_rows.push_back({index,{},0});
    }
    g_sort_dirty=false;
}
std::string CatGroupButtonLabel() {
    static const char* labels[]={"不分组","位置","职业","标记"};
    char text[128];
    ImFormatString(text,sizeof(text),roomcats::Tx("分组：%s"),roomcats::Tx(labels[int(g_group)]));
    return text;
}
void DrawCatGroupButton() {
    const auto label=CatGroupButtonLabel()+"###RoomCatGroupButton";
    const auto popup_id=ImGui::GetID("##RoomCatGroupMenu");
    const bool pressed=roomcats::ui::Button(label.c_str());
    const ImRect button_bounds(ImGui::GetItemRectMin(),ImGui::GetItemRectMax());
    if(pressed) {
        ClearMovePopup(); ClearTraitHover();
        ImGui::OpenPopupEx(popup_id,ImGuiPopupFlags_None);
    }
    if(!ImGui::IsPopupOpen(popup_id,ImGuiPopupFlags_None) && ImGui::IsItemHovered())
        ImGui::SetTooltip("%s",roomcats::Tx("先分组，再按表头选择的条件在组内排序。"));
    // Use the same button anchor, minimum width and screen-edge handling as
    // the language dropdown, instead of positioning a context menu at the mouse.
    if(ImGui::BeginComboPopup(popup_id,button_bounds,ImGuiComboFlags_HeightSmall)) {
        roomcats::ui::WindowPaper(false,true);
        g_display_popup_drawn=true; g_mouse.SetPopupOpen(true);
        static const char* labels[]={"不分组","按位置","按职业","按标记"};
        for(int i=0;i<4;++i)
            if(ImGui::Selectable(roomcats::Tx(labels[i]),int(g_group)==i)) SetCatGroup(static_cast<roomcats::CatGroup>(i));
        ImGui::EndCombo();
    }
}
float ListFontValueWidth() {
    return std::max(ImGui::CalcTextSize("100%").x,ImGui::CalcTextSize("150%").x)+ImGui::GetStyle().FramePadding.x;
}
float ListFontButtonWidth() {
    return std::max(ImGui::GetFrameHeight(),ImGui::CalcTextSize("+").x+ImGui::GetStyle().FramePadding.x*2);
}
float ListFontControlsWidth() {
    return ListFontButtonWidth()*2+ListFontValueWidth()+ImGui::GetStyle().ItemSpacing.x*2;
}
void ListFooterText(char* text,std::size_t size) {
    ImFormatString(text,size,roomcats::Tx("滚轮上下 · Shift+滚轮左右 · 点击表头排序 · %s"),
        g_view.IsParents() || g_view.screening ? roomcats::Tx("Esc 返回上一级") : roomcats::Tx("Esc 关闭"));
}
float ListFooterHeight() {
    char text[512]; ListFooterText(text,sizeof(text));
    const float wrap=std::max(20.0f,ImGui::GetContentRegionAvail().x-ListFontControlsWidth()-ImGui::GetStyle().ItemSpacing.x*2);
    return std::max(ImGui::GetFrameHeight(),ImGui::CalcTextSize(text,nullptr,false,wrap).y+ImGui::GetStyle().FramePadding.y*2)
        +ImGui::GetStyle().ItemSpacing.y*2;
}
void DrawListFooter() {
    const auto start=ImGui::GetCursorPos();
    const float height=ImGui::GetFrameHeight(),button_width=ListFontButtonWidth(),gap=ImGui::GetStyle().ItemSpacing.x;
    const float value_width=ListFontValueWidth(),controls_width=ListFontControlsWidth();
    const float controls_x=start.x+ImGui::GetContentRegionAvail().x-controls_width;
    char text[512]; ListFooterText(text,sizeof(text));
    ImGui::BeginGroup();
    ImGui::AlignTextToFramePadding();
    ImGui::PushTextWrapPos(std::max(start.x+20,controls_x-gap*2));
    ImGui::TextDisabled("%s",text); ImGui::PopTextWrapPos();
    ImGui::SetCursorPos({controls_x,start.y});
    ImGui::BeginGroup();
    ImGui::BeginDisabled(g_font_percent<=kMinFontPercent);
    if(roomcats::ui::Button("-###RoomCatFontDown",{button_width,height})) SetListFontPercent(g_font_percent-kFontPercentStep);
    ImGui::EndDisabled();
    const auto percentage=std::to_string(g_font_percent)+"%";
    // SameLine offsets inside this group are relative to the controls group.
    ImGui::SameLine(button_width+gap+(value_width-ImGui::CalcTextSize(percentage.c_str()).x)*.5f);
    ImGui::AlignTextToFramePadding(); ImGui::TextDisabled("%s",percentage.c_str());
    ImGui::SameLine(button_width+gap+value_width+gap);
    ImGui::BeginDisabled(g_font_percent>=kMaxFontPercent);
    if(roomcats::ui::Button("+###RoomCatFontUp",{button_width,height})) SetListFontPercent(g_font_percent+kFontPercentStep);
    ImGui::EndDisabled(); ImGui::EndGroup();
    if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip(roomcats::Tx("字号：%d%%–%d%%，每次调整%d%%。"),kMinFontPercent,kMaxFontPercent,kFontPercentStep);
    ImGui::EndGroup();
}
