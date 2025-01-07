#include <memory>

#define LUCKY_MAIN
#include <Lucky/Graphics/BatchRenderer.hpp>
#include <Lucky/Graphics/Color.hpp>
#include <Lucky/Graphics/ShaderProgram.hpp>
#include <Lucky/Graphics/Texture.hpp>
#include <Lucky/LuckyMain.hpp>

struct ExampleData
{
    std::unique_ptr<Lucky::BatchRenderer> batchRenderer;
    std::shared_ptr<Lucky::ShaderProgram> shader;
    double totalTime;
};

void SDLCALL LuckyGetConfig(Lucky::ConfigData *configData)
{
    configData->screenWidth = 1920;
    configData->screenHeight = 1080;
    configData->windowTitle = "Shader Example";
}

SDL_AppResult SDLCALL LuckyInitialize(Lucky::GameData *gameData)
{
    ExampleData *exampleData = new ExampleData();

    exampleData->batchRenderer = std::make_unique<Lucky::BatchRenderer>(gameData->graphicsDevice, 1000);

    Lucky::VertexShader vertexShader("Basic.vert");
    Lucky::FragmentShader fragmentShader("ShaderArt.frag");
    exampleData->shader =
        std::make_shared<Lucky::ShaderProgram>(gameData->graphicsDevice, vertexShader, fragmentShader);
    exampleData->totalTime = 0;

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

    exampleData->totalTime += deltaSeconds;

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDLCALL LuckyDraw(Lucky::GameData *gameData)
{
    ExampleData *exampleData = (ExampleData *)gameData->userData;

    gameData->graphicsDevice->BeginFrame();
    gameData->graphicsDevice->ClearScreen(Lucky::Color::Black);

    const int width = gameData->graphicsDevice->GetScreenWidth();
    const int height = gameData->graphicsDevice->GetScreenHeight();

    exampleData->batchRenderer->Begin(Lucky::BlendMode::None, nullptr, exampleData->shader);
    exampleData->shader->SetParameter("iResolution", glm::vec2((float)width, (float)height));
    exampleData->shader->SetParameter("iTime", (float)exampleData->totalTime);
    exampleData->batchRenderer->BatchQuadUV(glm::vec2(0.0f, 0.0f), glm::vec2((float)width, (float)height),
        glm::vec2(0.0, 0.0f), glm::vec2((float)width, (float)height), Lucky::Color::White);
    exampleData->batchRenderer->End();

    gameData->graphicsDevice->EndFrame();

    return SDL_APP_CONTINUE;
}

void SDLCALL LuckyQuit(Lucky::GameData *gameData)
{
    ExampleData *exampleData = (ExampleData *)gameData->userData;

    exampleData->shader.reset();
    exampleData->batchRenderer.reset();
    delete exampleData;

    gameData->userData = nullptr;
}
