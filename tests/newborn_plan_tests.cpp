#include "newborn_test_fixture.hpp"
#include <algorithm>
int TestNewbornPlan() {
    using namespace roomcats; using namespace newborn_test;
    auto s=House();
    s.cats={MakeCat(1,0,48,56,1,0,7),MakeCat(2,0,49,56,1,0,6),MakeCat(3,0,48,56,1,2,7),MakeCat(4,0,47),MakeCat(5,0,46)};
    const auto fingerprint=NurseryFingerprint(s); auto p=PlanNewbornCats(s,17);
    CHECK_NEWBORN(p.Valid() && p.newborns==5 && NurseryFingerprint(s)==fingerprint);
    CHECK_NEWBORN(Room(Decision(p,1))==0 && Room(Decision(p,3))==0 && Room(Decision(p,2))!=0);
    for(const auto& d:p.decisions) CHECK_NEWBORN(d.marker=="triangle");
    int attic=0,left=0,right=0;
    for(const auto& d:p.decisions) { attic+=Room(d)==0; left+=Room(d)==1; right+=Room(d)==2; }
    CHECK_NEWBORN(attic==2 && std::abs(left-right)<=1);
    // Each second-floor room keeps 2 local + 1 opposite-room kittens when feasible.
    s=House(); for(int i=0;i<6;++i) s.cats.push_back(MakeCat(i+1,i<3 ? 1 : 2,46));
    p=PlanNewbornCats(s,19); int local[2]{},cross[2]{};
    for(const auto& d:p.decisions) { const int side=Room(d)-1; CHECK_NEWBORN(side==0 || side==1); (int(NurseryOf(d.cat.location_key))==Room(d) ? local[side] : cross[side])++; }
    CHECK_NEWBORN(local[0]==2 && local[1]==2 && cross[0]==1 && cross[1]==1);
    // Six places are shared with arrivals; weak seventh/eighth kittens drop.
    s.cats.push_back(MakeCat(7,1,45)); s.cats.push_back(MakeCat(8,2,44)); p=PlanNewbornCats(s,19);
    CHECK_NEWBORN(Room(Decision(p,7))==3 && Room(Decision(p,8))==4);
    // Equal genetics: quality mutation count, then class, then seeded random.
    s=House(); s.cats={MakeCat(1,0,48,56,1,1),MakeCat(2,0,48,56,1,2),MakeCat(3,0,48,56,1,1)};
    s.cats[2].details.collar="Mage"; p=PlanNewbornCats(s,23);
    CHECK_NEWBORN(Room(Decision(p,2))==0 && Room(Decision(p,3))==0 && Room(Decision(p,1))!=0);
    const auto repeat=PlanNewbornCats(s,23);
    for(std::size_t i=0;i<p.decisions.size();++i) CHECK_NEWBORN(p.decisions[i].destination==repeat.decisions[i].destination);
    // Promotions: <=2 per original floor, and only one tier per run.
    s=House(); s.cats={MakeCat(1,1,49),MakeCat(2,2,49),MakeCat(3,1,48),MakeCat(4,3,49),MakeCat(5,3,47),MakeCat(6,3,46)};
    p=PlanNewbornCats(s,29); CHECK_NEWBORN(p.Valid());
    CHECK_NEWBORN(Room(Decision(p,1))==0 && Room(Decision(p,2))==0 && Room(Decision(p,3))!=0);
    CHECK_NEWBORN(Room(Decision(p,4))>=1 && Room(Decision(p,4))<=2 && Room(Decision(p,5))>=1 && Room(Decision(p,5))<=2);
    CHECK_NEWBORN(Room(Decision(p,6))==3);
    // Same inbred pair produces two kittens: downgrade the weaker parent once.
    s=House(); s.cats={MakeCat(100,0,47,56,10,2),MakeCat(101,0,47,56,10,3),MakeCat(1,0,49,51),MakeCat(2,0,49,52)};
    for(int i=2;i<4;++i) { s.cats[i].details.inbreeding=0.25; s.cats[i].parents={true,{100,101},{"Dad","Mom"}}; }
    p=PlanNewbornCats(s,31); CHECK_NEWBORN(p.Valid());
    CHECK_NEWBORN(Room(Decision(p,100))>=1 && Room(Decision(p,100))<=2 && Room(Decision(p,101))==0);
    CHECK_NEWBORN(Decision(p,1).discard && !Decision(p,2).discard && Room(Decision(p,2))==4);
    // Lowest breeder threshold and the battle-room screen apply independently.
    s=House(); s.cats={MakeCat(1,3,45,52),MakeCat(2,3,44,52),MakeCat(3,4,49,51,20),MakeCat(4,4,49,56,20),MakeCat(5,1,30,40,20)};
    s.cats[3].details.real={5,5,10,9,9,9,9}; p=PlanNewbornCats(s,37);
    CHECK_NEWBORN(Room(Decision(p,1))==3 && Room(Decision(p,2))==4 && !Decision(p,2).discard);
    CHECK_NEWBORN(Decision(p,3).discard && Decision(p,4).discard && !Decision(p,5).discard && Room(Decision(p,5))==1);
    CHECK_NEWBORN(Decision(p,3).marker.empty());
    std::string reason; CHECK_NEWBORN(ValidateNewbornPlan(p,s,reason));
    std::reverse(s.locations.begin(),s.locations.end());
    CHECK_NEWBORN(ValidateNewbornPlan(p,s,reason)); // Component iteration order is not a room change.
    s.cats[0].details.real[0]++; CHECK_NEWBORN(!ValidateNewbornPlan(p,s,reason));
    s=House(); s.cats={MakeCat(1,0,49)}; s.cats[0].details.valid=false;
    CHECK_NEWBORN(!PlanNewbornCats(s,0).Valid());
    s=House(); s.locations.pop_back(); s.locations.pop_back(); CHECK_NEWBORN(!PlanNewbornCats(s,0).Valid());
    for(unsigned seed=0;seed<40;++seed) {
        s=House();
        for(unsigned i=0;i<45;++i) s.cats.push_back(MakeCat(i+1,i%4,44+(i*17+seed)%6,56,1,i%3));
        p=PlanNewbornCats(s,seed); CHECK_NEWBORN(p.Valid()); int counts[3]{};
        for(const auto& d:p.decisions) {
            const int room=Room(d); const auto genetics=TotalStats(d.cat.details.genetic);
            if(room<3) { ++counts[room]; CHECK_NEWBORN(genetics>=(room==0 ? 48 : 46)); }
            if(room==3) CHECK_NEWBORN(genetics>=45);
        }
        CHECK_NEWBORN(counts[0]<=2 && counts[1]<=3 && counts[2]<=3);
    }
    s=House(); s.cats={MakeCat(1,1,46)}; s.cats[0].details.marker="triangle";
    CHECK_NEWBORN(Decision(PlanNewbornCats(s,7),1).marker=="triangle");
    s=House(); s.cats={MakeCat(1,4,40,51,10)}; s.cats[0].details.inbreeding=-1;
    CHECK_NEWBORN(PlanNewbornCats(s,0).Valid()); // Combat room ignores breeding potential.
    s=House(); s.cats={MakeCat(100,3,44,56,10),MakeCat(101,3,45,56,10),MakeCat(1,3,48)};
    s.cats.back().details.inbreeding=0.25; s.cats.back().parents={true,{100,101},{"Dad","Mom"}};
    CHECK_NEWBORN(Room(Decision(PlanNewbornCats(s,0),100))==4);
    CHECK_NEWBORN(Room(Decision(PlanNewbornCats(s,0,NewbornRules{false}),100))==3);
    // Health risks bypass every breeding threshold and promotion path.
    s=House(); s.cats={MakeCat(1,0,49),MakeCat(2,1,49,51),MakeCat(3,3,49),MakeCat(4,2,49)};
    s.cats[0].details.bad.push_back({"Bad",""}); s.cats[1].details.diseases.push_back({"Sick",""});
    s.cats[2].details.bad.push_back({"Bad",""}); s.cats[3].details.diseases.push_back({"Sick",""});
    p=PlanNewbornCats(s,10); CHECK_NEWBORN(p.Valid());
    for(const auto& d:p.decisions) CHECK_NEWBORN(Room(d)==4);
    CHECK_NEWBORN(!Decision(p,1).discard && Decision(p,2).discard);
    // House-marked diversity cats are never marked, moved or culled.
    for(auto& c:s.cats) c.details.marker="appeal";
    s.cats[3].details.inbreeding=.4;
    p=PlanNewbornCats(s,10); CHECK_NEWBORN(p.Valid() && p.newborns==0);
    for(const auto& d:p.decisions) CHECK_NEWBORN(d.marker.empty() && !d.discard && d.destination==d.cat.location_key);
    s=House(); s.cats={MakeCat(100,3,44,40,10),MakeCat(101,3,45,56,10),MakeCat(1,3,48)};
    s.cats[0].details.marker="appeal"; s.cats[2].details.inbreeding=.25; s.cats[2].parents={true,{100,101},{"Dad","Mom"}};
    p=PlanNewbornCats(s,0);
    CHECK_NEWBORN(Room(Decision(p,100))==3 && !Decision(p,100).discard && Room(Decision(p,101))==4);
    s.cats[1].details.marker="appeal"; p=PlanNewbornCats(s,0);
    CHECK_NEWBORN(p.Valid() && Room(Decision(p,100))==3 && Room(Decision(p,101))==3 && Room(Decision(p,1))==4);
    // Chungus participates in the quality tie break and is no longer a health
    // risk. An additional real disease or bad mutation still sends it to battle.
    s=House(); s.cats={MakeCat(1,0,48,51),MakeCat(2,0,48),MakeCat(3,0,48)};
    s.cats[0].details.good.push_back(LookupDisorder("Chungus"));
    s.cats[1].details.collar="Mage";
    CHECK_NEWBORN(QualityMutations(s.cats[0].details)==1 && !BreedingHealthRisk(s.cats[0]));
    p=PlanNewbornCats(s,17); CHECK_NEWBORN(p.Valid());
    CHECK_NEWBORN(Room(Decision(p,1))==0 && !Decision(p,1).discard && Room(Decision(p,2))==0);
    s.cats[0].details.diseases.push_back(LookupDisorder("Rabies"));
    p=PlanNewbornCats(s,17);
    CHECK_NEWBORN(Room(Decision(p,1))==4 && Decision(p,1).discard);
    s.cats[0].details.diseases.clear(); s.cats[0].details.bad.push_back({"Bad",""});
    p=PlanNewbornCats(s,17);
    CHECK_NEWBORN(Room(Decision(p,1))==4 && Decision(p,1).discard);
    std::cout << "Newborn planner passed: thresholds, DEX priority, tier quotas, quality/class ties, seeded ties, promotions, sibling deduplication, adult preservation, combat culls, Chungus exception and stale-plan rejection.\n";
    return 0;
}
