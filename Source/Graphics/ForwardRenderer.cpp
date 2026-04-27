#include <algorithm>
#include <filesystem>

#include <SDL3/SDL.h>
#include <SDL3/SDL_assert.h>
#include <SDL3/SDL_filesystem.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <spdlog/spdlog.h>

#include <Lucky/Camera.hpp>
#include <Lucky/ForwardRenderer.hpp>
#include <Lucky/GraphicsDevice.hpp>
#include <Lucky/Material.hpp>
#include <Lucky/Mesh.hpp>
#include <Lucky/Sampler.hpp>
#include <Lucky/Scene3D.hpp>
#include <Lucky/Shader.hpp>
#include <Lucky/Texture.hpp>

namespace Lucky {

namespace {

constexpr int MaxLights = 8;

// Per-draw vertex UBO at slot 1, shared between the forward and shadow
// pipelines so the per-object push is identical for both passes.
struct ObjectUBO {
    glm::mat4 model;
    glm::vec4 colorTint;
};

// Per-draw fragment UBO at slot 1 (b1, space3). Layout mirrors the HLSL
// cbuffer in forward.frag.hlsl exactly; do not reorder.
struct MaterialUBO {
    glm::vec4 baseColorFactor;
    glm::vec3 emissiveFactor;
    float metallicFactor;
    float roughnessFactor;
    int hasBaseColorTexture;
    int hasMetallicRoughnessTexture;
    int hasEmissiveTexture;
};
static_assert(sizeof(MaterialUBO) == 48, "MaterialUBO must match HLSL cbuffer layout");

// Per-light entry in the fragment LightingUBO. Layout mirrors the HLSL
// Light struct in forward.frag.hlsl exactly; do not reorder fields.
struct LightUBOEntry {
    glm::vec3 position;
    float range;
    glm::vec3 color;
    float intensity;
    glm::vec3 direction;
    int type; // 0=Directional, 1=Point, 2=Spot
    float innerCone;
    float outerCone;
    int shadowIndex; // -1 = no shadow, 0..3 = index into ShadowVP / shadow maps
    float pad;
};
static_assert(sizeof(LightUBOEntry) == 64, "Light entry must match HLSL stride");

struct LightingUBO {
    glm::vec3 ambientColor;
    int lightCount;
    glm::vec3 cameraPosition;
    float pad;
    LightUBOEntry lights[MaxLights];
    glm::mat4 shadowVP[ForwardRenderer::MaxShadowMaps];
};
static_assert(sizeof(LightingUBO) == 32 + MaxLights * 64 + ForwardRenderer::MaxShadowMaps * 64,
    "LightingUBO must match HLSL cbuffer layout");

int LightTypeToShader(LightType type) {
    switch (type) {
    case LightType::Directional:
        return 0;
    case LightType::Point:
        return 1;
    case LightType::Spot:
        return 2;
    }
    return 0;
}

// Build a light view-projection matrix appropriate for the light type.
// Directional lights get an orthographic frustum centered on the origin;
// spot lights get a perspective from the light's position with FOV
// derived from its outer cone angle.
glm::mat4 BuildShadowViewProj(const Light &light) {
    if (light.type == LightType::Directional) {
        // Fixed-size ortho centered on origin. Simple but enough for the
        // demo; a proper implementation would size this to the camera
        // frustum to maximize shadow texture utilization.
        const glm::vec3 dir = glm::normalize(light.direction);
        const glm::vec3 target(0.0f, 0.0f, 0.0f);
        const float distance = 30.0f;
        const glm::vec3 eye = target - dir * distance;
        glm::vec3 up(0.0f, 1.0f, 0.0f);
        if (std::abs(glm::dot(dir, up)) > 0.99f) {
            up = glm::vec3(0.0f, 0.0f, 1.0f);
        }
        const glm::mat4 view = glm::lookAt(eye, target, up);
        // Fixed-size frustum sized to cover the typical demo scene.
        // A real scene wants this fitted to the camera frustum each
        // frame (cascaded shadow maps); the trade-off here is shadow
        // texel density: ShadowMapSize / (2*halfExtent) world units
        // per texel (currently 2048 / 24 ≈ 12mm/texel).
        const float halfExtent = 12.0f;
        const glm::mat4 proj = glm::orthoRH_ZO(
            -halfExtent, halfExtent, -halfExtent, halfExtent, 0.1f, distance * 2.0f);
        return proj * view;
    }

    // Spot
    const glm::vec3 dir = glm::normalize(light.direction);
    glm::vec3 up(0.0f, 1.0f, 0.0f);
    if (std::abs(glm::dot(dir, up)) > 0.99f) {
        up = glm::vec3(0.0f, 0.0f, 1.0f);
    }
    const float outerAngle = std::acos(glm::clamp(light.outerCone, -1.0f, 1.0f));
    const float fov = 2.0f * outerAngle;
    const float range = (light.range > 0.0f) ? light.range : 25.0f;
    const float nearPlane = std::max(0.5f, range * 0.05f);
    const glm::mat4 view = glm::lookAt(light.position, light.position + dir, up);
    const glm::mat4 proj = glm::perspectiveRH_ZO(fov, 1.0f, nearPlane, range);
    return proj * view;
}

void DrawSceneGeometry(SDL_GPURenderPass *pass, SDL_GPUCommandBuffer *cmd, const Scene3D &scene) {
    for (const SceneObject &object : scene.objects) {
        if (!object.mesh) {
            continue;
        }

        ObjectUBO ubo;
        ubo.model = object.transform;
        ubo.colorTint = glm::vec4(object.color, 1.0f);
        SDL_PushGPUVertexUniformData(cmd, 1, &ubo, sizeof(ubo));

        SDL_GPUBufferBinding vbufBinding;
        SDL_zero(vbufBinding);
        vbufBinding.buffer = object.mesh->GetVertexBuffer();
        SDL_BindGPUVertexBuffers(pass, 0, &vbufBinding, 1);

        SDL_GPUBufferBinding ibufBinding;
        SDL_zero(ibufBinding);
        ibufBinding.buffer = object.mesh->GetIndexBuffer();
        SDL_BindGPUIndexBuffer(pass, &ibufBinding, SDL_GPU_INDEXELEMENTSIZE_32BIT);

        SDL_DrawGPUIndexedPrimitives(pass, object.mesh->GetIndexCount(), 1, 0, 0, 0);
    }
}

} // namespace

ForwardRenderer::ForwardRenderer(GraphicsDevice &graphicsDevice) : graphicsDevice(&graphicsDevice) {
    std::filesystem::path basePath = SDL_GetBasePath();

    forwardVertexShader = std::make_unique<Shader>(graphicsDevice,
        (basePath / "Content/Shaders/forward.vert").generic_string(),
        SDL_GPU_SHADERSTAGE_VERTEX);
    forwardFragmentShader = std::make_unique<Shader>(graphicsDevice,
        (basePath / "Content/Shaders/forward.frag").generic_string(),
        SDL_GPU_SHADERSTAGE_FRAGMENT);
    shadowVertexShader = std::make_unique<Shader>(graphicsDevice,
        (basePath / "Content/Shaders/shadow_depth.vert").generic_string(),
        SDL_GPU_SHADERSTAGE_VERTEX);
    shadowFragmentShader = std::make_unique<Shader>(graphicsDevice,
        (basePath / "Content/Shaders/shadow_depth.frag").generic_string(),
        SDL_GPU_SHADERSTAGE_FRAGMENT);

    for (int i = 0; i < MaxShadowMaps; i++) {
        shadowMaps[i] = std::make_unique<Texture>(graphicsDevice,
            TextureType::DepthTarget,
            ShadowMapSize,
            ShadowMapSize,
            TextureFormat::Depth);
    }

    SamplerDescription shadowSampDesc;
    shadowSampDesc.filter = SamplerFilter::Point;
    shadowSampDesc.mipmapFilter = SamplerFilter::Point;
    shadowSampDesc.addressU = SamplerAddressMode::ClampToEdge;
    shadowSampDesc.addressV = SamplerAddressMode::ClampToEdge;
    shadowSampDesc.addressW = SamplerAddressMode::ClampToEdge;
    shadowSampler = std::make_unique<Sampler>(graphicsDevice, shadowSampDesc);

    // 1x1 white fallback, bound whenever a material doesn't supply
    // its own base-color, metallic-roughness, or emissive texture.
    // The texture-presence flags in the MaterialUBO tell the shader
    // to ignore the sampled value in those cases, so the content
    // only matters as a sentinel.
    uint8_t whitePixel[4] = {255, 255, 255, 255};
    whiteTexture = std::make_unique<Texture>(graphicsDevice,
        TextureType::Default,
        1u,
        1u,
        whitePixel,
        static_cast<uint32_t>(sizeof(whitePixel)));

    SamplerDescription matSampDesc; // linear/repeat default for materials.
    defaultMaterialSampler = std::make_unique<Sampler>(graphicsDevice, matSampDesc);
}

ForwardRenderer::~ForwardRenderer() {
    SDL_GPUDevice *device = graphicsDevice->GetDevice();
    for (auto &[key, pipeline] : forwardPipelines) {
        SDL_ReleaseGPUGraphicsPipeline(device, pipeline);
    }
    if (shadowPipeline) {
        SDL_ReleaseGPUGraphicsPipeline(device, shadowPipeline);
    }
}

void ForwardRenderer::Render(const Scene3D &scene, const Camera &camera) {
    SDL_assert(graphicsDevice->GetCommandBuffer() != nullptr);
    SDL_assert(graphicsDevice->GetCurrentRenderPass() == nullptr);
    SDL_assert(graphicsDevice->IsDepthEnabled() || graphicsDevice->IsUsingDepthTarget());

    const SDL_GPUTextureFormat colorFormat = graphicsDevice->IsUsingRenderTarget()
                                                 ? SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM
                                                 : graphicsDevice->GetSwapchainFormat();
    const SDL_GPUTextureFormat depthFormat = graphicsDevice->GetDepthFormat();

    SDL_GPUGraphicsPipeline *forwardPipeline = GetOrCreateForwardPipeline(colorFormat, depthFormat);
    SDL_GPUGraphicsPipeline *shadowPipe = GetOrCreateShadowPipeline(depthFormat);
    if (!forwardPipeline || !shadowPipe) {
        return;
    }

    SDL_GPUCommandBuffer *cmd = graphicsDevice->GetCommandBuffer();

    // Pre-pack the lighting UBO so we can fill in shadow VPs as we render
    // each shadow map below. Light-to-shadow-slot assignment follows
    // scene.lights order: first MaxShadowMaps eligible casters get slots
    // 0..3; everything else is non-shadow-casting from the shader's POV.
    LightingUBO lightingUbo{};
    lightingUbo.ambientColor = scene.ambientColor;
    lightingUbo.lightCount = static_cast<int>(std::min<size_t>(scene.lights.size(), MaxLights));
    lightingUbo.cameraPosition = camera.position;
    lightingUbo.pad = 0.0f;
    int shadowCount = 0;

    for (int i = 0; i < lightingUbo.lightCount; i++) {
        const Light &src = scene.lights[i];
        LightUBOEntry &dst = lightingUbo.lights[i];
        dst.position = src.position;
        dst.range = src.range;
        dst.color = src.color;
        dst.intensity = src.intensity;
        dst.direction = glm::normalize(src.direction);
        dst.type = LightTypeToShader(src.type);
        dst.innerCone = src.innerCone;
        dst.outerCone = src.outerCone;
        dst.shadowIndex = -1;
        dst.pad = 0.0f;

        const bool eligible = src.castsShadows && shadowCount < MaxShadowMaps &&
                              (src.type == LightType::Directional || src.type == LightType::Spot);
        if (eligible) {
            const glm::mat4 lightVP = BuildShadowViewProj(src);
            lightingUbo.shadowVP[shadowCount] = lightVP;
            dst.shadowIndex = shadowCount;

            // Render scene depth from the light's POV into shadowMaps[shadowCount].
            graphicsDevice->BindDepthRenderTarget(*shadowMaps[shadowCount]);
            graphicsDevice->BeginRenderPass();
            SDL_GPURenderPass *shadowPass = graphicsDevice->GetCurrentRenderPass();
            if (shadowPass) {
                SDL_BindGPUGraphicsPipeline(shadowPass, shadowPipe);
                SDL_PushGPUVertexUniformData(cmd, 0, &lightVP, sizeof(lightVP));
                DrawSceneGeometry(shadowPass, cmd, scene);
            }
            graphicsDevice->EndRenderPass();

            shadowCount++;
        }
    }

    // Shadow passes finished; switch back to the swapchain target for
    // the main forward pass.
    graphicsDevice->UnbindDepthRenderTarget();

    graphicsDevice->BeginRenderPass();
    SDL_GPURenderPass *renderPass = graphicsDevice->GetCurrentRenderPass();
    if (!renderPass) {
        return;
    }

    SDL_BindGPUGraphicsPipeline(renderPass, forwardPipeline);

    const float aspect = static_cast<float>(graphicsDevice->GetScreenWidth()) /
                         static_cast<float>(graphicsDevice->GetScreenHeight());
    const glm::mat4 viewProj = camera.GetProjectionMatrix(aspect) * camera.GetViewMatrix();

    SDL_PushGPUVertexUniformData(cmd, 0, &viewProj, sizeof(viewProj));
    SDL_PushGPUFragmentUniformData(cmd, 0, &lightingUbo, sizeof(lightingUbo));

    // Per-object: bind material textures (base color, metallic-
    // roughness, emissive) plus the four shadow maps, push the
    // material UBO, push the per-object vertex UBO, then draw. We
    // can't share with the shadow path's DrawSceneGeometry helper
    // because shadow passes don't bind materials.
    Material defaultMaterial;
    defaultMaterial.sampler = defaultMaterialSampler.get();

    SDL_GPUSampler *defaultSampler = defaultMaterialSampler->GetSampler();
    SDL_GPUSampler *shadowSamp = shadowSampler->GetSampler();
    SDL_GPUTexture *whiteGpuTex = whiteTexture->GetGPUTexture();

    for (const SceneObject &object : scene.objects) {
        if (!object.mesh) {
            continue;
        }

        const Material *mat = object.material ? object.material : &defaultMaterial;

        MaterialUBO matUbo;
        matUbo.baseColorFactor = mat->baseColorFactor;
        matUbo.emissiveFactor = mat->emissiveFactor;
        matUbo.metallicFactor = mat->metallicFactor;
        matUbo.roughnessFactor = mat->roughnessFactor;
        matUbo.hasBaseColorTexture = mat->baseColorTexture ? 1 : 0;
        matUbo.hasMetallicRoughnessTexture = mat->metallicRoughnessTexture ? 1 : 0;
        matUbo.hasEmissiveTexture = mat->emissiveTexture ? 1 : 0;
        SDL_PushGPUFragmentUniformData(cmd, 1, &matUbo, sizeof(matUbo));

        SDL_GPUSampler *matSampler = (mat->sampler ? mat->sampler->GetSampler() : defaultSampler);

        // Bind all seven fragment texture+sampler pairs in one call.
        // Splitting these into separate range binds (e.g. material per
        // object + shadows per frame) doesn't actually rebind on D3D12;
        // each draw needs the complete set together. Slots 0..1 are
        // material textures (base color, metallic-roughness); 2..5 are
        // the four shadow maps; 6 is the emissive texture.
        SDL_GPUTextureSamplerBinding bindings[3 + MaxShadowMaps];
        SDL_zero(bindings[0]);
        bindings[0].texture =
            mat->baseColorTexture ? mat->baseColorTexture->GetGPUTexture() : whiteGpuTex;
        bindings[0].sampler = matSampler;
        SDL_zero(bindings[1]);
        bindings[1].texture = mat->metallicRoughnessTexture
                                  ? mat->metallicRoughnessTexture->GetGPUTexture()
                                  : whiteGpuTex;
        bindings[1].sampler = matSampler;
        for (int i = 0; i < MaxShadowMaps; i++) {
            SDL_zero(bindings[2 + i]);
            bindings[2 + i].texture = shadowMaps[i]->GetGPUTexture();
            bindings[2 + i].sampler = shadowSamp;
        }
        SDL_zero(bindings[2 + MaxShadowMaps]);
        bindings[2 + MaxShadowMaps].texture =
            mat->emissiveTexture ? mat->emissiveTexture->GetGPUTexture() : whiteGpuTex;
        bindings[2 + MaxShadowMaps].sampler = matSampler;
        SDL_BindGPUFragmentSamplers(renderPass, 0, bindings, 3 + MaxShadowMaps);

        ObjectUBO ubo;
        ubo.model = object.transform;
        ubo.colorTint = glm::vec4(object.color, 1.0f);
        SDL_PushGPUVertexUniformData(cmd, 1, &ubo, sizeof(ubo));

        SDL_GPUBufferBinding vbufBinding;
        SDL_zero(vbufBinding);
        vbufBinding.buffer = object.mesh->GetVertexBuffer();
        SDL_BindGPUVertexBuffers(renderPass, 0, &vbufBinding, 1);

        SDL_GPUBufferBinding ibufBinding;
        SDL_zero(ibufBinding);
        ibufBinding.buffer = object.mesh->GetIndexBuffer();
        SDL_BindGPUIndexBuffer(renderPass, &ibufBinding, SDL_GPU_INDEXELEMENTSIZE_32BIT);

        SDL_DrawGPUIndexedPrimitives(renderPass, object.mesh->GetIndexCount(), 1, 0, 0, 0);
    }

    graphicsDevice->EndRenderPass();
}

SDL_GPUGraphicsPipeline *ForwardRenderer::GetOrCreateForwardPipeline(
    SDL_GPUTextureFormat colorFormat, SDL_GPUTextureFormat depthFormat) {
    ForwardPipelineKey key{colorFormat, depthFormat};
    if (auto it = forwardPipelines.find(key); it != forwardPipelines.end()) {
        return it->second;
    }

    SDL_GPUDevice *device = graphicsDevice->GetDevice();

    SDL_GPUVertexBufferDescription vbufDesc;
    SDL_zero(vbufDesc);
    vbufDesc.slot = 0;
    vbufDesc.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
    vbufDesc.pitch = sizeof(Vertex3D);

    SDL_GPUVertexAttribute attrs[3];
    SDL_zero(attrs);
    attrs[0].location = 0;
    attrs[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
    attrs[0].offset = offsetof(Vertex3D, x);
    attrs[1].location = 1;
    attrs[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
    attrs[1].offset = offsetof(Vertex3D, u);
    attrs[2].location = 2;
    attrs[2].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
    attrs[2].offset = offsetof(Vertex3D, nx);

    SDL_GPUColorTargetDescription colorTarget;
    SDL_zero(colorTarget);
    colorTarget.format = colorFormat;

    SDL_GPUGraphicsPipelineCreateInfo ci;
    SDL_zero(ci);
    ci.vertex_shader = forwardVertexShader->GetHandle();
    ci.fragment_shader = forwardFragmentShader->GetHandle();
    ci.vertex_input_state.num_vertex_buffers = 1;
    ci.vertex_input_state.vertex_buffer_descriptions = &vbufDesc;
    ci.vertex_input_state.num_vertex_attributes = 3;
    ci.vertex_input_state.vertex_attributes = attrs;
    ci.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    ci.depth_stencil_state.enable_depth_test = true;
    ci.depth_stencil_state.enable_depth_write = true;
    ci.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS_OR_EQUAL;
    ci.target_info.num_color_targets = 1;
    ci.target_info.color_target_descriptions = &colorTarget;
    ci.target_info.has_depth_stencil_target = true;
    ci.target_info.depth_stencil_format = depthFormat;

    SDL_GPUGraphicsPipeline *pipeline = SDL_CreateGPUGraphicsPipeline(device, &ci);
    if (!pipeline) {
        spdlog::error("Failed to create forward pipeline: {}", SDL_GetError());
        return nullptr;
    }

    forwardPipelines[key] = pipeline;
    return pipeline;
}

SDL_GPUGraphicsPipeline *ForwardRenderer::GetOrCreateShadowPipeline(
    SDL_GPUTextureFormat depthFormat) {
    if (shadowPipeline && shadowPipelineDepthFormat == depthFormat) {
        return shadowPipeline;
    }
    if (shadowPipeline) {
        SDL_ReleaseGPUGraphicsPipeline(graphicsDevice->GetDevice(), shadowPipeline);
        shadowPipeline = nullptr;
    }

    SDL_GPUVertexBufferDescription vbufDesc;
    SDL_zero(vbufDesc);
    vbufDesc.slot = 0;
    vbufDesc.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
    vbufDesc.pitch = sizeof(Vertex3D);

    SDL_GPUVertexAttribute attrs[3];
    SDL_zero(attrs);
    attrs[0].location = 0;
    attrs[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
    attrs[0].offset = offsetof(Vertex3D, x);
    attrs[1].location = 1;
    attrs[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
    attrs[1].offset = offsetof(Vertex3D, u);
    attrs[2].location = 2;
    attrs[2].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
    attrs[2].offset = offsetof(Vertex3D, nx);

    SDL_GPUGraphicsPipelineCreateInfo ci;
    SDL_zero(ci);
    ci.vertex_shader = shadowVertexShader->GetHandle();
    ci.fragment_shader = shadowFragmentShader->GetHandle();
    ci.vertex_input_state.num_vertex_buffers = 1;
    ci.vertex_input_state.vertex_buffer_descriptions = &vbufDesc;
    ci.vertex_input_state.num_vertex_attributes = 3;
    ci.vertex_input_state.vertex_attributes = attrs;
    ci.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;

    // Depth bias keeps shadow acne off lit surfaces. Tuned together
    // with the in-shader normal offset for a 24-unit ortho frustum on
    // a D16 shadow map; smaller frustums or higher-precision formats
    // tolerate lower values, but these bias up enough that the
    // ground plane stays clean even with the 5x5 PCF kernel
    // averaging individual acne dots into rings.
    ci.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
    ci.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
    ci.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
    ci.rasterizer_state.enable_depth_bias = true;
    ci.rasterizer_state.depth_bias_constant_factor = 4.0f;
    ci.rasterizer_state.depth_bias_slope_factor = 4.0f;

    ci.depth_stencil_state.enable_depth_test = true;
    ci.depth_stencil_state.enable_depth_write = true;
    ci.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS_OR_EQUAL;

    ci.target_info.num_color_targets = 0;
    ci.target_info.has_depth_stencil_target = true;
    ci.target_info.depth_stencil_format = depthFormat;

    SDL_GPUGraphicsPipeline *pipeline =
        SDL_CreateGPUGraphicsPipeline(graphicsDevice->GetDevice(), &ci);
    if (!pipeline) {
        spdlog::error("Failed to create shadow pipeline: {}", SDL_GetError());
        return nullptr;
    }

    shadowPipeline = pipeline;
    shadowPipelineDepthFormat = depthFormat;
    return shadowPipeline;
}

} // namespace Lucky
