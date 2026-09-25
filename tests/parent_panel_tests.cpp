// Shared production table and synthetic parents; no native/game/device access.
#include "ui_test_host.hpp"
#include <iostream>
#define CHECK(x) do { if (!(x)) { std::cerr << "Parent UI check failed, line " << __LINE__ << ": " << #x << "\n"; return 1; } } while(false)
namespace {
roomcats::RoomSnapshot home;
std::vector<roomcats::Cat> relatives;
int moves=0, actions=0, preview=0;
std::uint64_t operated=0;
roomcats::CatActionKind action_kind{};
std::vector<roomcats::NpcChoice> Npcs(const roomcats::RoomSnapshot&,const roomcats::Cat& cat) {
    ++preview; return {{0,"NPC",!cat.record_only,""},{8,"Discard",!cat.record_only,""}};
}
roomcats::CatActionStatus Act(const roomcats::RoomSnapshot&,roomcats::CatAction& a,std::string&) {
    ++actions; operated=a.cat; action_kind=a.kind; return roomcats::CatActionStatus::Complete;
}
bool CanMove(const roomcats::RoomSnapshot&,const roomcats::Cat&,const roomcats::CatLocation&,std::string&) { return true; }
bool DoMove(const roomcats::RoomSnapshot&,const roomcats::Cat& c,const roomcats::CatLocation& where,std::string&) {
    ++moves; operated=c.id;
    for (auto& parent:relatives) if (parent.id==c.id) {
        parent.location=where.component; parent.location_key=where.key; parent.location_label=where.label;
    }
    return true;
}
roomcats::RoomSnapshot Snapshot() {
    auto next=home;
    if (g_view.IsParents()) {
        next.room=0; next.room_id.clear(); next.cats.clear();
        for (auto id:g_view.ids) if (id)
            for (const auto& cat:relatives) if (cat.id==id) next.cats.push_back(cat);
    }
    return next;
}
void Frame(int n=1) {
    for (int i=0;i<n;++i) {
        ApplyRoomSnapshot(Snapshot(),false,0);
        const bool popup=g_mouse.PopupOpen();
        ImGui::NewFrame(); HandleGamepadBack(popup); DrawUi();
        PublishMouseCapture(); ImGui::Render(); g_mouse.EndFrame();
    }
}
void Point(float x,float y) { g_mouse.Move(int(x),int(y)); ImGui::GetIO().AddMousePosEvent(x,y); }
void Click() {
    if (g_mouse.Down(1)==roomcats::MouseOwner::List) ImGui::GetIO().AddMouseButtonEvent(0,true);
    Frame();
    if (g_mouse.Up(1)==roomcats::MouseOwner::List) ImGui::GetIO().AddMouseButtonEvent(0,false);
    Frame(4);
}
ImGuiTable* Table() {
    auto* window=ImGui::FindWindowByName("猫咪列表###RoomCatsPanel");
    return GImGui->Tables.GetByKey(ImHashStr("RoomCatsDetailsRowsV6",0,window->ID));
}
float RowY(int index=0) {
    return Table()->OuterRect.Min.y+ImGui::GetTextLineHeight()*2+4+ImGui::GetStyle().CellPadding.y*2+8+
        index*(ImGui::GetTextLineHeight()*2+ImGui::GetStyle().ItemSpacing.y+ImGui::GetStyle().CellPadding.y*2);
}
void Cell(int column,int row=0) { Point(Table()->Columns[column].WorkMinX+55,RowY(row)); Frame(2); Click(); }
void Option(int row) {
    auto* popup=GImGui->OpenPopupStack.back().Window;
    Point(popup->Pos.x+ImGui::GetStyle().WindowPadding.x+25,
          popup->Pos.y+ImGui::GetStyle().WindowPadding.y+ImGui::GetTextLineHeight()*0.5f+row*ImGui::GetTextLineHeightWithSpacing());
    Frame(2); Click();
}
}
int TestParentPanel() {
    using namespace roomcats;
    ImGui::CreateContext(); auto& io=ImGui::GetIO();
    io.IniFilename=io.LogFilename=nullptr; io.DisplaySize={1440,900}; io.DeltaTime=1.0f/60;
    unsigned char* pixels; int w,h; io.Fonts->GetTexDataAsRGBA32(&pixels,&w,&h);
    home.scene=100; home.generation=1; home.room=300; home.room_id="Floor1_Small"; home.valid=home.in_house=true;
    home.locations={{"room","Room",300,1,LocationKind::Room},{"box","Box",400,1,LocationKind::Box}};
    for (int i=0;i<90;++i) {
        Cat c; c.id=i+1; c.name="Child "+std::to_string(i); c.details.valid=true; c.details.age=i;
        c.location_key="room"; c.location_label="Room";
        c.parents={true,{201,202},{"Parent live","Parent historical"}}; home.cats.push_back(c);
    }
    Cat live; live.id=201; live.name="Parent live"; live.component=2000; live.generation=1; live.location=300;
    live.location_key="room"; live.location_label="Room"; live.details.valid=true; live.details.age=100;
    live.details.collar="Mage"; live.details.real.fill(7); live.details.genetic.fill(5);
    live.details.good={{"Mutation","An effect"}}; live.details.diseases={{"Disease","An effect"}};
    live.parents={true,{301,0},{"Grandparent",""}};
    Cat historical=live; historical.id=202; historical.name="Parent historical";
    historical.component=0; historical.record_only=true; historical.location=0; historical.location_key.clear(); historical.location_label="已故";
    historical.details.age=110; historical.parents={};
    Cat ancestor=historical; ancestor.id=301; ancestor.name="Grandparent"; ancestor.parents={};
    relatives={live,historical,ancestor};
    ConfigureCatMoves(CanMove,DoMove); ConfigureCatActions(Npcs,Act);
    g_open=true; Frame(4);
    g_sort.Click(Age); g_sort_dirty=true; Frame(3);
    const float row_height=ImGui::GetTextLineHeight()*2+ImGui::GetStyle().ItemSpacing.y+ImGui::GetStyle().CellPadding.y*2;
    ImGui::SetScrollY(Table()->InnerWindow,row_height*5); ImGui::SetScrollX(Table()->InnerWindow,20); Frame(4);
    const auto original_scroll=Table()->InnerWindow->Scroll;
    Cell(Parents);
    CHECK(g_view.IsParents() && g_view_history.size()==1 && g_snapshot.cats.size()==2);
    CHECK(g_snapshot.cats[0].id==201 && g_snapshot.cats[1].id==202 && !g_snapshot.room);
    CHECK(g_sort.column==-1 && Table()->ColumnsCount==CatColumnCount);
    Cell(Position); CHECK(g_mouse.PopupOpen() && !HasPendingCatMove());
    Option(1); CHECK(HasPendingCatMove() && moves==0);
    ProcessCatMove(g_snapshot); MoveResult moved;
    CHECK(TakeCatMoveResult(moved) && moved.success && moves==1 && operated==201);
    Frame(3); CHECK(g_view.IsParents() && g_snapshot.cats[0].location_key=="box");
    Cell(SendTo); CHECK(g_mouse.PopupOpen()); Option(0); CHECK(HasPendingCatAction());
    ProcessCatAction(g_snapshot,100); CatActionResult result;
    CHECK(TakeCatActionResult(result) && !result.hide_list && operated==201 && action_kind==CatActionKind::Donate);
    Frame(3); Cell(Name); CHECK(HasPendingCatAction());
    ProcessCatAction(g_snapshot,200); CHECK(TakeCatActionResult(result) && operated==201 && action_kind==CatActionKind::Inspect && g_open);
    const int previous_actions=actions, previous_previews=preview;
    Cell(Position,1); CHECK(!g_mouse.PopupOpen() && !HasPendingCatMove());
    Cell(SendTo,1); CHECK(!g_mouse.PopupOpen() && !HasPendingCatAction());
    Cell(Name,1); CHECK(actions==previous_actions && preview==previous_previews && !HasPendingCatAction());
    CHECK(!QueueCatMove(g_snapshot,202,"room") && !QueueCatAction(g_snapshot,202,CatActionKind::Inspect));
    CHECK(CatNpcChoices(g_snapshot,202).empty());
    for (const auto& choice:CatMoveChoices(g_snapshot,202)) CHECK(!choice.enabled);
    Cell(Parents); CHECK(g_view_history.size()==2 && g_snapshot.cats.size()==1 && g_snapshot.cats[0].id==301);
    g_pad_frame.back_pressed=true; Frame(4); CHECK(g_view_history.size()==1 && g_snapshot.cats.size()==2);
    Point(Table()->Columns[Age].WorkMinX+10,Table()->OuterRect.Min.y+10); Frame(2); Click();
    CHECK(g_sort.column==Age && g_sorted_rows.front()==1);
    auto* panel=ImGui::FindWindowByName("猫咪列表###RoomCatsPanel");
    Point(panel->Pos.x+ImGui::GetStyle().WindowPadding.x+25,
          panel->Pos.y+panel->TitleBarHeight+ImGui::GetStyle().WindowPadding.y+8); Frame(2); Click();
    CHECK(!g_view.IsParents() && g_view_history.empty() && g_snapshot.cats.size()==90);
    CHECK(g_sort.column==Age && !g_sort.Ascending() && g_sorted_rows.front()==89);
    CHECK(Table()->InnerWindow->Scroll.x==original_scroll.x && Table()->InnerWindow->Scroll.y==original_scroll.y);
    Cell(Parents); CHECK(g_view.IsParents());
    auto paused=Snapshot(); paused.in_house=false; paused.suspended=true;
    ApplyRoomSnapshot(paused,false,0); CHECK(g_open && !g_has_house && g_view.IsParents());
    Frame(3); CHECK(g_open && g_has_house && g_view.IsParents());
    ++home.generation; Frame(3); CHECK(!g_open && !g_view.IsParents() && g_view_history.empty());
    ConfigureCatMoves(nullptr,nullptr); ConfigureCatActions(nullptr,nullptr); ImGui::DestroyContext();
    std::cout << "Parent table passed: independent ID scope, shared columns/actions, historical rows disabled, ancestor/back navigation, sort/XY-scroll restoration, pause/resume and scene reset.\n";
    return 0;
}
