#include "ui_test_host.hpp"
#include "newborn_test_fixture.hpp"
#include <cstring>

namespace {
roomcats::RoomSnapshot world;
void Frame(int n=1) {
    for(int i=0;i<n;++i) {
        ApplyRoomSnapshot(world,false,0);
        const bool popup=g_mouse.PopupOpen(); ImGui::NewFrame(); HandleGamepadBack(popup); DrawUi();
        PublishMouseCapture(); ImGui::Render(); g_mouse.EndFrame();
    }
}
void Point(float x,float y) { g_mouse.Move(int(x),int(y)); ImGui::GetIO().AddMousePosEvent(x,y); }
void Click() {
    if(g_mouse.Down(1)==roomcats::MouseOwner::List) ImGui::GetIO().AddMouseButtonEvent(0,true);
    Frame();
    if(g_mouse.Up(1)==roomcats::MouseOwner::List) ImGui::GetIO().AddMouseButtonEvent(0,false);
    Frame(3);
}
ImGuiWindow* Modal() { return ImGui::FindWindowByName("筛选设置###ScreeningSettings"); }
void Open() {
    auto* panel=ImGui::FindWindowByName("猫咪列表###RoomCatsPanel"); const auto& style=ImGui::GetStyle();
    const auto button=[&](const char* label) { return ImGui::CalcTextSize(label).x+style.FramePadding.x*2; };
    Point(panel->WorkRect.Max.x-button("新生猫筛选")-button("老猫数量控制")-style.ItemSpacing.x*2-button("筛选设置")*.5f,
        panel->WorkRect.Min.y+ImGui::GetFrameHeight()*.5f); Frame(2); Click();
}
void Footer(bool save) {
    const auto& style=ImGui::GetStyle(); auto* modal=Modal();
    const auto restore=ImGui::CalcTextSize("恢复默认").x+style.FramePadding.x*2;
    const auto apply=ImGui::CalcTextSize("保存并关闭").x+style.FramePadding.x*2;
    const float x=modal->WorkRect.Min.x+restore+style.ItemSpacing.x+(save ? apply*.5f : apply+style.ItemSpacing.x+10);
    Point(x,modal->DC.CursorMaxPos.y-ImGui::GetFrameHeight()*.5f); Frame(2); Click();
}
std::vector<roomcats::NpcChoice> Choices(const roomcats::RoomSnapshot&,const roomcats::Cat&) { return {{8,"Discard",true,""}}; }
roomcats::CatActionStatus Action(const roomcats::RoomSnapshot&,roomcats::CatAction&,std::string&) { return roomcats::CatActionStatus::Complete; }
}
int TestScreeningSettingsPanel() {
    using namespace roomcats; using namespace newborn_test;
    world=House(); world.cats={MakeCat(1,0,49)};
    ImGui::CreateContext(); roomcats::ui::Apply(false); auto& io=ImGui::GetIO(); io.IniFilename=io.LogFilename=nullptr;
    io.DisplaySize={1440,900}; io.DeltaTime=1.0f/60;
    unsigned char* pixels; int w,h; io.Fonts->GetTexDataAsRGBA32(&pixels,&w,&h);
    RegisterScreeningRulesSettings(); g_open=true; Frame(3); Open();
    CHECK_NEWBORN(g_rules_popup_drawn && g_mouse.PopupOpen());
    ImGuiWindow* child=nullptr;
    for(auto* window:GImGui->Windows) if(window->ParentWindow==Modal() && std::strstr(window->Name,"RuleFields")) child=window;
    CHECK_NEWBORN(child);
    Point(child->WorkRect.Max.x-140,child->WorkRect.Min.y+ImGui::GetFrameHeight()*.5f); Frame(2); Click();
    io.AddKeyEvent(ImGuiMod_Ctrl,true); io.AddKeyEvent(ImGuiKey_A,true); Frame();
    io.AddKeyEvent(ImGuiKey_A,false); io.AddKeyEvent(ImGuiMod_Ctrl,false); Frame();
    io.AddInputCharactersUTF8("50"); Frame(2);
    CHECK_NEWBORN(g_rules_draft.newborn.breeding.genetic_min[0]==50 && g_screening_rules.newborn.breeding.genetic_min[0]==48);
    Point(0,0); CHECK_NEWBORN(g_mouse.CapturesPointer());
    Footer(true);
    CHECK_NEWBORN(!g_mouse.PopupOpen() && g_screening_rules.newborn.breeding.genetic_min[0]==50 && g_rules_save_requested);
    CHECK_NEWBORN(!HasPendingCatAction() && !HasPendingCatMove());
    // Save/load uses the actual ImGui handler, alongside window geometry.
    const auto saved=std::string(ImGui::SaveIniSettingsToMemory());
    CHECK_NEWBORN(saved.find("[RoomCatScreening][Rules]")!=std::string::npos && saved.find("newborn.genetic_attic=50")!=std::string::npos);
    g_screening_rules={}; ImGui::LoadIniSettingsFromMemory(saved.c_str(),saved.size());
    CHECK_NEWBORN(g_screening_rules.newborn.breeding.genetic_min[0]==50);
    Open(); g_rules_draft.adult.population_target=80; Footer(false);
    CHECK_NEWBORN(!g_mouse.PopupOpen() && g_screening_rules.adult.population_target==90);
    Open(); g_rules_draft.newborn.second_limit=100; Footer(true);
    CHECK_NEWBORN(g_mouse.PopupOpen() && g_screening_rules.newborn.second_limit==3);
    g_pad_frame.back_pressed=true; Frame(2);
    CHECK_NEWBORN(!g_mouse.PopupOpen() && g_open); // Back cancels settings, not the main list.
    // Settings changes invalidate a review, retaining its script type. No
    // old plan can be confirmed using new labels and different parameters.
    world.population_comfort_valid=true;
    g_screening_plan=PlanAdultCats(world,1); CHECK_NEWBORN(g_screening_plan->Valid());
    g_view.screening=true;
    auto unrelated=g_screening_rules; ++unrelated.newborn.attic_limit;
    CHECK_NEWBORN(ApplyScreeningRules(unrelated) && g_screening_plan && !g_prepare_screening);
    g_view.screening=true; g_confirm_screening=true;
    auto edited=g_screening_rules; edited.adult.population_target=80;
    CHECK_NEWBORN(ApplyScreeningRules(edited));
    CHECK_NEWBORN(!g_screening_plan && !g_confirm_screening && g_prepare_screening && g_prepare_screening_kind==ScreeningKind::Adult);
    ResetListViews(); ConfigureCatActions(Choices,Action);
    auto plan=PlanNewbornCats(world,1); std::string reason;
    CHECK_NEWBORN(BeginNewbornBatch(plan,world,reason));
    edited.adult.population_target=70;
    CHECK_NEWBORN(!ApplyScreeningRules(edited) && g_screening_rules.adult.population_target==80);
    CHECK_NEWBORN(GetNewbornBatchStatus().rules_fingerprint==RuleFingerprint(plan.newborn_rules));
    StopNewbornBatch(); ConfigureCatActions(nullptr,nullptr);
    roomcats::ui::Reset(); ImGui::DestroyContext();
    g_screening_rules={}; ImGui::CreateContext(); roomcats::ui::Apply(false); ImGui::GetIO().IniFilename=ImGui::GetIO().LogFilename=nullptr;
    RegisterScreeningRulesSettings(); ImGui::LoadIniSettingsFromMemory(saved.c_str(),saved.size());
    CHECK_NEWBORN(g_screening_rules.newborn.breeding.genetic_min[0]==50);
    roomcats::ui::Reset(); ImGui::DestroyContext();
    std::cout << "Screening settings UI passed: editable keyboard fields, modal mouse capture, explicit save/cancel, validation, ImGui persistence, back handling, stale-review invalidation and running-batch lock.\n";
    return 0;
}
