#include <Lucky/BlendState.hpp>
#include <Lucky/GraphicsDevice.hpp>
#include <Lucky/SlugFont.hpp>
#include <Lucky/SlugRenderer.hpp>

#include <SDL3/SDL.h>
#include <SDL3/SDL_assert.h>
#include <spdlog/spdlog.h>

#include <cstring>
#include <filesystem>
#include <stdexcept>

namespace Lucky {

SlugRenderer::SlugRenderer(GraphicsDevice &graphicsDevice, uint32_t maxTriangles)
    : graphicsDevice(&graphicsDevice), maxVertices(maxTriangles * 3) {

    SDL_assert(maxTriangles >= 2);

    std::filesystem::path basePath = SDL_GetBasePath();

    vertexShader = std::make_unique<Shader>(graphicsDevice,
        (basePath / "Content/Shaders/slug.vert").generic_string(),
        SDL_GPU_SHADERSTAGE_VERTEX);
    fragmentShader = std::make_unique<Shader>(graphicsDevice,
        (basePath / "Content/Shaders/slug.frag").generic_string(),
        SDL_GPU_SHADERSTAGE_FRAGMENT);

    vertices.resize(maxVertices);

    vertexBuffer = std::make_unique<VertexBuffer<SlugVertex>>(graphicsDevice, maxVertices);
}

SlugRenderer::~SlugRenderer() {
    SDL_GPUDevice *device = graphicsDevice->GetDevice();
    if (pipeline)
        SDL_ReleaseGPUGraphicsPipeline(device, pipeline);
}

void SlugRenderer::CreatePipeline(BlendMode blendMode) {
    SDL_GPUDevice *device = graphicsDevice->GetDevice();

    if (pipeline) {
        SDL_ReleaseGPUGraphicsPipeline(device, pipeline);
        pipeline = nullptr;
    }

    SDL_GPUGraphicsPipelineCreateInfo pipelineCI;
    SDL_zero(pipelineCI);

    // SlugVertex: pos(2) + norm(2) + emUV(2) + jac(4) + glyphLoc(1uint) + color(4) + pad(3) = 64
    // bytes
    SDL_GPUVertexBufferDescription vbufDesc;
    SDL_zero(vbufDesc);
    vbufDesc.slot = 0;
    vbufDesc.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
    vbufDesc.pitch = sizeof(SlugVertex);

    SDL_GPUVertexAttribute attrs[6];
    SDL_zero(attrs);

    // TEXCOORD0: position + normal (float4)
    attrs[0].buffer_slot = 0;
    attrs[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
    attrs[0].location = 0;
    attrs[0].offset = offsetof(SlugVertex, posX);

    // TEXCOORD1: emUV + jac row0 (float4)
    attrs[1].buffer_slot = 0;
    attrs[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
    attrs[1].location = 1;
    attrs[1].offset = offsetof(SlugVertex, emU);

    // TEXCOORD2: jac row1 + glyphLoc (float2 + uint + pad -> float4)
    attrs[2].buffer_slot = 0;
    attrs[2].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
    attrs[2].location = 2;
    attrs[2].offset = offsetof(SlugVertex, invJac10);

    // TEXCOORD3: color (float4)
    attrs[3].buffer_slot = 0;
    attrs[3].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
    attrs[3].location = 3;
    attrs[3].offset = offsetof(SlugVertex, r);

    pipelineCI.vertex_input_state.num_vertex_buffers = 1;
    pipelineCI.vertex_input_state.vertex_buffer_descriptions = &vbufDesc;
    pipelineCI.vertex_input_state.num_vertex_attributes = 4;
    pipelineCI.vertex_input_state.vertex_attributes = attrs;

    pipelineCI.vertex_shader = vertexShader->GetHandle();
    pipelineCI.fragment_shader = fragmentShader->GetHandle();
    pipelineCI.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;

    bool useDepth = graphicsDevice->IsDepthEnabled();
    pipelineCI.depth_stencil_state.enable_depth_test = useDepth;
    pipelineCI.depth_stencil_state.enable_depth_write = useDepth;
    pipelineCI.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS_OR_EQUAL;

    pipelineCI.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
    pipelineCI.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
    pipelineCI.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;

    SDL_GPUColorTargetDescription ctd;
    SDL_zero(ctd);

    SDL_GPUTextureFormat targetFormat;
    if (graphicsDevice->IsUsingRenderTarget()) {
        targetFormat = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    } else {
        targetFormat = graphicsDevice->GetSwapchainFormat();
    }
    ctd.format = targetFormat;
    ctd.blend_state = GetBlendState(blendMode);

    pipelineCI.target_info.num_color_targets = 1;
    pipelineCI.target_info.color_target_descriptions = &ctd;
    pipelineCI.target_info.has_depth_stencil_target = useDepth;
    if (useDepth) {
        pipelineCI.target_info.depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D24_UNORM;
    }

    pipeline = SDL_CreateGPUGraphicsPipeline(device, &pipelineCI);
    if (!pipeline) {
        spdlog::error("Failed to create slug pipeline: {}", SDL_GetError());
        throw std::runtime_error("Failed to create slug pipeline");
    }

    currentBlendMode = blendMode;
}

void SlugRenderer::Begin(BlendMode blendMode, const SlugFont &font, const glm::mat4 &mvp) {
    SDL_assert(!batchStarted);

    if (!pipeline || blendMode != currentBlendMode) {
        CreatePipeline(blendMode);
    }

    atlasBuffer = font.GetAtlasBuffer();
    mvpMatrix = mvp;
    activeVertices = 0;
    batchStarted = true;
}

void SlugRenderer::UpdateAtlas(const SlugFont &font) {
    atlasBuffer = font.GetAtlasBuffer();
}

void SlugRenderer::End() {
    SDL_assert(batchStarted);

    if (activeVertices > 0) {
        Flush();
    }

    atlasBuffer = nullptr;
    batchStarted = false;
}

void SlugRenderer::BatchGlyphQuad(const SlugVertex verts[6]) {
    SDL_assert(batchStarted);

    if (activeVertices + 6 > maxVertices) {
        Flush();
    }

    memcpy(&vertices[activeVertices], verts, 6 * sizeof(SlugVertex));
    activeVertices += 6;
}

void SlugRenderer::Flush() {
    if (activeVertices == 0)
        return;

    // Upload vertex data via VertexBuffer
    graphicsDevice->EndRenderPass();
    vertexBuffer->SetVertexData(vertices.data(), activeVertices);

    // Begin render pass and draw
    graphicsDevice->BeginRenderPass();
    SDL_GPURenderPass *renderPass = graphicsDevice->GetCurrentRenderPass();
    if (!renderPass) {
        activeVertices = 0;
        return;
    }

    SDL_BindGPUGraphicsPipeline(renderPass, pipeline);

    struct {
        glm::mat4 mvp;
        float viewportW, viewportH;
        float _pad0, _pad1;
    } viewConstants;
    viewConstants.mvp = mvpMatrix;
    viewConstants.viewportW = (float)graphicsDevice->GetScreenWidth();
    viewConstants.viewportH = (float)graphicsDevice->GetScreenHeight();
    viewConstants._pad0 = viewConstants._pad1 = 0;

    SDL_PushGPUVertexUniformData(
        graphicsDevice->GetCommandBuffer(), 0, &viewConstants, sizeof(viewConstants));

    // Bind atlas as fragment storage buffer (StructuredBuffer in shader)
    if (!atlasBuffer) {
        spdlog::error("SlugRenderer::Flush: null atlas buffer");
        activeVertices = 0;
        return;
    }
    SDL_BindGPUFragmentStorageBuffers(renderPass, 0, &atlasBuffer, 1);

    // Bind vertex buffer
    SDL_GPUBufferBinding vbufBinding = {};
    vbufBinding.buffer = vertexBuffer->GetGPUBuffer();
    vbufBinding.offset = 0;
    SDL_BindGPUVertexBuffers(renderPass, 0, &vbufBinding, 1);

    SDL_DrawGPUPrimitives(renderPass, activeVertices, 1, 0, 0);

    activeVertices = 0;
}

} // namespace Lucky
