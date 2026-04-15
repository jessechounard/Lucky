#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include <glm/vec2.hpp>

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

/**
 * Per-glyph metadata produced while encoding a glyph into the atlas.
 *
 * Stored in SlugFont's glyph cache. Consumers should not need to touch this
 * directly; DrawString() uses it internally.
 */
struct SlugGlyphInfo {
    uint32_t atlasOffset;         // offset into the GPU atlas storage buffer
    float minX, minY, maxX, maxY; // em-space extents
    float advanceWidth;
    bool empty;
};

/**
 * A TrueType font loaded and rendered via the Slug vector-glyph pipeline.
 *
 * Loads a .ttf (or .otf) file through HarfBuzz, then uses HarfBuzz's GPU
 * draw API to encode each glyph's vector path into a compact blob. Encoded
 * glyphs live in a GPU storage buffer; the matching SlugRenderer's shader
 * walks the encoded path per fragment to compute coverage. The result is a
 * sharp vector outline at any scale with no atlas texture.
 *
 * # Usage
 *
 * Create a font, preload the glyphs you'll use, then draw with a
 * SlugRenderer:
 *
 *     SlugFont font(graphicsDevice, "Content/Fonts/MyFont.ttf");
 *     font.PreloadAscii();
 *
 *     slugRenderer.Begin(BlendMode::Alpha, font, mvp);
 *     font.DrawString(slugRenderer, "Hello", 100, 200, 32.0f, Color::White);
 *     slugRenderer.End();
 *
 * # Glyph preloading
 *
 * Encoding a glyph uploads fresh data to the GPU. DrawString() will
 * transparently encode any missing glyphs the first time they appear,
 * but that path is fragile: the upload happens mid-frame via its own
 * command buffer, and if the atlas storage buffer needs to grow, the old
 * buffer is released while it may still be in flight. For this reason,
 * callers should call PreloadAscii() (or preload their specific glyph set)
 * during setup, before the first frame draws. Treat the dynamic-upload
 * path as a fallback for development, not for shipping code.
 *
 * # Coordinate conventions
 *
 * Text is drawn in pixel-space coordinates (consistent with the rest of
 * Lucky's renderers). The `(x, y)` position passed to DrawString() is the
 * left edge of the baseline. Y values increase upward, so glyphs with a
 * positive ascender extend above `y` and descenders extend below.
 *
 * # No newline handling
 *
 * DrawString() shapes the entire string as a single HarfBuzz run, so
 * `'\n'` is treated like any other glyph (usually glyph 0 with an advance
 * but no visible shape). To render multi-line text, split the string on
 * line breaks and call DrawString() once per line, decrementing the y
 * coordinate by GetLineHeight() between lines.
 *
 * # Thread safety
 *
 * SlugFont is not thread-safe. A single instance must be used from one
 * thread at a time, and must be used with one SlugRenderer at a time.
 */
class SlugFont {
  public:
    /**
     * Loads a font from a file path.
     *
     * Reads the .ttf or .otf file, creates a HarfBuzz font object, and
     * prepares the GPU atlas buffer. Throws std::runtime_error if the file
     * cannot be loaded or HarfBuzz setup fails.
     *
     * \param graphicsDevice the graphics device that owns the GPU resources.
     *                       Must outlive the SlugFont.
     * \param ttfPath path to a TrueType or OpenType font file.
     */
    SlugFont(GraphicsDevice &graphicsDevice, const std::string &ttfPath);
    SlugFont(const SlugFont &) = delete;
    SlugFont &operator=(const SlugFont &) = delete;
    ~SlugFont();

    /**
     * Encodes and uploads all printable ASCII glyphs (U+0020 through U+007E).
     *
     * After this call the font is safe to use with ASCII-only text without
     * triggering the dynamic upload path.
     */
    void PreloadAscii();

