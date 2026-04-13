#include <doctest/doctest.h>

#include <glm/glm.hpp>

#include <Lucky/Collision.hpp>

using namespace Lucky;

namespace {

// Axis-aligned square centered at the origin with the given half-extent.
Collider MakeSquare(float halfExtent) {
    return MakeCollider({
        {-halfExtent, -halfExtent},
        {halfExtent, -halfExtent},
        {halfExtent, halfExtent},
        {-halfExtent, halfExtent},
    });
}

} // namespace

TEST_CASE("CalculateProjectedInterval projects a square onto the X axis") {
    std::vector<glm::vec2> square = {
        {-1.0f, -1.0f}, {1.0f, -1.0f}, {1.0f, 1.0f}, {-1.0f, 1.0f}};
    float min, max;

    CalculateProjectedInterval(square, glm::vec2{1.0f, 0.0f}, min, max);
    CHECK(min == doctest::Approx(-1.0f));
    CHECK(max == doctest::Approx(1.0f));
}

TEST_CASE("CalculateProjectedInterval projects a square onto the Y axis") {
    std::vector<glm::vec2> square = {
        {-1.0f, -1.0f}, {1.0f, -1.0f}, {1.0f, 1.0f}, {-1.0f, 1.0f}};
    float min, max;

    CalculateProjectedInterval(square, glm::vec2{0.0f, 1.0f}, min, max);
    CHECK(min == doctest::Approx(-1.0f));
    CHECK(max == doctest::Approx(1.0f));
}

TEST_CASE("CalculateProjectedInterval handles non-axis-aligned triangles") {
    std::vector<glm::vec2> triangle = {{0.0f, 0.0f}, {3.0f, 0.0f}, {0.0f, 4.0f}};
    float min, max;

    CalculateProjectedInterval(triangle, glm::vec2{1.0f, 0.0f}, min, max);
    CHECK(min == doctest::Approx(0.0f));
    CHECK(max == doctest::Approx(3.0f));

    CalculateProjectedInterval(triangle, glm::vec2{0.0f, 1.0f}, min, max);
    CHECK(min == doctest::Approx(0.0f));
    CHECK(max == doctest::Approx(4.0f));
}

TEST_CASE("CalculateProjectedInterval with offset translates before projecting") {
    std::vector<glm::vec2> square = {
        {-1.0f, -1.0f}, {1.0f, -1.0f}, {1.0f, 1.0f}, {-1.0f, 1.0f}};
    float min, max;

    CalculateProjectedInterval(square, glm::vec2{5.0f, 0.0f}, glm::vec2{1.0f, 0.0f}, min, max);
    CHECK(min == doctest::Approx(4.0f));
    CHECK(max == doctest::Approx(6.0f));

    CalculateProjectedInterval(square, glm::vec2{0.0f, 3.0f}, glm::vec2{0.0f, 1.0f}, min, max);
    CHECK(min == doctest::Approx(2.0f));
    CHECK(max == doctest::Approx(4.0f));
}

TEST_CASE("GetMaximumPolygonComponent returns an edge when two vertices tie") {
    // A unit square's right edge (vertices 1 and 2) projects to the same max
    // value in the +X direction.
    std::vector<glm::vec2> square = {
        {-1.0f, -1.0f}, {1.0f, -1.0f}, {1.0f, 1.0f}, {-1.0f, 1.0f}};
    int a, b;

    PolygonComponent pc = GetMaximumPolygonComponent(square, glm::vec2{1.0f, 0.0f}, a, b);
    CHECK(pc == PolygonComponent::Edge);
    // Indices 1 and 2 both project to 1.0f.
    CHECK(((a == 1 && b == 2) || (a == 2 && b == 1)));
}

TEST_CASE("GetMaximumPolygonComponent returns a vertex when one clearly dominates") {
    std::vector<glm::vec2> square = {
        {-1.0f, -1.0f}, {1.0f, -1.0f}, {1.0f, 1.0f}, {-1.0f, 1.0f}};
    int a, b;

    // Diagonal direction: only (1, 1) at index 2 projects to the max.
    PolygonComponent pc = GetMaximumPolygonComponent(square, glm::vec2{1.0f, 1.0f}, a, b);
    CHECK(pc == PolygonComponent::Vertex);
    CHECK(a == 2);
    // Per contract, when returning Vertex the B index mirrors A.
    CHECK(b == 2);
}

TEST_CASE("GetMaximumPolygonComponent finds the max vertex of a triangle") {
    std::vector<glm::vec2> triangle = {{0.0f, 0.0f}, {1.0f, 0.0f}, {0.0f, 1.0f}};
    int a, b;

    PolygonComponent pc = GetMaximumPolygonComponent(triangle, glm::vec2{1.0f, 0.0f}, a, b);
    CHECK(pc == PolygonComponent::Vertex);
    CHECK(a == 1);
}

TEST_CASE("MakeCollider populates vertices, axes, and center") {
    Collider c = MakeSquare(1.0f);

    CHECK(c.vertices.size() == 4);
    CHECK(c.collisionAxes.size() == 4);

    CHECK(c.center.x == doctest::Approx(0.0f));
    CHECK(c.center.y == doctest::Approx(0.0f));
}

TEST_CASE("MakeCollider computes normalized edge normals") {
    Collider c = MakeSquare(2.0f);

    // Every edge normal of an axis-aligned square must be unit-length.
    for (const auto &axis : c.collisionAxes) {
        CHECK(glm::length(axis) == doctest::Approx(1.0f));
    }
}

