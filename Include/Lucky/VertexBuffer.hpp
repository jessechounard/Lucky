#pragma once

#include <stdint.h>
#include <string.h>

#include <SDL3/SDL_assert.h>
#include <SDL3/SDL_gpu.h>
#include <Lucky/GraphicsDevice.hpp>

namespace Lucky {

/**
 * A typed wrapper around an SDL_GPUBuffer holding a vertex array.
 *
 * Supports two constructions: dynamic (re-uploadable each frame) and
 * static (uploaded once at construction, never again). The static form
 * releases its transfer buffer immediately after upload, saving the
 * staging memory equal to `vertexCount * sizeof(VertexType)`.
 *
 * # Choosing static vs dynamic
 *
 * - Use **dynamic** when the vertices change per frame: BatchRenderer,
 *   SlugRenderer, particle systems, anything that re-batches geometry on
 *   the CPU.
 * - Use **static** when the vertices are fixed for the lifetime of the
 *   buffer: 3D model meshes, fullscreen post-process quads, baked tile
 *   maps, anything uploaded once and just re-bound.
 *
 * # Lifetime
 *
 * Holds a pointer to the GraphicsDevice. The GraphicsDevice must outlive
 * this buffer.
 */
template <typename VertexType> struct VertexBuffer {
    /**
     * Constructs a dynamic vertex buffer with `maximumVertices` of capacity.
     *
     * Allocates both the GPU buffer and a same-size transfer buffer used
     * for subsequent SetVertexData calls.
     *
     * \param graphicsDevice the graphics device. Must outlive this buffer.
     * \param maximumVertices upper bound on the vertex count that
     *                        SetVertexData may upload. Must be positive.
     */
    VertexBuffer(GraphicsDevice &graphicsDevice, uint32_t maximumVertices)
        : graphicsDevice(&graphicsDevice) {
        SDL_assert(maximumVertices > 0);

        bufferSize = maximumVertices * sizeof(VertexType);
        CreateGPUBuffer();
        CreateTransferBuffer();
    }

    /**
     * Constructs a static vertex buffer pre-populated with `vertices`.
     *
     * Allocates the GPU buffer, uploads `vertexCount` vertices through a
     * temporary transfer buffer, then releases the transfer buffer. The
     * resulting buffer cannot be modified -- calling SetVertexData on it
     * asserts.
     *
     * \param graphicsDevice the graphics device. Must outlive this buffer.
     * \param vertices vertex data to upload. Must not be null.
     * \param vertexCount number of vertices in `vertices`. Must be positive.
     */
    VertexBuffer(GraphicsDevice &graphicsDevice, const VertexType *vertices, uint32_t vertexCount)
        : graphicsDevice(&graphicsDevice) {
        SDL_assert(vertices != nullptr);
        SDL_assert(vertexCount > 0);

        bufferSize = vertexCount * sizeof(VertexType);
        CreateGPUBuffer();
        CreateTransferBuffer();
        Upload(vertices, vertexCount);
        ReleaseTransferBuffer();
    }

    VertexBuffer(const VertexBuffer &) = delete;
    VertexBuffer &operator=(const VertexBuffer &) = delete;
    VertexBuffer(VertexBuffer &&) = delete;
    VertexBuffer &operator=(VertexBuffer &&) = delete;

    ~VertexBuffer() {
        if (transferBuffer) {
            SDL_ReleaseGPUTransferBuffer(graphicsDevice->GetDevice(), transferBuffer);
        }
        if (gpuBuffer) {
            SDL_ReleaseGPUBuffer(graphicsDevice->GetDevice(), gpuBuffer);
        }
    }

    /**
     * Re-uploads vertex data to a dynamic buffer.
     *
     * Asserts in debug if called on a static buffer (no transfer buffer is
     * available). Asserts that `vertexCount * sizeof(VertexType)` fits
     * inside the buffer's allocated capacity.
     *
     * Internally calls BeginCopyPass / EndCopyPass on the GraphicsDevice,
     * which auto-ends any active render pass. Avoid calling mid-frame
     * unless that side effect is acceptable.
     *
     * \param vertices vertex data to upload. Must not be null.
     * \param vertexCount number of vertices to upload. Must be positive
     *                    and fit within the buffer's capacity.
     */
    void SetVertexData(const VertexType *vertices, uint32_t vertexCount) {
        SDL_assert(transferBuffer != nullptr);
        SDL_assert(vertices != nullptr);
        SDL_assert(vertexCount > 0);
        SDL_assert(vertexCount * sizeof(VertexType) <= bufferSize);

        Upload(vertices, vertexCount);
    }

    /**
     * Returns the underlying SDL_GPUBuffer handle for use in pipeline
     * binding.
     */
    SDL_GPUBuffer *GetGPUBuffer() const {
        return gpuBuffer;
    }

  private:
    void CreateGPUBuffer() {
        SDL_GPUBufferCreateInfo bufCI;
        SDL_zero(bufCI);
        bufCI.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
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

    void Upload(const VertexType *vertices, uint32_t vertexCount) {
        uint32_t dataSize = vertexCount * sizeof(VertexType);

        void *mapped = SDL_MapGPUTransferBuffer(graphicsDevice->GetDevice(), transferBuffer, true);
        memcpy(mapped, vertices, dataSize);
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

    GraphicsDevice *graphicsDevice;
    SDL_GPUBuffer *gpuBuffer = nullptr;
    SDL_GPUTransferBuffer *transferBuffer = nullptr;
    uint32_t bufferSize = 0;
};

} // namespace Lucky
