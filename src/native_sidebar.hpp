#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "mouse_router.hpp"

namespace roomcats {
using SidebarClick = void (*)();
void BootstrapNativeAssets(HMODULE module);
bool StartNativeSidebar(SidebarClick click, void (*draw_list)(), MouseRouter& mouse, bool (*refresh_pointer)());
bool ReadNativeVirtualPointer(float& x, float& y);
void SetNativeListMouseCapture(bool capture);
void StopNativeSidebar();
}
