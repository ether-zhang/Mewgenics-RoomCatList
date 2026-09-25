#include "cat_details.hpp"
#include "game_reader.hpp"
#include "trait_catalog.generated.hpp"
#include <algorithm>
#include <cstring>
#include <cmath>
#include <unordered_map>

namespace roomcats {
namespace {
constexpr std::size_t kCatDataSize = 0xC58;
NativeDetailsReader g_read_native = nullptr;
struct CachedDetails {
    std::array<unsigned char, kCatDataSize> bytes;
    int game_day = -1;
    CatDetails details;
};
std::unordered_map<std::uint64_t, CachedDetails> g_cache;

unsigned NormalizePart(unsigned part) {
    if (part == 11 || part == 12) return 3;
    if (part == 13 || part == 14) return 4;
    if (part == 15 || part == 16) return 6;
    if (part == 17 || part == 18) return 7;
    if (part == 19 || part == 20) return 8;
    return part;
}
const char* Category(unsigned part) {
    switch (part) {
        case 0: return "body"; case 1: return "head"; case 2: return "tail";
        case 3: case 4: return "legs"; case 6: return "eyes"; case 7: return "eyebrows";
        case 8: return "ears"; case 9: return "mouth"; case 10: return "texture";
        default: return "";
    }
}
template<class T> T Field(const std::array<unsigned char, kCatDataSize>& bytes, std::size_t offset) {
    T value{};
    std::memcpy(&value, bytes.data() + offset, sizeof(value));
    return value;
}
}

bool LookupMutation(std::uint64_t key, Trait& trait, bool& bad) {
    const auto part = NormalizePart(static_cast<unsigned>(key));
    const auto id = static_cast<std::int32_t>(key >> 32);
    const auto category = Category(part);
    for (const auto& definition : kMutationDefinitions) {
        if (definition.id != id || std::strcmp(category, definition.category)) continue;
        bad = definition.bad;
        trait.quality = definition.quality;
        trait.effect = definition.effect;
        trait.effect_en = definition.effect_en;
        std::memcpy(trait.stat_delta.data(), definition.stat_delta, sizeof(Stats));
        for (const auto& title : kMutationTitles)
            if (title.part == part) {
                trait.name = bad ? title.bad : title.good;
                trait.name_en = bad ? title.bad_en : title.good_en;
                return true;
            }
    }
    return false;
}

Trait LookupDisorder(const std::string& id, std::int64_t level) {
    const DisorderDefinition* best = nullptr;
    for (const auto& definition : kDisorderDefinitions)
        if (id == definition.id && definition.level <= std::max<std::int64_t>(1, level) &&
            (!best || definition.level > best->level)) best = &definition;
    if (best) {
        Trait trait{best->name, best->effect, {}, best->positive_mutation};
        trait.name_en = best->name_en;
        trait.effect_en = best->effect_en;
        std::memcpy(trait.stat_delta.data(), best->stat_delta, sizeof(Stats));
        return trait;
    }
    return {id, roomcats::Tx("暂无对应效果说明"), {}, false, id, "No matching effect description"};
}

void ConfigureCatDetailsReader(NativeDetailsReader reader) { g_read_native = reader; ClearCatDetailsCache(); }
void ClearCatDetailsCache() { g_cache.clear(); }

CatDetails ReadCatDetails(std::uintptr_t data, std::uint64_t id, int game_day) {
    CatDetails out;
    if (!g_read_native || !data || game_day < 0) return out;
    std::array<unsigned char, kCatDataSize> bytes{};
    if (!ReadBytes(data, bytes.data(), bytes.size()) || Field<std::uint64_t>(bytes, 0xC48) != id) return out;
    const auto found = g_cache.find(id);
    if (found != g_cache.end() && found->second.game_day == game_day && found->second.bytes == bytes)
        return found->second.details;
    const auto birth = Field<std::int64_t>(bytes, 0xC38);
    const auto death = Field<std::int64_t>(bytes, 0xC40);
    const auto reference = death == -1 ? static_cast<std::int64_t>(game_day) : death;
    if (birth < -1000000 || birth > reference || reference - birth > 1000000) return out;
    out.age = static_cast<int>(reference - birth);
    out.sex = Field<std::int32_t>(bytes, 0x58);
    if (out.sex < 0 || out.sex > 2) out.sex = -1;
    out.sexuality = Field<double>(bytes, 0xBC0);
    if (!std::isfinite(out.sexuality) || out.sexuality < 0 || out.sexuality > 1) out.sexuality = -1;
    ReadNarrowString(data + 0x38, out.marker);
    ReadNarrowString(data + 0xC10, out.collar);
    out.inbreeding = Field<double>(bytes, 0xC50);
    if (!std::isfinite(out.inbreeding) || out.inbreeding < 0 || out.inbreeding > 1) out.inbreeding = -1;
    out.genetic = Field<Stats>(bytes, 0x6F0);
    NativeCatDetails native;
    if (!g_read_native(data, native) || native.mutation_keys.size() > 32) return out;
    out.real = native.real;
    for (std::size_t i = 0; i < out.real.size(); ++i)
        if (out.real[i] < -100000 || out.real[i] > 100000 ||
            out.genetic[i] < -100000 || out.genetic[i] > 100000) return CatDetails{};
    for (const auto key : native.mutation_keys) {
        Trait trait;
        bool bad = false;
        if (!LookupMutation(key, trait, bad)) return CatDetails{};
        (bad ? out.bad : out.good).push_back(std::move(trait));
    }
    // These are the two Disorder slots, separate from the normal passives.
    for (const auto offset : {0x960, 0x988}) {
        std::string disorder;
        if (!ReadNarrowString(data + offset, disorder)) return CatDetails{};
        if (!disorder.empty() && disorder != "None") {
            auto trait = LookupDisorder(disorder, Field<std::int64_t>(bytes, offset + 32));
            // Chungus is a pure benefit: share one classification for the
            // list, mutation totals and both screening planners. Native data
            // and the effective stats already returned by the game stay intact.
            (trait.quality ? out.good : out.diseases).push_back(std::move(trait));
        }
    }
    out.valid = true;
    static std::uint64_t next_revision = 0;
    out.revision = ++next_revision;
    if (g_cache.size() >= 10000) g_cache.clear();
    g_cache[id] = {bytes, game_day, out};
    return out;
}

std::string FormatStat(const CatDetails& details, int stat) {
    if (!details.valid || stat < 0 || stat >= 7) return "—";
    return std::to_string(details.real[stat]) + "(" + std::to_string(details.genetic[stat]) + ")";
}

std::int64_t TotalStats(const Stats& stats) {
    std::int64_t total = 0;
    for (const auto stat : stats) total += stat;
    return total;
}

std::string FormatTotalStat(const CatDetails& details) {
    if (!details.valid) return "—";
    return std::to_string(TotalStats(details.real)) + "(" + std::to_string(TotalStats(details.genetic)) + ")";
}

Stats MutationStatImpact(const CatDetails& details) {
    Stats result{};
    for (const auto* group : {&details.good, &details.bad})
        for (const auto& trait : *group)
            for (std::size_t i = 0; i < result.size(); ++i) result[i] += trait.stat_delta[i];
    return result;
}

std::string FormatSignedStat(std::int64_t value) {
    return (value >= 0 ? "+" : "") + std::to_string(value);
}

std::string FormatSexOrientation(const CatDetails& details) {
    if (!details.valid) return "—";
    const char* sex = details.sex == 0 ? roomcats::Tx("公") : details.sex == 1 ? roomcats::Tx("母") : details.sex == 2 ? roomcats::Tx("无") : roomcats::Tx("未知");
    // Same strict thresholds as native HouseCatStatus: endpoints 0.1/0.9
    // belong to the middle (bi) range. Sex is independent of orientation.
    const char* orientation = details.sexuality < 0 ? roomcats::Tx("未知") : details.sexuality < 0.1 ? roomcats::Tx("异") :
        details.sexuality > 0.9 ? roomcats::Tx("同") : roomcats::Tx("双");
    return std::string(sex) + "/" + orientation + "/" + InbreedingLabel(details.inbreeding);
}

const char* InbreedingLabel(double coefficient) {
    if (!std::isfinite(coefficient) || coefficient < 0 || coefficient > 1) return roomcats::Tx("未知");
    // Native HouseCatStatus uses strict > 0.1 / 0.25 / 0.5 / 0.8.
    // Merge its high and maximum grades into the requested single roomcats::Tx("重").
    if (coefficient <= 0.1) return roomcats::Tx("无");
    if (coefficient <= 0.25) return roomcats::Tx("轻");
    if (coefficient <= 0.5) return roomcats::Tx("中");
    return roomcats::Tx("重");
}
}
