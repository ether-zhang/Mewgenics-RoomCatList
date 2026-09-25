#pragma once
namespace roomcats {
struct GamepadFrame {
    bool connected = false, back_pressed = false;
    float scroll_x = 0, scroll_y = 0;
};
}
