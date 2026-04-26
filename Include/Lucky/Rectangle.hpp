#pragma once

namespace Lucky {

/**
 * An axis-aligned integer rectangle in pixel space.
 *
 * Used throughout Lucky for viewport, scissor, and texture-region
 * specification. All coordinates are integer pixels; for sub-pixel work
 * (sprite positions, shader uniforms) prefer glm::vec2 / glm::vec4.
 *
 * Width and height are signed but not validated -- negative sizes are
 * legal at the type level but rarely meaningful.
 */
struct Rectangle {
    Rectangle() : x(0), y(0), width(0), height(0) {
    }

    Rectangle(int x, int y, int width, int height) : x(x), y(y), width(width), height(height) {
    }

    int x;
    int y;
    int width;
    int height;
};

} // namespace Lucky
