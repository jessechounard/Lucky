#pragma once

#include <memory>
#include <unordered_map>

#include <SDL3/SDL_gpu.h>

namespace Lucky {

struct Camera;
struct GraphicsDevice;
struct Material;
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
 *      render scene depth into the light's 2D shadow texture.
 *   2. For each shadow-casting `Point` light (up to 2), render scene
 *      depth into all six faces of the light's cube shadow texture
 *      (six passes per cube).
 *   3. Render the scene into the bound color/depth target with full
 *      lighting from `Scene3D::lights`, sampling the appropriate shadow
 *      map (2D or cube) per light to attenuate the BRDF contribution.
 *
 * Per-frame slot caps are first-come-first-served by `scene.lights`
 * order; lights past the cap are still lit but cast no shadow.
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

    /** Edge length of each 2D shadow map in texels. */
    static constexpr uint32_t ShadowMapSize = 2048;

    /**
     * Maximum number of cube shadow maps the renderer maintains. Each
     * cube costs six render passes per frame, so this cap is intentionally
     * lower than MaxShadowMaps -- a typical scene might place a single
     * point caster (a torch, a magic effect) and accept that everything
     * else is unshadowed.
     */
    static constexpr int MaxPointShadows = 2;

    /**
     * Edge length of each cube face in texels. Smaller than ShadowMapSize
     * because cube shadows render six times per frame and the player is
     * usually closer to the point caster than to the sun, so per-texel
     * world coverage is naturally tighter.
     */
    static constexpr uint32_t PointShadowMapSize = 1024;

    /**
     * Maximum number of joints per skinned mesh.
     *
     * Sized to fit comfortably inside the 16 KB SDL_PushGPU* uniform
     * cap (128 * 64 = 8 KB). Asset note: typical fully-rigged humans
     * use 50-80 joints; the Khronos CesiumMan sample uses 19. A model
     * exceeding this cap will trigger a runtime assert from the
     * skinned draw path.
     */
    static constexpr int MaxJoints = 128;

  private:
    SDL_GPUGraphicsPipeline *GetOrCreateForwardPipeline(
        SDL_GPUTextureFormat colorFormat, SDL_GPUTextureFormat depthFormat);

    SDL_GPUGraphicsPipeline *GetOrCreateShadowPipeline(SDL_GPUTextureFormat depthFormat);

    SDL_GPUGraphicsPipeline *GetOrCreateForwardSkinnedPipeline(
        SDL_GPUTextureFormat colorFormat, SDL_GPUTextureFormat depthFormat);

    SDL_GPUGraphicsPipeline *GetOrCreateShadowSkinnedPipeline(SDL_GPUTextureFormat depthFormat);

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
    std::unique_ptr<Shader> forwardSkinnedVertexShader;
    std::unique_ptr<Shader> shadowVertexShader;
    std::unique_ptr<Shader> shadowFragmentShader;
    std::unique_ptr<Shader> shadowSkinnedVertexShader;

    std::unordered_map<ForwardPipelineKey, SDL_GPUGraphicsPipeline *, ForwardPipelineKeyHash>
        forwardPipelines;
    std::unordered_map<ForwardPipelineKey, SDL_GPUGraphicsPipeline *, ForwardPipelineKeyHash>
        forwardSkinnedPipelines;
    SDL_GPUGraphicsPipeline *shadowPipeline = nullptr;
    SDL_GPUTextureFormat shadowPipelineDepthFormat = SDL_GPU_TEXTUREFORMAT_INVALID;
    SDL_GPUGraphicsPipeline *shadowSkinnedPipeline = nullptr;
    SDL_GPUTextureFormat shadowSkinnedPipelineDepthFormat = SDL_GPU_TEXTUREFORMAT_INVALID;

    std::unique_ptr<Texture> shadowMaps[MaxShadowMaps];
    std::unique_ptr<Texture> pointShadowMaps[MaxPointShadows];
    std::unique_ptr<Sampler> shadowSampler;

    // 1x1 white texture used as the base-color fallback when an object's
    // material has no base-color texture (or no material at all). Same
    // for the metallic-roughness slot so the shader always has a valid
    // sample even when the texture-presence flag tells it to ignore the
    // sample's value.
    std::unique_ptr<Texture> whiteTexture;
    std::unique_ptr<Sampler> defaultMaterialSampler;
};

} // namespace Lucky
