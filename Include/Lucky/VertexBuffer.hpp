#pragma once

#include <stdint.h>
#include <string.h>

#include <SDL3/SDL_assert.h>
#include <SDL3/SDL_gpu.h>
#include <Lucky/GraphicsDevice.hpp>

namespace Lucky {

enum class VertexBufferType {
    Static,
    Dynamic,
};

template <typename VertexType> struct VertexBuffer {
  public:
    VertexBuffer(
        GraphicsDevice &graphicsDevice, VertexBufferType vertexBufferType, uint32_t maximumVertices)
        : graphicsDevice(&graphicsDevice) {
        SDL_assert(maximumVertices > 0);

        bufferSize = maximumVertices * sizeof(VertexType);

        SDL_GPUBufferCreateInfo bufCI;
        SDL_zero(bufCI);
        bufCI.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
        bufCI.size = bufferSize;

        gpuBuffer = SDL_CreateGPUBuffer(graphicsDevice.GetDevice(), &bufCI);
        SDL_assert(gpuBuffer);

        SDL_GPUTransferBufferCreateInfo tbCI;
        SDL_zero(tbCI);
        tbCI.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        tbCI.size = bufferSize;

        transferBuffer = SDL_CreateGPUTransferBuffer(graphicsDevice.GetDevice(), &tbCI);
        SDL_assert(transferBuffer);
    }

    VertexBuffer(const VertexBuffer &) = delete;
    VertexBuffer &operator=(const VertexBuffer &) = delete;

    ~VertexBuffer() {
        if (transferBuffer) {
            SDL_ReleaseGPUTransferBuffer(graphicsDevice->GetDevice(), transferBuffer);
        }
        if (gpuBuffer) {
            SDL_ReleaseGPUBuffer(graphicsDevice->GetDevice(), gpuBuffer);
        }
    }

    void SetVertexData(VertexType *vertices, uint32_t vertexCount) {
        SDL_assert(vertices != nullptr);
        SDL_assert(vertexCount > 0);
        SDL_assert(vertexCount * sizeof(VertexType) <= bufferSize);

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

    SDL_GPUBuffer *GetGPUBuffer() const {
        return gpuBuffer;
    }

  private:
    GraphicsDevice *graphicsDevice;
    SDL_GPUBuffer *gpuBuffer = nullptr;
    SDL_GPUTransferBuffer *transferBuffer = nullptr;
    uint32_t bufferSize;
};

} // namespace Lucky
