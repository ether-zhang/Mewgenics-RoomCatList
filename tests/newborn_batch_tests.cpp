#include "newborn_test_fixture.hpp"
#include "../src/newborn_batch.hpp"
#include "../src/adult_plan.hpp"
#include "../src/cat_moves.hpp"
#include "../src/cat_actions.hpp"
namespace {
roomcats::RoomSnapshot world;
int writes=0, marks=0,moves=0,donations=0;
bool accepts=true,one_npc=false;
bool delayed_reset=false;
int reset_frames=0;
std::vector<std::pair<std::uint64_t,int>> sent;
roomcats::Cat* Find(std::uint64_t id) { for(auto& c:world.cats) if(c.id==id) return &c; return nullptr; }
std::vector<roomcats::NpcChoice> Choices(const roomcats::RoomSnapshot&,const roomcats::Cat&) {
    return {{0,"NPC",accepts && !reset_frames,"Unavailable",accepts && reset_frames>0},
            {8,"Discard",!reset_frames,"Waiting for pipe reset",reset_frames>0}};
}
bool CanMove(const roomcats::RoomSnapshot&,const roomcats::Cat&,const roomcats::CatLocation&,std::string&) { return true; }
bool Move(const roomcats::RoomSnapshot&,const roomcats::Cat& c,const roomcats::CatLocation& room,std::string&) {
    ++writes; ++moves; auto* actual=Find(c.id); actual->location_key=room.key; actual->location_label=room.label; actual->location=room.component; return true;
}
roomcats::CatActionStatus Action(const roomcats::RoomSnapshot&,roomcats::CatAction& a,std::string&) {
    using namespace roomcats;
    auto* c=Find(a.cat);
    if(a.kind==CatActionKind::Mark) { ++writes; ++marks; c->details.marker=a.marker; return CatActionStatus::Complete; }
    if(!a.phase) { a.phase=1; return CatActionStatus::Running; }
    ++writes; ++donations; c->active=false; sent.push_back({a.cat,a.npc}); if(one_npc) accepts=false;
    if(delayed_reset) { reset_frames=3; std::reverse(world.locations.begin(),world.locations.end()); }
    return CatActionStatus::Complete;
}
void Reset() {
    roomcats::ConfigureCatActions(Choices,Action); roomcats::ConfigureCatMoves(CanMove,Move);
    writes=marks=moves=donations=0; sent.clear(); accepts=true; one_npc=delayed_reset=false; reset_frames=0;
}
void Pump(std::uint64_t time) {
    using namespace roomcats;
    auto fresh=world; if(!NewbornBatchNeedsCats()) fresh.cats.clear();
    TickNewbornBatch(fresh,time); ProcessCatMove(world); ProcessCatAction(world,time);
    if(reset_frames>0) --reset_frames;
}
}
int TestNewbornBatch() {
    using namespace roomcats; using namespace newborn_test; Reset();
    world=House(); world.cats={MakeCat(1,0,47),MakeCat(2,4,40,51,10),MakeCat(3,4,40,51,10),MakeCat(4,4,40,51,10)};
    auto plan=PlanNewbornCats(world,41);
    for(auto& d:plan.decisions) { if(d.cat.id==2) d.choice=ReviewChoice::Keep; if(d.cat.id==3) { d.choice=ReviewChoice::Npc; d.npc=0; } }
    CHECK_NEWBORN(writes==0 && !HasPendingCatAction() && !HasPendingCatMove());
    std::string reason; auto stale=world; stale.game_day++;
    CHECK_NEWBORN(!BeginNewbornBatch(plan,stale,reason) && writes==0);
    CHECK_NEWBORN(BeginNewbornBatch(plan,world,reason) && writes==0);
    CHECK_NEWBORN(!BeginNewbornBatch(plan,world,reason));
    for(unsigned i=0;i<40 && NewbornBatchActive();++i) Pump(100+i*100);
    const auto done=GetNewbornBatchStatus();
    CHECK_NEWBORN(done.finished && !done.running && done.completed==done.total && marks==1 && moves==1 && donations==2);
    CHECK_NEWBORN(Find(1)->details.marker=="triangle" && NurseryOf(Find(1)->location_key)!=NurseryRoom::Attic);
    CHECK_NEWBORN(Find(2)->active && Find(2)->location_key=="room:Floor1_Small");
    CHECK_NEWBORN((sent==std::vector<std::pair<std::uint64_t,int>>{{3,0},{4,8}}));
    // Day changes cancel a queued marker BEFORE the native executor can run it.
    Reset(); world=House(); world.cats={MakeCat(1,0,47)}; plan=PlanNewbornCats(world,42);
    CHECK_NEWBORN(BeginNewbornBatch(plan,world,reason));
    TickNewbornBatch(world,5000); CHECK_NEWBORN(HasPendingCatAction());
    ++world.game_day; TickNewbornBatch(world,5010); ProcessCatAction(world,5010);
    CHECK_NEWBORN(!NewbornBatchActive() && !HasPendingCatAction() && writes==0);
    // An NPC becoming unavailable never falls back to abandoning that cat.
    Reset(); one_npc=true; world=House(); world.cats={MakeCat(10,4,45,51,10),MakeCat(11,4,45,51,10)};
    plan=PlanNewbornCats(world,43); for(auto& d:plan.decisions) { d.choice=ReviewChoice::Npc; d.npc=0; }
    CHECK_NEWBORN(BeginNewbornBatch(plan,world,reason));
    for(unsigned i=0;i<30 && NewbornBatchActive();++i) Pump(6000+i*100);
    CHECK_NEWBORN(!NewbornBatchActive() && GetNewbornBatchStatus().stopped && donations==1 && Find(11)->active);
    // Stop finishes the already started marker, then skips the room move.
    Reset(); world=House(); world.cats={MakeCat(1,0,47)}; plan=PlanNewbornCats(world,44);
    CHECK_NEWBORN(BeginNewbornBatch(plan,world,reason)); TickNewbornBatch(world,10000);
    StopNewbornBatch(); ProcessCatAction(world,10000); Pump(10100);
    CHECK_NEWBORN(!NewbornBatchActive() && GetNewbornBatchStatus().stopped && marks==1 && moves==0);
    // A review override can move a rejected cat to a box instead of disposal.
    Reset(); world=House(); world.cats={MakeCat(2,4,45,51,10)}; plan=PlanNewbornCats(world,45);
    plan.decisions[0].choice=ReviewChoice::Location; plan.decisions[0].alternative_location="box:999";
    CHECK_NEWBORN(BeginNewbornBatch(plan,world,reason));
    for(unsigned i=0;i<10 && NewbornBatchActive();++i) Pump(11000+i*100);
    CHECK_NEWBORN(GetNewbornBatchStatus().finished && moves==1 && donations==0 && Find(2)->active && Find(2)->location_key=="box:999");
    // Reproduce the gap after the first native disposal: the actor is gone
    // while the transport is still latched occupied. Wait, then send all cats.
    Reset(); delayed_reset=true; world=House();
    world.cats={MakeCat(21,4,45,51,10),MakeCat(22,4,45,51,10),MakeCat(23,4,45,51,10)};
    plan=PlanNewbornCats(world,46); CHECK_NEWBORN(BeginNewbornBatch(plan,world,reason));
    Pump(12000); Pump(12100); Pump(12200);
    CHECK_NEWBORN(NewbornBatchActive() && GetNewbornBatchStatus().completed==1 && donations==1 && !HasPendingCatAction());
    CHECK_NEWBORN(NewbornBatchDiagnostic().find("Cat 22")==std::string::npos && NewbornBatchDiagnostic().find("#22")!=std::string::npos);
    for(unsigned i=0;i<40 && NewbornBatchActive();++i) Pump(12300+i*100);
    CHECK_NEWBORN(GetNewbornBatchStatus().finished && donations==3 && sent.size()==3);
    // A transport which never resets is bounded and never receives a new cat.
    Reset(); world=House(); world.cats={MakeCat(31,4,45,51,10)};
    plan=PlanNewbornCats(world,47); CHECK_NEWBORN(BeginNewbornBatch(plan,world,reason));
    reset_frames=100; TickNewbornBatch(world,20000); TickNewbornBatch(world,36000);
    CHECK_NEWBORN(!NewbornBatchActive() && GetNewbornBatchStatus().stopped && donations==0 && !HasPendingCatAction());
    // Adult batches share delivery readiness, but validate furniture as well.
    Reset(); delayed_reset=true; world=House(); world.population_comfort_valid=true;
    world.locations[0].comfort_base=15; world.locations[1].comfort_base=world.locations[2].comfort_base=5;
    world.cats={MakeCat(40,0,45,56,10),MakeCat(41,4,49,51,10),MakeCat(42,4,49,51,10),MakeCat(43,4,49,51,10),MakeCat(44,4,40,40,1)};
    plan=PlanAdultCats(world,48); CHECK_NEWBORN(BeginNewbornBatch(plan,world,reason));
    world.population_comfort_valid=false; Pump(40000);
    CHECK_NEWBORN(NewbornBatchActive() && writes==0);
    world.population_comfort_valid=true;
    for(unsigned i=0;i<60 && NewbornBatchActive();++i) Pump(40100+i*100);
    CHECK_NEWBORN(GetNewbornBatchStatus().finished && GetNewbornBatchStatus().kind==ScreeningKind::Adult && donations==3 && moves==1 && marks==0 && Find(44)->active);
    CHECK_NEWBORN(GetNewbornBatchStatus().post_fingerprint==ScreeningFingerprint(world,ScreeningKind::Adult));
    Reset(); world=House(); world.population_comfort_valid=true;
    world.cats={MakeCat(50,4,49,51,10)}; plan=PlanAdultCats(world,49);
    CHECK_NEWBORN(BeginNewbornBatch(plan,world,reason)); world.locations[0].comfort_base+=1;
    Pump(50000); CHECK_NEWBORN(!NewbornBatchActive() && writes==0 && GetNewbornBatchStatus().stopped);
    ConfigureCatActions(nullptr,nullptr); ConfigureCatMoves(nullptr,nullptr);
    std::cout << "Newborn batch passed: no pre-confirmation writes, one batch, marker/move/NPC/discard order, keep override, completion checks, day-change cancellation and no destructive fallback.\n";
    return 0;
}
