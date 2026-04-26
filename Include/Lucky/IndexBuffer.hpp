#pragma once

#include <stdint.h>
#include <string.h>
#include <type_traits>

#include <SDL3/SDL_assert.h>
#include <SDL3/SDL_gpu.h>
#include <Lucky/GraphicsDevice.hpp>

namespace Lucky {

/**
 * Compile-time mapping from an index type to its SDL_GPU element size.
 *
 * `IndexBuffer<T>` is restricted to `uint16_t` and `uint32_t`; any other type
 * trips a `static_assert` in the buffer's constructor.
 */
template <typename IndexType> constexpr SDL_GPUIndexElementSize IndexElementSizeOf() {
    static_assert(std::is_same_v<IndexType, uint16_t> || std::is_same_v<IndexType, uint32_t>,
        "IndexBuffer only supports uint16_t and uint32_t index types");
    if constexpr (std::is_same_v<IndexType, uint16_t>) {
        return SDL_GPU_INDEXELEMENTSIZE_16BIT;
    } else {
        return SDL_GPU_INDEXELEMENTSIZE_32BIT;
    }
}

/**
 * A typed wrapper around an SDL_GPUBuffer holding an index array.
 *
 * Mirrors `VertexBuffer<T>`: supports both dynamic (re-uploadable each frame)
 * and static (uploaded once at construction) usage. The static form releases
 * its transfer buffer immediately after upload. `IndexType` must be
 * `uint16_t` or `uint32_t`; `GetElementSize()` returns the matching
 * `SDL_GPUIndexElementSize` for binding via `SDL_BindGPUIndexBuffer`.
 *
 * # Choosing static vs dynamic
 *
 * - Use **dynamic** when index data changes per frame (rare for indices, but
 *   useful for streaming meshes or LOD swaps that re-upload through the
 *   same buffer).
 * - Use **static** for fixed mesh topology -- 3D model indices, baked tile
 *   maps, anything uploaded once and just re-bound.
 *
 * # Lifetime
 *
 * Holds a pointer to the `GraphicsDevice`. The `GraphicsDevice` must
 * outlive this buffer.
 */
template <typename IndexType> struct IndexBuffer {
    /**
     * Constructs a dynamic index buffer with `maximumIndices` of capacity.
     *
     * Allocates both the GPU buffer and a same-size transfer buffer used for
     * subsequent `SetIndexData` calls.
     *
     * \param graphicsDevice the graphics device. Must outlive this buffer.
     * \param maximumIndices upper bound on the index count that
     *                       `SetIndexData` may upload. Must be positive.
     */
    IndexBuffer(GraphicsDevice &graphicsDevice, uint32_t maximumIndices)
        : graphicsDevice(&graphicsDevice) {
        SDL_assert(maximumIndices > 0);

        bufferSize = maximumIndices * sizeof(IndexType);
        CreateGPUBuffer();
        CreateTransferBuffer();
    }

    /**
     * Constructs a static index buffer pre-populated with `indices`.
     *
     * Allocates the GPU buffer, uploads `indexCount` indices through a
     * temporary transfer buffer, then releases the transfer buffer. The
     * resulting buffer cannot be modified -- calling `SetIndexData` on it
     * asserts.
     *
     * \param graphicsDevice the graphics device. Must outlive this buffer.
     * \param indices index data to upload. Must not be null.
     * \param indexCount number of indices in `indices`. Must be positive.
     */
    IndexBuffer(GraphicsDevice &graphicsDevice, const IndexType *indices, uint32_t indexCount)
        : graphicsDevice(&graphicsDevice) {
        SDL_assert(indices != nullptr);
        SDL_assert(indexCount > 0);

        bufferSize = indexCount * sizeof(IndexType);
        CreateGPUBuffer();
        CreateTransferBuffer();
        UploadStandalone(indices, indexCount);
        ReleaseTransferBuffer();
    }

    IndexBuffer(const IndexBuffer &) = delete;
    IndexBuffer &operator=(const IndexBuffer &) = delete;
    IndexBuffer(IndexBuffer &&) = delete;
    IndexBuffer &operator=(IndexBuffer &&) = delete;

    ~IndexBuffer() {
        if (transferBuffer) {
            SDL_ReleaseGPUTransferBuffer(graphicsDevice->GetDevice(), transferBuffer);
        }
        if (gpuBuffer) {
            SDL_ReleaseGPUBuffer(graphicsDevice->GetDevice(), gpuBuffer);
        }
    }

    /**
     * Re-uploads index data to a dynamic buffer.
     *
     * Asserts in debug if called on a static buffer (no transfer buffer is
     * available). Asserts that `indexCount * sizeof(IndexType)` fits inside
     * the buffer's allocated capacity.
     *
     * Internally calls `BeginCopyPass` / `EndCopyPass` on the
     * `GraphicsDevice`, which auto-ends any active render pass. Avoid
     * calling mid-frame unless that side effect is acceptable.
     *
     * \param indices index data to upload. Must not be null.
     * \param indexCount number of indices to upload. Must be positive and
     *                   fit within the buffer's capacity.
     */
    void SetIndexData(const IndexType *indices, uint32_t indexCount) {
        SDL_assert(transferBuffer != nullptr);
        SDL_assert(indices != nullptr);
        SDL_assert(indexCount > 0);
        SDL_assert(indexCount * sizeof(IndexType) <= bufferSize);

        Upload(indices, indexCount);
    }

    /**
     * Returns the underlying SDL_GPUBuffer handle for use in pipeline
     * binding.
     */
    SDL_GPUBuffer *GetGPUBuffer() const {
        return gpuBuffer;
    }

    /**
     * Returns the SDL_GPU element size matching `IndexType`. Pass alongside
     * `GetGPUBuffer()` to `SDL_BindGPUIndexBuffer`.
     */
    static constexpr SDL_GPUIndexElementSize GetElementSize() {
        return IndexElementSizeOf<IndexType>();
    }

  private:
    void CreateGPUBuffer() {
        SDL_GPUBufferCreateInfo bufCI;
        SDL_zero(bufCI);
        bufCI.usage = SDL_GPU_BUFFERUSAGE_INDEX;
        bufCI.size = bufferSize;

        gpuBuffer = SDL_CreateGPUBuffer(graphicsDevice->GetDevice(), &bufCI);
        SDL_assert(gpuBuffer);
    }

    void CreateTransferBuffer() {
        SDL_GPUTransferBufferCreateInfo tbCI;
        SDL_zero(tbCI);
        tbCI.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        tbCI.size = bufferSize;

        transferBuffer = SDL_CreateGPUTransferBuffer(graphicsDevice->GetDevice(), &tbCI);
        SDL_assert(transferBuffer);
    }

    void ReleaseTransferBuffer() {
        SDL_ReleaseGPUTransferBuffer(graphicsDevice->GetDevice(), transferBuffer);
        transferBuffer = nullptr;
    }

    void Upload(const IndexType *indices, uint32_t indexCount) {
        uint32_t dataSize = indexCount * sizeof(IndexType);

        void *mapped = SDL_MapGPUTransferBuffer(graphicsDevice->GetDevice(), transferBuffer, true);
        memcpy(mapped, indices, dataSize);
        SDL_UnmapGPUTransferBuffer(graphicsDevice->GetDevice(), transferBuffer);

        graphicsDevice->BeginCopyPass();
        SDL_GPUCopyPass *copyPass = graphicsDevice->GetCurrentCopyPass();
        if (!copyPass) {
            return;
        }

        SDL_GPUTransferBufferLocation src;
        SDL_zero(src);
        src.transfer_buffer = transferBuffer;
        src.offset = 0;

        SDL_GPUBufferRegion dst;
        SDL_zero(dst);
        dst.buffer = gpuBuffer;
        dst.offset = 0;
        dst.size = dataSize;

        SDL_UploadToGPUBuffer(copyPass, &src, &dst, false);
        graphicsDevice->EndCopyPass();
    }

    // Used by the static constructor: acquires + submits its own
    // SDL_GPUCommandBuffer so a static IndexBuffer can be built outside
    // an active frame.
    void UploadStandalone(const IndexType *indices, uint32_t indexCount) {
        uint32_t dataSize = indexCount * sizeof(IndexType);
        SDL_GPUDevice *device = graphicsDevice->GetDevice();

        void *mapped = SDL_MapGPUTransferBuffer(device, transferBuffer, true);
        memcpy(mapped, indices, dataSize);
        SDL_UnmapGPUTransferBuffer(device, transferBuffer);

        SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(device);
        if (!cmd) {
            return;
        }
        SDL_GPUCopyPass *copyPass = SDL_BeginGPUCopyPass(cmd);
        if (!copyPass) {
            SDL_SubmitGPUCommandBuffer(cmd);
            return;
        }

        SDL_GPUTransferBufferLocation src;
        SDL_zero(src);
        src.transfer_buffer = transferBuffer;

        SDL_GPUBufferRegion dst;
        SDL_zero(dst);
        dst.buffer = gpuBuffer;
        dst.size = dataSize;

        SDL_UploadToGPUBuffer(copyPass, &src, &dst, false);
        SDL_EndGPUCopyPass(copyPass);
        SDL_SubmitGPUCommandBuffer(cmd);
    }

    GraphicsDevice *graphicsDevice;
    SDL_GPUBuffer *gpuBuffer = nullptr;
    SDL_GPUTransferBuffer *transferBuffer = nullptr;
    uint32_t bufferSize = 0;
};

} // namespace Lucky
