#include <SDL3/SDL_joystick.h>

#include <Lucky/Input/Gamepad.hpp>

bool GetGamepadEvent_impl(Lucky::GamepadEvent *event);
SDL_Gamepad *GetGamepadFromJoystickId_impl(SDL_JoystickID joystickId);
bool GetGamepadState_impl(SDL_JoystickID joystickId, Lucky::GamepadState *gamepadState);

namespace Lucky
{
    bool GetGamepadEvent(GamepadEvent *event)
    {
        return false;
    }

    SDL_Gamepad *GetGamepadFromJoystickId(SDL_JoystickID joystickId)
    {
        return nullptr;
    }

    bool GetGamepadState(SDL_JoystickID joystickId, GamepadState *gamepadState)
    {
        return GetGamepadState_impl(joystickId, gamepadState);
    }
} // namespace Lucky