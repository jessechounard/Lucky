#define LUCKY_MAIN
#include <Lucky/LuckyMain.hpp>

void SDLCALL LuckyGetConfig(Lucky::ConfigData *configData)
{
    configData->windowTitle = "Clear Screen Example";
}

SDL_AppResult SDLCALL LuckyInitialize(Lucky::GameData *gameData)
{
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDLCALL LuckyEvent(Lucky::GameData *gameData, SDL_Event *event, bool *eventHandled)
{
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDLCALL LuckyUpdate(Lucky::GameData *gameData, double deltaSeconds)
{
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDLCALL LuckyDraw(Lucky::GameData *gameData)
{
    gameData->graphicsDevice->BeginFrame();
    gameData->graphicsDevice->ClearScreen(Lucky::Color::CornflowerBlue);
    gameData->graphicsDevice->EndFrame();

    return SDL_APP_CONTINUE;
}

void SDLCALL LuckyQuit(Lucky::GameData *gameData)
{
}
