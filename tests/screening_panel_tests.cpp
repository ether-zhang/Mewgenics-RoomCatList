#include "ui_test_host.hpp"
#include "newborn_test_fixture.hpp"
namespace {
roomcats::RoomSnapshot world;
int writes=0, inspected=0;
bool accepts_primary=true, interrupt_after_delivery=false;
std::vector<int> delivered;
bool capture_text=false;
std::string rendered_text;
std::uint64_t now=10000;
roomcats::Cat* Find(std::uint64_t id) { for(auto& c:world.cats) if(c.id==id) return &c; return nullptr; }
std::vector<roomcats::NpcChoice> Choices(const roomcats::RoomSnapshot&,const roomcats::Cat&) { return {{0,"NPC",accepts_primary,"Not accepting"},{3,"Next NPC",true,""},{8,"Discard",true,""}}; }
bool CanMove(const roomcats::RoomSnapshot&,const roomcats::Cat&,const roomcats::CatLocation&,std::string&) { return true; }
bool MoveCat(const roomcats::RoomSnapshot&,const roomcats::Cat& c,const roomcats::CatLocation& room,std::string&) {
    ++writes; auto* actual=Find(c.id); actual->location_key=room.key; actual->location_label=room.label; actual->location=room.component; return true;
}
roomcats::CatActionStatus Action(const roomcats::RoomSnapshot&,roomcats::CatAction& a,std::string&) {
    if(a.kind==roomcats::CatActionKind::Inspect) ++inspected;
    else {
        ++writes; auto* c=Find(a.cat);
        if(a.kind==roomcats::CatActionKind::Mark) c->details.marker=a.marker;
        else { c->active=false; delivered.push_back(a.npc); if(interrupt_after_delivery) accepts_primary=false; }
    }
    return roomcats::CatActionStatus::Complete;
}
void Frame(int n=1) {
    using namespace roomcats;
    for(int i=0;i<n;++i) {
        if(g_prepare_screening) {
            g_prepare_screening=false;
            g_screening_plan=g_prepare_screening_kind==ScreeningKind::Adult ? PlanAdultCats(world,101,g_screening_rules.adult) : PlanNewbornCats(world,101,g_screening_rules.newborn);
            g_screening_choices.Prepare(*g_screening_plan);
            g_screening_submitted=false;
            if(!g_view.screening) { ListView v; v.screening=true; g_pending_parent_view=v; ApplyQueuedListView(); }
            else { g_view_refresh_requested=true; g_reset_list_scroll=true; g_sort_dirty=true; ClearMovePopup(); ClearTraitHover(); }
        }
        if(g_confirm_screening) {
            g_confirm_screening=false; std::string why;
            if(g_screening_plan) g_screening_submitted=BeginNewbornBatch(*g_screening_plan,world,why);
        }
        if(NewbornBatchActive()) { auto fresh=world; if(!NewbornBatchNeedsCats()) fresh.cats.clear(); TickNewbornBatch(fresh,now); }
        ProcessCatMove(world); ProcessCatAction(world,now); now+=100;
        ApplyRoomSnapshot(g_view.screening ? ScreeningSnapshot(world) : world,false,0);
        const bool popup=g_mouse.PopupOpen(); ImGui::NewFrame(); HandleGamepadBack(popup);
        if(capture_text) ImGui::LogToBuffer();
        DrawUi();
        if(capture_text) { rendered_text=GImGui->LogBuffer.c_str(); ImGui::LogFinish(); capture_text=false; }
        PublishMouseCapture(); ImGui::Render(); g_mouse.EndFrame();
    }
}
void Point(float x,float y) { g_mouse.Move(int(x),int(y)); ImGui::GetIO().AddMousePosEvent(x,y); }
void Click() {
    if(g_mouse.Down(1)==roomcats::MouseOwner::List) ImGui::GetIO().AddMouseButtonEvent(0,true);
    Frame();
    if(g_mouse.Up(1)==roomcats::MouseOwner::List) ImGui::GetIO().AddMouseButtonEvent(0,false);
    Frame(4);
}
ImGuiWindow* Panel() { return ImGui::FindWindowByName("猫咪列表###RoomCatsPanel"); }
ImGuiTable* Table() { return GImGui->Tables.GetByKey(ImHashStr(g_view.screening ? "RoomCatsScreeningRowsV1" : "RoomCatsDetailsRowsV6",0,Panel()->ID)); }
void Toolbar() { auto* p=Panel(); Point(p->WorkRect.Max.x-12,p->WorkRect.Min.y+ImGui::GetFrameHeight()*0.5f); Frame(2); Click(); }
void AdultToolbar() {
    auto* p=Panel(); const auto padding=ImGui::GetStyle().FramePadding.x*2;
    Point(p->WorkRect.Max.x-ImGui::CalcTextSize("新生猫筛选").x-padding-ImGui::GetStyle().ItemSpacing.x-12,
        p->WorkRect.Min.y+ImGui::GetFrameHeight()*0.5f); Frame(2); Click();
}
void Cell(int index) {
    auto* table=Table(); Point(table->Columns[index].WorkMinX+35,
        table->OuterRect.Min.y+ImGui::GetTextLineHeight()*2+4+ImGui::GetStyle().CellPadding.y*2+8); Frame(2); Click();
}
void Keep() {
    Cell(0); auto* menu=GImGui->OpenPopupStack.back().Window;
    Point(menu->Pos.x+ImGui::GetStyle().WindowPadding.x+25,menu->Pos.y+ImGui::GetStyle().WindowPadding.y+
        ImGui::GetTextLineHeight()*0.5f+ImGui::GetTextLineHeightWithSpacing()); Frame(2); Click();
}
}
int TestScreeningPanel() {
    using namespace roomcats; using namespace newborn_test;
    ConfigureCatActions(Choices,Action); ConfigureCatMoves(CanMove,MoveCat);
    world=House(); world.scene=9876; world.cats={MakeCat(1,0,47),MakeCat(2,4,45,51,20)};
    ImGui::CreateContext(); roomcats::ui::Apply(false); auto& io=ImGui::GetIO(); io.IniFilename=io.LogFilename=nullptr;
    io.DisplaySize={1440,900}; io.DeltaTime=1.0f/60;
    unsigned char* pixels; int w,h; io.Fonts->GetTexDataAsRGBA32(&pixels,&w,&h);
    g_open=true; Frame(4);
    Toolbar(); CHECK_NEWBORN(g_view.screening && g_screening_plan && g_snapshot.cats.size()==1 && g_snapshot.cats[0].id==2);
    CHECK_NEWBORN(writes==0 && !g_screening_submitted && Table()->ColumnsCount==CatColumnCount+2);
    CHECK_NEWBORN(ScreeningDecision(2)->choice==ReviewChoice::Npc && ScreeningDecision(2)->npc==0);
    CHECK_NEWBORN(TableColumnIndex(Name)==1 && Table()->FreezeColumnsRequest==2);
    ImGui::SetScrollX(Table()->InnerWindow,80); Frame(3);
    CHECK_NEWBORN(Table()->FreezeColumnsCount==2);
    ImGui::SetScrollX(Table()->InnerWindow,0); Frame(3);
    Keep(); CHECK_NEWBORN(ScreeningDecision(2)->choice==ReviewChoice::Keep && writes==0);
    Cell(TableColumnIndex(Position)); CHECK_NEWBORN(!g_mouse.PopupOpen() && !HasPendingCatMove() && writes==0);
    Cell(TableColumnIndex(Name)); CHECK_NEWBORN(inspected==1 && writes==0);
    // Back/cancel restores the original list and does not run the plan.
    auto* p=Panel(); Point(p->WorkRect.Min.x+20,p->WorkRect.Min.y+8); Frame(2); Click();
    CHECK_NEWBORN(!g_view.screening && !g_screening_plan && writes==0 && world.cats[1].active);
    Toolbar(); CHECK_NEWBORN(g_view.screening && ScreeningDecision(2)->choice==ReviewChoice::Keep); Keep();
    CHECK_NEWBORN(writes==0); Toolbar(); Frame(15);
    CHECK_NEWBORN(g_screening_submitted && GetNewbornBatchStatus().finished && writes==2 && Find(2)->active);
    CHECK_NEWBORN(Find(1)->details.marker=="triangle" && Find(1)->location_key!="room:Attic");
    ResetListViews(); world=House(); world.scene=9876; world.population_comfort_valid=true;
    world.locations[0].comfort_base=15; world.locations[1].comfort_base=world.locations[2].comfort_base=5;
    world.cats={MakeCat(10,4,45,51,10),MakeCat(11,4,45,51,10),MakeCat(12,4,45,51,10),MakeCat(13,4,45,51,1)};
    writes=0; Frame(4); AdultToolbar();
    CHECK_NEWBORN(g_view.screening && g_screening_plan->kind==ScreeningKind::Adult && g_snapshot.cats.size()==3 && writes==0);
    CHECK_NEWBORN(ProjectPopulation(*g_screening_plan).total==1);
    Keep(); CHECK_NEWBORN(ProjectPopulation(*g_screening_plan).total==2 && writes==0);
    Toolbar(); Frame(25);
    CHECK_NEWBORN(GetNewbornBatchStatus().finished && GetNewbornBatchStatus().kind==ScreeningKind::Adult && writes==2 && Find(10)->active && Find(13)->active);
    CHECK_NEWBORN((delivered==std::vector<int>{0,0}));
    // One NPC stops accepting after the first delivery. Recheck preserves the
    // other choices and requires a fresh confirmation, with no discard fallback.
    ResetListViews(); world.cats={MakeCat(20,4,45,51,10),MakeCat(21,4,45,51,10),MakeCat(22,4,45,51,10)};
    writes=0; delivered.clear(); interrupt_after_delivery=true;
    Frame(4); AdultToolbar(); Toolbar(); Frame(20);
    CHECK_NEWBORN(GetNewbornBatchStatus().stopped && writes==1 && !Find(20)->active && Find(21)->active);
    Toolbar(); Frame(4); // The "recheck" action retains the adult script.
    CHECK_NEWBORN(g_view.screening && !g_screening_submitted && g_screening_plan->kind==ScreeningKind::Adult && g_snapshot.cats.size()==2);
    CHECK_NEWBORN(ScreeningDecision(21)->choice==ReviewChoice::Npc && ScreeningDecision(21)->npc==0 && ScreeningChoiceLabel(*ScreeningDecision(21))=="送至 NPC");
    Toolbar(); Frame(4); CHECK_NEWBORN(!g_screening_submitted && writes==1); // Unavailable remembered choice cannot execute.
    auto resumed=world; resumed.scene=9999; ++resumed.generation;
    world={}; Frame(3); // A native scene transition clears the panel state.
    CHECK_NEWBORN(!g_open && !g_screening_plan);
    world=resumed; Frame(2); g_open=true; Frame(3); AdultToolbar();
    CHECK_NEWBORN(g_snapshot.cats.size()==2 && ScreeningDecision(21)->npc==0 && ScreeningDecision(22)->npc==0 && writes==1);
    Keep();
    g_open=false; Frame(2); g_open=true; Frame(3); AdultToolbar();
    CHECK_NEWBORN(ScreeningDecision(21)->choice==ReviewChoice::Keep && ScreeningDecision(22)->npc==0 && writes==1);
    // A bad mutation with passing combat stats produces an empty discard
    // table, but its pending room move must be visible before confirmation.
    ResetListViews(); writes=0; interrupt_after_delivery=false; accepts_primary=true;
    world.cats={MakeCat(30,0,49,54)};
    world.cats[0].details.real={9,3,7,6,10,8,11}; world.cats[0].details.bad.push_back({"Bad legs","Speed -1"});
    Frame(3); Toolbar(); capture_text=true; Frame();
    CHECK_NEWBORN(g_snapshot.cats.empty() && writes==0 && !g_screening_submitted);
    CHECK_NEWBORN(rendered_text.find("无可遗弃，存在调房：1 只猫咪。")!=std::string::npos);
    CHECK_NEWBORN(rendered_text.find("请点击右上角“确定处理”后执行调房")!=std::string::npos);
    Toolbar(); Frame(15); capture_text=true; Frame();
    CHECK_NEWBORN(Find(30)->location_key=="room:Floor1_Small" && writes==2);
    CHECK_NEWBORN(rendered_text.find("本轮处理进度见下方")!=std::string::npos && rendered_text.find("存在调房：")==std::string::npos);
    ResetListViews(); world.cats={MakeCat(31,0,49)}; Frame(3); Toolbar(); capture_text=true; Frame();
    CHECK_NEWBORN(rendered_text.find("有 1 只猫咪需要打标")!=std::string::npos && rendered_text.find("存在调房：")==std::string::npos);
    ResetListViews(); world.cats={MakeCat(32,0,49,56,10)}; Frame(3); Toolbar(); capture_text=true; Frame();
    CHECK_NEWBORN(rendered_text.find("无可遗弃，也无需调房或打标。")!=std::string::npos);
    roomcats::ui::Reset(); ImGui::DestroyContext(); ConfigureCatActions(nullptr,nullptr); ConfigureCatMoves(nullptr,nullptr);
    std::cout << "Screening UI passed: top-right button, full review table, fixed decision/name columns, keep dropdown, read-only locations, inspection, cancel with no writes and explicit confirmation.\n";
    return 0;
}
