#include <doctest/doctest.h>

#include <Lucky/Color.hpp>

using namespace Lucky;

TEST_CASE("Color default constructor produces opaque black") {
    Color c;
    CHECK(c.r == doctest::Approx(0.0f));
    CHECK(c.g == doctest::Approx(0.0f));
    CHECK(c.b == doctest::Approx(0.0f));
    CHECK(c.a == doctest::Approx(1.0f));
}

TEST_CASE("Color component constructor stores values") {
    Color c(0.25f, 0.5f, 0.75f, 0.1f);
    CHECK(c.r == doctest::Approx(0.25f));
    CHECK(c.g == doctest::Approx(0.5f));
    CHECK(c.b == doctest::Approx(0.75f));
    CHECK(c.a == doctest::Approx(0.1f));
}

TEST_CASE("Color equality compares within tolerance") {
    Color a(0.5f, 0.5f, 0.5f, 1.0f);
    Color b(0.5f, 0.5f, 0.5f, 1.0f);
    CHECK(a == b);
    CHECK_FALSE(a != b);
}

TEST_CASE("Color equality allows small differences") {
    Color a(0.5f, 0.5f, 0.5f, 1.0f);
    Color b(0.50001f, 0.5f, 0.5f, 1.0f);
    CHECK(a == b);
}

TEST_CASE("Color inequality detects real differences") {
    Color a(0.5f, 0.5f, 0.5f, 1.0f);
    Color b(0.6f, 0.5f, 0.5f, 1.0f);
    CHECK(a != b);
    CHECK_FALSE(a == b);
}
