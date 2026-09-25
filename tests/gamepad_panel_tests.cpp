// Actual UI and pointer router, simulated game: no native hooks or device APIs.
#include "ui_test_host.hpp"
#include "../src/gamepad_router.hpp"
#include <iostream>
#define CHECK(x) do { if (!(x)) { std::cerr << "Check failed, line " << __LINE__ << ": " << #x << "\n"; return 1; } } while(false)
namespace {
roomcats::GamepadRouter pad;
int actions=0, moves=0, game_clicks=0, last_target=-1;
roomcats::CatActionKind last_kind{};
std::vector<roomcats::NpcChoice> Choices(const roomcats::RoomSnapshot&,const roomcats::Cat&) {
    return {{0,"NPC",true,""},{1,"Disabled NPC",false,"Not accepted"},{8,"Discard",true,""}};
}
roomcats::CatActionStatus Action(const roomcats::RoomSnapshot&,roomcats::CatAction& a,std::string&) {
    ++actions; last_kind=a.kind; last_target=a.npc; return roomcats::CatActionStatus::Complete;
}
bool CanMove(const roomcats::RoomSnapshot&,const roomcats::Cat&,const roomcats::CatLocation&,std::string&) { return true; }
bool MoveCat(const roomcats::RoomSnapshot&,const roomcats::Cat&,const roomcats::CatLocation&,std::string&) { ++moves; return true; }
void Move(float x,float y) { g_mouse.Move(int(x),int(y)); ImGui::GetIO().AddMousePosEvent(x,y); }
void Frame(int count=1) {
    for (int i=0;i<count;++i) {
        pad.SetActive(g_open && g_has_house,g_mouse);
        for (const auto& click:pad.TakeClicks()) {
            ImGui::GetIO().AddMousePosEvent(float(click.x),float(click.y));
            ImGui::GetIO().AddMouseButtonEvent(0,click.down || (g_mouse.ListButtons() & 1u));
        }
        g_pad_frame.back_pressed=pad.TakeBack();
        const bool popup=g_mouse.PopupOpen();
        ImGui::NewFrame(); HandleGamepadBack(popup);
        if (g_has_house) DrawUi();
        else { g_panel_rect={}; ClearMovePopup(); ClearTraitHover(); }
        PublishMouseCapture(); ImGui::Render(); g_mouse.EndFrame();
    }
}
void Click() {
    if (pad.ButtonForGame(0,true,g_mouse)) ++game_clicks;
    Frame(2); pad.ButtonForGame(0,false,g_mouse); Frame(3);
}
void Back() { pad.ButtonForGame(1,true,g_mouse); Frame(2); pad.ButtonForGame(1,false,g_mouse); Frame(2); }
void PointAtOption(int index) {
    auto* menu=GImGui->OpenPopupStack.back().Window;
    Move(menu->Pos.x+ImGui::GetStyle().WindowPadding.x+25,
         menu->Pos.y+ImGui::GetStyle().WindowPadding.y+ImGui::GetTextLineHeight()*0.5f+index*ImGui::GetTextLineHeightWithSpacing());
    Frame(2);
}
}
int main() {
    using namespace roomcats;
    ImGui::CreateContext(); roomcats::ui::Apply(false);
    auto& io=ImGui::GetIO(); io.IniFilename=io.LogFilename=nullptr;
    io.DisplaySize={1280,900}; io.DeltaTime=1.0f/60;
    io.ConfigFlags|=ImGuiConfigFlags_NavEnableKeyboard;
    unsigned char* pixels; int w,h; io.Fonts->GetTexDataAsRGBA32(&pixels,&w,&h);
    g_open=g_has_house=g_snapshot.in_house=g_snapshot.valid=true;
    g_snapshot.scene=100; g_snapshot.generation=1;
    g_snapshot.locations={{"room","Room",1000,1,LocationKind::Room},{"box","Box",2000,1,LocationKind::Box}};
    for (int i=0;i<90;++i) {
        Cat cat; cat.id=i+1; cat.name="Cat "+std::to_string(i); cat.details.valid=true; cat.details.age=i;
        cat.location_key="room"; cat.location_label="Room";
        cat.details.good={{"Mutation","An effect"}}; cat.details.bad={{"Defect","Another effect"}};
        cat.details.diseases={{"Disease","Disease effect"}};
        g_snapshot.cats.push_back(cat);
    }
    ConfigureCatActions(Choices,Action); ConfigureCatMoves(CanMove,MoveCat);
    g_pad_frame.connected=true; Move(20,20); Frame(4);
    auto* panel=ImGui::FindWindowByName("猫咪列表###RoomCatsPanel");
    auto* table=GImGui->Tables.GetByKey(ImHashStr("RoomCatsDetailsRowsV6",0,panel->ID));
    auto* rows=table->InnerWindow;
    const float y=table->OuterRect.Min.y+ImGui::GetTextLineHeight()*2+4+ImGui::GetStyle().CellPadding.y*2+8;
    Move(table->Columns[Position].WorkMinX+40,y); Frame(3);
    CHECK(g_mouse.CapturesPointer() && !io.MouseDrawCursor);
    Click(); CHECK(g_mouse.PopupOpen() && g_move_popup_cat==1 && game_clicks==0);
    PointAtOption(1); Click();
    CHECK(HasPendingCatMove() && !g_mouse.PopupOpen());
    ProcessCatMove(g_snapshot); MoveResult movement;
    CHECK(TakeCatMoveResult(movement) && movement.success && moves==1 && g_open);
    Move(table->Columns[SendTo].WorkMinX+40,y); Frame(2); Click();
    CHECK(g_mouse.PopupOpen()); PointAtOption(1); Click();
    CHECK(!HasPendingCatAction() && g_mouse.PopupOpen());
    PointAtOption(2); Click(); CHECK(HasPendingCatAction());
    ProcessCatAction(g_snapshot,100); CatActionResult result;
    CHECK(TakeCatActionResult(result) && !result.hide_list && last_target==8 && actions==1 && g_open);
    Move(table->Columns[Name].WorkMinX+40,y); Frame(2); Click();
    CHECK(HasPendingCatAction() && game_clicks==0);
    ProcessCatAction(g_snapshot,200);
    CHECK(TakeCatActionResult(result) && last_kind==CatActionKind::Inspect && !result.hide_list);
    Move(table->Columns[Age].WorkMinX+15,table->OuterRect.Min.y+10); Frame(2); Click();
    CHECK(g_sort.column==Age && !g_sort.Ascending() && g_sorted_rows.front()==89);
    Click(); CHECK(g_sort.Ascending() && g_sorted_rows.front()==0);
    Move(rows->InnerRect.GetCenter().x,rows->InnerRect.GetCenter().y); Frame(2);
    g_pad_frame.scroll_y=1; Frame(20); g_pad_frame.scroll_y=0; Frame(2); CHECK(rows->Scroll.y>0);
    ImGui::SetScrollY(rows,0); Frame(3);
    g_pad_frame.scroll_x=1; Frame(140); g_pad_frame.scroll_x=0; Frame(3); CHECK(rows->Scroll.x>0);
    for (int group=0;group<3;++group) {
        Move(table->Columns[GoodMutations+group].WorkMinX+30,y); Frame(4);
        CHECK(g_trait_hover.cat==1 && g_trait_hover.group==group);
    }
    Move(20,20); Frame(20); CHECK(!g_trait_hover.cat && !g_mouse.CapturesPointer());
    const auto scroll=rows->Scroll;
    Click(); CHECK(game_clicks==1 && !HasPendingCatAction() && !HasPendingCatMove());
    CHECK(rows->Scroll.x==scroll.x && rows->Scroll.y==scroll.y);
    ImGui::SetScrollX(rows,0); Frame(3);
    Move(table->Columns[Position].WorkMinX+40,y); Frame(2);
    const auto nav_id=GImGui->NavId;
    for (int key : {11,12,13,14}) { pad.ButtonForGame(key,true,g_mouse); Frame(5); pad.ButtonForGame(key,false,g_mouse); Frame(2); }
    CHECK(!g_mouse.PopupOpen() && GImGui->NavId==nav_id && !GImGui->NavCursorVisible);
    Click(); CHECK(g_mouse.PopupOpen()); Back(); CHECK(g_open && !g_mouse.PopupOpen());
    Click(); Move(20,20); Frame(2); Click(); CHECK(g_open && !g_mouse.PopupOpen() && game_clicks==1);
    // A complete tap followed by pointer movement before rendering keeps its
    // original target and balanced button pair in the actual ImGui queue.
    Move(table->Columns[Position].WorkMinX+40,y); Frame(2);
    pad.ButtonForGame(0,true,g_mouse); pad.ButtonForGame(0,false,g_mouse); Move(20,20); Frame(6);
    CHECK(g_mouse.PopupOpen() && !io.MouseDown[0]); Back(); CHECK(!g_mouse.PopupOpen());
    const auto visible=g_snapshot;
    RoomSnapshot paused; paused.scene=visible.scene; paused.generation=visible.generation; paused.suspended=true;
    g_reset_list_scroll=false; ApplyRoomSnapshot(paused,false,0); Frame(3);
    CHECK(g_open && !g_has_house && g_snapshot.cats.size()==visible.cats.size());
    ApplyRoomSnapshot(visible,false,0); Frame(3); CHECK(g_open && g_has_house && !g_reset_list_scroll);
    Move(table->Columns[Position].WorkMinX+40,y); Frame(2); Back(); CHECK(!g_open);
    ConfigureCatActions(nullptr,nullptr); ConfigureCatMoves(nullptr,nullptr); roomcats::ui::Reset(); ImGui::DestroyContext();
    std::cout << "Gamepad pointer UI passed: coordinate clicks, location/NPC/disabled/discard/name/sort, scrolling, trait hover, outside game input, no D-pad navigation, popup back/dismissal, fast taps and House resume.\n";
}
