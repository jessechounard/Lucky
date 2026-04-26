#pragma once

#include <SDL3/SDL_gpu.h>

namespace Lucky {

struct GraphicsDevice;

/**
 * Texel filter mode used for minification, magnification, and (when enabled)
 * mip selection.
 *
 * - `Linear` — bilinear / linear-mip blending. Smooth scaling, the default
 *   for photographic content and 3D model textures.
 * - `Point` — nearest-neighbor. Preserves hard texel edges; the typical
 *   pick for pixel-art sampling and for shadow-map sampling without PCF.
 */
enum class SamplerFilter {
    Linear,
    Point,
};

/**
 * How sample coordinates outside `[0, 1]` are resolved on a single axis.
 *
 * - `Repeat` — tile the texture infinitely. Standard for tileable model
 *   textures.
 * - `ClampToEdge` — clamp to the nearest edge texel. The right pick for
 *   render targets, atlases, and shadow maps where wrapping would alias
 *   neighboring content into the sample.
 * - `MirroredRepeat` — tile with every other repeat flipped, preventing
 *   visible seams across the boundary.
 */
enum class SamplerAddressMode {
    Repeat,
    ClampToEdge,
    MirroredRepeat,
};

/**
 * Knobs for constructing a `Sampler`. Defaults match the most common 3D
 * mesh sampler (linear + repeat); override what you need.
 *
 * For a shadow-map sampler, set `filter` and `mipmapFilter` to `Point` and
 * the three address modes to `ClampToEdge`.
 */
struct SamplerDescription {
    SamplerFilter filter = SamplerFilter::Linear;
    SamplerFilter mipmapFilter = SamplerFilter::Linear;
    SamplerAddressMode addressU = SamplerAddressMode::Repeat;
    SamplerAddressMode addressV = SamplerAddressMode::Repeat;
    SamplerAddressMode addressW = SamplerAddressMode::Repeat;
};

/**
 * RAII wrapper around an `SDL_GPUSampler`.
 *
 * `Texture` already pairs each image with its own internal sampler, which
 * is convenient for sprites and atlases but does not scale to 3D content
 * where a single sampler is shared across many meshes, or to specialized
 * sampling (shadow maps, render-to-texture sampling). `Sampler` is the
 * standalone form: construct one, hand its handle to renderers, share
 * freely.
 *
 * # Lifetime
 *
 * Holds a pointer to the `GraphicsDevice`. The `GraphicsDevice` must
 * outlive this sampler; destruction releases the underlying GPU sampler
 * through it.
 */
struct Sampler {
    /**
     * Creates a sampler matching `description`.
     *
     * \param graphicsDevice the graphics device. Must outlive this sampler.
     * \param description filter + address-mode knobs. Pass a default-
     *                    constructed description for the standard linear /
     *                    repeat sampler.
     * \throws std::runtime_error if the GPU sampler cannot be created.
     */
    Sampler(GraphicsDevice &graphicsDevice, const SamplerDescription &description = {});

    Sampler(const Sampler &) = delete;
    Sampler &operator=(const Sampler &) = delete;
    Sampler(Sampler &&) = delete;
    Sampler &operator=(Sampler &&) = delete;

    ~Sampler();

    /**
     * Returns the underlying `SDL_GPUSampler` handle for binding alongside
     * a texture in a shader resource set.
     */
    SDL_GPUSampler *GetSampler() const {
        return sampler;
    }

  private:
    GraphicsDevice &graphicsDevice;
    SDL_GPUSampler *sampler = nullptr;
};

} // namespace Lucky
