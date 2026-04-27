#include <stdexcept>

#include <SDL3/SDL_assert.h>
#include <cgltf.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <spdlog/spdlog.h>

#include <Lucky/Model.hpp>
#include <Lucky/Sampler.hpp>
#include <Lucky/Scene3D.hpp>
#include <Lucky/Texture.hpp>

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

// Decode a glTF embedded image into a sampleable Lucky::Texture. Returns
// nullptr if the image has no buffer view (external URI -- not
// supported yet) or stb_image rejects the encoded bytes.
std::unique_ptr<Texture> LoadEmbeddedImage(
    GraphicsDevice &graphicsDevice, const cgltf_image &image) {
    if (!image.buffer_view) {
        spdlog::warn("glTF image '{}' has no buffer view (external URI not supported)",
            image.name ? image.name : "(unnamed)");
        return nullptr;
    }
    const void *imgPtr = cgltf_buffer_view_data(image.buffer_view);
    const auto byteCount = static_cast<uint32_t>(image.buffer_view->size);
    try {
        // Lucky::Texture's memory ctor takes a non-const pointer; stb_image
        // reads but does not write the source bytes, so the const_cast is
        // safe in practice.
        return std::make_unique<Texture>(graphicsDevice,
            const_cast<uint8_t *>(static_cast<const uint8_t *>(imgPtr)),
            byteCount,
            TextureFilter::Linear,
            TextureFormat::Normal);
    } catch (const std::exception &e) {
        spdlog::warn("Failed to decode glTF image '{}': {}",
            image.name ? image.name : "(unnamed)",
            e.what());
        return nullptr;
    }
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

    // One sampler shared by every material in this model. Linear/repeat
    // matches the typical glTF authoring assumption; per-sampler glTF
    // attributes are ignored for now.
    SamplerDescription sampDesc;
    sampDesc.filter = SamplerFilter::Linear;
    sampDesc.mipmapFilter = SamplerFilter::Linear;
    sampDesc.addressU = SamplerAddressMode::Repeat;
    sampDesc.addressV = SamplerAddressMode::Repeat;
    sampDesc.addressW = SamplerAddressMode::Repeat;
    materialSampler = std::make_unique<Sampler>(graphicsDevice, sampDesc);

    // Decode embedded images into Lucky::Texture instances. textures[i]
    // may be nullptr if the source image had no buffer view or failed
    // to decode; materials skip texture pointers in that case.
    textures.reserve(data->images_count);
    for (cgltf_size i = 0; i < data->images_count; i++) {
        textures.push_back(LoadEmbeddedImage(graphicsDevice, data->images[i]));
    }

    // Materials. Reserve to final size so &materials[i] stays stable
    // through pushes (the SceneObject::material pointer relies on this).
    materials.reserve(data->materials_count);
    for (cgltf_size m = 0; m < data->materials_count; m++) {
        const cgltf_material &mat = data->materials[m];
        Material material;
        material.sampler = materialSampler.get();

        if (mat.has_pbr_metallic_roughness) {
            const auto &pbr = mat.pbr_metallic_roughness;
            material.baseColorFactor = glm::vec4(pbr.base_color_factor[0],
                pbr.base_color_factor[1],
                pbr.base_color_factor[2],
                pbr.base_color_factor[3]);
            material.metallicFactor = pbr.metallic_factor;
            material.roughnessFactor = pbr.roughness_factor;

            if (pbr.base_color_texture.texture && pbr.base_color_texture.texture->image) {
                const int idx =
                    static_cast<int>(pbr.base_color_texture.texture->image - data->images);
                if (idx >= 0 && idx < static_cast<int>(textures.size())) {
                    material.baseColorTexture = textures[idx].get();
                }
            }
            if (pbr.metallic_roughness_texture.texture &&
                pbr.metallic_roughness_texture.texture->image) {
                const int idx =
                    static_cast<int>(pbr.metallic_roughness_texture.texture->image - data->images);
                if (idx >= 0 && idx < static_cast<int>(textures.size())) {
                    material.metallicRoughnessTexture = textures[idx].get();
                }
            }
        }

        // glTF semantics: emissive_factor is multiplied by the
        // emissive_texture sample. The shader handles the multiply;
        // we just pass both through. (Without the texture, the
        // factor IS the emission for the whole surface -- typically
        // (0,0,0) unless the asset wants uniform glow.)
        material.emissiveFactor = glm::vec3(
            mat.emissive_factor[0], mat.emissive_factor[1], mat.emissive_factor[2]);
        if (mat.emissive_texture.texture && mat.emissive_texture.texture->image) {
            const int idx =
                static_cast<int>(mat.emissive_texture.texture->image - data->images);
            if (idx >= 0 && idx < static_cast<int>(textures.size())) {
                material.emissiveTexture = textures[idx].get();
            }
        }

        materials.push_back(material);
    }

    // One Lucky Mesh per primitive; remember which cgltf_mesh maps to
    // which range so node-mesh references resolve to the correct GPU
    // buffers, and record each primitive's material index in parallel.
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
            const cgltf_primitive &prim = cgMesh.primitives[p];
            MeshData meshData;
            if (!BuildPrimitiveData(prim, meshData)) {
                continue;
            }
            meshes.push_back(std::make_unique<Mesh>(graphicsDevice, meshData));
            const int matIndex =
                prim.material ? static_cast<int>(prim.material - data->materials) : -1;
            meshMaterialIndices.push_back(matIndex);
            mapping.primitiveCount++;
        }
        meshMap.push_back(mapping);
    }

    // Node hierarchy.
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

    spdlog::info("Loaded glTF '{}': {} mesh(es), {} material(s), {} texture(s), {} node(s)",
        path,
        meshes.size(),
        materials.size(),
        textures.size(),
        nodes.size());
}

Model::~Model() = default;

void Model::ComputeRestWorldTransforms(
    std::vector<glm::mat4> &out, const glm::mat4 &rootTransform) const {
    out.assign(nodes.size(), glm::mat4(1.0f));
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
    Scene3D &scene, const glm::mat4 &rootTransform, const glm::vec3 &colorTint) {
    std::vector<glm::mat4> world;
    ComputeRestWorldTransforms(world, rootTransform);
    for (size_t i = 0; i < nodes.size(); i++) {
        const NodeTemplate &node = nodes[i];
        for (int meshIdx : node.meshIndices) {
            SceneObject obj;
            obj.mesh = meshes[meshIdx].get();
            obj.material = GetMaterialForMesh(meshIdx);
            obj.transform = world[i];
            obj.color = colorTint;
            scene.objects.push_back(obj);
        }
    }
}

} // namespace Lucky
