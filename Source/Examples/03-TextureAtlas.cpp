#include <memory>

#define LUCKY_MAIN
#include <Lucky/Graphics/BatchRenderer.hpp>
#include <Lucky/Graphics/Texture.hpp>
#include <Lucky/Graphics/TextureAtlas.hpp>
#include <Lucky/LuckyMain.hpp>
#include <Lucky/Utility/FileSystem.hpp>

struct ExampleData
{
    std::unique_ptr<Lucky::BatchRenderer> batchRenderer;
    std::shared_ptr<Lucky::TextureAtlas> textureAtlas;
    std::shared_ptr<Lucky::Texture> texture;
};

void SDLCALL LuckyGetConfig(Lucky::ConfigData *configData)
{
    configData->screenWidth = 1920;
    configData->screenHeight = 1080;
    configData->windowTitle = "Texture Atlas Example";
}

SDL_AppResult SDLCALL LuckyInitialize(Lucky::GameData *gameData)
{
    ExampleData *exampleData = new ExampleData();

    exampleData->batchRenderer = std::make_unique<Lucky::BatchRenderer>(gameData->graphicsDevice, 1000);

    const auto atlasPath = "./texture_atlas.json";
    exampleData->textureAtlas = std::make_shared<Lucky::TextureAtlas>(atlasPath);
    const auto texturePath =
        Lucky::CombinePaths(Lucky::GetPathName(atlasPath), exampleData->textureAtlas->TexturePath());
    exampleData->texture = std::make_shared<Lucky::Texture>(texturePath);

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

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDLCALL LuckyDraw(Lucky::GameData *gameData)
{
    ExampleData *exampleData = (ExampleData *)gameData->userData;

    gameData->graphicsDevice->BeginFrame();
    gameData->graphicsDevice->ClearScreen(Lucky::Color::CornflowerBlue);

    const int width = gameData->graphicsDevice->GetScreenWidth();
    const int height = gameData->graphicsDevice->GetScreenHeight();

    exampleData->batchRenderer->Begin(Lucky::BlendMode::Alpha, exampleData->texture);

    const auto region1 = exampleData->textureAtlas->GetRegion("playerShip1_blue.png");
    exampleData->batchRenderer->BatchQuad(&region1.bounds, glm::vec2(width / 4.0f, height / 4.0f), 0.0f,
        glm::vec2(1.0f, 1.0f), region1.originCenter,
        region1.rotated ? Lucky::UVMode::RotatedCW90 : Lucky::UVMode::Normal, Lucky::Color::White);

    const auto region2 = exampleData->textureAtlas->GetRegion("playerShip2_green.png");
    exampleData->batchRenderer->BatchQuad(&region2.bounds, glm::vec2(width / 4.0f, height * 3.0f / 4.0f), 0.0f,
        glm::vec2(1.0f, 1.0f), region2.originCenter,
        region2.rotated ? Lucky::UVMode::RotatedCW90 : Lucky::UVMode::Normal, Lucky::Color::White);

    const auto region3 = exampleData->textureAtlas->GetRegion("playerShip3_orange.png");
    exampleData->batchRenderer->BatchQuad(&region3.bounds, glm::vec2(width * 3.0f / 4.0f, height / 4.0f), 0.0f,
        glm::vec2(1.0f, 1.0f), region3.originCenter,
        region3.rotated ? Lucky::UVMode::RotatedCW90 : Lucky::UVMode::Normal, Lucky::Color::White);

    const auto region4 = exampleData->textureAtlas->GetRegion("ufoRed.png");
    exampleData->batchRenderer->BatchQuad(&region4.bounds, glm::vec2(width * 3.0f / 4.0f, height * 3.0f / 4.0f), 0.0f,
        glm::vec2(1.0f, 1.0f), region4.originCenter,
        region4.rotated ? Lucky::UVMode::RotatedCW90 : Lucky::UVMode::Normal, Lucky::Color::White);

    exampleData->batchRenderer->BatchQuad(nullptr, glm::vec2(width / 2.0f, height / 2.0f), 0.0f, glm::vec2(1.0f, 1.0f),
        glm::vec2(0.5f, 0.5f), Lucky::UVMode::Normal, Lucky::Color::White);

    exampleData->batchRenderer->End();

    gameData->graphicsDevice->EndFrame();

    return SDL_APP_CONTINUE;
}

void SDLCALL LuckyQuit(Lucky::GameData *gameData)
{
    ExampleData *exampleData = (ExampleData *)gameData->userData;

    exampleData->texture.reset();
    exampleData->textureAtlas.reset();
    exampleData->batchRenderer.reset();
    delete exampleData;

    gameData->userData = nullptr;
}
