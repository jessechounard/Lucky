#include <SDL3/SDL_assert.h>

#include <Lucky/SkinnedMesh.hpp>

namespace Lucky {

SkinnedMesh::SkinnedMesh(GraphicsDevice &graphicsDevice, const Vertex3DSkinned *vertices,
    uint32_t vertexCount, const uint32_t *indices, uint32_t indexCount)
    : vertexBuffer(graphicsDevice, vertices, vertexCount),
      indexBuffer(graphicsDevice, indices, indexCount), vertexCount(vertexCount),
      indexCount(indexCount) {
}

SkinnedMesh::SkinnedMesh(GraphicsDevice &graphicsDevice, const SkinnedMeshData &data)
    : SkinnedMesh(graphicsDevice, data.vertices.data(), static_cast<uint32_t>(data.vertices.size()),
          data.indices.data(), static_cast<uint32_t>(data.indices.size())) {
    SDL_assert(!data.vertices.empty());
    SDL_assert(!data.indices.empty());
}

} // namespace Lucky
