#pragma once

#include <stdint.h>

#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_video.h>

#include <Lucky/Color.hpp>
#include <Lucky/Types.hpp>
#include <Lucky/Rectangle.hpp>

namespace Lucky {

struct Color;
struct Texture;

struct GraphicsDevice {
  public:
    GraphicsDevice(SDL_Window *windowHandle, const char *gpuDriver = nullptr);
    GraphicsDevice(const GraphicsDevice &) = delete;
    ~GraphicsDevice();

    GraphicsDevice &operator=(const GraphicsDevice &) = delete;
    GraphicsDevice &operator=(const GraphicsDevice &&) = delete;

    void SetViewport(const Rectangle &viewport);
    void GetViewport(Rectangle &viewport) const;

    int GetScreenWidth() const {
        return screenWidth;
    }

    int GetScreenHeight() const {
        return screenHeight;
    }

    void SetClearColor(const Color &clearColor);

    void SetBlendMode(BlendMode blendMode);
    BlendMode GetBlendMode() const {
        return blendMode;
    }

    void EnableScissorsRectangle(const Rectangle &scissorsRectangle);
    void DisableScissorsRectangle();

    void BindRenderTarget(const Texture &texture, bool setViewport = true);
    void UnbindRenderTarget(bool resetViewport = true);
    bool IsUsingRenderTarget() const;

    void BeginFrame();
    void EndFrame();

    // Render pass management
    void BeginRenderPass();
    void EndRenderPass();
    SDL_GPURenderPass *GetCurrentRenderPass() {
        return currentRenderPass;
    }

    // Copy pass management (for buffer uploads)
    void BeginCopyPass();
    void EndCopyPass();
    SDL_GPUCopyPass *GetCurrentCopyPass() {
        return currentCopyPass;
    }

    // Compute pass management
    void BeginComputePass(const SDL_GPUStorageBufferReadWriteBinding *bufferBindings = nullptr,
        uint32_t numBufferBindings = 0);
    void EndComputePass();
    SDL_GPUComputePass *GetCurrentComputePass() {
        return currentComputePass;
    }

    // Accessors for SDL_GPU objects
    SDL_GPUDevice *GetDevice() {
        return device;
    }
    SDL_GPUCommandBuffer *GetCommandBuffer() {
        return commandBuffer;
    }
    SDL_GPUTextureFormat GetSwapchainFormat() const {
        return swapchainFormat;
    }

    // The render target texture currently bound (nullptr = swapchain)
    SDL_GPUTexture *GetCurrentColorTarget();

    // Depth buffer
    void SetDepthEnabled(bool enabled);
    bool IsDepthEnabled() const {
        return depthEnabled;
    }

  private:
    void EnsureDepthTexture();
    SDL_GPUDevice *device;
    SDL_Window *windowHandle;
    SDL_GPUTextureFormat swapchainFormat;

    SDL_GPUCommandBuffer *commandBuffer;
    SDL_GPURenderPass *currentRenderPass;
    SDL_GPUCopyPass *currentCopyPass;
    SDL_GPUComputePass *currentComputePass;
    SDL_GPUTexture *swapchainTexture;
    uint32_t drawableW, drawableH;

    Rectangle viewport;
    Color clearColor;
    bool needsClear;

    BlendMode blendMode;

    int screenWidth;
    int screenHeight;

    bool scissorsEnabled;
    Rectangle scissorsRectangle;

    int drawCallsThisFrame;

    // Render target state
    const Texture *currentRenderTarget;

    // Depth buffer
    SDL_GPUTexture *depthTexture = nullptr;
    int depthTextureWidth = 0;
    int depthTextureHeight = 0;
    bool depthEnabled = false;
};

} // namespace Lucky
