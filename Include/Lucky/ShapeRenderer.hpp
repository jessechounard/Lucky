#pragma once

#include <vector>

#include <glm/glm.hpp>

#include <Lucky/BatchRenderer.hpp>
#include <Lucky/BlendState.hpp>
#include <Lucky/Color.hpp>
#include <Lucky/Shader.hpp>

namespace Lucky {

/**
 * Renders antialiased 2D shape outlines by dispatching to an SDF fragment
 * shader.
 *
 * Each shape draws a single textured quad whose fragment shader computes the
 * shape's signed distance field and stamps out a smooth outline at the
 * requested thickness. The backing shader is provided by the caller at
 * Begin() time; Lucky ships a suitable shader at `Content/Shaders/sdf_outline.frag`.
 *
 * # Usage
 *
 * Construct one ShapeRenderer per BatchRenderer you want to draw shapes
 * through. Then, once per group of shapes:
 *
 * 1. Call Begin() with the blend mode and the SDF outline shader.
 * 2. Call any number of Draw*() methods.
 * 3. Call End().
 *
 * The shader object must remain alive for the entire Begin/End block.
 *
 * # Output is outline-only
 *
 * Every Draw*() method produces an antialiased outline at `thickness` world
 * units wide. There is no fill mode; callers that want a filled shape
 * should draw a BatchRenderer quad separately. This matches the debug-draw
 * semantics ShapeRenderer is primarily intended for.
 *
 * # Performance
 *
 * Each call to a Draw*() method produces its own BatchRenderer Begin/End
 * and, therefore, its own draw call. Shapes can't be batched together
 * because each one needs unique per-instance uniform data in the fragment
 * shader. ShapeRenderer is suitable for debug drawing, UI, and small scenes
 * (tens to low hundreds of shapes per frame). It is not suitable for
 * drawing thousands of shapes; use a custom instanced renderer for that.
 *
 * # Thread safety
 *
 * ShapeRenderer is not thread-safe. A single ShapeRenderer instance must be
 * used from a single thread at a time.
 *
 * # Coordinate system
 *
 * All positions, sizes, and thicknesses are in the same world-space units
 * used by the parent BatchRenderer. Thickness does not compensate for
 * camera zoom; if you zoom in, outlines grow with everything else.
 */
struct ShapeRenderer {
    /**
     * Constructs a ShapeRenderer that draws through the given BatchRenderer.
     *
     * The BatchRenderer must outlive the ShapeRenderer. No GPU resources are
     * allocated by this constructor.
     *
     * \param batchRenderer the BatchRenderer to route draw calls through.
     */
    ShapeRenderer(BatchRenderer &batchRenderer);

    /**
     * Begins a group of shape draws.
     *
     * Stores the blend mode, shader, and model/view transform that
     * subsequent Draw*() calls will use. Must be paired with a matching
     * End(). Calling Begin() twice without an intervening End() is a
     * programming error.
     *
     * The transform matrix is passed through to the underlying
     * BatchRenderer as its model/view transform, applied before the
     * BatchRenderer's built-in orthographic projection. Passing the
     * default identity matrix means shapes are drawn in pixel-space
     * coordinates. Passing a camera matrix will place shapes in world
     * space.
     *
     * \param blendMode the blend mode for shapes drawn in this group.
     * \param shader the SDF outline fragment shader. Lucky ships a suitable
     *               shader at `Content/Shaders/sdf_outline.frag`; see the
     *               class documentation for shader requirements. The shader
     *               object must outlive the Begin/End block.
     * \param transformMatrix the model/view transform to apply before the
     *                        BatchRenderer's orthographic projection.
     *                        Defaults to the identity matrix.
     */
    void Begin(
        BlendMode blendMode, Shader &shader, const glm::mat4 &transformMatrix = glm::mat4(1.0f));

    /**
     * Ends a group of shape draws.
     *
     * After End() returns, the stored shader pointer is cleared and
     * subsequent Draw*() calls are an error until Begin() is called again.
     */
    void End();

