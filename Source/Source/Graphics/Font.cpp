#include <stdio.h>

#include <spdlog/spdlog.h>
#include <stb_rect_pack.h>
#include <stb_truetype.h>

#include <Lucky/Graphics/Font.hpp>

namespace Lucky
{
    struct FontEntry
    {
        float size;
        float scaleFactor;
        std::vector<stbtt_packedchar> packedData;
        std::vector<int> codePoints;
        std::shared_ptr<Lucky::Texture> texture;
        std::map<std::pair<int, int>, int> kerning;
    };

    struct Font::Impl
    {
        stbtt_fontinfo fontInfo;
        std::map<std::string, FontEntry> fontEntries;
    };

    Font::Font(const std::string &fileName)
        : pImpl(std::make_unique<Impl>())
    {
        FILE *fontFile = fopen(fileName.c_str(), "rb");

        if (!fontFile)
        {
            spdlog::error("Couldn't open font file. {}", fileName);
            throw;
        }

        fseek(fontFile, 0, SEEK_END);
        int fileLength = ftell(fontFile);
        fseek(fontFile, 0, SEEK_SET);

        fontMemoryBuffer = malloc(fileLength);
        if (fontMemoryBuffer == nullptr)
        {
            spdlog::error("Couldn't allocate memory for font");
        }

        fread(fontMemoryBuffer, 1, fileLength, fontFile);
        fclose(fontFile);

        shouldFreeMemory = true;

        Initialize();
    }

    Font::Font(void *memory)
        : pImpl(std::make_unique<Impl>())
    {
        pImpl->fontEntries.clear();

        fontMemoryBuffer = memory;
        shouldFreeMemory = false;

        Initialize();
    }

    Font::~Font()
    {
        if (shouldFreeMemory)
        {
            free(fontMemoryBuffer);
        }
    }

    std::shared_ptr<Texture> Font::CreateFontEntry(const std::string &name, const float fontSize, int *codePoints,
        int pointCount, uint32_t oversampling, bool kerningEnabled)
    {
        FontEntry entry;
        entry.codePoints.insert(entry.codePoints.end(), codePoints, codePoints + pointCount);
        entry.packedData.resize(pointCount);

        stbtt_pack_range range;
        range.font_size = fontSize;
        range.array_of_unicode_codepoints = codePoints;
        range.num_chars = pointCount;
        range.chardata_for_range = &entry.packedData[0];

        stbtt_pack_context packContext;

        bool done = false;
        bool horizVert = true;
        int sizeIncrement = 512;
        int bitmapWidth = 512;
        int bitmapHeight = 512;

        std::vector<uint8_t> bitmapData(bitmapWidth * bitmapHeight, 0);

        do
        {
            stbtt_PackBegin(&packContext, &bitmapData[0], bitmapWidth, bitmapHeight, 0, 1, nullptr);
            stbtt_PackSetOversampling(&packContext, oversampling, oversampling);
            if (!stbtt_PackFontRanges(&packContext, (const uint8_t *)fontMemoryBuffer, 0, &range, 1))
            {
                if (horizVert)
                {
                    bitmapWidth += sizeIncrement;
                }
                else
                {
                    bitmapHeight += sizeIncrement;
                }
                horizVert = !horizVert;
                bitmapData.resize(bitmapWidth * bitmapHeight);
                stbtt_PackEnd(&packContext);
            }
            else
            {
                stbtt_PackEnd(&packContext);
                done = true;
            }
        } while (!done);

        std::vector<uint8_t> colorBitmapData(bitmapData.size() * 4, 0);
        uint8_t *s = &bitmapData[0];
        uint8_t *d = &colorBitmapData[0];

        for (size_t i = 0; i < bitmapData.size(); i++)
        {
            *d++ = *s;
            *d++ = *s;
            *d++ = *s;
            *d++ = *s;
            s++;
        }

        entry.texture = std::make_shared<Lucky::Texture>(Lucky::TextureType::Default, bitmapWidth, bitmapHeight,
            &colorBitmapData[0], (uint32_t)colorBitmapData.size(), Lucky::TextureFilter::Linear);

        entry.scaleFactor = stbtt_ScaleForPixelHeight(&pImpl->fontInfo, fontSize);

        if (kerningEnabled)
        {
            for (int first = 0; first < pointCount; first++)
            {
                for (int second = 0; second < pointCount; second++)
                {
                    int kern = stbtt_GetCodepointKernAdvance(&pImpl->fontInfo, codePoints[first], codePoints[second]);
                    if (kern != 0)
                    {
                        entry.kerning[std::pair<int, int>(codePoints[first], codePoints[second])] = kern;
                    }
                }
            }
        }

        pImpl->fontEntries[name] = entry;

        return entry.texture;
    }

    std::shared_ptr<Texture> Font::GetTexture(const std::string &entryName)
    {
        auto &entry = pImpl->fontEntries[entryName];
        return entry.texture;
    }

    void Font::DrawString(BatchRenderer &batchRenderer, const std::string &text, const std::string &entryName,
        const float x, const float y, Color color)
    {
        auto &entry = pImpl->fontEntries[entryName];

        float xpos = x;
        int ch = 0;

        while (text[ch])
        {
            int character = text[ch];

            // find the character index
            auto found = std::find_if(entry.codePoints.begin(), entry.codePoints.end(),
                [character](const int cp) { return cp == character; });

            if (found == entry.codePoints.end())
            {
                ++ch;
                continue;
            }

            const size_t index = found - entry.codePoints.begin();
            auto cd = entry.packedData[index];
            float iw = 1.0f / entry.texture->GetWidth();
            float ih = 1.0f / entry.texture->GetHeight();

            if (text[ch] != ' ')
            {
                batchRenderer.BatchQuadUV(glm::vec2(cd.x0 * iw, cd.y0 * ih), glm::vec2(cd.x1 * iw, cd.y1 * ih),
                    glm::vec2(xpos + cd.xoff, y + cd.yoff), glm::vec2(xpos + cd.xoff2, y + cd.yoff2), color);
            }

            xpos += cd.xadvance;
            if (text[ch + 1])
            {
                auto kernFound = entry.kerning.find(std::pair<int, int>(text[ch], text[ch + 1]));
                if (kernFound != entry.kerning.end())
                {
                    xpos += entry.scaleFactor * kernFound->second;
                }
            }
            ++ch;
        }
    }

    void Font::Initialize()
    {
        if (!stbtt_InitFont(&pImpl->fontInfo, (const uint8_t *)fontMemoryBuffer, 0))
        {
            spdlog::error("Couldn't initialize font.");
            throw;
        }
    }
} // namespace Lucky
