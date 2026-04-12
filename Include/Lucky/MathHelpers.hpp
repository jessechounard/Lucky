#pragma once

#include <algorithm>
#include <cmath>
#include <type_traits>

#include <glm/vec2.hpp>

namespace Lucky {

/**
 * Linearly interpolates between two values.
 *
 * Computes `v0 + t * (v1 - v0)`. When `t` is `0` the result is `v0`, when `t`
 * is `1` the result is `v1`. Values of `t` outside `[0, 1]` extrapolate.
 *
 * If `v0` and `v1` are equal the result is always `v0`, regardless of `t`.
 *
 * \param v0 the value returned when `t` is `0`.
 * \param v1 the value returned when `t` is `1`.
 * \param t the interpolation parameter.
 * \returns the interpolated value.
 *
 * \sa InverseLerp
 */
template <typename ValueType, typename InterpolantType>
ValueType Lerp(const ValueType &v0, const ValueType &v1, InterpolantType t) {
    return v0 + t * (v1 - v0);
}

/**
 * Computes the interpolation parameter that would produce a given value.
 *
 * This is the inverse of Lerp: given a value `v` somewhere between `v0` and
 * `v1`, returns the `t` parameter such that `Lerp(v0, v1, t) == v`.
 *
 * Values of `v` outside `[v0, v1]` return a parameter outside `[0, 1]`
 * (extrapolation). Reversed ranges where `v0 > v1` are supported.
 *
 * If `v0` and `v1` are approximately equal the range is degenerate and the
 * function returns `0`.
 *
 * \param v0 the value corresponding to parameter `0`.
 * \param v1 the value corresponding to parameter `1`.
 * \param v the value to compute the parameter for.
 * \returns the interpolation parameter, or `0` if the range is degenerate.
 *
 * \sa Lerp
 */
float InverseLerp(const float &v0, const float &v1, const float &v);

/**
 * Returns the sign of a value.
 *
 * \param val the value to test.
 * \returns `1` if `val` is positive, `-1` if negative, or `0` if zero.
 */
template <typename T> int Sign(T val) {
    return (int)((T(0) < val) - (val < T(0)));
}

/**
 * Clamps a value to a range.
 *
 * If `v` is less than `lo`, returns `lo`. If `v` is greater than `hi`,
 * returns `hi`. Otherwise returns `v`.
 *
 * \param v the value to clamp.
 * \param lo the lower bound of the range.
 * \param hi the upper bound of the range.
 * \returns `v` clamped to `[lo, hi]`.
 *
 * \note If `lo` is greater than `hi` the function returns `lo` regardless of
 *       `v`. Callers should supply a valid range.
 */
template <typename T> T Clamp(const T &v, const T &lo, const T &hi) {
    T result = std::min(v, hi);
    return std::max(result, lo);
}

/**
 * Moves a value toward a target by a fixed step, without overshooting.
 *
 * If the distance to the target is less than `delta`, the target is returned
 * exactly. Otherwise the result is `current` plus (or minus) `delta` in the
 * direction of `target`.
 *
 * `delta` is treated as its absolute value, so passing a negative step still
 * moves toward the target.
 *
 * \param current the starting value.
 * \param target the destination value.
 * \param delta the maximum amount to move per call.
 * \returns the new value, guaranteed to be between `current` and `target`
 *          (inclusive).
 */
template <typename T> T MoveTowards(const T &current, const T &target, const T &delta) {
    static_assert(std::is_arithmetic_v<T>, "MoveTowards requires an arithmetic type");

    T absDelta = std::abs(delta);
    if (target > current) {
        return std::min(current + absDelta, target);
    } else if (target < current) {
        return std::max(current - absDelta, target);
    }

    return target;
}

/**
 * Returns the counter-clockwise angle from a reference vector to another
 * vector, in radians.
 *
 * The returned angle is in the range `[0, 2*Pi)`. Vector magnitudes are
 * ignored; only the directions matter.
 *
 * \param vector the target direction.
 * \param reference the reference direction that angle zero points along.
 * \returns the counter-clockwise angle from `reference` to `vector`, in
 *          radians, normalized to `[0, 2*Pi)`.
 *
 * \note If either vector is the zero vector the function returns `0`, because
 *       `atan2(0, 0)` is defined as `0` in C. Callers should avoid passing
 *       zero vectors if they need to distinguish that case.
 */
float FindAngle(const glm::vec2 &vector, const glm::vec2 &reference);

/**
 * Tests whether two floats are approximately equal within a tolerance.
 *
 * The effective tolerance scales with the magnitude of the inputs, so large
 * values get a proportionally larger tolerance. This makes the comparison
 * behave like a relative comparison for values much larger than `1`, while
 * still handling values near zero sensibly. See
 * https://realtimecollisiondetection.net/blog/?p=89 for the algorithm.
 *
 * Differences exactly equal to the effective tolerance count as equal.
 *
 * \param floatA the first value.
 * \param floatB the second value.
 * \param tolerance the base tolerance for the comparison.
 * \returns true if the values are approximately equal.
 *
 * \sa ApproximatelyZero
 */
bool ApproximatelyEqual(float floatA, float floatB, float tolerance = 0.0001f);

/**
 * Tests whether a float is approximately zero within a tolerance.
 *
 * Values with absolute value less than or equal to `tolerance` count as zero.
 *
 * \param f the value to test.
 * \param tolerance the tolerance for the comparison.
 * \returns true if `f` is approximately zero.
 *
 * \sa ApproximatelyEqual
 */
bool ApproximatelyZero(float f, float tolerance = 0.0001f);

/**
 * Maps a world-space point to a rectangle's local `[0, 1]` coordinates.
 *
 * Given a rectangle with opposite corners `xy0` and `xy1`, returns the
 * normalized position of `world` inside that rectangle. `xy0` maps to
 * `(0, 0)` and `xy1` maps to `(1, 1)`. Points outside the rectangle map to
 * coordinates outside `[0, 1]` (extrapolation).
 *
 * Non-axis-aligned rectangles are not supported. Reversed ranges where
 * `xy0` is greater than `xy1` on either axis are handled by InverseLerp.
 *
 * \param xy0 one corner of the rectangle, mapping to local `(0, 0)`.
 * \param xy1 the opposite corner, mapping to local `(1, 1)`.
 * \param world the point to convert to local coordinates.
 * \returns the point in rectangle-local `[0, 1]` coordinates.
 *
 * \note If `xy0` and `xy1` are equal on an axis the rectangle has zero size
 *       on that axis and the corresponding result component is `0`.
 */
glm::vec2 WorldToLocal(const glm::vec2 &xy0, const glm::vec2 &xy1, const glm::vec2 &world);

} // namespace Lucky
