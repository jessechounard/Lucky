#include <memory>
#include <stdint.h>

#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>

#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <spdlog/spdlog.h>

#include <Lucky/Graphics/Color.hpp>
#include <Lucky/Graphics/BatchRenderer.hpp>
#include <Lucky/Graphics/GraphicsDevice.hpp>
#include <Lucky/Graphics/Texture.hpp>

const int windowWidth = 1920;
const int windowHeight = 1080;

SDL_Window *window;
std::shared_ptr<Lucky::GraphicsDevice> graphicsDevice;
std::unique_ptr<Lucky::BatchRenderer> batchRenderer;
std::shared_ptr<Lucky::Texture> texture;
float rotation = 0.0f;
uint64_t currentTime;

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);

    SDL_SetAppMetadata("Texture Example", "1.0", "dev.jessechounard.texture");

    if (!SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO | SDL_INIT_GAMEPAD))
    {
        spdlog::error("Failed to initialize SDL {}", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    auto attributes = Lucky::GraphicsDevice::PrepareWindowAttributes(Lucky::GraphicsAPI::OpenGL);

    window = SDL_CreateWindow("Texture Example", windowWidth, windowHeight, attributes);
    if (window == NULL)
    {
        spdlog::error("Failed to create window {}", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    graphicsDevice = std::make_shared<Lucky::GraphicsDevice>(
        Lucky::GraphicsAPI::OpenGL, window, Lucky::VerticalSyncType::AdaptiveEnabled);
    batchRenderer = std::make_unique<Lucky::BatchRenderer>(graphicsDevice, 1000);
    texture = std::make_shared<Lucky::Texture>("texture.png");

    currentTime = SDL_GetPerformanceCounter();

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    if (event->type == SDL_EVENT_QUIT)
    {
        return SDL_APP_SUCCESS;
    }
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *appstate)
{
    uint64_t newTime = SDL_GetPerformanceCounter();
    double frameTime = (newTime - currentTime) / (double)SDL_GetPerformanceFrequency();
    currentTime = newTime;

    rotation += (float)frameTime;

    graphicsDevice->BeginFrame();
    graphicsDevice->ClearScreen(Lucky::Color::Black);

    batchRenderer->Begin(Lucky::BlendMode::Alpha, texture);
    batchRenderer->BatchQuad(nullptr, glm::vec2(windowWidth / 2.0f, windowHeight / 2.0f), rotation, glm::vec2(1.0f, 1.0f),
        glm::vec2(0.5f, 0.5f),
        Lucky::UVMode::Normal, Lucky::Color::White);
    batchRenderer->End();

    graphicsDevice->EndFrame();
    SDL_GL_SwapWindow(window);

    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    graphicsDevice.reset();
    SDL_DestroyWindow(window);
    SDL_Quit();
}
