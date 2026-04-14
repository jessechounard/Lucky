#include <doctest/doctest.h>

#include <glm/glm.hpp>

#include <Lucky/MathConstants.hpp>
#include <Lucky/ShapeRenderer.hpp>

using namespace Lucky;

TEST_CASE("GetRectangleVertices returns corners at rotation zero") {
    auto verts = ShapeRenderer::GetRectangleVertices({0.0f, 0.0f}, {4.0f, 2.0f}, 0.0f);
    REQUIRE(verts.size() == 4);

    // Counter-clockwise from bottom-left.
    CHECK(verts[0].x == doctest::Approx(-2.0f));
    CHECK(verts[0].y == doctest::Approx(-1.0f));
    CHECK(verts[1].x == doctest::Approx(2.0f));
    CHECK(verts[1].y == doctest::Approx(-1.0f));
    CHECK(verts[2].x == doctest::Approx(2.0f));
    CHECK(verts[2].y == doctest::Approx(1.0f));
    CHECK(verts[3].x == doctest::Approx(-2.0f));
    CHECK(verts[3].y == doctest::Approx(1.0f));
}

TEST_CASE("GetRectangleVertices rotates 90 degrees counter-clockwise") {
    auto verts = ShapeRenderer::GetRectangleVertices(
        {0.0f, 0.0f}, {2.0f, 2.0f}, MathConstants::Pi / 2.0f);
    REQUIRE(verts.size() == 4);

    // A unit square rotated 90 degrees CCW: each vertex ends up rotated.
    CHECK(verts[0].x == doctest::Approx(1.0f));
    CHECK(verts[0].y == doctest::Approx(-1.0f));
    CHECK(verts[1].x == doctest::Approx(1.0f));
    CHECK(verts[1].y == doctest::Approx(1.0f));
    CHECK(verts[2].x == doctest::Approx(-1.0f));
    CHECK(verts[2].y == doctest::Approx(1.0f));
    CHECK(verts[3].x == doctest::Approx(-1.0f));
    CHECK(verts[3].y == doctest::Approx(-1.0f));
}

TEST_CASE("GetRectangleVertices respects the center offset") {
    auto verts = ShapeRenderer::GetRectangleVertices({10.0f, 20.0f}, {2.0f, 2.0f}, 0.0f);
    REQUIRE(verts.size() == 4);
    CHECK(verts[0].x == doctest::Approx(9.0f));
    CHECK(verts[0].y == doctest::Approx(19.0f));
    CHECK(verts[2].x == doctest::Approx(11.0f));
    CHECK(verts[2].y == doctest::Approx(21.0f));
}

TEST_CASE("GetTriangleVertices returns the input at rotation zero") {
    glm::vec2 p0{0.0f, 1.0f}, p1{-1.0f, -1.0f}, p2{1.0f, -1.0f};
    auto verts = ShapeRenderer::GetTriangleVertices(p0, p1, p2, 0.0f);

    REQUIRE(verts.size() == 3);
    CHECK(verts[0].x == doctest::Approx(p0.x));
    CHECK(verts[0].y == doctest::Approx(p0.y));
    CHECK(verts[1].x == doctest::Approx(p1.x));
    CHECK(verts[1].y == doctest::Approx(p1.y));
    CHECK(verts[2].x == doctest::Approx(p2.x));
    CHECK(verts[2].y == doctest::Approx(p2.y));
}

TEST_CASE("GetTriangleVertices rotates around centroid, not origin") {
    // An off-origin triangle whose centroid is (10, 0). Rotating 180 degrees
    // should swap each vertex across the centroid, not across the origin.
    glm::vec2 p0{11.0f, 0.0f};
    glm::vec2 p1{9.5f, 0.866f};
    glm::vec2 p2{9.5f, -0.866f};
    glm::vec2 centroid = (p0 + p1 + p2) / 3.0f;

    auto verts = ShapeRenderer::GetTriangleVertices(p0, p1, p2, MathConstants::Pi);
    REQUIRE(verts.size() == 3);

    // Each rotated vertex should be the reflection of the original through
    // the centroid.
    for (size_t i = 0; i < 3; i++) {
        glm::vec2 original = (i == 0) ? p0 : (i == 1) ? p1 : p2;
        glm::vec2 expected = 2.0f * centroid - original;
        CHECK(verts[i].x == doctest::Approx(expected.x));
        CHECK(verts[i].y == doctest::Approx(expected.y));
    }
}

