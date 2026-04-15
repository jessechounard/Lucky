#include <filesystem>

#include <SDL3/SDL.h>
#include <SDL3/SDL_filesystem.h>

#include <glm/gtc/matrix_transform.hpp>

#include <Lucky/Color.hpp>
#include <Lucky/GraphicsDevice.hpp>
#include <Lucky/SlugFont.hpp>
#include <Lucky/SlugRenderer.hpp>

#include "../DemoBase.hpp"
#include "../DemoRegistry.hpp"

namespace {

// All the text this demo draws. Declared up front so the constructor can
// preload exactly these strings and the Draw method can iterate the same
// list when rendering.
constexpr const char *TitleText = "Hello, World!";

constexpr const char *AttributionLines[] = {
    "\"Mouse Memoirs\" by Brian J. Bonislawsky (Astigmatic)",
    "Licensed under the SIL Open Font License, Version 1.1",
    "https://openfontlicense.org",
};
constexpr int AttributionLineCount = sizeof(AttributionLines) / sizeof(AttributionLines[0]);

// Renders a few lines of text with SlugFont to verify vector font rendering
// end-to-end. Exercises:
//   - SlugFont construction + PreloadString
//   - SlugRenderer::Begin / End with a pixel-space MVP matrix
//   - SlugFont::DrawString
//   - SlugFont::MeasureString for horizontal centering
//   - SlugFont::GetLineHeight for stacking multi-line text
class SlugTextDemo : public LuckyDemos::DemoBase {
  public:
    explicit SlugTextDemo(SDL_Window *window)
        : graphicsDevice(window), slugRenderer(graphicsDevice, 1024),
          font(graphicsDevice,
              (std::filesystem::path(SDL_GetBasePath()) / "Content/Fonts/MouseMemoirs-Regular.ttf")
                  .generic_string()) {
        graphicsDevice.SetClearColor({0.08f, 0.08f, 0.12f, 1.0f});

        // Preload exactly the glyphs the demo will render. A real app
        // typically knows its text at setup time and should prefer this
        // over PreloadAscii to avoid encoding glyphs it will never draw.
        font.PreloadString(TitleText);
        for (int i = 0; i < AttributionLineCount; i++) {
            font.PreloadString(AttributionLines[i]);
        }
    }

    void HandleEvent(const SDL_Event &event) override {
        if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE) {
            RequestExit();
        }
    }

    void Update(float /*deltaSeconds*/) override {
    }

    void Draw() override {
        graphicsDevice.BeginFrame();
        if (!graphicsDevice.GetCommandBuffer()) {
            return;
        }

        const float screenWidth = static_cast<float>(graphicsDevice.GetScreenWidth());
        const float screenHeight = static_cast<float>(graphicsDevice.GetScreenHeight());

        // SlugRenderer wants a pixel-space orthographic MVP that matches
        // Lucky's Y-up convention (y = 0 at the bottom of the screen).
        glm::mat4 mvp = glm::ortho(0.0f, screenWidth, 0.0f, screenHeight);

        slugRenderer.Begin(Lucky::BlendMode::Alpha, font, mvp);

        // Large title, centered near the top of the screen. MeasureString
        // gives us the pixel width so we can compute the left edge.
        {
            const float fontSize = 96.0f;
            glm::vec2 size = font.MeasureString(TitleText, fontSize);
            float x = (screenWidth - size.x) * 0.5f;
            float y = screenHeight * 0.7f;
            font.DrawString(slugRenderer, TitleText, x, y, fontSize, Lucky::Color::White);
        }

        // Font attribution, right-aligned in the bottom-right corner.
        // DrawString does not handle newlines, so each line is drawn
        // separately and positioned using MeasureString (for the right
        // edge) and GetLineHeight (for the vertical stacking).
        {
            const float fontSize = 20.0f;
            float lineHeight = font.GetLineHeight(fontSize);
            float margin = 16.0f;
            float rightEdge = screenWidth - margin;
            // Anchor the baseline of the last line a comfortable margin
            // above the bottom of the window, then stack earlier lines
            // upward.
            float lastBaselineY = margin - font.GetDescent(fontSize);

            for (int i = 0; i < AttributionLineCount; i++) {
                const char *text = AttributionLines[i];
                glm::vec2 size = font.MeasureString(text, fontSize);
                float x = rightEdge - size.x;
                float y = lastBaselineY + (AttributionLineCount - 1 - i) * lineHeight;
                font.DrawString(slugRenderer, text, x, y, fontSize, Lucky::Color::White);
            }
        }

        slugRenderer.End();

        graphicsDevice.EndFrame();
    }

  private:
    Lucky::GraphicsDevice graphicsDevice;
    Lucky::SlugRenderer slugRenderer;
    Lucky::SlugFont font;
};

} // namespace

LUCKY_DEMO("SlugText", "Vector font rendering with SlugFont", SlugTextDemo);
