#include <doctest/doctest.h>

#include <Lucky/Mesh.hpp>

using namespace Lucky;

TEST_CASE("Vertex3D is 48 bytes packed in declaration order") {
    CHECK(sizeof(Vertex3D) == 48);
    Vertex3D v{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
    CHECK(v.x == 1);
    CHECK(v.y == 2);
    CHECK(v.z == 3);
    CHECK(v.u == 4);
    CHECK(v.v == 5);
    CHECK(v.nx == 6);
    CHECK(v.ny == 7);
    CHECK(v.nz == 8);
    CHECK(v.tx == 9);
    CHECK(v.ty == 10);
    CHECK(v.tz == 11);
    CHECK(v.tw == 12);
}

TEST_CASE("MakeCubeMeshData returns 24 vertices and 36 indices") {
    MeshData data = MakeCubeMeshData();
    CHECK(data.vertices.size() == 24);
    CHECK(data.indices.size() == 36);
}

TEST_CASE("MakeCubeMeshData scales vertices by half-size on each axis") {
    MeshData data = MakeCubeMeshData(2.0f);
    for (const Vertex3D &v : data.vertices) {
        CHECK(std::abs(v.x) == doctest::Approx(1.0f));
        CHECK(std::abs(v.y) == doctest::Approx(1.0f));
        CHECK(std::abs(v.z) == doctest::Approx(1.0f));
    }
}

TEST_CASE("MakeCubeMeshData faces have unit normals matching axis directions") {
    MeshData data = MakeCubeMeshData();
    // The six face normals should each cover one axis direction; group
    // vertices by their face (every 4 consecutive share a normal) and
    // confirm the magnitude is 1 and components are ±1 / 0.
    for (size_t i = 0; i < data.vertices.size(); i += 4) {
        const Vertex3D &v = data.vertices[i];
        const float mag = v.nx * v.nx + v.ny * v.ny + v.nz * v.nz;
        CHECK(mag == doctest::Approx(1.0f));
        for (size_t j = i + 1; j < i + 4; j++) {
            CHECK(data.vertices[j].nx == v.nx);
            CHECK(data.vertices[j].ny == v.ny);
            CHECK(data.vertices[j].nz == v.nz);
        }
    }
}

TEST_CASE("MakePlaneMeshData returns 4 vertices and 6 indices, all +Y normal") {
    MeshData data = MakePlaneMeshData(2.0f);
    CHECK(data.vertices.size() == 4);
    CHECK(data.indices.size() == 6);
    for (const Vertex3D &v : data.vertices) {
        CHECK(v.y == doctest::Approx(0.0f));
        CHECK(v.nx == doctest::Approx(0.0f));
        CHECK(v.ny == doctest::Approx(1.0f));
        CHECK(v.nz == doctest::Approx(0.0f));
    }
}
