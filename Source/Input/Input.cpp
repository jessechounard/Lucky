#include <algorithm>
#include <stdexcept>

#include <spdlog/spdlog.h>

#include <Lucky/Input.hpp>

namespace Lucky {

void GetInitialGamepadState(GamepadState &gamepadState) {
    for (int i = 0; i < SDL_GAMEPAD_BUTTON_COUNT; i++) {
        gamepadState.buttons[i] = SDL_GetGamepadButton(gamepadState.gamepad, (SDL_GamepadButton)i);
    }
    for (int i = 0; i < SDL_GAMEPAD_AXIS_COUNT; i++) {
        gamepadState.axes[i] = SDL_GetGamepadAxis(gamepadState.gamepad, (SDL_GamepadAxis)i);
    }
    // todo: touchpad states?
}

void AddGamepad(std::map<SDL_JoystickID, GamepadState> &gamepads, SDL_JoystickID id) {
    GamepadState gamepadState;
    gamepadState.gamepad = SDL_OpenGamepad(id);

    if (gamepadState.gamepad == nullptr) {
        spdlog::error("AddGamepad: Failed to initialize gamepad with SDL_OpenGamepad");
        return;
    }

    GetInitialGamepadState(gamepadState);
    gamepads.insert_or_assign(id, gamepadState);
}

void RemoveGamepad(std::map<SDL_JoystickID, GamepadState> &gamepads, SDL_JoystickID id) {
    auto iterator = gamepads.find(id);
    if (iterator != gamepads.end()) {
        SDL_CloseGamepad(iterator->second.gamepad);
        gamepads.erase(id);
    }
}

void SetGamepadButtonState(std::map<SDL_JoystickID, GamepadState> &gamepads, SDL_JoystickID id,
    SDL_GamepadButton button, bool state) {
    auto iterator = gamepads.find(id);
    if (iterator != gamepads.end()) {
        iterator->second.buttons[button] = state;
    } else {
        spdlog::error("SetGamepadButtonState: Trying to set state on a disconnected gamepad");
    }
}

void SetGamepadAxisState(std::map<SDL_JoystickID, GamepadState> &gamepads, SDL_JoystickID id,
    SDL_GamepadAxis axis, int16_t state) {
    auto iterator = gamepads.find(id);
    if (iterator != gamepads.end()) {
        iterator->second.axes[axis] = state;
    } else {
        spdlog::error("SetGamepadAxisState: Trying to set state on a disconnected gamepad");
    }
}

InputHelper::InputHelper() {
    const bool *keys = SDL_GetKeyboardState(&keyboard.keyCount);
    memcpy(keyboard.keys, keys, sizeof(bool) * keyboard.keyCount);
}

void InputHelper::BeginFrame() {
}

void InputHelper::EndFrame() {
    frameEvents.clear();
}

void InputHelper::AddEvent(const SDL_Event &event) {
    frameEvents.push_back(event);
    switch (event.type) {
    // keyboard
    case SDL_EVENT_KEY_DOWN:
        keyboard.keys[event.key.scancode] = true;
        break;
    case SDL_EVENT_KEY_UP:
        keyboard.keys[event.key.scancode] = false;
        break;

    // gamepad
    case SDL_EVENT_GAMEPAD_ADDED:
        AddGamepad(gamepads, event.gdevice.which);
        break;
    case SDL_EVENT_GAMEPAD_REMOVED:
        RemoveGamepad(gamepads, event.gdevice.which);
        break;
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
        SetGamepadButtonState(
            gamepads, event.gbutton.which, (SDL_GamepadButton)event.gbutton.button, true);
        break;
    case SDL_EVENT_GAMEPAD_BUTTON_UP:
        SetGamepadButtonState(
            gamepads, event.gbutton.which, (SDL_GamepadButton)event.gbutton.button, false);
        break;
    case SDL_EVENT_GAMEPAD_AXIS_MOTION:
        SetGamepadAxisState(
            gamepads, event.gaxis.which, (SDL_GamepadAxis)event.gaxis.axis, event.gaxis.value);
        break;
    case SDL_EVENT_GAMEPAD_TOUCHPAD_UP:
    case SDL_EVENT_GAMEPAD_TOUCHPAD_DOWN:
    case SDL_EVENT_GAMEPAD_TOUCHPAD_MOTION:
        // todo: handle touchpad events
        break;
    }
}

SDL_JoystickID InputHelper::FindGamepadButtonPressEvent(
    SDL_GamepadButton button, std::vector<SDL_JoystickID> ignoreList) {
    for (const auto &event : frameEvents) {
        if (event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN && event.gbutton.button == button) {
            auto iterator = std::find(ignoreList.begin(), ignoreList.end(), event.gbutton.which);
            if (iterator != ignoreList.end()) {
                // we found the device in the ignore list
                continue;
            }

            return event.gbutton.which;
        }
    }
    return 0;
}

SDL_JoystickID InputHelper::FindGamepadButtonPressEvent(
    std::vector<SDL_GamepadButton> buttons, std::vector<SDL_JoystickID> ignoreList) {
    for (const auto &event : frameEvents) {
        if (event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
            auto buttonIterator = std::find(buttons.begin(), buttons.end(), event.gbutton.button);
            if (buttonIterator == buttons.end()) {
                // we didn't find the button in the button list
                continue;
            }

            auto iterator = std::find(ignoreList.begin(), ignoreList.end(), event.gbutton.which);
            if (iterator != ignoreList.end()) {
                // we found the device in the ignore list
                continue;
            }

            return event.gbutton.which;
        }
    }
    return 0;
}

const KeyboardState &InputHelper::GetKeyboardState() const {
    return keyboard;
}

const GamepadState &InputHelper::GetGamepadState(SDL_JoystickID id) const {
    auto iterator = gamepads.find(id);
    if (iterator != gamepads.end()) {
        return iterator->second;
    } else {
        spdlog::error("GetGamepadState: Tried to get input on a disconnected gamepad");
        throw std::runtime_error("GetGamepadState: Tried to get input on a disconnected gamepad");
    }
}

bool InputHelper::IsGamepadConnected(SDL_JoystickID id) {
    auto iterator = gamepads.find(id);
    return iterator != gamepads.end();
}

} // namespace Lucky
