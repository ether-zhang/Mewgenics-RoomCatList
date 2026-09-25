#pragma once

namespace roomcats {
enum class MouseOwner { Game, List };

struct MouseRect {
    int left = 0, top = 0, right = 0, bottom = 0;
    bool Contains(int x, int y) const noexcept {
        return left < right && top < bottom &&
            x >= left && x < right && y >= top && y < bottom;
    }
};

// One gesture belongs to the side on which its first button was pressed.
// Bounds alone are insufficient: moving a held button across the panel must
// neither start a second drag nor strand the original side without an up.
class MouseRouter {
public:
    void SetPanel(bool open, MouseRect bounds) noexcept { open_ = open; bounds_ = bounds; }
    void SetTooltip(MouseRect bounds) noexcept { tooltip_ = bounds; }
    void SetPopupOpen(bool open) noexcept { popup_open_ = open; }
    bool PopupOpen() const noexcept { return open_ && popup_open_; }
    void Move(int x, int y, bool in_client = true) noexcept { x_ = x; y_ = y; in_client_ = in_client; }
    bool OverPanel() const noexcept {
        return open_ && in_client_ && (bounds_.Contains(x_, y_) || tooltip_.Contains(x_, y_));
    }
    bool CapturesPointer() const noexcept {
        if (game_buttons_ || game_release_pending_) return false;
        return list_buttons_ || PopupOpen() || OverPanel();
    }
    MouseOwner Down(unsigned button) noexcept {
        const auto owner = CapturesPointer() ? MouseOwner::List : MouseOwner::Game;
        (owner == MouseOwner::List ? list_buttons_ : game_buttons_) |= button;
        return owner;
    }
    MouseOwner Up(unsigned button) noexcept {
        if (list_buttons_ & button) {
            list_buttons_ &= ~button;
            return MouseOwner::List;
        }
        if (game_buttons_ & button) {
            game_buttons_ &= ~button;
            // Let the game consume the release at its real position before
            // transferring hover to a panel the game drag ended over.
            game_release_pending_ = true;
            return MouseOwner::Game;
        }
        return CapturesPointer() ? MouseOwner::List : MouseOwner::Game;
    }
    MouseOwner Wheel() const noexcept { return CapturesPointer() ? MouseOwner::List : MouseOwner::Game; }
    unsigned ListButtons() const noexcept { return list_buttons_; }
    unsigned PhysicalButtons() const noexcept { return (list_buttons_ | game_buttons_) & 31u; }
    int X() const noexcept { return x_; }
    int Y() const noexcept { return y_; }
    bool HasButtons() const noexcept { return list_buttons_ || game_buttons_; }
    void EndFrame() noexcept { game_release_pending_ = false; }
    void Cancel() noexcept {
        list_buttons_ = game_buttons_ = 0;
        game_release_pending_ = false;
        in_client_ = false;
    }
private:
    MouseRect bounds_{};
    MouseRect tooltip_{};
    int x_ = 0, y_ = 0;
    unsigned list_buttons_ = 0, game_buttons_ = 0;
    bool open_ = false, in_client_ = false, game_release_pending_ = false;
    bool popup_open_ = false;
};
}
