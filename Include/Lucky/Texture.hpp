#pragma once

#include <stdint.h>
#include <string>

#include <SDL3/SDL_gpu.h>

namespace Lucky {

/**
 * Filter mode used when a texture is sampled at a non-1:1 ratio.
 *
 * - `Linear` — bilinear filtering (GPU blends between neighboring texels).
 *   Produces smooth magnification and minification; the standard choice for
 *   photographic content and most sprites.
 * - `Point` — nearest-neighbor filtering. Preserves hard texel edges, best
 *   for pixel art and when you explicitly do not want interpolation.
 */
enum class TextureFilter {
    Linear,
    Point,
};

/**
 * How a texture will be used by the GPU.
 *
 * - `Default` — 2D, sampler-only. The texture can be bound as a shader
 *   input.
 * - `RenderTarget` — 2D, sampler *and* color target. The texture can be
 *   bound as the output of a render pass and later sampled as input. Use
 *   for effects like bloom, off-screen composition, or post-processing.
 * - `DepthTarget` — 2D depth-stencil target, also sampleable. Used for
 *   shadow maps and any custom depth pass that the application later reads
 *   back through a sampler. Created with no internal sampler -- pair with a
 *   `Sampler` configured for the desired comparison or filter mode.
 * - `CubeRenderTarget` — cube-map color target, sampleable. The six faces
 *   share one allocation; bind a face by layer index when starting a render
 *   pass. Useful for environment captures and dynamic reflections. Created
 *   with no internal sampler -- pair with a `Sampler`.
 * - `CubeDepthTarget` — cube-map depth-stencil target, sampleable. Used for
 *   omnidirectional (point-light) shadow maps. Created with no internal
 *   sampler -- pair with a `Sampler`.
 */
enum class TextureType {
    Default,
    RenderTarget,
    DepthTarget,
    CubeRenderTarget,
    CubeDepthTarget,
};

/**
 * Returns true for `TextureType` values whose underlying SDL_GPU texture
 * has six layers (one per cube face) instead of one.
 */
constexpr bool IsCubeTextureType(TextureType type) {
    return type == TextureType::CubeRenderTarget || type == TextureType::CubeDepthTarget;
}

/**
 * Returns true for `TextureType` values that allocate a depth-stencil
 * target instead of a color target. Depth textures must be created with
 * `TextureFormat::Depth`.
 */
constexpr bool IsDepthTextureType(TextureType type) {
    return type == TextureType::DepthTarget || type == TextureType::CubeDepthTarget;
}

/**
 * Pixel format for a texture's GPU storage.
 *
 * - `Normal` — 8-bit RGBA (`R8G8B8A8_UNORM`), 4 bytes per pixel. The default
 *   for sprites, UI, fonts, and most content.
 * - `HDR` — 16-bit-per-channel half-float RGBA (`R16G16B16A16_FLOAT`),
 *   8 bytes per pixel. For render targets that need to store values outside
 *   `[0, 1]` (bloom, tone mapping). File and in-memory image loading do not
 *   currently support this format; supply pre-formatted half-float data via
 *   the raw-data constructor instead.
 * - `Depth` — driver-supported 24-bit unorm depth, falling back to 32-bit
 *   float depth if the device does not advertise D24. Required for
 *   `TextureType::DepthTarget` and `TextureType::CubeDepthTarget`. Cannot
 *   be uploaded to from CPU pixel data.
 */
enum class TextureFormat {
    Normal,
    HDR,
    Depth,
};

/**
 * Returns the number of bytes a single pixel occupies in the given format.
 *
 * \param format the pixel format.
 * \returns the bytes-per-pixel for `format`.
 */
constexpr uint32_t BytesPerPixel(TextureFormat format) {
    switch (format) {
    case TextureFormat::Normal:
        return 4;
    case TextureFormat::HDR:
        return 8;
    case TextureFormat::Depth:
        return 4;
    }
    return 4;
}

struct GraphicsDevice;

/**
 * A 2D GPU texture plus its paired sampler.
 *
 * Wraps an `SDL_GPUTexture` together with an `SDL_GPUSampler` configured for
 * the requested filter mode. The sampler's address mode is fixed at
 * clamp-to-edge on all axes.
 *
 * # Usage
 *
 * Load from a file:
 *
 *     Texture texture(graphicsDevice, "Content/Sprites/player.png");
 *
 * Load from an in-memory encoded image (PNG/JPEG/etc.):
 *
 *     Texture texture(graphicsDevice, encodedBytes, encodedByteCount);
 *
 * Create from raw pixel data (sized to the format's bytes-per-pixel), or as
 * a render target with no initial pixels:
 *
 *     Texture pixels(graphicsDevice, TextureType::Default, w, h, rgba, w * h * 4);
 *     Texture rt(graphicsDevice, TextureType::RenderTarget, w, h, nullptr, 0);
 *
 * Update a sub-region later with `SetTextureData()`.
 *
 * # Lifetime
 *
 * The `GraphicsDevice` passed to the constructor must outlive the Texture;
 * destruction releases the GPU texture and sampler through it.
 *
 * # Upload synchronization
 *
 * Every upload (construction with pixel data, and `SetTextureData()`)
 * acquires its own `SDL_GPUCommandBuffer` and submits it immediately. This
 * is suitable outside the frame loop and for mid-frame updates that do not
 * need to be ordered with the main command buffer. It is not a batched
 * upload path.
 *
 * # Thread safety
 *
 * Not thread-safe. A single Texture instance must be used from one thread at
 * a time. The GraphicsDevice it uses is likewise single-threaded.
 */
struct Texture {
  public:
    /**
     * Loads an image from a file and creates a GPU texture from it.
     *
     * Supported formats are whatever `stb_image` accepts (PNG, JPEG, BMP,
     * TGA, and others). The image is decoded to 8-bit RGBA regardless of its
     * source channel count.
     *
     * \param graphicsDevice the graphics device that owns the GPU resources.
     *                       Must outlive this Texture.
     * \param filename path to the image file.
     * \param textureFilter the sampler filter mode. Defaults to Linear.
     * \param textureFormat the GPU pixel format. Only `Normal` is supported
     *                      for file loading; passing `HDR` throws.
     * \throws std::runtime_error if the file cannot be opened or decoded,
     *                            or if HDR is requested.
     */
    Texture(GraphicsDevice &graphicsDevice, const std::string &filename,
        TextureFilter textureFilter = TextureFilter::Linear,
        TextureFormat textureFormat = TextureFormat::Normal);

