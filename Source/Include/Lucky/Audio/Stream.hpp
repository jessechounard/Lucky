#pragma once

#include <string>

namespace Lucky
{
    class Stream
    {
      public:
        Stream(const std::string &fileName);
        Stream(void *buffer, uint32_t bufferByteSize);
        Stream(const Stream &stream);
        ~Stream();

        uint32_t GetFrames(int16_t *buffer, uint32_t frames, bool *loop);

        uint32_t sampleRate;
        uint16_t channels;

      private:
        enum class StreamSource
        {
            File,
            Memory,
        };

        struct Impl;
        std::unique_ptr<Impl> pImpl;
    };
} // namespace Lucky
