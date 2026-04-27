// Forward PBR fragment shader (glTF 2.0 metallic-roughness model).
// Cook-Torrance BRDF: GGX D + Smith G + Schlick F. No image-based
// lighting yet, so pure metals look dark when no analytic light is
// reflecting toward the camera -- that's expected, not a bug.

struct Light {
    float3 Position;
    float  Range;
    float3 Color;
    float  Intensity;
    float3 Direction;
    int    Type;        // 0=Directional, 1=Point, 2=Spot
    float  InnerCone;
    float  OuterCone;
    int    ShadowIndex; // -1 = no 2D shadow; 0..3 = index into ShadowVP
    float  _pad;
};

cbuffer LightingUBO : register(b0, space3) {
    float3   AmbientColor;
    int      LightCount;
    float3   CameraPosition;
    float    _frameLightPad;
    Light    Lights[8];
    float4x4 ShadowVP[4];
};

cbuffer MaterialUBO : register(b1, space3) {
    float4 BaseColorFactor;
    float3 EmissiveFactor;
    float  MetallicFactor;
    float  RoughnessFactor;
    int    HasBaseColorTexture;
    int    HasMetallicRoughnessTexture;
    int    HasEmissiveTexture;
};

// SDL_shadercross classifies each texture+sampler PAIR as a "sampler"
// binding and any unpaired textures as "storage_textures". To get all
// six textures into the SDL_BindGPUFragmentSamplers path, every
// texture needs its own SamplerState declaration -- even though we
// bind the same Lucky::Sampler instance to all four shadow slots.
Texture2D    BaseColorTexture          : register(t0, space2);
Texture2D    MetallicRoughnessTexture  : register(t1, space2);
Texture2D    ShadowMap0                : register(t2, space2);
Texture2D    ShadowMap1                : register(t3, space2);
Texture2D    ShadowMap2                : register(t4, space2);
Texture2D    ShadowMap3                : register(t5, space2);
Texture2D    EmissiveTexture           : register(t6, space2);

SamplerState BaseColorSampler          : register(s0, space2);
SamplerState MetallicRoughnessSampler  : register(s1, space2);
SamplerState ShadowSampler0            : register(s2, space2);
SamplerState ShadowSampler1            : register(s3, space2);
SamplerState ShadowSampler2            : register(s4, space2);
SamplerState ShadowSampler3            : register(s5, space2);
SamplerState EmissiveSampler           : register(s6, space2);

static const float PI = 3.14159265359;

float SampleShadowMap(int index, float2 uv) {
    // Sample .r channel: depth maps store depth in red.
    if (index == 0) return ShadowMap0.Sample(ShadowSampler0, uv).r;
    if (index == 1) return ShadowMap1.Sample(ShadowSampler1, uv).r;
    if (index == 2) return ShadowMap2.Sample(ShadowSampler2, uv).r;
    return ShadowMap3.Sample(ShadowSampler3, uv).r;
}

float ComputeShadow(int shadowIndex, float3 worldPos, float3 N, float3 L) {
    if (shadowIndex < 0) return 1.0;

    float NdotL = saturate(dot(N, L));
    // Normal offset scales with the shadow texel footprint in world
    // units. For a 24-unit ortho on a 1024 shadow map (~23mm/texel),
    // a 0.05-0.10 offset comfortably pushes self-shadowing samples
    // off the surface without producing visible peter-panning gaps
    // along the shadow contact.
    float normalOffset = 0.08 * (1.0 - NdotL) + 0.02;
    float3 biasedPos = worldPos + N * normalOffset;

    float4 lightClip = mul(ShadowVP[shadowIndex], float4(biasedPos, 1.0));
    float3 ndc = lightClip.xyz / lightClip.w;

    float2 shadowUV = ndc.xy * 0.5 + 0.5;
    shadowUV.y = 1.0 - shadowUV.y;
    float currentDepth = ndc.z;

    if (any(shadowUV < 0.0) || any(shadowUV > 1.0)) return 1.0;
    if (currentDepth < 0.0 || currentDepth > 1.0) return 1.0;

    float texelSize = 1.0 / 2048.0; // must match ForwardRenderer::ShadowMapSize
    float shadow = 0.0;
    for (int y = -2; y <= 2; y++) {
        for (int x = -2; x <= 2; x++) {
            float depth = SampleShadowMap(shadowIndex, shadowUV + float2(x, y) * texelSize);
            shadow += (currentDepth > depth) ? 0.0 : 1.0;
        }
    }
    return shadow / 25.0;
}

// GGX / Trowbridge-Reitz normal distribution function.
float D_GGX(float NdotH, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float denom = NdotH * NdotH * (a2 - 1.0) + 1.0;
    return a2 / (PI * denom * denom);
}

// Smith geometry term using Schlick-GGX approximation, split into
// view/light components and combined.
float G_SchlickGGX(float NdotX, float roughness) {
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return NdotX / (NdotX * (1.0 - k) + k);
}

float G_Smith(float NdotV, float NdotL, float roughness) {
    return G_SchlickGGX(NdotV, roughness) * G_SchlickGGX(NdotL, roughness);
}

// Schlick Fresnel approximation, with F0 = surface reflectance at
// normal incidence (0.04 for dielectrics, baseColor for pure metals).
float3 F_Schlick(float VdotH, float3 F0) {
    return F0 + (1.0 - F0) * pow(1.0 - VdotH, 5.0);
}

