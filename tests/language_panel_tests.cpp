#include "ui_test_host.hpp"
#include "newborn_test_fixture.hpp"
#include <cassert>
#include <cstring>
#include <iostream>
#include <stdlib.h>

namespace {
void Frame(int n=1) {
    for(int i=0;i<n;++i) {
        const bool popup=g_mouse.PopupOpen();
        ImGui::NewFrame(); HandleGamepadBack(popup); DrawUi();
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
}

int TestLanguagePanel() {
    _set_error_mode(_OUT_TO_STDERR);
    using namespace roomcats;
    SetLanguage(Language::English);
    ImGui::CreateContext(); ui::Apply(false);
    auto& io=ImGui::GetIO(); io.IniFilename=io.LogFilename=nullptr;
    io.DisplaySize={1600,1000}; io.DeltaTime=1.0f/60;
    unsigned char* pixels; int w,h; io.Fonts->GetTexDataAsRGBA32(&pixels,&w,&h);
    RegisterLanguageSettings();
    auto house=newborn_test::House(); house.cats={newborn_test::MakeCat(1,0,44,51)};
    g_snapshot=house; g_open=g_has_house=true;
    Frame(3);
    auto* panel=ImGui::FindWindowByName("Cat List###RoomCatsPanel");
    assert(panel && std::strstr(panel->Name,"Cat List"));
    assert(ClassLabel("Fighter")=="Fighter" && Tx("好突变")==std::string("Good"));
    assert(std::string(ImGui::SaveIniSettingsToMemory()).find("code=en")!=std::string::npos);

    // Activate the actual top-right combo through the list's input router.
    const auto& style=ImGui::GetStyle();
    const float item=style.FramePadding.x*2;
    const float settings=ImGui::CalcTextSize(Tx("筛选设置")).x+item+style.ItemSpacing.x;
    const float actions=ImGui::CalcTextSize(Tx("新生猫筛选")).x+item+
        ImGui::CalcTextSize(Tx("老猫数量控制")).x+item+style.ItemSpacing.x;
    const float combo_left=panel->ContentRegionRect.Max.x-
        (118+style.ItemSpacing.x+settings+actions);
    Point(combo_left+50,panel->WorkRect.Min.y+ImGui::GetFrameHeight()*.5f);
    Frame(2); Click();
    assert(g_language_popup_drawn && g_mouse.PopupOpen());
    auto* popup=GImGui->OpenPopupStack.back().Window;
    assert(popup && popup->Active);
    Point(popup->WorkRect.Min.x+35,popup->WorkRect.Min.y+ImGui::GetTextLineHeightWithSpacing()*1.5f);
    Frame(2); Click();
    assert(Chinese() && g_language_save_requested && !g_mouse.PopupOpen());
    Frame(2);
    assert(std::string(Tx("猫咪列表"))=="猫咪列表" && ClassLabel("Fighter")=="战士");
    const auto saved=std::string(ImGui::SaveIniSettingsToMemory());
    assert(saved.find("[RoomCatLanguage][Selection]")!=std::string::npos && saved.find("code=zh-cn")!=std::string::npos);
    const float chinese_settings=ImGui::CalcTextSize(Tx("筛选设置")).x+item+style.ItemSpacing.x;
    const float chinese_actions=ImGui::CalcTextSize(Tx("新生猫筛选")).x+item+
        ImGui::CalcTextSize(Tx("老猫数量控制")).x+item+style.ItemSpacing.x;
    const float chinese_left=panel->ContentRegionRect.Max.x-
        (118+style.ItemSpacing.x+chinese_settings+chinese_actions);
    Point(chinese_left+50,panel->WorkRect.Min.y+ImGui::GetFrameHeight()*.5f);
    Frame(2); Click();
    assert(g_language_popup_drawn && g_mouse.PopupOpen());
    g_pad_frame.back_pressed=true; Frame(2);
    assert(!g_mouse.PopupOpen() && g_open && Chinese()); // Controller B closes the language menu first.

    // Keep a manual review destination and its rank/identity while the pure
    // plan text changes language. No native move or delivery is queued.
    auto plan=PlanNewbornCats(house,17);
    assert(plan.Valid() && newborn_test::Decision(plan,1).discard);
    g_screening_choices.Prepare(plan);
    auto& choice=plan.decisions.front();
    choice.choice=ReviewChoice::Location;
    choice.alternative_location="room:Floor1_Small";
    choice.selection_label="1F right";
    g_screening_choices.Remember(plan,choice);
    g_screening_plan=std::move(plan); g_view.screening=true;
    const auto before=ScreeningFingerprint(g_screening_plan->source,ScreeningKind::Newborn);
    SwitchUiLanguage(Language::English);
    assert(!Chinese() && g_screening_plan && g_screening_plan->Valid());
    const auto& after=g_screening_plan->decisions.front();
    assert(after.choice==ReviewChoice::Location && after.alternative_location=="room:Floor1_Small");
    assert(ScreeningFingerprint(g_screening_plan->source,ScreeningKind::Newborn)==before);
    assert(after.reason.find("First-floor")!=std::string::npos || after.reason.find("Attic")!=std::string::npos);
    assert(!HasPendingCatMove() && !HasPendingCatAction());

    ui::Reset(); ImGui::DestroyContext();
    SetLanguage(Language::English);
    ImGui::CreateContext(); ImGui::GetIO().IniFilename=ImGui::GetIO().LogFilename=nullptr;
    RegisterLanguageSettings(); ImGui::LoadIniSettingsFromMemory(saved.c_str(),saved.size());
    assert(Chinese());
    ImGui::LoadIniSettingsFromMemory("[Window][###RoomCatsPanel]\nPos=20,20\n");
    assert(!Chinese()); // Existing installations without a language entry default to English.
    ImGui::DestroyContext();
    g_screening_plan.reset(); g_view={}; g_open=false;
    std::cout << "Language UI passed: English default, combo mouse capture, Chinese selection, persisted preference, bilingual class labels, preserved review choices and language-independent fingerprints.\n";
    return 0;
}
