#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include <Lucky/Color.hpp>

struct SDL_GPUDevice;
struct SDL_GPUBuffer;
struct hb_blob_t;
struct hb_face_t;
struct hb_font_t;
struct hb_buffer_t;
struct hb_gpu_draw_t;

namespace Lucky {

struct GraphicsDevice;
struct SlugRenderer;

struct SlugGlyphInfo {
    uint32_t atlasOffset;         // offset into atlas StructuredBuffer
    float minX, minY, maxX, maxY; // em-space extents
    float advanceWidth;
    bool empty;
};

class SlugFont {
  public:
    SlugFont(GraphicsDevice &graphicsDevice, const std::string &ttfPath);
    ~SlugFont();

    void PreloadAscii();

    void DrawString(SlugRenderer &renderer, const std::string &text, float x, float y,
        float fontSize, const Color &color);

    SDL_GPUBuffer *GetAtlasBuffer() const {
        return atlasBuffer;
    }

  private:
    SlugGlyphInfo encodeGlyph(int glyphIndex);
    void uploadAtlas();
    void shapeText(const std::string &text);

    GraphicsDevice &graphicsDevice;

    // HarfBuzz
    hb_blob_t *hbBlob = nullptr;
    hb_face_t *hbFace = nullptr;
    hb_font_t *hbFont = nullptr;
    hb_buffer_t *hbBuffer = nullptr;
    hb_gpu_draw_t *hbGpuDraw = nullptr;
    unsigned int upem = 0;

    // Atlas data (CPU-side, before upload)
    std::vector<int32_t> atlasData; // int4 elements (4 int32 per entry)
    uint32_t atlasCursor = 0;

    // GPU buffer
    SDL_GPUBuffer *atlasBuffer = nullptr;
    uint32_t atlasBufferSize = 0;

    // Glyph cache
    std::unordered_map<int, SlugGlyphInfo> glyphCache;

    // Font metrics
    float ascent = 0, descent = 0, lineGap = 0;
};

} // namespace Lucky
