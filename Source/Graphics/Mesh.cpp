#include <SDL3/SDL_assert.h>

#include <Lucky/Mesh.hpp>

namespace Lucky {

Mesh::Mesh(GraphicsDevice &graphicsDevice, const Vertex3D *vertices, uint32_t vertexCount,
    const uint32_t *indices, uint32_t indexCount)
    : vertexBuffer(graphicsDevice, vertices, vertexCount),
      indexBuffer(graphicsDevice, indices, indexCount), vertexCount(vertexCount),
      indexCount(indexCount) {
}

Mesh::Mesh(GraphicsDevice &graphicsDevice, const MeshData &data)
    : Mesh(graphicsDevice, data.vertices.data(), static_cast<uint32_t>(data.vertices.size()),
          data.indices.data(), static_cast<uint32_t>(data.indices.size())) {
    SDL_assert(!data.vertices.empty());
    SDL_assert(!data.indices.empty());
}

MeshData MakeCubeMeshData(float size) {
    SDL_assert(size > 0.0f);
    const float h = size * 0.5f;

    // Six faces, each with four vertices having the face's outward normal
    // and corner UVs in [0, 1]. Two CCW triangles per face.
    MeshData data;
    data.vertices.reserve(24);
    data.indices.reserve(36);

    auto addFace = [&](float nx,
                       float ny,
                       float nz,
                       float p0x,
                       float p0y,
                       float p0z,
                       float p1x,
                       float p1y,
                       float p1z,
                       float p2x,
                       float p2y,
                       float p2z,
                       float p3x,
                       float p3y,
                       float p3z) {
        const uint32_t base = static_cast<uint32_t>(data.vertices.size());
        data.vertices.push_back({p0x, p0y, p0z, 0.0f, 0.0f, nx, ny, nz});
        data.vertices.push_back({p1x, p1y, p1z, 1.0f, 0.0f, nx, ny, nz});
        data.vertices.push_back({p2x, p2y, p2z, 1.0f, 1.0f, nx, ny, nz});
        data.vertices.push_back({p3x, p3y, p3z, 0.0f, 1.0f, nx, ny, nz});
        data.indices.push_back(base + 0);
        data.indices.push_back(base + 1);
        data.indices.push_back(base + 2);
        data.indices.push_back(base + 0);
        data.indices.push_back(base + 2);
        data.indices.push_back(base + 3);
    };

    // +X face: looking from outside, CCW winding
    addFace(1, 0, 0, h, -h, h, h, -h, -h, h, h, -h, h, h, h);
    // -X face
    addFace(-1, 0, 0, -h, -h, -h, -h, -h, h, -h, h, h, -h, h, -h);
    // +Y face (top)
    addFace(0, 1, 0, -h, h, h, h, h, h, h, h, -h, -h, h, -h);
    // -Y face (bottom)
    addFace(0, -1, 0, -h, -h, -h, h, -h, -h, h, -h, h, -h, -h, h);
    // +Z face
    addFace(0, 0, 1, -h, -h, h, h, -h, h, h, h, h, -h, h, h);
    // -Z face
    addFace(0, 0, -1, h, -h, -h, -h, -h, -h, -h, h, -h, h, h, -h);

    return data;
}

MeshData MakePlaneMeshData(float size) {
    SDL_assert(size > 0.0f);
    const float h = size * 0.5f;

    MeshData data;
    data.vertices = {
        {-h, 0.0f, h, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f},
        {h, 0.0f, h, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f},
        {h, 0.0f, -h, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f},
        {-h, 0.0f, -h, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f},
    };
    data.indices = {0, 1, 2, 0, 2, 3};
    return data;
}

} // namespace Lucky