struct PSInput {
    float3 WorldPos  : TEXCOORD0;
    float2 TexCoord  : TEXCOORD1;
    float3 Normal    : TEXCOORD2;
    float3 ColorTint : TEXCOORD3;
};

struct PSOutput {
    float4 Color : SV_Target;
};

PSOutput main(PSInput input) {
    // Sample material textures (1x1 white fallback if the flag is off).
    float4 baseColorSample = BaseColorTexture.Sample(BaseColorSampler, input.TexCoord);
    // glTF base-color textures are encoded in sRGB. Lucky's Texture
    // loads them as linear UNORM, so we decode in-shader. 2.2 is the
    // sRGB approximation -- a piecewise sRGB curve is more accurate
    // but the visual difference is small. The proper fix is a
    // TextureFormat::sRGB that lets the GPU do this in hardware.
    baseColorSample.rgb = pow(baseColorSample.rgb, 2.2);

    float4 baseColor = BaseColorFactor;
    if (HasBaseColorTexture != 0) {
        baseColor *= baseColorSample;
    }
    baseColor.rgb *= input.ColorTint;

    float4 mrSample = MetallicRoughnessTexture.Sample(MetallicRoughnessSampler, input.TexCoord);
    float metallic = MetallicFactor;
    float roughness = RoughnessFactor;
    if (HasMetallicRoughnessTexture != 0) {
        // glTF 2.0 packs metallic in B and roughness in G.
        metallic *= mrSample.b;
        roughness *= mrSample.g;
    }
    // Roughness floor of 0.5 with non-IBL rendering. Production
    // engines with image-based lighting clamp at 0.045-0.08 because
    // IBL prefiltering localizes the specular peak. With only
    // analytical lights, low roughness produces broad bright
    // highlights that whitewash surfaces (e.g. BoomBox packs
    // roughness=0 for its body, expecting IBL). Half-rough is the
    // smallest floor that keeps mirror-authored materials looking
    // like dark plastic instead of overexposed white.
    roughness = clamp(roughness, 0.5, 1.0);

    // PBR setup: F0 lerps between dielectric (0.04) and full metal (base color).
    float3 N = normalize(input.Normal);
    float3 V = normalize(CameraPosition - input.WorldPos);

    float3 F0 = lerp(float3(0.04, 0.04, 0.04), baseColor.rgb, metallic);
    float3 albedo = baseColor.rgb * (1.0 - metallic);

    float3 lighting = AmbientColor * albedo;

    for (int i = 0; i < LightCount; i++) {
        float3 L;
        float attenuation;

        if (Lights[i].Type == 0) {
            L = -Lights[i].Direction;
            attenuation = Lights[i].Intensity;
        } else {
            float3 toLight = Lights[i].Position - input.WorldPos;
            float dist = length(toLight);
            L = toLight / max(dist, 1e-4);
            float falloff = dist / Lights[i].Range;
            attenuation = Lights[i].Intensity / (1.0 + falloff * falloff);

            if (Lights[i].Type == 2) {
                float cosAngle = dot(-L, Lights[i].Direction);
                attenuation *=
                    smoothstep(Lights[i].OuterCone, Lights[i].InnerCone, cosAngle);
            }
        }

        float3 H = normalize(L + V);
        float NdotL = saturate(dot(N, L));
        float NdotV = saturate(dot(N, V));
        float NdotH = saturate(dot(N, H));
        float VdotH = saturate(dot(V, H));

        float D = D_GGX(NdotH, roughness);
        float G = G_Smith(NdotV, NdotL, roughness);
        float3 F = F_Schlick(VdotH, F0);

        float3 specular = (D * G * F) / max(4.0 * NdotV * NdotL, 1e-4);
        specular = min(specular, float3(1.0, 1.0, 1.0));

        float3 kd = (1.0 - F) * (1.0 - metallic);
        // Lambert without the energy-normalizing /PI -- production
        // engines that include it expect light intensities pre-scaled
        // by PI. Keeping unscaled light intensities makes the scene
        // way too dim, so we drop the divisor here. Specular and
        // diffuse still energy-conserve relative to each other via
        // kd = (1 - F) * (1 - metallic).
        float3 diffuse = kd * albedo;

        float shadow = ComputeShadow(Lights[i].ShadowIndex, input.WorldPos, N, L);

        lighting += (diffuse + specular) * Lights[i].Color * attenuation * NdotL * shadow;
    }

    // Emissive: factor * (texture if present, else 1). The texture is
    // sRGB-encoded so we decode like base color.
    float3 emissive = EmissiveFactor;
    if (HasEmissiveTexture != 0) {
        float3 emissiveSample = EmissiveTexture.Sample(EmissiveSampler, input.TexCoord).rgb;
        emissive *= pow(emissiveSample, 2.2);
    }
    lighting += emissive;

    // Tonemap-ish exposure compression to keep bright accumulations in [0,1].
    float3 mapped = 1.0 - exp(-lighting * 0.6);

    // Encode linear lighting result back to sRGB for the linear UNORM
    // swapchain (B8G8R8A8_UNORM on this device). The OS interprets
    // those bytes as sRGB-encoded for display, so without this encode
    // the image displays ~2x too dark and desaturated. If the swapchain
    // were a `_UNORM_SRGB` format the hardware would do this for us
    // and this line should be removed.
    mapped = pow(mapped, 1.0 / 2.2);

    PSOutput o;
    o.Color = float4(mapped, baseColor.a);
    return o;
}