    /**
     * Draws an antialiased line segment.
     *
     * The line is stroked at `thickness` units wide and rotated around its
     * midpoint by `rotation` radians. Lines with zero length (start == end)
     * are a programming error; the implementation will assert in debug
     * builds.
     *
     * \param start the line's first endpoint in world space.
     * \param end the line's second endpoint. Must not equal `start`.
     * \param rotation additional rotation, in radians, applied around the
     *                 midpoint of the line.
     * \param color the color of the line.
     * \param thickness the stroke width in world units. Must be positive.
     */
    void DrawLine(
        glm::vec2 start, glm::vec2 end, float rotation, Color color, float thickness = 1.0f);

    /**
     * Draws an antialiased circle outline.
     *
     * \param center the center of the circle in world space.
     * \param radius the circle's radius. Must be positive.
     * \param color the color of the outline.
     * \param thickness the stroke width in world units. Must be positive.
     */
    void DrawCircle(glm::vec2 center, float radius, Color color, float thickness = 1.0f);

    /**
     * Draws an antialiased rectangle outline.
     *
     * \param center the center of the rectangle in world space.
     * \param dimensions the full width and height of the rectangle (not
     *                   half-extents). Both components must be positive.
     * \param rotation the rotation, in radians, applied around `center`.
     * \param color the color of the outline.
     * \param thickness the stroke width in world units. Must be positive.
     */
    void DrawRectangle(glm::vec2 center, glm::vec2 dimensions, float rotation, Color color,
        float thickness = 1.0f);

    /**
     * Draws an antialiased triangle outline.
     *
     * The three vertices can be given in any winding order. The triangle
     * must not be degenerate (zero-area or with coincident vertices);
     * passing a degenerate triangle produces undefined visual output.
     *
     * \param p0 the first vertex in world space.
     * \param p1 the second vertex in world space.
     * \param p2 the third vertex in world space.
     * \param rotation an additional rotation, in radians, applied around
     *                 the triangle's centroid.
     * \param color the color of the outline.
     * \param thickness the stroke width in world units. Must be positive.
     */
    void DrawTriangle(glm::vec2 p0, glm::vec2 p1, glm::vec2 p2, float rotation, Color color,
        float thickness = 1.0f);

    /**
     * Draws an antialiased diamond (rhombus) outline.
     *
     * `width` and `height` are the full axis lengths of the diamond, not
     * half-extents, and specify the distance between opposite vertices.
     *
     * \param center the center of the diamond in world space.
     * \param width the full horizontal extent at rotation zero. Must be
     *              positive.
     * \param height the full vertical extent at rotation zero. Must be
     *               positive.
     * \param rotation the rotation, in radians, applied around `center`.
     * \param color the color of the outline.
     * \param thickness the stroke width in world units. Must be positive.
     */
    void DrawDiamond(glm::vec2 center, float width, float height, float rotation, Color color,
        float thickness = 1.0f);

    /**
     * Draws an antialiased regular polygon outline (triangle, square, etc.).
     *
     * At rotation zero, the polygon's first vertex points along +Y, matching
     * the winding produced by GetRegularPolygonVertices().
     *
     * \param center the center of the polygon in world space.
     * \param radius the distance from center to any vertex. Must be positive.
     * \param sides the number of sides. Must be at least 3.
     * \param rotation the rotation, in radians, applied around `center`.
     * \param color the color of the outline.
     * \param thickness the stroke width in world units. Must be positive.
     */
    void DrawRegularPolygon(glm::vec2 center, float radius, int sides, float rotation, Color color,
        float thickness = 1.0f);

    /**
     * Draws an antialiased n-pointed star outline.
     *
     * At rotation zero, the star's first outer point points along +Y,
     * matching the winding produced by GetStarVertices().
     *
     * \param center the center of the star in world space.
     * \param outerRadius the distance from center to any outer point. Must
     *                    be positive.
     * \param innerRadius the distance from center to any inner point. Must
     *                    be positive and less than or equal to outerRadius.
     * \param points the number of outer points. Must be at least 2.
     * \param rotation the rotation, in radians, applied around `center`.
     * \param color the color of the outline.
     * \param thickness the stroke width in world units. Must be positive.
     */
    void DrawStar(glm::vec2 center, float outerRadius, float innerRadius, int points,
        float rotation, Color color, float thickness = 1.0f);