    /**
     * Encodes and uploads the glyphs required to render `text`.
     *
     * Runs HarfBuzz shaping on the input, extracts the resulting glyph
     * indices, and encodes any that are not already cached. The atlas is
     * uploaded once at the end. Because shaping is used, this method
     * preloads ligature glyphs correctly: e.g., preloading `"office"` in
     * a font with an "ffi" ligature will include the ligature glyph.
     *
     * Callers that know exactly which text they will render can call this
     * method in setup to avoid the dynamic upload path during the first
     * DrawString of that text.
     *
     * \param text the UTF-8 text to preload. May contain any characters,
     *             including duplicates -- each unique resulting glyph is
     *             encoded once.
     */
    void PreloadString(const std::string &text);

    /**
     * Draws a string at the given baseline position.
     *
     * The caller must have called SlugRenderer::Begin(..., *this, ...)
     * before calling DrawString. The matching SlugRenderer::End() flushes
     * the accumulated glyph vertices to the GPU.
     *
     * Missing glyphs will be encoded and uploaded on demand, but this is
     * discouraged in shipping code; see the class-level preloading note.
     *
     * \param renderer the SlugRenderer to append glyph geometry to. Must
     *                 have been started via Begin() with this same font.
     * \param text the UTF-8 string to draw. Does not handle newlines; see
     *             the class-level note.
     * \param x the left edge of the baseline in pixel space.
     * \param y the baseline Y in pixel space.
     * \param fontSize the font size in pixels (em).
     * \param color the color applied to every glyph.
     */
    void DrawString(SlugRenderer &renderer, const std::string &text, float x, float y,
        float fontSize, const Color &color);

    /**
     * Measures the pixel size of a string at the given font size without
     * drawing it.
     *
     * Runs HarfBuzz shaping internally but does not encode glyphs, upload
     * data, or emit any GPU geometry — safe to call every frame for layout
     * and alignment.
     *
     * The returned width is the total horizontal advance of the shaped
     * glyph run. The returned height is the font's line height at the
     * requested size. Like DrawString, this does not handle newlines.
     *
     * \param text the UTF-8 string to measure.
     * \param fontSize the font size in pixels.
     * \returns the text size as (width, height) in pixel space.
     */
    glm::vec2 MeasureString(const std::string &text, float fontSize);

    /**
     * Returns the font's ascent at the given size, in pixels.
     *
     * The ascent is the distance from the baseline up to the tallest glyph
     * the font declares. Positive in Lucky's Y-up coordinate system.
     *
     * \param fontSize the font size in pixels.
     * \returns the ascent in pixels.
     */
    float GetAscent(float fontSize) const;

    /**
     * Returns the font's descent at the given size, in pixels.
     *
     * The descent is the distance from the baseline down to the lowest
     * glyph the font declares. Conventionally negative (below the baseline)
     * in Lucky's Y-up coordinate system.
     *
     * \param fontSize the font size in pixels.
     * \returns the descent in pixels (typically negative).
     */
    float GetDescent(float fontSize) const;

    /**
     * Returns the font's recommended line height at the given size, in
     * pixels.
     *
     * Equal to `ascent - descent + lineGap` scaled to the requested size.
     * Use this as the vertical step between baselines when drawing
     * multi-line text.
     *
     * \param fontSize the font size in pixels.
     * \returns the line height in pixels.
     */
    float GetLineHeight(float fontSize) const;

    /**
     * Returns the handle of the GPU storage buffer holding encoded glyph
     * data. Consumed by SlugRenderer during Begin() and UpdateAtlas().
     */
    SDL_GPUBuffer *GetAtlasBuffer() const {
        return atlasBuffer;
    }

  private:
    SlugGlyphInfo encodeGlyph(uint32_t glyphIndex);
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
    std::unordered_map<uint32_t, SlugGlyphInfo> glyphCache;

    // Font metrics in em space (scale by fontSize / upem for pixel space)
    float ascent = 0, descent = 0, lineGap = 0;
};

} // namespace Lucky
