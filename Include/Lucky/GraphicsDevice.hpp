#pragma once

#include <stdint.h>

#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_video.h>

#include <Lucky/Color.hpp>
#include <Lucky/Rectangle.hpp>
#include <Lucky/Types.hpp>

namespace Lucky {

struct Color;
struct Texture;

/**
 * Wraps SDL3's GPU API into a single owned device.
 *
 * GraphicsDevice owns the SDL_GPUDevice, claims the application window for
 * GPU rendering, and orchestrates the per-frame command buffer / pass
 * lifecycle. Renderers (BatchRenderer, ShapeRenderer, SlugRenderer, etc.)
 * read raw SDL handles from this class to issue draw and compute commands.
 *
 * # Frame lifecycle
 *
 * Every frame follows the same shape:
 *
 *     graphicsDevice.BeginFrame();
 *     if (!graphicsDevice.GetCommandBuffer()) {
 *         return; // swapchain unavailable this frame, skip drawing
 *     }
 *
 *     // ...issue render/copy/compute passes via renderers...
 *
 *     graphicsDevice.EndFrame();
 *
 * BeginFrame acquires a command buffer and the swapchain texture; EndFrame
 * submits. Skipping BeginFrame, double-calling it, or calling EndFrame
 * without a successful BeginFrame is a programmer error and asserts in
 * debug. The `GetCommandBuffer() == nullptr` check is how callers detect a
 * failed swapchain acquisition (e.g. window minimized) and skip the frame
 * gracefully -- in that case do not call EndFrame.
 *
 * # Pass mutual exclusion
 *
 * SDL_GPU command buffers can only have one active pass at a time. This
 * class tracks the active pass and silently ends an existing pass of a
 * different kind when a new one is started -- e.g., calling BeginCopyPass
 * inside a render pass auto-ends the render pass first. This is convenient
 * but means renderers should not assume a render pass survives across an
 * unrelated copy or compute call.
 *
 * EndFrame closes any pass left open by the caller, so a leaked
 * Begin*Pass without a matching End*Pass will not corrupt submission --
 * but renderers should still pair Begin/End for clarity.
 *
 * # Lazy clear
 *
 * Clears happen at the start of the next render pass after one of the
 * following triggers:
 *   - SetClearColor (any time)
 *   - BeginFrame (every frame)
 *   - BindRenderTarget / UnbindRenderTarget (target switch)
 *
 * The first BeginRenderPass after each trigger uses LOADOP_CLEAR; later
 * BeginRenderPass calls in the same frame use LOADOP_LOAD so existing
 * pixels survive. This is important for layered rendering (e.g. UI on top
 * of game): only the first pass clears the target.
 *
 * # Render targets
 *
 * BindRenderTarget / UnbindRenderTarget switches the active color target
 * between the swapchain (default) and a user-provided Texture. The
 * GraphicsDevice stores a pointer to the Texture object -- the caller MUST
 * keep the Texture alive until UnbindRenderTarget is called or the
 * GraphicsDevice is destroyed. A destroyed Texture leaves a dangling
 * pointer that will be dereferenced on the next render pass.
 *
 * Binding a target with `setViewport == true` (the default) replaces the
 * viewport with one matching the texture's full size; the previous
 * viewport is not saved.
 *
 * Depth attachment is currently swapchain-only -- when a user render
 * target is active, depth is not attached even if SetDepthEnabled(true)
 * was called.
 *
 * # Depth buffer
 *
 * SetDepthEnabled(true) opts the swapchain into depth-stencil testing.
 * The depth texture is created lazily on the first depth-enabled render
 * pass and re-created on resize. Format is D24_UNORM, falling back to
 * D32_FLOAT if the driver doesn't support D24. SetDepthEnabled must be
 * called outside an active render pass; calling it mid-pass asserts in
 * debug. The change takes effect on the next BeginRenderPass.
 *
 * # Coordinate convention
 *
 * SDL_GPU's viewport is Y-down by convention. Lucky's renderers (Batch,
 * Shape, Slug, Sdf) all build their MVP with `glm::ortho(0, w, 0, h)` --
 * Y-up in pixel space. The viewport you pass to SetViewport is in SDL's
 * Y-down coordinates, but draw geometry is normally produced by the
 * renderers in their own Y-up frame and projected through the MVP. Avoid
 * mixing the two unless you understand which space your shader expects.
 *
 * # Present mode
 *
 * The constructor uses SDL's default present mode (typically VSync). There
 * is currently no API to select a different mode; if you need immediate or
 * mailbox presentation, extend the constructor.
 *
 * # Thread safety
 *
 * GraphicsDevice is not thread-safe. All methods must be called from the
 * thread that created the SDL window.
 */
struct GraphicsDevice {
    /**
     * Creates the GPU device and claims the window for GPU rendering.
     *
     * Picks the best driver SDL can offer matching SPIR-V, DXIL, MSL, or
     * the platform's private shader format. Throws std::runtime_error if
     * device creation or window claim fails.
     *
     * \param windowHandle the SDL window to render into. Must outlive the
     *                     GraphicsDevice.
     * \param gpuDriver optional SDL driver name ("vulkan", "direct3d12",
     *                  "metal", etc.). nullptr lets SDL pick.
     */
    GraphicsDevice(SDL_Window *windowHandle, const char *gpuDriver = nullptr);

