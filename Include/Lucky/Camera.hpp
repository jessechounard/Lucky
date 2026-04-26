#pragma once

#include <glm/glm.hpp>

namespace Lucky {

/**
 * A perspective camera defined by world-space position and yaw/pitch
 * angles.
 *
 * The camera looks down the -Z axis when `yaw == 0` and `pitch == 0`, with
 * world up at +Y. Yaw rotates around world Y (turning left/right); pitch
 * rotates around the camera-local X (looking up/down). Pitch should be
 * clamped to (-pi/2, pi/2) to avoid gimbal flip; the update helpers do
 * this for you.
 *
 * The struct is plain data: change fields freely between frames, then
 * call `GetViewMatrix()` / `GetProjectionMatrix()` to feed shaders.
 *
 * # Choosing a movement style
 *
 * Lucky ships two free-function update helpers that overwrite Camera
 * fields each frame from input deltas:
 *
 * - `UpdateFlyCamera` -- first-person fly-through. Position is driven by
 *   local-space WASD-style movement; yaw/pitch is driven by mouse look.
 * - `UpdateOrbitCamera` -- orbits around an external `OrbitCameraState`
 *   target at a configurable distance. Yaw/pitch swing the camera
 *   around the target; distance zooms in/out.
 *
 * Both helpers take pre-collected input deltas (already scaled by your
 * sensitivity choices) so the Camera stays decoupled from any specific
 * input source. Demos compose Lucky::Mouse / Lucky::Keyboard /
 * Lucky::Gamepad state into the input struct themselves.
 */
struct Camera {
    glm::vec3 position = {0.0f, 0.0f, 5.0f};

    /** Rotation around world Y axis, in radians. 0 looks down -Z. */
    float yaw = 0.0f;

    /** Rotation around camera-local X axis, in radians. 0 is level. */
    float pitch = 0.0f;

    /** Vertical field of view, in radians. */
    float fovY = glm::radians(60.0f);

    /** Near plane distance in world units. Must be positive. */
    float zNear = 0.01f;

    /** Far plane distance in world units. Must be greater than `zNear`. */
    float zFar = 100.0f;

    /**
     * Returns the unit forward vector in world space (the direction the
     * camera looks).
     */
    glm::vec3 GetForward() const;

    /**
     * Returns the unit right vector in world space, perpendicular to
     * forward and world up.
     */
    glm::vec3 GetRight() const;

    /**
     * Returns the unit up vector in world space (right x forward).
     */
    glm::vec3 GetUp() const;

    /**
     * Returns the world-to-view matrix.
     */
    glm::mat4 GetViewMatrix() const;

    /**
     * Returns the perspective projection matrix.
     *
     * \param aspectRatio width / height of the target. Must be positive.
     */
    glm::mat4 GetProjectionMatrix(float aspectRatio) const;
};

/**
 * Per-frame input for `UpdateFlyCamera`. All fields are pre-scaled by the
 * caller; the helper does not apply sensitivity multipliers.
 */
struct FlyCameraInput {
    /**
     * Local-space movement direction. `x` is right, `y` is up,
     * `z` is forward. Magnitude scales the resulting move; the helper
     * normalizes any non-zero vector before applying `moveSpeed`.
     */
    glm::vec3 moveLocal = {0.0f, 0.0f, 0.0f};

    /** Yaw delta in radians (positive turns left). */
    float yawDelta = 0.0f;

    /** Pitch delta in radians (positive looks up). */
    float pitchDelta = 0.0f;

    /** World units per second when `moveLocal` has length 1. */
    float moveSpeed = 3.0f;
};

/**
 * Updates a fly-through camera in place from per-frame input.
 *
 * Applies `yawDelta` / `pitchDelta` directly; clamps pitch to
 * (-pi/2 + epsilon, pi/2 - epsilon). Translates position by
 * `moveLocal`-rotated-by-yaw-and-pitch scaled by `moveSpeed * deltaTime`.
 * The y component of `moveLocal` always moves along world up regardless
 * of pitch -- typical "Q/E up/down" feels natural in fly mode.
 */
void UpdateFlyCamera(Camera &camera, const FlyCameraInput &input, float deltaTime);

/**
 * State owned by an orbit-camera demo, separate from the Camera itself.
 *
 * The Camera's position is recomputed each frame from `target`,
 * `distance`, and the camera's yaw/pitch. `target` and `distance` are
 * the persistent state the demo manipulates between frames.
 */
struct OrbitCameraState {
    glm::vec3 target = {0.0f, 0.0f, 0.0f};
    float distance = 5.0f;
};

/**
 * Per-frame input for `UpdateOrbitCamera`.
 */
struct OrbitCameraInput {
    /** Yaw delta in radians; positive sweeps the camera left around the target. */
    float yawDelta = 0.0f;

    /** Pitch delta in radians; positive sweeps the camera up over the target. */
    float pitchDelta = 0.0f;

    /**
     * Multiplicative distance change. `1.0` keeps distance unchanged;
     * `<1.0` zooms in, `>1.0` zooms out. Caller typically derives this
     * from a wheel-delta exponent like `pow(0.9, wheel)`.
     */
    float distanceScale = 1.0f;

    /** Minimum allowed distance after applying `distanceScale`. */
    float minDistance = 0.5f;

    /** Maximum allowed distance after applying `distanceScale`. */
    float maxDistance = 100.0f;
};

/**
 * Updates an orbit camera in place from per-frame input.
 *
 * Mutates both the Camera (rewrites `position` based on the new yaw/pitch
 * and clamped distance) and the `OrbitCameraState` (updates `distance`).
 * The Camera's `yaw` / `pitch` fields hold the orbit angles and are
 * advanced by the input deltas. Pitch is clamped the same way as the
 * fly-camera helper.
 */
void UpdateOrbitCamera(Camera &camera, OrbitCameraState &state, const OrbitCameraInput &input);

} // namespace Lucky
