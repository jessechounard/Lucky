#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include <glm/vec2.hpp>

#include <Lucky/BatchRenderer.hpp>
#include <Lucky/Color.hpp>
#include <Lucky/TextureAtlas.hpp>

namespace Lucky {

struct SpriteAnimation {
    SpriteAnimation(TextureAtlas &textureAtlas, const std::string &name, int frameCount,
        float duration, bool loop);

    TextureAtlas *atlas;
    std::vector<TextureRegion> frames;
    float duration;
    bool loop;
};

class SpriteAnimationPlayer {
  public:
    void AddAnimation(const std::string &name, std::shared_ptr<SpriteAnimation> animation);

    void Play(const std::string &name, float startOffset = 0.0f);
    void Pause();

    float GetTime() const {
        return time;
    }
    bool IsComplete() const;

    void Update(float frameTime);

    // This assumes that the batch is already started, which means that custom shaders and transform
    // matrix is sorted. This is just a basic sprite render from our perspective.
    void Render(BatchRenderer &batchRenderer, const glm::vec2 &position, float rotation,
        const glm::vec2 &scale, const Color &color);

  private:
    std::map<std::string, std::shared_ptr<SpriteAnimation>> animations;
    SpriteAnimation *currentAnimation = nullptr;
    float time = 0;
    bool paused = false;
};

} // namespace Lucky
