#pragma once

#include <stdint.h>
#include <vector>

#include <Lucky/IndexBuffer.hpp>
#include <Lucky/VertexBuffer.hpp>

namespace Lucky {

struct GraphicsDevice;

/**
 * Vertex layout used by the skinned forward shader: position, UV,
 * normal, joint indices, and joint weights.
 *
 * 64 bytes, packed in declaration order: 3 floats position, 2 floats
 * UV, 3 floats normal, 4 uints joints, 4 floats weights.
 *
 * Each vertex names up to four joints (bones) that influence it, with
 * a weight per joint summing to 1.0. The vertex shader looks up four
 * joint matrices from the per-frame UBO and blends them by weight.
 */
struct Vertex3DSkinned {
    float x, y, z;           /**< position in mesh-local space. */
    float u, v;              /**< texture coordinates, normalized. */
    float nx, ny, nz;        /**< unit normal in mesh-local space. */
    uint32_t j0, j1, j2, j3; /**< joint indices into the model's skin. */
    float w0, w1, w2, w3;    /**< joint weights; should sum to 1.0. */
};

/**
 * CPU-side skinned mesh data: vertex array + 32-bit index array.
 *
 * Mirrors `MeshData` but with `Vertex3DSkinned` for the skinning path.
 * Returned by glTF loading when a primitive declares `JOINTS_0` and
 * `WEIGHTS_0` attributes.
 */
struct SkinnedMeshData {
    std::vector<Vertex3DSkinned> vertices;
    std::vector<uint32_t> indices;
};

/**
 * GPU-resident skinned mesh: paired vertex and index buffers, both
 * static.
 *
 * Mirrors `Mesh` but stores `Vertex3DSkinned` in the vertex buffer.
 * The renderer routes it through the skinned forward pipeline.
 *
 * # Lifetime
 *
 * Holds a reference to the `GraphicsDevice` through its internal
 * buffers. The `GraphicsDevice` must outlive this SkinnedMesh.
 *
 * # Non-movable
 *
 * Like `Mesh`, this is non-copyable and non-movable. Wrap in
 * `std::unique_ptr<SkinnedMesh>` to store in containers.
 */
struct SkinnedMesh {
    /**
     * Uploads `vertices` and `indices` to GPU buffers.
     *
     * \param graphicsDevice the graphics device that owns the GPU
     *                       resources. Must outlive this SkinnedMesh.
     * \param vertices vertex array. Must not be null. Must contain at
     *                 least `vertexCount` entries.
     * \param vertexCount number of vertices. Must be positive.
     * \param indices index array, in 32-bit format. Must not be null.
     *                Must contain at least `indexCount` entries.
     * \param indexCount number of indices. Must be positive.
     */
    SkinnedMesh(GraphicsDevice &graphicsDevice, const Vertex3DSkinned *vertices,
        uint32_t vertexCount, const uint32_t *indices, uint32_t indexCount);

    /**
     * Convenience: uploads the contents of a `SkinnedMeshData` directly.
     */
    SkinnedMesh(GraphicsDevice &graphicsDevice, const SkinnedMeshData &data);

    SkinnedMesh(const SkinnedMesh &) = delete;
    SkinnedMesh &operator=(const SkinnedMesh &) = delete;
    SkinnedMesh(SkinnedMesh &&) = delete;
    SkinnedMesh &operator=(SkinnedMesh &&) = delete;

    ~SkinnedMesh() = default;

    SDL_GPUBuffer *GetVertexBuffer() const {
        return vertexBuffer.GetGPUBuffer();
    }

    SDL_GPUBuffer *GetIndexBuffer() const {
        return indexBuffer.GetGPUBuffer();
    }

    uint32_t GetVertexCount() const {
        return vertexCount;
    }

    uint32_t GetIndexCount() const {
        return indexCount;
    }

  private:
    VertexBuffer<Vertex3DSkinned> vertexBuffer;
    IndexBuffer<uint32_t> indexBuffer;
    uint32_t vertexCount;
    uint32_t indexCount;
};

} // namespace Lucky
