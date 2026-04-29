// Diagnostic tests for tangent / normal-map alignment in shipped glTF
// assets. The forward fragment shader's TBN construction produces NaN
// when the per-vertex tangent is degenerate (zero length), which then
// blackens any pixel that consumes it. These tests parse the source
// glTFs with cgltf directly so we can answer two questions per
// (material, primitive) pair:
//
//   1. If the material declares a normal_texture, does every primitive
//      using that material carry a TANGENT vertex attribute?
//   2. If TANGENT is present, are the values actually non-degenerate
//      (some exporters write all-zero tangents which trip the same
//      NaN path)?
//
// These tests are intentionally not assertions about the renderer --
// they're assertions about the asset, run through the same loader path
// the engine uses. A failure here points at the source asset (or our
// cgltf attribute-walk code), not at the shader.

#include <cstring>
#include <doctest/doctest.h>
#include <memory>
#include <string>
#include <vector>

#include <SDL3/SDL_filesystem.h>
#include <cgltf.h>

namespace {

struct GlbDeleter {
    void operator()(cgltf_data *p) const {
        if (p) {
            cgltf_free(p);
        }
    }
};
using GlbPtr = std::unique_ptr<cgltf_data, GlbDeleter>;

GlbPtr ParseGlb(const std::string &path) {
    cgltf_options options{};
    cgltf_data *raw = nullptr;
    REQUIRE(cgltf_parse_file(&options, path.c_str(), &raw) == cgltf_result_success);
    GlbPtr data(raw);
    REQUIRE(cgltf_load_buffers(&options, data.get(), path.c_str()) == cgltf_result_success);
    return data;
}

// Resolve a model path against the source tree, not the build output.
// The Tests project doesn't copy models into its OutDir (only Demos
// does), so we walk up from the test exe to the repo root and into
// `Demos/Content/Models`. SDL_GetBasePath returns `Build/Output/<Config>/`.
std::string ModelPath(const char *name) {
    return std::string(SDL_GetBasePath()) + "../../../Demos/Content/Models/" + name;
}

// Returns the first primitive from `data` whose material is `mat`, or
// nullptr if none. Used to ask "which primitive is this normal-mapped
// material applied to" so the diagnostic message points at the right
// mesh.
struct PrimitiveLocation {
    cgltf_size meshIndex;
    cgltf_size primIndex;
    const cgltf_primitive *prim;
};

std::vector<PrimitiveLocation> FindPrimitivesUsingMaterial(
    const cgltf_data *data, const cgltf_material *mat) {
    std::vector<PrimitiveLocation> hits;
    for (cgltf_size m = 0; m < data->meshes_count; m++) {
        const cgltf_mesh &mesh = data->meshes[m];
        for (cgltf_size p = 0; p < mesh.primitives_count; p++) {
            if (mesh.primitives[p].material == mat) {
                hits.push_back({m, p, &mesh.primitives[p]});
            }
        }
    }
    return hits;
}

const cgltf_accessor *FindAttribute(const cgltf_primitive &prim, cgltf_attribute_type type) {
    for (cgltf_size a = 0; a < prim.attributes_count; a++) {
        if (prim.attributes[a].type == type && prim.attributes[a].index == 0) {
            return prim.attributes[a].data;
        }
    }
    return nullptr;
}

} // namespace

TEST_CASE("BoomBox: every normal-mapped primitive ships a TANGENT attribute") {
    GlbPtr data = ParseGlb(ModelPath("BoomBox.glb"));

    int normalMappedMaterials = 0;
    int primitivesMissingTangent = 0;

    for (cgltf_size m = 0; m < data->materials_count; m++) {
        const cgltf_material &mat = data->materials[m];
        if (!mat.normal_texture.texture) {
            continue;
        }
        normalMappedMaterials++;

        const auto hits = FindPrimitivesUsingMaterial(data.get(), &mat);
        const std::string matName = mat.name ? mat.name : ("material_" + std::to_string(m));
        INFO("Material '" << matName << "' has a normal map; used by " << hits.size()
                          << " primitive(s)");

        for (const auto &loc : hits) {
            const bool hasTangent = FindAttribute(*loc.prim, cgltf_attribute_type_tangent) != nullptr;
            INFO("  mesh[" << loc.meshIndex << "] prim[" << loc.primIndex
                           << "]: TANGENT " << (hasTangent ? "PRESENT" : "MISSING"));
            CHECK(hasTangent);
            if (!hasTangent) {
                primitivesMissingTangent++;
            }
        }
    }

    INFO("BoomBox normal-mapped materials: " << normalMappedMaterials
                                             << "; primitives missing TANGENT: "
                                             << primitivesMissingTangent);
    CHECK(normalMappedMaterials > 0); // sanity: BoomBox is supposed to ship a normal map
}

TEST_CASE("BoomBox: TANGENT values are non-degenerate on normal-mapped primitives") {
    GlbPtr data = ParseGlb(ModelPath("BoomBox.glb"));

    for (cgltf_size m = 0; m < data->materials_count; m++) {
        const cgltf_material &mat = data->materials[m];
        if (!mat.normal_texture.texture) {
            continue;
        }
        const auto hits = FindPrimitivesUsingMaterial(data.get(), &mat);

        for (const auto &loc : hits) {
            const cgltf_accessor *tan = FindAttribute(*loc.prim, cgltf_attribute_type_tangent);
            if (!tan) {
                continue; // tracked by the prior test
            }

            int zeroCount = 0;
            int sampleCount = 0;
            const cgltf_size step = std::max<cgltf_size>(1, tan->count / 64);
            for (cgltf_size i = 0; i < tan->count; i += step) {
                float v[4] = {0, 0, 0, 0};
                cgltf_accessor_read_float(tan, i, v, 4);
                const float lenSq = v[0] * v[0] + v[1] * v[1] + v[2] * v[2];
                if (lenSq < 1e-6f) {
                    zeroCount++;
                }
                sampleCount++;
            }
            INFO("mesh[" << loc.meshIndex << "] prim[" << loc.primIndex << "]: "
                         << zeroCount << "/" << sampleCount
                         << " sampled tangents have near-zero length");
            CHECK(zeroCount == 0);
        }
    }
}

TEST_CASE("BoomBox: full primitive attribute inventory (informational)") {
    // Always-passing snapshot: prints the attribute set of every
    // primitive so we can sanity-check assumptions about what BoomBox
    // ships. Run with `--success` if you want to see it on a green run.
    GlbPtr data = ParseGlb(ModelPath("BoomBox.glb"));

    for (cgltf_size m = 0; m < data->meshes_count; m++) {
        const cgltf_mesh &mesh = data->meshes[m];
        const std::string meshName = mesh.name ? mesh.name : ("mesh_" + std::to_string(m));
        for (cgltf_size p = 0; p < mesh.primitives_count; p++) {
            const cgltf_primitive &prim = mesh.primitives[p];

            std::string attrs;
            for (cgltf_size a = 0; a < prim.attributes_count; a++) {
                if (!attrs.empty()) {
                    attrs += ", ";
                }
                const cgltf_attribute &att = prim.attributes[a];
                const char *name = att.name ? att.name : "<unnamed>";
                attrs += name;
            }
            const char *matName = (prim.material && prim.material->name) ? prim.material->name : "<none>";
            const bool hasNormalMap =
                prim.material && prim.material->normal_texture.texture;
            INFO(meshName << " prim[" << p << "] material=" << matName
                          << " normalMap=" << (hasNormalMap ? "YES" : "NO")
                          << " attrs=[" << attrs << "]");
            CHECK(true);
        }
    }
}
