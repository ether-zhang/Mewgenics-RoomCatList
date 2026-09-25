#pragma once
#include "imgui.h"
#include "marker_icons.generated.hpp"
#include "mew_ui_theme.hpp"
#include "localization.hpp"
#include <array>
#include <cstring>
#include <string>

namespace roomcats {
inline constexpr int kMarkerCount = sizeof(kMarkerIcons) / sizeof(kMarkerIcons[0]);
inline constexpr ImWchar kMarkerGlyphStart = 0xE000;
inline constexpr ImWchar kClassGlyphStart = 0xE100;
inline constexpr int kClassCount = sizeof(kClassIcons) / sizeof(kClassIcons[0]);
inline void BuildMarkerGlyphs(ImFontAtlas& atlas, ImFont* font) {
    const auto skin_rects=ui::ReserveAtlas(atlas);
    std::array<int, kMarkerCount> rects{};
    for (int i = 0; i < kMarkerCount; ++i)
        rects[i] = atlas.AddCustomRectFontGlyph(font, kMarkerGlyphStart + i, 24, 24, 28, {0, -2});
    unsigned char* pixels = nullptr;
    std::array<int,kClassCount> class_rects{};
    for (int i=0;i<kClassCount;++i)
        class_rects[i] = atlas.AddCustomRectFontGlyph(font,kClassGlyphStart+i,24,24,28,{0,-2});
    int width = 0, height = 0;
    atlas.GetTexDataAsRGBA32(&pixels, &width, &height);
    ui::FillAtlas(atlas,skin_rects,pixels,width,height);
    for (int i = 0; i < kMarkerCount; ++i) {
        const auto* rect = atlas.GetCustomRectByIndex(rects[i]);
        for (int y = 0; y < 24; ++y)
            for (int x = 0; x < 24; ++x) {
                auto* p = pixels + ((rect->Y+y)*width+rect->X+x)*4;
                p[0] = p[1] = p[2] = 255;
                p[3] = kMarkerIcons[i].alpha[y*24+x];
            }
    }
    for (int i=0;i<kClassCount;++i) {
        const auto* rect = atlas.GetCustomRectByIndex(class_rects[i]);
        for (int y=0;y<24;++y) for (int x=0;x<24;++x) {
            auto* p = pixels+((rect->Y+y)*width+rect->X+x)*4;
            p[0]=p[1]=p[2]=255; p[3]=kClassIcons[i].alpha[y*24+x];
        }
    }
}

inline int ClassIndex(const std::string& collar) {
    const auto& id = collar.empty() || collar == "None" ? std::string("Colorless") : collar;
    for (int i=0;i<kClassCount;++i) if (id == kClassIcons[i].name) return i;
    return -1;
}
inline std::string ClassLabel(const std::string& collar) {
    const int index = ClassIndex(collar);
    return index < 0 ? collar : Chinese() ? kClassIcons[index].label : kClassIcons[index].label_en;
}
inline std::string ClassGlyph(const std::string& collar) {
    const int index = ClassIndex(collar);
    if (index < 0) return "?";
    const unsigned cp = kClassGlyphStart+index;
    return std::string{char(0xE0 | cp >> 12),char(0x80 | (cp >> 6 & 63)),char(0x80 | (cp & 63))};
}

inline std::string MarkerGlyph(const std::string& marker) {
    if (marker.empty()) return {};
    for (int i = 0; i < kMarkerCount; ++i) {
        if (marker != kMarkerIcons[i].name) continue;
        const unsigned cp = kMarkerGlyphStart + i;
        return std::string{char(0xE0 | cp >> 12), char(0x80 | (cp >> 6 & 63)), char(0x80 | (cp & 63))};
    }
    return "[" + marker + "] "; // Preserve unfamiliar tags added by other mods.
}
}
