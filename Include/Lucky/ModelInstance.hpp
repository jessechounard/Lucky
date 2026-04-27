#pragma once

#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Lucky {

struct Model;
struct Scene3D;

/**
 * Per-instance playback state for one of the bound `Model`'s
 * animations.
 *
 * `time` is the current playhead in seconds since the animation
 * started. `playing` gates whether `ModelInstance::Update` advances
 * this animation. `loop` controls behavior when `time` exceeds the
 * animation's duration: wrap to zero (loop) or clamp to the last
 * frame and stop (one-shot).
 */
struct AnimationPlayback {
    float time = 0.0f;
    bool playing = false;
    bool loop = false;
};

/**
 * One placement of a `Model` in the world, with per-node TRS values
 * separate from the shared rest pose.
 *
 * The instance starts with each node's TRS copied from the Model's
 * rest pose, plus an identity root transform. Mutate any node's
 * translation/rotation/scale or the root transform between frames; the
 * world matrices are recomputed lazily before the next read or scene
 * append.
 *
 * Multiple `ModelInstance`s sharing the same `Model` reuse one set of
 * GPU buffers, with per-instance world placement layered on top. This
 * is the seam animation playback will hook into when that lands -- it
 * writes into the per-node TRS each frame and lets the dirty flag
 * trigger recomputation.
 *
 * # Lifetime
 *
 * Holds a pointer to the underlying `Model`; the Model must outlive
 * any of its instances.
 */
struct ModelInstance {
    /**
     * Builds an instance bound to `model` with all nodes at the model's
     * rest TRS and an identity root transform.
     */
    explicit ModelInstance(Model &model);

    /**
     * Sets the world-space transform layered on top of every node's
     * local transform. Default: identity.
     */
    void SetRootTransform(const glm::mat4 &transform);

    const glm::mat4 &GetRootTransform() const {
        return rootTransform;
    }

    /**
     * Overwrites the TRS for a single node. Asserts on out-of-range index.
     */
    void SetNodeTransform(int nodeIndex, const glm::vec3 &translation, const glm::quat &rotation,
        const glm::vec3 &scale);

    /**
     * Resets every node's TRS to the bound Model's rest values. The
     * root transform is left alone -- call `SetRootTransform(mat4(1))`
     * separately if you want a full reset.
     */
    void ResetToRestPose();

    /**
     * Starts playback of one of the bound Model's animations.
     *
     * Resets the playback's time to 0 and begins advancing it on
     * subsequent `Update` calls. Index out of range is silently
     * ignored. Multiple animations can play simultaneously; their
     * channels are written in playback order so later animations
     * overwrite earlier ones (no blending in this tier).
     *
     * \param index animation index in the Model's list, in
     *              `[0, GetModel().GetAnimationCount())`.
     * \param loop if true, wraps `time` back to 0 when it exceeds the
     *             animation's duration; otherwise clamps and stops.
     */
    void PlayAnimation(int index, bool loop = true);

    /**
     * Stops playback of the given animation. Out-of-range indices are
     * silently ignored. The animation's playhead is preserved; calling
     * `PlayAnimation` again resets it to 0.
     */
    void StopAnimation(int index);

    /** Stops all currently-playing animations on this instance. */
    void StopAllAnimations();

    /**
     * Advances the playhead of every playing animation by `deltaTime`
     * seconds, samples each channel, and writes the result into the
     * affected nodes' TRS values. Marks the world-transform cache
     * dirty if any animation produced a write.
     *
     * Channels not driven by any playing animation keep whatever value
     * was last written to them (rest pose by default, or a value the
     * caller wrote via `SetNodeTransform`). This is the seam where
     * procedural animation (look-at, IK, springs) layers on top: call
     * `Update`, then call `SetNodeTransform` for any procedurally-
     * driven joints, then `AppendToScene`.
     */
    void Update(float deltaTime);

    /**
     * Returns the cached world transforms, recomputing if any TRS or
     * the root transform has changed since the last call.
     */
    const std::vector<glm::mat4> &GetWorldTransforms() const;

    /**
     * Returns the per-joint world matrices for the given skin (one per
     * entry in `Skin::jointNodes`). Each matrix is
     * `worldTransform[jointNode] * Skin::inverseBindMatrices[j]`, the
     * exact matrix the skinned vertex shader multiplies by per-vertex
     * weight.
     *
     * The returned reference is valid until the bound `Model` is
     * destroyed; the data inside is refreshed implicitly on the next
     * `Update`, `SetNodeTransform`, or `SetRootTransform`.
     *
     * \param skinIndex skin index in `[0, GetModel().GetSkinCount())`.
     */
    const std::vector<glm::mat4> &GetSkinJointMatrices(int skinIndex) const;

    /**
     * Appends one `SceneObject` per (node, mesh) pair to `scene.objects`,
     * using the current world transforms. Material pointers are resolved
     * from the bound Model, so the renderer sees the same materials it
     * would for a static `Model::AppendToScene`. Non-const because the
     * Material pointers handed out are non-const.
     */
    void AppendToScene(Scene3D &scene, const glm::vec3 &colorTint = glm::vec3(1.0f));

    /** Returns the bound Model. */
    Model &GetModel() {
        return *model;
    }

    const Model &GetModel() const {
        return *model;
    }

  private:
    void RecomputeWorldTransformsIfDirty() const;

    Model *model;
    std::vector<glm::vec3> translations;
    std::vector<glm::quat> rotations;
    std::vector<glm::vec3> scales;
    glm::mat4 rootTransform = glm::mat4(1.0f);

    // Per-animation playback state, parallel to `model->animations`.
    // Sized once in the constructor so `playbacks[i]` is always valid
    // for a valid animation index.
    std::vector<AnimationPlayback> playbacks;

    // Mutable cache: invalidated when any TRS or root transform changes,
    // recomputed by GetWorldTransforms / AppendToScene.
    mutable std::vector<glm::mat4> worldTransforms;

    // Mutable cache: per-skin joint matrix arrays
    // (`worldTransforms[joint] * inverseBindMatrix[joint]`), refreshed
    // alongside `worldTransforms`. Sized once in the constructor so the
    // pointer to `skinJointMatrices[i]` is stable for the instance's
    // lifetime; the inner vectors keep their capacity once filled.
    mutable std::vector<std::vector<glm::mat4>> skinJointMatrices;

    mutable bool dirty = true;
};

} // namespace Lucky
