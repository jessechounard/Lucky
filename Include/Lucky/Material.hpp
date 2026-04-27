#pragma once

#include <glm/glm.hpp>

namespace Lucky {

struct Sampler;
struct Texture;

/**
 * Plain-data PBR material in the glTF 2.0 metallic-roughness model.
 *
 * Mirrors the fields the forward renderer's fragment shader expects.
 * Per glTF: `baseColorFactor` is a linear-space RGBA tint multiplied
 * by the base-color texture sample. `metallicFactor` and
 * `roughnessFactor` similarly multiply the metallic-roughness texture
 * (which packs metallic in B and roughness in G). `emissiveFactor` is
 * unlit RGB added on top of the lit result.
 *
 * Texture pointers are non-owning. The owner (typically a `Model`)
 * keeps the `Texture` and `Sampler` alive for the duration of any
 * draw that reads this material.
 *
 * # Defaults
 *
 * The default-constructed Material renders a fully white, fully rough,
 * fully dielectric surface with no textures -- the closest "neutral"
 * material the BRDF can produce. Use this as the fallback for
 * `SceneObject` instances that don't carry a glTF-loaded material.
 */
struct Material {
    glm::vec4 baseColorFactor = {1.0f, 1.0f, 1.0f, 1.0f};
    // Defaults are a neutral plastic dielectric: zero metallic, medium
    // roughness. SceneObjects with no material use this exact value
    // (built into ForwardRenderer's fallback path), which is why the
    // defaults need to look reasonable on their own. The cgltf loader
    // overwrites these with the source asset's PBR factors when a
    // material is parsed.
    float metallicFactor = 0.0f;
    float roughnessFactor = 0.5f;
    glm::vec3 emissiveFactor = {0.0f, 0.0f, 0.0f};

    Texture *baseColorTexture = nullptr;
    Texture *metallicRoughnessTexture = nullptr;
    Texture *emissiveTexture = nullptr;
    Sampler *sampler = nullptr;
};

} // namespace Lucky
