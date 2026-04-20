#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include <SDL3/SDL.h>
#include <SDL3/SDL_filesystem.h>

#include <glm/vec2.hpp>

#include <Lucky/BatchRenderer.hpp>
#include <Lucky/Color.hpp>
#include <Lucky/GraphicsDevice.hpp>
#include <Lucky/SpriteAnimation.hpp>
#include <Lucky/Texture.hpp>
#include <Lucky/TextureAtlas.hpp>

#include "../DemoBase.hpp"
#include "../DemoRegistry.hpp"

namespace {

constexpr float CycleDuration = 0.8f;
constexpr float RenderScale = 3.0f;

// The atlas packs the zombie's eight walk frames unrotated and the robot's
// eight frames rotated 90° CW by TexturePacker. Rendering both animations
// through the same BatchRenderer batch exercises Lucky's rotated-UV path
// side-by-side with the plain path, so any regression in either shows up
// as a visibly wrong sprite.
class SpriteAnimationDemo : public LuckyDemos::DemoBase {
  public:
    explicit SpriteAnimationDemo(SDL_Window *window)
        : graphicsDevice(window), batchRenderer(graphicsDevice, 16),
          atlas((std::filesystem::path(SDL_GetBasePath()) /
                 "Content/Graphics/character_zombie_walk_atlas.json")
                  .generic_string()),
          texture(graphicsDevice,
              (std::filesystem::path(SDL_GetBasePath()) / atlas.TexturePath()).generic_string(),
              Lucky::TextureFilter::Point) {
        graphicsDevice.SetClearColor({0.08f, 0.08f, 0.12f, 1.0f});

        auto zombieClip = std::make_shared<Lucky::SpriteAnimation>(atlas,
            std::vector<std::string>{"character_zombie_walk0.png",
                "character_zombie_walk1.png",
                "character_zombie_walk2.png",
                "character_zombie_walk3.png",
                "character_zombie_walk4.png",
                "character_zombie_walk5.png",
                "character_zombie_walk6.png",
                "character_zombie_walk7.png"},
            CycleDuration,
            true);

        auto robotClip = std::make_shared<Lucky::SpriteAnimation>(atlas,
            std::vector<std::string>{"character_robot_walk0.png",
                "character_robot_walk1.png",
                "character_robot_walk2.png",
                "character_robot_walk3.png",
                "character_robot_walk4.png",
                "character_robot_walk5.png",
                "character_robot_walk6.png",
                "character_robot_walk7.png"},
            CycleDuration,
            true);

        zombiePlayer.AddAnimation("walk", zombieClip);
        zombiePlayer.Play("walk");

        // Phase-shift the robot by half a cycle so the two characters step
        // out of sync. This also exercises Play()'s startOffset path.
        robotPlayer.AddAnimation("walk", robotClip);
        robotPlayer.Play("walk", CycleDuration * 0.5f);
    }

    void HandleEvent(const SDL_Event &event) override {
        if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE) {
            RequestExit();
        }
    }

    void Update(float deltaSeconds) override {
        zombiePlayer.Update(deltaSeconds);
        robotPlayer.Update(deltaSeconds);
    }

    void Draw() override {
        graphicsDevice.BeginFrame();
        if (!graphicsDevice.GetCommandBuffer()) {
            return;
        }

        const float screenWidth = static_cast<float>(graphicsDevice.GetScreenWidth());
        const float screenHeight = static_cast<float>(graphicsDevice.GetScreenHeight());

        const float y = screenHeight * 0.5f;
        const glm::vec2 zombiePosition{screenWidth * (1.0f / 3.0f), y};
        const glm::vec2 robotPosition{screenWidth * (2.0f / 3.0f), y};

        batchRenderer.Begin(Lucky::BlendMode::Alpha, texture);
        zombiePlayer.Render(
            batchRenderer, zombiePosition, 0.0f, glm::vec2(2.0f, 2.0f), Lucky::Color::White);
        robotPlayer.Render(
            batchRenderer, robotPosition, 0.0f, glm::vec2(-3.0f, 3.0f), Lucky::Color::White);
        batchRenderer.End();

        graphicsDevice.EndFrame();
    }

  private:
    Lucky::GraphicsDevice graphicsDevice;
    Lucky::BatchRenderer batchRenderer;
    Lucky::TextureAtlas atlas;
    Lucky::Texture texture;
    Lucky::SpriteAnimationPlayer zombiePlayer;
    Lucky::SpriteAnimationPlayer robotPlayer;
};

} // namespace

LUCKY_DEMO("SpriteAnimation",
    "Two walk-cycle sprite animations, phase-shifted; exercises rotated + non-rotated atlas frames",
    SpriteAnimationDemo);
