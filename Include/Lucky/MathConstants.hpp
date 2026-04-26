#pragma once

namespace Lucky {

/**
 * Common math constants exposed as compile-time `float` values.
 *
 * Defined as `inline constexpr` so callers get constant folding without an
 * extra translation unit.
 */
struct MathConstants {
    static constexpr float Pi = 3.141592653589793238463f; /**< pi. */
    static constexpr float Tau = 2 * Pi;                  /**< 2*pi; one full turn in radians. */
};

} // namespace Lucky
