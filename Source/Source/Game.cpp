#include <spdlog/spdlog.h>

#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>

#include <Lucky/Game.hpp>

extern "C" SDL_AppResult SDL_AppInit(void **appState, int argc, char *argv[])
{
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);

    Lucky::ConfigData configData = {0, 0, "", 1024, 768};

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

    if (!eventHandled)
    {
        if (event->type == SDL_EVENT_QUIT)
        {
            return SDL_APP_SUCCESS;
        }
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