#include "localization.hpp"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <gl/GL.h>
#include "game_reader.hpp"
#include "font_glyph_cache.hpp"
#include "native_sidebar.hpp"
#include "mouse_router.hpp"
#include "cat_sort.hpp"
#include "cat_moves.hpp"
#include "cat_actions.hpp"
#include "gamepad_input.hpp"
#include "gamepad_router.hpp"
#include "marker_icons.hpp"
#include "list_ui_state.hpp"
#include "imgui.h"
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_opengl3.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <string>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

namespace {
using SwapFunction = BOOL(WINAPI*)(HDC);
HMODULE g_module = nullptr;
SwapFunction g_original_swap = nullptr;
void** g_swap_slot = nullptr;
WNDPROC g_original_wndproc = nullptr;
HWND g_window = nullptr;
HGLRC g_context = nullptr;
bool g_initialized = false;
bool g_failed = false;
bool g_escape_up = false;
bool g_rendered_since_swap = false;
bool g_was_open = false;
std::string g_ini_path;
std::string g_last_newborn_message;
std::array<bool, 256> g_list_keys{}, g_game_keys{};

std::atomic<bool> g_native_toggle_requested{false};
ULONGLONG g_last_poll = 0;
roomcats::FontGlyphCache g_font_glyphs;

constexpr auto kUiText = "猫咪列表 全屋猫咪 "
    "无可遗弃，存在调房：只猫咪。请点击右上角“确定处理”后执行调房。悬停下方“调房”数量可查看名单。 "
    "无可遗弃，无需调房；有只猫咪需要打标，点击“确定处理”后执行。也无需调房或打标。本轮处理进度见下方。 "
    "筛选设置 新生猫 老猫 恢复默认 保存并关闭 取消 可填范围 至 需要在 之间 "
    "排序：属性 > 优质变异 > 有无职业。房子标记始终保护；小猫年龄=1，老猫年龄>1。 "
    "修改只影响新生成的方案；保存后仍须复查并确认。数值可直接输入，也可点击加减按钮。 "
    "二楼新生每房最多可设6只；对面出生数量需小于等于每房上限。 "
    "遗传门槛需满足：顶楼 ≥ 二楼 ≥ 一楼左 对面出生数量不能超过二楼每房保留上限 战备真实下限 新生名额 "
    "重新复查 已保留原选择：此NPC当前不可用。请等待或手动修改去向。 默认优先送至第一个可选NPC 已恢复处理去向 "
    "新生猫筛选复查 确定处理 停止处理 返回名单 处理方式 筛选原因 不遗弃 原地保留 打标 调房 优质变异 → "
    "其他调整 老猫数量控制 预计总数 房子保护 舒适度 顶楼 二楼左 二楼右 一楼左 年龄 遗传门槛 允许轻度近亲 不参与筛选 "
    "保留限制或手动选择使目标未达成；确认将执行当前选择。目标舒适度： 坏变异/疾病转备战；房子标记保留； "
    "等待投送管道复位 等待投送界面收起 "
    "一楼左较弱亲本转备战近亲按轻中重偏科指至少两项真实值 确认前不修改猫咪悬停上方数量可查看调房摘要 没有需要遗弃的猫咪确认后执行打标和调房 步 ≥ ≤ "
    "职业 白色基线 总属性按七项平均 返回上一级 的父母 点击查看父母表格 不在家园 资料未找到 已故 此猫不在家园无法移动或投送无法打开家园详情 "
    "选择去向 遗弃 正在遗弃 已遗弃 手柄 左摇杆移动光标 点击 返回 右摇杆滚动 A B "
    "父母 近亲 无 轻 中 重 一楼左 一楼右 二楼左 二楼右 异 同 双 "
    "送至 选择 NPC 暂无已解锁角色 性别 取向 公 母 无性别 异性恋 同性恋 双性恋 未知 点击打开猫咪界面 "
    "比尼斯博士 布奇 汀可 弗兰克 杰克宝宝 特蕾茜 风琴师 史蒂文 "
    "猫咪或家园界面暂不可用 尚未解锁 此角色不接收猫咪 当前不接收此猫（条件或接收状态不符） 投送管道正在使用中 "
    "请先关闭当前原生面板 投送状态已变化请查看原生界面 猫咪已离开投送管道后续操作已取消 NPC 面板已关闭 "
    "已送至 正在送至 原生投送中断未自动重试 猫咪界面暂未打开请重试 已打开猫咪界面 未能进入投送管道 "
    "场景或猫咪已变化操作已取消 投送等待已停止请在原生界面确认当前状态 本次操作中断未自动重试 "
    "投送操作已交由原生界面处理 "
    "序号 名字 年龄 总属性 力量 敏捷 体质 智力 速度 魅力 幸运 好突变 坏突变 疾病 无 真实 遗传 点击排序 ↑↓ — "
    "点击表头依次切换：真实降序、真实升序、遗传降序、遗传升序。年龄在降序和升序之间切换。 "
    "属性显示为真实(遗传)。详情暂不可用。滚轮上下 · Shift+滚轮左右 · 点击表头排序 · Esc 关闭 "
    "全部突变 固定属性合计 总计 条件效果见左侧 "
    "位置 箱子 未分配 其他位置 当前位置 移动中… 正在移送 已移至 移动功能暂不可用 位置数据已经变化 猫咪正在其他交互中 "
    "箱子暂不可用 箱子已满 此猫当前不能进入箱子 无法读取房间位置 移动未完成请刷新后重试 家园或猫咪的位置已经变化请重新选择 "
    "家园场景已经变化移动已取消 移动中断已停止本次操作 "
    "只猫咪 此房间暂无猫咪。 家园暂无猫咪。 阁楼 一楼大房间 一楼小房间 二楼大房间 二楼小房间 出征区域 未命名猫咪";

void Log(const std::string& message) {
    std::array<wchar_t, 32768> path{};
    const auto length = GetModuleFileNameW(g_module, path.data(), static_cast<DWORD>(path.size()));
    if (!length || length >= path.size()) return;
    auto slash = wcsrchr(path.data(), L'\\');
    if (!slash) return;
    wcscpy_s(slash + 1, path.size() - static_cast<std::size_t>(slash + 1 - path.data()), L"RoomCatList.log");
    HANDLE file = CreateFileW(path.data(), FILE_APPEND_DATA,
        FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return;
    const std::string line = message + "\r\n";
    DWORD written = 0;
    WriteFile(file, line.data(), static_cast<DWORD>(line.size()), &written, nullptr);
    CloseHandle(file);
}

void PublishMouseCapture() {
    g_mouse.SetPanel(g_open && g_has_house && GetForegroundWindow() == g_window,
        {g_panel_rect.left, g_panel_rect.top, g_panel_rect.right, g_panel_rect.bottom});
    roomcats::SetNativeListMouseCapture(g_initialized && !g_failed && g_mouse.CapturesPointer());
}

void UpdateMousePosition(HWND window, POINT point) {
    RECT client{};
    GetClientRect(window, &client);
    g_mouse.Move(point.x, point.y, GetForegroundWindow() == window && PtInRect(&client, point));
    PublishMouseCapture();
}

void PollMousePosition() {
    POINT point{};
    float x = 0, y = 0;
    if (roomcats::ReadNativeVirtualPointer(x,y)) {
        UpdateMousePosition(g_window,{static_cast<LONG>(x),static_cast<LONG>(y)});
    } else if (GetCursorPos(&point) && ScreenToClient(g_window, &point)) UpdateMousePosition(g_window, point);
}

bool RefreshInputPointer() {
    if (!g_initialized || g_failed) return false;
    PollMousePosition();
    return g_open && g_has_house && GetForegroundWindow() == g_window;
}

void CancelMouseInput(HWND window) {
    roomcats::CancelGamepadInput();
    g_mouse.Cancel();
    for (int i = 0; i < 5; ++i) ImGui::GetIO().AddMouseButtonEvent(i,false);
    PublishMouseCapture();
}

LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    if (!g_initialized || g_failed) return CallWindowProcW(g_original_wndproc, window, message, wparam, lparam);
    if (message == WM_CLOSE && !g_ini_path.empty()) ImGui::SaveIniSettingsToDisk(g_ini_path.c_str());
    if (message == WM_KEYDOWN && wparam == VK_ESCAPE && ((g_open && g_has_house) || g_escape_up)) {
        if (!g_escape_up) {
            if (g_mouse.PopupOpen()) g_close_move_popup = true;
            else if (!RequestListBack()) { g_open = false; g_panel_rect = {}; PublishMouseCapture(); }
        }
        g_escape_up = true;
        return 0;
    }
    if (message == WM_KEYUP && wparam == VK_ESCAPE && g_escape_up) {
        g_escape_up = false;
        return 0;
    }
    if ((message == WM_KEYDOWN || message == WM_KEYUP) && wparam < g_list_keys.size()) {
        const auto key = static_cast<std::size_t>(wparam);
        const bool down = message == WM_KEYDOWN;
        const bool owned = g_list_keys[key] || (down && g_mouse.PopupOpen() && !g_game_keys[key]);
        if (owned) {
            g_list_keys[key] = down;
            ImGui_ImplWin32_WndProcHandler(window, message, wparam, lparam);
            return 0;
        }
        g_game_keys[key] = down;
    }
    if (message == WM_CHAR && g_mouse.PopupOpen()) {
        ImGui_ImplWin32_WndProcHandler(window, message, wparam, lparam);
        return 0;
    }
    unsigned button = 0;
    bool down = false, up = false;
    switch (message) {
        case WM_LBUTTONDOWN: case WM_LBUTTONDBLCLK: button=1; down=true; break;
        case WM_LBUTTONUP: button=1; up=true; break;
        case WM_RBUTTONDOWN: case WM_RBUTTONDBLCLK: button=2; down=true; break;
        case WM_RBUTTONUP: button=2; up=true; break;
        case WM_MBUTTONDOWN: case WM_MBUTTONDBLCLK: button=4; down=true; break;
        case WM_MBUTTONUP: button=4; up=true; break;
        case WM_XBUTTONDOWN: case WM_XBUTTONDBLCLK:
            button=GET_XBUTTON_WPARAM(wparam) == XBUTTON1 ? 8 : 16; down=true; break;
        case WM_XBUTTONUP:
            button=GET_XBUTTON_WPARAM(wparam) == XBUTTON1 ? 8 : 16; up=true; break;
        case WM_KILLFOCUS:
            g_escape_up = false;
            g_list_keys.fill(false); g_game_keys.fill(false);
            CancelMouseInput(window);
            break;
        case WM_CAPTURECHANGED:
            // Normal ReleaseCapture after a game's up must not erase the
            // deferred handoff before the game has processed that release.
            if (reinterpret_cast<HWND>(lparam) != window && g_mouse.PhysicalButtons()) CancelMouseInput(window);
            break;
        case WM_CANCELMODE:
            CancelMouseInput(window);
            break;
        case WM_MOUSELEAVE: case WM_NCMOUSELEAVE:
            PollMousePosition();
            break;
    }
    const bool wheel = message == WM_MOUSEWHEEL || message == WM_MOUSEHWHEEL;
    if (button || wheel || message == WM_MOUSEMOVE || message == WM_NCMOUSEMOVE) {
        POINT point{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
        if (wheel || message == WM_NCMOUSEMOVE) ScreenToClient(window, &point);
        UpdateMousePosition(window, point);
        if (button || wheel) {
            const auto owner = down ? g_mouse.Down(button) : up ? g_mouse.Up(button) : g_mouse.Wheel();
            PublishMouseCapture();
            if (owner == roomcats::MouseOwner::List) {
                // A button/wheel can arrive without a preceding motion message.
                ImGui::GetIO().AddMousePosEvent(static_cast<float>(point.x), static_cast<float>(point.y));
                if (button) {
                    // Merge physical and virtual left buttons before ImGui sees
                    // an edge. Neither source can prematurely end the other.
                    if (down && !GetCapture()) SetCapture(window);
                    const int index = button == 1 ? 0 : button == 2 ? 1 : button == 4 ? 2 : button == 8 ? 3 : 4;
                    ImGui::GetIO().AddMouseButtonEvent(index,index == 0 ? roomcats::ListLeftButtonDown(g_mouse) : down);
                    if (up && !(g_mouse.ListButtons() & 31u) && GetCapture() == window) ReleaseCapture();
                } else ImGui_ImplWin32_WndProcHandler(window, message, wparam, lparam);
                return message == WM_XBUTTONDOWN || message == WM_XBUTTONDBLCLK || message == WM_XBUTTONUP ? TRUE : 0;
            }
            // Do not give the list a game's down/up or let ImGui take/release
            // Windows capture for a drag that started on a cat or furniture.
            return CallWindowProcW(g_original_wndproc, window, message, wparam, lparam);
        }
    }
    ImGui_ImplWin32_WndProcHandler(window, message, wparam, lparam);
    // Keep the physical cursor current. The native mouse-position hook filters
    // the engine's hit tests while the list owns the pointer, including polling.
    return CallWindowProcW(g_original_wndproc, window, message, wparam, lparam);
}

bool UpdateFont() {
    if (!g_font_scan_pending) return true;
    g_font_scan_pending = false;
    if (g_font_glyphs.CharacterCount() == 0) {
        g_font_glyphs.AddBasicLatin();
        g_font_glyphs.AddText(kUiText);
        g_font_glyphs.AddText("简体中文 语言");
        for (const auto* label : roomcats::kCatColumnLabels) g_font_glyphs.AddText(label);
        for (const auto& icon : kClassIcons) g_font_glyphs.AddText(icon.label);
        roomcats::VisitScreeningRules(g_screening_rules,[&](const auto&,const char* label,const auto&,int,int) { g_font_glyphs.AddText(label); });
    }
    g_font_glyphs.AddText(g_snapshot.room_id.c_str());
    g_font_glyphs.AddText(g_snapshot.error.c_str());
    g_font_glyphs.AddText(g_move_feedback.c_str());
    g_font_glyphs.AddText(g_view.child_name.c_str());
    if(g_screening_plan) {
        for(const auto& d:g_screening_plan->decisions) { g_font_glyphs.AddText(d.cat.name.c_str()); g_font_glyphs.AddText(d.reason.c_str()); }
        for(const auto& note:g_screening_plan->notes) g_font_glyphs.AddText(note.c_str());
    }
    for (const auto& location : g_snapshot.locations) g_font_glyphs.AddText(location.label.c_str());
    for (const auto& cat : g_snapshot.cats) {
        g_font_glyphs.AddText(cat.name.c_str());
        g_font_glyphs.AddText(cat.location_label.c_str());
        g_font_glyphs.AddText(cat.details.marker.c_str());
        g_font_glyphs.AddText(roomcats::ClassLabel(cat.details.collar).c_str());
        for (const auto& parent : cat.parents.names) g_font_glyphs.AddText(parent.c_str());
        for (const auto* group : {&cat.details.good, &cat.details.bad, &cat.details.diseases})
            for (const auto& trait : *group) {
                g_font_glyphs.AddText(roomcats::TraitName(trait).c_str());
                g_font_glyphs.AddText(roomcats::TraitEffect(trait).c_str());
            }
    }
    if (!g_font_glyphs.UpdateRanges()) return true;
    auto& io = ImGui::GetIO();
    ImGui_ImplOpenGL3_DestroyFontsTexture();
    io.Fonts->Clear();
    std::array<char, MAX_PATH> windows_path{};
    GetWindowsDirectoryA(windows_path.data(), static_cast<UINT>(windows_path.size()));
    const auto font_path=(std::filesystem::u8path(g_ini_path).parent_path()/"assets"/"native-ui.ttf").u8string();
    std::error_code font_error;
    if(!std::filesystem::is_regular_file(std::filesystem::u8path(font_path),font_error)) {
        Log("Native UI font cache is missing; apply the DLL and assets together."); return false;
    }
    ImFontConfig config;
    config.OversampleH = 2;
    config.OversampleV = 1;
    config.FontNo = 0;
    auto* font = io.Fonts->AddFontFromFileTTF(font_path.c_str(), 22.0f, &config, g_font_glyphs.Ranges());
    if (!font) {
        Log("Could not load cached native UI font; run tools/build_ui_theme.py before installing.");
        return false;
    }
    // Native TikaFont/TikaFontCN first; retain rare Unicode-name coverage.
    config.MergeMode=true;
    const std::string fallback=std::string(windows_path.data())+"\\Fonts\\msyh.ttc";
    io.Fonts->AddFontFromFileTTF(fallback.c_str(),22.0f,&config,g_font_glyphs.Ranges());
    roomcats::BuildMarkerGlyphs(*io.Fonts, font);
    return ImGui_ImplOpenGL3_CreateFontsTexture();
}

void NativeSidebarClicked() {
    g_native_toggle_requested.store(true, std::memory_order_release);
}

void DrawListBeforeCursor();

bool Initialize(HDC dc) {
    std::string reason;
    if (!roomcats::ValidateGameBuild(reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr)), reason)) {
        Log(reason);
        return false;
    }
    g_window = WindowFromDC(dc);
    g_context = wglGetCurrentContext();
    if (!g_window || !g_context) return false;
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    auto& io = ImGui::GetIO();
    std::array<wchar_t, 32768> module_path{};
    const auto path_length = GetModuleFileNameW(g_module, module_path.data(), static_cast<DWORD>(module_path.size()));
    if (!path_length || path_length >= module_path.size()) return false;
    g_ini_path = (std::filesystem::path(module_path.data()).parent_path() / L"ui-state.ini").u8string();
    io.IniFilename = g_ini_path.c_str();
    RegisterScreeningRulesSettings();
    RegisterLanguageSettings();
    io.LogFilename = nullptr;
    // The list is drawn before the engine's cursor pass. Keep that single
    // native cursor above the panel, including the title bar and scrollbar.
    io.MouseDrawCursor = false;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NoMouseCursorChange;
    io.ConfigFlags &= ~ImGuiConfigFlags_NavEnableGamepad;
    roomcats::ui::Apply();
    if (!ImGui_ImplWin32_InitForOpenGL(g_window) || !ImGui_ImplOpenGL3_Init("#version 330")) {
        Log("Could not initialize the in-game OpenGL UI.");
        return false;
    }
    if (!UpdateFont()) return false;
    SetLastError(0);
    const auto previous = SetWindowLongPtrW(g_window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&WindowProc));
    if (!previous && GetLastError() != 0) {
        Log("Could not attach the mod's mouse handler.");
        return false;
    }
    g_original_wndproc = reinterpret_cast<WNDPROC>(previous);
    g_initialized = true;
    // Native buttons are scene-owned and share the engine's input/render order.
    const bool native_started = roomcats::StartNativeSidebar(NativeSidebarClicked, DrawListBeforeCursor, g_mouse, RefreshInputPointer);
    if (!native_started)
        Log("Native sidebar could not start. See mod_logs/chainloader.log for the native-UI diagnostic.");
    Log(std::string("RoomCatList 0.15.3 ready; native sidebar/input startup=") + (native_started ? "yes" : "no") + "; English/Chinese list text; native paper/font skin and sidebar artwork.");
    return native_started;
}

