#pragma once

#include <memory>
#include <stdint.h>
#include <string>

#include <SDL3/SDL.h>

#include <Lucky/Graphics/GraphicsDevice.hpp>

namespace Lucky
{
	struct GameData
	{
        SDL_Window *window;
        std::shared_ptr<Lucky::GraphicsDevice> graphicsDevice;
        uint64_t currentTime;
        void *userData;
	};

	struct ConfigData
	{
        SDL_InitFlags initFlags;
        SDL_WindowFlags windowFlags;
        std::string windowTitle;
        int screenWidth;
        int screenHeight;
	};
}

void SDLCALL LuckyGetConfig(Lucky::ConfigData *configData);
SDL_AppResult SDLCALL LuckyInitialize(Lucky::GameData *gameData);
SDL_AppResult SDLCALL LuckyEvent(Lucky::GameData *gameData, SDL_Event *event, bool *eventHandled);
SDL_AppResult SDLCALL LuckyUpdate(Lucky::GameData *gameData, double deltaSeconds);
SDL_AppResult SDLCALL LuckyDraw(Lucky::GameData *gameData);
void SDLCALL LuckyQuit(Lucky::GameData *gameData);
