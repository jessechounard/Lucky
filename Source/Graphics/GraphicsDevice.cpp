#include <SDL3/SDL_assert.h>
#include <stdexcept>

#include <Lucky/Color.hpp>
#include <Lucky/GraphicsDevice.hpp>
#include <Lucky/Texture.hpp>
#include <Lucky/Rectangle.hpp>
#include <SDL3/SDL.h>
#include <spdlog/spdlog.h>

namespace Lucky {

namespace {

const char *GPUTextureFormatName(SDL_GPUTextureFormat format) {
    switch (format) {
    case SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM:
        return "R8G8B8A8_UNORM";
    case SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM:
        return "B8G8R8A8_UNORM";
    case SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB:
        return "R8G8B8A8_UNORM_SRGB";
    case SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB:
        return "B8G8R8A8_UNORM_SRGB";
    case SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT:
        return "R16G16B16A16_FLOAT";
    case SDL_GPU_TEXTUREFORMAT_R10G10B10A2_UNORM:
        return "R10G10B10A2_UNORM";
    default:
        return "unknown";
    }
}

} // namespace

GraphicsDevice::GraphicsDevice(SDL_Window *windowHandle, const char *gpuDriver)
    : windowHandle(windowHandle) {
    SDL_assert(windowHandle != nullptr);

    device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL |
                                     SDL_GPU_SHADERFORMAT_MSL | SDL_GPU_SHADERFORMAT_PRIVATE,
        false,
        gpuDriver);
    if (!device) {
        spdlog::error("Failed to create GPU device: {}", SDL_GetError());
        throw std::runtime_error("Failed to create GPU device");
    }

    if (!SDL_ClaimWindowForGPUDevice(device, windowHandle)) {
        spdlog::error("Failed to claim window for GPU device: {}", SDL_GetError());
        throw std::runtime_error("Failed to claim window for GPU device");
    }

    swapchainFormat = SDL_GetGPUSwapchainTextureFormat(device, windowHandle);

    SDL_GetWindowSizeInPixels(windowHandle, &screenWidth, &screenHeight);
    viewport = {0, 0, screenWidth, screenHeight};

    clearColor = Color::Black;
    needsClear = true;

    scissorEnabled = false;

    commandBuffer = nullptr;
    currentRenderPass = nullptr;
    currentCopyPass = nullptr;
    currentComputePass = nullptr;
    swapchainTexture = nullptr;
    currentRenderTarget = nullptr;

    int windowW, windowH;
    SDL_GetWindowSize(windowHandle, &windowW, &windowH);
    SDL_WindowFlags flags = SDL_GetWindowFlags(windowHandle);

    spdlog::info("GPU: SDL_GPU device created");
    spdlog::info("  Driver: {}", SDL_GetGPUDeviceDriver(device));
    spdlog::info("  Swapchain format: {}", GPUTextureFormatName(swapchainFormat));
    spdlog::info(
        "  Window size: {}x{}, drawable: {}x{}", windowW, windowH, screenWidth, screenHeight);
    spdlog::info("  Fullscreen: {}", (flags & SDL_WINDOW_FULLSCREEN) ? "yes" : "no");
    spdlog::info("  Present modes supported: vsync={}, immediate={}, mailbox={}",
        SDL_WindowSupportsGPUPresentMode(device, windowHandle, SDL_GPU_PRESENTMODE_VSYNC),
        SDL_WindowSupportsGPUPresentMode(device, windowHandle, SDL_GPU_PRESENTMODE_IMMEDIATE),
        SDL_WindowSupportsGPUPresentMode(device, windowHandle, SDL_GPU_PRESENTMODE_MAILBOX));
}

GraphicsDevice::~GraphicsDevice() {
    if (depthTexture) {
        SDL_ReleaseGPUTexture(device, depthTexture);
    }
    SDL_ReleaseWindowFromGPUDevice(device, windowHandle);
    SDL_DestroyGPUDevice(device);
}

void GraphicsDevice::SetDepthEnabled(bool enabled) {
    SDL_assert(currentRenderPass == nullptr);
    depthEnabled = enabled;
}

