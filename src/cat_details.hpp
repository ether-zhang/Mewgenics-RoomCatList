#pragma once
#include "localization.hpp"
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace roomcats {
using Stats = std::array<std::int32_t, 7>;
struct Trait {
    std::string name;
    std::string effect;
    Stats stat_delta{};
    bool quality = false;
    std::string name_en;
    std::string effect_en;
};
inline const std::string& TraitName(const Trait& trait) {
    return !Chinese() && !trait.name_en.empty() ? trait.name_en : trait.name;
}
inline const std::string& TraitEffect(const Trait& trait) {
    return !Chinese() && !trait.effect_en.empty() ? trait.effect_en : trait.effect;
}
struct CatDetails {
    bool valid = false;
    int age = 0;
    Stats real{}, genetic{};
    std::vector<Trait> good, bad, diseases;
    std::uint64_t revision = 0;
    int sex = -1;
    double sexuality = -1;
    std::string marker;
    double inbreeding = -1;
    std::string collar;
};
struct NativeCatDetails {
    alignas(16) Stats real{};
    std::vector<std::uint64_t> mutation_keys;
};
using NativeDetailsReader = bool (*)(std::uintptr_t data, NativeCatDetails& result);
void ConfigureCatDetailsReader(NativeDetailsReader reader);
void ClearCatDetailsCache();
CatDetails ReadCatDetails(std::uintptr_t data, std::uint64_t id, int game_day);
std::string FormatStat(const CatDetails& details, int stat);
std::int64_t TotalStats(const Stats& stats);
std::string FormatTotalStat(const CatDetails& details);
Stats MutationStatImpact(const CatDetails& details);
std::string FormatSignedStat(std::int64_t value);
std::string FormatSexOrientation(const CatDetails& details);
const char* InbreedingLabel(double coefficient);
Trait LookupDisorder(const std::string& id, std::int64_t level = 1);
bool LookupMutation(std::uint64_t key, Trait& trait, bool& bad);
}
