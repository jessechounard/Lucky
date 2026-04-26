#include <SDL3/SDL.h>

#include <Lucky/BatchRenderer.hpp>
#include <Lucky/Color.hpp>
#include <Lucky/GraphicsDevice.hpp>

#include "../DemoBase.hpp"
#include "../DemoRegistry.hpp"

namespace {

// The classic "hello triangle" from any new rendering API: a single
// triangle with red, green, and blue vertices so you can see color
// interpolation across the surface. The whole thing is done through
// BatchRenderer.BatchTriangles, which takes raw vertices with per-vertex
// color. Lucky's default sprite shader samples an internal white pixel
// and multiplies by vertex color, so the output is just the interpolated
// vertex colors.
class HelloTriangleDemo : public LuckyDemos::DemoBase {
  public:
    explicit HelloTriangleDemo(SDL_Window *window)
        : graphicsDevice(window), batchRenderer(graphicsDevice, 1) {
        graphicsDevice.SetClearColor({0.08f, 0.08f, 0.12f, 1.0f});
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
            // Swapchain unavailable (for example, the window is minimized).
            return;
        }

        const float screenWidth = static_cast<float>(graphicsDevice.GetScreenWidth());
        const float screenHeight = static_cast<float>(graphicsDevice.GetScreenHeight());

        // Center the triangle in the window. BatchRenderer uses a Y-up
        // projection (y = 0 at the bottom of the screen), so the top
        // vertex has the largest Y and the bottom vertices have the
        // smallest Y.
        const float centerX = screenWidth * 0.5f;
        const float topY = screenHeight * 0.85f;
        const float bottomY = screenHeight * 0.20f;
        const float leftX = screenWidth * 0.20f;
        const float rightX = screenWidth * 0.80f;

        Lucky::BatchVertex triangle[3] = {
            // Top vertex -- red
            {centerX, topY, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f},
            // Bottom-left vertex -- green
            {leftX, bottomY, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
            // Bottom-right vertex -- blue
            {rightX, bottomY, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f},
        };

        // Begin with no explicit texture: BatchRenderer binds its internal
        // 1x1 white pixel and uses the default sprite shader. The shader
        // samples the white pixel and multiplies by vertex color, so each
        // pixel of the triangle shows the interpolated vertex color.
        batchRenderer.Begin(Lucky::BlendMode::Alpha);
        batchRenderer.BatchTriangles(triangle, 1);
        batchRenderer.End();

        graphicsDevice.EndFrame();
    }

  private:
    Lucky::GraphicsDevice graphicsDevice;
    Lucky::BatchRenderer batchRenderer;
};

} // namespace

LUCKY_DEMO(
    "HelloTriangle", "Classic RGB-interpolated triangle via BatchTriangles", HelloTriangleDemo);
