#pragma once

#include <stdint.h>
#include <vector>

#include <Lucky/IndexBuffer.hpp>
#include <Lucky/VertexBuffer.hpp>

namespace Lucky {

struct GraphicsDevice;

/**
 * The vertex layout used by Lucky's 3D mesh path: position, UV, normal.
 *
 * 32 bytes, packed in declaration order: 3 floats position, 2 floats UV,
 * 3 floats normal. Matches what the forward renderer expects in its
 * vertex input state.
 */
struct Vertex3D {
    float x, y, z;    /**< position in mesh-local space. */
    float u, v;       /**< texture coordinates, normalized. */
    float nx, ny, nz; /**< unit normal in mesh-local space. */
};

/**
 * CPU-side mesh data: a vertex array plus a 32-bit index array.
 *
 * Returned by the `Make*MeshData` generators so callers can inspect or
 * transform the geometry before uploading. To upload, hand the contained
 * arrays to a `Mesh` constructor.
 */
struct MeshData {
    std::vector<Vertex3D> vertices;
    std::vector<uint32_t> indices;
};

/**
 * Generates an axis-aligned cube centered at the origin.
 *
 * Each face has its own four vertices so face normals are exact (sharing
 * vertices between faces would average normals into something useless for
 * a cube). Produces 24 vertices and 36 indices.
 *
 * \param size edge length. Default 1.0; positive only.
 */
MeshData MakeCubeMeshData(float size = 1.0f);

/**
 * Generates an axis-aligned box centered at the origin with separate
 * extents per axis.
 *
 * Same per-face vertex layout as `MakeCubeMeshData` (24 vertices, 36
 * indices). Prefer this over scaling a cube non-uniformly: a non-uniform
 * scale on the model matrix breaks normals (the renderer transforms
 * normals by the model upper-3x3, which is only correct for rotations
 * and uniform scales).
 *
 * \param width  X extent. Positive.
 * \param height Y extent. Positive.
 * \param depth  Z extent. Positive.
 */
MeshData MakeBoxMeshData(float width, float height, float depth);

/**
 * Generates a flat XZ plane centered at the origin, with the normal
 * pointing along +Y.
 *
 * Useful as a ground plane in test scenes. Produces 4 vertices and 6
 * indices. UVs span [0, 1].
 *
 * \param size edge length. Default 1.0; positive only.
 */
MeshData MakePlaneMeshData(float size = 1.0f);

/**
 * Generates a vertically-elongated octahedron (a gemstone-like diamond
 * shape) centered at the origin.
 *
 * Eight triangular faces meet at the top and bottom apexes, with four
 * equatorial vertices forming a square waist on the XZ plane. Each
 * face has its own three vertices so face normals are exact and the
 * lit result is sharply faceted -- the look most people imagine for
 * "spinning diamond." Produces 24 vertices and 24 indices.
 *
 * \param radius distance from the Y axis to the equator. Positive.
 * \param height distance from the origin to each apex. Positive.
 */
MeshData MakeDiamondMeshData(float radius = 1.0f, float height = 1.5f);

/**
 * GPU-resident mesh: paired vertex and index buffers, both static.
 *
 * Uploads the supplied geometry once at construction and holds onto the
 * GPU buffers for the lifetime of the object. Index format is fixed at
 * 32 bits so meshes from different sources (generated primitives,
 * cgltf-loaded models) share a single binding path.
 *
 * # Lifetime
 *
 * Holds a reference to the `GraphicsDevice` through its internal buffers.
 * The `GraphicsDevice` must outlive this Mesh.
 *
 * # Non-movable
 *
 * Like `Texture` and the buffer wrappers, Mesh is non-copyable and
 * non-movable. Wrap in `std::unique_ptr<Mesh>` if you need to store
 * meshes in containers or transfer ownership.
 */
struct Mesh {
    /**
     * Uploads `vertices` and `indices` to GPU buffers.
     *
     * \param graphicsDevice the graphics device that owns the GPU resources.
     *                       Must outlive this Mesh.
     * \param vertices vertex array. Must not be null. Must contain at
     *                 least `vertexCount` entries.
     * \param vertexCount number of vertices. Must be positive.
     * \param indices index array, in 32-bit format. Must not be null.
     *                Must contain at least `indexCount` entries.
     * \param indexCount number of indices. Must be positive.
     */
    Mesh(GraphicsDevice &graphicsDevice, const Vertex3D *vertices, uint32_t vertexCount,
        const uint32_t *indices, uint32_t indexCount);

    /**
     * Convenience overload: uploads the contents of a `MeshData` directly.
     */
    Mesh(GraphicsDevice &graphicsDevice, const MeshData &data);

    Mesh(const Mesh &) = delete;
    Mesh &operator=(const Mesh &) = delete;
    Mesh(Mesh &&) = delete;
    Mesh &operator=(Mesh &&) = delete;

    ~Mesh() = default;

    /**
     * Returns the vertex GPU buffer handle for binding via
     * `SDL_BindGPUVertexBuffers`.
     */
    SDL_GPUBuffer *GetVertexBuffer() const {
        return vertexBuffer.GetGPUBuffer();
    }

    /**
     * Returns the index GPU buffer handle for binding via
     * `SDL_BindGPUIndexBuffer`. The element size is always 32-bit.
     */
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
    VertexBuffer<Vertex3D> vertexBuffer;
    IndexBuffer<uint32_t> indexBuffer;
    uint32_t vertexCount;
    uint32_t indexCount;
};

} // namespace Lucky