void ApplyRoomSnapshot(roomcats::RoomSnapshot next, bool force, double read_ms);

void PollRoom(bool force) {
    const auto now = GetTickCount64();
    if (!force && !g_view_refresh_requested && now - g_last_poll < 250) return;
    g_last_poll = now;
    const auto read_start = std::chrono::steady_clock::now();
    const auto image = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    auto next = g_view.screening ? ScreeningSnapshot(roomcats::ReadFocusedRoom(image,false)) : g_view.IsParents() ? roomcats::ReadParentCats(image,g_view.ids,g_view.names) :
        roomcats::ReadFocusedRoom(image,g_open || force);
    const double read_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - read_start).count();
    ApplyRoomSnapshot(std::move(next), force, read_ms);
}

}

#include "list_ui.inl"

namespace {
void HandleScreeningRequests() {
    const auto image=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    if(g_prepare_screening) {
        g_prepare_screening=false;
        if(roomcats::NewbornBatchActive() || roomcats::HasPendingCatMove() || roomcats::HasPendingCatAction()) g_move_feedback=roomcats::Tx("请先等待当前操作完成");
        else {
            auto source=roomcats::ReadFocusedRoom(image,true,true);
            if(g_prepare_screening_kind==roomcats::ScreeningKind::Adult) roomcats::ReadPopulationComfort(source);
            const auto previous=roomcats::GetNewbornBatchStatus();
            if(previous.finished && previous.kind==g_prepare_screening_kind && previous.rules_fingerprint==CurrentRuleFingerprint(g_prepare_screening_kind) && previous.post_fingerprint==roomcats::ScreeningFingerprint(source,g_prepare_screening_kind)) g_move_feedback=roomcats::Tx("当前状态已处理，无需重复执行");
            else {
                auto plan=g_prepare_screening_kind==roomcats::ScreeningKind::Adult ? roomcats::PlanAdultCats(source,GetTickCount64(),g_screening_rules.adult) : roomcats::PlanNewbornCats(source,GetTickCount64(),g_screening_rules.newborn);
                if(!plan.Valid()) g_move_feedback=plan.error;
                else {
                    const int restored=g_screening_choices.Prepare(plan);
                    g_screening_plan=std::move(plan); g_screening_submitted=false;
                    if(!g_view.screening) { ListView view; view.screening=true; g_pending_parent_view=std::move(view); ApplyQueuedListView(); }
                    else { g_view_refresh_requested=true; g_reset_list_scroll=true; g_sort_dirty=true; ClearMovePopup(); ClearTraitHover(); }
                    ApplyRoomSnapshot(ScreeningSnapshot(source),false,0);
                    g_move_feedback=restored ? roomcats::Tx("已恢复 ")+std::to_string(restored)+roomcats::Tx(" 只猫的处理去向，请复查后确认") : roomcats::Tx("默认优先送至第一个可选NPC；请复查后确认");
                }
            }
        }
        g_font_scan_pending=true;
    }
    if(g_confirm_screening) {
        g_confirm_screening=false;
        if(g_screening_plan && !g_screening_submitted) {
            auto fresh=roomcats::ReadFocusedRoom(image,true,true);
            if(g_screening_plan->kind==roomcats::ScreeningKind::Adult) roomcats::ReadPopulationComfort(fresh);
            std::string reason;
            if(roomcats::BeginNewbornBatch(*g_screening_plan,fresh,reason)) g_screening_submitted=true;
            else g_move_feedback=reason;
            g_font_scan_pending=true;
        }
    }
    const auto progress=roomcats::GetNewbornBatchStatus();
    const auto message=progress.message.empty() ? std::string{} :
        std::string(progress.kind==roomcats::ScreeningKind::Adult ? roomcats::Tx("老猫控制 ") : roomcats::Tx("新生筛选 "))+std::to_string(progress.completed)+"/"+std::to_string(progress.total)+roomcats::Tx(" 步：")+progress.message;
    if(!message.empty() && message!=g_last_newborn_message) {
        g_last_newborn_message=message; g_move_feedback=message; g_font_scan_pending=true;
        Log("Newborn batch: running="+std::string(progress.running ? "yes" : "no")+"; complete="+
            std::to_string(progress.completed)+"/"+std::to_string(progress.total)+"; cat="+std::to_string(progress.cat)+"; "+roomcats::NewbornBatchDiagnostic());
    }
}

void DrawListBeforeCursor() {
    if (g_initialized && !g_failed && !g_rendered_since_swap &&
        wglGetCurrentContext() == g_context && WindowFromDC(wglGetCurrentDC()) == g_window) {
        g_rendered_since_swap = true;
        try {
            roomcats::MoveResult move_result;
            if (!roomcats::NewbornBatchActive() && roomcats::TakeCatMoveResult(move_result)) {
                g_move_feedback = move_result.message;
                g_font_scan_pending = true;
                g_last_poll = 0;
                Log("Cat move: id=" + std::to_string(move_result.cat) + "; success=" + (move_result.success ? "yes" : "no") + "; " + move_result.message);
            }
            roomcats::CatActionResult action_result;
            if (!roomcats::NewbornBatchActive() && roomcats::TakeCatActionResult(action_result)) {
                g_move_feedback = action_result.message;
                if (action_result.hide_list) g_open = false;
                g_font_scan_pending = true;
                g_last_poll = 0;
                Log("Cat action: id=" + std::to_string(action_result.cat) + "; success=" + (action_result.success ? "yes" : "no") + "; " + action_result.message);
            }
            PollRoom(false);
            HandleScreeningRequests();
            if (g_native_toggle_requested.exchange(false, std::memory_order_acq_rel) && g_has_house) {
                g_open = !g_open;
                if (g_open) PollRoom(true);
            }
            if (!UpdateFont()) g_failed = true;
            else {
                PollMousePosition();
                // Route individual events, not a frame-wide NoMouse flag: a
                // quick list click/drag may already have ended outside while
                // ImGui is still draining the matching queued down/up events.
                ImGui_ImplOpenGL3_NewFrame();
                ImGui_ImplWin32_NewFrame();
                // Win32 reports the stationary OS cursor when a controller is
                // moving the game's virtual pointer. Override it before input.
                PollMousePosition();
                ImGui::GetIO().AddMousePosEvent(static_cast<float>(g_mouse.X()),static_cast<float>(g_mouse.Y()));
                g_pad_frame = roomcats::UpdateGamepadInput(g_open && g_has_house && GetForegroundWindow() == g_window);
                const bool popup_before_frame = g_mouse.PopupOpen();
                ImGui::NewFrame();
                HandleGamepadBack(popup_before_frame);
                if (g_has_house) DrawUi();
                else {
                    g_panel_rect = {};
                    ClearMovePopup();
                    ClearTraitHover();
                    ImGui::GetIO().MouseDrawCursor = false;
                }
                ImGui::Render();
                if((g_rules_save_requested || g_language_save_requested) && !g_ini_path.empty()) {
                    ImGui::SaveIniSettingsToDisk(g_ini_path.c_str());
                    g_rules_save_requested=g_language_save_requested=false;
                }
                ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            }
        } catch (...) {
            g_open = false;
            g_failed = true;
            Log("Stopped the mod UI after an unexpected error.");
        }
        if (g_failed) { g_open = false; CancelMouseInput(g_window); }
        if (g_was_open && !g_open && !g_ini_path.empty()) ImGui::SaveIniSettingsToDisk(g_ini_path.c_str());
        g_was_open = g_open;
        PublishMouseCapture();
    }
}

BOOL WINAPI SwapBuffersHook(HDC dc) {
    if (!g_failed && !g_initialized && wglGetCurrentContext()) {
        try { if (!Initialize(dc)) g_failed = true; }
        catch (...) { g_failed = true; Log("UI initialization failed."); }
    }
    if (g_initialized && wglGetCurrentContext() == g_context && WindowFromDC(dc) == g_window) {
        // House input has now consumed any game-owned release. The next frame
        // may transfer hover back to the list even when the mouse stays still.
        g_mouse.EndFrame();
        PublishMouseCapture();
        g_rendered_since_swap = false;
    }
    return g_original_swap(dc);
}

bool InstallSwapImport() {
    // Redirect just this executable's existing SwapBuffers import. There is
    // no remote-process injection and no rewriting of game code or assets.
    auto* image = reinterpret_cast<unsigned char*>(GetModuleHandleW(nullptr));
    const auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(image);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return false;
    const auto* nt = reinterpret_cast<IMAGE_NT_HEADERS64*>(image + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return false;
    const auto import_rva = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
    if (!import_rva) return false;
    auto* descriptor = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(image + import_rva);
    for (; descriptor->Name; ++descriptor) {
        if (_stricmp(reinterpret_cast<char*>(image + descriptor->Name), "GDI32.dll") != 0 || !descriptor->OriginalFirstThunk) continue;
        auto* names = reinterpret_cast<IMAGE_THUNK_DATA64*>(image + descriptor->OriginalFirstThunk);
        auto* slots = reinterpret_cast<IMAGE_THUNK_DATA64*>(image + descriptor->FirstThunk);
        for (; names->u1.AddressOfData; ++names, ++slots) {
            if (IMAGE_SNAP_BY_ORDINAL64(names->u1.Ordinal)) continue;
            const auto* imported = reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(image + names->u1.AddressOfData);
            if (std::strcmp(reinterpret_cast<const char*>(imported->Name), "SwapBuffers")) continue;
            DWORD previous = 0;
            g_swap_slot = reinterpret_cast<void**>(&slots->u1.Function);
            if (!VirtualProtect(g_swap_slot, sizeof(void*), PAGE_READWRITE, &previous)) return false;
            g_original_swap = reinterpret_cast<SwapFunction>(InterlockedExchangePointer(g_swap_slot, reinterpret_cast<void*>(&SwapBuffersHook)));
            DWORD unused = 0;
            VirtualProtect(g_swap_slot, sizeof(void*), previous, &unused);
            return g_original_swap != nullptr;
        }
    }
    return false;
}
}

BOOL WINAPI DllMain(HMODULE module, DWORD reason, LPVOID reserved) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_module = module;
        DisableThreadLibraryCalls(module);
        // All font, UI, file and window work is deferred to the render thread.
        roomcats::BootstrapNativeAssets(module);
        InstallSwapImport();
    } else if (reason == DLL_PROCESS_DETACH && reserved == nullptr && g_swap_slot && g_original_swap) {
        roomcats::StopNativeSidebar();
        DWORD previous = 0;
        if (VirtualProtect(g_swap_slot, sizeof(void*), PAGE_READWRITE, &previous)) {
            InterlockedCompareExchangePointer(g_swap_slot, reinterpret_cast<void*>(g_original_swap), reinterpret_cast<void*>(&SwapBuffersHook));
            DWORD unused = 0;
            VirtualProtect(g_swap_slot, sizeof(void*), previous, &unused);
        }
        if (g_original_wndproc && IsWindow(g_window) && GetWindowLongPtrW(g_window, GWLP_WNDPROC) == reinterpret_cast<LONG_PTR>(&WindowProc))
            SetWindowLongPtrW(g_window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(g_original_wndproc));
    }
    return TRUE;
}
