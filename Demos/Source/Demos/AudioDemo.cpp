#include <cstdio>
#include <filesystem>
#include <memory>
#include <string>

#include <SDL3/SDL.h>
#include <SDL3/SDL_filesystem.h>

#include <glm/gtc/matrix_transform.hpp>

#include <Lucky/AudioPlayer.hpp>
#include <Lucky/Color.hpp>
#include <Lucky/GraphicsDevice.hpp>
#include <Lucky/SlugFont.hpp>
#include <Lucky/SlugRenderer.hpp>
#include <Lucky/Sound.hpp>
#include <Lucky/Stream.hpp>

#include "../DemoBase.hpp"
#include "../DemoRegistry.hpp"

namespace {

constexpr float VolumeStep = 0.1f;

class AudioDemo : public LuckyDemos::DemoBase {
  public:
    explicit AudioDemo(SDL_Window *window)
        : graphicsDevice(window), slugRenderer(graphicsDevice, 1024),
          font(graphicsDevice,
              (std::filesystem::path(SDL_GetBasePath()) / "Content/Fonts/MouseMemoirs-Regular.ttf")
                  .generic_string()) {
        graphicsDevice.SetClearColor({0.08f, 0.08f, 0.12f, 1.0f});

        std::string basePath = SDL_GetBasePath();

        music = std::make_shared<Lucky::Stream>(basePath + "Content/Audio/The Complex.mp3");
        sfx1 = std::make_shared<Lucky::Sound>(basePath + "Content/Audio/laserSmall_002.wav");
        sfx2 = std::make_shared<Lucky::Sound>(basePath + "Content/Audio/explosionCrunch_000.wav");

        PreloadText();
    }

    void HandleEvent(const SDL_Event &event) override {
        if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat) {
            switch (event.key.key) {
            case SDLK_ESCAPE:
                RequestExit();
                break;
            case SDLK_SPACE:
                ToggleMusic();
                break;
            case SDLK_1:
                audioPlayer.Play(sfx1);
                break;
            case SDLK_2:
                audioPlayer.Play(sfx2);
                break;
            case SDLK_UP: {
                float vol = audioPlayer.GetMasterVolume();
                audioPlayer.SetMasterVolume(std::min(vol + VolumeStep, 1.0f));
                break;
            }
            case SDLK_DOWN: {
                float vol = audioPlayer.GetMasterVolume();
                audioPlayer.SetMasterVolume(std::max(vol - VolumeStep, 0.0f));
                break;
            }
            }
        }
    }

    void Update(float /*deltaSeconds*/) override {
        audioPlayer.Update();
    }

    void Draw() override {
        graphicsDevice.BeginFrame();
        if (!graphicsDevice.GetCommandBuffer()) {
            return;
        }

        const float screenWidth = static_cast<float>(graphicsDevice.GetScreenWidth());
        const float screenHeight = static_cast<float>(graphicsDevice.GetScreenHeight());

        glm::mat4 mvp = glm::ortho(0.0f, screenWidth, 0.0f, screenHeight);
        slugRenderer.Begin(Lucky::BlendMode::Alpha, font, mvp);

        const float fontSize = 32.0f;
        const float lineHeight = font.GetLineHeight(fontSize);
        const float margin = 32.0f;
        float y = screenHeight - margin - fontSize;

        auto drawLine = [&](const char *text, Lucky::Color color = Lucky::Color::White) {
            font.DrawString(slugRenderer, text, margin, y, fontSize, color);
            y -= lineHeight;
        };

        drawLine("Audio Demo", Lucky::Color::Yellow);
        y -= lineHeight * 0.5f;

        drawLine("[Space]  Play / Stop music");
        drawLine("[1]      Sound effect 1");
        drawLine("[2]      Sound effect 2");
        drawLine("[Up]     Volume up");
        drawLine("[Down]   Volume down");

        y -= lineHeight * 0.5f;

        Lucky::AudioState musicState = audioPlayer.GetState(musicRef);
        const char *stateText = (musicState == Lucky::AudioState::Playing) ? "Playing" : "Stopped";
        char statusLine[128];
        std::snprintf(statusLine, sizeof(statusLine), "Music: %s", stateText);
        drawLine(statusLine,
            (musicState == Lucky::AudioState::Playing) ? Lucky::Color::Green : Lucky::Color::Gray);

        int volumePercent = static_cast<int>(audioPlayer.GetMasterVolume() * 100.0f + 0.5f);
        std::snprintf(statusLine, sizeof(statusLine), "Volume: %d%%", volumePercent);
        drawLine(statusLine);

        slugRenderer.End();
        graphicsDevice.EndFrame();
    }

  private:
    void ToggleMusic() {
        Lucky::AudioState state = audioPlayer.GetState(musicRef);
        if (state == Lucky::AudioState::Playing) {
            audioPlayer.Stop(musicRef);
            musicRef = 0;
        } else {
            music = std::make_shared<Lucky::Stream>(*music);
            musicRef = audioPlayer.Play(music, "default", true);
        }
    }

    void PreloadText() {
        font.PreloadString("Audio Demo");
        font.PreloadString("[Space]  Play / Stop music");
        font.PreloadString("[1]      Sound effect 1");
        font.PreloadString("[2]      Sound effect 2");
        font.PreloadString("[Up]     Volume up");
        font.PreloadString("[Down]   Volume down");
        font.PreloadString("Music: Playing");
        font.PreloadString("Music: Stopped");
        font.PreloadString("Volume: 100%");
    }

    Lucky::GraphicsDevice graphicsDevice;
    Lucky::SlugRenderer slugRenderer;
    Lucky::SlugFont font;
    Lucky::AudioPlayer audioPlayer;

    std::shared_ptr<Lucky::Stream> music;
    std::shared_ptr<Lucky::Sound> sfx1;
    std::shared_ptr<Lucky::Sound> sfx2;
    Lucky::AudioRef musicRef = 0;
};

} // namespace

LUCKY_DEMO("Audio", "Sound effects and streaming music playback", AudioDemo);
