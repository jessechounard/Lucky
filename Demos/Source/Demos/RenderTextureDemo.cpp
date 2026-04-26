#include <SDL3/SDL.h>
#include <memory>
#include <vector>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/vec2.hpp>

#include <Lucky/BatchRenderer.hpp>
#include <Lucky/Color.hpp>
#include <Lucky/GraphicsDevice.hpp>
#include <Lucky/Texture.hpp>
#include <Lucky/Types.hpp>

#include "../DemoBase.hpp"
#include "../DemoRegistry.hpp"

namespace {

// Bouncing colored squares at full screen size, with a small picture-in-
// picture inset in the top-right corner that mirrors the same scene at
// quarter resolution. The scene is drawn twice each frame: once to the
// swapchain at 1:1, once to a smaller render texture with a 0.25x model
// transform, then the texture is composited back as the inset.
//
// Demonstrates GraphicsDevice::BindRenderTarget / UnbindRenderTarget plus
// rendering the same content to two targets at different scales.
class RenderTextureDemo : public LuckyDemos::DemoBase {
  public:
    explicit RenderTextureDemo(SDL_Window *window)
        : graphicsDevice(window), batchRenderer(graphicsDevice, 64) {
        graphicsDevice.SetClearColor(Lucky::Color::CornflowerBlue);

        screenWidth = static_cast<float>(graphicsDevice.GetScreenWidth());
        screenHeight = static_cast<float>(graphicsDevice.GetScreenHeight());

        minimapWidth = static_cast<uint32_t>(screenWidth * minimapScale);
        minimapHeight = static_cast<uint32_t>(screenHeight * minimapScale);

        minimapRenderTexture = std::make_unique<Lucky::Texture>(graphicsDevice,
            Lucky::TextureType::RenderTarget,
            minimapWidth,
            minimapHeight,
            nullptr,
            0);

        balls = {
            {{200, 200}, {180, 140}, Lucky::Color::Red, 30},
            {{500, 350}, {-220, 110}, Lucky::Color::Green, 40},
            {{800, 150}, {130, -180}, Lucky::Color::Blue, 25},
            {{300, 500}, {-160, -200}, Lucky::Color::Yellow, 35},
            {{700, 450}, {200, 170}, {1.0f, 0.4f, 0.8f, 1.0f}, 28},
        };
    }

    void HandleEvent(const SDL_Event &event) override {
        if (event.type != SDL_EVENT_KEY_DOWN || event.key.repeat) {
            return;
        }
        switch (event.key.key) {
        case SDLK_ESCAPE:
            RequestExit();
            break;
        case SDLK_PAGEDOWN:
            RequestNextDemo();
            break;
        case SDLK_PAGEUP:
            RequestPrevDemo();
            break;
        }
    }

    void Update(float deltaSeconds) override {
        for (Ball &ball : balls) {
            ball.position += ball.velocity * deltaSeconds;

            if (ball.position.x - ball.radius < 0) {
                ball.position.x = ball.radius;
                ball.velocity.x = -ball.velocity.x;
            } else if (ball.position.x + ball.radius > screenWidth) {
                ball.position.x = screenWidth - ball.radius;
                ball.velocity.x = -ball.velocity.x;
            }

            if (ball.position.y - ball.radius < 0) {
                ball.position.y = ball.radius;
                ball.velocity.y = -ball.velocity.y;
            } else if (ball.position.y + ball.radius > screenHeight) {
                ball.position.y = screenHeight - ball.radius;
                ball.velocity.y = -ball.velocity.y;
            }
        }
    }

    void Draw() override {
        graphicsDevice.BeginFrame();
        if (!graphicsDevice.GetCommandBuffer()) {
            return;
        }

        // Step 1: render the scene into the minimap render texture. The
        // texture's viewport is minimapWidth x minimapHeight, so we apply a
        // 0.25x model scale to fit screen-space ball positions into it.
        graphicsDevice.BindRenderTarget(*minimapRenderTexture);
        const glm::mat4 minimapTransform =
            glm::scale(glm::mat4(1.0f), glm::vec3(minimapScale, minimapScale, 1.0f));
        DrawBalls(minimapTransform);

        // Step 2: render the scene full-size to the swapchain.
        graphicsDevice.UnbindRenderTarget();
        DrawBalls(glm::mat4(1.0f));

        // Step 3: composite the minimap onto the swapchain in the top-right
        // corner, with a thin black border to set it off from the scene.
        const float marginX = 24.0f;
        const float marginY = 24.0f;
        const float borderThickness = 4.0f;
        const float minimapX0 = screenWidth - minimapWidth - marginX;
        const float minimapY0 = screenHeight - minimapHeight - marginY;
        const float minimapX1 = minimapX0 + minimapWidth;
        const float minimapY1 = minimapY0 + minimapHeight;

        // Border behind the minimap, drawn as a solid quad.
        batchRenderer.Begin(Lucky::BlendMode::Alpha);
        batchRenderer.BatchQuadUV({0.0f, 0.0f},
            {1.0f, 1.0f},
            {minimapX0 - borderThickness, minimapY0 - borderThickness},
            {minimapX1 + borderThickness, minimapY1 + borderThickness},
            Lucky::Color::Black);
        batchRenderer.End();

        // Minimap on top of the border. UVs are V-flipped because the RT
        // was rendered with BatchRenderer's Y-up projection while textures
        // are stored Y-down.
        batchRenderer.Begin(Lucky::BlendMode::None, *minimapRenderTexture);
        batchRenderer.BatchQuadUV({0.0f, 1.0f},
            {1.0f, 0.0f},
            {minimapX0, minimapY0},
            {minimapX1, minimapY1},
            Lucky::Color::White);
        batchRenderer.End();

        graphicsDevice.EndFrame();
    }

  private:
    struct Ball {
        glm::vec2 position;
        glm::vec2 velocity;
        Lucky::Color color;
        float radius;
    };

    void DrawBalls(const glm::mat4 &transformMatrix) {
        batchRenderer.Begin(Lucky::BlendMode::Alpha, transformMatrix);
        for (const Ball &ball : balls) {
            batchRenderer.BatchQuadUV({0.0f, 0.0f},
                {1.0f, 1.0f},
                {ball.position.x - ball.radius, ball.position.y - ball.radius},
                {ball.position.x + ball.radius, ball.position.y + ball.radius},
                ball.color);
        }
        batchRenderer.End();
    }

    Lucky::GraphicsDevice graphicsDevice;
    Lucky::BatchRenderer batchRenderer;

    std::unique_ptr<Lucky::Texture> minimapRenderTexture;

    std::vector<Ball> balls;

    float screenWidth = 0;
    float screenHeight = 0;
    uint32_t minimapWidth = 0;
    uint32_t minimapHeight = 0;

    static constexpr float minimapScale = 0.25f;
};

} // namespace

LUCKY_DEMO("RenderTexture", "Bouncing squares with a quarter-scale picture-in-picture minimap",
    RenderTextureDemo);
