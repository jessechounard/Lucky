#include <stdexcept>

#include <Lucky/BlendState.hpp>
#include <Lucky/GraphicsDevice.hpp>
#include <Lucky/ParticleEmitter.hpp>
#include <Lucky/Shader.hpp>
#include <SDL3/SDL.h>
#include <spdlog/spdlog.h>

namespace Lucky {

struct UpdateParams {
    float deltaTime;
    float _pad0;
    float gravityX;
    float gravityY;
    float screenWidth;
    float screenHeight;
    float _pad1[2];
};

ParticleEmitter::ParticleEmitter(GraphicsDevice &graphicsDevice, const std::string &shaderPath,
    uint32_t maxParticles, SDL_GPUTexture *texture)
    : graphicsDevice(&graphicsDevice), maxParticles(maxParticles), nextEmitIndex(0), aliveCount(0),
      texture(texture), ownsTexture(texture == nullptr) {

    SDL_GPUDevice *device = graphicsDevice.GetDevice();

    // Create default 64x64 soft circle texture if none provided
    if (!this->texture) {
        this->texture = CreateDefaultCircleTexture();
    }

    // Create sampler
    SDL_GPUSamplerCreateInfo samplerCI;
    SDL_zero(samplerCI);
    samplerCI.min_filter = SDL_GPU_FILTER_LINEAR;
    samplerCI.mag_filter = SDL_GPU_FILTER_LINEAR;
    samplerCI.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    samplerCI.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    sampler = SDL_CreateGPUSampler(device, &samplerCI);

    // Create particle storage buffer
    SDL_GPUBufferCreateInfo bufCI;
    SDL_zero(bufCI);
    bufCI.usage = SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ |
                  SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE |
                  SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ;
    bufCI.size = maxParticles * sizeof(Particle);
    particleBuffer = SDL_CreateGPUBuffer(device, &bufCI);
    if (!particleBuffer) {
        throw std::runtime_error("Failed to create particle storage buffer");
    }

    // Create transfer buffer for uploading emitted particles
    SDL_GPUTransferBufferCreateInfo tbCI;
    SDL_zero(tbCI);
    tbCI.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    tbCI.size = maxParticles * sizeof(Particle);
    transferBuffer = SDL_CreateGPUTransferBuffer(device, &tbCI);
    if (!transferBuffer) {
        throw std::runtime_error("Failed to create particle transfer buffer");
    }

    // Zero-initialize the buffer so all particles start dead
    void *mapped = SDL_MapGPUTransferBuffer(device, transferBuffer, false);
    memset(mapped, 0, maxParticles * sizeof(Particle));
    SDL_UnmapGPUTransferBuffer(device, transferBuffer);

    graphicsDevice.BeginCopyPass();
    SDL_GPUTransferBufferLocation src;
    SDL_zero(src);
    src.transfer_buffer = transferBuffer;
    src.offset = 0;

    SDL_GPUBufferRegion dst;
    SDL_zero(dst);
    dst.buffer = particleBuffer;
    dst.offset = 0;
    dst.size = maxParticles * sizeof(Particle);

    SDL_UploadToGPUBuffer(graphicsDevice.GetCurrentCopyPass(), &src, &dst, false);
    graphicsDevice.EndCopyPass();

    // Create atomic counter buffer (4 bytes)
    SDL_GPUBufferCreateInfo counterCI;
    SDL_zero(counterCI);
    counterCI.usage = SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE;
    counterCI.size = sizeof(uint32_t);
    counterBuffer = SDL_CreateGPUBuffer(device, &counterCI);
    if (!counterBuffer) {
        throw std::runtime_error("Failed to create counter buffer");
    }

    // Download transfer buffer for reading counter back to CPU
    SDL_GPUTransferBufferCreateInfo dlCI;
    SDL_zero(dlCI);
    dlCI.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
    dlCI.size = sizeof(uint32_t);
    counterDownloadBuffer = SDL_CreateGPUTransferBuffer(device, &dlCI);
    if (!counterDownloadBuffer) {
        throw std::runtime_error("Failed to create counter download buffer");
    }

    // Create pipelines
    std::string computePath = shaderPath + "/particle_update.comp";
    std::string vertPath = shaderPath + "/particle_render.vert";
    std::string fragPath = shaderPath + "/particle_render.frag";

    updatePipeline = CreateComputePipeline(computePath);
    renderPipeline = CreateRenderPipeline(vertPath, fragPath);

    spdlog::info("ParticleEmitter: created with {} max particles", maxParticles);
}

SDL_GPUTexture *ParticleEmitter::CreateDefaultCircleTexture() {
    SDL_GPUDevice *device = graphicsDevice->GetDevice();

    const uint32_t texSize = 64;
    const uint32_t texBytes = texSize * texSize * 4;

    SDL_GPUTextureCreateInfo texCI;
    SDL_zero(texCI);
    texCI.type = SDL_GPU_TEXTURETYPE_2D;
    texCI.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    texCI.width = texSize;
    texCI.height = texSize;
    texCI.layer_count_or_depth = 1;
    texCI.num_levels = 1;
    texCI.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
    SDL_GPUTexture *tex = SDL_CreateGPUTexture(device, &texCI);

    SDL_GPUTransferBufferCreateInfo pixelTbCI;
    SDL_zero(pixelTbCI);
    pixelTbCI.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    pixelTbCI.size = texBytes;
    SDL_GPUTransferBuffer *pixelTb = SDL_CreateGPUTransferBuffer(device, &pixelTbCI);
    uint8_t *pixels = static_cast<uint8_t *>(SDL_MapGPUTransferBuffer(device, pixelTb, false));

    for (uint32_t y = 0; y < texSize; ++y) {
        for (uint32_t x = 0; x < texSize; ++x) {
            float u = (x + 0.5f) / texSize * 2.0f - 1.0f;
            float v = (y + 0.5f) / texSize * 2.0f - 1.0f;
            float d = u * u + v * v;
            float alpha = 1.0f - d;
            if (alpha < 0.0f)
                alpha = 0.0f;

            uint32_t i = (y * texSize + x) * 4;
            pixels[i + 0] = 255;
            pixels[i + 1] = 255;
            pixels[i + 2] = 255;
            pixels[i + 3] = static_cast<uint8_t>(alpha * 255.0f);
        }
    }

    SDL_UnmapGPUTransferBuffer(device, pixelTb);

    SDL_GPUCommandBuffer *uploadCmdBuf = SDL_AcquireGPUCommandBuffer(device);
    SDL_GPUCopyPass *copyPass = SDL_BeginGPUCopyPass(uploadCmdBuf);
    SDL_GPUTextureTransferInfo texSrc;
    SDL_zero(texSrc);
    texSrc.transfer_buffer = pixelTb;
    SDL_GPUTextureRegion texDst;
    SDL_zero(texDst);
    texDst.texture = tex;
    texDst.w = texSize;
    texDst.h = texSize;
    texDst.d = 1;
    SDL_UploadToGPUTexture(copyPass, &texSrc, &texDst, false);
    SDL_EndGPUCopyPass(copyPass);
    SDL_SubmitGPUCommandBuffer(uploadCmdBuf);

    SDL_ReleaseGPUTransferBuffer(device, pixelTb);

    return tex;
}

void ParticleEmitter::SetTexture(SDL_GPUTexture *newTexture) {
    if (ownsTexture && texture) {
        SDL_ReleaseGPUTexture(graphicsDevice->GetDevice(), texture);
    }
    if (newTexture) {
        texture = newTexture;
        ownsTexture = false;
    } else {
        texture = CreateDefaultCircleTexture();
        ownsTexture = true;
    }
}

ParticleEmitter::~ParticleEmitter() {
    SDL_GPUDevice *device = graphicsDevice->GetDevice();
    SDL_ReleaseGPUComputePipeline(device, updatePipeline);
    SDL_ReleaseGPUGraphicsPipeline(device, renderPipeline);
    SDL_ReleaseGPUBuffer(device, particleBuffer);
    SDL_ReleaseGPUBuffer(device, counterBuffer);
    SDL_ReleaseGPUTransferBuffer(device, transferBuffer);
    SDL_ReleaseGPUTransferBuffer(device, counterDownloadBuffer);
    SDL_ReleaseGPUSampler(device, sampler);
    if (ownsTexture) {
        SDL_ReleaseGPUTexture(device, texture);
    }
}

SDL_GPUComputePipeline *ParticleEmitter::CreateComputePipeline(const std::string &basePath) {
    SDL_GPUDevice *device = graphicsDevice->GetDevice();

    Shader::ShaderBinary binary = Shader::LoadBinary(device, basePath);

    SDL_GPUComputePipelineCreateInfo ci;
    SDL_zero(ci);
    ci.code = binary.code.data();
    ci.code_size = binary.code.size();
    ci.entrypoint = "main";
    ci.format = binary.format;
    ci.num_samplers = binary.numSamplers;
    ci.num_readonly_storage_textures = binary.numReadonlyStorageTextures;
    ci.num_readonly_storage_buffers = binary.numReadonlyStorageBuffers;
    ci.num_readwrite_storage_textures = binary.numReadwriteStorageTextures;
    ci.num_readwrite_storage_buffers = binary.numReadwriteStorageBuffers;
    ci.num_uniform_buffers = binary.numUniformBuffers;
    ci.threadcount_x = 64;
    ci.threadcount_y = 1;
    ci.threadcount_z = 1;

    SDL_GPUComputePipeline *pipeline = SDL_CreateGPUComputePipeline(device, &ci);

    if (!pipeline) {
        throw std::runtime_error(
            std::string("Failed to create compute pipeline: ") + SDL_GetError());
    }

    return pipeline;
}

SDL_GPUGraphicsPipeline *ParticleEmitter::CreateRenderPipeline(
    const std::string &vertPath, const std::string &fragPath) {
    SDL_GPUDevice *device = graphicsDevice->GetDevice();

    Shader vertShader(*graphicsDevice, vertPath, SDL_GPU_SHADERSTAGE_VERTEX);
    Shader fragShader(*graphicsDevice, fragPath, SDL_GPU_SHADERSTAGE_FRAGMENT);

    SDL_GPUGraphicsPipelineCreateInfo pipelineCI;
    SDL_zero(pipelineCI);

    // No vertex buffers — vertex shader generates quads from SV_VertexID
    pipelineCI.vertex_input_state.num_vertex_buffers = 0;
    pipelineCI.vertex_input_state.num_vertex_attributes = 0;

    pipelineCI.vertex_shader = vertShader.GetHandle();
    pipelineCI.fragment_shader = fragShader.GetHandle();
    pipelineCI.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;

    pipelineCI.depth_stencil_state.enable_depth_test = false;
    pipelineCI.depth_stencil_state.enable_depth_write = false;

    pipelineCI.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
    pipelineCI.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
    pipelineCI.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;

    // Premultiplied alpha blending — supports both alpha-blended and additive particles:
    // Alpha-blended: store color * alpha in RGB, alpha in A
    // Additive: store color in RGB, 0 in A
    SDL_GPUColorTargetDescription ctd;
    SDL_zero(ctd);
    ctd.format = graphicsDevice->GetSwapchainFormat();
    ctd.blend_state = GetBlendState(BlendMode::PremultipliedAlpha);
    // Override alpha blend factor for particles (both src and dst use ONE_MINUS_SRC_ALPHA)
    ctd.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;

    pipelineCI.target_info.num_color_targets = 1;
    pipelineCI.target_info.color_target_descriptions = &ctd;
    pipelineCI.target_info.has_depth_stencil_target = false;

    SDL_GPUGraphicsPipeline *pipeline = SDL_CreateGPUGraphicsPipeline(device, &pipelineCI);

    if (!pipeline) {
        throw std::runtime_error(
            std::string("Failed to create particle render pipeline: ") + SDL_GetError());
    }

    return pipeline;
}

void ParticleEmitter::Emit(const ParticleEmitParams &params, uint32_t count) {
    for (uint32_t i = 0; i < count; ++i) {
        Particle p;
        float t = static_cast<float>(std::rand()) / RAND_MAX;

        p.position = params.positionFunc ? params.positionFunc() : params.position;
        p.velocity.x = params.velocityMin.x + t * (params.velocityMax.x - params.velocityMin.x);
        t = static_cast<float>(std::rand()) / RAND_MAX;
        p.velocity.y = params.velocityMin.y + t * (params.velocityMax.y - params.velocityMin.y);

        t = static_cast<float>(std::rand()) / RAND_MAX;
        p.color.r = params.colorStart.r + t * (params.colorEnd.r - params.colorStart.r);
        p.color.g = params.colorStart.g + t * (params.colorEnd.g - params.colorStart.g);
        p.color.b = params.colorStart.b + t * (params.colorEnd.b - params.colorStart.b);
        p.color.a = params.colorStart.a + t * (params.colorEnd.a - params.colorStart.a);

        t = static_cast<float>(std::rand()) / RAND_MAX;
        p.lifetime = params.lifetimeMin + t * (params.lifetimeMax - params.lifetimeMin);
        p.maxLifetime = p.lifetime;

        t = static_cast<float>(std::rand()) / RAND_MAX;
        p.size = params.sizeMin + t * (params.sizeMax - params.sizeMin);
        t = static_cast<float>(std::rand()) / RAND_MAX;
        p.rotation = params.rotationMin + t * (params.rotationMax - params.rotationMin);
        t = static_cast<float>(std::rand()) / RAND_MAX;
        p.rotationSpeed =
            params.rotationSpeedMin + t * (params.rotationSpeedMax - params.rotationSpeedMin);
        p._pad0 = 0;
        p._pad1 = 0;
        p._pad2 = 0;
        p.uvRect = params.uvRect;

        emitStaging.push_back(p);
    }
}

void ParticleEmitter::Update(float deltaTime, const glm::vec2 &gravity) {
    SDL_GPUDevice *device = graphicsDevice->GetDevice();

    // Upload newly emitted particles
    if (!emitStaging.empty()) {
        void *mapped = SDL_MapGPUTransferBuffer(device, transferBuffer, true);
        uint32_t count = static_cast<uint32_t>(emitStaging.size());

        for (uint32_t i = 0; i < count; ++i) {
            uint32_t index = (nextEmitIndex + i) % maxParticles;
            memcpy(static_cast<uint8_t *>(mapped) + index * sizeof(Particle),
                &emitStaging[i],
                sizeof(Particle));
        }

        SDL_UnmapGPUTransferBuffer(device, transferBuffer);

        graphicsDevice->BeginCopyPass();
        SDL_GPUCopyPass *copyPass = graphicsDevice->GetCurrentCopyPass();

        // Upload in contiguous chunks (handle wraparound)
        uint32_t firstChunkStart = nextEmitIndex;
        uint32_t firstChunkCount = std::min(count, maxParticles - nextEmitIndex);

        SDL_GPUTransferBufferLocation src;
        SDL_zero(src);
        src.transfer_buffer = transferBuffer;
        src.offset = firstChunkStart * sizeof(Particle);

        SDL_GPUBufferRegion dst;
        SDL_zero(dst);
        dst.buffer = particleBuffer;
        dst.offset = firstChunkStart * sizeof(Particle);
        dst.size = firstChunkCount * sizeof(Particle);

        SDL_UploadToGPUBuffer(copyPass, &src, &dst, false);

        if (firstChunkCount < count) {
            uint32_t secondChunkCount = count - firstChunkCount;

            SDL_GPUTransferBufferLocation src2;
            SDL_zero(src2);
            src2.transfer_buffer = transferBuffer;
            src2.offset = 0;

            SDL_GPUBufferRegion dst2;
            SDL_zero(dst2);
            dst2.buffer = particleBuffer;
            dst2.offset = 0;
            dst2.size = secondChunkCount * sizeof(Particle);

            SDL_UploadToGPUBuffer(copyPass, &src2, &dst2, false);
        }

        graphicsDevice->EndCopyPass();

        nextEmitIndex = (nextEmitIndex + count) % maxParticles;
        emitStaging.clear();
    }

    // Zero the counter buffer before compute
    {
        // Reuse the upload transfer buffer for the 4-byte zero write
        void *mapped = SDL_MapGPUTransferBuffer(device, transferBuffer, true);
        uint32_t zero = 0;
        memcpy(mapped, &zero, sizeof(uint32_t));
        SDL_UnmapGPUTransferBuffer(device, transferBuffer);

        graphicsDevice->BeginCopyPass();
        SDL_GPUTransferBufferLocation counterSrc;
        SDL_zero(counterSrc);
        counterSrc.transfer_buffer = transferBuffer;
        counterSrc.offset = 0;

        SDL_GPUBufferRegion counterDst;
        SDL_zero(counterDst);
        counterDst.buffer = counterBuffer;
        counterDst.offset = 0;
        counterDst.size = sizeof(uint32_t);

        SDL_UploadToGPUBuffer(
            graphicsDevice->GetCurrentCopyPass(), &counterSrc, &counterDst, false);
        graphicsDevice->EndCopyPass();
    }

    // Compute pass: update all particles and count alive
    SDL_GPUStorageBufferReadWriteBinding bufferBindings[2];
    SDL_zero(bufferBindings);
    bufferBindings[0].buffer = particleBuffer;
    bufferBindings[0].cycle = false;
    bufferBindings[1].buffer = counterBuffer;
    bufferBindings[1].cycle = false;

    graphicsDevice->BeginComputePass(bufferBindings, 2);
    SDL_GPUComputePass *computePass = graphicsDevice->GetCurrentComputePass();

    SDL_BindGPUComputePipeline(computePass, updatePipeline);

    UpdateParams params;
    params.deltaTime = deltaTime;
    params._pad0 = 0;
    params.gravityX = gravity.x;
    params.gravityY = gravity.y;
    params.screenWidth = static_cast<float>(graphicsDevice->GetScreenWidth());
    params.screenHeight = static_cast<float>(graphicsDevice->GetScreenHeight());
    params._pad1[0] = 0;
    params._pad1[1] = 0;
    SDL_PushGPUComputeUniformData(
        graphicsDevice->GetCommandBuffer(), 0, &params, sizeof(UpdateParams));

    uint32_t groupCount = (maxParticles + 63) / 64;
    SDL_DispatchGPUCompute(computePass, groupCount, 1, 1);

    graphicsDevice->EndComputePass();

    // Download counter to CPU (result available next frame)
    {
        graphicsDevice->BeginCopyPass();

        SDL_GPUBufferRegion counterSrc;
        SDL_zero(counterSrc);
        counterSrc.buffer = counterBuffer;
        counterSrc.offset = 0;
        counterSrc.size = sizeof(uint32_t);

        SDL_GPUTransferBufferLocation counterDst;
        SDL_zero(counterDst);
        counterDst.transfer_buffer = counterDownloadBuffer;
        counterDst.offset = 0;

        SDL_DownloadFromGPUBuffer(graphicsDevice->GetCurrentCopyPass(), &counterSrc, &counterDst);
        graphicsDevice->EndCopyPass();
    }

    // Read previous frame's result
    {
        void *mapped = SDL_MapGPUTransferBuffer(device, counterDownloadBuffer, false);
        memcpy(&aliveCount, mapped, sizeof(uint32_t));
        SDL_UnmapGPUTransferBuffer(device, counterDownloadBuffer);
    }
}

void ParticleEmitter::Draw(const glm::mat4 &projectionMatrix) {
    graphicsDevice->BeginRenderPass();
    SDL_GPURenderPass *renderPass = graphicsDevice->GetCurrentRenderPass();

    SDL_BindGPUGraphicsPipeline(renderPass, renderPipeline);

    SDL_GPUBuffer *const buffers[] = {particleBuffer};
    SDL_BindGPUVertexStorageBuffers(renderPass, 0, buffers, 1);

    SDL_GPUTextureSamplerBinding texBindings[] = {{texture, sampler}};
    SDL_BindGPUFragmentSamplers(renderPass, 0, texBindings, 1);

    SDL_PushGPUVertexUniformData(
        graphicsDevice->GetCommandBuffer(), 0, &projectionMatrix, sizeof(glm::mat4));

    SDL_DrawGPUPrimitives(renderPass, maxParticles * 6, 1, 0, 0);
}

} // namespace Lucky