    /**
     * Decodes an in-memory encoded image and creates a GPU texture from it.
     *
     * Same format coverage as the file constructor — the caller supplies the
     * encoded bytes (PNG, JPEG, etc.) instead of a path. Useful for assets
     * bundled into executables or delivered over the network.
     *
     * \param graphicsDevice the graphics device that owns the GPU resources.
     *                       Must outlive this Texture.
     * \param memory pointer to the encoded image bytes. Must not be null.
     * \param memoryLength size of the encoded buffer in bytes.
     * \param textureFilter the sampler filter mode. Defaults to Linear.
     * \param textureFormat the GPU pixel format. Only `Normal` is supported
     *                      for in-memory loading; passing `HDR` throws.
     * \throws std::runtime_error if the image cannot be decoded, or if HDR
     *                            is requested.
     */
    Texture(GraphicsDevice &graphicsDevice, uint8_t *memory, uint32_t memoryLength,
        TextureFilter textureFilter = TextureFilter::Linear,
        TextureFormat textureFormat = TextureFormat::Normal);

    /**
     * Creates a texture from raw pixel data, or allocates an uninitialized
     * render target.
     *
     * Pass `nullptr` for `pixelData` to create an empty render target (use
     * `TextureType::RenderTarget`) or an empty texture that will be filled
     * in later via `SetTextureData()`. When `pixelData` is non-null, it
     * must contain at least `width * height * BytesPerPixel(textureFormat)`
     * bytes laid out as tightly-packed rows.
     *
     * \param graphicsDevice the graphics device that owns the GPU resources.
     *                       Must outlive this Texture.
     * \param textureType whether this is a sampler-only or render-target
     *                    texture.
     * \param width the texture width in pixels. Must be positive.
     * \param height the texture height in pixels. Must be positive.
     * \param pixelData pointer to tightly-packed pixel data, or `nullptr`
     *                  to leave the texture uninitialized.
     * \param dataLength length of the `pixelData` buffer in bytes; must be
     *                   at least the expected packed size when `pixelData`
     *                   is non-null.
     * \param textureFilter the sampler filter mode. Defaults to Linear.
     * \param textureFormat the GPU pixel format. Defaults to Normal.
     * \throws std::runtime_error on GPU allocation or upload failure.
     */
    Texture(GraphicsDevice &graphicsDevice, TextureType textureType, uint32_t width,
        uint32_t height, uint8_t *pixelData, uint32_t dataLength,
        TextureFilter textureFilter = TextureFilter::Linear,
        TextureFormat textureFormat = TextureFormat::Normal);

