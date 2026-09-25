#include "../src/gamepad_router.hpp"
#include <iostream>
#define CHECK(x) do { if (!(x)) { std::cerr << "Check failed, line " << __LINE__ << ": " << #x << "\n"; return 1; } } while(false)
int main() {
    using namespace roomcats;
    MouseRouter mouse;
    GamepadRouter pad;
    mouse.Move(20,20);
    CHECK(pad.ButtonForGame(0,true,mouse));
    mouse.SetPanel(true,{100,100,500,500});
    mouse.Move(200,200);
    pad.SetActive(true,mouse);
    CHECK(!pad.ButtonForGame(0,true,mouse) && pad.TakeClicks().empty());
    pad.ButtonForGame(0,false,mouse); mouse.EndFrame();
    CHECK(mouse.CapturesPointer());
    CHECK(!pad.ButtonForGame(0,true,mouse));
    for (int i=0;i<20;++i) CHECK(!pad.ButtonForGame(0,true,mouse));
    auto clicks=pad.TakeClicks();
    CHECK(clicks.size()==1 && clicks[0].down && clicks[0].x==200 && ListLeftButtonDown(mouse));
    mouse.Move(20,20);
    CHECK(mouse.CapturesPointer()); // List drag remains ours outside its bounds.
    pad.ButtonForGame(0,false,mouse);
    clicks=pad.TakeClicks();
    CHECK(clicks.size()==1 && !clicks[0].down && !mouse.CapturesPointer());
    CHECK(pad.ButtonForGame(0,true,mouse)); // House drag starts outside.
    mouse.Move(200,200);
    CHECK(!mouse.CapturesPointer() && pad.ButtonForGame(0,true,mouse));
    pad.ButtonForGame(0,false,mouse);
    CHECK(pad.TakeClicks().empty() && !mouse.CapturesPointer());
    mouse.EndFrame(); CHECK(mouse.CapturesPointer());
    for (int axis=0;axis<2;++axis) {
        CHECK(pad.AxisForGame(axis,25000,mouse)==25000);
        CHECK(pad.AxisForList(axis,25000,mouse)==0);
    }
    CHECK(pad.AxisForGame(2,25000,mouse)==0 && pad.AxisForList(2,25000,mouse)==25000);
    for (int key : {11,12,13,14}) {
        CHECK(!pad.ButtonForGame(key,true,mouse));
        pad.ButtonForGame(key,false,mouse);
    }
    CHECK(!pad.TakeBack() && pad.TakeClicks().empty());
    CHECK(!pad.ButtonForGame(1,true,mouse) && pad.TakeBack());
    CHECK(!pad.ButtonForGame(1,true,mouse) && !pad.TakeBack());
    pad.ButtonForGame(1,false,mouse);
    mouse.Move(20,20);
    CHECK(pad.ButtonForGame(1,true,mouse) && !pad.TakeBack());
    CHECK(pad.AxisForGame(2,25000,mouse)==25000 && pad.AxisForList(2,25000,mouse)==0);
    pad.ButtonForGame(1,false,mouse);
    mouse.SetPopupOpen(true); // Dropdown dismissals belong to UI even outside.
    CHECK(!pad.ButtonForGame(0,true,mouse));
    pad.ButtonForGame(0,false,mouse); clicks=pad.TakeClicks();
    CHECK(clicks.size()==2 && clicks[0].down && !clicks[1].down);
    mouse.SetPopupOpen(false); mouse.Move(200,200);
    mouse.Down(1); pad.ButtonForGame(0,true,mouse); pad.TakeClicks();
    mouse.Up(1); CHECK(ListLeftButtonDown(mouse));
    pad.ButtonForGame(0,false,mouse); pad.TakeClicks(); CHECK(!ListLeftButtonDown(mouse));
    pad.ButtonForGame(0,true,mouse); mouse.Down(1); pad.TakeClicks();
    pad.ButtonForGame(0,false,mouse); CHECK(ListLeftButtonDown(mouse));
    mouse.Up(1); pad.TakeClicks(); CHECK(!ListLeftButtonDown(mouse));
    pad.ButtonForGame(0,true,mouse); pad.TakeClicks();
    pad.SetActive(false,mouse); clicks=pad.TakeClicks();
    CHECK(clicks.size()==1 && !clicks[0].down && !ListLeftButtonDown(mouse));
    pad.SetActive(true,mouse);
    CHECK(!pad.ButtonForGame(0,true,mouse) && pad.TakeClicks().empty());
    pad.ButtonForGame(0,false,mouse); mouse.EndFrame();
    pad.ButtonForGame(0,true,mouse); pad.TakeClicks();
    pad.Reset(mouse); clicks=pad.TakeClicks();
    CHECK(clicks.size()==1 && !clicks[0].down && !mouse.HasButtons());
    pad.PrimeButton(0,true); pad.SetActive(true,mouse);
    CHECK(!pad.ButtonForGame(0,true,mouse) && pad.TakeClicks().empty());
    std::cout << "Gamepad pointer routing passed: opening hold, virtual clicks, drag ownership, fast taps, mixed mouse inputs, native left stick, local scroll/back, no D-pad actions and cancellation.\n";
}
