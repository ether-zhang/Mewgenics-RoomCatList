#pragma once
#include "imgui.h"
#include "imgui_internal.h"
#include "ui_skin_data.hpp"
#include <algorithm>
#include <array>
#include <cstring>

namespace roomcats::ui {
enum Skin { Paper, Up, Over, Down, Disabled, Tooltip, SkinCount };
struct SkinRegion { ImVec2 min{},max{}; };
inline ImGuiContext* context=nullptr;
inline ImFontAtlas* skin_atlas=nullptr;
inline std::array<SkinRegion,SkinCount> regions{};
inline bool Active() { return GImGui && context==GImGui; }
inline ImVec4 Ink() { return {0.065f,0.063f,0.055f,1}; }
inline ImVec4 Warning() { return Active() ? ImVec4{.48f,.23f,.05f,1} : ImVec4{1,.7f,.3f,1}; }
inline ImVec4 TraitTitle() { return Active() ? ImVec4{.28f,.22f,.12f,1} : ImVec4{.91f,.81f,.58f,1}; }

inline void Apply(bool metrics=true) {
    context=GImGui;
    auto& s=ImGui::GetStyle();
    ImGui::StyleColorsLight(&s);
    s.WindowRounding=s.ChildRounding=s.PopupRounding=s.FrameRounding=s.TabRounding=0;
    s.ScrollbarRounding=1; s.GrabRounding=0;
    s.WindowBorderSize=2; s.ChildBorderSize=1; s.PopupBorderSize=2; s.FrameBorderSize=1;
    if(metrics) {
        s.WindowPadding={18,16}; s.FramePadding={13,8}; s.ItemSpacing={10,10}; s.CellPadding={9,7};
        s.ScrollbarSize=17;
    }
    auto* c=s.Colors;
    c[ImGuiCol_Text]=Ink(); c[ImGuiCol_TextDisabled]={.36f,.35f,.32f,1};
    c[ImGuiCol_WindowBg]={.82f,.82f,.79f,1}; c[ImGuiCol_ChildBg]={0,0,0,0};
    c[ImGuiCol_PopupBg]={.89f,.88f,.84f,1}; c[ImGuiCol_Border]={.18f,.17f,.15f,1}; c[ImGuiCol_BorderShadow]={0,0,0,.15f};
    c[ImGuiCol_TitleBg]={.79f,.79f,.76f,1}; c[ImGuiCol_TitleBgActive]={.88f,.88f,.84f,1}; c[ImGuiCol_TitleBgCollapsed]=c[ImGuiCol_TitleBg];
    c[ImGuiCol_FrameBg]={.87f,.86f,.82f,1}; c[ImGuiCol_FrameBgHovered]={.96f,.95f,.91f,1}; c[ImGuiCol_FrameBgActive]={.74f,.73f,.68f,1};
    c[ImGuiCol_Button]=c[ImGuiCol_FrameBg]; c[ImGuiCol_ButtonHovered]=c[ImGuiCol_FrameBgHovered]; c[ImGuiCol_ButtonActive]=c[ImGuiCol_FrameBgActive];
    c[ImGuiCol_Header]={.38f,.35f,.29f,.26f}; c[ImGuiCol_HeaderHovered]={.40f,.36f,.28f,.38f}; c[ImGuiCol_HeaderActive]={.35f,.31f,.23f,.48f};
    c[ImGuiCol_TableHeaderBg]={.67f,.65f,.59f,.90f}; c[ImGuiCol_TableRowBg]={1,1,1,.05f}; c[ImGuiCol_TableRowBgAlt]={.20f,.18f,.13f,.08f};
    c[ImGuiCol_TableBorderStrong]={.19f,.18f,.15f,.65f}; c[ImGuiCol_TableBorderLight]={.23f,.21f,.17f,.28f};
    c[ImGuiCol_CheckMark]=Ink(); c[ImGuiCol_SliderGrab]={.26f,.25f,.22f,1}; c[ImGuiCol_SliderGrabActive]=Ink();
    c[ImGuiCol_ScrollbarBg]={.60f,.59f,.54f,.65f}; c[ImGuiCol_ScrollbarGrab]={.31f,.30f,.26f,1};
    c[ImGuiCol_ScrollbarGrabHovered]={.20f,.19f,.16f,1}; c[ImGuiCol_ScrollbarGrabActive]=Ink();
    c[ImGuiCol_Separator]={.22f,.20f,.16f,.65f}; c[ImGuiCol_SeparatorHovered]={.10f,.09f,.07f,1}; c[ImGuiCol_SeparatorActive]=Ink();
    c[ImGuiCol_ResizeGrip]={.24f,.22f,.17f,.50f}; c[ImGuiCol_ResizeGripHovered]={.18f,.16f,.11f,.85f}; c[ImGuiCol_ResizeGripActive]=Ink();
    c[ImGuiCol_Tab]={.66f,.64f,.57f,1}; c[ImGuiCol_TabHovered]={.88f,.86f,.79f,1};
    c[ImGuiCol_TabSelected]={.89f,.87f,.80f,1}; c[ImGuiCol_TabSelectedOverline]=Ink();
    c[ImGuiCol_TextSelectedBg]={.47f,.42f,.30f,.30f}; c[ImGuiCol_NavCursor]={.23f,.20f,.13f,1};
    c[ImGuiCol_ModalWindowDimBg]={.05f,.045f,.03f,.4f};
}

inline std::array<int,SkinCount> ReserveAtlas(ImFontAtlas& atlas) {
    std::array<int,SkinCount> ids{};
    for(int i=0;i<SkinCount;++i) ids[i]=atlas.AddCustomRectRegular(kNativeUiSkinImages[i].width,kNativeUiSkinImages[i].height);
    return ids;
}
inline void FillAtlas(ImFontAtlas& atlas,const std::array<int,SkinCount>& ids,unsigned char* pixels,int width,int height) {
    for(int i=0;i<SkinCount;++i) {
        const auto& image=kNativeUiSkinImages[i]; const auto* rect=atlas.GetCustomRectByIndex(ids[i]);
        regions[i]={{float(rect->X)/width,float(rect->Y)/height},{float(rect->X+rect->Width)/width,float(rect->Y+rect->Height)/height}};
        for(int y=0;y<image.height;++y) for(int x=0;x<image.width;++x) {
            const auto* src=kNativeUiSkinPixels+image.offset+(y*image.width+x)*2;
            auto* dst=pixels+((rect->Y+y)*width+rect->X+x)*4;
            dst[0]=dst[1]=dst[2]=src[0]; dst[3]=src[1];
        }
    }
    skin_atlas=&atlas;
}
inline void Reset() { context=nullptr;skin_atlas=nullptr;regions={}; }

inline void DrawSkin(ImDrawList* draw,Skin skin,ImVec2 min,ImVec2 max,float alpha=1) {
    if(max.x<=min.x || max.y<=min.y) return;
    if(skin_atlas!=ImGui::GetIO().Fonts || !skin_atlas->TexID) {
        draw->AddRectFilled(min,max,ImGui::GetColorU32(ImVec4{.85f,.84f,.8f,alpha}));
        draw->AddRect(min,max,ImGui::GetColorU32(Ink()),0,0,1.5f);return;
    }
    const auto& image=kNativeUiSkinImages[skin]; const auto& uv=regions[skin];
    const float edge=skin==Paper || skin==Tooltip ? 20.0f : 9.0f;
    const float src_x=skin==Paper || skin==Tooltip ? 24.0f : 20.0f;
    const float src_y=skin==Paper || skin==Tooltip ? 24.0f : 12.0f;
    const float ex=std::min(edge,(max.x-min.x)*.25f),ey=std::min(edge,(max.y-min.y)*.25f);
    const float xs[]={min.x,min.x+ex,max.x-ex,max.x},ys[]={min.y,min.y+ey,max.y-ey,max.y};
    const float us[]={uv.min.x,uv.min.x+(uv.max.x-uv.min.x)*src_x/image.width,uv.max.x-(uv.max.x-uv.min.x)*src_x/image.width,uv.max.x};
    const float vs[]={uv.min.y,uv.min.y+(uv.max.y-uv.min.y)*src_y/image.height,uv.max.y-(uv.max.y-uv.min.y)*src_y/image.height,uv.max.y};
    for(int y=0;y<3;++y) for(int x=0;x<3;++x)
        draw->AddImage(skin_atlas->TexID,{xs[x],ys[y]},{xs[x+1],ys[y+1]},{us[x],vs[y]},{us[x+1],vs[y+1]},ImGui::GetColorU32(ImVec4{1,1,1,alpha}));
}

// Called immediately after Begin(), before content. Preserve existing scrollbar
// hit boxes/rendering, then redraw only the title/close ink above the paper.
inline void WindowPaper(bool close_button=false,bool tooltip=false,const char* localized_title=nullptr) {
    if(!Active()) return;
    auto* w=ImGui::GetCurrentWindow(); auto* draw=w->DrawList; const auto& style=ImGui::GetStyle();
    const ImVec2 end{w->Pos.x+w->Size.x-w->ScrollbarSizes.x,w->Pos.y+w->Size.y-w->ScrollbarSizes.y};
    draw->PushClipRect(w->Pos,{w->Pos.x+w->Size.x,w->Pos.y+w->Size.y},false);
    DrawSkin(draw,tooltip ? Tooltip : Paper,w->Pos,end);
    if(!(w->Flags&ImGuiWindowFlags_NoTitleBar)) {
        const char* title=localized_title ? localized_title : w->Name;
        const char* end_label=ImGui::FindRenderedTextEnd(title);
        const ImVec2 pos{w->Pos.x+style.FramePadding.x+5,w->Pos.y+style.FramePadding.y};
        draw->AddText(pos,ImGui::GetColorU32(ImGuiCol_Text),title,end_label);
        draw->AddLine({w->Pos.x+9,w->Pos.y+w->TitleBarHeight},{end.x-9,w->Pos.y+w->TitleBarHeight-1},ImGui::GetColorU32(ImGuiCol_Separator),1.5f);
        if(close_button) {
            const float r=ImGui::GetFontSize()*.26f;
            const ImVec2 c{w->Pos.x+w->Size.x-style.FramePadding.x-ImGui::GetFontSize()*.5f,w->Pos.y+style.FramePadding.y+ImGui::GetFontSize()*.5f};
            if(GImGui->HoveredId==w->GetID("#CLOSE")) draw->AddCircleFilled(c,ImGui::GetFontSize()*.6f,ImGui::GetColorU32(ImGuiCol_HeaderHovered));
            draw->AddLine({c.x-r,c.y-r},{c.x+r,c.y+r},ImGui::GetColorU32(ImGuiCol_Text),2);
            draw->AddLine({c.x-r,c.y+r},{c.x+r,c.y-r},ImGui::GetColorU32(ImGuiCol_Text),2);
        }
    }
    if(!(w->Flags&ImGuiWindowFlags_NoResize) && !(w->Flags&ImGuiWindowFlags_AlwaysAutoResize))
        for(int i=0;i<3;++i) draw->AddLine({end.x-5-4*i,end.y-5},{end.x-5,end.y-5-4*i},ImGui::GetColorU32(ImGuiCol_ResizeGrip),1.3f);
    draw->PopClipRect();
}

// Same layout, IDs and input behavior as the bundled Dear ImGui 1.91.9b
// Button/BeginCombo (MIT); only their frame rendering uses native artwork.
inline bool Button(const char* label,ImVec2 requested={0,0}) {
    if(!Active()) return ImGui::Button(label,requested);
    auto* w=ImGui::GetCurrentWindow(); if(w->SkipItems) return false;
    const auto& s=ImGui::GetStyle(); const auto id=w->GetID(label); const auto text=ImGui::CalcTextSize(label,nullptr,true);
    const auto size=ImGui::CalcItemSize(requested,text.x+s.FramePadding.x*2,text.y+s.FramePadding.y*2);
    const ImRect bb(w->DC.CursorPos,{w->DC.CursorPos.x+size.x,w->DC.CursorPos.y+size.y});
    ImGui::ItemSize(size,s.FramePadding.y); if(!ImGui::ItemAdd(bb,id)) return false;
    bool hovered=false,held=false; const bool pressed=ImGui::ButtonBehavior(bb,id,&hovered,&held);
    const bool disabled=(GImGui->CurrentItemFlags&ImGuiItemFlags_Disabled)!=0;
    DrawSkin(w->DrawList,disabled ? Disabled : held && hovered ? Down : hovered ? Over : Up,bb.Min,bb.Max);
    if(hovered && !disabled) w->DrawList->AddRect(bb.Min,bb.Max,ImGui::GetColorU32(ImVec4{.08f,.07f,.04f,.35f}),0,0,1);
    ImGui::RenderNavCursor(bb,id);
    if(GImGui->LogEnabled) ImGui::LogSetNextTextDecoration("[","]");
    ImGui::RenderTextClipped({bb.Min.x+s.FramePadding.x,bb.Min.y+s.FramePadding.y},{bb.Max.x-s.FramePadding.x,bb.Max.y-s.FramePadding.y},label,nullptr,&text,s.ButtonTextAlign,&bb);
    return pressed;
}
inline bool BeginCombo(const char* label,const char* preview,ImGuiComboFlags flags=0) {
    if(!Active()) return ImGui::BeginCombo(label,preview,flags);
    auto& g=*GImGui; auto* w=ImGui::GetCurrentWindow(); const auto saved=g.NextWindowData.HasFlags;g.NextWindowData.ClearFlags();
    if(w->SkipItems) return false;
    const auto& s=g.Style; const auto id=w->GetID(label); const auto text=ImGui::CalcTextSize(label,nullptr,true);
    const float arrow=(flags&ImGuiComboFlags_NoArrowButton) ? 0 : ImGui::GetFrameHeight();
    const float fit=(flags&ImGuiComboFlags_WidthFitPreview) && preview ? ImGui::CalcTextSize(preview,nullptr,true).x : 0;
    const float width=flags&ImGuiComboFlags_NoPreview ? arrow : flags&ImGuiComboFlags_WidthFitPreview ? arrow+fit+s.FramePadding.x*2 : ImGui::CalcItemWidth();
    const ImRect bb(w->DC.CursorPos,{w->DC.CursorPos.x+width,w->DC.CursorPos.y+text.y+s.FramePadding.y*2});
    const ImRect total(bb.Min,{bb.Max.x+(text.x>0 ? s.ItemInnerSpacing.x+text.x : 0),bb.Max.y});
    ImGui::ItemSize(total,s.FramePadding.y); if(!ImGui::ItemAdd(total,id,&bb)) return false;
    bool hovered=false,held=false; const bool pressed=ImGui::ButtonBehavior(bb,id,&hovered,&held);
    const auto popup=ImHashStr("##ComboPopup",0,id); bool open=ImGui::IsPopupOpen(popup,ImGuiPopupFlags_None);
    if(pressed && !open) { ImGui::OpenPopupEx(popup,ImGuiPopupFlags_None);open=true; }
    const bool disabled=(g.CurrentItemFlags&ImGuiItemFlags_Disabled)!=0;
    DrawSkin(w->DrawList,disabled ? Disabled : open || held ? Down : hovered ? Over : Up,bb.Min,bb.Max);
    ImGui::RenderNavCursor(bb,id); const float right=std::max(bb.Min.x,bb.Max.x-arrow);
    if(arrow) {
        w->DrawList->AddLine({right,bb.Min.y+4},{right-1,bb.Max.y-4},ImGui::GetColorU32(ImGuiCol_Separator),1.5f);
        ImGui::RenderArrow(w->DrawList,{right+s.FramePadding.y,bb.Min.y+s.FramePadding.y},ImGui::GetColorU32(ImGuiCol_Text),ImGuiDir_Down);
    }
    if(preview && !(flags&ImGuiComboFlags_NoPreview)) {
        if(g.LogEnabled) ImGui::LogSetNextTextDecoration("{","}");
        ImGui::RenderTextClipped({bb.Min.x+s.FramePadding.x,bb.Min.y+s.FramePadding.y},{right,bb.Max.y},preview,nullptr,nullptr);
    }
    if(text.x>0) ImGui::RenderText({bb.Max.x+s.ItemInnerSpacing.x,bb.Min.y+s.FramePadding.y},label);
    if(!open) return false;
    g.NextWindowData.HasFlags=saved;
    const bool began=ImGui::BeginComboPopup(popup,bb,flags);
    if(began) WindowPaper(false,true);
    return began;
}
inline bool Checkbox(const char* label,bool* value) {
    if(!Active()) return ImGui::Checkbox(label,value);
    auto* w=ImGui::GetCurrentWindow(); const auto pos=ImGui::GetCursorScreenPos(); const float size=ImGui::GetFrameHeight();
    ImGui::PushStyleColor(ImGuiCol_FrameBg,{0,0,0,0}); ImGui::PushStyleColor(ImGuiCol_FrameBgHovered,{0,0,0,0});
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive,{0,0,0,0}); ImGui::PushStyleColor(ImGuiCol_CheckMark,{0,0,0,0});
    const bool changed=ImGui::Checkbox(label,value); ImGui::PopStyleColor(4);
    if(ImGui::IsItemVisible()) {
        DrawSkin(w->DrawList,ImGui::IsItemHovered() ? Over : Up,pos,{pos.x+size,pos.y+size});
        if(*value) ImGui::RenderCheckMark(w->DrawList,{pos.x+size*.20f,pos.y+size*.18f},ImGui::GetColorU32(ImGuiCol_Text),size*.64f);
    }
    return changed;
}
inline bool InputInt(const char* label,int* value,int step=1,int fast_step=100) {
    if(!Active() || step<=0) return ImGui::InputInt(label,value,step,fast_step);
    auto* window=ImGui::GetCurrentWindow();
    if(window->SkipItems) return false;
    const auto pos=ImGui::GetCursorScreenPos(); const float width=ImGui::CalcItemWidth(),size=ImGui::GetFrameHeight();
    const bool changed=ImGui::InputInt(label,value,step,fast_step);
    if(!ImGui::IsItemVisible()) return changed;
    const bool disabled=(GImGui->CurrentItemFlags&ImGuiItemFlags_Disabled)!=0;
    const float gap=ImGui::GetStyle().ItemInnerSpacing.x;
    for(int i=0;i<2;++i) {
        const ImVec2 min{pos.x+width-(2-i)*size-(1-i)*gap,pos.y},max{min.x+size,min.y+size};
        const bool hover=!disabled && ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem) && ImGui::IsMouseHoveringRect(min,max);
        DrawSkin(window->DrawList,disabled ? Disabled : hover && ImGui::GetIO().MouseDown[0] ? Down : hover ? Over : Up,min,max);
        const char* text=i ? "+" : "-"; const auto extent=ImGui::CalcTextSize(text);
        window->DrawList->AddText({min.x+(size-extent.x)*.5f,min.y+(size-extent.y)*.5f},ImGui::GetColorU32(ImGuiCol_Text),text);
    }
    return changed;
}
}