    GraphicsDevice(const GraphicsDevice &) = delete;
    GraphicsDevice &operator=(const GraphicsDevice &) = delete;
    GraphicsDevice &operator=(GraphicsDevice &&) = delete;

    ~GraphicsDevice();

    /**
     * Sets the viewport rectangle (in SDL Y-down pixel coordinates).
     *
     * Updates the stored viewport and, if a render pass is currently
     * active, applies the new viewport immediately. The viewport is also
     * re-applied at the start of every BeginRenderPass.
     */
    void SetViewport(const Rectangle &viewport);

    /**
     * Reads the current viewport into the caller's rectangle.
     */
    void GetViewport(Rectangle &viewport) const;

    /**
     * Returns the current drawable width in pixels.
     *
     * Updated by BeginFrame from the swapchain texture; tracks window
     * resizes automatically.
     */
    int GetScreenWidth() const {
        return screenWidth;
    }

    /**
     * Returns the current drawable height in pixels.
     */
    int GetScreenHeight() const {
        return screenHeight;
    }

    /**
     * Sets the color used to clear the active target on the next clearing
     * BeginRenderPass.
     *
     * Marks the device as needing-clear, so the next render pass uses
     * LOADOP_CLEAR. See the class-level "Lazy clear" section for the full
     * trigger list.
     */
    void SetClearColor(const Color &clearColor);

    /**
     * Restricts subsequent draw calls to a sub-rectangle of the viewport.
     *
     * If a render pass is currently active, the scissor is applied
     * immediately. Otherwise it is applied at the start of the next
     * BeginRenderPass. Scissor coordinates are in SDL Y-down pixel space,
     * same as the viewport.
     */
    void EnableScissorRectangle(const Rectangle &scissorRectangle);

    /**
     * Removes the scissor restriction, returning to whole-viewport
     * rendering.
     */
    void DisableScissorRectangle();

    /**
     * Switches the active color target to a user-provided Texture.
     *
     * Auto-ends any active render pass. Marks the device as
     * needing-clear, so the next BeginRenderPass clears the target with
     * the current clear color.
     *
     * The Texture object MUST outlive the binding (until UnbindRenderTarget
     * is called or the GraphicsDevice is destroyed). The device stores the
     * Texture pointer and dereferences it on every subsequent render pass.
     *
     * \param texture render target. Must remain alive until unbound.
     * \param setViewport if true (default), replaces the viewport with one
     *                    matching the texture's full size. The previous
     *                    viewport is not saved.
     */
    void BindRenderTarget(const Texture &texture, bool setViewport = true);

    /**
     * Switches the active color target back to the swapchain.
     *
     * Auto-ends any active render pass and marks the device as
     * needing-clear.
     *
     * \param resetViewport if true (default), resets the viewport to the
     *                      full screen.
     */
    void UnbindRenderTarget(bool resetViewport = true);

    /**
     * Returns true while a Texture render target is bound.
     */
    bool IsUsingRenderTarget() const;

    /**
     * Acquires a fresh command buffer and the swapchain texture for this
     * frame.
     *
     * Asserts that no frame is currently in progress (commandBuffer must be
     * null on entry). On failure (e.g. window minimized, swapchain
     * unavailable), the command buffer is cancelled and GetCommandBuffer()
     * returns nullptr; callers should detect this and skip the frame
     * without calling EndFrame.
     */
    void BeginFrame();

