#include <spdlog/spdlog.h>

#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>

#include <SDL3/SDL_gamepad.h>

#include <Lucky/Game.hpp>
#include <Lucky/Input/Gamepad.hpp>

extern "C" SDL_AppResult SDL_AppInit(void **appState, int argc, char *argv[])
{
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);

    Lucky::ConfigData configData = {SDL_INIT_AUDIO | SDL_INIT_GAMEPAD, 0, "", 1024, 768};

    LuckyGetConfig(&configData);

    if (!SDL_Init(SDL_INIT_VIDEO | configData.initFlags))
    {
        spdlog::error("Failed to initialize SDL {}", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    Lucky::GameData *gameData = new Lucky::GameData();

    auto windowAttributes = Lucky::GraphicsDevice::PrepareWindowAttributes(Lucky::GraphicsAPI::OpenGL);

    gameData->window = SDL_CreateWindow(configData.windowTitle.c_str(), configData.screenWidth, configData.screenHeight,
        windowAttributes | configData.windowFlags);
    if (gameData->window == NULL)
    {
        spdlog::error("Failed to create window {}", SDL_GetError());
        delete gameData;
        return SDL_APP_FAILURE;
    }

    gameData->graphicsDevice = std::make_shared<Lucky::GraphicsDevice>(
        Lucky::GraphicsAPI::OpenGL, gameData->window, Lucky::VerticalSyncType::AdaptiveEnabled);

    gameData->currentTime = SDL_GetPerformanceCounter();
    *appState = gameData;

    SDL_AppResult result = LuckyInitialize(gameData);

    return result;
}

extern "C" SDL_AppResult SDL_AppEvent(void *appState, SDL_Event *event)
{
    Lucky::GameData *gameData = (Lucky::GameData *)appState;

    bool eventHandled = false;
    SDL_AppResult result = LuckyEvent(gameData, event, &eventHandled);

    if (result != SDL_APP_CONTINUE)
    {
        return result;
    }

    if (eventHandled)
    {
        return SDL_APP_CONTINUE;
    }

    switch (event->type)
    {
    case SDL_EVENT_QUIT:
        return SDL_APP_SUCCESS;

    case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
        if (event->window.windowID == SDL_GetWindowID(gameData->window))
        {
            return SDL_APP_SUCCESS;
        }
        break;

    case SDL_EVENT_KEY_DOWN:
        printf("Key Down %d\n", event->key.which);
        // if (event.key.keysym.scancode >= 0 && event.key.keysym.scancode < Lucky::MaxKeyboardKeys)
        //{
        //     keyboardState.keys[event.key.keysym.scancode] = true;
        // }

        break;

    case SDL_EVENT_KEY_UP:
        // if (event.key.keysym.scancode >= 0 && event.key.keysym.scancode < Lucky::MaxKeyboardKeys)
        //{
        //     keyboardState.keys[event.key.keysym.scancode] = false;
        // }
        break;

    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        // if (event.button.button > 0 && event.button.button < Lucky::MaxMouseButtons)
        //{
        //     mouseState.buttons[event.button.button] = true;
        // }
        break;

    case SDL_EVENT_MOUSE_BUTTON_UP:
        // if (event.button.button > 0 && event.button.button < Lucky::MaxMouseButtons)
        //{
        //     mouseState.buttons[event.button.button] = false;
        // }
        break;

    case SDL_EVENT_MOUSE_MOTION:
        // mouseState.x = event.motion.x;
        // mouseState.y = event.motion.y;
        break;

    case SDL_EVENT_MOUSE_WHEEL:
        // if (event.wheel.direction != SDL_MOUSEWHEEL_FLIPPED)
        //{
        //     mouseState.wheelDelta += event.wheel.y;
        // }
        // else
        //{
        //     mouseState.wheelDelta -= event.wheel.y;
        // }
        break;

    case SDL_EVENT_KEYBOARD_ADDED:
        printf("Keyboard Added %d\n", event->kdevice.which);
        break;

    case SDL_EVENT_MOUSE_ADDED:
        printf("Mouse Added %d\n", event->mdevice.which);
        break;

    case SDL_EVENT_KEYBOARD_REMOVED:
        printf("Keyboard removed %d\n", event->kdevice.which);
        break;

    case SDL_EVENT_MOUSE_REMOVED:
        printf("Mouse removed %d\n", event->mdevice.which);
        break;

    case SDL_EVENT_GAMEPAD_ADDED: {
        Lucky::GamepadState gamepadState;
        gamepadState.gamepad = SDL_OpenGamepad(event->gdevice.which);
        if (!gamepadState.gamepad)
        {
            // todo: handle this error
            break;
        }
        Lucky::GetInitialGamepadState(gamepadState.gamepad, gamepadState.buttons, SDL_GAMEPAD_BUTTON_COUNT - 1,
            gamepadState.axes, SDL_GAMEPAD_AXIS_COUNT - 1);
        gameData->gamepadStates[event->gdevice.which] = gamepadState;
        break;
    }

    case SDL_EVENT_GAMEPAD_REMOVED:
        SDL_CloseGamepad(gameData->gamepadStates[event->gdevice.which].gamepad);
        gameData->gamepadStates.erase(event->gdevice.which);
        break;

    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
        gameData->gamepadEvents.emplace_back(
            event->gbutton.which, Lucky::GamepadEventType::ButtonPressed, (SDL_GamepadButton)event->gbutton.button);
        gameData->gamepadStates[event->gbutton.which].buttons[event->gbutton.button] = true;
        break;

    case SDL_EVENT_GAMEPAD_BUTTON_UP:
        gameData->gamepadEvents.emplace_back(
            event->gbutton.which, Lucky::GamepadEventType::ButtonReleased, (SDL_GamepadButton)event->gbutton.button);
        gameData->gamepadStates[event->gbutton.which].buttons[event->gbutton.button] = false;
        break;

    case SDL_EVENT_GAMEPAD_AXIS_MOTION:
        // gamepadStates[event.gaxis.which].buttons[event.gaxis.axis] = event.gaxis.value;
        break;

    case SDL_EVENT_GAMEPAD_TOUCHPAD_UP:
        // todo:
        // event->gtouchpad
        break;

    case SDL_EVENT_GAMEPAD_TOUCHPAD_DOWN:
        // todo:
        // event->gtouchpad
        break;

    case SDL_EVENT_GAMEPAD_TOUCHPAD_MOTION:
        // todo:
        // event->gtouchpad
        break;

    default:
        break;
    }

    return SDL_APP_CONTINUE;
}

extern "C" SDL_AppResult SDL_AppIterate(void *appState)
{
    Lucky::GameData *gameData = (Lucky::GameData *)appState;

    uint64_t newTime = SDL_GetPerformanceCounter();
    double deltaSeconds = (newTime - gameData->currentTime) / (double)SDL_GetPerformanceFrequency();
    gameData->currentTime = newTime;

    SDL_AppResult result = LuckyUpdate(gameData, deltaSeconds);
    if (result != SDL_APP_CONTINUE)
    {
        return result;
    }

    result = LuckyDraw(gameData);
    if (result != SDL_APP_CONTINUE)
    {
        return result;
    }

    SDL_GL_SwapWindow(gameData->window);

    gameData->gamepadEvents.clear();

    return SDL_APP_CONTINUE;
}

extern "C" void SDL_AppQuit(void *appState, SDL_AppResult result)
{
    Lucky::GameData *gameData = (Lucky::GameData *)appState;

    LuckyQuit(gameData);

    gameData->graphicsDevice.reset();
    SDL_DestroyWindow(gameData->window);
    delete gameData;
    SDL_Quit();
}