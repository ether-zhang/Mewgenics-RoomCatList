#include "newborn_test_fixture.hpp"
#include "../src/adult_plan.hpp"
#include <algorithm>
#include <limits>

namespace {
roomcats::RoomSnapshot ComfortableHouse() {
    auto s=newborn_test::House(); s.population_comfort_valid=true;
    s.locations[0].comfort_base=15; s.locations[1].comfort_base=s.locations[2].comfort_base=5;
    return s;
}
}
int TestAdultPlan() {
    using namespace roomcats; using namespace newborn_test;
    CHECK_NEWBORN(RoomComfort(20,4)==20 && RoomComfort(20,5)==19 && RoomComfort(20,12)==12);
    auto s=ComfortableHouse();
    for(int i=0;i<10;++i) s.cats.push_back(MakeCat(i+1,i%4,48,56,10,i==0 ? 3 : 0,i==9 ? 6 : 7));
    s.cats.push_back(MakeCat(100,0,40,40,1)); // Untouched newborn uses a room slot.
    auto p=PlanAdultCats(s,17); auto projection=ProjectPopulation(p);
    CHECK_NEWBORN(p.Valid() && p.kind==ScreeningKind::Adult && projection.counts[0]==4 && projection.comfort[0]==15);
    CHECK_NEWBORN(Room(Decision(p,1))==0 && Room(Decision(p,10))!=0 && Room(Decision(p,100))==0 && !Decision(p,100).discard);
    for(const auto& d:p.decisions) CHECK_NEWBORN(d.marker.empty());
    CHECK_NEWBORN(std::abs(projection.comfort[1]-projection.comfort[2])<=1);
    // The four genetic thresholds and inbreeding exceptions are independent.
    s=ComfortableHouse();
    s.cats={MakeCat(1,0,47,56,10),MakeCat(2,1,45,56,10),MakeCat(3,2,44,56,10),
        MakeCat(4,1,49,56,10),MakeCat(5,0,49,56,10),MakeCat(6,4,55,56,10)};
    s.cats[3].details.inbreeding=.25; s.cats[4].details.inbreeding=.2501;
    p=PlanAdultCats(s,22);
    CHECK_NEWBORN(p.Valid() && (Room(Decision(p,1))==1 || Room(Decision(p,1))==2));
    CHECK_NEWBORN(Room(Decision(p,2))==3 && Room(Decision(p,3))==4 && Room(Decision(p,4))==3 && Room(Decision(p,5))==4);
    CHECK_NEWBORN(Room(Decision(p,6))==4); // Combat cats stay assigned to combat.
    // Diseases and bad mutations never stay in a breeding room, except protection.
    s=ComfortableHouse(); s.cats={MakeCat(1,0,49,56,10),MakeCat(2,1,49,51,10),MakeCat(3,2,49,40,10),MakeCat(4,3,49,40,1)};
    for(auto& c:s.cats) c.details.diseases.push_back({"Disease",""});
    s.cats[0].details.bad.push_back({"Bad",""}); s.cats[2].details.marker="appeal";
    p=PlanAdultCats(s,23);
    CHECK_NEWBORN(Room(Decision(p,1))==4 && !Decision(p,1).discard && Decision(p,2).discard);
    CHECK_NEWBORN(Room(Decision(p,3))==2 && !Decision(p,3).discard && Room(Decision(p,4))==3 && !Decision(p,4).discard);
    // Unequal furniture capacities balance comfort, not raw head counts.
    s=ComfortableHouse(); s.locations[1].comfort_base=8;
    for(int i=0;i<14;++i) s.cats.push_back(MakeCat(i+1,1,46,56,10));
    p=PlanAdultCats(s,24); projection=ProjectPopulation(p);
    CHECK_NEWBORN(projection.counts[1]==7 && projection.counts[2]==4 && projection.comfort[1]==5 && projection.comfort[2]==5);
    CHECK_NEWBORN(projection.counts[3]==3 && projection.comfort[3]==0);
    // Comfort auras move with their cat; first four occupants cause no penalty.
    s=ComfortableHouse();
    for(int i=0;i<10;++i) s.cats.push_back(MakeCat(i+1,0,48,56,10,i));
    s.cats[0].comfort_effect=3;
    p=PlanAdultCats(s,25); projection=ProjectPopulation(p);
    CHECK_NEWBORN(projection.counts[0]==7 && projection.comfort[0]==15 && Room(Decision(p,1))==0);
    // Population includes protected cats, newborns and box occupants.
    s=ComfortableHouse();
    for(int i=0;i<94;++i) s.cats.push_back(MakeCat(i+1,4,49,60+i,10));
    s.cats[0].details.marker="appeal";
    s.cats.push_back(MakeCat(100,5,40,40,10)); s.cats.push_back(MakeCat(101,0,49,40,1));
    p=PlanAdultCats(s,26); projection=ProjectPopulation(p);
    CHECK_NEWBORN(p.Valid() && projection.total==90 && projection.protected_cats==1);
    CHECK_NEWBORN(!Decision(p,1).discard && !Decision(p,100).discard && !Decision(p,101).discard);
    for(int i=2;i<=7;++i) CHECK_NEWBORN(Decision(p,i).discard);
    CHECK_NEWBORN(!Decision(p,8).discard);
    // Combat selection must not depend on genetic potential.
    auto prior=p;
    for(auto& c:s.cats) if(c.details.age>1) c.details.genetic.fill(int(c.id%12));
    p=PlanAdultCats(s,26);
    for(const auto& d:p.decisions) CHECK_NEWBORN(d.discard==Decision(prior,d.cat.id).discard);
    // Review overrides recompute population and room pressure without writes.
    auto it=std::find_if(p.decisions.begin(),p.decisions.end(),[](const auto& d) { return d.discard; });
    it->choice=ReviewChoice::Keep; CHECK_NEWBORN(ProjectPopulation(p).total==91);
    it->choice=ReviewChoice::Location; it->alternative_location=s.locations[0].key;
    CHECK_NEWBORN(ProjectPopulation(p).total==91 && ProjectPopulation(p).counts[0]==2);
    it->choice=ReviewChoice::Npc; CHECK_NEWBORN(ProjectPopulation(p).total==90);
    // If combat removals cannot reach 90, take weaker lower-tier adults next.
    s=ComfortableHouse(); s.locations[0].comfort_base=200;
    for(int i=0;i<92;++i) s.cats.push_back(MakeCat(i+1,0,49,56,10,i%4));
    s.cats.push_back(MakeCat(100,3,45,56,10));
    p=PlanAdultCats(s,27); CHECK_NEWBORN(ProjectPopulation(p).total==90 && Decision(p,100).discard);
    // The limit never overrides house protection or the adult-only scope.
    s=ComfortableHouse();
    for(int i=0;i<92;++i) { auto c=MakeCat(i+1,4,40,40,10); c.details.marker="appeal"; s.cats.push_back(c); }
    p=PlanAdultCats(s,28); CHECK_NEWBORN(p.Valid() && ProjectPopulation(p).total==92);
    for(const auto& d:p.decisions) CHECK_NEWBORN(!d.discard && d.destination==d.cat.location_key);
    // Stale furniture, newly added boxed cats and invalid effects reject execution.
    std::string why; CHECK_NEWBORN(ValidateNewbornPlan(p,s,why));
    std::reverse(s.locations.begin(),s.locations.end()); CHECK_NEWBORN(ValidateNewbornPlan(p,s,why));
    s.locations.back().comfort_base+=1; CHECK_NEWBORN(!ValidateNewbornPlan(p,s,why));
    s=ComfortableHouse(); p=PlanAdultCats(s,1); s.cats.push_back(MakeCat(999,5,40,40,10));
    CHECK_NEWBORN(!ValidateNewbornPlan(p,s,why));
    s.cats[0].comfort_effect=std::numeric_limits<double>::quiet_NaN(); CHECK_NEWBORN(!PlanAdultCats(s,1).Valid());
    s=House(); CHECK_NEWBORN(!PlanAdultCats(s,1).Valid());
    // Mixed populations: preserve every exemption and enforce floors/thresholds.
    for(unsigned seed=0;seed<40;++seed) {
        s=ComfortableHouse(); s.locations[0].comfort_base=22; s.locations[1].comfort_base=14; s.locations[2].comfort_base=11; s.locations[3].comfort_base=15;
        for(unsigned i=0;i<110;++i) {
            auto c=MakeCat(i+1,i%5,44+(i*17+seed)%7,52+i%9,i%11 ? 10 : 1,i%3);
            if(i%13==0) c.details.marker="appeal";
            else if(i%17==0) c.details.bad.push_back({"Bad",""});
            else if(i%7==0) c.details.inbreeding=.25;
            s.cats.push_back(c);
        }
        p=PlanAdultCats(s,seed); projection=ProjectPopulation(p);
        CHECK_NEWBORN(p.Valid() && projection.total<=90);
        for(int r=0;r<4;++r) CHECK_NEWBORN(projection.comfort[r]+1e-6>=kBreedingComfortMinimum[r]);
        for(const auto& d:p.decisions) {
            const auto& c=d.cat; const auto room=Room(d);
            if(ProtectedCat(c) || c.details.age<=1) CHECK_NEWBORN(!d.discard && d.marker.empty() && d.destination==c.location_key);
            else if(!d.discard && room<4) {
                CHECK_NEWBORN(!BreedingHealthRisk(c) && c.details.inbreeding<=(room==3 ? .25 : .1));
                CHECK_NEWBORN(TotalStats(c.details.genetic)>=(room==0 ? 48 : room==3 ? 45 : 46));
            }
        }
    }
    // One attic slot: Chungus wins the quality tie over class and does not
    // trigger the disease rule. Other health risks remain independent.
    s=ComfortableHouse();
    for(int i=0;i<3;++i) { auto c=MakeCat(100+i,0,49,56,10); c.details.marker="appeal"; s.cats.push_back(c); }
    s.cats.push_back(MakeCat(1,0,48,56,10)); s.cats.push_back(MakeCat(2,0,48,56,10));
    s.cats[3].details.good.push_back(LookupDisorder("Chungus")); s.cats[4].details.collar="Mage";
    p=PlanAdultCats(s,17); CHECK_NEWBORN(p.Valid());
    CHECK_NEWBORN(Room(Decision(p,1))==0 && !Decision(p,1).discard && Room(Decision(p,2))!=0);
    s.cats[3].details.diseases.push_back(LookupDisorder("Rabies"));
    p=PlanAdultCats(s,17);
    CHECK_NEWBORN(Room(Decision(p,1))==4 && !Decision(p,1).discard && Room(Decision(p,2))==0);
    s.cats[3].details.diseases.clear(); s.cats[3].details.bad.push_back({"Bad",""});
    p=PlanAdultCats(s,17);
    CHECK_NEWBORN(Room(Decision(p,1))==4 && !Decision(p,1).discard);
    std::cout << "Adult planner passed: genetic tiers, comfort/aura capacities, balanced second floor, health/inbreeding, adult-only scope, house protection, combat ranking, total 90, Chungus exception, review overrides and stale-plan rejection.\n";
    return 0;
}
