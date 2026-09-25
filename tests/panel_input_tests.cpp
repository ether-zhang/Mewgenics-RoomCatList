// Exercise the actual panel with Dear ImGui's real input queue. No HWND,
// graphics context, game process or game functions are created/called.
#include "ui_test_host.hpp"
#include <cassert>
#include <iostream>
int TestParentPanel();
int TestNewbornPlan();
int TestAdultPlan();
int TestScreeningChoices();
int TestScreeningRules();
int TestScreeningSettingsPanel();
int TestNewbornBatch();
int TestScreeningPanel();
int TestLanguagePanel();

namespace {
int game_downs = 0, game_ups = 0, game_wheels = 0;
int ui_moves = 0;
int ui_actions = 0;
std::vector<roomcats::NpcChoice> QueryUiNpcs(const roomcats::RoomSnapshot&, const roomcats::Cat&) {
    return {{0,"Ready NPC",true,""}, {1,"Unavailable NPC",false,"Not accepted"}};
}
roomcats::CatActionStatus PerformUiAction(const roomcats::RoomSnapshot&, roomcats::CatAction& action, std::string& message) {
    ++ui_actions;
    assert(action.cat == 1);
    message = "Native UI opened";
    return roomcats::CatActionStatus::Complete;
}
bool CheckUiMove(const roomcats::RoomSnapshot&, const roomcats::Cat&, const roomcats::CatLocation&, std::string&) { return true; }
bool PerformUiMove(const roomcats::RoomSnapshot&, const roomcats::Cat& cat, const roomcats::CatLocation& target, std::string&) {
    ++ui_moves;
    assert(cat.id == 1 && target.key == "box");
    return true;
}

void Move(float x, float y) {
    g_mouse.Move(static_cast<int>(x), static_cast<int>(y));
    ImGui::GetIO().AddMousePosEvent(x, y);
}

void Button(bool down) {
    const auto owner = down ? g_mouse.Down(1) : g_mouse.Up(1);
    if (owner == roomcats::MouseOwner::List) ImGui::GetIO().AddMouseButtonEvent(0, down);
    else ++(down ? game_downs : game_ups);
}

void Wheel(float amount) {
    if (g_mouse.Wheel() == roomcats::MouseOwner::List) ImGui::GetIO().AddMouseWheelEvent(0, amount);
    else ++game_wheels;
}

void Frame(int count = 1) {
    for (int i = 0; i < count; ++i) {
        const bool had_popup = g_mouse.PopupOpen();
        ImGui::NewFrame();
        HandleGamepadBack(had_popup);
        DrawUi();
        PublishMouseCapture();
        ImGui::Render();
        assert(!ImGui::GetIO().MouseDrawCursor);
        g_mouse.EndFrame();
    }
}

ImGuiWindow* ScrollingChild() {
    for (auto* window : ImGui::GetCurrentContext()->Windows)
        if (window->Active && window->ParentWindow && window->ScrollMax.y > 0) return window;
    return nullptr;
}
}

