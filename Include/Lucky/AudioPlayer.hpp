#pragma once

#include <functional>
#include <memory>
#include <stdint.h>
#include <string>
#include <vector>

#include <SDL3/SDL_audio.h>

#include <Lucky/Sound.hpp>
#include <Lucky/Stream.hpp>

namespace Lucky {

/**
 * Opaque handle identifying a single playback started by AudioPlayer::Play.
 *
 * 0 is reserved as an invalid sentinel returned when Play fails (e.g. unknown group in release
 * builds). A ref remains valid from Play() until the sound is stopped or runs to its natural end
 * with `canLoop == false`. After natural completion, the matching instance is removed and the ref
 * becomes stale. AudioPlayer silently ignores stale refs in
 * Pause/Resume/Stop/GetState/GetLoopCount.
 */
typedef uint32_t AudioRef;

/**
 * Current playback state for an AudioRef.
 */
enum class AudioState {
    Playing, /**< actively pulling samples and feeding SDL. */
    Paused,  /**< unbound from the SDL audio stream; preserves position. */
    Stopped, /**< not present in the player (also returned for stale refs). */
};

/**
 * Per-group device and volume configuration.
 */
struct SoundGroupSettings {
    SDL_AudioDeviceID deviceId =
        SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK; /**< SDL logical device id
                                             or SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK
                                             for the system default. */
    float volume = 1.0f;                   /**< linear gain applied to every instance in this
                                                group, multiplied with the master volume. */
};

/**
 * Entry in the list returned by AudioPlayer::GetAudioOutputDevices.
 */
struct AudioDevice {
    std::string name;           /**< human-readable device name, as reported by SDL. */
    SDL_AudioDeviceID deviceId; /**< pass to SoundGroupSettings::deviceId to route a group here. */
};

/**
 * Mixes Sound and Stream playback through SDL3 audio streams.
 *
 * AudioPlayer orchestrates playback of any number of concurrent sounds and
 * streams, routing them through one or more SDL audio devices. Each
 * playback is identified by an AudioRef returned from Play() and remains
 * live until it ends naturally, is explicitly Stop()'d, or its owning
 * AudioPlayer is destroyed.
 *
 * # Sound groups
 *
 * Every playback belongs to a named "sound group." A group is a routing and
 * volume bucket -- it owns one SDL audio device (opened via
 * SDL_OpenAudioDevice) and holds a linear volume multiplier applied to
 * every instance playing in it. Typical setups use groups like "music",
 * "sfx", and "ui" so each can be muted or faded independently.
 *
 * The group named `"default"` is created automatically by the constructor,
 * using the SoundGroupSettings you pass in. Any other group must be
 * registered explicitly via CreateSoundGroup() before it can be used. Play
 * and the *Group() helpers assert (SDL_assert) if given an unknown group
 * name; in release builds they fall back to "no audio" rather than silently
 * auto-creating a group with defaults.
 *
 * # Volume model
 *
 * Effective gain per instance = MasterVolume * GroupVolume. Both sit in
 * linear [0, 1] by convention (values above 1 are allowed but may clip).
 *
 * # Update cadence
 *
 * Update() must be called regularly from the application's main loop. It
 * tops up SDL's queue with the next batch of samples for every active
 * playback and marks instances as Stopped once their source is exhausted.
 * The queue depth is ~1/30 second, so callers must tick Update at or above
 * 30 Hz to avoid audible gaps.
 *
 * # AudioRef lifetime
 *
 * An AudioRef is valid from the moment Play() returns it until either:
 *   - the caller calls Stop(ref), or
 *   - the source runs out with `canLoop == false` and Update() removes the
 *     instance.
 *
 * After either event, the ref is stale. Calling Pause/Resume/Stop with a
 * stale ref is a no-op; GetState returns Stopped. This is by design --
 * sound effects can end on their own without callers having to coordinate.
 *
 * # Thread safety
 *
 * AudioPlayer is not thread-safe. All methods must be called from the same
 * thread.
 */
class AudioPlayer {
  public:
    /**
     * Enumerates the system's audio output devices.
     *
     * \returns a list of (name, deviceId) pairs. The deviceId values can be
     *          passed to SoundGroupSettings::deviceId to route a group to a
     *          specific device; otherwise use SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK
     *          for the system default.
     */
    static std::vector<AudioDevice> GetAudioOutputDevices();

    /**
     * Constructs an AudioPlayer and creates the `"default"` sound group.
     *
     * \param defaultSoundGroupSettings settings applied to the automatically
     *                                  created `"default"` group. Other
     *                                  groups must be registered with
     *                                  CreateSoundGroup.
     */
    AudioPlayer(SoundGroupSettings defaultSoundGroupSettings = SoundGroupSettings());

    ~AudioPlayer();

    /**
     * Creates or updates a named sound group.
     *
     * If `soundGroupName` is new, opens an SDL audio device for it and
     * stores the settings. If it already exists and the device id differs,
     * opens the new device, migrates every currently playing instance in
     * the group to it, and closes the old device. Always updates the
     * group's volume to `settings.volume`.
     *
     * Throws std::runtime_error if opening the SDL audio device fails.
     *
     * \param soundGroupName name to register or update.
     * \param settings device id and volume for the group.
     */
    void CreateSoundGroup(
        const std::string &soundGroupName, SoundGroupSettings settings = SoundGroupSettings());

