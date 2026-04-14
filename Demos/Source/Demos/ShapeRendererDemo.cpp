#include <filesystem>

#include <SDL3/SDL.h>
#include <SDL3/SDL_filesystem.h>

#include <Lucky/BatchRenderer.hpp>
#include <Lucky/Color.hpp>
#include <Lucky/GraphicsDevice.hpp>
#include <Lucky/Shader.hpp>
#include <Lucky/ShapeRenderer.hpp>

#include "../DemoBase.hpp"
#include "../DemoRegistry.hpp"

namespace {

class ShapeRendererDemo : public LuckyDemos::DemoBase {
  public:
    explicit ShapeRendererDemo(SDL_Window *window)
        : graphicsDevice(window), batchRenderer(graphicsDevice, 1024), shapeRenderer(batchRenderer),
          sdfShader(graphicsDevice,
              (std::filesystem::path(SDL_GetBasePath()) / "Content/Shaders/sdf_outline.frag")
                  .generic_string(),
              SDL_GPU_SHADERSTAGE_FRAGMENT) {
        graphicsDevice.SetClearColor({0.1f, 0.1f, 0.15f, 1.0f});
    }

    void HandleEvent(const SDL_Event &event) override {
        if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE) {
            RequestExit();
        }
    }

    void Update(float deltaSeconds) override {
        time += deltaSeconds;
    }

    void Draw() override {
        graphicsDevice.BeginFrame();
        if (!graphicsDevice.GetCommandBuffer()) {
            // Swapchain unavailable (window minimized, for example).
            return;
        }

        const float screenWidth = static_cast<float>(graphicsDevice.GetScreenWidth());
        const float screenHeight = static_cast<float>(graphicsDevice.GetScreenHeight());

        // Arrange seven shapes evenly across the width of the window.
        constexpr int shapeCount = 7;
        const float spacing = screenWidth / (shapeCount + 1);
        const float y = screenHeight * 0.5f;
        const float radius = spacing * 0.35f;
        const float thickness = 3.0f;
        const float rotation = time * 0.5f;

        shapeRenderer.Begin(Lucky::BlendMode::Alpha, sdfShader);

        // 1: Line
        shapeRenderer.DrawLine({spacing * 1 - radius, y - radius},
            {spacing * 1 + radius, y + radius},
            rotation,
            Lucky::Color::White,
            thickness);

        // 2: Circle
        shapeRenderer.DrawCircle({spacing * 2, y}, radius, Lucky::Color::Red, thickness);

        // 3: Rectangle
        shapeRenderer.DrawRectangle({spacing * 3, y},
            {radius * 2, radius * 1.4f},
            rotation,
            Lucky::Color::Green,
            thickness);

        // 4: Triangle
        shapeRenderer.DrawTriangle({spacing * 4, y - radius},
            {spacing * 4 + radius, y + radius},
            {spacing * 4 - radius, y + radius},
            rotation,
            Lucky::Color::Blue,
            thickness);

        // 5: Diamond
        shapeRenderer.DrawDiamond({spacing * 5, y},
            radius * 1.6f,
            radius * 2.0f,
            rotation,
            Lucky::Color::CornflowerBlue,
            thickness);

        // 6: Regular polygon (hexagon)
        shapeRenderer.DrawRegularPolygon(
            {spacing * 6, y}, radius, 6, rotation, Lucky::Color::White, thickness);

        // 7: Star
        shapeRenderer.DrawStar(
            {spacing * 7, y}, radius, radius * 0.5f, 5, rotation, Lucky::Color::Red, thickness);

        shapeRenderer.End();

        graphicsDevice.EndFrame();
    }

  private:
    Lucky::GraphicsDevice graphicsDevice;
    Lucky::BatchRenderer batchRenderer;
    Lucky::ShapeRenderer shapeRenderer;
    Lucky::Shader sdfShader;

    float time = 0.0f;
};

} // namespace

LUCKY_DEMO("ShapeRenderer", "Draws each shape supported by ShapeRenderer", ShapeRendererDemo);
