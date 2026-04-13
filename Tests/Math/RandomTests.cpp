#include <doctest/doctest.h>

#include <Lucky/Random.hpp>

using namespace Lucky;

TEST_CASE("GenerateRandom is deterministic for the same inputs") {
    uint32_t a = GenerateRandom(42, 1234);
    uint32_t b = GenerateRandom(42, 1234);
    CHECK(a == b);
}

TEST_CASE("GenerateRandom produces different values for different positions") {
    uint32_t a = GenerateRandom(0, 1234);
    uint32_t b = GenerateRandom(1, 1234);
    uint32_t c = GenerateRandom(2, 1234);

    // A good noise function should produce distinct values for adjacent
    // positions. This is not strictly guaranteed, but collisions for these
    // specific inputs would indicate a serious regression.
    CHECK(a != b);
    CHECK(b != c);
    CHECK(a != c);
}

TEST_CASE("GenerateRandom produces different values for different seeds") {
    uint32_t a = GenerateRandom(100, 1);
    uint32_t b = GenerateRandom(100, 2);
    CHECK(a != b);
}

TEST_CASE("GenerateRandom integer range stays within bounds") {
    for (int32_t pos = 0; pos < 1000; pos++) {
        int value = GenerateRandom<int>(pos, 42, 10, 20);
        CHECK(value >= 10);
        CHECK(value <= 20);
    }
}

TEST_CASE("GenerateRandom integer range covers both endpoints") {
    // Over 10000 samples in a range of [0, 9], we expect to see both 0 and 9
    // at least once. If either endpoint is missing, the range is exclusive
    // when it should be inclusive.
    bool sawMin = false;
    bool sawMax = false;
    for (int32_t pos = 0; pos < 10000; pos++) {
        int value = GenerateRandom<int>(pos, 7, 0, 9);
        if (value == 0)
            sawMin = true;
        if (value == 9)
            sawMax = true;
        if (sawMin && sawMax)
            break;
    }
    CHECK(sawMin);
    CHECK(sawMax);
}

TEST_CASE("GenerateRandom integer range is deterministic") {
    int a = GenerateRandom<int>(55, 100, 0, 99);
    int b = GenerateRandom<int>(55, 100, 0, 99);
    CHECK(a == b);
}

TEST_CASE("GenerateRandom float range stays within bounds") {
    for (int32_t pos = 0; pos < 1000; pos++) {
        float value = GenerateRandom<float>(pos, 42, -5.0f, 5.0f);
        CHECK(value >= -5.0f);
        CHECK(value <= 5.0f);
    }
}

TEST_CASE("GenerateRandom float range is deterministic") {
    float a = GenerateRandom<float>(12, 34, 0.0f, 1.0f);
    float b = GenerateRandom<float>(12, 34, 0.0f, 1.0f);
    CHECK(a == doctest::Approx(b));
}
