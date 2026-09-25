#include "localization.hpp"
#include "cat_moves.hpp"
#include <algorithm>
#include <mutex>
#include <optional>

namespace roomcats {
namespace {
struct Request {
    std::uintptr_t scene;
    std::uint64_t generation, cat, target_generation;
    std::string source, destination;
};
std::mutex g_lock;
std::optional<Request> g_request;
std::optional<MoveResult> g_result;
bool g_executing = false;
CheckCatMove g_check = nullptr;
PerformCatMove g_perform = nullptr;
const Cat* FindCat(const RoomSnapshot& snapshot, std::uint64_t id) {
    const auto found = std::find_if(snapshot.cats.begin(), snapshot.cats.end(),
        [id](const Cat& cat) { return cat.id == id; });
    return found == snapshot.cats.end() ? nullptr : &*found;
}
const CatLocation* FindLocation(const RoomSnapshot& snapshot, const std::string& key) {
    const auto found = std::find_if(snapshot.locations.begin(), snapshot.locations.end(),
        [&](const CatLocation& location) { return location.key == key; });
    return found == snapshot.locations.end() ? nullptr : &*found;
}
}

void ConfigureCatMoves(CheckCatMove check, PerformCatMove perform) {
    std::lock_guard<std::mutex> lock(g_lock);
    g_request.reset(); g_result.reset(); g_executing = false;
    g_check = check; g_perform = perform;
}

std::vector<MoveChoice> CatMoveChoices(const RoomSnapshot& snapshot, std::uint64_t id) {
    std::vector<MoveChoice> choices;
    const auto* cat = FindCat(snapshot, id);
    if (!snapshot.valid || !snapshot.in_house || !cat) return choices;
    for (const auto& destination : snapshot.locations) {
        MoveChoice choice;
        choice.destination = destination;
        choice.current = cat->location_key == destination.key;
        if (cat->record_only) choice.reason = roomcats::Tx("此猫不在家园，无法移动");
        else if (choice.current) choice.reason = roomcats::Tx("当前位置");
        else if (g_check && g_perform) choice.enabled = g_check(snapshot, *cat, destination, choice.reason);
        else choice.reason = roomcats::Tx("移动功能暂不可用");
        choices.push_back(std::move(choice));
    }
    return choices;
}

bool QueueCatMove(const RoomSnapshot& snapshot, std::uint64_t id, const std::string& destination) {
    const auto* cat = FindCat(snapshot, id);
    const auto* target = FindLocation(snapshot, destination);
    if (!snapshot.valid || !snapshot.in_house || !cat || cat->record_only || !target || cat->location_key == destination) return false;
    std::lock_guard<std::mutex> lock(g_lock);
    if (g_request || g_executing || !g_check || !g_perform) return false;
    g_request = Request{snapshot.scene, snapshot.generation, id, target->generation, cat->location_key, destination};
    return true;
}

bool HasPendingCatMove() {
    std::lock_guard<std::mutex> lock(g_lock);
    return g_request.has_value() || g_executing;
}

void ProcessCatMove(const RoomSnapshot& fresh) {
    Request request;
    {
        std::lock_guard<std::mutex> lock(g_lock);
        if (!g_request) return;
        request = std::move(*g_request);
        g_request.reset(); g_executing = true;
    }
    MoveResult result{false, request.cat, roomcats::Tx("家园或猫咪的位置已经变化，请重新选择")};
    const auto* cat = FindCat(fresh, request.cat);
    const auto* destination = FindLocation(fresh, request.destination);
    try {
        if (fresh.valid && fresh.in_house && fresh.scene == request.scene && fresh.generation == request.generation &&
            cat && !cat->record_only && destination && destination->generation == request.target_generation && cat->location_key == request.source &&
            g_check && g_perform) {
            result.message.clear();
            if (g_check(fresh, *cat, *destination, result.message))
                result.success = g_perform(fresh, *cat, *destination, result.message);
            if (result.success) result.message = roomcats::Tx("已移至 ") + destination->label;
            else if (result.message.empty()) result.message = roomcats::Tx("移动未完成，请重新选择");
        }
    } catch (...) { result.message = roomcats::Tx("移动中断，已停止本次操作"); }
    std::lock_guard<std::mutex> lock(g_lock);
    g_result = std::move(result);
    g_executing = false;
}

bool TakeCatMoveResult(MoveResult& result) {
    std::lock_guard<std::mutex> lock(g_lock);
    if (!g_result) return false;
    result = std::move(*g_result); g_result.reset();
    return true;
}

void CancelCatMove() {
    std::lock_guard<std::mutex> lock(g_lock);
    if (g_request) g_result = MoveResult{false, g_request->cat, roomcats::Tx("家园场景已经变化，移动已取消")};
    g_request.reset();
}
}
