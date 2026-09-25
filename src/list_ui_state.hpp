#pragma once
#include "imgui.h"
#include "imgui_internal.h"
#include "game_reader.hpp"
#include "cat_sort.hpp"
#include "cat_moves.hpp"
#include "cat_actions.hpp"
#include "marker_icons.hpp"
#include "mouse_router.hpp"
#include "gamepad_state.hpp"
#include "newborn_plan.hpp"
#include "newborn_batch.hpp"
#include "adult_plan.hpp"
#include "screening_choices.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <string>
#include <vector>

// Shared by the injected Mod and platform-free UI tests.
namespace {
void Log(const std::string& message);
bool g_open = false;
bool g_has_house = false;
bool g_font_scan_pending = true;
bool g_reset_list_scroll = false;
roomcats::MouseRouter g_mouse;
roomcats::CatSort g_sort;
std::vector<int> g_sorted_rows;
bool g_sort_dirty = true;
std::uint64_t g_move_popup_cat = 0;
bool g_move_popup_drawn = false, g_close_move_popup = false;
std::string g_move_feedback;
roomcats::GamepadFrame g_pad_frame;
roomcats::RoomSnapshot g_snapshot;
struct ListView {
    std::array<std::uint64_t,2> ids{};
    std::array<std::string,2> names{};
    std::string child_name;
    roomcats::CatSort sort;
    ImVec2 scroll{};
    bool IsParents() const { return ids[0] || ids[1]; }
    bool screening = false;
} g_view;
std::vector<ListView> g_view_history;
std::optional<ListView> g_pending_parent_view;
bool g_pending_view_back = false, g_view_refresh_requested = false, g_restore_view_scroll = false;
ImVec2 g_list_scroll{};
std::optional<roomcats::NewbornPlan> g_screening_plan;
roomcats::ScreeningChoices g_screening_choices;
roomcats::ScreeningRules g_screening_rules, g_rules_draft;
bool g_rules_popup_drawn=false, g_rules_save_requested=false;
bool g_language_popup_drawn=false;
bool g_language_save_requested=false;
std::string g_rules_load_text, g_rules_error;
void RegisterScreeningRulesSettings();
void RegisterLanguageSettings();
bool g_prepare_screening = false, g_confirm_screening = false;
roomcats::ScreeningKind g_prepare_screening_kind = roomcats::ScreeningKind::Newborn;
bool g_screening_submitted = false;
std::uint64_t CurrentRuleFingerprint(roomcats::ScreeningKind kind) {
    return kind==roomcats::ScreeningKind::Adult ? roomcats::RuleFingerprint(g_screening_rules.adult) : roomcats::RuleFingerprint(g_screening_rules.newborn);
}
roomcats::RoomSnapshot ScreeningSnapshot(const roomcats::RoomSnapshot& meta);
bool InScreeningReview() {
    return g_view.screening || std::any_of(g_view_history.begin(),g_view_history.end(),[](const ListView& v) { return v.screening; });
}
bool RequestListBack();
void ResetListViews();
struct TraitHover {
    std::uint64_t cat = 0;
    int group = 0;
    double last_hover = 0;
    ImVec2 source_min{}, source_max{};
    roomcats::MouseRect bounds{};
    bool reset_scroll = false;
} g_trait_hover;
roomcats::MouseRect g_panel_rect{};
}
