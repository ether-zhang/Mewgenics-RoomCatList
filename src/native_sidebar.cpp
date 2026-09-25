#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "native_sidebar.hpp"
#include "native_args.hpp"
#include "native_signatures.generated.hpp"
#include "game_reader.hpp"
#include "native_moves.hpp"
#include "native_actions.hpp"
#include "cat_actions.hpp"
#include "cat_moves.hpp"
#include "gamepad_input.hpp"
#include "newborn_batch.hpp"
#include <cmath>
#include <intrin.h>
#include "mew_ui_api.h"
#include <array>
#include <atomic>
#include <cstring>
#include <string>
#include <vector>

namespace roomcats {
namespace {
constexpr const char* kOwner = "RoomCatList";
using ProcessArgs = std::intptr_t(__fastcall*)(void*, int, const char**);
MewjectorAPI g_api{};
ProcessArgs g_next_args = nullptr;
std::string g_mod_folder;
std::string g_game_folder;
std::atomic<bool> g_assets_registered{false};
bool g_bootstrap_ready = false;
SidebarClick g_click = nullptr;
using MousePosition = double* (__fastcall*)(void*, double*);
using DrawCursor = void (__fastcall*)(void*);
MousePosition g_next_mouse_position = nullptr;
DrawCursor g_next_draw_cursor = nullptr;
void (*g_draw_list)() = nullptr;
bool (*g_refresh_pointer)() = nullptr;
std::atomic<bool> g_list_mouse_capture{false};
MewUISceneBinding g_house{};
void* g_button = nullptr;
void* g_hud = nullptr;
void* g_hud_root = nullptr;
std::uint32_t g_generation = 0;
bool g_button_attempted = false;
unsigned g_node_retries = 0;
ULONGLONG g_retry_after = 0;

template<class T> bool Read(const void* object, std::size_t offset, T& output) {
    return ReadBytes(reinterpret_cast<std::uintptr_t>(object) + offset, &output, sizeof(output));
}
bool VerifyNativeCode() {
    const auto image = g_api.GetGameBase();
    for (const auto& check : kRoomCatNativeSignatures) {
        // A shared Mewjector hook may already own a function's entry bytes.
        // The six independent game-layout checks still guard the build.
        if (g_api.QueryHook(check.rva) > 0) continue;
        std::array<unsigned char, 16> bytes{};
        if (!ReadBytes(image + check.rva, bytes.data(), bytes.size()) ||
            std::memcmp(bytes.data(), check.bytes, bytes.size()) != 0) {
            g_api.Log(kOwner, "Native UI signature mismatch: %s", check.name);
            return false;
        }
    }
    std::string reason;
    if (!ValidateGameBuild(image, reason)) {
        g_api.Log(kOwner, "%s", reason.c_str());
        return false;
    }
    return true;
}

double* __fastcall MousePositionHook(void* controls, double* point) {
    auto* result = g_next_mouse_position(controls, point);
    // VirtualMouse uses these reads for motion and its saved return position.
    // Keep both real; hiding hit-test coordinates must not warp the cursor.
    const auto caller = reinterpret_cast<std::uintptr_t>(_ReturnAddress());
    const auto caller_rva = caller - g_api.GetGameBase();
    if (caller_rva == 0x74DA8D || caller_rva == 0x74E6DE) return result;
    if (g_refresh_pointer) g_refresh_pointer();
    // Filter the engine's hit-test coordinates, not its physical mouse cache.
    // Cursor rendering uses the real pointer independently. Leaving the list
    // therefore restores hover immediately, without warping or freezing it.
    if (g_list_mouse_capture.load(std::memory_order_relaxed))
        result[0] = result[1] = -1000000.0;
    return result;
}

void __fastcall DrawCursorHook(void* cursor) {
    // This is the engine's final cursor pass, after the house render has
    // completed. The list belongs below that pass, rather than at SwapBuffers
    // where it would cover the game's software cursor.
    if (g_draw_list) g_draw_list();
    g_next_draw_cursor(cursor);
}

struct NativeMutationVector {
    std::uint32_t capacity, size;
    std::uint64_t* data;
};
static_assert(sizeof(NativeMutationVector) == 16);

bool RunNativeCatQueries(std::uintptr_t cat, Stats* real, NativeMutationVector* mutations) noexcept {
    // Match CatData's own house-info UI: seven -1 default overrides, enabled
    // equipment/passive adjustments and the house display's age option.
    // The mutation vector belongs to the engine's frame arena. Copy it in the
    // caller immediately; never retain or free its storage with the mod CRT.
    using Calculate = Stats* (__fastcall*)(void*, Stats*, const Stats*, std::uint8_t, std::uint8_t);
    using Mutations = NativeMutationVector* (__fastcall*)(void*, NativeMutationVector*);
    alignas(16) const Stats defaults{-1, -1, -1, -1, -1, -1, -1};
    __try {
        const auto image = g_api.GetGameBase();
        reinterpret_cast<Calculate>(image + 0xC1820)(reinterpret_cast<void*>(cat), real, &defaults, 1, 1);
        reinterpret_cast<Mutations>(image + 0xCB690)(reinterpret_cast<void*>(cat), mutations);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

bool ReadNativeCatDetails(std::uintptr_t cat, NativeCatDetails& details) {
    NativeMutationVector mutations{};
    if (!RunNativeCatQueries(cat, &details.real, &mutations) ||
        mutations.size > mutations.capacity || mutations.size > 32) return false;
    details.mutation_keys.resize(mutations.size);
    return !mutations.size || ReadBytes(reinterpret_cast<std::uintptr_t>(mutations.data),
        details.mutation_keys.data(), mutations.size * sizeof(std::uint64_t));
}

std::uintptr_t LoadNativeParentCat(std::uintptr_t manager, std::uint64_t id) noexcept {
    using GetCat = std::uintptr_t (__fastcall*)(void*, std::uint64_t);
    __try {
        return reinterpret_cast<GetCat>(g_api.GetGameBase() + 0xD7220)(reinterpret_cast<void*>(manager), id);
    } __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
}

std::uintptr_t ReadNativeRoomEffects(std::uintptr_t room) noexcept {
    using GetEffects=std::uintptr_t (__fastcall*)(std::uintptr_t);
    __try { return reinterpret_cast<GetEffects>(g_api.GetGameBase()+0x2EABE0)(room); }
    __except(EXCEPTION_EXECUTE_HANDLER) { return 0; }
}

std::intptr_t __fastcall ProcessArgsHook(void* application, int count, const char** argv) {
    if (count < 1 || count > 4096 || !argv) return g_next_args(application, count, argv);
    std::vector<std::string> args;
    try {
        std::vector<std::string> original;
        original.reserve(count);
        for (int i = 0; i < count; ++i) original.emplace_back(argv[i] ? argv[i] : "");
        args = AddOwnModPath(original, g_mod_folder, g_game_folder);
    } catch (...) {
        g_api.Log(kOwner, "Could not prepare the resource path; preserving original startup arguments.");
        return g_next_args(application, count, argv);
    }
    std::vector<const char*> pointers;
    pointers.reserve(args.size());
    for (const auto& value : args) pointers.push_back(value.c_str());
    const auto result = g_next_args(application, static_cast<int>(pointers.size()), pointers.data());
    g_assets_registered = true;
    g_api.Log(kOwner, "Native sidebar resource path registered through the game's mod loader.");
    return result;
}

void ResetSidebar() {
    g_button = nullptr;
    g_hud = nullptr;
    g_hud_root = nullptr;
    g_button_attempted = false;
    g_node_retries = 0;
    g_retry_after = 0;
}

void __cdecl SceneChanged(MewUISceneBinding*, MewUISceneRefreshResult state, void*, void*, void*) {
    if (state != MEW_UI_SCENE_REFRESH_UNCHANGED) { ResetSidebar(); CancelCatMove(); CancelCatAction(); }
}

void __cdecl ButtonEvent(void* button, MewButtonEvent event, MewButtonState, MewButtonState, void*) {
    if (event == MEW_BUTTON_EVENT_CLICK && button == g_button && g_click) g_click();
}

void __cdecl UiTick(void*) {
    MewUI_RefreshSceneBinding(&g_house);
    auto* scene = MewUI_GetSceneBindingScene(&g_house);
    // Validate the confirmed batch's scene/day before executing a queued step.
    if(NewbornBatchActive()) {
        const bool cats=NewbornBatchNeedsCats();
        auto snapshot=ReadFocusedRoom(g_api.GetGameBase(),cats,true);
        if(cats && GetNewbornBatchStatus().kind==ScreeningKind::Adult) ReadPopulationComfort(snapshot);
        TickNewbornBatch(snapshot,GetTickCount64());
    }
    TickNativeCatMoves(reinterpret_cast<std::uintptr_t>(scene));
    TickNativeCatActions(reinterpret_cast<std::uintptr_t>(scene));
    if (!scene || !MewUI_IsSceneReadyForUITick(scene)) return;
    const auto generation = MewUI_GetSceneBindingGeneration(&g_house);
    if (generation != g_generation) { ResetSidebar(); g_generation = generation; }
    if (!g_hud) {
        if (GetTickCount64() < g_retry_after) return;
        g_retry_after = GetTickCount64() + 250;
        g_hud = reinterpret_cast<void*>(FindHouseHudRenderer(g_api.GetGameBase(), reinterpret_cast<std::uintptr_t>(scene)));
        if (!g_hud) return; // House may still be constructing its HUD.
    }
    void* root = nullptr;
    if (!Read(g_hud, 0x80, root) || !root) return;
    if (g_hud_root && root != g_hud_root) {
        if (g_button && MewUI_IsComponentInScene(scene, g_button)) MewUI_SetButtonEnabled(g_button, 0);
        g_button = nullptr;
        g_button_attempted = false;
        g_node_retries = 0;
    }
    g_hud_root = root;
    if (!g_button && !g_button_attempted) {
        if (GetTickCount64() < g_retry_after) return;
        g_retry_after = GetTickCount64() + 100;
        auto* node = MewUI_FindChildByName(g_hud, "rcl_button");
        if (!node) {
            if (++g_node_retries >= 30) {
                g_button_attempted = true;
                g_api.Log(kOwner, "Native sidebar sprite did not expose rcl_button after initialization.");
            }
            return;
        }
        g_button_attempted = true;
        MewButtonCreateInfo info{};
        info.scene_manager = scene;
        info.root_node = g_hud;
        info.button_node = node;
        info.node_name = "rcl_button";
        info.role_name = "RCL_Sidebar";
        info.enabled = 1;
        info.activate_enabled = 1;
        info.strict_mouse = 0;
        info.interact_override = MEW_BUTTON_INTERACT_GAME_DEFAULT;
        info.callback = ButtonEvent;
        // No label and no tooltip: this is the same native Button component
        // and input path used by the game's icon-only sidebar controls.
        g_button = MewUI_CreateButtonFromNode(&info);
        if (!g_button) { g_api.Log(kOwner, "Native Button creation failed."); return; }
        g_api.Log(kOwner, "Native sidebar Button attached to the House HUD; generation=%u.", generation);
    }
}
}

void BootstrapNativeAssets(HMODULE module) {
    if (!MJ_Resolve(&g_api) || !VerifyNativeCode()) return;
    std::array<wchar_t, 32768> path{};
    const auto length = GetModuleFileNameW(module, path.data(), static_cast<DWORD>(path.size()));
    if (!length || length >= path.size()) return;
    auto* slash = wcsrchr(path.data(), L'\\');
    if (!slash) return;
    *slash = 0;
    const int size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, path.data(), -1, nullptr, 0, nullptr, nullptr);
    if (size < 2) return;
    g_mod_folder.resize(size);
    WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, path.data(), -1, g_mod_folder.data(), size, nullptr, nullptr);
    g_mod_folder.pop_back();
    const auto game_length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    if (!game_length || game_length >= path.size()) return;
    slash = wcsrchr(path.data(), L'\\');
    if (!slash) return;
    *slash = 0;
    const int game_size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, path.data(), -1, nullptr, 0, nullptr, nullptr);
    if (game_size < 2) return;
    g_game_folder.resize(game_size);
    WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, path.data(), -1, g_game_folder.data(), game_size, nullptr, nullptr);
    g_game_folder.pop_back();
    void* next = nullptr;
    if (!g_api.InstallHook(0x9B8BB0, 15, reinterpret_cast<void*>(&ProcessArgsHook), &next, 25, kOwner) || !next) {
        g_api.Log(kOwner, "Could not register the native sidebar resource bootstrap.");
        return;
    }
    g_next_args = reinterpret_cast<ProcessArgs>(next);
    g_bootstrap_ready = true;
}

