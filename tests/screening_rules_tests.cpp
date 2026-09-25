#include "newborn_test_fixture.hpp"
#include "../src/adult_plan.hpp"

int TestScreeningRules() {
    using namespace roomcats; using namespace newborn_test;
    ScreeningRules settings; std::string error;
    settings.newborn.attic_limit=1; settings.newborn.second_limit=2;
    settings.newborn.promote=false; settings.newborn.mark_birth=false;
    auto s=House(); for(int i=0;i<8;++i) s.cats.push_back(MakeCat(i+1,0,49));
    auto p=PlanNewbornCats(s,13,settings.newborn);
    CHECK_NEWBORN(p.Valid()); int counts[5]{};
    for(const auto& d:p.decisions) { ++counts[Room(d)]; CHECK_NEWBORN(d.marker.empty()); }
    CHECK_NEWBORN(counts[0]==1 && counts[1]==2 && counts[2]==2 && counts[3]==3);
    settings.newborn.second_limit=settings.newborn.second_cross_target=0;
    p=PlanNewbornCats(s,13,settings.newborn); CHECK_NEWBORN(p.Valid());
    for(const auto& d:p.decisions) CHECK_NEWBORN(Room(d)==0 || Room(d)==3);
    settings.newborn.second_limit=7; CHECK_NEWBORN(!PlanNewbornCats(s,13,settings.newborn).Valid());
    settings={}; settings.newborn.attic_limit=1;
    s=House(); s.cats={MakeCat(1,0,48,56,1,0,6),MakeCat(2,0,49,56,1,0,7)};
    settings.newborn.breeding.preferred_dex=6;
    CHECK_NEWBORN(Room(Decision(PlanNewbornCats(s,1,settings.newborn),1))==0);
    settings.newborn.breeding.prioritize_dex=false;
    CHECK_NEWBORN(Room(Decision(PlanNewbornCats(s,1,settings.newborn),2))==0);
    s=House(); s.cats={MakeCat(1,1,49)}; settings.newborn.promote=false;
    CHECK_NEWBORN(Room(Decision(PlanNewbornCats(s,1,settings.newborn),1))!=0);
    settings.newborn.promote=true; settings.newborn.promotion_limit=0;
    CHECK_NEWBORN(Room(Decision(PlanNewbornCats(s,1,settings.newborn),1))!=0);
    settings.newborn.promotion_limit=1;
    CHECK_NEWBORN(Room(Decision(PlanNewbornCats(s,1,settings.newborn),1))==0);
    // Health toggles are independent and don't change the fixed house protection.
    s=House(); s.cats={MakeCat(1,0,49)}; s.cats[0].details.bad.push_back({"Bad",""});
    settings.newborn.breeding.reject_bad=false;
    CHECK_NEWBORN(Room(Decision(PlanNewbornCats(s,1,settings.newborn),1))==0);
    s.cats[0].details.diseases.push_back({"Sick",""});
    CHECK_NEWBORN(Room(Decision(PlanNewbornCats(s,1,settings.newborn),1))==4);
    settings.newborn.breeding.reject_disease=false;
    CHECK_NEWBORN(Room(Decision(PlanNewbornCats(s,1,settings.newborn),1))==0);
    s.cats[0].details.inbreeding=.25; settings.newborn.split_parents=false;
    CHECK_NEWBORN(PlanNewbornCats(s,1,settings.newborn).Valid()); // No pedigree needed when splitting is off.
    s=House(); s.cats={MakeCat(10,3,44,80,10),MakeCat(11,3,45,80,10),MakeCat(1,3,48)};
    s.cats[0].details.collar="Mage"; s.cats[2].details.inbreeding=.25; s.cats[2].parents={true,{10,11},{"Dad","Mom"}};
    p=PlanNewbornCats(s,1);
    CHECK_NEWBORN(Decision(p,10).discard && Room(Decision(p,10))==3);
    settings={}; settings.newborn.class_parent_no_battle=false;
    p=PlanNewbornCats(s,1,settings.newborn); CHECK_NEWBORN(!Decision(p,10).discard && Room(Decision(p,10))==4);
    s.cats[0]=MakeCat(10,3,45,80,10); s.cats[1].details.collar="Mage";
    p=PlanNewbornCats(s,1);
    CHECK_NEWBORN(Room(Decision(p,10))==4 && Room(Decision(p,11))==3 && !Decision(p,11).discard);
    // Class is only the third breeding tie break, including room-capacity culls.
    s=House(); s.population_comfort_valid=true;
    s.locations[0].comfort_base=15; s.locations[1].comfort_base=s.locations[2].comfort_base=5;
    for(int i=0;i<3;++i) { auto c=MakeCat(10+i,0,49,56,10); c.details.marker="appeal"; s.cats.push_back(c); }
    s.cats.push_back(MakeCat(1,0,49,80,10)); s.cats.push_back(MakeCat(2,0,48,80,10,10)); s.cats.back().details.collar="Mage";
    p=PlanAdultCats(s,1); CHECK_NEWBORN(Room(Decision(p,1))==0 && Room(Decision(p,2))!=0);
    s.cats[3]=MakeCat(1,0,48,80,10,11);
    p=PlanAdultCats(s,1); CHECK_NEWBORN(Room(Decision(p,1))==0 && Room(Decision(p,2))!=0);
    s.cats[3]=MakeCat(1,0,48,80,10,10);
    p=PlanAdultCats(s,1); CHECK_NEWBORN(Room(Decision(p,2))==0 && Room(Decision(p,1))!=0);
    // A rejected classed breeder is never automatically moved to battle.
    s.cats={MakeCat(1,0,44,80,10),MakeCat(2,0,49,80,10),MakeCat(3,4,49,80,10),MakeCat(4,0,44,40,10)};
    for(auto& c:s.cats) c.details.collar="Mage";
    s.cats[1].details.diseases.push_back({"Sick",""}); s.cats[3].details.marker="appeal";
    p=PlanAdultCats(s,1);
    CHECK_NEWBORN(Decision(p,1).discard && Room(Decision(p,1))==0 && Decision(p,2).discard && Room(Decision(p,2))==0);
    CHECK_NEWBORN(!Decision(p,3).discard && Room(Decision(p,3))==4 && !Decision(p,4).discard);
    settings={}; settings.adult.class_no_battle=false;
    p=PlanAdultCats(s,1,settings.adult); CHECK_NEWBORN(!Decision(p,1).discard && Room(Decision(p,1))==4);
    settings.adult.breeding.genetic_min={52,50,48};
    s.cats={MakeCat(1,0,49,80,10)}; p=PlanAdultCats(s,1,settings.adult);
    CHECK_NEWBORN(Room(Decision(p,1))==3 && p.adult_rules.breeding.genetic_min[0]==52);
    s.cats[0].details.inbreeding=.25;
    CHECK_NEWBORN(Room(Decision(PlanAdultCats(s,1,settings.adult),1))==3);
    settings.adult.allow_mild_first=false;
    CHECK_NEWBORN(Room(Decision(PlanAdultCats(s,1,settings.adult),1))==4);
    // Configured pressure and population caps affect actual plans, not just labels.
    settings={}; settings.adult.comfort_min={17,6,7,1};
    s.locations[0].comfort_base=20; s.locations[1].comfort_base=s.locations[2].comfort_base=10; s.locations[3].comfort_base=5;
    s.cats.clear(); for(int i=0;i<40;++i) s.cats.push_back(MakeCat(i+1,0,49,80,10));
    p=PlanAdultCats(s,1,settings.adult); auto projection=ProjectPopulation(p);
    CHECK_NEWBORN(p.Valid());
    for(int i=0;i<4;++i) CHECK_NEWBORN(projection.comfort[i]==settings.adult.comfort_min[i]);
    settings.adult.population_target=20; p=PlanAdultCats(s,1,settings.adult);
    CHECK_NEWBORN(ProjectPopulation(p).total==20);
    // Both scripts honor the same configurable battle comparisons.
    s.cats={MakeCat(1,4,49,56,10)}; s.cats[0].details.real={6,6,7,8,9,10,10};
    settings.adult.battle={54,true,6,3}; settings.newborn.battle=settings.adult.battle;
    CHECK_NEWBORN(!Decision(PlanAdultCats(s,1,settings.adult),1).discard && !Decision(PlanNewbornCats(s,1,settings.newborn),1).discard);
    settings.adult.battle.low_stat_count=2; settings.newborn.battle=settings.adult.battle;
    CHECK_NEWBORN(Decision(PlanAdultCats(s,1,settings.adult),1).discard && Decision(PlanNewbornCats(s,1,settings.newborn),1).discard);
    settings.adult.battle.reject_lopsided=false;
    CHECK_NEWBORN(!Decision(PlanAdultCats(s,1,settings.adult),1).discard);
    settings.adult.battle.real_min=60;
    CHECK_NEWBORN(Decision(PlanAdultCats(s,1,settings.adult),1).discard);
    ScreeningRules loaded;
    const auto serialized=SerializeScreeningRules(settings);
    CHECK_NEWBORN(ParseScreeningRules(serialized,loaded,error) && SerializeScreeningRules(loaded)==serialized);
    CHECK_NEWBORN(RuleFingerprint(settings.adult)==RuleFingerprint(loaded.adult));
    const auto original=SerializeScreeningRules(loaded);
    CHECK_NEWBORN(!ParseScreeningRules("adult.population_target=-1\n",loaded,error) && SerializeScreeningRules(loaded)==original);
    CHECK_NEWBORN(!ParseScreeningRules("newborn.second_limit=100\n",loaded,error));
    CHECK_NEWBORN(!ParseScreeningRules("newborn.reject_bad=2\n",loaded,error));
    CHECK_NEWBORN(!ParseScreeningRules("adult.real_min=52oops\n",loaded,error));
    CHECK_NEWBORN(!ParseScreeningRules("adult.genetic_attic=40\n",loaded,error));
    CHECK_NEWBORN(ParseScreeningRules("future.option=10\nadult.population_target=80\n",loaded,error) && loaded.adult.population_target==80);
    CHECK_NEWBORN(RuleFingerprint(loaded.adult)!=RuleFingerprint(AdultRules{}));
    std::cout << "Screening rules passed: configurable thresholds/quotas/promotions/comfort/population/conditions, third-place class tie break, no battle transfer for rejected classed breeders, bounded validation and persistence.\n";
    return 0;
}