    /**
     * Stops every playback in a group, then removes the group.
     *
     * Asserts if `soundGroupName` does not exist (no-op in release).
     */
    void DestroySoundGroup(const std::string &soundGroupName);

    /**
     * Stops every playback in a group. The group itself remains.
     *
     * Stopped instances are removed immediately; their AudioRefs become
     * stale. Asserts if the group does not exist.
     */
    void StopGroup(const std::string &soundGroupName);

    /**
     * Pauses every playing instance in a group.
     *
     * Asserts if the group does not exist. Paused instances are unbound
     * from their SDL audio device but keep their position; a subsequent
     * ResumeGroup (or Resume) continues playback.
     */
    void PauseGroup(const std::string &soundGroupName);

    /**
     * Resumes every paused instance in a group.
     *
     * Asserts if the group does not exist.
     */
    void ResumeGroup(const std::string &soundGroupName);

    /**
     * Returns the SDL logical audio device id for a group.
     *
     * Asserts if the group does not exist; returns 0 (invalid device id) in
     * release builds.
     */
    SDL_AudioDeviceID GetGroupDeviceId(const std::string &soundGroupName) const;

    /**
     * Returns the linear volume multiplier for a group.
     *
     * Asserts if the group does not exist; returns 0.0f in release builds.
     */
    float GetGroupVolume(const std::string &soundGroupName) const;

    /**
     * Returns the master volume (global multiplier applied to every group).
     */
    float GetMasterVolume() const;

    /**
     * Sets the master volume and updates the gain on every active instance.
     *
     * \param volume new linear multiplier. By convention in [0, 1];
     *               values above 1 may cause clipping.
     */
    void SetMasterVolume(float volume);

    /**
     * Starts a new playback of `sound` in the given group.
     *
     * Asserts if `soundGroupName` does not exist (returns AudioRef(0) in
     * release). The playback is immediately added to the active list; it
     * will begin pushing samples on the next Update() call.
     *
     * \param sound audio data to play. May be shared between multiple
     *              simultaneous playbacks; each gets its own cursor.
     * \param soundGroupName group to route playback through. Defaults to
     *                       `"default"`.
     * \param loop if true, the playback loops indefinitely when it hits the
     *             end of `sound`.
     * \returns a non-zero AudioRef on success, 0 on failure.
     */
    AudioRef Play(std::shared_ptr<Sound> sound, const std::string &soundGroupName = "default",
        const bool loop = false);

    /**
     * Starts a new playback of `stream` in the given group.
     *
     * The AudioPlayer internally clones `stream` so it can track its own
     * decoder cursor independently of the caller's; the caller's Stream is
     * not consumed by playback. Asserts if `soundGroupName` does not exist
     * (returns AudioRef(0) in release).
     *
     * \param stream streaming audio source. Cloned internally.
     * \param soundGroupName group to route playback through. Defaults to
     *                       `"default"`.
     * \param loop if true, the playback loops indefinitely when the stream
     *             is exhausted.
     * \returns a non-zero AudioRef on success, 0 on failure.
     */
    AudioRef Play(std::shared_ptr<Stream> stream, const std::string &soundGroupName = "default",
        const bool loop = false);

    /**
     * Pauses a single playback, preserving its position.
     *
     * No-op if the ref is stale or already paused.
     */
    void Pause(const AudioRef &audioRef);

    /**
     * Resumes a single paused playback.
     *
     * No-op if the ref is stale or already playing.
     */
    void Resume(const AudioRef &audioRef);

    /**
     * Stops and discards a single playback.
     *
     * The ref becomes stale after this call. No-op if the ref is already stale.
     */
    void Stop(const AudioRef &audioRef);

    /**
     * Reports the current state of a playback.
     *
     * \returns AudioState::Playing or AudioState::Paused for a live ref.
     *          Returns AudioState::Stopped for a stale ref (including after
     *          natural completion).
     */
    AudioState GetState(const AudioRef &audioRef) const;

    /**
     * Returns the number of times this playback has wrapped from the end
     * of its source back to the beginning.
     *
     * Increments by 1 each Update tick during which a loop wrap was
     * observed. Reaches non-zero only when Play was called with `loop ==
     * true`. Returns 0 for stale refs.
     *
     * The counter is updated at Update granularity, not at sample-accurate
     * loop boundaries. To detect a "just looped" event, sample the value
     * each frame and compare against the previous reading.
     */
    uint32_t GetLoopCount(const AudioRef &audioRef) const;

    /**
     * Tops up SDL's audio queue and retires finished playbacks. Call this function regularly.
     *
     * We currently push enough samples (if available) to ensure that there's 1/30 of a second worth
     * of samples in the queue. This should be enough to allow for the occasional slowdown without
     * taking up huge amounts of memory. This might made configurable in the future.
     */
    void Update();

  private:
    AudioPlayer(const AudioPlayer &) = delete;
    AudioPlayer(AudioPlayer &&) = delete;
    AudioPlayer &operator=(const AudioPlayer &) = delete;
    AudioPlayer &operator=(AudioPlayer &&) = delete;

    struct Impl;
    std::unique_ptr<Impl> pImpl;
};

} // namespace Lucky
