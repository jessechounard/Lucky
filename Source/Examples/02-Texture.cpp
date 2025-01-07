#include <memory>

#define LUCKY_MAIN
#include <Lucky/Graphics/BatchRenderer.hpp>
#include <Lucky/Graphics/Texture.hpp>
#include <Lucky/LuckyMain.hpp>

struct ExampleData
{
    std::unique_ptr<Lucky::BatchRenderer> batchRenderer;
    std::shared_ptr<Lucky::Texture> texture;
    float rotation = 0.0f;
};

void SDLCALL LuckyGetConfig(Lucky::ConfigData *configData)
{
    configData->screenWidth = 1920;
    configData->screenHeight = 1080;
    configData->windowTitle = "Texture Example";
}

SDL_AppResult SDLCALL LuckyInitialize(Lucky::GameData *gameData)
{
    ExampleData *exampleData = new ExampleData();

    exampleData->batchRenderer = std::make_unique<Lucky::BatchRenderer>(gameData->graphicsDevice, 1000);
    exampleData->texture = std::make_shared<Lucky::Texture>("texture.png");

    gameData->userData = exampleData;
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDLCALL LuckyEvent(Lucky::GameData *gameData, SDL_Event *event, bool *eventHandled)
{
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDLCALL LuckyUpdate(Lucky::GameData *gameData, double deltaSeconds)
{
    ExampleData *exampleData = (ExampleData *)gameData->userData;

    exampleData->rotation += (float)deltaSeconds;

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDLCALL LuckyDraw(Lucky::GameData *gameData)
{
    ExampleData *exampleData = (ExampleData *)gameData->userData;

    gameData->graphicsDevice->BeginFrame();
    gameData->graphicsDevice->ClearScreen(Lucky::Color::Black);

    const int width = gameData->graphicsDevice->GetScreenWidth();
    const int height = gameData->graphicsDevice->GetScreenHeight();

    exampleData->batchRenderer->Begin(Lucky::BlendMode::Alpha, exampleData->texture);
    exampleData->batchRenderer->BatchQuad(nullptr, glm::vec2(width / 2.0f, height / 2.0f), exampleData->rotation,
        glm::vec2(1.0f, 1.0f), glm::vec2(0.5f, 0.5f), Lucky::UVMode::Normal, Lucky::Color::White);
    exampleData->batchRenderer->End();

    gameData->graphicsDevice->EndFrame();

    return SDL_APP_CONTINUE;
}

void SDLCALL LuckyQuit(Lucky::GameData *gameData)
{
    ExampleData *exampleData = (ExampleData *)gameData->userData;

    exampleData->texture.reset();
    exampleData->batchRenderer.reset();
    delete exampleData;

    gameData->userData = nullptr;
}
