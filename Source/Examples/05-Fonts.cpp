#include <memory>
#include <stdint.h>

#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>

#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <spdlog/spdlog.h>

#include <Lucky/Graphics/BatchRenderer.hpp>
#include <Lucky/Graphics/Color.hpp>
#include <Lucky/Graphics/Font.hpp>
#include <Lucky/Graphics/GraphicsDevice.hpp>

const int windowWidth = 1920;
const int windowHeight = 1080;

SDL_Window *window;
std::shared_ptr<Lucky::GraphicsDevice> graphicsDevice;
std::unique_ptr<Lucky::BatchRenderer> batchRenderer;
std::shared_ptr<Lucky::Font> font;
uint64_t currentTime;

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);

    SDL_SetAppMetadata("Font Example", "1.0", "dev.jessechounard.fonts");

    if (!SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO | SDL_INIT_GAMEPAD))
    {
        spdlog::error("Failed to initialize SDL {}", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    auto attributes = Lucky::GraphicsDevice::PrepareWindowAttributes(Lucky::GraphicsAPI::OpenGL);

    window = SDL_CreateWindow("Font Example", windowWidth, windowHeight, attributes);
    if (window == NULL)
    {
        spdlog::error("Failed to create window {}", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    graphicsDevice = std::make_shared<Lucky::GraphicsDevice>(
        Lucky::GraphicsAPI::OpenGL, window, Lucky::VerticalSyncType::AdaptiveEnabled);
    batchRenderer = std::make_unique<Lucky::BatchRenderer>(graphicsDevice, 1000);

    font = std::make_shared<Lucky::Font>("Raleway-Regular.ttf");
    int codePoints[] = {'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's',
        't', 'u', 'v', 'w', 'x', 'y', 'z', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O',
        'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', ' ', '!', ','};
    font->CreateFontEntry("font_entry", 100, codePoints, 55, 2);

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

    graphicsDevice->BeginFrame();
    graphicsDevice->ClearScreen(Lucky::Color::Black);

    batchRenderer->Begin(Lucky::BlendMode::Alpha, font->GetTexture("font_entry"));
    font->DrawString(*batchRenderer, "Rendering fonts!", "font_entry", 620, 560, Lucky::Color::White);
    batchRenderer->End();

    graphicsDevice->EndFrame();
    SDL_GL_SwapWindow(window);

    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    font.reset();
    graphicsDevice.reset();
    SDL_DestroyWindow(window);
    SDL_Quit();
}
