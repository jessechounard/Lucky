#pragma once

#include <memory>
#include <stdint.h>
#include <vector>

#include <glm/glm.hpp>

#include <Lucky/Shader.hpp>
#include <Lucky/VertexBuffer.hpp>

struct SDL_GPUGraphicsPipeline;
struct SDL_GPUBuffer;

namespace Lucky {

struct GraphicsDevice;
class SlugFont;
enum class BlendMode;

/**
 * A single vertex in the Slug glyph pipeline.
 *
 * Laid out to pack into exactly 64 bytes so it maps cleanly onto four
 * `float4` vertex attribute slots. `glyphLocF` is the atlas offset
 * reinterpreted as a float so it can travel through a `float4` slot — the
 * shader bitcasts it back to a uint. Callers normally do not construct
 * these manually; SlugFont::DrawString builds them internally and passes
 * them to SlugRenderer::BatchGlyphQuad.
 */
struct SlugVertex {
    float posX, posY;         // object-space position
    float normX, normY;       // outward normal for dilation     (float4 @ location 0)
    float emU, emV;           // em-space texture coordinates
    float invJac00, invJac01; // inverse Jacobian row 0          (float4 @ location 1)
    float invJac10, invJac11; // inverse Jacobian row 1
    float glyphLocF;          // atlas offset (bitcast to uint in shader)
    float _pad0;              //                                 (float4 @ location 2)
    float r, g, b, a;         // color                           (float4 @ location 3)
}; // 64 bytes

/**
 * Batches glyph quads produced by SlugFont and draws them through the
 * Slug fragment shader.
 *
 * The fragment shader reads each glyph's encoded vector path from a GPU
 * storage buffer (the "atlas") belonging to a SlugFont, and walks the path
 * per-pixel to compute coverage. SlugRenderer owns the vertex buffer and
 * pipeline; the atlas buffer it samples comes from the active SlugFont.
 *
 * # Usage
 *
 * Paired with a SlugFont:
 *
 *     slugRenderer.Begin(BlendMode::Alpha, font, mvpMatrix);
 *     font.DrawString(slugRenderer, "Hello", x, y, size, color);
 *     slugRenderer.End();
 *
 * # Render pass interaction
 *
 * SlugRenderer::End() (or a mid-batch overflow flush) calls
 * GraphicsDevice::EndRenderPass / BeginRenderPass around the vertex buffer
 * upload, because SDL_GPU copy operations cannot happen inside a render
 * pass. This means Flush may restart the active render pass once during a
 * batch. Callers should assume that after End() returns, the active render
 * pass is whatever BeginFrame's first render pass would be.
 *
 * # Thread safety
 *
 * SlugRenderer is not thread-safe. A single instance must be used from one
 * thread at a time.
 */
struct SlugRenderer {
    /**
     * Constructs a SlugRenderer with the given upper bound on in-flight
     * vertices.
     *
     * Loads Lucky's slug vertex and fragment shaders from
     * `Content/Shaders/slug.vert` and `Content/Shaders/slug.frag` via the
     * host's base path.
     *
     * \param graphicsDevice the graphics device that owns the GPU
     *                       resources. Must outlive the SlugRenderer.
     * \param maxTriangles the hard upper bound on triangles held in flight
     *                     before a mid-batch flush is forced. Each glyph
     *                     uses two triangles, so this must be at least
     *                     two; in practice pick a number large enough for
     *                     one frame's worth of expected glyphs (a few
     *                     hundred is typical for UI text).
     */
    SlugRenderer(GraphicsDevice &graphicsDevice, uint32_t maxTriangles);
    ~SlugRenderer();

    SlugRenderer(const SlugRenderer &) = delete;
    SlugRenderer &operator=(const SlugRenderer &) = delete;

    /**
     * Begins a batch against a specific font.
     *
     * Caches the atlas buffer pointer from the font and stores the MVP
     * matrix used by the vertex shader. The MVP should map pixel-space
     * input coordinates to NDC the same way BatchRenderer's built-in
     * orthographic projection does. For a pixel-space Y-up setup, build it
     * with `glm::ortho(0, width, 0, height)`.
     *
     * Calling Begin() while a batch is already open is a programming error
     * and will assert.
     *
     * \param blendMode the blend mode for this batch.
     * \param font the SlugFont whose atlas buffer this batch will sample
     *             from. Must outlive the Begin/End block.
     * \param mvpMatrix the model/view/projection matrix for the batch.
     *                  Defaults to identity.
     */
    void Begin(
        BlendMode blendMode, const SlugFont &font, const glm::mat4 &mvpMatrix = glm::mat4(1.0f));

    /**
     * Ends the current batch and flushes remaining glyph vertices.
     *
     * Must be paired with a preceding Begin(). Calling End() without a
     * matching Begin() is a programming error and will assert.
     */
    void End();

    /**
     * Refreshes the cached atlas buffer handle from the given font.
     *
     * Called by SlugFont::DrawString() when it encodes new glyphs
     * mid-batch, since growing the atlas may replace the underlying
     * SDL_GPUBuffer handle. Application code normally does not call this
     * directly.
     *
     * \param font the SlugFont to fetch the new atlas buffer from.
     */
    void UpdateAtlas(const SlugFont &font);

    /**
     * Appends six vertices forming one glyph quad (two triangles) to the
     * current batch.
     *
     * If the batch would overflow, this method flushes the accumulated
     * vertices to the GPU first. Called by SlugFont::DrawString();
     * application code normally does not call this directly.
     *
     * \param verts the six vertices of the glyph quad. The caller is
     *              responsible for constructing them with valid atlas
     *              offsets and inverse Jacobians.
     */
    void BatchGlyphQuad(const SlugVertex verts[6]);

  private:
    void Flush();
    void CreatePipeline(BlendMode blendMode);

    GraphicsDevice *graphicsDevice;

    std::unique_ptr<Shader> vertexShader;
    std::unique_ptr<Shader> fragmentShader;
    SDL_GPUGraphicsPipeline *pipeline = nullptr;
    BlendMode currentBlendMode{};

    std::unique_ptr<VertexBuffer<SlugVertex>> vertexBuffer;

    std::vector<SlugVertex> vertices;
    uint32_t activeVertices = 0;
    uint32_t maxVertices;

    // State for current batch
    SDL_GPUBuffer *atlasBuffer = nullptr;
    glm::mat4 mvpMatrix;
    bool batchStarted = false;
};

} // namespace Lucky
