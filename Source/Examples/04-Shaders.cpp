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
#include <Lucky/Graphics/GraphicsDevice.hpp>
#include <Lucky/Graphics/ShaderProgram.hpp>

const int windowWidth = 1920;
const int windowHeight = 1080;

SDL_Window *window;
std::shared_ptr<Lucky::GraphicsDevice> graphicsDevice;
std::unique_ptr<Lucky::BatchRenderer> batchRenderer;
std::shared_ptr<Lucky::ShaderProgram> shader;
uint64_t currentTime;
float totalTime = 0;

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);

    SDL_SetAppMetadata("Shader Example", "1.0", "dev.jessechounard.shaders");

    if (!SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO | SDL_INIT_GAMEPAD))
    {
        spdlog::error("Failed to initialize SDL {}", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    auto attributes = Lucky::GraphicsDevice::PrepareWindowAttributes(Lucky::GraphicsAPI::OpenGL);

    window = SDL_CreateWindow("Shader Example", windowWidth, windowHeight, attributes);
    if (window == NULL)
    {
        spdlog::error("Failed to create window {}", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    graphicsDevice = std::make_shared<Lucky::GraphicsDevice>(
        Lucky::GraphicsAPI::OpenGL, window, Lucky::VerticalSyncType::AdaptiveEnabled);
    batchRenderer = std::make_unique<Lucky::BatchRenderer>(graphicsDevice, 1000);

    Lucky::VertexShader vertexShader("Basic.vert");
    Lucky::FragmentShader fragmentShader("ShaderArt.frag");
    shader = std::make_shared<Lucky::ShaderProgram>(graphicsDevice, vertexShader, fragmentShader);

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

    totalTime += (float)frameTime;

    graphicsDevice->BeginFrame();
    graphicsDevice->ClearScreen(Lucky::Color::Black);

    batchRenderer->Begin(Lucky::BlendMode::None, nullptr, shader);
    shader->SetParameter("iResolution", glm::vec2((float)windowWidth, (float)windowHeight));
    shader->SetParameter("iTime", totalTime);
    batchRenderer->BatchQuadUV(glm::vec2(0.0f, 0.0f), glm::vec2((float)windowWidth, (float)windowHeight),
        glm::vec2(0.0, 0.0f), glm::vec2((float)windowWidth, (float)windowHeight), Lucky::Color::White);
    batchRenderer->End();

    graphicsDevice->EndFrame();
    SDL_GL_SwapWindow(window);

    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    shader.reset();
    batchRenderer.reset();
    graphicsDevice.reset();
    SDL_DestroyWindow(window);
    SDL_Quit();
}
