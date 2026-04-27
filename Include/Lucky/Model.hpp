#pragma once

#include <memory>
#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <Lucky/Material.hpp>
#include <Lucky/Mesh.hpp>
#include <Lucky/SkinnedMesh.hpp>

namespace Lucky {

struct GraphicsDevice;
struct Sampler;
struct Scene3D;
struct Texture;

/**
 * One node in a `Model`'s rest hierarchy.
 *
 * Stores the rest TRS (translation/rotation/scale) used by the source
 * glTF and the indices of any meshes attached to the node. `parentIndex`
 * is the index of the parent in `Model::GetNodes()`, or `-1` for roots.
 *
 * `meshIndices` references entries in `Model::GetMesh()` (rigid
 * primitives). `skinnedMeshIndices` references entries in
 * `Model::GetSkinnedMesh()` (primitives with `JOINTS_0` / `WEIGHTS_0`).
 * A single glTF mesh may produce a mix of both. Each list is in source
 * primitive order.
 *
 * `skinIndex` names the `Skin` that supplies joint matrices for this
 * node's skinned meshes, or `-1` if the node has no skin attached.
 *
 * NodeTemplate is the data side of a model: shared across all instances.
 * Per-instance animated transforms live on `ModelInstance`.
 */
struct NodeTemplate {
    std::string name;
    glm::vec3 restTranslation = {0.0f, 0.0f, 0.0f};
    glm::quat restRotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    glm::vec3 restScale = {1.0f, 1.0f, 1.0f};
    int parentIndex = -1;
    std::vector<int> meshIndices;
    std::vector<int> skinnedMeshIndices;
    int skinIndex = -1;
};

/**
 * Which TRS component an animation channel targets.
 */
enum class AnimationPath {
    Translation,
    Rotation,
    Scale,
};

/**
 * Keyframe interpolation mode for an animation channel.
 *
 * - `Step` — hold the previous keyframe's value until the next time.
 * - `Linear` — interpolate linearly (LERP for vec3, SLERP for quat).
 * - `CubicSpline` — cubic Hermite spline with author-supplied tangents.
 *   Lucky currently treats this as `Linear` since most assets either
 *   use `Linear` or pre-bake cubic curves into linear keyframes via
 *   the glTF exporter.
 */
enum class AnimationInterpolation {
    Step,
    Linear,
    CubicSpline,
};

/**
 * One keyframe in an animation channel.
 *
 * `time` is in seconds since the animation start. `value` packs three
 * components for `Translation` / `Scale` channels (xyz) or four for
 * `Rotation` channels (quaternion in glTF order, x/y/z/w).
 */
struct AnimationKeyframe {
    float time = 0.0f;
    glm::vec4 value = {0.0f, 0.0f, 0.0f, 0.0f};
};

/**
 * One animation channel: a stream of keyframes targeting a specific
 * node's TRS component.
 */
struct AnimationChannel {
    int nodeIndex = -1;
    AnimationPath path = AnimationPath::Translation;
    AnimationInterpolation interpolation = AnimationInterpolation::Linear;
    std::vector<AnimationKeyframe> keyframes;
};

/**
 * One named animation: a duration plus the channels that play during
 * that duration.
 *
 * Owned by the `Model` (rest data). `ModelInstance` references these
 * via index for playback.
 */
struct AnimationDef {
    std::string name;
    float duration = 0.0f;
    std::vector<AnimationChannel> channels;
};

/**
 * A glTF skin: an ordered list of joint nodes plus their inverse bind
 * matrices. A skinned mesh transforms each vertex by a weighted sum of
 * `jointWorld[j] * inverseBindMatrices[j]` for the four joints listed
 * in the vertex's `Vertex3DSkinned::j0..j3`.
 *
 * `jointNodes[j]` is the index of joint `j` in `Model::GetNode()`.
 * Vertex joint indices reference this array, NOT the node array
 * directly: a Skin's joint 0 is whichever node `jointNodes[0]` names.
 *
 * `skeletonRoot` is the optional `skeleton` node from glTF; `-1` if
 * unset. The renderer doesn't require it -- joint world transforms are
 * computed from the full node hierarchy regardless -- but it can be
 * useful as an "isolated armature" anchor for tooling.
 */
struct Skin {
    std::string name;
    std::vector<int> jointNodes;
    std::vector<glm::mat4> inverseBindMatrices;
    int skeletonRoot = -1;
};

/**
 * A glTF / GLB model loaded into GPU buffers.
 *
 * Loads geometry and the node hierarchy from a `.glb` or `.gltf` file
 * via cgltf. Each glTF mesh primitive becomes a Lucky `Mesh` (one vertex
 * buffer + one 32-bit index buffer); 16-bit indices in the source are
 * widened on load so all `Mesh`es share a single binding path.
 *
 * # What's loaded
 *
 * - Geometry (position, UV, normal). Missing normals default to +Y;
 *   missing UVs default to (0, 0).
 * - Node hierarchy with rest TRS and parent indices. Matrix-valued node
 *   transforms are decomposed into TRS.
 * - PBR metallic-roughness materials with embedded textures (base color,
 *   metallic-roughness, and emissive maps). One shared linear/repeat
 *   sampler per model. Materials with external (URI) textures are
 *   loaded with no texture; only embedded buffer-view images are
 *   decoded today.
 * - Animations (rigid node TRS). Translation, Rotation, and Scale
 *   channels with `Step` and `Linear` interpolation. Combine with skins
 *   (below) to drive skeletal animation; the joint nodes are animated
 *   the same way any other node is.
 * - Skins. Each glTF skin produces a `Skin` listing joint nodes and
 *   inverse bind matrices. Primitives with `JOINTS_0` / `WEIGHTS_0`
 *   attributes load as `SkinnedMesh` instead of `Mesh`. The node
 *   referencing such a primitive carries `skinIndex` to select which
 *   skin supplies its joint matrices.
 *
 * # What's not loaded yet
 *
 * Embedded cameras, lights, and non-PBR material extensions
 * (specular-glossiness, clearcoat, etc.). External-URI textures are
 * skipped. These will land alongside the matching consumers.
 *
 * # Lifetime
 *
 * Holds `Mesh` instances, each of which references the `GraphicsDevice`.
 * The `GraphicsDevice` must outlive this Model.
 */
struct Model {
    /**
     * Loads a `.glb` or `.gltf` file into GPU buffers.
     *
     * \param graphicsDevice the graphics device that owns the GPU
     *                       resources. Must outlive this Model.
     * \param path filesystem path to the model file. Forward or back
     *             slashes both work on Windows.
     * \throws std::runtime_error on parse, buffer-load, or upload failure.
     */
    Model(GraphicsDevice &graphicsDevice, const std::string &path);

