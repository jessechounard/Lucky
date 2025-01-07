#include <memory>

#define LUCKY_MAIN
#include <Lucky/Graphics/BatchRenderer.hpp>
#include <Lucky/Graphics/Font.hpp>
#include <Lucky/LuckyMain.hpp>

struct ExampleData
{
    std::unique_ptr<Lucky::BatchRenderer> batchRenderer;
    std::shared_ptr<Lucky::Font> font;
};

void SDLCALL LuckyGetConfig(Lucky::ConfigData *configData)
{
    configData->screenWidth = 1920;
    configData->screenHeight = 1080;
    configData->windowTitle = "Font Example";
}

SDL_AppResult SDLCALL LuckyInitialize(Lucky::GameData *gameData)
{
    ExampleData *exampleData = new ExampleData();

    exampleData->batchRenderer = std::make_unique<Lucky::BatchRenderer>(gameData->graphicsDevice, 1000);

    exampleData->font = std::make_shared<Lucky::Font>("Raleway-Regular.ttf");
    int codePoints[] = {'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's',
        't', 'u', 'v', 'w', 'x', 'y', 'z', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O',
        'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', ' ', '!', ','};
    exampleData->font->CreateFontEntry("font_entry", 100, codePoints, 55, 2);

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
    gameData->graphicsDevice->ClearScreen(Lucky::Color::Black);

    const int width = gameData->graphicsDevice->GetScreenWidth();
    const int height = gameData->graphicsDevice->GetScreenHeight();

    exampleData->batchRenderer->Begin(Lucky::BlendMode::Alpha, exampleData->font->GetTexture("font_entry"));
    exampleData->font->DrawString(
        *exampleData->batchRenderer, "Rendering fonts!", "font_entry", 620, 560, Lucky::Color::White);
    exampleData->batchRenderer->End();

    gameData->graphicsDevice->EndFrame();

    return SDL_APP_CONTINUE;
}

void SDLCALL LuckyQuit(Lucky::GameData *gameData)
{
    ExampleData *exampleData = (ExampleData *)gameData->userData;

    exampleData->font.reset();
    exampleData->batchRenderer.reset();
    delete exampleData;

    gameData->userData = nullptr;
}
