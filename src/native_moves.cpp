#include "localization.hpp"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "native_moves.hpp"
#include "cat_moves.hpp"
#include "mew_ui_api.h"
#include <array>
#include <cmath>
#include <cstring>

namespace roomcats {
namespace {
std::uintptr_t g_image = 0;
struct RoomBounds { double left, right, bottom, top; };
using BoundsFn = RoomBounds* (__fastcall*)(void*, RoomBounds*);
using AddCatFn = void (__fastcall*)(void*, void*);
using BoxAcceptFn = std::uint8_t (__fastcall*)(void*, void*);
BoundsFn g_bounds = nullptr;
AddCatFn g_add_cat = nullptr;
BoxAcceptFn g_box_accept = nullptr;

template<class T> bool Read(std::uintptr_t address, T& value) { return ReadBytes(address, &value, sizeof(value)); }

bool ReadBounds(std::uintptr_t room, RoomBounds& bounds) noexcept {
    __try {
        g_bounds(reinterpret_cast<void*>(room), &bounds);
        return std::isfinite(bounds.left) && std::isfinite(bounds.right) &&
            std::isfinite(bounds.bottom) && std::isfinite(bounds.top) &&
            std::abs(bounds.left) < 100000 && std::abs(bounds.right) < 100000 &&
            std::abs(bounds.bottom) < 100000 && std::abs(bounds.top) < 100000 &&
            bounds.right - bounds.left > 1 && bounds.top - bounds.bottom > 2;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

bool BoxAccepts(std::uintptr_t box, std::uintptr_t cat) noexcept {
    __try {
        return g_box_accept(reinterpret_cast<void*>(box), reinterpret_cast<void*>(cat)) != 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

bool CheckMove(const RoomSnapshot& snapshot, const Cat& cat, const CatLocation& target, std::string& reason) {
    reason = roomcats::Tx("位置数据已经变化");
    if (!g_image || !MewUI_IsSceneReadyForUITick(reinterpret_cast<void*>(snapshot.scene)) ||
        !MewUI_IsComponentInScene(reinterpret_cast<void*>(snapshot.scene), reinterpret_cast<void*>(cat.component)) ||
        !MewUI_IsComponentInScene(reinterpret_cast<void*>(snapshot.scene), reinterpret_cast<void*>(target.component)) ||
        !ComponentHasType(cat.component, g_image, ".?AVHouseCat@glaiel@@")) return false;
    std::uint64_t id = 0, generation = 0;
    std::uintptr_t source = 0, transform = 0;
    if (!Read(cat.component + 0x80, id) || id != cat.id || !Read(cat.component + 0xE8, source) || source != cat.location ||
        !Read(target.component - 8, generation) || generation != target.generation ||
        !Read(cat.component + 0x40, transform) || !transform) return false;
    std::array<double, 3> position{};
    if (!Read(transform + 0x80, position)) return false;
    for (double value : position) if (!std::isfinite(value) || std::abs(value) >= 100000) return false;
    // Delivery pipes are not storage/rooms: do not interrupt an NPC handoff.
    if (source && !ComponentHasType(source, g_image, ".?AVFurnitureGrid@glaiel@@") &&
        !ComponentHasType(source, g_image, ".?AVButchBox@glaiel@@")) {
        reason = roomcats::Tx("猫咪正在其他交互中");
        return false;
    }
    if (target.kind == LocationKind::Room) {
        RoomBounds bounds{};
        if (!ComponentHasType(target.component, g_image, ".?AVFurnitureGrid@glaiel@@") || !ReadBounds(target.component, bounds)) return false;
        std::uintptr_t room_transform = 0;
        double z = 0;
        if (!Read(target.component + 0x38, room_transform) || !Read(room_transform + 0x90, z) || !std::isfinite(z) || std::abs(z) >= 100000) return false;
    } else {
        if (!ComponentHasType(target.component, g_image, ".?AVButchBox@glaiel@@")) return false;
        std::uint8_t enabled = 0;
        std::uintptr_t slots_address = 0;
        std::array<std::uintptr_t, 4> slots{};
        if (!Read(target.component + 0xF0, enabled) || !enabled ||
            !Read(target.component + 0x100, slots_address) || !Read(slots_address, slots)) {
            reason = roomcats::Tx("箱子暂不可用");
            return false;
        }
        bool empty = false;
        for (auto slot : slots) empty |= slot == 0;
        if (!empty) { reason = roomcats::Tx("箱子已满"); return false; }
        if (!BoxAccepts(target.component, cat.component)) { reason = roomcats::Tx("此猫当前不能进入箱子"); return false; }
    }
    reason.clear();
    return true;
}

void RestoreRejectedPosition(std::uintptr_t cat, std::uintptr_t old_area, std::uintptr_t transform,
                             const std::array<double, 3>& before) noexcept {
    __try {
        if (transform && *reinterpret_cast<std::uintptr_t*>(cat + 0xE8) == old_area &&
            *reinterpret_cast<std::uintptr_t*>(cat + 0x40) == transform)
            std::memcpy(reinterpret_cast<void*>(transform + 0x80), before.data(), sizeof(before));
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
}

bool CommitMove(std::uintptr_t cat, std::uintptr_t target, bool room, const RoomBounds* bounds) noexcept {
    std::uintptr_t transform = 0, old_area = 0;
    std::array<double, 3> before{};
    bool positioned = false;
    __try {
        transform = *reinterpret_cast<std::uintptr_t*>(cat + 0x40);
        old_area = *reinterpret_cast<std::uintptr_t*>(cat + 0xE8);
        std::memcpy(before.data(), reinterpret_cast<void*>(transform + 0x80), sizeof(before));
        if (room) {
            const auto room_transform = *reinterpret_cast<std::uintptr_t*>(target + 0x38);
            const std::array<double, 3> position{(bounds->left + bounds->right) * 0.5,
                bounds->bottom + 1.5, *reinterpret_cast<double*>(room_transform + 0x90)};
            // Match the game's House loader/drag placement: update the live
            // transform, then use CatPlacementArea::add for membership and
            // both containers' enter/leave callbacks. No CatData/save edits.
            std::memcpy(reinterpret_cast<void*>(transform + 0x80), position.data(), sizeof(position));
            positioned = true;
        }
        // Box's native enter handler chooses its slot and positions the cat.
        g_add_cat(reinterpret_cast<void*>(target), reinterpret_cast<void*>(cat));
        if (*reinterpret_cast<std::uintptr_t*>(cat + 0xE8) == target) return true;
        if (positioned) RestoreRejectedPosition(cat, old_area, transform, before);
        return false;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        if (positioned) RestoreRejectedPosition(cat, old_area, transform, before);
        return false; // Never retry a native operation after an uncertain failure.
    }
}

bool PerformMove(const RoomSnapshot&, const Cat& cat, const CatLocation& target, std::string& reason) {
    RoomBounds bounds{};
    if (target.kind == LocationKind::Room && !ReadBounds(target.component, bounds)) { reason = roomcats::Tx("无法读取房间位置"); return false; }
    if (!CommitMove(cat.component, target.component, target.kind == LocationKind::Room, &bounds)) {
        reason = roomcats::Tx("移动未完成，请刷新后重试");
        return false;
    }
    return true;
}
}

void StartNativeCatMoves(std::uintptr_t image) {
    g_image = image;
    g_bounds = reinterpret_cast<BoundsFn>(image + 0x2EA460);
    g_add_cat = reinterpret_cast<AddCatFn>(image + 0x2E88D0);
    g_box_accept = reinterpret_cast<BoxAcceptFn>(image + 0xAEC00);
    ConfigureCatMoves(CheckMove, PerformMove);
}

void TickNativeCatMoves(std::uintptr_t scene) {
    if (!HasPendingCatMove()) return;
    if (!scene || !MewUI_IsSceneReadyForUITick(reinterpret_cast<void*>(scene))) { CancelCatMove(); return; }
    // Execute on the native House update thread, not inside a table draw loop.
    ProcessCatMove(ReadFocusedRoom(g_image, true, true));
}

void StopNativeCatMoves() {
    ConfigureCatMoves(nullptr, nullptr);
    g_image = 0; g_bounds = nullptr; g_add_cat = nullptr; g_box_accept = nullptr;
}
}
