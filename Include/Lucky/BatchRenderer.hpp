#pragma once

#include <map>
#include <memory>
#include <stdint.h>
#include <vector>

#include <glm/glm.hpp>
#include <Lucky/GraphicsDevice.hpp>
#include <Lucky/Rectangle.hpp>
#include <Lucky/Shader.hpp>
#include <Lucky/Types.hpp>
#include <Lucky/VertexBuffer.hpp>

namespace Lucky {

struct BatchRenderer;
struct Color;

/**
 * The vertex layout consumed by BatchRenderer's pipelines: position, UV,
 * color.
 *
 * 32 bytes, packed in declaration order: vec2 position, vec2 uv, vec4 rgba.
 * Other renderers in Lucky (SlugRenderer, ShapeRenderer) define their own
 * vertex types matching their specialized pipelines.
 */
struct BatchVertex {
    float x, y;       /**< pixel-space position. */
    float u, v;       /**< texture coordinates, normalized to [0, 1]. */
    float r, g, b, a; /**< color, linear [0, 1] per channel. */
};

inline constexpr UVMode operator&(UVMode lhs, UVMode rhs) {
    return static_cast<UVMode>(static_cast<uint32_t>(lhs) & static_cast<uint32_t>(rhs));
}

inline UVMode &operator&=(UVMode &lhs, UVMode rhs) {
    lhs = lhs & rhs;
    return lhs;
}

inline constexpr UVMode operator|(UVMode lhs, UVMode rhs) {
    return static_cast<UVMode>(static_cast<uint32_t>(lhs) | static_cast<uint32_t>(rhs));
}

inline UVMode &operator|=(UVMode &lhs, UVMode rhs) {
    lhs = lhs | rhs;
    return lhs;
}

inline bool HasFlag(UVMode t, UVMode flag) {
    return (static_cast<uint32_t>(t) & static_cast<uint32_t>(flag)) != 0;
}

/**
 * Accumulates quad and triangle geometry into a single dynamic vertex buffer
 * and flushes it to the GPU as one draw call per state change.
 *
 * # Usage
 *
 * Construct one BatchRenderer per context. Draw like this:
 *
 *     batchRenderer.Begin(BlendMode::Alpha, texture);
 *     batchRenderer.BatchQuadUV(...);
 *     batchRenderer.BatchQuadUV(...);
 *     batchRenderer.End();
 *
 * Every Batch*() call between Begin and End accumulates vertices into a
 * shared buffer. Those vertices are flushed to the GPU either when the
 * buffer fills up (transparently, mid-batch) or when End() is called.
 *
 * # Begin overloads
 *
 * Begin() has four forms. They cover the common combinations of texture
 * and fragment shader:
 *
 * - `Begin(blendMode)` — no texture (uses the internal 1x1 white pixel),
 *   default sprite fragment shader. Good for vertex-color triangles and
 *   solid quads.
 * - `Begin(blendMode, texture)` — user texture, default fragment shader.
 *   The common sprite case.
 * - `Begin(blendMode, texture, shader)` — user texture, user fragment
 *   shader. For post-process effects and custom looks.
 * - `Begin(blendMode, shader)` — no texture, user fragment shader. For
 *   shaders that only use UVs (for example SDF shape shaders) and do not
 *   sample a bound texture. Calling this with a shader that samples
 *   `Texture0` is a programming error: no sampler will be bound.
 *
 * All four optionally take a model/view transform matrix, applied before
 * the BatchRenderer's built-in orthographic projection.
 *
 * # Coordinate system
 *
 * BatchRenderer uses a pixel-space orthographic projection built from the
 * current GraphicsDevice viewport. With an identity transform, input x/y
 * coordinates are interpreted in pixel space where (0, 0) is the viewport
 * origin and (width, height) is the opposite corner.
 *
 * # Thread safety
 *
 * BatchRenderer is not thread-safe. A single instance must be used from
 * one thread at a time.
 */
struct BatchRenderer {
  public:
    /**
     * Constructs a BatchRenderer with the given upper bound on in-flight
     * vertices.
     *
     * Lucky ships a default sprite vertex and fragment shader pair in
     * `Content/Shaders/sprite.vert` and `Content/Shaders/sprite.frag`; the
     * constructor loads them via the host's base path.
     *
     * \param graphicsDevice the graphics device that owns the GPU resources.
     *                       Must outlive the BatchRenderer.
     * \param maximumTriangles the hard upper bound on triangles that can be
     *                         in-flight before a mid-batch flush is forced.
     *                         Must be positive.
     */
    BatchRenderer(GraphicsDevice &graphicsDevice, uint32_t maximumTriangles);
    BatchRenderer(const BatchRenderer &) = delete;
    ~BatchRenderer();

