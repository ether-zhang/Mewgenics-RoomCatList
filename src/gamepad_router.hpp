#pragma once
#include "mouse_router.hpp"
#include <array>
#include <cstdint>
#include <vector>

namespace roomcats {
// A virtual left click shares physical mouse routing with a separate bit.
inline constexpr unsigned kGamepadMouseButton = 1u << 5;
inline bool ListLeftButtonDown(const MouseRouter& mouse) {
    return (mouse.ListButtons() & (1u | kGamepadMouseButton)) != 0;
}
class GamepadRouter {
public:
    struct Click { bool down; int x, y; };
private:
    enum Owner { Game, List, Blocked };
    struct Control { bool held = false; Owner owner = Game; };
    std::array<Control,32> buttons_{};
    std::vector<Click> clicks_;
    bool active_ = false, back_ = false, pointer_registered_ = false;
    void Observe(Control& c, bool held, bool capture) {
        if (!held) { c = {}; return; }
        if (!c.held) { c.held = true; c.owner = capture ? List : Game; }
    }
public:
    void SetActive(bool active, MouseRouter& mouse) {
        if (active == active_) return;
        if (pointer_registered_) {
            mouse.Up(kGamepadMouseButton);
            if (buttons_[0].owner == List) clicks_.push_back({false,mouse.X(),mouse.Y()});
            pointer_registered_ = false;
        }
        for (auto& c : buttons_) if (c.held) c.owner = Blocked;
        active_ = active;
        back_ = false;
    }
    bool ButtonForGame(unsigned index, bool down, MouseRouter& mouse) {
        if (index >= buttons_.size()) return down;
        auto& c = buttons_[index];
        if (index == 0) {
            if (down && !c.held) {
                const auto owner = mouse.Down(kGamepadMouseButton);
                c = {true, active_ && owner == MouseOwner::List ? List : Game};
                pointer_registered_ = true;
                if (c.owner == List) clicks_.push_back({true,mouse.X(),mouse.Y()});
            } else if (!down && c.held) {
                if (pointer_registered_) mouse.Up(kGamepadMouseButton);
                if (c.owner == List) clicks_.push_back({false,mouse.X(),mouse.Y()});
                pointer_registered_ = false;
                c = {};
            }
        } else {
            const bool was_held = c.held;
            Observe(c, down, active_ && mouse.CapturesPointer());
            if (down && !was_held && c.owner == List && index == 1) back_ = true;
        }
        return down && c.owner == Game;
    }
    std::int16_t AxisForGame(unsigned index, std::int16_t value, const MouseRouter& mouse) const {
        // VirtualMouse must keep receiving the native left stick unchanged.
        return index >= 2 && index < 6 && active_ && mouse.CapturesPointer() ? 0 : value;
    }
    std::int16_t AxisForList(unsigned index, std::int16_t value, const MouseRouter& mouse) const {
        return index >= 2 && index < 4 && active_ && mouse.CapturesPointer() ? value : 0;
    }
    void PrimeButton(unsigned index, bool down) {
        if (index < buttons_.size()) buttons_[index] = {down,Blocked};
    }
    std::vector<Click> TakeClicks() { auto result = std::move(clicks_); clicks_.clear(); return result; }
    bool TakeBack() { const bool result = back_; back_ = false; return result; }
    void Reset(MouseRouter& mouse) {
        SetActive(false,mouse);
        if (pointer_registered_) mouse.Up(kGamepadMouseButton);
        pointer_registered_ = false;
        buttons_ = {};
    }
};
}