    /**
     * Closes any open pass and submits the frame's command buffer.
     *
     * Asserts that a frame is in progress (commandBuffer must be non-null).
     * If BeginFrame failed (commandBuffer null), do not call EndFrame.
     */
    void EndFrame();

    /**
     * Begins a render pass against the current color target (swapchain or
     * bound Texture).
     *
     * Asserts that a frame is in progress and no render pass is currently
     * active. Applies the clear color (if needsClear), the viewport, the
     * scissor (if enabled), and depth attachment (if enabled and
     * rendering to the swapchain).
     */
    void BeginRenderPass();

    /**
     * Ends the current render pass, if one is active.
     *
     * Safe to call even when no pass is active.
     */
    void EndRenderPass();

    /**
     * Returns the active render pass handle, or nullptr if none is active.
     *
     * Renderers pass this to SDL_BindGPUGraphicsPipeline, SDL_DrawGPU*, etc.
     */
    SDL_GPURenderPass *GetCurrentRenderPass() const {
        return currentRenderPass;
    }

    /**
     * Begins a copy pass for buffer or texture uploads.
     *
     * Asserts that a frame is in progress. If a render pass or compute
     * pass is currently active, it is auto-ended first.
     */
    void BeginCopyPass();

    /**
     * Ends the current copy pass, if one is active.
     */
    void EndCopyPass();

    /**
     * Returns the active copy pass handle, or nullptr if none is active.
     */
    SDL_GPUCopyPass *GetCurrentCopyPass() const {
        return currentCopyPass;
    }

    /**
     * Begins a compute pass with optional storage buffer bindings.
     *
     * Asserts that a frame is in progress. If a render pass or copy pass
     * is currently active, it is auto-ended first.
     *
     * \param bufferBindings optional array of read/write storage buffer
     *                       bindings the compute shaders will access.
     * \param numBufferBindings number of entries in bufferBindings.
     */
    void BeginComputePass(const SDL_GPUStorageBufferReadWriteBinding *bufferBindings = nullptr,
        uint32_t numBufferBindings = 0);

    /**
     * Ends the current compute pass, if one is active.
     */
    void EndComputePass();

    /**
     * Returns the active compute pass handle, or nullptr if none is active.
     */
    SDL_GPUComputePass *GetCurrentComputePass() const {
        return currentComputePass;
    }

    /**
     * Returns the underlying SDL_GPUDevice. Used by renderers to create
     * pipelines, buffers, samplers, and other GPU resources.
     */
    SDL_GPUDevice *GetDevice() const {
        return device;
    }

    /**
     * Returns the current frame's command buffer, or nullptr if BeginFrame
     * has not been called or failed to acquire a swapchain.
     *
     * Used by renderers to record draw and dispatch commands. Callers
     * should null-check this after BeginFrame to detect a skipped frame.
     */
    SDL_GPUCommandBuffer *GetCommandBuffer() const {
        return commandBuffer;
    }

    /**
     * Returns the swapchain's texture format. Renderers building pipelines
     * targeting the swapchain need this to match the color attachment
     * format.
     */
    SDL_GPUTextureFormat GetSwapchainFormat() const {
        return swapchainFormat;
    }

    /**
     * Returns the active color target -- the bound Texture's GPU handle if
     * one is set, otherwise the current frame's swapchain texture.
     */
    SDL_GPUTexture *GetCurrentColorTarget() const;

    /**
     * Enables or disables depth-stencil testing on the swapchain.
     *
     * Must be called outside a render pass; asserts in debug if a pass is
     * active. Takes effect at the next BeginRenderPass. The depth texture
     * is created lazily on first use and recreated on resize.
     *
     * Only affects the swapchain target; user-bound render targets do not
     * receive a depth attachment regardless of this setting.
     */
    void SetDepthEnabled(bool enabled);

    /**
     * Returns true if depth-stencil testing is enabled.
     */
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

    Rectangle viewport;
    Color clearColor;
    bool needsClear;

    int screenWidth;
    int screenHeight;

    bool scissorEnabled;
    Rectangle scissorRectangle;

    // Render target state
    const Texture *currentRenderTarget;

    // Depth buffer
    SDL_GPUTexture *depthTexture = nullptr;
    int depthTextureWidth = 0;
    int depthTextureHeight = 0;
    bool depthEnabled = false;
};

} // namespace Lucky
