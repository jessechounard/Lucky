#pragma once

#include <memory>
#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <Lucky/Material.hpp>
#include <Lucky/Mesh.hpp>

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
 * `meshIndices` references entries in `Model::GetMesh()`. A glTF mesh
 * with multiple primitives produces one Lucky `Mesh` per primitive, so
 * a single node may reference several mesh indices.
 *
 * NodeTemplate is the data side of a model: shared across all instances.
 * Per-instance animated transforms live on `ModelInstance` (see future
 * task).
 */
struct NodeTemplate {
    std::string name;
    glm::vec3 restTranslation = {0.0f, 0.0f, 0.0f};
    glm::quat restRotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    glm::vec3 restScale = {1.0f, 1.0f, 1.0f};
    int parentIndex = -1;
    std::vector<int> meshIndices;
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
 * - PBR metallic-roughness materials with embedded textures (base color
 *   and metallic-roughness maps). One shared linear/repeat sampler per
 *   model. Materials with external (URI) textures are loaded with no
 *   texture; only embedded buffer-view images are decoded today.
 *
 * # What's not loaded yet
 *
 * Animations, embedded cameras, lights, and non-PBR material extensions
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

    /** Number of nodes in the model's hierarchy. */
    int GetNodeCount() const {
        return static_cast<int>(nodes.size());
    }

    /** Returns the node template at the given index. */
    const NodeTemplate &GetNode(int index) const {
        return nodes[index];
    }

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
    std::vector<NodeTemplate> nodes;
    std::vector<Material> materials;
    std::vector<std::unique_ptr<Texture>> textures;
    std::unique_ptr<Sampler> materialSampler;
};

} // namespace Lucky
