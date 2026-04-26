#pragma once

#include <memory>
#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <Lucky/Mesh.hpp>

namespace Lucky {

struct GraphicsDevice;
struct Scene3D;

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
 *
 * # What's not loaded yet
 *
 * Materials, textures, animations, embedded cameras, and lights. The
 * forward renderer's color tint suffices for opaque-but-untextured
 * preview rendering. These will land alongside the matching consumers
 * (material system, animation playback, etc.).
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

    ~Model() = default;

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
     * `rootTransform` and `colorTint` copied into each object.
     *
     * Useful for static models in test scenes; for animated or
     * per-instance content, use `ModelInstance` (TODO) instead.
     */
    void AppendToScene(Scene3D &scene, const glm::mat4 &rootTransform = glm::mat4(1.0f),
        const glm::vec3 &colorTint = glm::vec3(1.0f)) const;

  private:
    std::vector<std::unique_ptr<Mesh>> meshes;
    std::vector<NodeTemplate> nodes;
};

} // namespace Lucky
