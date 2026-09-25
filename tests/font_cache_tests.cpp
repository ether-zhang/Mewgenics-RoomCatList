#include "../src/font_glyph_cache.hpp"
#include <cassert>
#include <chrono>
#include <iostream>
#include <string>
#include <unordered_set>
#include <vector>

namespace {
bool Contains(const ImWchar* ranges, ImWchar c) {
    for (int i=0; ranges && ranges[i]; i+=2)
        if (c >= ranges[i] && c <= ranges[i+1]) return true;
    return false;
}

void PreviousPerNameScan(const std::vector<std::string>& names) {
    std::unordered_set<ImWchar> characters;
    for (const auto& name : names) {
        ImFontGlyphRangesBuilder builder;
        builder.AddText(name.c_str());
        ImVector<ImWchar> ranges;
        builder.BuildRanges(&ranges);
        for (int i=0; i+1 < ranges.Size && ranges[i]; i+=2)
            for (unsigned c=ranges[i]; c<=ranges[i+1]; ++c) characters.insert(c);
    }
    assert(!characters.empty());
}

template<class F> double Milliseconds(F fn, int repeats) {
    const auto start = std::chrono::steady_clock::now();
    for (int i=0; i<repeats; ++i) fn();
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now()-start).count()/repeats;
}
}

int main() {
    static_assert(sizeof(ImWchar) == 4, "Test the same full-Unicode configuration as the mod");
    roomcats::FontGlyphCache cache;
    cache.AddBasicLatin();
    cache.AddText("阿花 小白 ## <tag> 🐈 𠀀");
    assert(cache.UpdateRanges());
    assert(Contains(cache.Ranges(), 0x82B1));
    assert(Contains(cache.Ranges(), '#'));
    assert(Contains(cache.Ranges(), 0x1F408));
    assert(Contains(cache.Ranges(), 0x20000));
    const auto count = cache.CharacterCount();
    const auto* old_ranges = cache.Ranges();
    for (int i=0; i<1000; ++i) {
        cache.AddText("小白 阿花");
        assert(!cache.UpdateRanges());
        assert(cache.Ranges() == old_ranges);
    }
    assert(cache.CharacterCount() == count);
    cache.AddText("新名字：龙");
    assert(cache.UpdateRanges());
    assert(Contains(cache.Ranges(), 0x9F99) && Contains(cache.Ranges(), 0x1F408));
    cache.AddText(nullptr); cache.AddText("");
    assert(!cache.UpdateRanges());

    std::vector<std::string> names;
    for (int i=0; i<95; ++i) names.push_back("猫咪阿花小白" + std::to_string(i));
    const double previous = Milliseconds([&]{PreviousPerNameScan(names);}, 3);
    const double initial = Milliseconds([&]{
        roomcats::FontGlyphCache fresh;
        for(const auto& name : names) fresh.AddText(name.c_str());
        assert(fresh.UpdateRanges());
    }, 100);
    roomcats::FontGlyphCache steady;
    for(const auto& name : names) steady.AddText(name.c_str());
    steady.UpdateRanges();
    const double unchanged = Milliseconds([&]{
        for(const auto& name : names) steady.AddText(name.c_str());
        assert(!steady.UpdateRanges());
    }, 1000);
    std::cout << "Font cache checks passed: CJK, supplementary Unicode, unchanged names, stable ranges and newly added names.\n"
              << "95-name glyph collection (ms, CPU microbenchmark): previous=" << previous
              << " sparse_initial=" << initial << " cached_repeat=" << unchanged << "\n";
}
