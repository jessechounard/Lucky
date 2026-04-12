#include <doctest/doctest.h>

#include <glm/vec2.hpp>

#include <Lucky/MathConstants.hpp>
#include <Lucky/MathHelpers.hpp>

using namespace Lucky;

TEST_CASE("Lerp interpolates between values") {
    CHECK(Lerp(0.0f, 10.0f, 0.0f) == doctest::Approx(0.0f));
    CHECK(Lerp(0.0f, 10.0f, 1.0f) == doctest::Approx(10.0f));
    CHECK(Lerp(0.0f, 10.0f, 0.5f) == doctest::Approx(5.0f));
    CHECK(Lerp(-10.0f, 10.0f, 0.5f) == doctest::Approx(0.0f));
}

TEST_CASE("Lerp extrapolates outside [0, 1]") {
    CHECK(Lerp(0.0f, 10.0f, 2.0f) == doctest::Approx(20.0f));
    CHECK(Lerp(0.0f, 10.0f, -0.5f) == doctest::Approx(-5.0f));
    CHECK(Lerp(10.0f, 20.0f, 1.5f) == doctest::Approx(25.0f));
}

TEST_CASE("Lerp handles degenerate range where v0 == v1") {
    CHECK(Lerp(5.0f, 5.0f, 0.5f) == doctest::Approx(5.0f));
    CHECK(Lerp(5.0f, 5.0f, 100.0f) == doctest::Approx(5.0f));
}

TEST_CASE("InverseLerp returns the parameter for a value") {
    CHECK(InverseLerp(0.0f, 10.0f, 5.0f) == doctest::Approx(0.5f));
    CHECK(InverseLerp(0.0f, 10.0f, 0.0f) == doctest::Approx(0.0f));
    CHECK(InverseLerp(0.0f, 10.0f, 10.0f) == doctest::Approx(1.0f));
}

TEST_CASE("InverseLerp extrapolates for values outside the range") {
    CHECK(InverseLerp(0.0f, 10.0f, 15.0f) == doctest::Approx(1.5f));
    CHECK(InverseLerp(0.0f, 10.0f, -5.0f) == doctest::Approx(-0.5f));
}

TEST_CASE("InverseLerp handles reversed range") {
    CHECK(InverseLerp(10.0f, 0.0f, 5.0f) == doctest::Approx(0.5f));
    CHECK(InverseLerp(10.0f, 0.0f, 10.0f) == doctest::Approx(0.0f));
    CHECK(InverseLerp(10.0f, 0.0f, 0.0f) == doctest::Approx(1.0f));
}

TEST_CASE("InverseLerp returns 0 when v0 == v1") {
    CHECK(InverseLerp(5.0f, 5.0f, 5.0f) == doctest::Approx(0.0f));
    CHECK(InverseLerp(5.0f, 5.0f, 100.0f) == doctest::Approx(0.0f));
}

TEST_CASE("Sign returns -1, 0, or 1") {
    CHECK(Sign(5) == 1);
    CHECK(Sign(-5) == -1);
    CHECK(Sign(0) == 0);
    CHECK(Sign(3.14f) == 1);
    CHECK(Sign(-3.14f) == -1);
}

TEST_CASE("Clamp restricts values to a range") {
    CHECK(Clamp(5, 0, 10) == 5);
    CHECK(Clamp(-5, 0, 10) == 0);
    CHECK(Clamp(15, 0, 10) == 10);
    CHECK(Clamp(0, 0, 10) == 0);
    CHECK(Clamp(10, 0, 10) == 10);
}

TEST_CASE("Clamp handles negative ranges") {
    CHECK(Clamp(-5, -10, -1) == -5);
    CHECK(Clamp(-15, -10, -1) == -10);
    CHECK(Clamp(0, -10, -1) == -1);
}

TEST_CASE("Clamp with lo > hi returns lo") {
    // Documented behavior: if the caller supplies lo > hi, Clamp returns lo
    // regardless of v. Callers should not rely on this and should supply a
    // valid range.
    CHECK(Clamp(5, 10, 0) == 10);
    CHECK(Clamp(-5, 10, 0) == 10);
    CHECK(Clamp(15, 10, 0) == 10);
}

