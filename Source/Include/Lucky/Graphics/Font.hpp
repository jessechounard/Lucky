#pragma once

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <Lucky/Graphics/BatchRenderer.hpp>
#include <Lucky/Graphics/Color.hpp>
#include <Lucky/Graphics/Texture.hpp>

namespace Lucky
{
    class Font
    {
      public:
        Font(const std::string &fileName);
        Font(void *memory);
        ~Font();

        // todo: future optimization - allow packing multiple sizes into one texture
        // todo: do we need to specify a maximum texture size?
        // todo: have a default character set? specifying code point array is annoying
        std::shared_ptr<Texture> CreateFontEntry(const std::string &entryName, const float fontSize, int *codePoints,
            int pointCount, uint32_t oversampling = 1, bool kerningEnabled = true);
        std::shared_ptr<Texture> GetTexture(const std::string &entryName);

        void DrawString(BatchRenderer &batchRenderer, const std::string &text, const std::string &entryName,
            const float x, const float y, Color color);

      private:
        void Initialize();

        void *fontMemoryBuffer;
        bool shouldFreeMemory;

        struct Impl;
        std::unique_ptr<Impl> pImpl;
    };
} // namespace Lucky