SDL_GPUTextureFormat GraphicsDevice::GetDepthFormat() const {
    // D16_UNORM is universally supported and avoids D3D12 quirks where
    // D24_UNORM is reported supported for textures but rejected during
    // pipeline state creation. Use D32_FLOAT only if D16 is unavailable
    // (extremely unlikely).
    SDL_GPUTextureFormat format = SDL_GPU_TEXTUREFORMAT_D16_UNORM;
    if (!SDL_GPUTextureSupportsFormat(device, format, SDL_GPU_TEXTURETYPE_2D,
            SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET)) {
        format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
    }
    return format;
}

void GraphicsDevice::EnsureDepthTexture() {
    if (depthTexture && depthTextureWidth == screenWidth && depthTextureHeight == screenHeight) {
        return;
    }

    if (depthTexture) {
        SDL_ReleaseGPUTexture(device, depthTexture);
        depthTexture = nullptr;
    }

    SDL_GPUTextureFormat depthFormat = GetDepthFormat();

    SDL_GPUTextureCreateInfo ci = {};
    ci.type = SDL_GPU_TEXTURETYPE_2D;
    ci.format = depthFormat;
    ci.width = screenWidth;
    ci.height = screenHeight;
    ci.layer_count_or_depth = 1;
    ci.num_levels = 1;
    ci.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
    depthTexture = SDL_CreateGPUTexture(device, &ci);

    if (!depthTexture) {
        spdlog::error("Failed to create depth texture: {}", SDL_GetError());
    }

    depthTextureWidth = screenWidth;
    depthTextureHeight = screenHeight;
}

void GraphicsDevice::SetViewport(const Rectangle &vp) {
    viewport = vp;
    // Viewport is applied when beginning a render pass or set on active pass
    if (currentRenderPass) {
        SDL_GPUViewport gpuVp;
        gpuVp.x = (float)viewport.x;
        gpuVp.y = (float)viewport.y;
        gpuVp.w = (float)viewport.width;
        gpuVp.h = (float)viewport.height;
        gpuVp.min_depth = 0.0f;
        gpuVp.max_depth = 1.0f;
        SDL_SetGPUViewport(currentRenderPass, &gpuVp);
    }
}

void GraphicsDevice::GetViewport(Rectangle &vp) const {
    vp = viewport;
}

void GraphicsDevice::SetClearColor(const Color &color) {
    clearColor = color;
    needsClear = true;
}

void GraphicsDevice::EnableScissorRectangle(const Rectangle &scissorRect) {
    scissorEnabled = true;
    scissorRectangle = scissorRect;

    if (currentRenderPass) {
        SDL_Rect rect;
        rect.x = scissorRectangle.x;
        rect.y = scissorRectangle.y;
        rect.w = scissorRectangle.width;
        rect.h = scissorRectangle.height;
        SDL_SetGPUScissor(currentRenderPass, &rect);
    }
}

void GraphicsDevice::DisableScissorRectangle() {
    scissorEnabled = false;

    if (currentRenderPass) {
        // Reset scissor to full viewport
        SDL_Rect rect;
        rect.x = viewport.x;
        rect.y = viewport.y;
        rect.w = viewport.width;
        rect.h = viewport.height;
        SDL_SetGPUScissor(currentRenderPass, &rect);
    }
}

void GraphicsDevice::BindRenderTarget(const Texture &texture, bool setViewport) {
    // End any active render pass since we're changing targets
    if (currentRenderPass) {
        EndRenderPass();
    }

    currentRenderTarget = &texture;
    needsClear = true;

    if (setViewport) {
        viewport = {
            0, 0, static_cast<int>(texture.GetWidth()), static_cast<int>(texture.GetHeight())};
    }
}

void GraphicsDevice::BindRenderTarget(
    const Texture &color, const Texture &depth, bool setViewport) {
    BindRenderTarget(color, setViewport);
    BindDepthRenderTarget(depth, 0);
}

void GraphicsDevice::BindDepthRenderTarget(const Texture &depth, uint32_t layer) {
    SDL_assert(IsDepthTextureType(depth.GetTextureType()));
    if (IsCubeTextureType(depth.GetTextureType())) {
        SDL_assert(layer < 6);
    } else {
        SDL_assert(layer == 0);
    }

    if (currentRenderPass) {
        EndRenderPass();
    }

    currentDepthTarget = &depth;
    currentDepthLayer = layer;
    needsClear = true;

    if (!currentRenderTarget) {
        viewport = {0, 0, static_cast<int>(depth.GetWidth()), static_cast<int>(depth.GetHeight())};
    }
}

