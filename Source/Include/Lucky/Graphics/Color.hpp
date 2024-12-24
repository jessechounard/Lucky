#pragma once

#include "../Math/MathHelpers.hpp"

namespace Lucky
{
    struct Color
    {
        Color()
            : r(0),
              g(0),
              b(0),
              a(1)
        {
        }

        Color(float r, float g, float b, float a)
            : r(r),
              g(g),
              b(b),
              a(a)
        {
        }

        Color(const Color &color) = default;
        Color(Color &&color) = default;

        Color &operator=(const Color &) = default;
        Color &operator=(Color &&) = default;

        bool operator==(Color otherColor) const
        {
            return ApproximatelyEqual(r, otherColor.r) && ApproximatelyEqual(g, otherColor.g) &&
                   ApproximatelyEqual(b, otherColor.b) && ApproximatelyEqual(a, otherColor.a);
        }

        bool operator!=(Color otherColor) const
        {
            return !(*this == otherColor);
        }

        float r, g, b, a;

        static Color Red;
        static Color Green;
        static Color Blue;
        static Color White;
        static Color Black;
        static Color CornflowerBlue;
    };
} // namespace Lucky
