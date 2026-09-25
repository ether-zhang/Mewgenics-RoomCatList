#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "gamepad_input.hpp"
#include "gamepad_router.hpp"
#include "imgui.h"
#include <algorithm>
#include <atomic>
#include <mutex>

namespace roomcats {
namespace {
using ButtonFn = bool (__cdecl*)(void*,int);
using AxisFn = std::int16_t (__cdecl*)(void*,int);
using EnumerateFn = std::uint32_t* (__cdecl*)(int*);
using FindFn = void* (__cdecl*)(std::uint32_t);
using FreeFn = void (__cdecl*)(void*);
ButtonFn g_original_button = nullptr;
AxisFn g_original_axis = nullptr;
EnumerateFn g_enumerate = nullptr;
FindFn g_find = nullptr;
FreeFn g_free = nullptr;
std::atomic<void*> g_primary{nullptr};
std::atomic<void*> g_recent{nullptr};
MouseRouter* g_mouse = nullptr;
bool (*g_refresh_pointer)() = nullptr;
std::mutex g_pad_lock;
GamepadRouter g_router;
std::uint32_t g_device = 0;
ULONGLONG g_next_scan = 0;
bool g_ready = false;

bool __cdecl ButtonHook(void* pad, int button) {
    const bool down = g_original_button(pad,button);
    if (down) g_recent.store(pad,std::memory_order_relaxed);
    if (!g_ready || !g_mouse || !g_refresh_pointer) return down;
    const bool active = g_refresh_pointer();
    if (pad != g_primary.load(std::memory_order_relaxed)) return active && g_mouse->CapturesPointer() ? false : down;
    std::lock_guard<std::mutex> lock(g_pad_lock);
    g_router.SetActive(active,*g_mouse);
    return g_router.ButtonForGame(button,down,*g_mouse);
}
std::int16_t __cdecl AxisHook(void* pad, int axis) {
    const auto value = g_original_axis(pad,axis);
    if (std::abs(int(value)) > 8000) g_recent.store(pad,std::memory_order_relaxed);
    if (!g_ready || !g_mouse || !g_refresh_pointer) return value;
    const bool active = g_refresh_pointer();
    if (pad != g_primary.load(std::memory_order_relaxed))
        return axis >= 2 && active && g_mouse->CapturesPointer() ? 0 : value;
    std::lock_guard<std::mutex> lock(g_pad_lock);
    g_router.SetActive(active,*g_mouse);
    return g_router.AxisForGame(axis,value,*g_mouse);
}
float Normalize(std::int16_t value) {
    const int magnitude = std::abs(int(value));
    if (magnitude <= 8000) return 0;
    return (value < 0 ? -1.0f : 1.0f)*std::min(1.0f,float(magnitude-8000)/24767.0f);
}
}

bool StartGamepadInput(const MewjectorAPI& api, MouseRouter& mouse, bool (*refresh_pointer)()) {
    g_mouse = &mouse;
    g_refresh_pointer = refresh_pointer;
    const auto game = GetModuleHandleW(nullptr);
    g_enumerate = reinterpret_cast<EnumerateFn>(GetProcAddress(game,"SDL_GetGamepads"));
    g_find = reinterpret_cast<FindFn>(GetProcAddress(game,"SDL_GetGamepadFromID"));
    g_free = reinterpret_cast<FreeFn>(GetProcAddress(game,"SDL_free"));
    if (!g_enumerate || !g_find || !g_free) return false;
    void* next = nullptr;
    if (!api.InstallHook(0xBBEF00,15,reinterpret_cast<void*>(&ButtonHook),&next,30,"RoomCatList gamepad") || !next) return false;
    g_original_button = reinterpret_cast<ButtonFn>(next);
    next = nullptr;
    if (!api.InstallHook(0xBBEAE0,16,reinterpret_cast<void*>(&AxisHook),&next,30,"RoomCatList gamepad") || !next) return false;
    g_original_axis = reinterpret_cast<AxisFn>(next);
    g_ready = true;
    return true;
}

GamepadFrame UpdateGamepadInput(bool capture) {
    GamepadFrame frame;
    auto& io = ImGui::GetIO();
    if (!g_ready || !g_mouse) return frame;
    void* pad = g_ready && g_device ? g_find(g_device) : nullptr;
    const auto now = GetTickCount64();
    const auto preferred = g_recent.load(std::memory_order_relaxed);
    if (g_ready && ((!pad && now >= g_next_scan) || (preferred && preferred != pad))) {
        g_next_scan = now+500;
        int count = 0;
        auto* ids = g_enumerate(&count);
        if (ids) {
            for (int i=0;i<std::min(count,16);++i)
                if (auto* candidate = g_find(ids[i])) {
                    if (!pad || candidate == preferred) { g_device = ids[i]; pad = candidate; }
                    if (candidate == preferred) break;
                }
            g_free(ids);
        }
        if (preferred && preferred != pad) g_recent.store(pad,std::memory_order_relaxed);
    }
    std::array<bool,15> buttons{};
    std::array<std::int16_t,6> axes{};
    if (pad) {
        // Use original SDL getters, never the filtered game-facing hooks.
        for (int i=0;i<int(buttons.size());++i) buttons[i] = g_original_button(pad,i);
        for (int i=0;i<int(axes.size());++i) axes[i] = g_original_axis(pad,i);
    }
    {
        std::lock_guard<std::mutex> lock(g_pad_lock);
        if (pad != g_primary.load(std::memory_order_relaxed)) {
            g_router.Reset(*g_mouse);
            // Prime with the physical state before granting UI focus.
            for (int i=0;i<int(buttons.size());++i) g_router.PrimeButton(i,buttons[i]);
            g_primary.store(pad,std::memory_order_relaxed);
        }
        g_router.SetActive(capture && pad,*g_mouse);
        for (int i=0;i<int(buttons.size());++i) g_router.ButtonForGame(i,buttons[i],*g_mouse);
        for (int i=0;i<int(axes.size());++i) axes[i] = g_router.AxisForList(i,axes[i],*g_mouse);
        for (const auto& click : g_router.TakeClicks()) {
            io.AddMousePosEvent(static_cast<float>(click.x),static_cast<float>(click.y));
            io.AddMouseButtonEvent(0,click.down || (g_mouse->ListButtons() & 1u));
        }
        frame.back_pressed = g_router.TakeBack();
    }
    frame.connected = pad != nullptr;
    // Confirmation is a mouse click at the engine pointer, never a nav key.
    io.BackendFlags &= ~ImGuiBackendFlags_HasGamepad;
    frame.scroll_x = Normalize(axes[2]); frame.scroll_y = Normalize(axes[3]);
    return frame;
}

void StopGamepadInput() {
    g_primary.store(nullptr,std::memory_order_relaxed);
    g_recent.store(nullptr,std::memory_order_relaxed);
    std::lock_guard<std::mutex> lock(g_pad_lock);
    if (g_mouse) g_router.Reset(*g_mouse);
    g_mouse = nullptr; g_refresh_pointer = nullptr; g_ready = false; g_device = 0;
}

void CancelGamepadInput() {
    std::lock_guard<std::mutex> lock(g_pad_lock);
    if (g_mouse) g_router.SetActive(false,*g_mouse);
}
}
