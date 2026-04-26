#include <doctest/doctest.h>

#include <Lucky/BatchRenderer.hpp> // for the UVMode operators
#include <Lucky/Types.hpp>

using namespace Lucky;

TEST_CASE("UVMode bit values are stable") {
    // These values match TexturePacker output and should not drift.
    CHECK(static_cast<uint32_t>(UVMode::Normal) == 0);
    CHECK(static_cast<uint32_t>(UVMode::RotatedCW90) == 1);
    CHECK(static_cast<uint32_t>(UVMode::FlipHorizontal) == 2);
    CHECK(static_cast<uint32_t>(UVMode::FlipVertical) == 4);
}

TEST_CASE("UVMode bitwise OR combines flags") {
    UVMode combined = UVMode::RotatedCW90 | UVMode::FlipHorizontal;
    CHECK(static_cast<uint32_t>(combined) == 3);
}

TEST_CASE("UVMode bitwise AND isolates flags") {
    UVMode combined = UVMode::RotatedCW90 | UVMode::FlipVertical;
    UVMode isolated = combined & UVMode::FlipVertical;
    CHECK(static_cast<uint32_t>(isolated) == static_cast<uint32_t>(UVMode::FlipVertical));
}

TEST_CASE("UVMode |= and &= mutate in place") {
    UVMode value = UVMode::Normal;
    value |= UVMode::RotatedCW90;
    value |= UVMode::FlipHorizontal;
    CHECK(static_cast<uint32_t>(value) == 3);

    value &= UVMode::RotatedCW90;
    CHECK(static_cast<uint32_t>(value) == 1);
}

TEST_CASE("HasFlag detects set bits") {
    UVMode combined = UVMode::RotatedCW90 | UVMode::FlipVertical;
    CHECK(HasFlag(combined, UVMode::RotatedCW90));
    CHECK(HasFlag(combined, UVMode::FlipVertical));
    CHECK_FALSE(HasFlag(combined, UVMode::FlipHorizontal));
}

TEST_CASE("HasFlag returns false for Normal against any flag") {
    CHECK_FALSE(HasFlag(UVMode::Normal, UVMode::RotatedCW90));
    CHECK_FALSE(HasFlag(UVMode::Normal, UVMode::FlipHorizontal));
    CHECK_FALSE(HasFlag(UVMode::Normal, UVMode::FlipVertical));
}
