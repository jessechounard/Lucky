#pragma once

#include <map>
#include <vector>

#include <SDL3/SDL.h>

namespace Lucky {

enum class InputEventType {
    KeyboardKeyPressed,
    KeyboardKeyReleased,
    GamepadButtonPressed,
    GamepadButtonReleased,
    GamepadConnected,
    GamepadDisconnected,
    // axis ?
    // touchpad motion ?
    // touchpad down/up ?
};

struct GamepadState {
    const GamepadState &operator=(const GamepadState &state) {
        gamepad = state.gamepad;
        memcpy(buttons, state.buttons, sizeof(bool) * SDL_GAMEPAD_BUTTON_COUNT);
        memcpy(axes, state.axes, sizeof(int16_t) * SDL_GAMEPAD_AXIS_COUNT);
        return *this;
    }

    SDL_Gamepad *gamepad;
    bool buttons[SDL_GAMEPAD_BUTTON_COUNT];
    int16_t axes[SDL_GAMEPAD_AXIS_COUNT];
    // todo: touchpad states?
};

struct KeyboardState {
    const KeyboardState &operator=(const KeyboardState &state) {
        memcpy(keys, state.keys, sizeof(bool) * SDL_SCANCODE_COUNT);
        keyCount = state.keyCount;
        return *this;
    }

    bool keys[SDL_SCANCODE_COUNT];
    int keyCount;
};

struct InputHelper {
    InputHelper();

    void BeginFrame();
    void EndFrame();

    void AddEvent(const SDL_Event &event);

    SDL_JoystickID FindGamepadButtonPressEvent(
        SDL_GamepadButton button, std::vector<SDL_JoystickID> ignoreList = {});

    SDL_JoystickID FindGamepadButtonPressEvent(
        std::vector<SDL_GamepadButton> buttons, std::vector<SDL_JoystickID> ignoreList = {});

    const KeyboardState &GetKeyboardState() const;
    const GamepadState &GetGamepadState(SDL_JoystickID id) const;

    bool IsGamepadConnected(SDL_JoystickID id);

    std::map<SDL_JoystickID, GamepadState> gamepads;
    KeyboardState keyboard;
    std::vector<SDL_Event> frameEvents;

    // todo: mouse state
    // todo: mouse events
};

} // namespace Lucky
