#include "localization.hpp"
#include "cat_actions.hpp"
#include <algorithm>
#include <mutex>
#include <optional>

namespace roomcats {
namespace {
std::mutex g_action_lock;
std::optional<CatAction> g_action;
std::optional<CatActionResult> g_action_result;
bool g_action_executing = false;
std::uint64_t g_action_epoch = 0;
QueryCatNpcs g_query_npcs = nullptr;
StepCatAction g_action_step = nullptr;
const Cat* FindActionCat(const RoomSnapshot& snapshot, std::uint64_t id) {
    for (const auto& cat : snapshot.cats) if (cat.id == id) return &cat;
    return nullptr;
}
}

void ConfigureCatActions(QueryCatNpcs query, StepCatAction step) {
    std::lock_guard<std::mutex> lock(g_action_lock);
    ++g_action_epoch;
    g_action.reset(); g_action_result.reset(); g_action_executing = false;
    g_query_npcs = query; g_action_step = step;
}

std::vector<NpcChoice> CatNpcChoices(const RoomSnapshot& snapshot, std::uint64_t id) {
    const auto* cat = FindActionCat(snapshot, id);
    return snapshot.valid && snapshot.in_house && cat && !cat->record_only && g_query_npcs ? g_query_npcs(snapshot, *cat) : std::vector<NpcChoice>{};
}

bool QueueCatAction(const RoomSnapshot& snapshot, std::uint64_t id, CatActionKind kind, int npc, const std::string& marker) {
    const auto* cat = FindActionCat(snapshot, id);
    if (!snapshot.valid || !snapshot.in_house || !cat || cat->record_only || !g_action_step) return false;
    if (kind == CatActionKind::Mark && marker!="triangle" && marker!="square" && marker!="circle" && marker!="sword") return false;
    if (kind == CatActionKind::Donate) {
        const auto choices = CatNpcChoices(snapshot, id);
        if (std::none_of(choices.begin(), choices.end(), [npc](const NpcChoice& c) { return c.npc == npc && c.enabled; })) return false;
    }
    std::lock_guard<std::mutex> lock(g_action_lock);
    if (g_action || g_action_executing) return false;
    CatAction action;
    action.kind = kind; action.npc = npc;
    action.marker = marker;
    action.scene = snapshot.scene; action.scene_generation = snapshot.generation;
    action.cat = id; action.component = cat->component; action.cat_generation = cat->generation;
    action.source = cat->location;
    g_action = action;
    return true;
}

bool HasPendingCatAction() {
    std::lock_guard<std::mutex> lock(g_action_lock);
    return g_action.has_value() || g_action_executing;
}
bool CatActionNeedsCats() {
    std::lock_guard<std::mutex> lock(g_action_lock);
    return g_action && g_action->phase == 0;
}

void ProcessCatAction(const RoomSnapshot& snapshot, std::uint64_t now) {
    CatAction action;
    std::uint64_t epoch;
    {
        std::lock_guard<std::mutex> lock(g_action_lock);
        if (!g_action) return;
        action = *g_action; epoch = g_action_epoch;
        g_action.reset(); g_action_executing = true;
    }
    const bool first = action.phase == 0;
    if (first) action.started = now;
    auto status = CatActionStatus::Failed;
    CatActionResult result{false, false, action.cat, roomcats::Tx("场景或猫咪已变化，操作已取消")};
    try {
        const auto* cat = first ? FindActionCat(snapshot, action.cat) : nullptr;
        const bool same_cat = !first || (cat && !cat->record_only && cat->component == action.component && cat->generation == action.cat_generation && cat->location == action.source);
        if (snapshot.valid && snapshot.in_house && snapshot.scene == action.scene && snapshot.generation == action.scene_generation && same_cat && g_action_step) {
            if (!first && now - action.started > 30000) result.message = roomcats::Tx("投送等待已停止，请在原生界面确认当前状态");
            else status = g_action_step(snapshot, action, result.message);
        }
    } catch (...) { result.message = roomcats::Tx("本次操作中断，未自动重试"); }
    result.success = status != CatActionStatus::Failed;
    result.completed = status != CatActionStatus::Running;
    result.hide_list = false;
    std::lock_guard<std::mutex> lock(g_action_lock);
    g_action_executing = false;
    if (epoch != g_action_epoch) return;
    if (status == CatActionStatus::Running) g_action = action;
    if (first || status != CatActionStatus::Running) g_action_result = std::move(result);
}

bool TakeCatActionResult(CatActionResult& result) {
    std::lock_guard<std::mutex> lock(g_action_lock);
    if (!g_action_result) return false;
    result = std::move(*g_action_result); g_action_result.reset();
    return true;
}

void CancelCatAction() {
    std::lock_guard<std::mutex> lock(g_action_lock);
    ++g_action_epoch;
    if (g_action) {
        const bool handed_off = g_action->phase == 2;
        g_action_result = CatActionResult{handed_off, false, g_action->cat, handed_off ?
            roomcats::Tx("投送操作已交由原生界面处理") : roomcats::Tx("场景已变化，后续操作已取消")};
        g_action_result->completed = true;
    }
    g_action.reset();
}
}
