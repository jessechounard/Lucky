#include <stdexcept>
#include <stdio.h>

#include <spdlog/spdlog.h>
#include <stb_rect_pack.h>
#include <stb_truetype.h>

#include <Lucky/FileSystem.hpp>
#include <Lucky/Font.hpp>

namespace Lucky {

struct FontEntry {
    float size;
    float scaleFactor;
    float scaledAscent, scaledDescent, scaledLineGap;
    std::vector<stbtt_packedchar> packedData;
    std::vector<int> codePoints;
    std::shared_ptr<Lucky::Texture> texture;
    std::map<std::pair<int, int>, int> kerning;
};

struct Font::Impl {
    stbtt_fontinfo fontInfo;
    std::map<std::string, FontEntry> fontEntries;
};

std::vector<int> CombineRanges(std::vector<std::pair<int, int>> ranges) {
    int capacity{0};

    for (const auto &r : ranges) {
        capacity += r.second - r.first + 1;
    }

    std::vector<int> result;
    result.reserve(capacity);

    for (const auto &r : ranges) {
        for (int i = r.first; i <= r.second; i++) {
            result.push_back(i);
        }
    }

    return result;
}

std::vector<int> Font::GetCodePointsLatin() {
    return CombineRanges({{0x0020, 0x00FF}});
}

Font::Font(GraphicsDevice &graphicsDevice, const std::string &fileName)
    : graphicsDevice(graphicsDevice), pImpl(std::make_unique<Impl>()) {
    FILE *fontFile = fopen(fileName.c_str(), "rb");

    if (!fontFile) {
        spdlog::error("Couldn't open font file. {}", fileName);
        throw std::runtime_error("Couldn't open font file: " + fileName);
    }

    fseek(fontFile, 0, SEEK_END);
    int fileLength = ftell(fontFile);
    fseek(fontFile, 0, SEEK_SET);

    fontMemoryBuffer = malloc(fileLength);
    if (fontMemoryBuffer == nullptr) {
        fclose(fontFile);
        spdlog::error("Couldn't allocate memory for font");
        throw std::runtime_error("Couldn't allocate memory for font");
    }

    fread(fontMemoryBuffer, 1, fileLength, fontFile);
    fclose(fontFile);

    shouldFreeMemory = true;

    Initialize();
}

Font::Font(GraphicsDevice &graphicsDevice, void *memory)
    : graphicsDevice(graphicsDevice), pImpl(std::make_unique<Impl>()) {
    pImpl->fontEntries.clear();

    fontMemoryBuffer = memory;
    shouldFreeMemory = false;

    Initialize();
}

Font::~Font() {
    if (shouldFreeMemory) {
        free(fontMemoryBuffer);
    }
}

std::shared_ptr<Texture> Font::CreateFontEntry(const std::string &name, const float fontSize,
    std::vector<int> codePoints, uint32_t oversampling, bool kerningEnabled) {
    FontEntry entry;
    int pointCount = static_cast<int>(codePoints.size());
    entry.codePoints.insert(entry.codePoints.end(), codePoints.begin(), codePoints.end());
    entry.packedData.resize(pointCount);

    stbtt_pack_range range;
    range.font_size = fontSize;
    range.array_of_unicode_codepoints = &codePoints[0];
    range.num_chars = pointCount;
    range.chardata_for_range = &entry.packedData[0];

    stbtt_pack_context packContext;

    bool done = false;
    bool horizVert = true;
    int sizeIncrement = 512;
    int bitmapWidth = 512;
    int bitmapHeight = 512;

    std::vector<uint8_t> bitmapData(bitmapWidth * bitmapHeight, 0);

    do {
        stbtt_PackBegin(&packContext, &bitmapData[0], bitmapWidth, bitmapHeight, 0, 1, nullptr);
        stbtt_PackSetOversampling(&packContext, oversampling, oversampling);
        if (!stbtt_PackFontRanges(&packContext, (const uint8_t *)fontMemoryBuffer, 0, &range, 1)) {
            if (horizVert) {
                bitmapWidth += sizeIncrement;
            } else {
                bitmapHeight += sizeIncrement;
            }
            horizVert = !horizVert;
            bitmapData.resize(bitmapWidth * bitmapHeight);
            stbtt_PackEnd(&packContext);
        } else {
            stbtt_PackEnd(&packContext);
            done = true;
        }
    } while (!done);

    std::vector<uint8_t> colorBitmapData(bitmapData.size() * 4, 0);
    uint8_t *s = &bitmapData[0];
    uint8_t *d = &colorBitmapData[0];

    for (size_t i = 0; i < bitmapData.size(); i++) {
        *d++ = *s;
        *d++ = *s;
        *d++ = *s;
        *d++ = *s;
        s++;
    }

    entry.texture = std::make_shared<Lucky::Texture>(graphicsDevice,
        Lucky::TextureType::Default,
        bitmapWidth,
        bitmapHeight,
        &colorBitmapData[0],
        (uint32_t)colorBitmapData.size(),
        Lucky::TextureFilter::Linear);

    entry.scaleFactor = stbtt_ScaleForPixelHeight(&pImpl->fontInfo, fontSize);
    entry.size = fontSize;

    if (kerningEnabled) {
        for (int first = 0; first < pointCount; first++) {
            for (int second = 0; second < pointCount; second++) {
                int kern = stbtt_GetCodepointKernAdvance(
                    &pImpl->fontInfo, codePoints[first], codePoints[second]);
                if (kern != 0) {
                    entry.kerning[std::pair<int, int>(codePoints[first], codePoints[second])] =
                        kern;
                }
            }
        }
    }

    int ascent, descent, lineGap;
    stbtt_GetFontVMetrics(&pImpl->fontInfo, &ascent, &descent, &lineGap);
    entry.scaledAscent = ascent * entry.scaleFactor;
    entry.scaledDescent = descent * entry.scaleFactor;
    entry.scaledLineGap = lineGap * entry.scaleFactor;

    pImpl->fontEntries[name] = entry;

    return entry.texture;
}

std::shared_ptr<Texture> Font::GetTexture(const std::string &entryName) {
    // todo: verify entryName exists
    auto &entry = pImpl->fontEntries[entryName];
    return entry.texture;
}

glm::vec2 Font::MeasureString(const std::string &text, const std::string &entryName) {
    // split on '\n'
    // for each string in the split get the max width
    // height = number of strings * GetLineAdvance

    // todo: Handle '\n'
    // todo: verify entryName exists
    auto &entry = pImpl->fontEntries[entryName];

    glm::vec2 result{0, 0};
    float lineAdvance = GetLineAdvance(entryName);

    auto strings = Split(text, "\n");

    for (const auto &s : strings) {
        result.y += lineAdvance;
        int ch = 0;
        float width = 0;

        while (s[ch]) {
            int character = s[ch];

            // find the character index
            auto found = std::find_if(entry.codePoints.begin(),
                entry.codePoints.end(),
                [character](const int cp) { return cp == character; });

            if (found == entry.codePoints.end()) {
                ++ch;
                continue;
            }

            const size_t index = found - entry.codePoints.begin();
            auto cd = entry.packedData[index];

            width += cd.xadvance;

            if (s[ch + 1]) {
                auto kernFound = entry.kerning.find(std::pair<int, int>(s[ch], s[ch + 1]));
                if (kernFound != entry.kerning.end()) {
                    width += entry.scaleFactor * kernFound->second;
                }
            }
            ++ch;
        }
        result.x = std::max(result.x, width);
    }

    return result;
}

/*
glm::vec2 Font::MeasureString(const std::string &text, const std::string &entryName) {
    // todo: Handle '\n'
    // todo: verify entryName exists
    auto &entry = pImpl->fontEntries[entryName];

    glm::vec2 result{0, entry.size};

    int ch = 0;
    while (text[ch]) {
        int character = text[ch];

        // find the character index
        auto found = std::find_if(entry.codePoints.begin(),
            entry.codePoints.end(),
            [character](const int cp) { return cp == character; });

        if (found == entry.codePoints.end()) {
            ++ch;
            continue;
        }

        const size_t index = found - entry.codePoints.begin();
        auto cd = entry.packedData[index];

        result.x += cd.xadvance;

        if (text[ch + 1]) {
            auto kernFound = entry.kerning.find(std::pair<int, int>(text[ch], text[ch + 1]));
            if (kernFound != entry.kerning.end()) {
                result.x += entry.scaleFactor * kernFound->second;
            }
        }
        ++ch;
    }

    return result;
}
*/

float Font::GetLineAdvance(const std::string &entryName) const {
    // todo: verify entryName exists
    auto &entry = pImpl->fontEntries[entryName];
    return entry.scaledAscent - entry.scaledDescent + entry.scaledLineGap;
}

float Font::GetAscent(const std::string &entryName) const {
    auto &entry = pImpl->fontEntries[entryName];
    return entry.scaledAscent;
}

float Font::GetDescent(const std::string &entryName) const {
    auto &entry = pImpl->fontEntries[entryName];
    return entry.scaledDescent;
}

float Font::GetLineGap(const std::string &entryName) const {
    auto &entry = pImpl->fontEntries[entryName];
    return entry.scaledLineGap;
}

void Font::DrawString(BatchRenderer &batchRenderer, const std::string &text,
    // todo: verify entryName exists
    const std::string &entryName, glm::vec2 position, Color color) {
    auto &entry = pImpl->fontEntries[entryName];

    glm::vec2 pos = position;
    int ch = 0;
    float lineAdvance = GetLineAdvance(entryName);

    while (text[ch]) {
        int character = text[ch];

        if (character == '\n') {
            ++ch;
            pos.x = position.x;
            pos.y += lineAdvance;
            continue;
        }

        // find the character index
        auto found = std::find_if(entry.codePoints.begin(),
            entry.codePoints.end(),
            [character](const int cp) { return cp == character; });

        if (found == entry.codePoints.end()) {
            ++ch;
            continue;
        }

        const size_t index = found - entry.codePoints.begin();
        auto cd = entry.packedData[index];
        float iw = 1.0f / entry.texture->GetWidth();
        float ih = 1.0f / entry.texture->GetHeight();

        if (text[ch] != ' ') {
            batchRenderer.BatchQuadUV(glm::vec2(cd.x0 * iw, cd.y0 * ih),
                glm::vec2(cd.x1 * iw, cd.y1 * ih),
                glm::vec2(pos.x + cd.xoff, pos.y + cd.yoff),
                glm::vec2(pos.x + cd.xoff2, pos.y + cd.yoff2),
                color);
        }

        pos.x += cd.xadvance;
        if (text[ch + 1]) {
            auto kernFound = entry.kerning.find(std::pair<int, int>(text[ch], text[ch + 1]));
            if (kernFound != entry.kerning.end()) {
                pos.x += entry.scaleFactor * kernFound->second;
            }
        }
        ++ch;
    }
}

void Font::Initialize() {
    if (!stbtt_InitFont(&pImpl->fontInfo, (const uint8_t *)fontMemoryBuffer, 0)) {
        spdlog::error("Couldn't initialize font.");
        throw std::runtime_error("Couldn't initialize font");
    }
}

} // namespace Lucky
