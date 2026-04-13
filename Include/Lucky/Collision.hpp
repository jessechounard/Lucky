#pragma once

#include <vector>

#include <glm/glm.hpp>

namespace Lucky {

/**
 * Identifies whether the extremal component of a projected polygon is a
 * single vertex or an edge shared between two vertices.
 */
enum class PolygonComponent {
    Vertex,
    Edge,
};

/**
 * A 2D convex collider used by the SAT collision functions.
 *
 * Prefer MakeCollider() to build a Collider from a list of vertices; it
 * populates `collisionAxes` and `center` automatically. Constructing a
 * Collider by hand is supported but error-prone: callers must populate
 * `collisionAxes` or collision queries will fail assertions.
 */
struct Collider {
    /** The polygon vertices, in world space relative to the collider origin. */
    std::vector<glm::vec2> vertices;

    /**
     * The separating-axis candidates for this collider. For a convex polygon
     * these are the outward (or inward — SAT is direction-agnostic) normals
     * of each edge. Must be non-empty when passed to FindCollision.
     */
    std::vector<glm::vec2> collisionAxes;

    /** The centroid of `vertices`, used to disambiguate the collision normal. */
    glm::vec2 center = {0, 0};

    /**
     * If true, this collider only collides from one side, like a one-way
     * platform. `oneWayDirection` must be non-zero when this is true.
     */
    bool isOneWay = false;

    /**
     * The outward direction from which collisions are allowed. A contact
     * whose axis points in the same direction as `oneWayDirection` is
     * accepted; other contacts are ignored.
     */
    glm::vec2 oneWayDirection = {0, 0};
};

/**
 * Builds a Collider from a convex polygon's vertices.
 *
 * Computes `collisionAxes` as the normalized edge normals and `center` as the
 * centroid of the vertices. The resulting collider is ready to pass to
 * FindCollision without further setup.
 *
 * The input polygon must be convex. Non-convex shapes produce colliders that
 * report incorrect results.
 *
 * \param vertices the vertices of a convex polygon, in any winding order.
 *                 Must contain at least 3 vertices.
 * \returns a fully-initialized Collider.
 */
Collider MakeCollider(std::vector<glm::vec2> vertices);

/**
 * Tests whether a point lies inside a collider.
 *
 * The point is considered inside if, for every collision axis, the point's
 * projection falls within the collider's projected interval (inclusive on
 * both ends).
 *
 * \param point the point to test, in world space.
 * \param collider the collider to test against. Must have `collisionAxes`
 *                 populated; use MakeCollider() to build one.
 * \param position the world-space offset applied to the collider's vertices.
 * \returns true if `point` lies inside the collider at `position`.
 *
 * \note The one-way flag on `collider` is ignored by this overload.
 */
bool FindCollision(const glm::vec2 &point, const Collider &collider, const glm::vec2 &position);

/**
 * Tests whether two colliders collide during a unit time step.
 *
 * This is a swept SAT test: it checks not only whether `colliderA` and
 * `colliderB` currently overlap, but whether they will overlap at some point
 * in the time range `[0, 1]` given their relative motion.
 *
 * `relativePosition` and `relativeVelocity` both describe A's state relative
 * to B. Callers should scale `relativeVelocity` by the frame delta time
 * before calling, since the function rejects collisions with `time > 1`.
 *
 * On a successful collision, `time` is set to the time of impact in the
 * range `[-1, 1]`. Negative values indicate the colliders are currently
 * overlapping, with larger negative values representing deeper penetration.
 * `axis` is set to a vector pointing from B's center toward A's center along
 * the separating axis.
 *
 * \param colliderA the first collider. Must have `collisionAxes` populated.
 * \param colliderB the second collider. Must have `collisionAxes` populated.
 * \param relativePosition A's position relative to B.
 * \param relativeVelocity A's velocity relative to B, scaled by delta time.
 * \param axis output: the collision normal pointing from B toward A.
 * \param time output: the time of impact in `[-1, 1]`.
 * \returns true if the colliders collide within the time step.
 */
bool FindCollision(const Collider &colliderA, const Collider &colliderB,
    const glm::vec2 &relativePosition, const glm::vec2 &relativeVelocity, glm::vec2 &axis,
    float &time);

/**
 * Finds the vertex or edge of a polygon furthest along a given direction.
 *
 * Scans the polygon's vertices, projecting each onto `direction`. The vertex
 * with the largest projection is stored in `vertexIndexA`. If another vertex
 * ties for the same maximum projection (within ApproximatelyEqual tolerance),
 * its index is stored in `vertexIndexB` and the return value is Edge;
 * otherwise the return value is Vertex.
 *
 * \param vertices the polygon's vertices. Must be non-empty.
 * \param direction the direction to search along.
 * \param vertexIndexA output: the index of the vertex with the largest
 *                     projection.
 * \param vertexIndexB output: the index of the second vertex when an edge is
 *                     returned. Set to `vertexIndexA` when a vertex is
 *                     returned.
 * \returns PolygonComponent::Edge when two vertices tie for the maximum
 *          projection, PolygonComponent::Vertex otherwise.
 */
PolygonComponent GetMaximumPolygonComponent(const std::vector<glm::vec2> &vertices,
    const glm::vec2 &direction, int &vertexIndexA, int &vertexIndexB);

/**
 * Computes the minimum and maximum projection of a set of vertices onto an
 * axis.
 *
 * \param vertices the vertices to project.
 * \param axis the axis to project onto. Does not need to be normalized, but
 *             the resulting interval is scaled by the axis length if not.
 * \param min output: the smallest projection value.
 * \param max output: the largest projection value.
 *
 * \note If `vertices` is empty, `min` is set to positive infinity and `max`
 *       to negative infinity.
 */
void CalculateProjectedInterval(
    const std::vector<glm::vec2> &vertices, const glm::vec2 &axis, float &min, float &max);

/**
 * Computes the minimum and maximum projection of a set of translated
 * vertices onto an axis.
 *
 * Equivalent to translating each vertex by `offset` before projecting.
 *
 * \param vertices the vertices to project.
 * \param offset a world-space offset applied to each vertex before
 *               projection.
 * \param axis the axis to project onto.
 * \param min output: the smallest projection value.
 * \param max output: the largest projection value.
 */
void CalculateProjectedInterval(const std::vector<glm::vec2> &vertices, const glm::vec2 &offset,
    const glm::vec2 &axis, float &min, float &max);

} // namespace Lucky
