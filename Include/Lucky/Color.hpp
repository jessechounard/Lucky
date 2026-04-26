#pragma once

#include <Lucky/MathHelpers.hpp>

namespace Lucky {

/**
 * A linear-space RGBA color with float components in [0, 1].
 *
 * Used wherever Lucky needs a color: clear color, vertex tint, font color,
 * shape fill. Components are stored in declaration order (`r, g, b, a`) so
 * the struct lays out as a packed `float[4]` for direct upload to GPU
 * uniforms or vertex attributes.
 *
 * # Equality
 *
 * `operator==` and `operator!=` use ApproximatelyEqual on each component
 * (default tolerance 0.0001f), not exact float comparison. This makes
 * equality robust to small precision drift from arithmetic but means
 * `Color(0.5f, ...) == Color(0.50005f, ...)` returns true. For exact
 * comparison, compare components directly.
 *
 * # Named constants
 *
 * Eight common colors are exposed as `inline const` static members
 * (`Color::Red`, `Color::White`, `Color::CornflowerBlue`, etc.) and live
 * in the header so consumers don't pay a translation-unit cost.
 */
struct Color {
    constexpr Color() noexcept : r(0), g(0), b(0), a(1) {
    }

    constexpr Color(float r, float g, float b, float a) noexcept : r(r), g(g), b(b), a(a) {
    }

    bool operator==(const Color &other) const {
        return ApproximatelyEqual(r, other.r) && ApproximatelyEqual(g, other.g) &&
               ApproximatelyEqual(b, other.b) && ApproximatelyEqual(a, other.a);
    }

    bool operator!=(const Color &other) const {
        return !(*this == other);
    }

    float r, g, b, a;

    static const Color Red;
    static const Color Green;
    static const Color Blue;
    static const Color White;
    static const Color Black;
    static const Color Yellow;
    static const Color Gray;
    static const Color CornflowerBlue;
};

inline const Color Color::Red{1.0f, 0.0f, 0.0f, 1.0f};
inline const Color Color::Green{0.0f, 1.0f, 0.0f, 1.0f};
inline const Color Color::Blue{0.0f, 0.0f, 1.0f, 1.0f};
inline const Color Color::White{1.0f, 1.0f, 1.0f, 1.0f};
inline const Color Color::Black{0.0f, 0.0f, 0.0f, 1.0f};
inline const Color Color::Yellow{1.0f, 1.0f, 0.0f, 1.0f};
inline const Color Color::Gray{0.5f, 0.5f, 0.5f, 1.0f};
inline const Color Color::CornflowerBlue{0.392f, 0.584f, 0.929f, 1.0f};

} // namespace Lucky
