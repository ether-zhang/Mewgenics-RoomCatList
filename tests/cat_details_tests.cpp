#include "../src/cat_details.hpp"
#include "../src/game_reader.hpp"
#include <array>
#include <cassert>
#include <cstring>
#include <iostream>
#include <limits>

namespace {
std::array<unsigned char, 0xC58> data{};
int calls = 0;
template<class T> void Put(std::size_t offset, const T& value) { std::memcpy(data.data() + offset, &value, sizeof(value)); }
void String(std::size_t offset, const char* value) {
    const auto length = std::strlen(value);
    assert(length < 16);
    std::memset(data.data() + offset, 0, 32);
    std::memcpy(data.data() + offset, value, length);
    Put(offset + 16, std::uint64_t(length));
    Put(offset + 24, std::uint64_t(15));
}
bool FakeNative(std::uintptr_t address, roomcats::NativeCatDetails& result) {
    ++calls;
    assert(address == reinterpret_cast<std::uintptr_t>(data.data()));
    result.real = {10, 4, 3, 9, 8, 7, 6};
    result.mutation_keys = {(std::uint64_t(300) << 32) | 0, (std::uint64_t(705) << 32) | 3};
    return true;
}
}

int main() {
    using namespace roomcats;
    SetLanguage(Language::Chinese);
    ConfigureCatDetailsReader(FakeNative);
    Put(0xC48, std::uint64_t(42));
    Put(0xC38, std::int64_t(14));
    Put(0xC40, std::int64_t(-1));
    Put(0x6F0, Stats{4, 8, 6, 5, 3, 7, 2});
    String(0x960, "Rabies");
    String(0x988, "Boils");
    String(0x38, "sword");
    String(0xC10, "Fighter");
    Put(0x58, std::int32_t(1));
    Put(0xBC0, 0.95);
    const auto address = reinterpret_cast<std::uintptr_t>(data.data());
    const auto original = data;
    const auto d = ReadCatDetails(address, 42, 100);
    assert(d.valid && d.age == 86);
    assert(d.marker == "sword" && FormatSexOrientation(d) == "母/同/无");
    assert(d.collar == "Fighter");
    assert(d.genetic[0] == 4 && d.real[0] == 10 && FormatStat(d, 0) == "10(4)");
    assert(TotalStats(d.real) == 47 && FormatTotalStat(d) == "47(35)");
    assert(FormatTotalStat(CatDetails{}) == "—");
    Stats large;
    large.fill(std::numeric_limits<std::int32_t>::max());
    assert(TotalStats(large) == 15032385529LL);
    assert(d.good.size() == 1 && d.bad.size() == 1 && d.diseases.size() == 2);
    assert(d.good[0].quality && !d.bad[0].quality);
    assert(MutationStatImpact(d) == (Stats{0, 0, 1, 0, 0, 0, 0}));
    auto combined = d;
    combined.good[0].stat_delta = {1, 0, 3, 0, 0, 0, 0};
    combined.bad[0].stat_delta = {0, 0, 0, 0, -2, 0, -1};
    combined.diseases[0].stat_delta.fill(100); // Diseases must not enter the mutation sum.
    assert(MutationStatImpact(combined) == (Stats{1, 0, 3, 0, -2, 0, -1}));
    assert(TotalStats(MutationStatImpact(combined)) == 1);
    assert(FormatSignedStat(1) == "+1" && FormatSignedStat(0) == "+0" && FormatSignedStat(-2) == "-2");
    assert(d.good[0].name == "身体突变" && d.good[0].effect.find("体质 +1") != std::string::npos);
    assert(d.diseases[0].name == "狂犬病");
    assert(d.diseases[0].effect.find("力量") != std::string::npos);
    SetLanguage(Language::English);
    assert(TraitName(d.good[0]) == "Body Mutation" && TraitEffect(d.good[0]).find("Constitution +1") != std::string::npos);
    assert(TraitName(d.diseases[0]) == "Rabies" && TraitEffect(d.diseases[0]).find("Strength") != std::string::npos);
    assert(FormatSexOrientation(d) == "F/G/None");
    SetLanguage(Language::Chinese);
    assert(TraitName(d.good[0]) == d.good[0].name && TraitName(d.diseases[0]) == "狂犬病");
    assert(data == original); // Viewing metadata must not edit the cat.
    for (int i = 0; i < 1000; ++i) assert(ReadCatDetails(address, 42, 100).revision == d.revision);
    assert(calls == 1);
    auto identity = d;
    identity.sex = 0; identity.sexuality = 0;
    assert(FormatSexOrientation(identity) == "公/异/无");
    for (double boundary : {0.1, 0.5, 0.9}) {
        identity.sexuality = boundary;
        assert(FormatSexOrientation(identity) == "公/双/无");
    }
    identity.sex = 2;
    assert(FormatSexOrientation(identity) == "无/双/无");
    assert(FormatSexOrientation(CatDetails{}) == "—");
    for (const auto& sample : std::initializer_list<std::pair<double,const char*>>{
            {0,"无"}, {0.1,"无"}, {0.10001,"轻"}, {0.25,"轻"}, {0.25001,"中"},
            {0.5,"中"}, {0.50001,"重"}, {0.8,"重"}, {0.95,"重"}, {1,"重"}})
        assert(std::string(InbreedingLabel(sample.first)) == sample.second);
    assert(std::string(InbreedingLabel(-1)) == "未知");
    assert(std::string(InbreedingLabel(std::numeric_limits<double>::quiet_NaN())) == "未知");
    Put(0x70C, std::int32_t(2)); // In-place level/stat change invalidates the cache.
    assert(ReadCatDetails(address, 42, 100).revision != d.revision && calls == 2);
    assert(ReadCatDetails(address, 42, 101).age == 87 && calls == 3);
    Put(0xC40, std::int64_t(90));
    assert(ReadCatDetails(address, 42, 101).age == 76); // Frozen age for deceased cats.
    assert(!ReadCatDetails(address, 43, 101).valid); // Identity mismatch.
    assert(!ReadCatDetails(0x1234, 42, 101).valid);
    String(0x960, "None"); String(0x988, "");
    assert(ReadCatDetails(address, 42, 101).diseases.empty());
    // The Chungus exception works in either native disorder slot. It adds one
    // quality mutation and its +4 CON to the tooltip, never to the real stats
    // a second time, and never exempts another disease or a bad mutation.
    for (const auto offset : {0x960, 0x988}) {
        const auto other = offset == 0x960 ? 0x988 : 0x960;
        String(offset, "Chungus"); Put(offset + 32, std::int64_t(1));
        String(other, "None");
        const auto raw = data;
        const auto positive = ReadCatDetails(address, 42, 101);
        assert(positive.valid && positive.good.size() == 2 && positive.bad.size() == 1 && positive.diseases.empty());
        const auto& chungus = positive.good.back();
        assert(chungus.name == "大块头" && chungus.effect == "体质 +4" && chungus.quality);
        assert(chungus.stat_delta == (Stats{0, 0, 4, 0, 0, 0, 0}));
        assert(MutationStatImpact(positive) == (Stats{0, 0, 5, 0, 0, 0, 0}));
        assert(positive.real == d.real && positive.genetic == d.genetic && data == raw);
        assert(ReadCatDetails(address, 42, 101).revision == positive.revision);
        String(other, "Rabies");
        const auto sick = ReadCatDetails(address, 42, 101);
        assert(sick.good.size() == 2 && sick.bad.size() == 1 && sick.diseases.size() == 1);
        assert(sick.diseases[0].name == "狂犬病" && !sick.diseases[0].quality);
        assert(sick.revision != positive.revision);
        String(offset, "None"); String(other, "None");
        assert(ReadCatDetails(address, 42, 101).good.size() == 1);
    }
    for (const auto disorder : {"Gigantism", "Gargantuan", "Gigachad", "FattyLiver", "Unknown"}) {
        String(0x960, disorder);
        const auto ordinary = ReadCatDetails(address, 42, 101);
        assert(ordinary.good.size() == 1 && ordinary.diseases.size() == 1 && !ordinary.diseases[0].quality);
    }
    String(0x960, "None");
    Trait t;
    bool bad = false;
    assert(LookupMutation((std::uint64_t(705) << 32) | 13, t, bad) && bad);
    assert(t.name.find("前腿") != std::string::npos); // Native paired-part aliases.
    assert(!LookupMutation((std::uint64_t(999999) << 32), t, bad));
    assert(FormatStat(CatDetails{}, 0) == "—");
    assert(LookupDisorder("Gigachad").effect.find("魅力 +8") != std::string::npos);
    assert(LookupDisorder("ScalyScabs").effect.find("护盾 +8") != std::string::npos);
    const auto mild = LookupDisorder("DejaVu", 1), severe = LookupDisorder("DejaVu", 3);
    assert(mild.effect.find("10%") != std::string::npos);
    assert(mild.name != severe.name && severe.effect.find("史蒂文") != std::string::npos);
    String(0x960, "DejaVu");
    Put(0x980, std::int64_t(3));
    assert(ReadCatDetails(address, 42, 101).diseases[0].name == severe.name);
    Put(0x980, std::int64_t(1));
    assert(ReadCatDetails(address, 42, 101).diseases[0].effect == mild.effect);
    Put(0xC50, 0.3143);
    assert(ReadCatDetails(address, 42, 101).inbreeding == 0.3143);
    assert(FormatSexOrientation(ReadCatDetails(address, 42, 101)) == "母/同/中");
    Put(0xC50, 0.85);
    assert(FormatSexOrientation(ReadCatDetails(address, 42, 101)) == "母/同/重");
    const auto class_before = ReadCatDetails(address,42,101).revision;
    String(0xC10,"Mage");
    const auto mage = ReadCatDetails(address,42,101);
    assert(mage.collar == "Mage" && mage.revision != class_before);
    ConfigureCatDetailsReader(nullptr);
    std::cout << "Cat details passed: real/genetic values, age, mutation classes, Chinese effects, two disorders, identity guards and cache invalidation.\n";
}
