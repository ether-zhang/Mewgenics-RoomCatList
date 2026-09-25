#include "localization.hpp"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "native_actions.hpp"
#include "cat_actions.hpp"
#include "cat_moves.hpp"
#include "mew_ui_api.h"
#include <array>

namespace roomcats {
namespace {
std::uintptr_t g_action_image = 0;
using AcceptNpcFn = unsigned char (__fastcall*)(void*, int, void*);
using OpenCatFn = void (__fastcall*)(void*, void*, unsigned char);
using PlaceCatFn = void (__fastcall*)(void*, void*);
using NpcClickFn = void (__fastcall*)(void*);
using AssignMarkerFn = void* (__fastcall*)(void*,const char*,std::size_t);
AcceptNpcFn g_accept_npc = nullptr;
OpenCatFn g_open_cat = nullptr;
OpenCatFn g_refresh_cat = nullptr;
AssignMarkerFn g_assign_marker = nullptr;
PlaceCatFn g_place_in_pipe = nullptr;
std::array<NpcClickFn, 7> g_npc_clicks{};
constexpr int kDiscardDestination = 8; // Mod menu ID, not passed to the NPC progress query.
NpcClickFn g_discard_click = nullptr;
const char* DestinationLabel(int id) { return id == kDiscardDestination ? roomcats::Tx("遗弃") : roomcats::NpcLabel(id); }
constexpr std::uintptr_t kNpcClicks[] = {0x27BC20, 0x27BEC0, 0x27B9A0, 0x27C790, 0x27C150, 0x27C320, 0x27C500};

template<class T> T Value(std::uintptr_t p) { T value{}; ReadBytes(p, &value, sizeof(value)); return value; }
std::uintptr_t Ptr(std::uintptr_t p) { return Value<std::uintptr_t>(p); }
bool Member(const RoomSnapshot& s, std::uintptr_t component, const char* type) {
    return component && MewUI_IsComponentInScene(reinterpret_cast<void*>(s.scene), reinterpret_cast<void*>(component)) &&
        ComponentHasType(component, g_action_image, type);
}
bool LiveCat(const RoomSnapshot& s, const CatAction& a) {
    return Member(s, a.component, ".?AVHouseCat@glaiel@@") &&
        Value<std::uint64_t>(a.component - 8) == a.cat_generation && Value<std::uint64_t>(a.component + 0x80) == a.cat;
}
std::uintptr_t Progress() { return Ptr(Ptr(g_action_image + 0x13DAC30) + 0x5A8); }
bool NpcUnlocked(int npc) {
    return npc >= 0 && npc < 8 && Progress() && Value<unsigned char>(Progress() + 0x48 + npc*0x78 + 0x50) != 0;
}
bool AcceptsNpc(int npc, std::uint64_t id) noexcept {
    __try {
        const auto data = FindCachedCatData(g_action_image, id);
        if (!data || Value<std::uint64_t>(data + 0xC48) != id) return false;
        return g_accept_npc(reinterpret_cast<void*>(Progress() + 0x48), npc, reinterpret_cast<void*>(data)) != 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

bool DeliveryAvailable(const RoomSnapshot& s, const CatAction& a, std::string& reason, bool starting, bool* transient = nullptr) {
    if(transient) *transient=false;
    reason = roomcats::Tx("猫咪或家园界面暂不可用");
    if (!g_action_image || !LiveCat(s, a) || !Value<unsigned char>(a.component + 0x88) ||
        !Member(s, s.pipe, ".?AVHousePipe@glaiel@@") || !Member(s, s.npc_drawer, ".?AVNPCMapDrawer@glaiel@@") ||
        !Member(s, s.drawer_ui, ".?AVHouseDrawerUI@glaiel@@")) return false;
    if (a.npc < 0 || a.npc > kDiscardDestination) return false;
    if (a.npc != kDiscardDestination && !NpcUnlocked(a.npc)) { reason = roomcats::Tx("尚未解锁"); return false; }
    // Steven is intentionally disabled by the game's pipe-mode NPC menu,
    // even though the lower-level generic eligibility function returns true.
    if (a.npc == 7) { reason = roomcats::Tx("此角色不接收猫咪"); return false; }
    if (a.npc != kDiscardDestination && !AcceptsNpc(a.npc, a.cat)) { reason = roomcats::Tx("当前不接收此猫（条件或接收状态不符）"); return false; }
    if (starting) {
        const auto source = Ptr(a.component + 0xE8);
        if (source != a.source || (source && !Member(s, source, ".?AVFurnitureGrid@glaiel@@") &&
            !Member(s, source, ".?AVButchBox@glaiel@@"))) { reason = roomcats::Tx("猫咪正在其他交互中"); return false; }
        if (Value<unsigned char>(s.pipe + 0xF4) || Ptr(Ptr(s.pipe + 0x100))) {
            if(transient) *transient=true;
            reason = roomcats::Tx("投送管道正在使用中"); return false;
        }
        // HousePipe::update starts pipe_down only on its cached 0 -> occupied
        // edge. A cleared slot alone is insufficient until an empty update
        // has reset +F0; inserting now loses the next animation/open callback.
        if(Value<int>(s.pipe+0xF0)!=0) {
            if(transient) *transient=true;
            reason=roomcats::Tx("等待投送管道复位"); return false;
        }
        if (!Ptr(s.pipe + 0x100) || !Ptr(s.pipe + 0x110) || !Ptr(s.pipe + 0xE8)) return false;
        // Do not replace another native drawer interaction with a donation.
        const auto open = Ptr(s.drawer_ui + 0x58);
        if (open && open != Ptr(s.cat_drawer + 0x38)) {
            const bool map_closing=open==Ptr(s.npc_drawer+0x38) && !Ptr(s.npc_drawer+0x48);
            if(transient) *transient=map_closing;
            reason=map_closing ? roomcats::Tx("等待投送界面收起") : roomcats::Tx("请先关闭当前原生面板");
            return false;
        }
    }
    reason.clear();
    return true;
}

std::vector<NpcChoice> QueryNpcs(const RoomSnapshot& s, const Cat& cat) {
    std::vector<NpcChoice> choices;
    CatAction a;
    a.cat = cat.id; a.component = cat.component; a.cat_generation = cat.generation; a.source = cat.location;
    for (int npc = 0; npc <= kDiscardDestination; ++npc) {
        if (npc != kDiscardDestination && !NpcUnlocked(npc)) continue;
        a.npc = npc;
        NpcChoice c{npc, DestinationLabel(npc)};
        c.enabled = DeliveryAvailable(s, a, c.reason, true, &c.transient);
        choices.push_back(std::move(c));
    }
    return choices;
}

bool MarkNativeCat(const RoomSnapshot& s,const CatAction& a) noexcept {
    __try {
        if(!g_assign_marker || (a.marker!="triangle" && a.marker!="square" && a.marker!="circle" && a.marker!="sword")) return false;
        const auto data=FindCachedCatData(g_action_image,a.cat);
        const auto director=Ptr(g_action_image+0x13DAC30);
        if(!data || Value<std::uint64_t>(data+0xC48)!=a.cat ||
            Value<int>(director+0x580)-Value<std::int64_t>(data+0xC38)!=1) return false;
        // The same native string assignment used by the marker menu. Using
        // the game's allocator also handles an existing non-inline symbol.
        g_assign_marker(reinterpret_cast<void*>(data+0x38),a.marker.data(),a.marker.size());
        if(g_refresh_cat && Ptr(s.cat_drawer+0x78)==a.component && Ptr(s.drawer_ui+0x58)==Ptr(s.cat_drawer+0x38)) {
            // Match the original marker callback's UI-dirty flag. Without it
            // the drawer deliberately skips refreshing the same selected cat.
            *reinterpret_cast<unsigned char*>(s.cat_drawer+0x70)=1;
            g_refresh_cat(reinterpret_cast<void*>(s.cat_drawer),reinterpret_cast<void*>(a.component),0);
        }
        return true;
    } __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}

bool OpenNativeCat(const RoomSnapshot& s, const CatAction& a) noexcept {
    __try {
        g_open_cat(reinterpret_cast<void*>(s.cat_drawer), reinterpret_cast<void*>(a.component), 1);
        return Ptr(s.cat_drawer + 0x78) == a.component && Ptr(s.drawer_ui + 0x58) == Ptr(s.cat_drawer + 0x38);
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

bool BeginNativeDelivery(const RoomSnapshot& s, CatAction& a) noexcept {
    __try {
        a.pipe = s.pipe; a.map = s.npc_drawer;
        a.pipe_generation = Value<std::uint64_t>(s.pipe - 8);
        a.map_generation = Value<std::uint64_t>(s.npc_drawer - 8);
        // Native placement triggers pipe_down and opens the NPC drawer when
        // its animation finishes. No NPC progress or CatData writes here.
        g_place_in_pipe(reinterpret_cast<void*>(s.pipe), reinterpret_cast<void*>(a.component));
        if (Ptr(a.component + 0xE8) != s.pipe || Ptr(Ptr(s.pipe + 0x100)) != a.component) return false;
        a.phase = 1;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

CatActionStatus AdvanceNativeDelivery(const RoomSnapshot& s, CatAction& a, std::string& message) {
    if (s.pipe != a.pipe || s.npc_drawer != a.map || Value<std::uint64_t>(a.pipe-8) != a.pipe_generation ||
        Value<std::uint64_t>(a.map-8) != a.map_generation) return CatActionStatus::Failed;
    if (a.phase == 2) {
        // Native donation callback sets the target NPC and clears its cat
        // reference only after awarding progress and removing that cat.
        if (!Ptr(a.map + 0x48) && Value<int>(a.map + 0xB8) == a.npc &&
            (!LiveCat(s, a) || !Value<unsigned char>(a.component + 0x88))) {
            message = a.npc == kDiscardDestination ? roomcats::Tx("已遗弃") : std::string(roomcats::Tx("已送至 ")) + DestinationLabel(a.npc);
            return CatActionStatus::Complete;
        }
        if (Ptr(a.map + 0x48) != a.component) { message = roomcats::Tx("投送状态已变化，请查看原生界面"); return CatActionStatus::Failed; }
        return CatActionStatus::Running;
    }
    if (!LiveCat(s, a) || Ptr(a.component + 0xE8) != a.pipe) {
        message = roomcats::Tx("猫咪已离开投送管道，后续操作已取消"); return CatActionStatus::Failed;
    }
    // +0x40 is a one-shot request, cleared at the end of NPCMapDrawer's
    // initialization (0x27B1BA). +0x41 is the persistent pipe/donation mode,
    // also used by the game's own click_<npc> handlers.
    if (Ptr(a.map + 0x48) != a.component) return CatActionStatus::Running;
    if (Value<std::uint64_t>(a.map + 0x50) != a.cat_generation) return CatActionStatus::Failed;
    if (Ptr(s.drawer_ui + 0x58) != Ptr(a.map + 0x38)) {
        message = roomcats::Tx("NPC 面板已关闭，后续操作已取消"); return CatActionStatus::Failed;
    }
    if (!Value<unsigned char>(a.map + 0x41)) return CatActionStatus::Running;
    if (!DeliveryAvailable(s, a, message, false)) return CatActionStatus::Failed;
    a.phase = 2; // Mark before invoking: no repeated click, including exceptions.
    message = a.npc == kDiscardDestination ? roomcats::Tx("正在遗弃…") : std::string(roomcats::Tx("正在送至 ")) + DestinationLabel(a.npc);
    return CatActionStatus::Running;
}

bool ClickNativeNpc(CatAction& a) noexcept {
    __try {
        if (a.npc == kDiscardDestination) {
            // This native entry is the original menu closure, with its map
            // capture at +8. It queues the original trash animation/callback.
            std::array<std::uintptr_t,2> capture{0,a.map};
            g_discard_click(capture.data());
        } else g_npc_clicks[a.npc](reinterpret_cast<void*>(a.map));
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

CatActionStatus StepNativeAction(const RoomSnapshot& s, CatAction& a, std::string& message) {
    if (a.phase) {
        const auto phase = a.phase;
        const auto status = AdvanceNativeDelivery(s, a, message);
        if (phase == 1 && a.phase == 2 && !ClickNativeNpc(a)) {
            message = roomcats::Tx("原生投送中断，未自动重试"); return CatActionStatus::Failed;
        }
        return status;
    }
    if (HasPendingCatMove() || !LiveCat(s, a) || !Value<unsigned char>(a.component + 0x88)) return CatActionStatus::Failed;
    if(a.kind==CatActionKind::Mark) {
        if(!MarkNativeCat(s,a)) { message=roomcats::Tx("新生打标失败或年龄已变化"); return CatActionStatus::Failed; }
        std::string actual;
        if(!ReadNarrowString(FindCachedCatData(g_action_image,a.cat)+0x38,actual) || actual!=a.marker) return CatActionStatus::Failed;
        message=roomcats::Tx("已按出生房间打标"); return CatActionStatus::Complete;
    }
    if (a.kind == CatActionKind::Inspect) {
        if (!Member(s, s.cat_drawer, ".?AVCatStatsDrawer@glaiel@@") || !Member(s, s.drawer_ui, ".?AVHouseDrawerUI@glaiel@@")) return CatActionStatus::Failed;
        const auto open = Ptr(s.drawer_ui + 0x58);
        if (open && open != Ptr(s.cat_drawer + 0x38)) { message = roomcats::Tx("请先关闭当前原生面板"); return CatActionStatus::Failed; }
        if (!OpenNativeCat(s, a)) { message = roomcats::Tx("猫咪界面暂未打开，请重试"); return CatActionStatus::Failed; }
        message = roomcats::Tx("已打开猫咪界面");
        return CatActionStatus::Complete;
    }
    if (!DeliveryAvailable(s, a, message, true)) return CatActionStatus::Failed;
    if (!BeginNativeDelivery(s, a)) { message = roomcats::Tx("未能进入投送管道，未自动重试"); return CatActionStatus::Failed; }
    message = a.npc == kDiscardDestination ? roomcats::Tx("正在遗弃…") : std::string(roomcats::Tx("正在送至 ")) + DestinationLabel(a.npc);
    return CatActionStatus::Running;
}
}

void StartNativeCatActions(std::uintptr_t image) {
    g_action_image = image;
    g_accept_npc = reinterpret_cast<AcceptNpcFn>(image + 0x276600);
    g_open_cat = reinterpret_cast<OpenCatFn>(image + 0xEC7B0);
    g_refresh_cat = reinterpret_cast<OpenCatFn>(image + 0xEBBA0);
    g_assign_marker = reinterpret_cast<AssignMarkerFn>(image + 0x520D0);
    g_place_in_pipe = reinterpret_cast<PlaceCatFn>(image + 0x2E88D0);
    g_discard_click = reinterpret_cast<NpcClickFn>(image + 0x27E1A0);
    for (int i = 0; i < 7; ++i) g_npc_clicks[i] = reinterpret_cast<NpcClickFn>(image + kNpcClicks[i]);
    ConfigureCatActions(QueryNpcs, StepNativeAction);
}
void TickNativeCatActions(std::uintptr_t scene) {
    if (!HasPendingCatAction()) return;
    if (!scene || !MewUI_IsSceneReadyForUITick(reinterpret_cast<void*>(scene))) { CancelCatAction(); return; }
    ProcessCatAction(ReadFocusedRoom(g_action_image, CatActionNeedsCats(), true), GetTickCount64());
}
void StopNativeCatActions() {
    ConfigureCatActions(nullptr, nullptr);
    g_action_image = 0; g_accept_npc = nullptr; g_open_cat = nullptr; g_place_in_pipe = nullptr;
    g_refresh_cat = nullptr; g_assign_marker = nullptr;
    g_discard_click = nullptr;
    g_npc_clicks.fill(nullptr);
}
}
