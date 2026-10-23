#include "ui_test_host.hpp"
#include "newborn_test_fixture.hpp"
#include <set>

namespace {
int queries=0;
bool accepts=true;
std::vector<roomcats::NpcChoice> Choices(const roomcats::RoomSnapshot&,const roomcats::Cat&) {
    ++queries; return {{0,"NPC",accepts,"Unavailable"},{8,"Discard",true,""}};
}
roomcats::CatActionStatus Action(const roomcats::RoomSnapshot&,roomcats::CatAction&,std::string&) { return roomcats::CatActionStatus::Failed; }
void Frame(int count=1) {
    for(int i=0;i<count;++i) { ImGui::NewFrame(); DrawUi(); PublishMouseCapture(); ImGui::Render(); g_mouse.EndFrame(); }
}
}

int TestDisplayPanel() {
    using namespace roomcats; using namespace newborn_test;
    ImGui::CreateContext(); roomcats::ui::Apply(false);
    auto& io=ImGui::GetIO(); io.IniFilename=io.LogFilename=nullptr; io.DisplaySize={1600,1000}; io.DeltaTime=1.0f/60;
    unsigned char* pixels; int width,height; io.Fonts->GetTexDataAsRGBA32(&pixels,&width,&height);
    RegisterDisplaySettings(); RegisterScreeningRulesSettings();
    g_snapshot=House(); g_has_house=g_open=true;
    for(int i=0;i<180;++i) {
        auto c=MakeCat(i+1,i%5,48,51);
        c.details.age=i+1; c.details.collar=i%2 ? "Mage" : "Fighter";
        c.details.marker=i%3==0 ? "triangle" : i%3==1 ? "square" : "circle";
        g_snapshot.cats.push_back(c);
    }
    Frame(3);
    auto* panel=ImGui::FindWindowByName("猫咪列表###RoomCatsPanel"); CHECK_NEWBORN(panel);
    const float original_font=panel->CalcFontSize();
    ImGui::ActivateItemByID(ImHashStr("###RoomCatGroupButton",0,panel->ID)); Frame(2);
    CHECK_NEWBORN(g_display_popup_drawn && g_mouse.PopupOpen() && !GImGui->OpenPopupStack.empty());
    auto* group_menu=GImGui->OpenPopupStack.back().Window; CHECK_NEWBORN(group_menu);
    const auto& style=ImGui::GetStyle();
    const auto button_width=[&](const char* text) { return ImGui::CalcTextSize(text).x+style.FramePadding.x*2; };
    const float group_width=button_width(CatGroupButtonLabel().c_str());
    const float toolbar_width=group_width+LanguageSelectorWidth()+button_width(Tx("筛选设置"))+
        button_width(Tx("大猫数量控制"))+button_width(Tx("新生猫筛选"))+style.ItemSpacing.x*4;
    CHECK_NEWBORN(group_menu->Pos.y>=panel->WorkRect.Min.y+ImGui::GetFrameHeight()-1);
    CHECK_NEWBORN(std::abs(group_menu->Pos.x-(panel->ContentRegionRect.Max.x-toolbar_width))<=1);
    CHECK_NEWBORN(group_menu->Size.x+1>=group_width);
    ImGui::ActivateItemByID(ImHashStr(roomcats::Tx("按位置"),0,group_menu->ID)); Frame(3);
    CHECK_NEWBORN(g_group==CatGroup::Location && !g_mouse.PopupOpen());
    SetCatGroup(CatGroup::None); Frame(2);
    // Exercise the actual footer buttons through the mouse input router.
    auto click_font=[&](bool increase) {
        const float button_width=ListFontButtonWidth(),height=ImGui::GetFrameHeight();
        const float x=panel->WorkRect.Max.x-(increase ? button_width*.5f : ListFontControlsWidth()-button_width*.5f);
        const float y=panel->DC.CursorMaxPos.y-height*.5f;
        g_mouse.Move(int(x),int(y)); io.AddMousePosEvent(x,y); Frame(2);
        if(g_mouse.Down(1)==MouseOwner::List) io.AddMouseButtonEvent(0,true);
        Frame();
        if(g_mouse.Up(1)==MouseOwner::List) io.AddMouseButtonEvent(0,false);
        Frame(3);
    };
    click_font(false); CHECK_NEWBORN(g_font_percent==95 && g_display_save_requested);
    click_font(true); CHECK_NEWBORN(g_font_percent==100);
    SetListFontPercent(150); Frame(3);
    CHECK_NEWBORN(std::abs(panel->CalcFontSize()-original_font*1.5f)<.01f);
    auto* table=GImGui->Tables.GetByKey(ImHashStr("RoomCatsDetailsRowsV6Font150",0,panel->ID));
    CHECK_NEWBORN(table && table->Columns[Strength].WidthGiven>kCatColumnWidths[Strength]*1.4f);
    g_sort.Click(Age);
    for(const auto sample:{std::pair{CatGroup::Location,5},std::pair{CatGroup::Class,2},std::pair{CatGroup::Marker,3}}) {
        SetCatGroup(sample.first); Frame(3);
        CHECK_NEWBORN(g_table_rows.size()==180+sample.second);
        int count=0,previous=10000; std::set<std::uint64_t> ids;
        for(const auto& row:g_table_rows) {
            if(row.cat<0) { count+=row.count; previous=10000; continue; }
            const auto& cat=g_snapshot.cats[row.cat];
            CHECK_NEWBORN(cat.details.age<=previous); previous=cat.details.age; ids.insert(cat.id);
        }
        CHECK_NEWBORN(count==180 && ids.size()==180);
    }
    const auto saved=std::string(ImGui::SaveIniSettingsToMemory());
    CHECK_NEWBORN(saved.find("font_percent=150")!=std::string::npos && saved.find("group=3")!=std::string::npos);
    g_font_percent=100; g_group=CatGroup::None;
    ImGui::LoadIniSettingsFromMemory(saved.c_str(),saved.size());
    CHECK_NEWBORN(g_font_percent==150 && g_group==CatGroup::Marker);
    ImGui::LoadIniSettingsFromMemory("[RoomCatDisplay][Preferences]\nfont_percent=9999\ngroup=900\n");
    CHECK_NEWBORN(g_font_percent==100 && g_group==CatGroup::None);
    ImGui::LoadIniSettingsFromMemory("[RoomCatDisplay][Preferences]\nfont_percent=75\ngroup=2\n"); Frame(3);
    CHECK_NEWBORN(g_font_percent==75 && g_group==CatGroup::Class);
    CHECK_NEWBORN(std::abs(panel->CalcFontSize()-original_font*.75f)<.01f);
    CHECK_NEWBORN(std::string(ImGui::SaveIniSettingsToMemory()).find("font_percent=75")!=std::string::npos);
    SetListFontPercent(10); CHECK_NEWBORN(g_font_percent==75);
    SetListFontPercent(200); CHECK_NEWBORN(g_font_percent==150);
    ImGui::LoadIniSettingsFromMemory("[RoomCatDisplay][Preferences]\nfont_percent=74\n");
    CHECK_NEWBORN(g_font_percent==100);
    SetListFontPercent(150); Frame(3);
    // Modal font scaling is absolute, not compounded with its parent.
    ImGui::NewFrame(); ImGui::Begin(roomcats::Tx("猫咪列表###RoomCatsPanel"),nullptr,ImGuiWindowFlags_NoCollapse);
    OpenScreeningSettings(); ImGui::End(); DrawUi(); ImGui::Render(); Frame(2);
    auto* modal=ImGui::FindWindowByName("筛选设置###ScreeningSettings");
    CHECK_NEWBORN(modal && std::abs(modal->CalcFontSize()-original_font*1.5f)<.01f);
    g_close_move_popup=true; Frame(2);
    SetListFontPercent(100); SetCatGroup(CatGroup::None); g_sort={}; g_sort_dirty=true;
    // The closed NPC preview should not query every visible cat every frame.
    ConfigureCatActions(Choices,Action);
    g_snapshot=House(); for(int i=0;i<180;++i) g_snapshot.cats.push_back(MakeCat(i+1,4,48,51));
    g_screening_plan=PlanNewbornCats(g_snapshot,1);
    for(auto& d:g_screening_plan->decisions) { d.choice=ReviewChoice::Npc; d.npc=0; }
    g_view.screening=true; g_npc_previews.clear(); g_sort_dirty=true; Frame(3);
    const int before=queries; Frame(5); CHECK_NEWBORN(queries==before);
    accepts=false;
    CHECK_NEWBORN(!QueueCatAction(g_snapshot,1,CatActionKind::Donate,0)); // Action revalidates despite cached preview.
    Frame(20); CHECK_NEWBORN(queries>before && !g_npc_previews.begin()->second.choices[0].enabled);
    ConfigureCatActions(nullptr,nullptr); ResetListViews(); roomcats::ui::Reset(); ImGui::DestroyContext();
    std::cout << "Display UI passed: 180 cats, stable groups and group sorting, footer +/- mouse clicks, 75-150% text and columns, modal scale, persistence, bounded preferences and cached NPC previews with live action validation.\n";
    return 0;
}