    BatchRenderer &operator=(const BatchRenderer &) = delete;
    BatchRenderer &operator=(const BatchRenderer &&) = delete;

    /**
     * Returns true when a Begin/End block is currently open.
     */
    bool BatchStarted() const {
        return batchStarted;
    }

    /**
     * Begins a batch with the internal 1x1 white pixel texture and the
     * default sprite fragment shader.
     *
     * Good for vertex-colored primitives (triangles or quads) where the
     * caller wants color interpolation without binding a texture.
     *
     * \param blendMode the blend mode for this batch.
     * \param transformMatrix an optional model/view matrix applied before
     *                        the built-in orthographic projection.
     */
    void Begin(BlendMode blendMode, const glm::mat4 &transformMatrix = glm::mat4(1.0f));

    /**
     * Begins a batch with a user-supplied texture and the default sprite
     * fragment shader.
     *
     * \param blendMode the blend mode for this batch.
     * \param texture the texture to sample. Must outlive the Begin/End block.
     * \param transformMatrix an optional model/view matrix.
     */
    void Begin(
        BlendMode blendMode, Texture &texture, const glm::mat4 &transformMatrix = glm::mat4(1.0f));

    /**
     * Begins a batch with a user-supplied texture and a user-supplied
     * fragment shader.
     *
     * \param blendMode the blend mode for this batch.
     * \param texture the texture to sample. Must outlive the Begin/End block.
     * \param fragmentShader the fragment shader to use. Must outlive the
     *                       Begin/End block.
     * \param transformMatrix an optional model/view matrix.
     */
    void Begin(BlendMode blendMode, Texture &texture, Shader &fragmentShader,
        const glm::mat4 &transformMatrix = glm::mat4(1.0f));

    /**
     * Begins a batch with a user-supplied fragment shader and no texture.
     *
     * No sampler is bound for this batch, so the fragment shader must not
     * sample `Texture0`. Intended for effect shaders that read only from
     * vertex inputs and uniforms (SDF shape shaders, for example).
     *
     * \param blendMode the blend mode for this batch.
     * \param fragmentShader the fragment shader to use. Must outlive the
     *                       Begin/End block, and must not sample a texture.
     * \param transformMatrix an optional model/view matrix.
     */
    void Begin(BlendMode blendMode, Shader &fragmentShader,
        const glm::mat4 &transformMatrix = glm::mat4(1.0f));

    /**
     * Ends the current batch and flushes any remaining vertices to the GPU.
     *
     * After End() returns the BatchRenderer is idle. Any fragment uniform
     * data set via SetFragmentUniformData() is cleared.
     */
    void End();

    /**
     * Appends a single axis-aligned quad to the current batch.
     *
     * The quad's lower-left corner is at `xy0` and upper-right at `xy1`.
     * UVs are interpolated between `uv0` and `uv1`. An optional rotation,
     * in radians, rotates the quad around its center.
     *
     * \param uv0 the UV of the lower-left vertex.
     * \param uv1 the UV of the upper-right vertex.
     * \param xy0 the world-space position of the lower-left vertex.
     * \param xy1 the world-space position of the upper-right vertex.
     * \param color the color applied uniformly to all four vertices.
     * \param rotation the rotation in radians around the quad's center.
     */
    void BatchQuadUV(const glm::vec2 &uv0, const glm::vec2 &uv1, const glm::vec2 &xy0,
        const glm::vec2 &xy1, const Color &color, float rotation = 0.0f);

