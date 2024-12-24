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
#include <Lucky/Graphics/TextureAtlas.hpp>
#include <Lucky/Utility/FileSystem.hpp>

const int windowWidth = 1920;
const int windowHeight = 1080;

SDL_Window *window;
std::shared_ptr<Lucky::GraphicsDevice> graphicsDevice;
std::unique_ptr<Lucky::BatchRenderer> batchRenderer;
std::shared_ptr<Lucky::Texture> texture;
std::shared_ptr<Lucky::TextureAtlas> textureAtlas;
uint64_t currentTime;

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);

    SDL_SetAppMetadata("Texture Atlas Example", "1.0", "dev.jessechounard.textureatlas");

    if (!SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO | SDL_INIT_GAMEPAD))
    {
        spdlog::error("Failed to initialize SDL {}", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    auto attributes = Lucky::GraphicsDevice::PrepareWindowAttributes(Lucky::GraphicsAPI::OpenGL);

    window = SDL_CreateWindow("TextureAtlas Example", windowWidth, windowHeight, attributes);
    if (window == NULL)
    {
        spdlog::error("Failed to create window {}", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    graphicsDevice = std::make_shared<Lucky::GraphicsDevice>(
        Lucky::GraphicsAPI::OpenGL, window, Lucky::VerticalSyncType::AdaptiveEnabled);
    batchRenderer = std::make_unique<Lucky::BatchRenderer>(graphicsDevice, 1000);

    const auto atlasPath = "./texture_atlas.json";
    textureAtlas = std::make_shared<Lucky::TextureAtlas>(atlasPath);
    const auto texturePath = Lucky::CombinePaths(Lucky::GetPathName(atlasPath), textureAtlas->TexturePath());
    texture = std::make_shared<Lucky::Texture>(texturePath);

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
    graphicsDevice->ClearScreen(Lucky::Color::CornflowerBlue);

    batchRenderer->Begin(Lucky::BlendMode::Alpha, texture);

    const auto region1 = textureAtlas->GetRegion("playerShip1_blue.png");
    batchRenderer->BatchQuad(&region1.bounds, glm::vec2(windowWidth / 4.0f, windowHeight / 4.0f), 0.0f,
        glm::vec2(1.0f, 1.0f), region1.originCenter,
        region1.rotated ? Lucky::UVMode::RotatedCW90 : Lucky::UVMode::Normal, Lucky::Color::White);

    const auto region2 = textureAtlas->GetRegion("playerShip2_green.png");
    batchRenderer->BatchQuad(&region2.bounds, glm::vec2(windowWidth / 4.0f, windowHeight * 3.0f / 4.0f), 0.0f,
        glm::vec2(1.0f, 1.0f), region2.originCenter,
        region2.rotated ? Lucky::UVMode::RotatedCW90 : Lucky::UVMode::Normal, Lucky::Color::White);

    const auto region3 = textureAtlas->GetRegion("playerShip3_orange.png");
    batchRenderer->BatchQuad(&region3.bounds, glm::vec2(windowWidth * 3.0f / 4.0f, windowHeight / 4.0f), 0.0f,
        glm::vec2(1.0f, 1.0f), region3.originCenter,
        region3.rotated ? Lucky::UVMode::RotatedCW90 : Lucky::UVMode::Normal, Lucky::Color::White);

    const auto region4 = textureAtlas->GetRegion("ufoRed.png");
    batchRenderer->BatchQuad(&region4.bounds, glm::vec2(windowWidth * 3.0f / 4.0f, windowHeight * 3.0f / 4.0f), 0.0f,
        glm::vec2(1.0f, 1.0f), region4.originCenter,
        region4.rotated ? Lucky::UVMode::RotatedCW90 : Lucky::UVMode::Normal, Lucky::Color::White);

    batchRenderer->BatchQuad(nullptr, glm::vec2(windowWidth / 2.0f, windowHeight / 2.0f), 0.0f, glm::vec2(1.0f, 1.0f),
        glm::vec2(0.5f, 0.5f), Lucky::UVMode::Normal, Lucky::Color::White);

    batchRenderer->End();

    graphicsDevice->EndFrame();
    SDL_GL_SwapWindow(window);

    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    texture.reset();
    textureAtlas.reset();
    batchRenderer.reset();
    graphicsDevice.reset();
    SDL_DestroyWindow(window);
    SDL_Quit();
}