bool StartNativeSidebar(SidebarClick click, void (*draw_list)(), MouseRouter& mouse, bool (*refresh_pointer)()) {
    if (!g_bootstrap_ready) return false;
    if (!g_assets_registered) {
        g_api.Log(kOwner, "Resource bootstrap did not run before UI startup; native sidebar disabled.");
        return false;
    }
    HMODULE pinned = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
        reinterpret_cast<LPCWSTR>(&StartNativeSidebar), &pinned);
    void* next_position = nullptr;
    void* next_cursor = nullptr;
    if (!g_api.InstallHook(0x986EE0, 15, reinterpret_cast<void*>(&MousePositionHook), &next_position, 30, kOwner) || !next_position)
        return false;
    g_next_mouse_position = reinterpret_cast<MousePosition>(next_position);
    if (!g_api.InstallHook(0xA20710, 15, reinterpret_cast<void*>(&DrawCursorHook), &next_cursor, 30, kOwner) || !next_cursor)
        return false;
    g_next_draw_cursor = reinterpret_cast<DrawCursor>(next_cursor);
    g_click = click;
    g_draw_list = draw_list;
    g_refresh_pointer = refresh_pointer;
    ConfigureCatDetailsReader(ReadNativeCatDetails);
    ConfigureParentCatLoader(LoadNativeParentCat);
    ConfigureRoomEffectsReader(ReadNativeRoomEffects);
    StartNativeCatMoves(g_api.GetGameBase());
    StartNativeCatActions(g_api.GetGameBase());
    g_api.Log(kOwner, "SDL gamepad pointer bridge: %s", StartGamepadInput(g_api,mouse,refresh_pointer) ? "ready" : "unavailable");
    g_api.Log(kOwner, "List input uses engine hit-test coordinates; list renders before the native cursor pass.");
    MewUI_InitSceneBinding(&g_house, "House", SceneChanged, nullptr);
    MewUI_SetDebugLogsEnabled(false);
    return MewUI_Start(kOwner, 30, 100, 16, UiTick, nullptr) != 0;
}

