#include <stdexcept>

#include <SDL3/SDL_assert.h>
#include <cgltf.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <spdlog/spdlog.h>

#include <Lucky/Model.hpp>
#include <Lucky/Scene3D.hpp>

namespace Lucky {

namespace {

struct CgltfDataDeleter {
    void operator()(cgltf_data *p) const {
        if (p) {
            cgltf_free(p);
        }
    }
};
using CgltfDataPtr = std::unique_ptr<cgltf_data, CgltfDataDeleter>;

void DecomposeNodeTransform(const cgltf_node &node, NodeTemplate &dst) {
    if (node.has_matrix) {
        glm::mat4 m = glm::make_mat4(node.matrix);
        dst.restTranslation = glm::vec3(m[3]);
        dst.restScale = glm::vec3(glm::length(glm::vec3(m[0])),
            glm::length(glm::vec3(m[1])),
            glm::length(glm::vec3(m[2])));
        const glm::mat3 rot(glm::vec3(m[0]) / dst.restScale.x,
            glm::vec3(m[1]) / dst.restScale.y,
            glm::vec3(m[2]) / dst.restScale.z);
        dst.restRotation = glm::quat_cast(rot);
        return;
    }

    dst.restTranslation =
        node.has_translation
            ? glm::vec3(node.translation[0], node.translation[1], node.translation[2])
            : glm::vec3(0.0f);
    // glTF stores quaternions as (x, y, z, w); glm::quat ctor takes (w, x, y, z).
    dst.restRotation =
        node.has_rotation
            ? glm::quat(node.rotation[3], node.rotation[0], node.rotation[1], node.rotation[2])
            : glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    dst.restScale =
        node.has_scale ? glm::vec3(node.scale[0], node.scale[1], node.scale[2]) : glm::vec3(1.0f);
}

// Read a primitive's attributes into Lucky's Vertex3D format. Missing UVs
// default to (0, 0); missing normals default to +Y so the surface still
// shows up under the renderer's lighting model.
bool BuildPrimitiveData(const cgltf_primitive &prim, MeshData &out) {
    const cgltf_accessor *posAccessor = nullptr;
    const cgltf_accessor *uvAccessor = nullptr;
    const cgltf_accessor *normalAccessor = nullptr;
    for (cgltf_size a = 0; a < prim.attributes_count; a++) {
        const cgltf_attribute &attr = prim.attributes[a];
        if (attr.type == cgltf_attribute_type_position) {
            posAccessor = attr.data;
        } else if (attr.type == cgltf_attribute_type_texcoord && attr.index == 0) {
            uvAccessor = attr.data;
        } else if (attr.type == cgltf_attribute_type_normal) {
            normalAccessor = attr.data;
        }
    }

    if (!posAccessor || !prim.indices) {
        return false;
    }

    const size_t vertexCount = posAccessor->count;
    out.vertices.resize(vertexCount);
    for (size_t i = 0; i < vertexCount; i++) {
        float pos[3] = {0, 0, 0};
        float uv[2] = {0, 0};
        float nrm[3] = {0, 1, 0};
        cgltf_accessor_read_float(posAccessor, i, pos, 3);
        if (uvAccessor) {
            cgltf_accessor_read_float(uvAccessor, i, uv, 2);
        }
        if (normalAccessor) {
            cgltf_accessor_read_float(normalAccessor, i, nrm, 3);
        }
        Vertex3D &v = out.vertices[i];
        v.x = pos[0];
        v.y = pos[1];
        v.z = pos[2];
        v.u = uv[0];
        v.v = uv[1];
        v.nx = nrm[0];
        v.ny = nrm[1];
        v.nz = nrm[2];
    }

    const size_t indexCount = prim.indices->count;
    out.indices.resize(indexCount);
    for (size_t i = 0; i < indexCount; i++) {
        out.indices[i] = static_cast<uint32_t>(cgltf_accessor_read_index(prim.indices, i));
    }
    return true;
}

} // namespace

Model::Model(GraphicsDevice &graphicsDevice, const std::string &path) {
    cgltf_options options{};
    cgltf_data *raw = nullptr;
    cgltf_result result = cgltf_parse_file(&options, path.c_str(), &raw);
    if (result != cgltf_result_success) {
        spdlog::error("cgltf_parse_file failed for '{}': {}", path, static_cast<int>(result));
        throw std::runtime_error("Failed to parse glTF file: " + path);
    }
    CgltfDataPtr data(raw);

    result = cgltf_load_buffers(&options, data.get(), path.c_str());
    if (result != cgltf_result_success) {
        spdlog::error("cgltf_load_buffers failed for '{}': {}", path, static_cast<int>(result));
        throw std::runtime_error("Failed to load glTF buffers: " + path);
    }

    // Walk meshes once, building one Lucky Mesh per primitive. Map glTF
    // mesh -> first Lucky mesh index so node-mesh references can be
    // resolved later (and duplicate cgltf_mesh* across nodes share GPU
    // buffers).
    struct MeshMapping {
        const cgltf_mesh *cgMesh;
        int firstMeshIndex;
        int primitiveCount;
    };
    std::vector<MeshMapping> meshMap;
    meshMap.reserve(data->meshes_count);

    for (cgltf_size m = 0; m < data->meshes_count; m++) {
        const cgltf_mesh &cgMesh = data->meshes[m];
        MeshMapping mapping;
        mapping.cgMesh = &cgMesh;
        mapping.firstMeshIndex = static_cast<int>(meshes.size());
        mapping.primitiveCount = 0;

        for (cgltf_size p = 0; p < cgMesh.primitives_count; p++) {
            MeshData meshData;
            if (!BuildPrimitiveData(cgMesh.primitives[p], meshData)) {
                continue;
            }
            meshes.push_back(std::make_unique<Mesh>(graphicsDevice, meshData));
            mapping.primitiveCount++;
        }
        meshMap.push_back(mapping);
    }

    // Build node templates. Parent indices reference the same array.
    nodes.resize(data->nodes_count);
    for (cgltf_size n = 0; n < data->nodes_count; n++) {
        const cgltf_node &cgNode = data->nodes[n];
        NodeTemplate &dst = nodes[n];

        dst.name = cgNode.name ? cgNode.name : ("node_" + std::to_string(n));
        DecomposeNodeTransform(cgNode, dst);
        dst.parentIndex = cgNode.parent ? static_cast<int>(cgNode.parent - data->nodes) : -1;

        if (cgNode.mesh) {
            for (const MeshMapping &mapping : meshMap) {
                if (mapping.cgMesh == cgNode.mesh) {
                    dst.meshIndices.reserve(mapping.primitiveCount);
                    for (int j = 0; j < mapping.primitiveCount; j++) {
                        dst.meshIndices.push_back(mapping.firstMeshIndex + j);
                    }
                    break;
                }
            }
        }
    }

    spdlog::info("Loaded glTF '{}': {} mesh(es), {} node(s)", path, meshes.size(), nodes.size());
}

void Model::ComputeRestWorldTransforms(
    std::vector<glm::mat4> &out, const glm::mat4 &rootTransform) const {
    out.assign(nodes.size(), glm::mat4(1.0f));
    // Single pass with up-to-root recursion (same approach as PS5_SDL_Test
    // — handles arbitrary node ordering since glTF doesn't promise
    // children come after parents).
    std::vector<bool> computed(nodes.size(), false);
    auto compute = [&](auto &self, int i) -> void {
        if (computed[i]) {
            return;
        }
        const NodeTemplate &n = nodes[i];
        const glm::mat4 local = glm::translate(glm::mat4(1.0f), n.restTranslation) *
                                glm::mat4_cast(n.restRotation) *
                                glm::scale(glm::mat4(1.0f), n.restScale);
        if (n.parentIndex < 0) {
            out[i] = rootTransform * local;
        } else {
            self(self, n.parentIndex);
            out[i] = out[n.parentIndex] * local;
        }
        computed[i] = true;
    };
    for (size_t i = 0; i < nodes.size(); i++) {
        compute(compute, static_cast<int>(i));
    }
}

void Model::AppendToScene(
    Scene3D &scene, const glm::mat4 &rootTransform, const glm::vec3 &colorTint) const {
    std::vector<glm::mat4> world;
    ComputeRestWorldTransforms(world, rootTransform);
    for (size_t i = 0; i < nodes.size(); i++) {
        const NodeTemplate &node = nodes[i];
        for (int meshIdx : node.meshIndices) {
            scene.objects.push_back({meshes[meshIdx].get(), world[i], colorTint});
        }
    }
}

} // namespace Lucky
