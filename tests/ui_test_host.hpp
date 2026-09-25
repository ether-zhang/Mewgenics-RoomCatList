#pragma once
// Headless tests share production UI only. No Win32/SDL/native-game adapters.
#include "../src/list_ui_state.hpp"
namespace {
void Log(const std::string&) {}
void PublishMouseCapture() { g_mouse.SetPanel(g_open && g_has_house,g_panel_rect); }
}
#include "../src/list_ui.inl"