TEST_CASE("MoveTowards advances current toward target by delta") {
    CHECK(MoveTowards(0.0f, 10.0f, 3.0f) == doctest::Approx(3.0f));
    CHECK(MoveTowards(10.0f, 0.0f, 3.0f) == doctest::Approx(7.0f));
    CHECK(MoveTowards(0.0f, 5.0f, 10.0f) == doctest::Approx(5.0f));
    CHECK(MoveTowards(5.0f, 5.0f, 1.0f) == doctest::Approx(5.0f));
}

TEST_CASE("MoveTowards treats negative delta as positive") {
    CHECK(MoveTowards(0.0f, 10.0f, -3.0f) == doctest::Approx(3.0f));
    CHECK(MoveTowards(10.0f, 0.0f, -3.0f) == doctest::Approx(7.0f));
}

TEST_CASE("MoveTowards does not overshoot the target") {
    CHECK(MoveTowards(0.0f, 5.0f, 100.0f) == doctest::Approx(5.0f));
    CHECK(MoveTowards(5.0f, 0.0f, 100.0f) == doctest::Approx(0.0f));
}

TEST_CASE("MoveTowards with zero delta returns current unchanged") {
    CHECK(MoveTowards(3.0f, 10.0f, 0.0f) == doctest::Approx(3.0f));
    CHECK(MoveTowards(10.0f, 3.0f, 0.0f) == doctest::Approx(10.0f));
}

TEST_CASE("ApproximatelyEqual respects tolerance") {
    CHECK(ApproximatelyEqual(1.0f, 1.0f));
    CHECK(ApproximatelyEqual(1.0f, 1.00001f));
    CHECK_FALSE(ApproximatelyEqual(1.0f, 1.1f));
    CHECK(ApproximatelyEqual(1.0f, 1.05f, 0.1f));
}

TEST_CASE("ApproximatelyEqual works with negative values") {
    CHECK(ApproximatelyEqual(-1.0f, -1.0f));
    CHECK(ApproximatelyEqual(-1.0f, -1.00001f));
    CHECK_FALSE(ApproximatelyEqual(-1.0f, -1.1f));
}

TEST_CASE("ApproximatelyEqual scales tolerance with magnitude") {
    // Tolerance scales by max(1, max(|a|, |b|)), so large values get a
    // proportionally larger effective tolerance. This makes the comparison
    // behave relatively for values much larger than 1.
    CHECK(ApproximatelyEqual(1000000.0f, 1000000.1f));
    CHECK(ApproximatelyEqual(1000000.0f, 1000050.0f));
    CHECK_FALSE(ApproximatelyEqual(1000000.0f, 1000200.0f));
}

TEST_CASE("ApproximatelyEqual includes the boundary") {
    // Uses <=, so a difference exactly equal to tolerance counts as equal.
    CHECK(ApproximatelyEqual(1.0f, 1.1f, 0.1f));
}

TEST_CASE("ApproximatelyZero respects tolerance") {
    CHECK(ApproximatelyZero(0.0f));
    CHECK(ApproximatelyZero(0.00001f));
    CHECK_FALSE(ApproximatelyZero(0.1f));
    CHECK(ApproximatelyZero(0.05f, 0.1f));
}

TEST_CASE("ApproximatelyZero works with negative values") {
    CHECK(ApproximatelyZero(-0.00001f));
    CHECK_FALSE(ApproximatelyZero(-0.1f));
}

TEST_CASE("ApproximatelyZero includes the boundary") {
    // Uses <=, so exactly-at-tolerance counts as zero (matching
    // ApproximatelyEqual).
    CHECK(ApproximatelyZero(0.0001f, 0.0001f));
    CHECK(ApproximatelyZero(-0.0001f, 0.0001f));
}

