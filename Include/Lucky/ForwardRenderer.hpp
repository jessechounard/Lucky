#pragma once

#include <memory>
#include <unordered_map>

#include <SDL3/SDL_gpu.h>

namespace Lucky {

struct Camera;
struct GraphicsDevice;
struct Sampler;
struct Scene3D;
struct Shader;
struct Texture;

/**
 * Renders a `Scene3D` of opaque meshes from a `Camera` using a forward
 * lighting pass with optional shadow mapping.
 *
 * # Pipeline
 *
 * Each frame:
 *   1. For each shadow-casting `Directional` or `Spot` light (up to 4),
 *      render scene depth into the light's 1024x1024 shadow texture.
 *   2. Render the scene into the bound color/depth target with full
 *      lighting from `Scene3D::lights`, sampling the shadow maps to
 *      attenuate diffuse contribution.
 *
 * Point-light shadows are not implemented yet; setting
 * `Light::castsShadows` on a point light is silently ignored.
 *
 * # Usage
 *
 * Construct one ForwardRenderer per `GraphicsDevice` and call
 * `Render(scene, camera)` once per frame between `BeginFrame` and
 * `EndFrame`. The renderer owns its render passes; the caller must not
 * have a render pass open when calling `Render`. Depth must be enabled
 * on the swapchain (`graphicsDevice.SetDepthEnabled(true)`).
 *
 * # Lifetime
 *
 * Holds a pointer to the `GraphicsDevice` and owns a handful of GPU
 * resources (shaders, pipelines, shadow maps, sampler). The
 * `GraphicsDevice` must outlive this renderer.
 */
struct ForwardRenderer {
    /**
     * Loads the forward + shadow-depth shaders and allocates shadow
     * resources.
     *
     * Shaders are loaded from `Content/Shaders/forward.vert/.frag` and
     * `Content/Shaders/shadow_depth.vert/.frag` next to the executable.
     * Throws `std::runtime_error` on shader load failure.
     */
    explicit ForwardRenderer(GraphicsDevice &graphicsDevice);

    ForwardRenderer(const ForwardRenderer &) = delete;
    ForwardRenderer &operator=(const ForwardRenderer &) = delete;
    ForwardRenderer(ForwardRenderer &&) = delete;
    ForwardRenderer &operator=(ForwardRenderer &&) = delete;

    ~ForwardRenderer();

    /**
     * Renders all `scene.objects` from `camera`, populating shadow maps
     * for shadow-casting directional and spot lights first.
     *
     * Skips objects whose `mesh` is null. Asserts that the device has
     * depth enabled and that no render pass is currently active.
     */
    void Render(const Scene3D &scene, const Camera &camera);

    /** Maximum number of 2D shadow maps the renderer maintains. */
    static constexpr int MaxShadowMaps = 4;

    /** Edge length of each shadow map in texels. */
    static constexpr uint32_t ShadowMapSize = 1024;

  private:
    SDL_GPUGraphicsPipeline *GetOrCreateForwardPipeline(
        SDL_GPUTextureFormat colorFormat, SDL_GPUTextureFormat depthFormat);

    SDL_GPUGraphicsPipeline *GetOrCreateShadowPipeline(SDL_GPUTextureFormat depthFormat);

    struct ForwardPipelineKey {
        SDL_GPUTextureFormat colorFormat;
        SDL_GPUTextureFormat depthFormat;

        bool operator==(const ForwardPipelineKey &other) const {
            return colorFormat == other.colorFormat && depthFormat == other.depthFormat;
        }
    };
    struct ForwardPipelineKeyHash {
        size_t operator()(const ForwardPipelineKey &k) const {
            return (static_cast<size_t>(k.colorFormat) << 16) ^ static_cast<size_t>(k.depthFormat);
        }
    };

    GraphicsDevice *graphicsDevice;

    std::unique_ptr<Shader> forwardVertexShader;
    std::unique_ptr<Shader> forwardFragmentShader;
    std::unique_ptr<Shader> shadowVertexShader;
    std::unique_ptr<Shader> shadowFragmentShader;

    std::unordered_map<ForwardPipelineKey, SDL_GPUGraphicsPipeline *, ForwardPipelineKeyHash>
        forwardPipelines;
    SDL_GPUGraphicsPipeline *shadowPipeline = nullptr;
    SDL_GPUTextureFormat shadowPipelineDepthFormat = SDL_GPU_TEXTUREFORMAT_INVALID;

    std::unique_ptr<Texture> shadowMaps[MaxShadowMaps];
    std::unique_ptr<Sampler> shadowSampler;
};

} // namespace Lucky
