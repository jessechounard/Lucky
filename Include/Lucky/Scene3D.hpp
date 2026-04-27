#pragma once

#include <vector>

#include <glm/glm.hpp>

namespace Lucky {

struct Material;
struct Mesh;

/**
 * Light source kinds supported by the forward renderer.
 *
 * - `Directional` — light at infinity. Only `direction` matters; the light
 *   illuminates the whole scene from one direction (a sun).
 * - `Point` — omnidirectional light at a position. `position` and `range`
 *   matter; intensity falls off with distance and cuts off at `range`.
 * - `Spot` — directional cone from a position. All position, direction,
 *   range, and the two cone angles matter.
 */
enum class LightType {
    Directional,
    Point,
    Spot,
};

/**
 * A single light source in a scene.
 *
 * Plain data: build the value, push into `Scene3D::lights`. The forward
 * renderer reads this directly into its per-frame light UBO.
 *
 * # Cone angles for spotlights
 *
 * `innerCone` and `outerCone` are stored as cosines of half-angles, not
 * angles. The renderer attenuates light smoothly between the two: full
 * intensity inside `innerCone`, zero outside `outerCone`. With `cos(theta)`
 * being a monotonically decreasing function on `[0, pi]`, larger cosines
 * correspond to smaller angles, so `innerCone >= outerCone`.
 *
 * # Shadows
 *
 * `castsShadows = true` opts a `Directional`/`Spot` light into a 2D shadow
 * map and a `Point` light into a cube shadow map. The renderer enforces
 * the per-frame caps (4 directional/spot, 2 point) by rejecting extras
 * silently rather than asserting.
 */
struct Light {
    LightType type = LightType::Directional;

    glm::vec3 position = {0.0f, 0.0f, 0.0f};
    glm::vec3 direction = {0.0f, -1.0f, 0.0f};

    glm::vec3 color = {1.0f, 1.0f, 1.0f};
    float intensity = 1.0f;

    /** Cutoff distance in world units. Used by `Point` and `Spot`. */
    float range = 10.0f;

    /** Cosine of the inner cone half-angle. Used by `Spot`. */
    float innerCone = 0.95f;

    /** Cosine of the outer cone half-angle. Used by `Spot`. */
    float outerCone = 0.85f;

    bool castsShadows = false;
};

/**
 * One drawable: a mesh placed in world space with an optional material
 * and a color tint that multiplies the material's base color.
 *
 * `mesh` and `material` are non-owned pointers; the caller keeps both
 * alive for the duration of any render call that reads this
 * `SceneObject`. `material == nullptr` is a valid request for a
 * default white-dielectric-rough material; the renderer substitutes a
 * built-in fallback so callers don't have to track one.
 *
 * `color` is multiplied into the material's `baseColorFactor` before
 * lighting. Useful for tinting the same loaded model in different
 * colors without cloning materials.
 */
struct SceneObject {
    Mesh *mesh = nullptr;
    Material *material = nullptr;
    glm::mat4 transform = glm::mat4(1.0f);
    glm::vec3 color = {1.0f, 1.0f, 1.0f};
};

/**
 * The application's per-frame view of the 3D scene.
 *
 * Holds the constant lighting environment plus the list of objects and
 * lights to draw. The renderer iterates these each frame to produce a
 * lit, optionally shadowed image.
 *
 * Camera is passed separately to the renderer because cameras are usually
 * controlled by code outside the scene description (player, cutscene,
 * editor tool).
 */
struct Scene3D {
    glm::vec3 ambientColor = {0.05f, 0.05f, 0.07f};
    std::vector<Light> lights;
    std::vector<SceneObject> objects;
};

} // namespace Lucky
