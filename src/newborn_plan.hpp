#pragma once
#include "game_reader.hpp"
#include "screening_rules.hpp"

namespace roomcats {
enum class NurseryRoom { Other=-1, Attic, SecondLeft, SecondRight, FirstLeft, Battle };
enum class ReviewChoice { Discard, Keep, Location, Npc };
enum class ScreeningKind { Newborn, Adult };
struct NewbornDecision {
    Cat cat;
    std::string destination, marker, reason;
    bool discard = false;
    ReviewChoice choice = ReviewChoice::Discard;
    std::string alternative_location;
    int npc = -1;
    std::string selection_label; // Remember the selected destination even if it becomes unavailable.
};
struct NewbornPlan {
    RoomSnapshot source;
    std::vector<NewbornDecision> decisions;
    std::vector<std::string> notes;
    std::string error;
    std::uint64_t fingerprint=0, seed=0;
    int newborns=0;
    ScreeningKind kind=ScreeningKind::Newborn;
    NewbornRules newborn_rules;
    AdultRules adult_rules;
    bool Valid() const { return error.empty() && source.valid && source.in_house; }
};
NurseryRoom NurseryOf(const std::string& location);
int QualityMutations(const CatDetails& details);
bool HasCatClass(const CatDetails& details);
bool ProtectedCat(const Cat& cat);
bool BreedingHealthRisk(const Cat& cat,const BreedingRules& rules = {});
bool FailsBattleScreen(const Cat& cat,const BattleRules& rules = {});
bool BetterBreeder(const Cat& a,const Cat& b,std::uint64_t seed,bool dex=false,bool use_class=true,int preferred_dex=7);
std::uint64_t NurseryFingerprint(const RoomSnapshot& snapshot);
std::uint64_t ScreeningFingerprint(const RoomSnapshot&,ScreeningKind);
NewbornPlan PlanNewbornCats(const RoomSnapshot&, std::uint64_t seed, NewbornRules rules = {});
bool ValidateNewbornPlan(const NewbornPlan&,const RoomSnapshot&,std::string& reason);
}