TEST_CASE("GetDiamondVertices returns four vertices along the axes") {
    auto verts = ShapeRenderer::GetDiamondVertices({0.0f, 0.0f}, 4.0f, 2.0f, 0.0f);
    REQUIRE(verts.size() == 4);

    // At rotation zero: +X, +Y, -X, -Y (halfWidth, halfHeight).
    CHECK(verts[0].x == doctest::Approx(2.0f));
    CHECK(verts[0].y == doctest::Approx(0.0f));
    CHECK(verts[1].x == doctest::Approx(0.0f));
    CHECK(verts[1].y == doctest::Approx(1.0f));
    CHECK(verts[2].x == doctest::Approx(-2.0f));
    CHECK(verts[2].y == doctest::Approx(0.0f));
    CHECK(verts[3].x == doctest::Approx(0.0f));
    CHECK(verts[3].y == doctest::Approx(-1.0f));
}

TEST_CASE("GetRegularPolygonVertices produces the right count") {
    auto hex = ShapeRenderer::GetRegularPolygonVertices({0.0f, 0.0f}, 1.0f, 6, 0.0f);
    CHECK(hex.size() == 6);

    auto tri = ShapeRenderer::GetRegularPolygonVertices({0.0f, 0.0f}, 1.0f, 3, 0.0f);
    CHECK(tri.size() == 3);

    auto octagon = ShapeRenderer::GetRegularPolygonVertices({0.0f, 0.0f}, 1.0f, 8, 0.0f);
    CHECK(octagon.size() == 8);
}

TEST_CASE("GetRegularPolygonVertices places vertices on the circle") {
    const float radius = 3.0f;
    auto verts = ShapeRenderer::GetRegularPolygonVertices({0.0f, 0.0f}, radius, 5, 0.0f);

    for (const auto &v : verts) {
        CHECK(glm::length(v) == doctest::Approx(radius));
    }
}

TEST_CASE("GetRegularPolygonVertices first vertex points along +Y at rotation zero") {
    auto verts = ShapeRenderer::GetRegularPolygonVertices({0.0f, 0.0f}, 1.0f, 6, 0.0f);
    REQUIRE(verts.size() == 6);
    CHECK(verts[0].x == doctest::Approx(0.0f));
    CHECK(verts[0].y == doctest::Approx(1.0f));
}

TEST_CASE("GetStarVertices produces points*2 vertices") {
    auto verts = ShapeRenderer::GetStarVertices({0.0f, 0.0f}, 2.0f, 1.0f, 5, 0.0f);
    CHECK(verts.size() == 10);

    auto four = ShapeRenderer::GetStarVertices({0.0f, 0.0f}, 2.0f, 1.0f, 4, 0.0f);
    CHECK(four.size() == 8);
}

TEST_CASE("GetStarVertices alternates outer and inner radii") {
    const float outer = 3.0f;
    const float inner = 1.5f;
    auto verts = ShapeRenderer::GetStarVertices({0.0f, 0.0f}, outer, inner, 5, 0.0f);
    REQUIRE(verts.size() == 10);

    for (size_t i = 0; i < verts.size(); i++) {
        float expected = (i % 2 == 0) ? outer : inner;
        CHECK(glm::length(verts[i]) == doctest::Approx(expected));
    }
}

TEST_CASE("GetStarVertices first outer point aims along +Y at rotation zero") {
    auto verts = ShapeRenderer::GetStarVertices({0.0f, 0.0f}, 1.0f, 0.5f, 5, 0.0f);
    REQUIRE(verts.size() == 10);
    CHECK(verts[0].x == doctest::Approx(0.0f));
    CHECK(verts[0].y == doctest::Approx(1.0f));
}
