#pragma once

namespace Lucky
{
    struct Rectangle
    {
        Rectangle()
            : x(0),
              y(0),
              width(0),
              height(0)
        {
        }

        Rectangle(int x, int y, int width, int height)
            : x(x),
              y(y),
              width(width),
              height(height)
        {
        }

        int x;
        int y;
        int width;
        int height;
    };
} // namespace Lucky