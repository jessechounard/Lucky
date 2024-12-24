#include <dr_mp3.h>
#include <spdlog/spdlog.h>
#include <stb_vorbis.h>

#include <Lucky/Audio/Stream.hpp>

namespace Lucky
{
    struct Stream::Impl
    {
        StreamSource streamSource;
        std::string fileName;
        void *buffer;
        uint32_t bufferByteSize;
        stb_vorbis *vorbis;
        drmp3 *mp3;

        Impl(const std::string &fileName)
            : streamSource(StreamSource::File),
              fileName(fileName),
              buffer(nullptr),
              bufferByteSize(0),
              vorbis(nullptr),
              mp3(nullptr)
        {
        }

        Impl(void *buffer, uint32_t bufferByteSize)
            : streamSource(StreamSource::Memory),
              buffer(buffer),
              bufferByteSize(bufferByteSize),
              vorbis(nullptr),
              mp3(nullptr)
        {
        }

        Impl(const Stream &stream)
            : streamSource(stream.pImpl->streamSource),
              fileName(stream.pImpl->fileName),
              buffer(stream.pImpl->buffer),
              bufferByteSize(stream.pImpl->bufferByteSize),
              vorbis(nullptr),
              mp3(nullptr)
        {
        }
    };

    Stream::Stream(const std::string &fileName)
        : sampleRate(0),
          channels(0),
          pImpl(std::make_unique<Impl>(fileName))
    {
        pImpl->vorbis = stb_vorbis_open_filename(fileName.c_str(), nullptr, nullptr);
        if (pImpl->vorbis)
        {
            stb_vorbis_info info = stb_vorbis_get_info(pImpl->vorbis);
            sampleRate = info.sample_rate;
            channels = info.channels;
        }
        else
        {
            pImpl->mp3 = new drmp3();
            if (drmp3_init_file(pImpl->mp3, fileName.c_str(), nullptr))
            {
                sampleRate = pImpl->mp3->sampleRate;
                channels = pImpl->mp3->channels;
            }
            else
            {
                delete pImpl->mp3;
                spdlog::error("Couldn't open audio stream from file {}", fileName);
                throw;
            }
        }
    }

    Stream::Stream(void *buffer, uint32_t bufferByteSize)
        : sampleRate(0),
          channels(0),
          pImpl(std::make_unique<Impl>(buffer, bufferByteSize))

    {
        pImpl->vorbis = stb_vorbis_open_memory((const unsigned char *)buffer, bufferByteSize, nullptr, nullptr);
        if (pImpl->vorbis)
        {
            stb_vorbis_info info = stb_vorbis_get_info(pImpl->vorbis);
            sampleRate = info.sample_rate;
            channels = info.channels;
        }
        else
        {
            pImpl->mp3 = new drmp3();
            if (drmp3_init_memory(pImpl->mp3, buffer, bufferByteSize, nullptr))
            {
                sampleRate = pImpl->mp3->sampleRate;
                channels = pImpl->mp3->channels;
            }
            else
            {
                delete pImpl->mp3;
                spdlog::error("Couldn't open audio stream from memory");
                throw;
            }
        }
    }

    Stream::Stream(const Stream &stream)
        : sampleRate(stream.sampleRate),
          channels(stream.channels),
          pImpl(std::make_unique<Impl>(stream))
    {
        if (stream.pImpl->vorbis)
        {
            if (pImpl->streamSource == StreamSource::File)
            {
                pImpl->vorbis = stb_vorbis_open_filename(pImpl->fileName.c_str(), nullptr, nullptr);
                if (!pImpl->vorbis)
                {
                    spdlog::error("Couldn't open audio stream from file {}", pImpl->fileName);
                    throw;
                }
            }
            else
            {
                pImpl->vorbis = stb_vorbis_open_memory(
                    (const unsigned char *)pImpl->buffer, pImpl->bufferByteSize, nullptr, nullptr);
                if (!pImpl->vorbis)
                {
                    spdlog::error("Couldn't open audio stream from memory");
                    throw;
                }
            }
            stb_vorbis_info info = stb_vorbis_get_info(pImpl->vorbis);
            if (channels != info.channels || sampleRate != info.sample_rate)
            {
                spdlog::error("Unexpected settings on cloned vorbis stream");
                throw;
            }
        }
        else
        {
            pImpl->mp3 = new drmp3();
            if (pImpl->streamSource == StreamSource::File)
            {
                if (!drmp3_init_file(pImpl->mp3, pImpl->fileName.c_str(), nullptr))
                {
                    delete pImpl->mp3;
                    spdlog::error("Couldn't open audio stream from file {}", pImpl->fileName);
                    throw;
                }
            }
            else
            {
                if (!drmp3_init_memory(pImpl->mp3, pImpl->buffer, pImpl->bufferByteSize, nullptr))
                {
                    delete pImpl->mp3;
                    spdlog::error("Couldn't open audio stream from memory");
                    throw;
                }
            }
            if (channels != pImpl->mp3->channels || sampleRate != pImpl->mp3->sampleRate)
            {
                delete pImpl->mp3;
                spdlog::error("Unexpected settings on cloned mp3 stream");
                throw;
            }
        }
    }

    Stream::~Stream()
    {
        if (pImpl->vorbis)
        {
            stb_vorbis_close(pImpl->vorbis);
            pImpl->vorbis = nullptr;
        }
        if (pImpl->mp3)
        {
            drmp3_uninit(pImpl->mp3);
            delete pImpl->mp3;
            pImpl->mp3 = nullptr;
        }
    }

    uint32_t Stream::GetFrames(int16_t *buffer, uint32_t frames, bool *loop)
    {
        if (loop)
        {
            *loop = false;
        }

        if (pImpl->vorbis)
        {
            uint32_t samples = static_cast<uint32_t>(
                stb_vorbis_get_samples_short_interleaved(pImpl->vorbis, channels, buffer, frames * channels));
            if (loop && samples < frames)
            {
                *loop = true;
                stb_vorbis_seek_start(pImpl->vorbis);
                return samples + stb_vorbis_get_samples_short_interleaved(pImpl->vorbis, channels,
                                     buffer + samples * channels, (frames - samples) * channels);
            }
            else
            {
                return samples;
            }
        }
        else
        {
            uint32_t samples = static_cast<uint32_t>(drmp3_read_pcm_frames_s16(pImpl->mp3, frames, buffer));
            if (loop && samples < frames)
            {
                *loop = true;
                drmp3_seek_to_pcm_frame(pImpl->mp3, 0);

                return samples + static_cast<uint32_t>(
                                     drmp3_read_pcm_frames_s16(pImpl->mp3, channels, buffer + samples * channels));
            }
            else
            {
                return samples;
            }
        }

        return 0;
    }
} // namespace Lucky
