#include <SDL3/SDL_assert.h>

#include <Lucky/SpriteAnimation.hpp>

namespace Lucky {

SpriteAnimation::SpriteAnimation(
    TextureAtlas &atlas, const std::string &name, int frameCount, float duration, bool loop)
    : atlas(&atlas), duration(duration), loop(loop) {
    SDL_assert(name != "");
    SDL_assert(frameCount > 0);
    SDL_assert(duration > 0);

    // look through atlas to get all the regions
    for (int i = 1; i <= frameCount; i++) {
        const std::string frameName = name + std::to_string(i);
        frames.push_back(atlas.GetRegionNoExt(frameName));
    }
}

void SpriteAnimationPlayer::AddAnimation(
    const std::string &name, std::shared_ptr<SpriteAnimation> animation) {
    animations[name] = animation;
}

void SpriteAnimationPlayer::Play(const std::string &name, float startOffset) {
    auto iter = animations.find(name);
    if (iter == animations.end() || iter->second == nullptr) {
        return;
    }

    currentAnimation = iter->second.get();
    time = startOffset;

    if (currentAnimation->loop) {
        time = std::fmodf(startOffset, currentAnimation->duration);
    }

    paused = false;
}

void SpriteAnimationPlayer::Pause() {
    paused = true;
}

bool SpriteAnimationPlayer::IsComplete() const {
    return currentAnimation != nullptr && currentAnimation->loop == false &&
           time >= currentAnimation->duration;
}

void SpriteAnimationPlayer::Update(float frameTime) {
    if (currentAnimation == nullptr || paused) {
        return;
    }

    time += frameTime;

    if (currentAnimation->loop) {
        time = std::fmodf(time, currentAnimation->duration);
    } else {
        time = std::min(time, currentAnimation->duration);
    }
}

void SpriteAnimationPlayer::Render(BatchRenderer &batchRenderer, const glm::vec2 &position,
    float rotation, const glm::vec2 &scale, const Color &color) {
    SDL_assert(batchRenderer.BatchStarted());

    if (currentAnimation == nullptr) {
        return;
    }

    float frameDuration = currentAnimation->duration / currentAnimation->frames.size();
    size_t frameNumber =
        std::min(size_t(time / frameDuration), currentAnimation->frames.size() - 1);

    auto frame = currentAnimation->frames[frameNumber];
    UVMode uvMode = frame.rotated ? UVMode::RotatedCW90 : UVMode::Normal;

    batchRenderer.BatchQuad(&frame.bounds, position, rotation, scale, frame.pivot, uvMode, color);
}

} // namespace Lucky
