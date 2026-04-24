#pragma once

#include <stdint.h>
#include <string>
#include <vector>

namespace Lucky {

/**
 * A short piece of audio loaded fully into memory.
 *
 * Decodes a WAV (via dr_wav) into a flat buffer of interleaved 16-bit PCM
 * samples on construction. Ideal for sound effects, UI blips, or anything
 * short enough that the memory cost of holding every sample is acceptable.
 * For music or long audio, use Stream instead.
 *
 * # Shared playback
 *
 * Sound owns only the decoded audio data -- it does not track a playback
 * position. GetFrames() takes the caller's position as a reference and
 * advances it, which lets multiple concurrent playbacks share the same
 * Sound instance. AudioPlayer uses this: a single `std::shared_ptr<Sound>`
 * can back any number of simultaneous playbacks, each maintaining its own
 * cursor.
 *
 * # Thread safety
 *
 * Sound is safe to read from multiple threads as long as each thread owns
 * its own `position` cursor. The shared `frames` buffer is never mutated
 * after construction.
 */
struct Sound {
    uint32_t totalFrames;        /**< total number of sample frames in the file. */
    uint32_t sampleRate;         /**< sample rate in Hz (e.g. 44100). */
    uint16_t channels;           /**< channel count (1 = mono, 2 = stereo, etc.). */
    std::vector<int16_t> frames; /**< interleaved 16-bit PCM samples, channels per frame. */

    /**
     * Loads a WAV file from disk.
     *
     * Throws std::runtime_error if the file cannot be opened or decoded.
     *
     * \param filename path to a RIFF WAV file.
     */
    Sound(const std::string &filename);

    /**
     * Loads a WAV from an in-memory buffer.
     *
     * The buffer is read-only and is not referenced after the constructor
     * returns -- the caller may free it immediately. Throws
     * std::runtime_error if the data cannot be decoded as WAV.
     *
     * \param buffer pointer to WAV-formatted bytes.
     * \param bufferByteSize size of the buffer in bytes.
     */
    Sound(const void *buffer, uint32_t bufferByteSize);

    ~Sound() = default;

    /**
     * Copies frames into the caller's buffer and advances their cursor.
     *
     * Copies up to `frameCount` frames starting at `position` into `buffer`,
     * then advances `position` by the number of frames actually copied. If
     * the cursor reaches the end of the sound and `canLoop` is true, the
     * cursor wraps to 0 and continues filling the caller's buffer from the
     * start. If `canLoop` is false, copying stops at the end.
     *
     * \param position in/out cursor, in frames from the start of the
     *                 sound. Must be in [0, totalFrames] on entry.
     * \param buffer destination buffer, must hold at least
     *               `frameCount * channels` int16_t samples.
     * \param frameCount number of frames the caller wants.
     * \param canLoop if true, GetFrames may wrap to the start when it hits
     *                the end to fill the entire request.
     * \param didLoop optional output: set to true if a wrap happened during
     *                this call, false otherwise. Pass nullptr to ignore.
     * \returns the number of frames actually written. Equals `frameCount`
     *          unless the cursor hit the end with `canLoop` false.
     */
    uint32_t GetFrames(uint32_t &position, int16_t *buffer, uint32_t frameCount, bool canLoop,
        bool *didLoop = nullptr);
};

} // namespace Lucky
