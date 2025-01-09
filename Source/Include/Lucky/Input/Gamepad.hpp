#pragma once

#include <stdint.h>

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_gamepad.h>

namespace Lucky
{
    struct GameData;

    enum class GamepadEventType
    {
        ButtonPressed,
        ButtonReleased,
        // touchpad events?
        // axis motion events?
    };

    struct GamepadEvent
    {
        SDL_JoystickID joystickId;
        GamepadEventType eventType;
        SDL_GamepadButton button;

        GamepadEvent(SDL_JoystickID joystickId, GamepadEventType eventType, SDL_GamepadButton button)
            : joystickId(joystickId),
              eventType(eventType),
              button(button)
        {
        }
    };

    struct GamepadState
    {
        SDL_Gamepad *gamepad;
        bool buttons[SDL_GAMEPAD_BUTTON_COUNT];
        int16_t axes[SDL_GAMEPAD_AXIS_COUNT];
    };

    void GetInitialGamepadState(SDL_Gamepad *gamepad, bool *buttons, int buttonCount, int16_t *axes, int axisCount);
    bool GetGamepadState(GameData *gameData, SDL_JoystickID id, GamepadState *state);
    bool FindGamepadButtonPress(GameData *gameData, SDL_JoystickID *id, SDL_GamepadButton button);

} // namespace Lucky