int MakeUiThemePreview(bool chinese);
int main(int argc,char** argv) {
    if(argc>1 && std::string(argv[1])=="--ui-preview") return MakeUiThemePreview(false);
    if(argc>1 && std::string(argv[1])=="--ui-preview-cn") return MakeUiThemePreview(true);
    if(argc>1 && std::string(argv[1])=="--language-test") return TestLanguagePanel();
    roomcats::SetLanguage(roomcats::Language::Chinese); // Existing UI fixtures assert the original labels.
    ImGui::CreateContext();
    auto& io = ImGui::GetIO();
    io.IniFilename = io.LogFilename = nullptr;
    io.DisplaySize = {1280, 900};
    io.DeltaTime = 1.0f / 60.0f;
    io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
    auto* test_font = io.Fonts->AddFontDefault();
    roomcats::BuildMarkerGlyphs(*io.Fonts, test_font);
    for (int i = 0; i < roomcats::kMarkerCount; ++i) assert(test_font->FindGlyphNoFallback(roomcats::kMarkerGlyphStart+i));
    assert(roomcats::kClassCount == 14);
    for (int i=0;i<roomcats::kClassCount;++i) {
        assert(test_font->FindGlyphNoFallback(roomcats::kClassGlyphStart+i));
        assert(!roomcats::ClassGlyph(kClassIcons[i].name).empty());
    }
    assert(roomcats::ClassLabel("Fighter") == "战士" && roomcats::ClassLabel("Mage") == "法师");
    assert(roomcats::ClassGlyph("") == roomcats::ClassGlyph("Colorless"));
    assert(roomcats::ClassGlyph("CustomClass") == "?");
    for (double base : {5.0,7.0}) {
        const auto white = StatValueColor(base,base), green = StatValueColor(base+1,base), red = StatValueColor(base-1,base);
        assert(white.x == 1 && white.y == 1 && white.z == 1);
        assert(green.y == 1 && green.x < 1 && green.z < 1);
        assert(red.x == 1 && red.y < 1 && red.z < 1);
        assert(StatValueColor(base+2,base).x < green.x && StatValueColor(base-2,base).y < red.y);
    }
    unsigned char* pixels = nullptr;
    int width = 0, height = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    g_open = g_has_house = g_snapshot.valid = true;
    g_snapshot.in_house = true;
    g_snapshot.scene = 100;
    g_snapshot.generation = 1;
    g_snapshot.locations = {{"room", "Room", 1000, 1, roomcats::LocationKind::Room},
                            {"box", "Box", 2000, 2, roomcats::LocationKind::Box}};
    roomcats::ConfigureCatMoves(CheckUiMove, PerformUiMove);
    roomcats::ConfigureCatActions(QueryUiNpcs, PerformUiAction);
    std::string long_effect;
    for (int i = 0; i < 100; ++i) long_effect += "A long effect remains readable by scrolling.\n";
    for (int i = 0; i < 95; ++i) {
        roomcats::Cat cat{static_cast<unsigned>(i + 1), "Cat " + std::to_string(i)};
        cat.details.valid = true;
        cat.location_key = "room";
        cat.location_label = "Room";
        cat.details.age = i;
        cat.details.marker = i == 0 ? "sword" : "";
        cat.parents = {true, {123, 456}, {"First parent", "Second parent"}};
        cat.details.inbreeding = 0.2;
        cat.details.sex = i % 3; cat.details.sexuality = (i % 11) / 10.0;
        cat.details.real.fill(i % 13);
        cat.details.genetic.fill(i % 9);
        cat.details.good.push_back({"Body mutation", "Constitution +1"});
        cat.details.bad.push_back({"Leg defect", "Movement -1"});
        cat.details.diseases = {{"Condition A", long_effect}, {"Condition B", "Second effect."}};
        g_snapshot.cats.push_back(std::move(cat));
    }
    Frame(3);
    auto* rows = ScrollingChild();
    assert(rows);
    const auto center = rows->InnerRect.GetCenter();

    Move(center.x, center.y);
    Frame(2);
    Wheel(-3);
    Frame(3);
    assert(rows->Scroll.y > 0);
    const float old_scroll = rows->Scroll.y;
    Move(20, 20);
    Wheel(-3);
    Frame(3);
    assert(rows->Scroll.y == old_scroll && game_wheels == 1);

    // Drag from the real scrollbar to outside the panel and release there.
    const auto bar = ImGui::GetWindowScrollbarRect(rows, ImGuiAxis_Y);
    // Start on the thumb; clicking the track initiates repeated page scrolling.
    const float visible_height = rows->InnerRect.GetHeight();
    const float thumb_center = bar.Min.y + bar.GetHeight() *
        (rows->Scroll.y + visible_height * 0.5f) / (rows->ScrollMax.y + visible_height);
    Move(bar.GetCenter().x, thumb_center);
    Frame(2);
    Button(true);
    Frame(2);
    assert(ImGui::GetCurrentContext()->ActiveId != 0);
    Move(bar.GetCenter().x, g_panel_rect.bottom + 50.0f);
    Frame(2);
    assert(g_mouse.CapturesPointer() && rows->Scroll.y > old_scroll);
    Button(false);
    Frame(3);
    assert(!io.MouseDown[0] && !g_mouse.HasButtons());
    assert(game_downs == 0 && game_ups == 0);

    // A game-origin gesture cannot activate a panel widget as it crosses it.
    Move(20, 20);
    Button(true);
    Move(bar.GetCenter().x, bar.Min.y + 25);
    Frame(2);
    assert(!io.MouseDown[0] && ImGui::GetCurrentContext()->ActiveId == 0);
    Button(false);
    Frame(3);
    assert(game_downs == 1 && game_ups == 1);

    // Both axes exist, and Shift+wheel moves X without moving Y.
    assert(rows->ScrollMax.x > 0 && rows->ScrollMax.y > 0);
    Move(center.x, center.y);
    Frame(30);
    const auto before_horizontal = rows->Scroll;
    io.AddKeyEvent(ImGuiMod_Shift, true);
    Wheel(-5);
    Frame(5);
    io.AddKeyEvent(ImGuiMod_Shift, false);
    Frame(2);
    assert(rows->Scroll.x > before_horizontal.x && rows->Scroll.y == before_horizontal.y);

    auto* panel = ImGui::FindWindowByName("猫咪列表###RoomCatsPanel");
    auto* table = ImGui::GetCurrentContext()->Tables.GetByKey(ImHashStr("RoomCatsDetailsRowsV6", 0, panel->ID));
    assert(table && table->ColumnsCount == roomcats::CatColumnCount);
    for (int col = roomcats::Total; col <= roomcats::Luck; ++col) {
        assert(table->Columns[col].WidthGiven < 80);
        if (col != roomcats::Total) assert(table->Columns[col].WidthGiven < 60);
        assert(table->Columns[col].WidthGiven >= ImGui::CalcTextSize("实/遗").x);
    }
    assert(table->Columns[roomcats::SexOrientation].WidthGiven < 90);
    assert(std::string(roomcats::kCatColumnLabels[roomcats::SexOrientation]) == "性/取/繁");

    for (int i = 0; i < roomcats::Diseases; ++i) assert(table->Columns[roomcats::Diseases].WidthGiven > table->Columns[i].WidthGiven);
    ImGui::SetScrollX(rows, 0);
    ImGui::SetScrollY(rows, 0);
    Frame(3);

    const auto selector_y = table->OuterRect.Min.y + ImGui::GetTextLineHeight() * 2 + 4 + ImGui::GetStyle().CellPadding.y * 2 + 8;
    const auto selector_x = table->Columns[0].WorkMinX + 40;
    Move(selector_x, selector_y);
    Frame(2);
    Button(true); Frame(); Button(false); Frame(3);
    assert(g_mouse.PopupOpen() && g_move_popup_cat == 1 && ui_moves == 0);
    auto* menu = ImGui::GetCurrentContext()->OpenPopupStack.back().Window;
    assert(menu && menu->Active);
    Move(menu->Pos.x + ImGui::GetStyle().WindowPadding.x + 35,
         menu->Pos.y + ImGui::GetStyle().WindowPadding.y + ImGui::GetTextLineHeightWithSpacing() + ImGui::GetTextLineHeight() * 0.5f);
    Frame(2);
    Button(true); Frame(); Button(false); Frame(3);
    assert(roomcats::HasPendingCatMove() && ui_moves == 0 && !g_mouse.PopupOpen());
    roomcats::ProcessCatMove(g_snapshot);
    roomcats::MoveResult result;
    assert(roomcats::TakeCatMoveResult(result) && result.success && ui_moves == 1);
    g_move_feedback = result.message;
    g_snapshot.cats[0].location_key = "box";
    g_snapshot.cats[0].location_label = "Box";
    Frame(3);
    Move(selector_x, selector_y);
    Frame(2);
    Button(true); Frame(); Button(false); Frame(3);
    assert(g_mouse.PopupOpen());
    Move(20, 20);
    Button(true); Frame(); Button(false); Frame(3);
    assert(!g_mouse.PopupOpen() && !roomcats::HasPendingCatMove() && ui_moves == 1);
    assert(game_downs == 1 && game_ups == 1); // Dismissal must not click the house.
    Move(selector_x, selector_y);
    Frame(2);
    Button(true); Frame(); Button(false); Frame(3);
    assert(g_mouse.PopupOpen());
    g_close_move_popup = true;
    Frame(3);
    assert(g_open && !g_mouse.PopupOpen()); // Escape first dismisses the dropdown.
    // New NPC dropdown preserves the same popup input ownership. Disabled
    // choices stay open and never queue, while selecting the enabled NPC
    // queues only; the frame does not execute any native action.
    Move(table->Columns[roomcats::SendTo].WorkMinX+40, selector_y);
    Frame(2); Button(true); Frame(); Button(false); Frame(3);
    assert(g_mouse.PopupOpen() && ui_actions == 0);
    menu = ImGui::GetCurrentContext()->OpenPopupStack.back().Window;
    const float npc_x = menu->Pos.x + ImGui::GetStyle().WindowPadding.x + 25;
    const float npc_y = menu->Pos.y + ImGui::GetStyle().WindowPadding.y + ImGui::GetTextLineHeight()*0.5f;
    Move(npc_x,npc_y+ImGui::GetTextLineHeightWithSpacing());
    Frame(2); Button(true); Frame(); Button(false); Frame(3);
    assert(g_mouse.PopupOpen() && !roomcats::HasPendingCatAction() && ui_actions == 0);
    Move(npc_x,npc_y);
    Frame(2); Button(true); Frame(); Button(false); Frame(3);
    assert(!g_mouse.PopupOpen() && roomcats::HasPendingCatAction() && ui_actions == 0);
    roomcats::ProcessCatAction(g_snapshot, 100);
    roomcats::CatActionResult action_result;
    assert(roomcats::TakeCatActionResult(action_result) && !action_result.hide_list && ui_actions == 1);
    Move(table->Columns[roomcats::Name].WorkMinX+45, selector_y);
    Frame(2); Button(true); Frame(); Button(false); Frame(3);
    assert(roomcats::HasPendingCatAction() && ui_actions == 1);
    roomcats::ProcessCatAction(g_snapshot, 200);
    assert(roomcats::TakeCatActionResult(action_result) && !action_result.hide_list && ui_actions == 2);
    Frame(3);
    assert(g_open && panel->Active); // Inspecting a cat keeps the list available.
    assert(table->Columns[roomcats::Parents].WidthGiven > table->Columns[roomcats::Age].WidthGiven);
    Move(table->Columns[roomcats::Age].WorkMinX + 15, table->OuterRect.Min.y + 10);
    Frame(2);
    Button(true); Frame(); Button(false); Frame(3);
    assert(g_sort.column == roomcats::Age && !g_sort.Ascending() && g_sorted_rows.front() == 94);
    Button(true); Frame(); Button(false); Frame(3);
    assert(g_sort.Ascending() && g_sorted_rows.front() == 0);
    Move(table->Columns[roomcats::Total].WorkMinX + 15, table->OuterRect.Min.y + 10);
    Frame(2);
    constexpr int first_by_total[] = {12, 0, 8, 0};
    for (int mode = 0; mode < 4; ++mode) {
        Button(true); Frame(); Button(false); Frame(3);
        assert(g_sort.column == roomcats::Total && g_sort.mode == mode && g_sorted_rows.front() == first_by_total[mode]);
    }

    // Disease names open a scrollable detail list. Wheel input over the hover
    // window must not scroll the underlying cat table or leak to the game.
    ImGui::SetScrollX(rows, rows->ScrollMax.x);
    Frame(3);
    Move(table->Columns[roomcats::Diseases].WorkMinX + 12,
         table->OuterRect.Min.y + ImGui::GetTextLineHeight() * 2 + 4 + ImGui::GetStyle().CellPadding.y * 2 + 8);
    Frame(3);
    assert(g_trait_hover.cat != 0);
    auto* tip = ImGui::FindWindowByName("##RoomCatsTraitHover");
    assert(tip && tip->Active && tip->ScrollMax.y > 0);
    // Focusing/reusing the main list must never put a reused hover window
    // underneath it. Check actual draw-list order, not merely window activity.
    ImGui::FocusWindow(panel);
    Frame(3);
    int panel_draw = -1, tip_draw = -1;
    const auto* draw = ImGui::GetDrawData();
    for (int i = 0; i < draw->CmdListsCount; ++i) {
        if (draw->CmdLists[i] == panel->DrawList) panel_draw = i;
        if (draw->CmdLists[i] == tip->DrawList) tip_draw = i;
    }
    assert(panel_draw >= 0 && tip_draw > panel_draw);

    // Blank space in either mutation cell must trigger, and moving across the
    // same row must switch groups without the old popup covering the target.
    const float trait_y = table->OuterRect.Min.y + ImGui::GetTextLineHeight() * 2 + 4 + ImGui::GetStyle().CellPadding.y * 2 + 8;
    for (int group : {0, 1, 2, 0, 2}) {
        Move(table->Columns[group + roomcats::GoodMutations].WorkMinX + 60, trait_y);
        Frame(3);
        assert(g_trait_hover.cat != 0 && g_trait_hover.group == group);
        assert(tip->Pos.y >= g_trait_hover.source_max.y || tip->Pos.y + tip->Size.y <= g_trait_hover.source_min.y);
        assert(tip->Size.x > 0 && tip->Size.y > 0 && tip->DrawList->VtxBuffer.Size > 0);
        ImGui::FocusWindow(panel);
        Frame(2);
        const auto* last_draw = ImGui::GetDrawData()->CmdLists.back();
        bool hover_drawn_last = last_draw == tip->DrawList;
        for (auto* child : tip->DC.ChildWindows)
            if (child->Active && child->DrawList == last_draw) hover_drawn_last = true;
        assert(hover_drawn_last);
        if (group < 2) {
            assert(tip->Size.x > 540 && tip->DC.ChildWindows.Size == 2);
            assert(tip->DC.ChildWindows[1]->Pos.x > tip->DC.ChildWindows[0]->Pos.x);
        }
    }
    Move(g_trait_hover.bounds.left + 80.0f, g_trait_hover.bounds.top + 90.0f);
    Frame(30);
    assert(ImGui::GetCurrentContext()->HoveredWindow == tip);
    const auto table_scroll = rows->Scroll;
    Wheel(-4);
    Frame(4);
    assert(tip->Scroll.y > 0 && rows->Scroll.x == table_scroll.x && rows->Scroll.y == table_scroll.y);
    Move(20, 20);
    Frame(20);
    assert(!g_trait_hover.cat && !g_mouse.CapturesPointer());

    // Down/up may both be queued before a frame, followed by leaving the list.
    // ImGui deliberately processes that click across multiple frames.
    panel = ImGui::FindWindowByName("猫咪列表###RoomCatsPanel");
    assert(panel);
    const auto title = panel->TitleBarRect();
    Move(title.Max.x - title.GetHeight() * 0.5f, title.GetCenter().y);
    Frame(2);
    Button(true);
    Button(false);
    Move(20, 20);
    Frame(5);
    assert(!g_open && !io.MouseDown[0]);
    assert(game_downs == 1 && game_ups == 1);

    // Reopening must retain the user's layout, and the saved settings must
    // also survive destruction/recreation of the entire UI context.
    g_open = true;
    Frame(2);
    ImGui::SetWindowPos(panel, {222, 133});
    ImGui::SetWindowSize(panel, {620, 610});
    Frame(2);
    g_open = false;
    Frame(2);
    g_open = true;
    Frame(2);
    assert(panel->Pos.x == 222 && panel->Pos.y == 133);
    assert(panel->Size.x == 620 && panel->Size.y == 610);
    const std::string saved = ImGui::SaveIniSettingsToMemory();
    assert(saved.find("[Window][###RoomCatsPanel]") != std::string::npos);
    ImGui::DestroyContext();
    roomcats::ConfigureCatMoves(nullptr, nullptr);
    roomcats::ConfigureCatActions(nullptr, nullptr);
    ImGui::CreateContext();
    auto& restored_io = ImGui::GetIO();
    restored_io.IniFilename = restored_io.LogFilename = nullptr;
    restored_io.DisplaySize = {1280, 900};
    restored_io.DeltaTime = 1.0f / 60.0f;
    restored_io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    ImGui::LoadIniSettingsFromMemory(saved.c_str(), saved.size());
    Frame(3);
    panel = ImGui::FindWindowByName("猫咪列表###RoomCatsPanel");
    assert(panel && panel->Pos.x == 222 && panel->Pos.y == 133);
    assert(panel->Size.x == 620 && panel->Size.y == 610);
    ImGui::DestroyContext();
    std::cout << "Window layout passed: reopen and settings round trip preserve position and size.\n";
    std::cout << "Detailed table passed: 17 columns, total/age header sorting, widest disease column, X/Y scrolling, full-cell hover and topmost interactive descriptions.\n";
    std::cout << "Move selector passed: box choice, deferred dispatch, no preview side effects, dismissal capture and Escape.\n";
    std::cout << "Actual ImGui panel passed: wheel ownership, scrollbar drag/release outside, game drag isolation and queued close click.\n";
    if(TestParentPanel() || TestNewbornPlan() || TestAdultPlan() || TestScreeningChoices() || TestScreeningRules() || TestNewbornBatch()) return 1;
    return TestScreeningPanel() || TestScreeningSettingsPanel() || TestLanguagePanel();
}