TEST_CASE("FindAngle returns the CCW angle from reference to vector") {
    const glm::vec2 right{1.0f, 0.0f};
    const glm::vec2 up{0.0f, 1.0f};
    const glm::vec2 left{-1.0f, 0.0f};
    const glm::vec2 down{0.0f, -1.0f};

    // Same direction: 0 radians
    CHECK(FindAngle(right, right) == doctest::Approx(0.0f));

    // 90 degrees CCW
    CHECK(FindAngle(up, right) == doctest::Approx(MathConstants::Pi / 2.0f));

    // 180 degrees
    CHECK(FindAngle(left, right) == doctest::Approx(MathConstants::Pi));

    // 270 degrees (negative 90, wrapped to [0, 2pi))
    CHECK(FindAngle(down, right) == doctest::Approx(3.0f * MathConstants::Pi / 2.0f));

    // 45 degrees
    CHECK(FindAngle(glm::vec2{1.0f, 1.0f}, right) == doctest::Approx(MathConstants::Pi / 4.0f));
}

TEST_CASE("FindAngle is independent of vector magnitudes") {
    // atan2 only cares about direction, so non-unit vectors give the same
    // angle as their normalized equivalents.
    CHECK(FindAngle(glm::vec2{5.0f, 0.0f}, glm::vec2{2.0f, 0.0f}) == doctest::Approx(0.0f));
    CHECK(FindAngle(glm::vec2{0.0f, 100.0f}, glm::vec2{50.0f, 0.0f}) ==
          doctest::Approx(MathConstants::Pi / 2.0f));
}

TEST_CASE("FindAngle returns 0 for the zero vector") {
    // Documented behavior: atan2(0, 0) is defined as 0 in C/C++, so passing a
    // zero vector returns 0 rather than throwing or returning NaN. Callers
    // should avoid passing zero vectors if they care about the distinction.
    CHECK(FindAngle(glm::vec2{0.0f, 0.0f}, glm::vec2{1.0f, 0.0f}) == doctest::Approx(0.0f));
}

TEST_CASE("WorldToLocal maps world coordinates into a [0,1] rectangle") {
    // Center of [0,0]..[10,10] is {0.5, 0.5}
    glm::vec2 result = WorldToLocal({0.0f, 0.0f}, {10.0f, 10.0f}, {5.0f, 5.0f});
    CHECK(result.x == doctest::Approx(0.5f));
    CHECK(result.y == doctest::Approx(0.5f));

    // Min corner
    result = WorldToLocal({0.0f, 0.0f}, {10.0f, 10.0f}, {0.0f, 0.0f});
    CHECK(result.x == doctest::Approx(0.0f));
    CHECK(result.y == doctest::Approx(0.0f));

    // Max corner
    result = WorldToLocal({0.0f, 0.0f}, {10.0f, 10.0f}, {10.0f, 10.0f});
    CHECK(result.x == doctest::Approx(1.0f));
    CHECK(result.y == doctest::Approx(1.0f));

    // Outside the rectangle (extrapolation allowed)
    result = WorldToLocal({0.0f, 0.0f}, {10.0f, 10.0f}, {15.0f, 15.0f});
    CHECK(result.x == doctest::Approx(1.5f));
    CHECK(result.y == doctest::Approx(1.5f));

    // Non-origin rectangle
    result = WorldToLocal({-10.0f, -10.0f}, {10.0f, 10.0f}, {0.0f, 0.0f});
    CHECK(result.x == doctest::Approx(0.5f));
    CHECK(result.y == doctest::Approx(0.5f));
}

TEST_CASE("WorldToLocal returns 0 for a zero-size rectangle") {
    // When xy0 == xy1 on an axis, InverseLerp's degenerate-range handling
    // returns 0. WorldToLocal inherits this behavior.
    glm::vec2 result = WorldToLocal({5.0f, 5.0f}, {5.0f, 5.0f}, {5.0f, 5.0f});
    CHECK(result.x == doctest::Approx(0.0f));
    CHECK(result.y == doctest::Approx(0.0f));
}