TEST_CASE("MakeCollider centroids a non-origin polygon") {
    Collider c = MakeCollider({
        {10.0f, 10.0f},
        {20.0f, 10.0f},
        {20.0f, 30.0f},
        {10.0f, 30.0f},
    });

    CHECK(c.center.x == doctest::Approx(15.0f));
    CHECK(c.center.y == doctest::Approx(20.0f));
}

TEST_CASE("FindCollision detects a point inside an axis-aligned square") {
    Collider square = MakeSquare(1.0f);

    CHECK(FindCollision(glm::vec2{0.0f, 0.0f}, square, glm::vec2{0.0f, 0.0f}));
    CHECK(FindCollision(glm::vec2{0.5f, 0.5f}, square, glm::vec2{0.0f, 0.0f}));
    CHECK(FindCollision(glm::vec2{-0.9f, 0.9f}, square, glm::vec2{0.0f, 0.0f}));
}

TEST_CASE("FindCollision rejects points outside an axis-aligned square") {
    Collider square = MakeSquare(1.0f);

    CHECK_FALSE(FindCollision(glm::vec2{2.0f, 0.0f}, square, glm::vec2{0.0f, 0.0f}));
    CHECK_FALSE(FindCollision(glm::vec2{-2.0f, 0.0f}, square, glm::vec2{0.0f, 0.0f}));
    CHECK_FALSE(FindCollision(glm::vec2{0.0f, 2.0f}, square, glm::vec2{0.0f, 0.0f}));
    CHECK_FALSE(FindCollision(glm::vec2{0.0f, -2.0f}, square, glm::vec2{0.0f, 0.0f}));
}

TEST_CASE("FindCollision treats the edge as inside") {
    // Documented behavior: the projection test uses strict <, so points
    // exactly on an edge count as inside the collider.
    Collider square = MakeSquare(1.0f);

    CHECK(FindCollision(glm::vec2{1.0f, 0.0f}, square, glm::vec2{0.0f, 0.0f}));
    CHECK(FindCollision(glm::vec2{0.0f, 1.0f}, square, glm::vec2{0.0f, 0.0f}));
    CHECK(FindCollision(glm::vec2{1.0f, 1.0f}, square, glm::vec2{0.0f, 0.0f}));
}

TEST_CASE("FindCollision respects the position offset") {
    Collider square = MakeSquare(1.0f);

    // Shift the square to (10, 0) -- (10, 0) is now inside.
    CHECK(FindCollision(glm::vec2{10.0f, 0.0f}, square, glm::vec2{10.0f, 0.0f}));
    // Origin is now outside.
    CHECK_FALSE(FindCollision(glm::vec2{0.0f, 0.0f}, square, glm::vec2{10.0f, 0.0f}));
}

TEST_CASE("Swept FindCollision reports no collision when bodies are separated and still") {
    Collider a = MakeSquare(1.0f);
    Collider b = MakeSquare(1.0f);
    glm::vec2 axis;
    float time;

    // A is at (4, 0) relative to B, both stationary. No overlap, no motion.
    CHECK_FALSE(FindCollision(a, b, glm::vec2{4.0f, 0.0f}, glm::vec2{0.0f, 0.0f}, axis, time));
}

TEST_CASE("Swept FindCollision reports collision when bodies already overlap") {
    Collider a = MakeSquare(1.0f);
    Collider b = MakeSquare(1.0f);
    glm::vec2 axis;
    float time;

    // A overlaps B by 0.5 units in X (A's right edge at +0.5, B's left at -1).
    REQUIRE(FindCollision(a, b, glm::vec2{1.5f, 0.0f}, glm::vec2{0.0f, 0.0f}, axis, time));
    // Time is the shallowest negative overlap.
    CHECK(time < 0.0f);
    // Axis should point from B toward A, which is +X here.
    CHECK(axis.x > 0.0f);
    CHECK(axis.y == doctest::Approx(0.0f));
}

TEST_CASE("Swept FindCollision computes the time of impact for approaching bodies") {
    Collider a = MakeSquare(1.0f);
    Collider b = MakeSquare(1.0f);
    glm::vec2 axis;
    float time;

    // A starts at x = -4 (4 units left of B), moving right at 4 units/frame.
    // Their right/left edges meet when A's right edge (-4 + 4t + 1 = 4t - 3)
    // equals B's left edge (-1), so t = 0.5.
    REQUIRE(FindCollision(a, b, glm::vec2{-4.0f, 0.0f}, glm::vec2{4.0f, 0.0f}, axis, time));
    CHECK(time == doctest::Approx(0.5f));
    // Axis points from B toward A, which is -X.
    CHECK(axis.x < 0.0f);
    CHECK(axis.y == doctest::Approx(0.0f));
}

TEST_CASE("Swept FindCollision reports no collision for bodies moving apart") {
    Collider a = MakeSquare(1.0f);
    Collider b = MakeSquare(1.0f);
    glm::vec2 axis;
    float time;

    // A is to the right of B and moving further right.
    CHECK_FALSE(FindCollision(a, b, glm::vec2{4.0f, 0.0f}, glm::vec2{2.0f, 0.0f}, axis, time));
}

TEST_CASE("Swept FindCollision rejects impacts later than time 1") {
    Collider a = MakeSquare(1.0f);
    Collider b = MakeSquare(1.0f);
    glm::vec2 axis;
    float time;

    // A is 10 units away and moving at only 1 unit/frame -- impact would be
    // at t = 8, well outside the frame.
    CHECK_FALSE(FindCollision(a, b, glm::vec2{-10.0f, 0.0f}, glm::vec2{1.0f, 0.0f}, axis, time));
}
