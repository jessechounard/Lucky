#pragma once

#include <stdint.h>
#include <string>
#include <vector>

namespace Lucky
{
    struct Sound
    {
        uint32_t totalFrames;
        uint32_t sampleRate;
        uint16_t channels;
        std::vector<int16_t> frames;

        Sound(const std::string &filename);
        Sound(void *buffer, uint32_t bufferByteSize);
        ~Sound() = default;

        uint32_t GetFrames(uint32_t &position, int16_t *buffer, uint32_t frames, bool *loop);
    };
} // namespace Lucky