    Model(const Model &) = delete;
    Model &operator=(const Model &) = delete;
    Model(Model &&) = delete;
    Model &operator=(Model &&) = delete;

    // Defined in Model.cpp because the unique_ptr members reference
    // forward-declared types (Sampler, Texture) -- the destructors need
    // those full definitions visible.
    ~Model();

    /**
     * Number of `Mesh` objects produced from the source glTF.
     *
     * Equal to the total number of primitives across all glTF meshes.
     */
    int GetMeshCount() const {
        return static_cast<int>(meshes.size());
    }

    /** Returns the `Mesh` at the given index. */
    Mesh &GetMesh(int index) {
        return *meshes[index];
    }

    const Mesh &GetMesh(int index) const {
        return *meshes[index];
    }

    /**
     * Number of `SkinnedMesh` objects produced from the source glTF.
     *
     * Equal to the count of primitives with `JOINTS_0` / `WEIGHTS_0`
     * across all glTF meshes. Zero for assets that ship no skinning.
     */
    int GetSkinnedMeshCount() const {
        return static_cast<int>(skinnedMeshes.size());
    }

    /** Returns the `SkinnedMesh` at the given index. */
    SkinnedMesh &GetSkinnedMesh(int index) {
        return *skinnedMeshes[index];
    }