void GraphicsDevice::UnbindDepthRenderTarget() {
    if (currentRenderPass) {
        EndRenderPass();
    }

    currentDepthTarget = nullptr;
    currentDepthLayer = 0;
    needsClear = true;

    // Restore viewport to whichever target is still bound: color
    // texture if one is, otherwise the full swapchain. Without this,
    // a depth-only shadow pass leaves the viewport at the shadow map
    // size and the next render pass renders into a stretched region.
    if (currentRenderTarget) {
        viewport = {0,
            0,
            static_cast<int>(currentRenderTarget->GetWidth()),
            static_cast<int>(currentRenderTarget->GetHeight())};
    } else {
        viewport = {0, 0, screenWidth, screenHeight};
    }
}

void GraphicsDevice::UnbindRenderTarget(bool resetViewport) {
    if (currentRenderPass) {
        EndRenderPass();
    }

    currentRenderTarget = nullptr;
    needsClear = true;

    if (resetViewport) {
        viewport = {0, 0, screenWidth, screenHeight};
    }
}

bool GraphicsDevice::IsUsingRenderTarget() const {
    return currentRenderTarget != nullptr;
}

bool GraphicsDevice::IsUsingDepthTarget() const {
    return currentDepthTarget != nullptr;
}

void GraphicsDevice::BeginFrame() {
    SDL_assert(commandBuffer == nullptr);

    commandBuffer = SDL_AcquireGPUCommandBuffer(device);
    if (!commandBuffer) {
        spdlog::error("Failed to acquire GPU command buffer: {}", SDL_GetError());
        return;
    }

    uint32_t drawableW = 0, drawableH = 0;
    if (!SDL_WaitAndAcquireGPUSwapchainTexture(
            commandBuffer, windowHandle, &swapchainTexture, &drawableW, &drawableH)) {
        spdlog::error("Failed to acquire swapchain texture: {}", SDL_GetError());
        SDL_CancelGPUCommandBuffer(commandBuffer);
        commandBuffer = nullptr;
        return;
    }

    if (!swapchainTexture) {
        SDL_CancelGPUCommandBuffer(commandBuffer);
        commandBuffer = nullptr;
        return;
    }

    // Update screen dimensions
    screenWidth = (int)drawableW;
    screenHeight = (int)drawableH;

    // Reset viewport to screen
    if (!currentRenderTarget) {
        viewport = {0, 0, screenWidth, screenHeight};
    }

    needsClear = true;
}

void GraphicsDevice::EndFrame() {
    SDL_assert(commandBuffer != nullptr);

    if (currentComputePass) {
        EndComputePass();
    }
    if (currentCopyPass) {
        EndCopyPass();
    }
    if (currentRenderPass) {
        EndRenderPass();
    }

    if (commandBuffer) {
        SDL_SubmitGPUCommandBuffer(commandBuffer);
        commandBuffer = nullptr;
    }

    swapchainTexture = nullptr;
}

