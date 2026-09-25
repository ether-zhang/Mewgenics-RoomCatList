#include "newborn_test_fixture.hpp"
#include "../src/screening_choices.hpp"
#include <algorithm>

namespace {
bool accepts_first=true;
std::vector<roomcats::NpcChoice> Choices(const roomcats::RoomSnapshot&,const roomcats::Cat& cat) {
    return {{8,"Discard",true,""},{0,"Locked",false,"Locked"},
        {1,"First",accepts_first && cat.id!=2,"Not accepting"},{3,"Next",cat.id!=2,"Not accepting"}};
}
}
int TestScreeningChoices() {
    using namespace roomcats; using namespace newborn_test;
    ConfigureCatActions(Choices,nullptr); accepts_first=true;
    auto s=House();
    for(int i=1;i<=5;++i) s.cats.push_back(MakeCat(i,4,40,40,10));
    s.cats[0].name=s.cats[1].name="Same name";
    ScreeningChoices memory;
    auto p=PlanNewbornCats(s,1);
    CHECK_NEWBORN(memory.Prepare(p)==0);
    CHECK_NEWBORN(Decision(p,1).choice==ReviewChoice::Npc && Decision(p,1).npc==1);
    CHECK_NEWBORN(Decision(p,2).choice==ReviewChoice::Discard); // Not the enabled discard entry at the front.
    for(auto& d:p.decisions) {
        if(d.cat.id==2) { d.choice=ReviewChoice::Keep; }
        if(d.cat.id==3) { d.choice=ReviewChoice::Location; d.alternative_location="box:999"; d.selection_label="Box"; }
        if(d.cat.id==4) { d.choice=ReviewChoice::Discard; }
        if(d.cat.id==5) { d.choice=ReviewChoice::Npc; d.npc=3; d.selection_label="Next"; }
        memory.Remember(p,d);
    }
    // A cutscene may remove or rebuild the House, but it doesn't change the save/day.
    RoomSnapshot transition; memory.Observe(transition);
    transition=s; transition.suspended=true; transition.valid=false; memory.Observe(transition);
    s.scene=9000; ++s.generation; std::reverse(s.cats.begin(),s.cats.end());
    for(auto& c:s.cats) { c.component+=10000; ++c.generation; }
    accepts_first=false; auto retry=PlanNewbornCats(s,2);
    CHECK_NEWBORN(memory.Prepare(retry)==5);
    CHECK_NEWBORN(Decision(retry,1).choice==ReviewChoice::Npc && Decision(retry,1).npc==1 && Decision(retry,1).selection_label=="First");
    CHECK_NEWBORN(Decision(retry,2).choice==ReviewChoice::Keep);
    CHECK_NEWBORN(Decision(retry,3).choice==ReviewChoice::Location && Decision(retry,3).alternative_location=="box:999");
    CHECK_NEWBORN(Decision(retry,4).choice==ReviewChoice::Discard);
    CHECK_NEWBORN(Decision(retry,5).choice==ReviewChoice::Npc && Decision(retry,5).npc==3);
    CHECK_NEWBORN(!HasPendingCatAction()); // Preferences never enqueue a delivery.
    // Unrelated cats use the new default; delivered cats cannot reappear from memory.
    s.cats.erase(std::remove_if(s.cats.begin(),s.cats.end(),[](const auto& c) { return c.id==1; }),s.cats.end());
    s.cats.push_back(MakeCat(6,4,40,40,10)); retry=PlanNewbornCats(s,3);
    CHECK_NEWBORN(memory.Prepare(retry)==4 && retry.decisions.size()==5 && Decision(retry,6).npc==3);
    // Script types remain independent, and a new day/save does not inherit old choices.
    auto adult=retry; adult.kind=ScreeningKind::Adult;
    CHECK_NEWBORN(memory.Prepare(adult)==0 && Decision(adult,2).choice==ReviewChoice::Discard && Decision(adult,3).npc==3);
    ++s.game_day; retry=PlanNewbornCats(s,4);
    CHECK_NEWBORN(memory.Prepare(retry)==0 && Decision(retry,2).choice==ReviewChoice::Discard);
    retry.decisions[0].choice=ReviewChoice::Keep; memory.Remember(retry,retry.decisions[0]);
    ++s.cat_database; retry=PlanNewbornCats(s,5);
    CHECK_NEWBORN(memory.Prepare(retry)==0);
    CHECK_NEWBORN(std::none_of(retry.decisions.begin(),retry.decisions.end(),[](const auto& d) { return d.choice==ReviewChoice::Keep; }));
    // A stale view cannot replace preferences belonging to the current save.
    memory.Remember(p,p.decisions[0]); retry=PlanNewbornCats(s,6);
    CHECK_NEWBORN(memory.Prepare(retry)==5);
    ConfigureCatActions(nullptr,nullptr);
    std::cout << "Screening choices passed: first enabled NPC, discard fallback, per-ID NPC/location/keep/discard memory, unavailable target preservation, cutscene/scene rebuild, mode and save/day isolation.\n";
    return 0;
}
