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
    int    ShadowIndex; // -1 = no shadow; for 2D path, index into ShadowVP / shadow maps;
                        // for cube path, index into PointShadowNearFar / point shadow cubes.
    int    ShadowType;  // 0 = 2D spot/directional shadow, 1 = point cube shadow.
};

cbuffer LightingUBO : register(b0, space3) {
    float3   AmbientColor;
    int      LightCount;
    float3   CameraPosition;
    float    _frameLightPad;
    Light    Lights[8];
    float4x4 ShadowVP[4];
    // (n0, f0, n1, f1) for the two point shadow cubes, used by
    // ComputePointShadow to convert eye-space distance into the same
    // normalized depth that the depth target stored.
    float4   PointShadowNearFar;
};

cbuffer MaterialUBO : register(b1, space3) {
    float4 BaseColorFactor;
    float3 EmissiveFactor;
    float  MetallicFactor;
    float  RoughnessFactor;
    int    HasBaseColorTexture;
    int    HasMetallicRoughnessTexture;
    int    HasEmissiveTexture;
    int    HasNormalTexture;
    float  NormalScale;
    float  _matPad0;
    float  _matPad1;
};

// SDL_shadercross classifies each texture+sampler PAIR as a "sampler"
// binding and any unpaired textures as "storage_textures". To get all
// eight textures into the SDL_BindGPUFragmentSamplers path, every
// texture needs its own SamplerState declaration -- even though we
// bind the same Lucky::Sampler instance to all four shadow slots.
Texture2D        BaseColorTexture          : register(t0, space2);
Texture2D        MetallicRoughnessTexture  : register(t1, space2);
Texture2D        ShadowMap0                : register(t2, space2);
Texture2D        ShadowMap1                : register(t3, space2);
Texture2D        ShadowMap2                : register(t4, space2);
Texture2D        ShadowMap3                : register(t5, space2);
Texture2D        EmissiveTexture           : register(t6, space2);
Texture2D        NormalTexture             : register(t7, space2);
TextureCube<float> PointShadowMap0         : register(t8, space2);
TextureCube<float> PointShadowMap1         : register(t9, space2);

SamplerState BaseColorSampler          : register(s0, space2);
SamplerState MetallicRoughnessSampler  : register(s1, space2);
SamplerState ShadowSampler0            : register(s2, space2);
SamplerState ShadowSampler1            : register(s3, space2);
SamplerState ShadowSampler2            : register(s4, space2);
SamplerState ShadowSampler3            : register(s5, space2);
SamplerState EmissiveSampler           : register(s6, space2);
SamplerState NormalSampler             : register(s7, space2);
SamplerState PointShadowSampler0       : register(s8, space2);
SamplerState PointShadowSampler1       : register(s9, space2);

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

float SamplePointShadowMap(int index, float3 dir) {
    if (index == 0) return PointShadowMap0.Sample(PointShadowSampler0, dir);
    return PointShadowMap1.Sample(PointShadowSampler1, dir);
}