    /**
     * Appends a sprite-oriented textured quad to the current batch.
     *
     * High-level "draw a sprite" entry point: computes UVs from a
     * sub-rectangle of the bound texture, positions, scales, rotates, and
     * pivots the destination quad, and corrects for the Y-up-screen vs
     * Y-down-texture orientation mismatch so the pixels appear upright.
     *
     * # Orientation
     *
     * BatchRenderer uses Y-up pixel coordinates while texture rows are
     * stored Y-down (row 0 at the top of the image). This method applies a
     * V flip internally so the default `uvMode` produces an upright sprite
     * when rendering directly to the swapchain. The symbolic flag names in
     * `UVMode` describe the *visible* result: `FlipVertical` makes the
     * sprite appear upside-down, `FlipHorizontal` mirrors it left-to-right.
     *
     * The raw overloads (`BatchQuad`, `BatchQuadUV`, `BatchTriangles`) pass
     * caller-supplied UVs through unchanged — no automatic flip — so a
     * caller composing its own UV rectangles is responsible for whichever
     * orientation it wants.
     *
     * \param sourceRectangle the source sub-rectangle in texture pixels,
     *                        or `nullptr` to use the whole texture.
     * \param position the destination center (or pivot — see `origin`) in
     *                 world space.
     * \param rotation the rotation in radians around `position`.
     * \param scale a scale factor applied to the destination size.
     * \param origin the pivot point in [0, 1] x [0, 1]. (0, 0) means the
     *               quad's lower-left corner is at `position`; (0.5, 0.5)
     *               means `position` is the center.
     * \param uvMode flags that flip or rotate the UV coordinates. See
     *               Orientation above for the sense of each flip.
     * \param color the color applied uniformly to all four vertices.
     *
     * \note Must not be called when the batch was started without a
     *       texture (the shader-only Begin overload).
     */
    void BatchSprite(const Rectangle *sourceRectangle, const glm::vec2 &position,
        const float rotation, const glm::vec2 &scale, const glm::vec2 &origin, const UVMode uvMode,
        const Color &color);

    /**
     * Appends a quad defined by four explicit corners and UVs.
     *
     * Use this when the quad is not axis-aligned (for example, glyphs
     * following a curved path). The corners are used as-is with no
     * additional rotation or centering.
     *
     * \param corners the four corner positions in world space, in
     *                counter-clockwise order starting from the lower-left.
     * \param uvs the four UV coordinates corresponding to `corners`.
     * \param color the color applied uniformly to all four vertices.
     */
    void BatchQuad(const glm::vec2 corners[4], const glm::vec2 uvs[4], const Color &color);

    /**
     * Appends raw triangles to the current batch.
     *
     * Each triangle is specified by three consecutive BatchVertex entries
     * in `triangleVertices`. Vertices carry per-vertex position, UV, and
     * color; the GPU interpolates between them.
     *
     * \param triangleVertices a pointer to `triangleCount * 3` vertices.
     *                         Must not be null.
     * \param triangleCount the number of triangles to append. Must be
     *                      positive.
     */
    void BatchTriangles(const BatchVertex *triangleVertices, const int triangleCount);

    /**
     * Sets the fragment shader uniform data for the current batch.
     *
     * The `data` pointer is stored, not copied. It must remain valid from
     * the call to SetFragmentUniformData until the next End() (or until the
     * batch implicitly flushes due to overflow — in practice that means
     * "until End()"). Passing a pointer to a stack variable inside a
     * Begin/End block is the typical usage.
     *
     * \param data a pointer to the uniform data. The memory must outlive
     *             the current Begin/End block.
     * \param size the size of the uniform data in bytes.
     * \param slot the fragment shader uniform slot. Defaults to 0.
     */
    void SetFragmentUniformData(const void *data, uint32_t size, uint32_t slot = 0);

  private:
    void Flush();
    SDL_GPUGraphicsPipeline *GetOrCreatePipeline(
        BlendMode blendMode, SDL_GPUTextureFormat targetFormat, SDL_GPUShader *fragShader);

    GraphicsDevice *graphicsDevice;
    std::unique_ptr<Texture> whitePixelTexture;
    Texture *texture;
    std::unique_ptr<Shader> vertexShader;
    std::unique_ptr<Shader> fragmentShader;
    Shader *activeFragmentShader;
    std::unique_ptr<VertexBuffer<BatchVertex>> vertexBuffer;
    glm::mat4 transformMatrix;
    BlendMode blendMode;

    uint32_t activeVertices;
    uint32_t maximumVertices;

    std::vector<BatchVertex> vertices;

    bool batchStarted;

    const void *fragmentUniformData = nullptr;
    uint32_t fragmentUniformSize = 0;
    uint32_t fragmentUniformSlot = 0;

    struct PipelineKey {
        BlendMode blendMode;
        SDL_GPUTextureFormat targetFormat;
        SDL_GPUShader *fragShader;

        bool operator<(const PipelineKey &other) const {
            if (blendMode != other.blendMode)
                return blendMode < other.blendMode;
            if (targetFormat != other.targetFormat)
                return targetFormat < other.targetFormat;
            return fragShader < other.fragShader;
        }
    };
    std::map<PipelineKey, SDL_GPUGraphicsPipeline *> pipelines;
};

} // namespace Lucky