void SetNativeListMouseCapture(bool capture) {
    g_list_mouse_capture.store(capture, std::memory_order_relaxed);
}

bool ReadNativeVirtualPointer(float& x, float& y) {
    if (!g_bootstrap_ready) return false;
    const auto base = g_api.GetGameBase();
    unsigned char active = 0;
    double point[2]{};
    if (!ReadBytes(base+0x13C1839,&active,sizeof(active)) || !active ||
        !ReadBytes(base+0x12FBE80,point,sizeof(point)) ||
        !std::isfinite(point[0]) || !std::isfinite(point[1]) ||
        std::abs(point[0]) > 100000 || std::abs(point[1]) > 100000) return false;
    // These are the same top-left client pixels cached by SDL mouse input.
    x = static_cast<float>(point[0]); y = static_cast<float>(point[1]);
    return true;
}

void StopNativeSidebar() {
    StopNativeCatMoves();
    StopNativeCatActions();
    StopGamepadInput();
    g_click = nullptr;
    g_draw_list = nullptr;
    g_refresh_pointer = nullptr;
    SetNativeListMouseCapture(false);
    ConfigureCatDetailsReader(nullptr);
    ConfigureParentCatLoader(nullptr);
    ConfigureRoomEffectsReader(nullptr);
    ResetSidebar();
    MewUI_ClearSceneBinding(&g_house);
    MewUI_Stop();
}
}
