#include <stdint.h>

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_gamepad.h>

namespace Lucky
{
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

    bool GetGamepadEvent(GamepadEvent *event);

    SDL_Gamepad *GetGamepadFromJoystickId(SDL_JoystickID joystickId);
    bool GetGamepadState(SDL_JoystickID joystickId, GamepadState *gamepadState);

} // namespace Lucky
