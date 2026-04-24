#pragma once

#include <memory>
#include <string>

namespace Lucky {

/**
 * A long piece of audio decoded on the fly from Ogg Vorbis or MP3.
 *
 * Holds a decoder (stb_vorbis or dr_mp3) and pulls samples from it on
 * demand. Use for background music, dialogue, or anything long enough that
 * decoding the whole file into memory would be wasteful. For short, reusable
 * sound effects, use Sound instead.
 *
 * # Format detection
 *
 * On construction, Stream first tries to open the source as Ogg Vorbis. If
 * that fails, it falls back to MP3. Both constructors throw
 * std::runtime_error if neither decoder accepts the source.
 *
 * # Playback state
 *
 * Unlike Sound, a Stream owns its own playback position (inside its
 * decoder). Two simultaneous playbacks of the same track therefore require
 * two Stream objects. Use Clone() to produce a second, independent cursor
 * over the same source.
 *
 * # Move-only
 *
 * Stream is move-only. The copy constructor is deleted; use Clone()
 * explicitly when you want another cursor.
 *
 * # Thread safety
 *
 * Stream is not thread-safe. A single instance must be used from one thread
 * at a time.
 */
class Stream {
  public:
    /**
     * Opens a streaming source from a file.
     *
     * Tries Ogg Vorbis first, then MP3. Throws std::runtime_error if neither
     * decoder can open the file.
     *
     * \param fileName path to a Vorbis (.ogg) or MP3 file.
     */
    Stream(const std::string &fileName);

    /**
     * Opens a streaming source from an in-memory buffer.
     *
     * The Stream does NOT copy the buffer -- it holds the pointer and reads
     * from it during playback. The caller must keep `buffer` alive and
     * unchanged for the lifetime of this Stream and any Clones. Throws
     * std::runtime_error if neither decoder can open the data.
     *
     * \param buffer pointer to Vorbis or MP3 bytes; must outlive the Stream.
     * \param bufferByteSize size of the buffer in bytes.
     */
    Stream(const void *buffer, uint32_t bufferByteSize);

    ~Stream();

    Stream(const Stream &) = delete;
    Stream &operator=(const Stream &) = delete;
    Stream(Stream &&) noexcept;
    Stream &operator=(Stream &&) noexcept;

    /**
     * Opens a fresh, independent decoder on the same source.
     *
     * The returned Stream starts at position 0 and is unaffected by the
     * original's playback position. For file sources, this re-opens the
     * file from disk. For memory sources, it shares the original's buffer
     * pointer -- the caller must continue to keep the buffer alive for the
     * clone's lifetime too.
     *
     * \returns a new Stream positioned at the beginning of the same source.
     */
    Stream Clone() const;

    /**
     * Decodes the next batch of frames into the caller's buffer.
     *
     * Pulls up to `frames` frames from the decoder. If the decoder runs dry
     * before `frames` frames are produced and `canLoop` is true, the
     * decoder is seeked back to the start and decoding resumes to fill the
     * rest of the request. If `canLoop` is false, decoding stops at the
     * end.
     *
     * \param buffer destination buffer, must hold at least
     *               `frames * channels` int16_t samples.
     * \param frames number of frames the caller wants.
     * \param canLoop if true, GetFrames may seek back to the start when the
     *                decoder finishes to fill the entire request.
     * \param didLoop optional output: set to true if a seek-to-start
     *                happened during this call, false otherwise. Pass
     *                nullptr to ignore.
     * \returns the number of frames actually written. Equals `frames`
     *          unless the decoder reached the end with `canLoop` false.
     */
    uint32_t GetFrames(int16_t *buffer, uint32_t frames, bool canLoop, bool *didLoop = nullptr);

    uint32_t sampleRate; /**< sample rate in Hz, read from the decoder metadata. */
    uint16_t channels;   /**< channel count, read from the decoder metadata. */

  private:
    enum class StreamSource {
        File,
        Memory,
    };

    struct Impl;
    std::unique_ptr<Impl> pImpl;
};

} // namespace Lucky
