#include <SDL3/SDL_assert.h>

#include <glm/glm.hpp>

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

MeshData MakeBoxMeshData(float width, float height, float depth) {
    SDL_assert(width > 0.0f);
    SDL_assert(height > 0.0f);
    SDL_assert(depth > 0.0f);
    const float hx = width * 0.5f;
    const float hy = height * 0.5f;
    const float hz = depth * 0.5f;

    // Six faces, each with four vertices having the face's outward normal
    // and corner UVs in [0, 1]. Two CCW triangles per face.
    MeshData data;
    data.vertices.reserve(24);
    data.indices.reserve(36);

    // Tangents are zeroed: generated primitives don't carry normal-mapped
    // materials in any current demo, so the fragment shader's
    // HasNormalTexture flag will keep the shader from reading them.
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
        data.vertices.push_back({p0x, p0y, p0z, 0.0f, 0.0f, nx, ny, nz, 0, 0, 0, 0});
        data.vertices.push_back({p1x, p1y, p1z, 1.0f, 0.0f, nx, ny, nz, 0, 0, 0, 0});
        data.vertices.push_back({p2x, p2y, p2z, 1.0f, 1.0f, nx, ny, nz, 0, 0, 0, 0});
        data.vertices.push_back({p3x, p3y, p3z, 0.0f, 1.0f, nx, ny, nz, 0, 0, 0, 0});
        data.indices.push_back(base + 0);
        data.indices.push_back(base + 1);
        data.indices.push_back(base + 2);
        data.indices.push_back(base + 0);
        data.indices.push_back(base + 2);
        data.indices.push_back(base + 3);
    };

    // +X face: looking from outside, CCW winding
    addFace(1, 0, 0, hx, -hy, hz, hx, -hy, -hz, hx, hy, -hz, hx, hy, hz);
    // -X face
    addFace(-1, 0, 0, -hx, -hy, -hz, -hx, -hy, hz, -hx, hy, hz, -hx, hy, -hz);
    // +Y face (top)
    addFace(0, 1, 0, -hx, hy, hz, hx, hy, hz, hx, hy, -hz, -hx, hy, -hz);
    // -Y face (bottom)
    addFace(0, -1, 0, -hx, -hy, -hz, hx, -hy, -hz, hx, -hy, hz, -hx, -hy, hz);
    // +Z face
    addFace(0, 0, 1, -hx, -hy, hz, hx, -hy, hz, hx, hy, hz, -hx, hy, hz);
    // -Z face
    addFace(0, 0, -1, hx, -hy, -hz, -hx, -hy, -hz, -hx, hy, -hz, hx, hy, -hz);

    return data;
}

MeshData MakeCubeMeshData(float size) {
    return MakeBoxMeshData(size, size, size);
}

MeshData MakeDiamondMeshData(float radius, float height) {
    SDL_assert(radius > 0.0f);
    SDL_assert(height > 0.0f);

    const glm::vec3 top(0.0f, height, 0.0f);
    const glm::vec3 bot(0.0f, -height, 0.0f);
    const glm::vec3 eq[4] = {
        glm::vec3(radius, 0.0f, 0.0f),
        glm::vec3(0.0f, 0.0f, radius),
        glm::vec3(-radius, 0.0f, 0.0f),
        glm::vec3(0.0f, 0.0f, -radius),
    };

    MeshData data;
    data.vertices.reserve(24);
    data.indices.reserve(24);

    auto addFace = [&](const glm::vec3 &a, const glm::vec3 &b, const glm::vec3 &c) {
        const glm::vec3 normal = glm::normalize(glm::cross(b - a, c - a));
        const uint32_t base = static_cast<uint32_t>(data.vertices.size());
        const Vertex3D v0{a.x, a.y, a.z, 0.0f, 1.0f, normal.x, normal.y, normal.z, 0, 0, 0, 0};
        const Vertex3D v1{b.x, b.y, b.z, 1.0f, 0.0f, normal.x, normal.y, normal.z, 0, 0, 0, 0};
        const Vertex3D v2{c.x, c.y, c.z, 0.0f, 0.0f, normal.x, normal.y, normal.z, 0, 0, 0, 0};
        data.vertices.push_back(v0);
        data.vertices.push_back(v1);
        data.vertices.push_back(v2);
        data.indices.push_back(base + 0);
        data.indices.push_back(base + 1);
        data.indices.push_back(base + 2);
    };

    for (int i = 0; i < 4; i++) {
        const glm::vec3 &p0 = eq[i];
        const glm::vec3 &p1 = eq[(i + 1) % 4];
        addFace(top, p0, p1);
        addFace(bot, p1, p0);
    }

    return data;
}

MeshData MakePlaneMeshData(float size) {
    SDL_assert(size > 0.0f);
    const float h = size * 0.5f;

    MeshData data;
    data.vertices = {
        {-h, 0.0f, h, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0, 0, 0, 0},
        {h, 0.0f, h, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0, 0, 0, 0},
        {h, 0.0f, -h, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0, 0, 0, 0},
        {-h, 0.0f, -h, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0, 0, 0, 0},
    };
    data.indices = {0, 1, 2, 0, 2, 3};
    return data;
}

} // namespace Lucky
