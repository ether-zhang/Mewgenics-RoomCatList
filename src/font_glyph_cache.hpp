#pragma once
#include "imgui.h"
#include "imgui_internal.h"
#include <set>

namespace roomcats {
// Keep only code points actually used by this UI. Rebuilding ranges visits
// this small ordered set, never the entire Unicode space for every cat name.
class FontGlyphCache {
public:
    void AddBasicLatin() {
        for (ImWchar c = 32; c < 256; ++c) dirty_ |= characters_.insert(c).second;
    }
    void AddText(const char* text) {
        if (!text) return;
        while (*text) {
            unsigned int codepoint = 0;
            const int consumed = ImTextCharFromUtf8(&codepoint, text, nullptr);
            if (consumed <= 0) break;
            text += consumed;
            if (codepoint) dirty_ |= characters_.insert(static_cast<ImWchar>(codepoint)).second;
        }
    }
    bool UpdateRanges() {
        if (!dirty_) return false;
        ranges_.clear();
        for (const auto c : characters_) {
            if (ranges_.Size && ranges_.back() + 1 == c) ranges_.back() = c;
            else { ranges_.push_back(c); ranges_.push_back(c); }
        }
        ranges_.push_back(0);
        dirty_ = false;
        return true;
    }
    const ImWchar* Ranges() const { return ranges_.Data; }
    std::size_t CharacterCount() const { return characters_.size(); }

private:
    std::set<ImWchar> characters_;
    ImVector<ImWchar> ranges_;
    bool dirty_ = false;
};
}
