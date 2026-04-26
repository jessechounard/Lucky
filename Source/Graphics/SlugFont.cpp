#include <Lucky/GraphicsDevice.hpp>
#include <Lucky/SlugFont.hpp>
#include <Lucky/SlugRenderer.hpp>

#include <SDL3/SDL.h>
#include <SDL3/SDL_assert.h>
#include <hb-gpu.h>
#include <hb.h>
#include <spdlog/spdlog.h>

#include <cstring>

namespace Lucky {

static const uint32_t INITIAL_ATLAS_CAPACITY = 16384; // int4 elements

SlugFont::SlugFont(GraphicsDevice &graphicsDevice, const std::string &ttfPath)
    : graphicsDevice(graphicsDevice) {

    hbBlob = hb_blob_create_from_file(ttfPath.c_str());
    if (!hb_blob_get_length(hbBlob)) {
        hb_blob_destroy(hbBlob);
        hbBlob = nullptr;
        throw std::runtime_error("Failed to load font: " + ttfPath);
    }
    hbFace = hb_face_create(hbBlob, 0);
    hbFont = hb_font_create(hbFace);
    hbBuffer = hb_buffer_create();
    upem = hb_face_get_upem(hbFace);

    hbGpuDraw = hb_gpu_draw_create_or_fail();
    if (!hbGpuDraw) {
        hb_buffer_destroy(hbBuffer);
        hb_font_destroy(hbFont);
        hb_face_destroy(hbFace);
        hb_blob_destroy(hbBlob);
        throw std::runtime_error("Failed to create HarfBuzz GPU draw");
    }

    // Font metrics from HarfBuzz
    hb_font_extents_t extents;
    hb_font_get_h_extents(hbFont, &extents);
    float emScale = 1.0f / (float)upem;
    ascent = extents.ascender * emScale;
    descent = extents.descender * emScale;
    lineGap = extents.line_gap * emScale;

    // Pre-allocate atlas buffer
    atlasData.resize(INITIAL_ATLAS_CAPACITY * 4, 0);
}

SlugFont::~SlugFont() {
    SDL_GPUDevice *device = graphicsDevice.GetDevice();
    if (atlasBuffer)
        SDL_ReleaseGPUBuffer(device, atlasBuffer);
    if (hbGpuDraw)
        hb_gpu_draw_destroy(hbGpuDraw);
    if (hbBuffer)
        hb_buffer_destroy(hbBuffer);
    if (hbFont)
        hb_font_destroy(hbFont);
    if (hbFace)
        hb_face_destroy(hbFace);
    if (hbBlob)
        hb_blob_destroy(hbBlob);
}

SlugGlyphInfo SlugFont::EncodeGlyph(uint32_t glyphIndex) {
    auto it = glyphCache.find(glyphIndex);
    if (it != glyphCache.end())
        return it->second;

    SlugGlyphInfo gi = {};

    // Get advance width
    hb_position_t advance = hb_font_get_glyph_h_advance(hbFont, glyphIndex);
    float emScale = 1.0f / (float)upem;
    gi.advanceWidth = advance * emScale;

    // Encode glyph using HarfBuzz GPU
    hb_gpu_draw_reset(hbGpuDraw);

    int xScale, yScale;
    hb_font_get_scale(hbFont, &xScale, &yScale);
    hb_gpu_draw_set_scale(hbGpuDraw, xScale, yScale);

    hb_gpu_draw_glyph(hbGpuDraw, hbFont, glyphIndex);

    hb_glyph_extents_t gpuExtents;
    hb_gpu_draw_get_extents(hbGpuDraw, &gpuExtents);

    if (gpuExtents.width == 0 || gpuExtents.height == 0) {
        gi.empty = true;
        glyphCache[glyphIndex] = gi;
        return gi;
    }

    gi.minX = (float)gpuExtents.x_bearing;
    gi.maxX = (float)(gpuExtents.x_bearing + gpuExtents.width);
    // HarfBuzz: y_bearing is top, height is negative (downward)
    gi.maxY = (float)gpuExtents.y_bearing;
    gi.minY = (float)(gpuExtents.y_bearing + gpuExtents.height);

    hb_blob_t *blob = hb_gpu_draw_encode(hbGpuDraw);
    if (!blob) {
        // HarfBuzz couldn't produce an encoded blob for this glyph. Cache
        // it as empty so we don't try again every frame.
        spdlog::error("hb_gpu_draw_encode returned null for glyph {}", glyphIndex);
        gi.empty = true;
        glyphCache[glyphIndex] = gi;
        return gi;
    }

    unsigned int encodedLength;
    const char *encodedData = hb_blob_get_data(blob, &encodedLength);

    if (!encodedData || encodedLength == 0) {
        gi.empty = true;
        hb_gpu_draw_recycle_blob(hbGpuDraw, blob);
        glyphCache[glyphIndex] = gi;
        return gi;
    }

    // Encoded data is RGBA16I: 4 x int16 per element (8 bytes each).
    // Our StructuredBuffer<int4> needs 4 x int32 per element (16 bytes each).
    // Unpack int16 -> int32 during copy.
    uint32_t numElements = encodedLength / 8;

    // Grow atlas if needed
    while ((atlasCursor + numElements) * 4 > atlasData.size()) {
        atlasData.resize(atlasData.size() * 2, 0);
    }

    gi.atlasOffset = atlasCursor;

    const int16_t *src = (const int16_t *)encodedData;
    for (uint32_t i = 0; i < numElements; i++) {
        atlasData[(atlasCursor + i) * 4 + 0] = (int32_t)src[i * 4 + 0];
        atlasData[(atlasCursor + i) * 4 + 1] = (int32_t)src[i * 4 + 1];
        atlasData[(atlasCursor + i) * 4 + 2] = (int32_t)src[i * 4 + 2];
        atlasData[(atlasCursor + i) * 4 + 3] = (int32_t)src[i * 4 + 3];
    }
    atlasCursor += numElements;

    hb_gpu_draw_recycle_blob(hbGpuDraw, blob);

    glyphCache[glyphIndex] = gi;
    return gi;
}

void SlugFont::UploadAtlas() {
    SDL_GPUDevice *device = graphicsDevice.GetDevice();

    uint32_t requiredSize = atlasCursor * 16; // 16 bytes per int4
    if (requiredSize == 0)
        return;

    // Recreate buffer if too small
    if (atlasBuffer && atlasBufferSize < requiredSize) {
        SDL_ReleaseGPUBuffer(device, atlasBuffer);
        atlasBuffer = nullptr;
    }

    if (!atlasBuffer) {
        SDL_GPUBufferCreateInfo bufCI = {};
        bufCI.usage = SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ;
        bufCI.size = requiredSize;
        atlasBuffer = SDL_CreateGPUBuffer(device, &bufCI);
        atlasBufferSize = requiredSize;
        if (!atlasBuffer) {
            spdlog::error("Failed to create atlas buffer: {}", SDL_GetError());
            return;
        }
    }

    // Upload via transfer buffer
    SDL_GPUTransferBufferCreateInfo tbCI = {};
    tbCI.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    tbCI.size = requiredSize;
    SDL_GPUTransferBuffer *tb = SDL_CreateGPUTransferBuffer(device, &tbCI);
    if (!tb)
        return;

    void *mapped = SDL_MapGPUTransferBuffer(device, tb, false);
    memcpy(mapped, atlasData.data(), requiredSize);
    SDL_UnmapGPUTransferBuffer(device, tb);

    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(device);
    SDL_GPUCopyPass *pass = SDL_BeginGPUCopyPass(cmd);

    SDL_GPUTransferBufferLocation src = {};
    src.transfer_buffer = tb;
    src.offset = 0;

    SDL_GPUBufferRegion dst = {};
    dst.buffer = atlasBuffer;
    dst.offset = 0;
    dst.size = requiredSize;

    SDL_UploadToGPUBuffer(pass, &src, &dst, false);
    SDL_EndGPUCopyPass(pass);
    SDL_SubmitGPUCommandBuffer(cmd);
    SDL_ReleaseGPUTransferBuffer(device, tb);
}

void SlugFont::PreloadAscii() {
    // Printable ASCII: space (0x20) through tilde (0x7E) inclusive.
    for (uint32_t c = 0x20; c <= 0x7E; c++) {
        hb_codepoint_t glyphIndex;
        if (hb_font_get_nominal_glyph(hbFont, c, &glyphIndex)) {
            EncodeGlyph(glyphIndex);
        }
    }
    UploadAtlas();
}

void SlugFont::PreloadString(const std::string &text) {
    if (text.empty())
        return;

    ShapeText(text);

    unsigned int glyphCount;
    hb_glyph_info_t *glyphInfo = hb_buffer_get_glyph_infos(hbBuffer, &glyphCount);

    for (unsigned int i = 0; i < glyphCount; i++) {
        EncodeGlyph(glyphInfo[i].codepoint);
    }

    UploadAtlas();
}

void SlugFont::ShapeText(const std::string &text) {
    hb_buffer_clear_contents(hbBuffer);
    hb_buffer_add_utf8(hbBuffer, text.c_str(), (int)text.size(), 0, (int)text.size());
    hb_buffer_guess_segment_properties(hbBuffer);
    hb_shape(hbFont, hbBuffer, nullptr, 0);
}

glm::vec2 SlugFont::MeasureString(const std::string &text, float fontSize) {
    if (text.empty() || fontSize <= 0.0f)
        return {0.0f, 0.0f};

    ShapeText(text);

    unsigned int glyphCount;
    hb_glyph_position_t *glyphPos = hb_buffer_get_glyph_positions(hbBuffer, &glyphCount);

    float hbScale = fontSize / (float)upem;
    float width = 0.0f;
    for (unsigned int i = 0; i < glyphCount; i++) {
        width += glyphPos[i].x_advance * hbScale;
    }

    return {width, GetLineHeight(fontSize)};
}

float SlugFont::GetAscent(float fontSize) const {
    return ascent * fontSize;
}

float SlugFont::GetDescent(float fontSize) const {
    return descent * fontSize;
}

float SlugFont::GetLineHeight(float fontSize) const {
    return (ascent - descent + lineGap) * fontSize;
}

void SlugFont::DrawString(SlugRenderer &renderer, const std::string &text, float x, float y,
    float fontSize, const Color &color) {
    if (text.empty() || fontSize <= 0.0f)
        return;

    ShapeText(text);

    unsigned int glyphCount;
    hb_glyph_info_t *glyphInfo = hb_buffer_get_glyph_infos(hbBuffer, &glyphCount);
    hb_glyph_position_t *glyphPos = hb_buffer_get_glyph_positions(hbBuffer, &glyphCount);

    // Ensure all shaped glyphs are encoded
    bool needsUpload = false;
    for (unsigned int i = 0; i < glyphCount; i++) {
        uint32_t glyphId = glyphInfo[i].codepoint;
        if (glyphCache.find(glyphId) == glyphCache.end()) {
            EncodeGlyph(glyphId);
            needsUpload = true;
        }
    }
    if (needsUpload) {
        UploadAtlas();
        renderer.UpdateAtlas(*this);
    }

    // hbScale converts font units to pixels
    float hbScale = fontSize / (float)upem;
    // invJac maps pixel-space back to font-unit-space (for the shader)
    float invJac = 1.0f / hbScale;

    float cursorX = x;
    float baselineY = y;

    for (unsigned int i = 0; i < glyphCount; i++) {
        uint32_t glyphId = glyphInfo[i].codepoint;

        float xOffset = glyphPos[i].x_offset * hbScale;
        float yOffset = glyphPos[i].y_offset * hbScale;
        float xAdvance = glyphPos[i].x_advance * hbScale;
        float yAdvance = glyphPos[i].y_advance * hbScale;

        auto it = glyphCache.find(glyphId);
        if (it != glyphCache.end() && !it->second.empty) {
            auto &gi = it->second;

            float gx = cursorX + xOffset;
            float gy = baselineY + yOffset;

            // Glyph extents are in font units, convert to pixels for positioning
            float x0 = gx + gi.minX * hbScale;
            float y0 = gy + gi.minY * hbScale;
            float x1 = gx + gi.maxX * hbScale;
            float y1 = gy + gi.maxY * hbScale;

            // UVs are in font units (matching encoded atlas data)
            struct {
                float px, py, nx, ny, eu, ev;
            } corners[4] = {
                {x0, y0, -1, -1, gi.minX, gi.minY},
                {x1, y0, 1, -1, gi.maxX, gi.minY},
                {x1, y1, 1, 1, gi.maxX, gi.maxY},
                {x0, y1, -1, 1, gi.minX, gi.maxY},
            };

            int indices[6] = {0, 1, 2, 0, 2, 3};
            SlugVertex verts[6];

            for (int vi = 0; vi < 6; vi++) {
                auto &c = corners[indices[vi]];
                SlugVertex &v = verts[vi];
                v.posX = c.px;
                v.posY = c.py;
                v.normX = c.nx;
                v.normY = c.ny;
                v.emU = c.eu;
                v.emV = c.ev;
                v.invJac00 = invJac;
                v.invJac01 = 0.0f;
                v.invJac10 = 0.0f;
                v.invJac11 = invJac;
                uint32_t loc = gi.atlasOffset;
                memcpy(&v.glyphLocF, &loc, sizeof(float));
                v._pad0 = 0;
                v.r = color.r;
                v.g = color.g;
                v.b = color.b;
                v.a = color.a;
            }

            renderer.BatchGlyphQuad(verts);
        }

        cursorX += xAdvance;
        baselineY += yAdvance;
    }
}

} // namespace Lucky
