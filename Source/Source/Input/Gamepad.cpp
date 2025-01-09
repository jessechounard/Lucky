#include <SDL3/SDL_joystick.h>

#include <Lucky/Game.hpp>
#include <Lucky/Input/Gamepad.hpp>

bool GetGamepadEvent_impl(Lucky::GamepadEvent *event);
SDL_Gamepad *GetGamepadFromJoystickId_impl(SDL_JoystickID joystickId);
bool GetGamepadState_impl(SDL_JoystickID joystickId, Lucky::GamepadState *gamepadState);

namespace Lucky
{
    void GetInitialGamepadState(SDL_Gamepad *gamepad, bool *buttons, int buttonCount, int16_t *axes, int axisCount)
    {
        for (int i = 0; i < buttonCount; i++)
        {
            buttons[i] = SDL_GetGamepadButton(gamepad, (SDL_GamepadButton)i);
        }
        for (int i = 0; i < axisCount; i++)
        {
            axes[i] = SDL_GetGamepadAxis(gamepad, (SDL_GamepadAxis)i);
        }
    }

    bool GetGamepadState(GameData *gameData, SDL_JoystickID id, GamepadState *state)
    {
        auto &gamepadStates = gameData->gamepadStates;

        auto it = gamepadStates.find(id);
        if (it == gamepadStates.end())
        {
            return false;
        }

        *state = it->second;
        return true;
    }

    bool FindGamepadButtonPress(GameData *gameData, SDL_JoystickID *id, SDL_GamepadButton button)
    {
        for (auto &event : gameData->gamepadEvents)
        {
            if (event.eventType == GamepadEventType::ButtonPressed && event.button == button)
            {
                *id = event.joystickId;
                return true;
            }
        }

        *id = 0;
        return false;
    }

} // namespace Lucky