    /**
     * Returns the four vertices of a rectangle after rotating around its
     * center.
     *
     * Vertices are returned in counter-clockwise order starting from the
     * bottom-left (at rotation zero): `(-halfW, -halfH)`, `(halfW, -halfH)`,
     * `(halfW, halfH)`, `(-halfW, halfH)`. Useful for building collision
     * shapes that match a drawn rectangle.
     *
     * \param center the center of the rectangle in world space.
     * \param dimensions the full width and height of the rectangle.
     * \param rotation the rotation, in radians, applied around `center`.
     * \returns the four corner vertices in counter-clockwise order.
     */
    static std::vector<glm::vec2> GetRectangleVertices(
        glm::vec2 center, glm::vec2 dimensions, float rotation);

    /**
     * Returns the three vertices of a triangle after rotating around its
     * centroid.
     *
     * \param p0 the first vertex in world space.
     * \param p1 the second vertex in world space.
     * \param p2 the third vertex in world space.
     * \param rotation the rotation, in radians, applied around the centroid
     *                 of the triangle.
     * \returns the three rotated vertices in the order `{p0, p1, p2}`.
     */
    static std::vector<glm::vec2> GetTriangleVertices(
        glm::vec2 p0, glm::vec2 p1, glm::vec2 p2, float rotation);

    /**
     * Returns the four vertices of a diamond after rotating around its
     * center.
     *
     * At rotation zero the vertices are returned in counter-clockwise order
     * starting from +X: `(halfW, 0)`, `(0, halfH)`, `(-halfW, 0)`,
     * `(0, -halfH)`.
     *
     * \param center the center of the diamond in world space.
     * \param width the full horizontal extent at rotation zero.
     * \param height the full vertical extent at rotation zero.
     * \param rotation the rotation, in radians, applied around `center`.
     * \returns the four diamond vertices in counter-clockwise order.
     */
    static std::vector<glm::vec2> GetDiamondVertices(
        glm::vec2 center, float width, float height, float rotation);

    /**
     * Returns the vertices of a regular polygon.
     *
     * Vertices are distributed evenly around a circle of the given radius.
     * At rotation zero, the first vertex points along +Y and subsequent
     * vertices are arranged counter-clockwise.
     *
     * \param center the center of the polygon in world space.
     * \param radius the distance from center to any vertex.
     * \param sides the number of sides (and vertices). Must be at least 3.
     * \param rotation the rotation, in radians, applied around `center`.
     * \returns `sides` vertices in counter-clockwise order starting from the
     *          rotated +Y direction.
     */
    static std::vector<glm::vec2> GetRegularPolygonVertices(
        glm::vec2 center, float radius, int sides, float rotation);

    /**
     * Returns the vertices of an n-pointed star.
     *
     * The returned list contains `points * 2` vertices, alternating between
     * outer and inner radii. At rotation zero, the first outer point points
     * along +Y and subsequent vertices are arranged counter-clockwise.
     *
     * \param center the center of the star in world space.
     * \param outerRadius the distance from center to any outer point.
     * \param innerRadius the distance from center to any inner point.
     * \param points the number of outer points. Must be at least 2.
     * \param rotation the rotation, in radians, applied around `center`.
     * \returns `points * 2` vertices in counter-clockwise order, alternating
     *          outer and inner.
     */
    static std::vector<glm::vec2> GetStarVertices(
        glm::vec2 center, float outerRadius, float innerRadius, int points, float rotation);

  private:
    // The SDF shader's constant buffer layout. `shapeType` selects which SDF
    // function to use; the other fields are the parameters that shape needs.
    // For lines, `tri[0]` and `tri[1]` hold the two endpoints in normalized
    // [-1, 1] quad space.
    struct SdfParams {
        uint32_t shapeType;
        float halfThickness;
        uint32_t count;
        float starRatio;
        glm::vec2 shapeSize;
        glm::vec2 tri[3];
    };

    void EmitQuad(
        glm::vec2 center, float quadHalf, float rotation, Color color, const SdfParams &params);

    BatchRenderer &batchRenderer;
    BlendMode blendMode;
    Shader *shader = nullptr;
    glm::mat4 transformMatrix;
};

} // namespace Lucky
