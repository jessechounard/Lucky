#include <algorithm>
#include <cmath>

#include <SDL3/SDL_assert.h>

#include <Lucky/SpriteAnimation.hpp>

namespace Lucky {

SpriteAnimation::SpriteAnimation(
    TextureAtlas &atlas, const std::vector<std::string> &frameNames, float duration, bool loop)
    : duration(duration), loop(loop) {
    SDL_assert(!frameNames.empty());
    SDL_assert(duration > 0.0f);

    frames.reserve(frameNames.size());
    for (const auto &name : frameNames) {
        frames.push_back(atlas.GetRegion(name));
    }
}

void SpriteAnimationPlayer::AddAnimation(
    const std::string &name, std::shared_ptr<SpriteAnimation> animation) {
    SDL_assert(animation != nullptr);
    animations[name] = std::move(animation);
}

void SpriteAnimationPlayer::Play(const std::string &name, float startOffset) {
    SDL_assert(startOffset >= 0.0f);

    auto iter = animations.find(name);
    SDL_assert(iter != animations.end());
    SDL_assert(iter->second != nullptr);

    currentAnimation = iter->second.get();

    if (currentAnimation->IsLooping()) {
        time = std::fmodf(startOffset, currentAnimation->GetDuration());
    } else {
        time = std::min(startOffset, currentAnimation->GetDuration());
    }

    paused = false;
}

void SpriteAnimationPlayer::Pause() {
    paused = true;
}

void SpriteAnimationPlayer::Resume() {
    paused = false;
}

bool SpriteAnimationPlayer::IsComplete() const {
    return currentAnimation != nullptr && !currentAnimation->IsLooping() &&
           time >= currentAnimation->GetDuration();
}

void SpriteAnimationPlayer::Update(float frameTime) {
    SDL_assert(frameTime >= 0.0f);

    if (currentAnimation == nullptr || paused) {
        return;
    }

    time += frameTime;

    if (currentAnimation->IsLooping()) {
        time = std::fmodf(time, currentAnimation->GetDuration());
    } else {
        time = std::min(time, currentAnimation->GetDuration());
    }
}

void SpriteAnimationPlayer::Render(BatchRenderer &batchRenderer, const glm::vec2 &position,
    float rotation, const glm::vec2 &scale, const Color &color) {
    SDL_assert(batchRenderer.BatchStarted());

    if (currentAnimation == nullptr) {
        return;
    }

    const auto &frames = currentAnimation->Frames();
    SDL_assert(!frames.empty());

    const float frameDuration = currentAnimation->GetDuration() / frames.size();
    const size_t frameNumber = std::min(size_t(time / frameDuration), frames.size() - 1);

    const auto &frame = frames[frameNumber];
    const UVMode uvMode = frame.rotated ? UVMode::RotatedCW90 : UVMode::Normal;

    batchRenderer.BatchSprite(&frame.bounds, position, rotation, scale, frame.pivot, uvMode, color);
}

} // namespace Lucky
