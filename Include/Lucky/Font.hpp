#pragma once

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <glm/vec2.hpp>

#include <Lucky/BatchRenderer.hpp>
#include <Lucky/Color.hpp>
#include <Lucky/Texture.hpp>

namespace Lucky {

struct GraphicsDevice;

class Font {
  public:
    Font(GraphicsDevice &graphicsDevice, const std::string &fileName);
    Font(GraphicsDevice &graphicsDevice, void *memory);
    ~Font();

    // todo: more predefined code point sets?
    static std::vector<int> GetCodePointsLatin();

    // todo: future optimization - allow packing multiple sizes into one texture
    // todo: do we need to specify a maximum texture size?
    std::shared_ptr<Texture> CreateFontEntry(const std::string &entryName, const float fontSize,
        std::vector<int> codePoints, uint32_t oversampling = 1, bool kerningEnabled = true);

    std::shared_ptr<Texture> GetTexture(const std::string &entryName);

    glm::vec2 MeasureString(const std::string &text, const std::string &entryName);

    void DrawString(BatchRenderer &batchRenderer, const std::string &text,
        const std::string &entryName, glm::vec2 position, Color color);

    float GetLineAdvance(const std::string &entryName) const;
    float GetAscent(const std::string &entryName) const;
    float GetDescent(const std::string &entryName) const;
    float GetLineGap(const std::string &entryName) const;

  private:
    void Initialize();

    GraphicsDevice &graphicsDevice;
    void *fontMemoryBuffer;
    bool shouldFreeMemory;

    struct Impl;
    std::unique_ptr<Impl> pImpl;
};

} // namespace Lucky