// Cube-shadow lookup for point lights. The depth target stores
// hardware (clip-space) depth, so we have to reconstruct what value
// the rasterizer would have written for the current fragment's
// distance from the light. The dominant axis of `fragToLight` plays
// the role of eye-space Z because each cube face's view matrix points
// straight down its own axis -- so |max component| is z_eye for the
// matching face. Reverse-projection of the perspective matrix gives
// the normalized depth value to compare against the sampled texel.
float ComputePointShadow(int shadowIndex, float3 worldPos, float3 lightPos, float3 N) {
    if (shadowIndex < 0) return 1.0;

    // Same normal-offset bias trick as the 2D path: shift the lookup
    // position along the surface normal so back-facing rays don't
    // self-shadow against the casting surface.
    float3 toLight = lightPos - worldPos;
    float distToLight = length(toLight);
    float3 L = toLight / max(distToLight, 1e-4);
    float NdotL = saturate(dot(N, L));
    float normalOffset = 0.05 * (1.0 - NdotL) + 0.01;
    float3 biasedPos = worldPos + N * normalOffset;

    float3 fragToLight = biasedPos - lightPos;
    float near = (shadowIndex == 0) ? PointShadowNearFar.x : PointShadowNearFar.z;
    float far  = (shadowIndex == 0) ? PointShadowNearFar.y : PointShadowNearFar.w;

    // Eye-space Z for the cube face this direction targets is the
    // magnitude of the dominant axis. (One of |x|, |y|, |z| equals
    // the eye-space Z for the face whose forward is that axis.)
    float3 absDir = abs(fragToLight);
    float z_eye = max(absDir.x, max(absDir.y, absDir.z));

    // Perspective depth = far*(z-near) / (z*(far-near)). Matches the
    // glm::perspectiveRH_ZO projection the renderer uses per face.
    float expectedZ = far * (z_eye - near) / (z_eye * (far - near));
    if (expectedZ < 0.0 || expectedZ > 1.0) return 1.0;

    // PCF: build a basis perpendicular to fragToLight and tap a 3x3
    // grid in tangent space. Offset scale grows with distance so the
    // kernel covers a roughly constant world-space area.
    float3 dir = fragToLight / max(distToLight, 1e-4);
    float3 helper = (abs(dir.y) < 0.99) ? float3(0, 1, 0) : float3(1, 0, 0);
    float3 tangent = normalize(cross(helper, dir));
    float3 bitangent = cross(dir, tangent);
    float offsetScale = z_eye / 1024.0 * 2.0; // matches PointShadowMapSize

    float shadow = 0.0;
    for (int y = -1; y <= 1; y++) {
        for (int x = -1; x <= 1; x++) {
            float3 sampleDir = fragToLight + (tangent * x + bitangent * y) * offsetScale;
            float depth = SamplePointShadowMap(shadowIndex, sampleDir);
            shadow += (expectedZ > depth) ? 0.0 : 1.0;
        }
    }
    return shadow / 9.0;
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
    float4 Tangent   : TEXCOORD4; // xyz tangent, w handedness
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
    // Roughness floor for non-IBL rendering. Production engines with
    // image-based lighting clamp at 0.045-0.08 because IBL
    // prefiltering localizes the specular peak. With only analytical
    // lights, very low roughness can produce single-pixel fireflies
    // as geometry rotates; 0.1 is the smallest floor that keeps that
    // stable.
    roughness = clamp(roughness, 0.1, 1.0);

    // PBR setup: F0 lerps between dielectric (0.04) and full metal (base color).
    float3 N = normalize(input.Normal);
    float3 V = normalize(CameraPosition - input.WorldPos);

    // Normal mapping: replace N with the tangent-space sample when a
    // normal map is bound. Build the TBN from the (Gram-Schmidt-
    // orthogonalized) interpolated tangent and the geometric normal,
    // then unpack the sample from [0,1] to [-1,1] and apply the
    // per-material xy scale before reconstructing z and rotating into
    // world space. Normal maps are linear data -- no sRGB decode here.
    if (HasNormalTexture != 0) {
        float3 T = normalize(input.Tangent.xyz - N * dot(N, input.Tangent.xyz));
        float3 B = cross(N, T) * input.Tangent.w;
        float3 nm = NormalTexture.Sample(NormalSampler, input.TexCoord).rgb * 2.0 - 1.0;
        nm.xy *= NormalScale;
        N = normalize(T * nm.x + B * nm.y + N * nm.z);
    }

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
        // Per-channel firefly clamp. Picked high enough that typical
        // off-peak pixels stay below it (preserving F0 color tint on
        // metals), but low enough to bound single-pixel mirror-peak
        // brightness on smooth surfaces. A magnitude clamp would scale
        // all channels uniformly and crush metal tints when the cap
        // hits, so per-channel is correct here.
        specular = min(specular, float3(16.0, 16.0, 16.0));

        float3 kd = (1.0 - F) * (1.0 - metallic);
        // Lambert without the energy-normalizing /PI -- production
        // engines that include it expect light intensities pre-scaled
        // by PI. Keeping unscaled light intensities makes the scene
        // way too dim, so we drop the divisor here. Specular and
        // diffuse still energy-conserve relative to each other via
        // kd = (1 - F) * (1 - metallic).
        float3 diffuse = kd * albedo;

        float shadow;
        if (Lights[i].ShadowType == 1) {
            shadow = ComputePointShadow(
                Lights[i].ShadowIndex, input.WorldPos, Lights[i].Position, N);
        } else {
            shadow = ComputeShadow(Lights[i].ShadowIndex, input.WorldPos, N, L);
        }

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
