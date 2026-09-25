#include "../src/mouse_router.hpp"
#include <cassert>
#include <iostream>

using roomcats::MouseOwner;
using roomcats::MouseRouter;
constexpr auto List = MouseOwner::List;
constexpr auto Game = MouseOwner::Game;

MouseRouter Panel() {
    MouseRouter r;
    r.SetPanel(true, {100, 100, 500, 400});
    return r;
}

int main() {
    auto r = Panel();
    r.Move(200, 200);
    assert(r.CapturesPointer() && r.Wheel() == List);
    r.Move(600, 200);
    assert(!r.CapturesPointer() && r.Wheel() == Game);
    r.Move(100, 100);
    assert(r.CapturesPointer());
    r.Move(500, 400);
    assert(!r.CapturesPointer());

    // Scrollbar, title-bar and resize drags remain in the list outside its bounds.
    for (unsigned button : {1u, 2u, 4u, 8u, 16u}) {
        r = Panel();
        r.Move(200, 200);
        assert(r.Down(button) == List);
        r.Move(-100, -100, false);
        assert(r.CapturesPointer() && r.Wheel() == List);
        assert(r.Up(button) == List);
        assert(!r.CapturesPointer() && r.ListButtons() == 0);
    }

    // Dropping a cat after crossing the list still delivers the game's release.
    r = Panel();
    r.Move(600, 200);
    assert(r.Down(1) == Game);
    r.Move(200, 200);
    assert(!r.CapturesPointer() && r.Wheel() == Game);
    assert(r.Up(1) == Game);
    assert(!r.HasButtons()); // A normal OS ReleaseCapture must preserve this handoff.
    assert(!r.CapturesPointer());
    r.EndFrame();
    assert(r.CapturesPointer());

    // Both X buttons and simultaneous buttons retain their own up events.
    r = Panel();
    r.Move(200, 200);
    assert(r.Down(8) == List);
    r.Move(600, 200);
    assert(r.Down(16) == List);
    assert(r.Up(8) == List && r.CapturesPointer());
    assert(r.Up(16) == List && !r.CapturesPointer());
    r = Panel();
    r.Move(600, 200);
    assert(r.Down(1) == Game);
    r.Move(200, 200);
    assert(r.Down(2) == Game);
    assert(r.Up(1) == Game);
    r.EndFrame();
    assert(!r.CapturesPointer());
    assert(r.Up(2) == Game);
    r.EndFrame();
    assert(r.CapturesPointer());

    // Closing mid-drag must not leak the matching up to the game.
    r = Panel();
    r.Move(200, 200);
    assert(r.Down(1) == List);
    r.SetPanel(false, {});
    assert(r.Up(1) == List);
    assert(!r.CapturesPointer());
    assert(r.Down(1) == Game && r.Up(1) == Game);

    // Focus/capture loss and moving/resizing the panel without mouse motion.
    r = Panel();
    r.Move(200, 200);
    r.Down(1);
    r.Cancel();
    assert(!r.CapturesPointer() && r.ListButtons() == 0);
    r.Move(200, 200);
    assert(r.CapturesPointer());
    r.SetPanel(true, {300, 300, 700, 600});
    assert(!r.CapturesPointer());
    r.SetPanel(true, {});
    assert(!r.CapturesPointer());
    r.SetPanel(true, {100, 100, 500, 400});
    r.SetTooltip({600, 100, 900, 500});
    r.Move(700, 200);
    assert(r.CapturesPointer() && r.Wheel() == List);
    r.Move(550, 200);
    assert(!r.CapturesPointer()); // Do not block the gap between panel and tooltip.
    r.SetTooltip({});
    r.Move(700, 200);
    assert(!r.CapturesPointer());
    r.SetPopupOpen(true);
    assert(r.CapturesPointer() && r.Down(1) == List); // Outside click dismisses the dropdown, not the game.
    r.SetPopupOpen(false);
    assert(r.Up(1) == List && !r.CapturesPointer());
    std::cout << "Mouse routing passed: hover, wheel, five buttons, boundary crossing, release, close and focus loss.\n";
}
