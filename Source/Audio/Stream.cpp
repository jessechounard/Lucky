#include <stdexcept>

#include <dr_mp3.h>
#include <spdlog/spdlog.h>
#define STB_VORBIS_HEADER_ONLY
#include <stb_vorbis.c>

#include <Lucky/Stream.hpp>

namespace Lucky {

namespace {

struct StbVorbisDeleter {
    void operator()(stb_vorbis *p) const noexcept {
        if (p) {
            stb_vorbis_close(p);
        }
    }
};

struct DrMp3Deleter {
    void operator()(drmp3 *p) const noexcept {
        if (p) {
            drmp3_uninit(p);
            delete p;
        }
    }
};

} // namespace

struct Stream::Impl {
    StreamSource streamSource;
    std::string fileName;
    const void *buffer;
    uint32_t bufferByteSize;
    std::unique_ptr<stb_vorbis, StbVorbisDeleter> vorbis;
    std::unique_ptr<drmp3, DrMp3Deleter> mp3;

    Impl(const std::string &fileName)
        : streamSource(StreamSource::File), fileName(fileName), buffer(nullptr), bufferByteSize(0) {
    }

    Impl(const void *buffer, uint32_t bufferByteSize)
        : streamSource(StreamSource::Memory), buffer(buffer), bufferByteSize(bufferByteSize) {
    }
};

Stream::Stream(const std::string &fileName)
    : sampleRate(0), channels(0), pImpl(std::make_unique<Impl>(fileName)) {
    pImpl->vorbis.reset(stb_vorbis_open_filename(fileName.c_str(), nullptr, nullptr));
    if (pImpl->vorbis) {
        stb_vorbis_info info = stb_vorbis_get_info(pImpl->vorbis.get());
        sampleRate = info.sample_rate;
        channels = info.channels;
    } else {
        auto candidate = std::make_unique<drmp3>();
        if (drmp3_init_file(candidate.get(), fileName.c_str(), nullptr)) {
            pImpl->mp3.reset(candidate.release());
            sampleRate = pImpl->mp3->sampleRate;
            channels = pImpl->mp3->channels;
        } else {
            spdlog::error("Couldn't open audio stream from file {}", fileName);
            throw std::runtime_error("Couldn't open audio stream from file " + fileName);
        }
    }
}

Stream::Stream(const void *buffer, uint32_t bufferByteSize)
    : sampleRate(0), channels(0), pImpl(std::make_unique<Impl>(buffer, bufferByteSize)) {
    pImpl->vorbis.reset(
        stb_vorbis_open_memory((const unsigned char *)buffer, bufferByteSize, nullptr, nullptr));
    if (pImpl->vorbis) {
        stb_vorbis_info info = stb_vorbis_get_info(pImpl->vorbis.get());
        sampleRate = info.sample_rate;
        channels = info.channels;
    } else {
        auto candidate = std::make_unique<drmp3>();
        if (drmp3_init_memory(candidate.get(), buffer, bufferByteSize, nullptr)) {
            pImpl->mp3.reset(candidate.release());
            sampleRate = pImpl->mp3->sampleRate;
            channels = pImpl->mp3->channels;
        } else {
            spdlog::error("Couldn't open audio stream from memory");
            throw std::runtime_error("Couldn't open audio stream from memory");
        }
    }
}

Stream::Stream(Stream &&) noexcept = default;
Stream &Stream::operator=(Stream &&) noexcept = default;
Stream::~Stream() = default;

Stream Stream::Clone() const {
    if (pImpl->streamSource == StreamSource::File) {
        return Stream(pImpl->fileName);
    } else {
        return Stream(pImpl->buffer, pImpl->bufferByteSize);
    }
}

uint32_t Stream::GetFrames(int16_t *buffer, uint32_t frames, bool canLoop, bool *didLoop) {
    if (didLoop) {
        *didLoop = false;
    }

    if (pImpl->vorbis) {
        uint32_t samples = static_cast<uint32_t>(stb_vorbis_get_samples_short_interleaved(
            pImpl->vorbis.get(), channels, buffer, frames * channels));
        if (canLoop && samples < frames) {
            if (didLoop) {
                *didLoop = true;
            }
            stb_vorbis_seek_start(pImpl->vorbis.get());
            return samples + stb_vorbis_get_samples_short_interleaved(pImpl->vorbis.get(),
                                 channels,
                                 buffer + samples * channels,
                                 (frames - samples) * channels);
        } else {
            return samples;
        }
    } else {
        uint32_t samples =
            static_cast<uint32_t>(drmp3_read_pcm_frames_s16(pImpl->mp3.get(), frames, buffer));
        if (canLoop && samples < frames) {
            if (didLoop) {
                *didLoop = true;
            }
            drmp3_seek_to_pcm_frame(pImpl->mp3.get(), 0);

            return samples + static_cast<uint32_t>(drmp3_read_pcm_frames_s16(
                                 pImpl->mp3.get(), frames - samples, buffer + samples * channels));
        } else {
            return samples;
        }
    }

    return 0;
}

} // namespace Lucky