    /**
     * Allocates a render target, depth target, or cube target with no
     * initial pixel data.
     *
     * The right entry point for any non-`Default` texture type. The
     * underlying GPU texture is created with the usage flags appropriate
     * for `textureType` (color target, depth-stencil target, sampleable);
     * cube types allocate six array layers in a single allocation.
     *
     * Internal sampler creation is skipped for depth and cube types --
     * `GetSampler()` returns `nullptr` for those, and the caller is
     * expected to pair the texture with a separate `Lucky::Sampler`.
     *
     * \param graphicsDevice the graphics device that owns the GPU resource.
     *                       Must outlive this Texture.
     * \param textureType the target kind. `Default` is rejected by an
     *                    assert; use a pixel-data constructor instead.
     * \param width texture width in pixels (or per-face for cube types).
     *              Must be positive.
     * \param height texture height in pixels (or per-face for cube types).
     *               Must be positive. Cube types must satisfy
     *               `width == height`.
     * \param textureFormat the storage format. Must be `Depth` for depth
     *                      types and a color format (`Normal` or `HDR`)
     *                      for color types.
     * \param textureFilter the filter mode for the internal sampler. Only
     *                      meaningful for `RenderTarget`; ignored for
     *                      depth and cube types.
     * \throws std::runtime_error on GPU allocation failure.
     */
    Texture(GraphicsDevice &graphicsDevice, TextureType textureType, uint32_t width,
        uint32_t height, TextureFormat textureFormat,
        TextureFilter textureFilter = TextureFilter::Linear);

    Texture(const Texture &) = delete;
    ~Texture();

    Texture &operator=(const Texture &) = delete;
    Texture &operator=(Texture &&) = delete;

    /**
     * Replaces a sub-rectangle of the texture with new pixel data.
     *
     * The data must be tightly packed and sized to exactly
     * `w * h * BytesPerPixel(GetTextureFormat())`. The region must lie
     * entirely within the texture (`x + w <= GetWidth()` and
     * `y + h <= GetHeight()`). Violations are programmer errors and trip an
     * assert in debug builds.
     *
     * The upload submits its own command buffer; the change is visible to
     * subsequent frames but is not ordered relative to a render pass that is
     * currently open on the GraphicsDevice.
     *
     * \param x the left edge of the destination region, in pixels.
     * \param y the top edge of the destination region, in pixels.
     * \param w the region width in pixels.
     * \param h the region height in pixels.
     * \param pixelData pointer to the source pixel data. Must not be null.
     * \param dataLength the source length in bytes; must equal
     *                   `w * h * BytesPerPixel(GetTextureFormat())`.
     * \throws std::runtime_error on GPU allocation or upload failure.
     */
    void SetTextureData(
        uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint8_t *pixelData, uint32_t dataLength);

    /**
     * Returns the current filter mode.
     */
    TextureFilter GetTextureFilter() const {
        return textureFilter;
    }

    /**
     * Replaces the sampler with one configured for a new filter mode.
     *
     * The previous sampler is released and a new one is created. The GPU
     * texture itself is unchanged.
     *
     * \param textureFilter the new filter mode.
     * \throws std::runtime_error if sampler creation fails.
     */
    void SetTextureFilter(TextureFilter textureFilter);

    /**
     * Returns whether the texture was created as Default or RenderTarget.
     */
    TextureType GetTextureType() const {
        return textureType;
    }

    /**
     * Returns the GPU pixel format the texture was created with.
     */
    TextureFormat GetTextureFormat() const {
        return textureFormat;
    }

    /**
     * Returns the texture width in pixels.
     */
    uint32_t GetWidth() const {
        return width;
    }

    /**
     * Returns the texture height in pixels.
     */
    uint32_t GetHeight() const {
        return height;
    }

    /**
     * Returns the underlying `SDL_GPUTexture` handle.
     *
     * Exposed so callers can bind the texture directly to SDL_GPU APIs (for
     * example as a color target in a render pass). The handle is owned by
     * this Texture and must not be released by the caller.
     */
    SDL_GPUTexture *GetGPUTexture() const {
        return gpuTexture;
    }

    /**
     * Returns the underlying `SDL_GPUSampler` handle, or `nullptr` for
     * depth and cube textures (which do not allocate an internal sampler).
     *
     * Exposed so callers can bind the sampler alongside the texture in a
     * shader resource set. For depth and cube targets, pair the texture
     * with a separate `Lucky::Sampler` instead. The handle is owned by
     * this Texture and is replaced whenever `SetTextureFilter()` is called;
     * do not hold it past that call.
     */
    SDL_GPUSampler *GetSampler() const {
        return sampler;
    }

  private:
    void Initialize(TextureType textureType, uint32_t width, uint32_t height, uint8_t *pixelData,
        uint32_t dataLength, TextureFilter textureFilter, TextureFormat textureFormat);

    void CreateSampler(TextureFilter filter);
    void UploadRegion(
        uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint8_t *pixelData, uint32_t dataLength);

    GraphicsDevice &graphicsDevice;
    TextureFilter textureFilter = TextureFilter::Linear;
    TextureType textureType = TextureType::Default;
    TextureFormat textureFormat = TextureFormat::Normal;
    uint32_t width = 0;
    uint32_t height = 0;
    SDL_GPUTexture *gpuTexture = nullptr;
    SDL_GPUSampler *sampler = nullptr;
};

} // namespace Lucky
