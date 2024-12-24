#include <dr_wav.h>
#include <spdlog/spdlog.h>

#include <Lucky/Audio/Sound.hpp>

namespace Lucky
{
    Sound::Sound(const std::string &filename)
    {
        unsigned int drwavChannels;
        unsigned int drwavSampleRate;
        drwav_uint64 drwavSampleCount;
        drwav_int16 *drwavSamples = drwav_open_file_and_read_pcm_frames_s16(
            filename.c_str(), &drwavChannels, &drwavSampleRate, &drwavSampleCount, NULL);

        if (!drwavSamples)
        {
            spdlog::error("Failed to open sound file {}", filename);
            throw;
        }

        totalFrames = static_cast<uint32_t>(drwavSampleCount);
        sampleRate = drwavSampleRate;
        channels = drwavChannels;
        frames.insert(frames.end(), drwavSamples, drwavSamples + totalFrames * channels);

        drwav_free(drwavSamples, nullptr);
    }

    Sound::Sound(void *buffer, uint32_t bufferByteSize)
    {
        unsigned int drwavChannels;
        unsigned int drwavSampleRate;
        drwav_uint64 drwavSampleCount;
        drwav_int16 *drwavSamples = drwav_open_memory_and_read_pcm_frames_s16(
            buffer, bufferByteSize, &drwavChannels, &drwavSampleRate, &drwavSampleCount, NULL);

        if (!drwavSamples)
        {
            spdlog::error("Failed to open sound from buffer");
            throw;
        }

        totalFrames = static_cast<uint32_t>(drwavSampleCount);
        sampleRate = drwavSampleRate;
        channels = drwavChannels;
        frames.insert(frames.end(), drwavSamples, drwavSamples + totalFrames);

        drwav_free(drwavSamples, nullptr);
    }

    uint32_t Sound::GetFrames(uint32_t &position, int16_t *buffer, uint32_t frameCount, bool *loop)
    {
        uint32_t framesAvailable = totalFrames - position;
        uint32_t framesToGet = std::min(frameCount, framesAvailable);

        if (framesToGet > 0)
        {
            memcpy(buffer, &frames[position * channels], framesToGet * channels * sizeof(int16_t));
            position += framesToGet;
            if (position == totalFrames && loop)
            {
                *loop = true;
                position = 0;
                return framesToGet +
                       GetFrames(position, buffer + framesToGet * channels, frameCount - framesToGet, loop);
            }
        }

        return framesToGet;
    }
} // namespace Lucky