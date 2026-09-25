#pragma once
#include "newborn_plan.hpp"

namespace roomcats {
inline constexpr int kPopulationTarget=90;
inline constexpr std::array<int,4> kBreedingComfortMinimum{15,5,5,0};
struct PopulationProjection {
    int total=0, protected_cats=0;
    std::array<int,5> counts{};
    std::array<double,5> comfort{};
};
double RoomComfort(double effects,int cats);
PopulationProjection ProjectPopulation(const NewbornPlan&);
NewbornPlan PlanAdultCats(const RoomSnapshot&,std::uint64_t seed,AdultRules rules = {});
}
