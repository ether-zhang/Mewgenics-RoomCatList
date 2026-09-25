#pragma once
#include "mew_ui_api.h"
#include "gamepad_state.hpp"
#include "mouse_router.hpp"
namespace roomcats {

bool StartGamepadInput(const MewjectorAPI& api, MouseRouter& mouse, bool (*refresh_pointer)());
void StopGamepadInput();
void CancelGamepadInput();
GamepadFrame UpdateGamepadInput(bool capture);
}