void GraphicsDevice::BeginRenderPass() {
    SDL_assert(commandBuffer != nullptr);
    SDL_assert(currentRenderPass == nullptr);

    if (!commandBuffer || currentRenderPass) {
        return;
    }

    bool clearing = needsClear;
    bool depthOnly = (currentRenderTarget == nullptr) && (currentDepthTarget != nullptr);

    SDL_GPUColorTargetInfo colorTargetInfo;
    SDL_GPUColorTargetInfo *colorInfo = nullptr;
    uint32_t colorCount = 0;

    if (!depthOnly) {
        SDL_GPUTexture *colorTarget = GetCurrentColorTarget();
        if (!colorTarget) {
            return;
        }

        SDL_zero(colorTargetInfo);
        colorTargetInfo.texture = colorTarget;

        if (clearing) {
            colorTargetInfo.load_op = SDL_GPU_LOADOP_CLEAR;
            colorTargetInfo.clear_color.r = clearColor.r;
            colorTargetInfo.clear_color.g = clearColor.g;
            colorTargetInfo.clear_color.b = clearColor.b;
            colorTargetInfo.clear_color.a = clearColor.a;
        } else {
            colorTargetInfo.load_op = SDL_GPU_LOADOP_LOAD;
        }

        colorTargetInfo.store_op = SDL_GPU_STOREOP_STORE;
        colorTargetInfo.cycle = false;
        colorInfo = &colorTargetInfo;
        colorCount = 1;
    }

    SDL_GPUDepthStencilTargetInfo *depthInfo = nullptr;
    SDL_GPUDepthStencilTargetInfo depthTargetInfo;

    if (currentDepthTarget) {
        SDL_zero(depthTargetInfo);
        depthTargetInfo.texture = currentDepthTarget->GetGPUTexture();
        depthTargetInfo.layer = static_cast<Uint8>(currentDepthLayer);
        depthTargetInfo.load_op = clearing ? SDL_GPU_LOADOP_CLEAR : SDL_GPU_LOADOP_LOAD;
        depthTargetInfo.store_op = SDL_GPU_STOREOP_STORE;
        depthTargetInfo.clear_depth = 1.0f;
        depthTargetInfo.cycle = false;
        depthInfo = &depthTargetInfo;
    } else if (depthEnabled && !currentRenderTarget) {
        EnsureDepthTexture();
        if (depthTexture) {
            SDL_zero(depthTargetInfo);
            depthTargetInfo.texture = depthTexture;
            depthTargetInfo.load_op = clearing ? SDL_GPU_LOADOP_CLEAR : SDL_GPU_LOADOP_LOAD;
            depthTargetInfo.store_op = SDL_GPU_STOREOP_STORE;
            depthTargetInfo.clear_depth = 1.0f;
            depthTargetInfo.cycle = false;
            depthInfo = &depthTargetInfo;
        }
    }

    if (clearing) {
        needsClear = false;
    }

    currentRenderPass = SDL_BeginGPURenderPass(commandBuffer, colorInfo, colorCount, depthInfo);
    if (!currentRenderPass) {
        spdlog::error("Failed to begin GPU render pass: {}", SDL_GetError());
        return;
    }

    {
        // Set viewport
        SDL_GPUViewport gpuVp;
        gpuVp.x = (float)viewport.x;
        gpuVp.y = (float)viewport.y;
        gpuVp.w = (float)viewport.width;
        gpuVp.h = (float)viewport.height;
        gpuVp.min_depth = 0.0f;
        gpuVp.max_depth = 1.0f;
        SDL_SetGPUViewport(currentRenderPass, &gpuVp);

        if (scissorEnabled) {
            SDL_Rect rect;
            rect.x = scissorRectangle.x;
            rect.y = scissorRectangle.y;
            rect.w = scissorRectangle.width;
            rect.h = scissorRectangle.height;
            SDL_SetGPUScissor(currentRenderPass, &rect);
        }
    }
}

void GraphicsDevice::EndRenderPass() {
    if (currentRenderPass) {
        SDL_EndGPURenderPass(currentRenderPass);
        currentRenderPass = nullptr;
    }
}

void GraphicsDevice::BeginComputePass(
    const SDL_GPUStorageBufferReadWriteBinding *bufferBindings, uint32_t numBufferBindings) {
    SDL_assert(commandBuffer != nullptr);

    if (!commandBuffer) {
        return;
    }

    if (currentRenderPass) {
        EndRenderPass();
    }
    if (currentCopyPass) {
        EndCopyPass();
    }

    currentComputePass =
        SDL_BeginGPUComputePass(commandBuffer, nullptr, 0, bufferBindings, numBufferBindings);
    if (!currentComputePass) {
        spdlog::error("Failed to begin GPU compute pass: {}", SDL_GetError());
    }
}

void GraphicsDevice::EndComputePass() {
    if (currentComputePass) {
        SDL_EndGPUComputePass(currentComputePass);
        currentComputePass = nullptr;
    }
}

void GraphicsDevice::BeginCopyPass() {
    SDL_assert(commandBuffer != nullptr);

    if (!commandBuffer) {
        return;
    }

    // Must end other passes before copy pass
    if (currentRenderPass) {
        EndRenderPass();
    }
    if (currentComputePass) {
        EndComputePass();
    }

    currentCopyPass = SDL_BeginGPUCopyPass(commandBuffer);
    if (!currentCopyPass) {
        spdlog::error("Failed to begin GPU copy pass: {}", SDL_GetError());
    }
}

void GraphicsDevice::EndCopyPass() {
    if (currentCopyPass) {
        SDL_EndGPUCopyPass(currentCopyPass);
        currentCopyPass = nullptr;
    }
}

SDL_GPUTexture *GraphicsDevice::GetCurrentColorTarget() const {
    if (currentRenderTarget) {
        return currentRenderTarget->GetGPUTexture();
    }
    return swapchainTexture;
}

} // namespace Lucky
