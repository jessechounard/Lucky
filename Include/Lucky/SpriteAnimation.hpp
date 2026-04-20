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

/**
 * An ordered sequence of atlas frames that together form one animation.
 *
 * The frames come from a `TextureAtlas`; the caller passes the exact frame
 * names (matching the atlas's `filename` entries) in the order they should
 * play. A `SpriteAnimation` stores only the resolved `TextureRegion`s — it
 * keeps no reference to the atlas after construction, so the atlas can be
 * destroyed independently.
 *
 * # Usage
 *
 * Typically, create one `SpriteAnimation` per named clip and hand it to a
 * `SpriteAnimationPlayer` that tracks playback state:
 *
 *     TextureAtlas atlas("Content/Atlases/hero.json");
 *     auto run = std::make_shared<SpriteAnimation>(
 *         atlas,
 *         std::vector<std::string>{
 *             "Hero_Run_00.png", "Hero_Run_01.png",
 *             "Hero_Run_02.png", "Hero_Run_03.png"},
 *         0.4f,
 *         true);
 *
 *     SpriteAnimationPlayer player;
 *     player.AddAnimation("run", run);
 *     player.Play("run");
 *
 * # Timing
 *
 * Frames play at a fixed rate: each frame shows for `duration / frameCount`
 * seconds. Variable per-frame timing (for example, holding one pose longer)
 * is not supported yet and may be added later — if your animation needs
 * non-uniform timing, repeat the relevant frame in the frame list as a
 * workaround.
 */
struct SpriteAnimation {
    /**
     * Constructs a SpriteAnimation from frames in the given atlas.
     *
     * Each entry in `frameNames` must match an atlas `filename` exactly
     * (including any extension). Frames are resolved up-front; the atlas
     * is not retained.
     *
     * \param textureAtlas the atlas to look frames up in. Does not need to
     *                     outlive the SpriteAnimation.
     * \param frameNames the atlas filenames of the frames, in playback
     *                   order. Must not be empty.
     * \param duration total animation length in seconds, split evenly
     *                 across frames. Must be positive.
     * \param loop whether playback wraps back to the start at the end.
     * \throws std::runtime_error (from TextureAtlas) if any frame name is
     *                            not in the atlas.
     */
    SpriteAnimation(TextureAtlas &textureAtlas, const std::vector<std::string> &frameNames,
        float duration, bool loop);

    /**
     * Returns the resolved frames in playback order.
     */
    const std::vector<TextureRegion> &Frames() const {
        return frames;
    }

    /**
     * Returns the total animation duration in seconds.
     */
    float GetDuration() const {
        return duration;
    }

    /**
     * Returns true if playback wraps back to the start at the end.
     */
    bool IsLooping() const {
        return loop;
    }

  private:
    std::vector<TextureRegion> frames;
    float duration;
    bool loop;
};

/**
 * Stateful player for named `SpriteAnimation` clips.
 *
 * Owns a name-to-animation map plus "currently playing" state (which clip,
 * how far along, whether it's paused). One player instance represents one
 * animated entity; entities with their own independent playback each need
 * their own player.
 *
 * # Usage
 *
 *     SpriteAnimationPlayer player;
 *     player.AddAnimation("idle", idleClip);
 *     player.AddAnimation("run",  runClip);
 *
 *     player.Play("run");
 *
 *     // Per frame:
 *     player.Update(frameSeconds);
 *     batchRenderer.Begin(BlendMode::Alpha, spriteSheet);
 *     player.Render(batchRenderer, position, rotation, scale, Color::White);
 *     batchRenderer.End();
 *
 * # Playback rules
 *
 * - `Play()` resets playback to `startOffset` (defaults to 0) and clears the
 *   paused flag.
 * - For a looping clip, `Update()` and `Play()`'s `startOffset` both wrap
 *   time with `fmod`, so any non-negative value is valid.
 * - For a non-looping clip, time is clamped to the clip's duration, so
 *   once the clip reaches the end `IsComplete()` returns true and
 *   `Render()` continues to show the final frame.
 * - `Pause()` stops `Update()` from advancing time. `Resume()` re-enables
 *   it without changing time or the current clip. `Play()` also implicitly
 *   clears the paused flag.
 *
 * # Thread safety
 *
 * Not thread-safe. A single player must be used from one thread at a time.
 */
class SpriteAnimationPlayer {
  public:
    /**
     * Registers an animation under the given name.
     *
     * If `name` already exists, the previous animation is replaced. The
     * animation is shared-owned: the player keeps a reference, and multiple
     * players may share the same clip.
     *
     * \param name the lookup key used by `Play()`.
     * \param animation the clip to register. Must not be null.
     */
    void AddAnimation(const std::string &name, std::shared_ptr<SpriteAnimation> animation);

    /**
     * Starts (or restarts) the named animation.
     *
     * Sets the current clip, resets time to `startOffset` (wrapped or
     * clamped depending on whether the clip loops), and clears the paused
     * flag. Calling `Play()` on a clip that is already current restarts it.
     *
     * \param name the animation name previously passed to `AddAnimation()`.
     *             Must be registered.
     * \param startOffset initial playback time in seconds. Must be
     *                    non-negative. Defaults to 0.
     */
    void Play(const std::string &name, float startOffset = 0.0f);

    /**
     * Stops `Update()` from advancing time.
     *
     * The current frame stays visible via `Render()`. `Resume()` or `Play()`
     * resume advancement.
     */
    void Pause();

    /**
     * Re-enables time advancement after `Pause()`.
     *
     * Has no effect if the player was not paused. Does not change the
     * current clip or time.
     */
    void Resume();

    /**
     * Returns the current playback time in seconds.
     */
    float GetTime() const {
        return time;
    }

    /**
     * Returns true once a non-looping animation has played to its end.
     *
     * Looping animations never report complete. Returns false when no
     * animation is currently playing.
     */
    bool IsComplete() const;

    /**
     * Advances playback by `frameTime` seconds.
     *
     * A no-op when no animation is playing or when paused. For a looping
     * clip, time wraps via `fmod`; for a non-looping clip, time is clamped
     * to the clip's duration.
     *
     * \param frameTime elapsed seconds since the last update. Must be
     *                  non-negative.
     */
    void Update(float frameTime);

    /**
     * Draws the current frame through the given BatchRenderer.
     *
     * The caller is responsible for configuring the batch: `Begin()` must
     * already have been called with the appropriate blend mode, texture
     * (the atlas image), and optional transform matrix. This method just
     * appends one quad for the current frame.
     *
     * A no-op when no animation is currently playing.
     *
     * \param batchRenderer the active batch. Must be between Begin() and
     *                      End().
     * \param position the destination position in the batch's coordinate
     *                 space, interpreted with the frame's pivot.
     * \param rotation rotation in radians applied around the pivot.
     * \param scale per-axis scale factor applied to the frame's original
     *              (pre-rotation) dimensions.
     * \param color a color multiplied into every sampled texel.
     */
    void Render(BatchRenderer &batchRenderer, const glm::vec2 &position, float rotation,
        const glm::vec2 &scale, const Color &color);

  private:
    std::map<std::string, std::shared_ptr<SpriteAnimation>> animations;
    SpriteAnimation *currentAnimation = nullptr;
    float time = 0.0f;
    bool paused = false;
};

} // namespace Lucky