    const SkinnedMesh &GetSkinnedMesh(int index) const {
        return *skinnedMeshes[index];
    }

    /** Number of skins loaded from the source glTF. */
    int GetSkinCount() const {
        return static_cast<int>(skins.size());
    }

    /** Returns the skin at the given index. */
    const Skin &GetSkin(int index) const {
        return skins[index];
    }

    /** Number of nodes in the model's hierarchy. */
    int GetNodeCount() const {
        return static_cast<int>(nodes.size());
    }

    /** Returns the node template at the given index. */
    const NodeTemplate &GetNode(int index) const {
        return nodes[index];
    }

    /** Number of animations loaded from the source glTF. */
    int GetAnimationCount() const {
        return static_cast<int>(animations.size());
    }

    /** Returns the animation at the given index. */
    const AnimationDef &GetAnimation(int index) const {
        return animations[index];
    }

    /**
     * Looks up an animation by name. Returns -1 if no animation in the
     * model matches.
     */
    int FindAnimation(const std::string &name) const;

    /** Number of materials loaded from the source glTF. */
    int GetMaterialCount() const {
        return static_cast<int>(materials.size());
    }

    /**
     * Returns the material at the given index. Pointer stays stable for
     * the Model's lifetime (storage is reserved up front).
     */
    Material *GetMaterial(int index) {
        return &materials[index];
    }

    /**
     * Returns the material associated with the given mesh, or nullptr
     * if the source glTF primitive had no material binding.
     */
    Material *GetMaterialForMesh(int meshIndex) {
        const int matIdx = meshMaterialIndices[meshIndex];
        return (matIdx >= 0) ? &materials[matIdx] : nullptr;
    }

    /**
     * Returns the material associated with the given skinned mesh, or
     * nullptr if the source glTF primitive had no material binding.
     */
    Material *GetMaterialForSkinnedMesh(int skinnedMeshIndex) {
        const int matIdx = skinnedMeshMaterialIndices[skinnedMeshIndex];
        return (matIdx >= 0) ? &materials[matIdx] : nullptr;
    }

    /**
     * Computes world-space transforms for every node from its rest pose.
     *
     * Output is sized to `GetNodeCount()`. `rootTransform` is left-
     * multiplied so the model can be placed in the scene without
     * touching the per-node TRS data.
     */
    void ComputeRestWorldTransforms(
        std::vector<glm::mat4> &out, const glm::mat4 &rootTransform = glm::mat4(1.0f)) const;

    /**
     * Convenience: appends one `SceneObject` per (node, mesh) pair to
     * `scene.objects`, with rest-pose world transforms pre-multiplied by
     * `rootTransform` and `colorTint` copied into each object. Each
     * SceneObject's material pointer is set to the primitive's loaded
     * Material, or null if the primitive had no material binding.
     *
     * Useful for static models in test scenes; for animated or
     * per-instance content, use `ModelInstance` instead. Non-const
     * because it hands out non-const pointers into the model's
     * material storage.
     */
    void AppendToScene(Scene3D &scene, const glm::mat4 &rootTransform = glm::mat4(1.0f),
        const glm::vec3 &colorTint = glm::vec3(1.0f));

  private:
    std::vector<std::unique_ptr<Mesh>> meshes;
    std::vector<int> meshMaterialIndices; // parallel to meshes; -1 = no material
    std::vector<std::unique_ptr<SkinnedMesh>> skinnedMeshes;
    std::vector<int> skinnedMeshMaterialIndices; // parallel to skinnedMeshes; -1 = no material
    std::vector<NodeTemplate> nodes;
    std::vector<Material> materials;
    std::vector<std::unique_ptr<Texture>> textures;
    std::unique_ptr<Sampler> materialSampler;
    std::vector<AnimationDef> animations;
    std::vector<Skin> skins;
};

} // namespace Lucky
