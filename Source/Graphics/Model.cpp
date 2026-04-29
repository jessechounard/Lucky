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
    const cgltf_accessor *tangentAccessor = nullptr;
    for (cgltf_size a = 0; a < prim.attributes_count; a++) {
        const cgltf_attribute &attr = prim.attributes[a];
        if (attr.type == cgltf_attribute_type_position) {
            posAccessor = attr.data;
        } else if (attr.type == cgltf_attribute_type_texcoord && attr.index == 0) {
            uvAccessor = attr.data;
        } else if (attr.type == cgltf_attribute_type_normal) {
            normalAccessor = attr.data;
        } else if (attr.type == cgltf_attribute_type_tangent) {
            tangentAccessor = attr.data;
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
        // Tangent absent -> zero. The forward shader's HasNormalTexture
        // flag gates the read, so an unread zero is harmless. If a model
        // ever ships a normal-mapped material without TANGENT, MikkTSpace
        // generation would land here.
        float tan[4] = {0, 0, 0, 0};
        cgltf_accessor_read_float(posAccessor, i, pos, 3);
        if (uvAccessor) {
            cgltf_accessor_read_float(uvAccessor, i, uv, 2);
        }
        if (normalAccessor) {
            cgltf_accessor_read_float(normalAccessor, i, nrm, 3);
        }
        if (tangentAccessor) {
            cgltf_accessor_read_float(tangentAccessor, i, tan, 4);
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
        v.tx = tan[0];
        v.ty = tan[1];
        v.tz = tan[2];
        v.tw = tan[3];
    }

    const size_t indexCount = prim.indices->count;
    out.indices.resize(indexCount);
    for (size_t i = 0; i < indexCount; i++) {
        out.indices[i] = static_cast<uint32_t>(cgltf_accessor_read_index(prim.indices, i));
    }
    return true;
}

// True if the primitive carries the JOINTS_0 + WEIGHTS_0 attributes
// that turn it into a skinned draw. We require the first set only;
// glTF allows multiple joint sets (8+ influences per vertex), but the
// skinned vertex shader reads exactly four influences.
bool PrimitiveIsSkinned(const cgltf_primitive &prim) {
    bool hasJoints = false;
    bool hasWeights = false;
    for (cgltf_size a = 0; a < prim.attributes_count; a++) {
        const cgltf_attribute &attr = prim.attributes[a];
        if (attr.type == cgltf_attribute_type_joints && attr.index == 0) {
            hasJoints = true;
        } else if (attr.type == cgltf_attribute_type_weights && attr.index == 0) {
            hasWeights = true;
        }
    }
    return hasJoints && hasWeights;
}

bool BuildSkinnedPrimitiveData(const cgltf_primitive &prim, SkinnedMeshData &out) {
    const cgltf_accessor *posAccessor = nullptr;
    const cgltf_accessor *uvAccessor = nullptr;
    const cgltf_accessor *normalAccessor = nullptr;
    const cgltf_accessor *tangentAccessor = nullptr;
    const cgltf_accessor *jointsAccessor = nullptr;
    const cgltf_accessor *weightsAccessor = nullptr;
    for (cgltf_size a = 0; a < prim.attributes_count; a++) {
        const cgltf_attribute &attr = prim.attributes[a];
        if (attr.type == cgltf_attribute_type_position) {
            posAccessor = attr.data;
        } else if (attr.type == cgltf_attribute_type_texcoord && attr.index == 0) {
            uvAccessor = attr.data;
        } else if (attr.type == cgltf_attribute_type_normal) {
            normalAccessor = attr.data;
        } else if (attr.type == cgltf_attribute_type_tangent) {
            tangentAccessor = attr.data;
        } else if (attr.type == cgltf_attribute_type_joints && attr.index == 0) {
            jointsAccessor = attr.data;
        } else if (attr.type == cgltf_attribute_type_weights && attr.index == 0) {
            weightsAccessor = attr.data;
        }
    }

    if (!posAccessor || !prim.indices || !jointsAccessor || !weightsAccessor) {
        return false;
    }

    const size_t vertexCount = posAccessor->count;
    out.vertices.resize(vertexCount);
    for (size_t i = 0; i < vertexCount; i++) {
        float pos[3] = {0, 0, 0};
        float uv[2] = {0, 0};
        float nrm[3] = {0, 1, 0};
        float tan[4] = {0, 0, 0, 0};
        cgltf_uint joints[4] = {0, 0, 0, 0};
        float weights[4] = {0, 0, 0, 0};
        cgltf_accessor_read_float(posAccessor, i, pos, 3);
        if (uvAccessor) {
            cgltf_accessor_read_float(uvAccessor, i, uv, 2);
        }
        if (normalAccessor) {
            cgltf_accessor_read_float(normalAccessor, i, nrm, 3);
        }
        if (tangentAccessor) {
            cgltf_accessor_read_float(tangentAccessor, i, tan, 4);
        }
        cgltf_accessor_read_uint(jointsAccessor, i, joints, 4);
        cgltf_accessor_read_float(weightsAccessor, i, weights, 4);

        Vertex3DSkinned &v = out.vertices[i];
        v.x = pos[0];
        v.y = pos[1];
        v.z = pos[2];
        v.u = uv[0];
        v.v = uv[1];
        v.nx = nrm[0];
        v.ny = nrm[1];
        v.nz = nrm[2];
        v.j0 = static_cast<uint32_t>(joints[0]);
        v.j1 = static_cast<uint32_t>(joints[1]);
        v.j2 = static_cast<uint32_t>(joints[2]);
        v.j3 = static_cast<uint32_t>(joints[3]);
        v.w0 = weights[0];
        v.w1 = weights[1];
        v.w2 = weights[2];
        v.w3 = weights[3];
        v.tx = tan[0];
        v.ty = tan[1];
        v.tz = tan[2];
        v.tw = tan[3];
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
        material.emissiveFactor =
            glm::vec3(mat.emissive_factor[0], mat.emissive_factor[1], mat.emissive_factor[2]);
        if (mat.emissive_texture.texture && mat.emissive_texture.texture->image) {
            const int idx = static_cast<int>(mat.emissive_texture.texture->image - data->images);
            if (idx >= 0 && idx < static_cast<int>(textures.size())) {
                material.emissiveTexture = textures[idx].get();
            }
        }

        // Normal map. glTF stores the per-texel scale in
        // normal_texture.scale (default 1); the shader applies it to
        // the xy components of the unpacked normal before perturbing N.
        if (mat.normal_texture.texture && mat.normal_texture.texture->image) {
            const int idx = static_cast<int>(mat.normal_texture.texture->image - data->images);
            if (idx >= 0 && idx < static_cast<int>(textures.size())) {
                material.normalTexture = textures[idx].get();
                material.normalScale = mat.normal_texture.scale;
            }
        }

        materials.push_back(material);
    }

    // One Lucky Mesh / SkinnedMesh per primitive; remember which
    // cgltf_mesh maps to which set so node-mesh references resolve to
    // the correct GPU buffers, and record each primitive's material
    // index in parallel.
    struct PrimitiveMapping {
        bool skinned;
        int index; // into meshes if !skinned, else into skinnedMeshes
    };
    struct MeshMapping {
        const cgltf_mesh *cgMesh;
        std::vector<PrimitiveMapping> primitives;
    };
    std::vector<MeshMapping> meshMap;
    meshMap.reserve(data->meshes_count);

    for (cgltf_size m = 0; m < data->meshes_count; m++) {
        const cgltf_mesh &cgMesh = data->meshes[m];
        MeshMapping mapping;
        mapping.cgMesh = &cgMesh;
        mapping.primitives.reserve(cgMesh.primitives_count);

        for (cgltf_size p = 0; p < cgMesh.primitives_count; p++) {
            const cgltf_primitive &prim = cgMesh.primitives[p];
            const int matIndex =
                prim.material ? static_cast<int>(prim.material - data->materials) : -1;

            if (PrimitiveIsSkinned(prim)) {
                SkinnedMeshData meshData;
                if (!BuildSkinnedPrimitiveData(prim, meshData)) {
                    continue;
                }
                const int idx = static_cast<int>(skinnedMeshes.size());
                skinnedMeshes.push_back(std::make_unique<SkinnedMesh>(graphicsDevice, meshData));
                skinnedMeshMaterialIndices.push_back(matIndex);
                mapping.primitives.push_back({true, idx});
            } else {
                MeshData meshData;
                if (!BuildPrimitiveData(prim, meshData)) {
                    continue;
                }
                const int idx = static_cast<int>(meshes.size());
                meshes.push_back(std::make_unique<Mesh>(graphicsDevice, meshData));
                meshMaterialIndices.push_back(matIndex);
                mapping.primitives.push_back({false, idx});
            }
        }
        meshMap.push_back(std::move(mapping));
    }

    // Skins. Joint indices reference data->nodes, which is parallel to
    // our `nodes` array, so the indices are interchangeable.
    skins.reserve(data->skins_count);
    for (cgltf_size s = 0; s < data->skins_count; s++) {
        const cgltf_skin &cgSkin = data->skins[s];
        Skin skin;
        skin.name = cgSkin.name ? cgSkin.name : ("skin_" + std::to_string(s));
        skin.skeletonRoot = cgSkin.skeleton ? static_cast<int>(cgSkin.skeleton - data->nodes) : -1;
        skin.jointNodes.resize(cgSkin.joints_count);
        for (cgltf_size j = 0; j < cgSkin.joints_count; j++) {
            skin.jointNodes[j] = static_cast<int>(cgSkin.joints[j] - data->nodes);
        }
        skin.inverseBindMatrices.assign(cgSkin.joints_count, glm::mat4(1.0f));
        if (cgSkin.inverse_bind_matrices) {
            for (cgltf_size j = 0; j < cgSkin.joints_count; j++) {
                float m[16];
                cgltf_accessor_read_float(cgSkin.inverse_bind_matrices, j, m, 16);
                skin.inverseBindMatrices[j] = glm::make_mat4(m);
            }
        }
        skins.push_back(std::move(skin));
    }

    // Node hierarchy.
    nodes.resize(data->nodes_count);
    for (cgltf_size n = 0; n < data->nodes_count; n++) {
        const cgltf_node &cgNode = data->nodes[n];
        NodeTemplate &dst = nodes[n];

        dst.name = cgNode.name ? cgNode.name : ("node_" + std::to_string(n));
        DecomposeNodeTransform(cgNode, dst);
        dst.parentIndex = cgNode.parent ? static_cast<int>(cgNode.parent - data->nodes) : -1;
        dst.skinIndex = cgNode.skin ? static_cast<int>(cgNode.skin - data->skins) : -1;

        if (cgNode.mesh) {
            for (const MeshMapping &mapping : meshMap) {
                if (mapping.cgMesh == cgNode.mesh) {
                    for (const PrimitiveMapping &pm : mapping.primitives) {
                        if (pm.skinned) {
                            dst.skinnedMeshIndices.push_back(pm.index);
                        } else {
                            dst.meshIndices.push_back(pm.index);
                        }
                    }
                    break;
                }
            }
        }
    }

    // Animations.
    animations.reserve(data->animations_count);
    for (cgltf_size a = 0; a < data->animations_count; a++) {
        const cgltf_animation &cgAnim = data->animations[a];
        AnimationDef anim;
        anim.name = cgAnim.name ? cgAnim.name : ("anim_" + std::to_string(a));
        anim.channels.reserve(cgAnim.channels_count);
        anim.duration = 0.0f;

        for (cgltf_size c = 0; c < cgAnim.channels_count; c++) {
            const cgltf_animation_channel &cgCh = cgAnim.channels[c];
            if (!cgCh.target_node || !cgCh.sampler) {
                continue;
            }

            AnimationChannel channel;
            channel.nodeIndex = static_cast<int>(cgCh.target_node - data->nodes);
            switch (cgCh.target_path) {
            case cgltf_animation_path_type_translation:
                channel.path = AnimationPath::Translation;
                break;
            case cgltf_animation_path_type_rotation:
                channel.path = AnimationPath::Rotation;
                break;
            case cgltf_animation_path_type_scale:
                channel.path = AnimationPath::Scale;
                break;
            default:
                // Skip weights and any future paths we don't support.
                continue;
            }
            switch (cgCh.sampler->interpolation) {
            case cgltf_interpolation_type_step:
                channel.interpolation = AnimationInterpolation::Step;
                break;
            case cgltf_interpolation_type_cubic_spline:
                channel.interpolation = AnimationInterpolation::CubicSpline;
                break;
            case cgltf_interpolation_type_linear:
            default:
                channel.interpolation = AnimationInterpolation::Linear;
                break;
            }

            const cgltf_accessor *timeAcc = cgCh.sampler->input;
            const cgltf_accessor *valAcc = cgCh.sampler->output;
            if (!timeAcc || !valAcc) {
                continue;
            }

            const size_t keyframeCount = timeAcc->count;
            const int components = (channel.path == AnimationPath::Rotation) ? 4 : 3;
            channel.keyframes.resize(keyframeCount);
            for (size_t k = 0; k < keyframeCount; k++) {
                AnimationKeyframe &kf = channel.keyframes[k];
                cgltf_accessor_read_float(timeAcc, k, &kf.time, 1);
                cgltf_accessor_read_float(valAcc, k, &kf.value.x, components);
                if (kf.time > anim.duration) {
                    anim.duration = kf.time;
                }
            }

            anim.channels.push_back(std::move(channel));
        }

        animations.push_back(std::move(anim));
    }

    spdlog::info("Loaded glTF '{}': {} mesh(es), {} skinned mesh(es), {} skin(s), "
                 "{} material(s), {} texture(s), {} node(s), {} animation(s)",
        path,
        meshes.size(),
        skinnedMeshes.size(),
        skins.size(),
        materials.size(),
        textures.size(),
        nodes.size(),
        animations.size());
}

Model::~Model() = default;

int Model::FindAnimation(const std::string &name) const {
    for (size_t i = 0; i < animations.size(); i++) {
        if (animations[i].name == name) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

int Model::FindNode(const std::string &name) const {
    for (size_t i = 0; i < nodes.size(); i++) {
        if (nodes[i].name == name) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

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
