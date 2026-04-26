#pragma once

#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Lucky {

struct Model;
struct Scene3D;

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
     * Returns the cached world transforms, recomputing if any TRS or
     * the root transform has changed since the last call.
     */
    const std::vector<glm::mat4> &GetWorldTransforms() const;

    /**
     * Appends one `SceneObject` per (node, mesh) pair to `scene.objects`,
     * using the current world transforms.
     */
    void AppendToScene(Scene3D &scene, const glm::vec3 &colorTint = glm::vec3(1.0f)) const;

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

    // Mutable cache: invalidated when any TRS or root transform changes,
    // recomputed by GetWorldTransforms / AppendToScene.
    mutable std::vector<glm::mat4> worldTransforms;
    mutable bool dirty = true;
};

} // namespace Lucky
