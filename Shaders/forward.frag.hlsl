struct Light {
    float3 Position;
    float  Range;
    float3 Color;
    float  Intensity;
    float3 Direction;
    int    Type;        // 0=Directional, 1=Point, 2=Spot (matches Lucky::LightType)
    float  InnerCone;   // cos(half-angle), spot only
    float  OuterCone;   // cos(half-angle), spot only
    int    ShadowIndex; // -1 = no 2D shadow; 0..3 = index into ShadowVP / shadow maps
    float  _pad;
};

cbuffer LightingUBO : register(b0, space3) {
    float3   AmbientColor;
    int      LightCount;
    Light    Lights[8];
    float4x4 ShadowVP[4];
};

Texture2D<float> ShadowMap0 : register(t0, space2);
Texture2D<float> ShadowMap1 : register(t1, space2);
Texture2D<float> ShadowMap2 : register(t2, space2);
Texture2D<float> ShadowMap3 : register(t3, space2);
SamplerState     ShadowSampler : register(s0, space2);

float SampleShadowMap(int index, float2 uv) {
    if (index == 0) return ShadowMap0.Sample(ShadowSampler, uv);
    if (index == 1) return ShadowMap1.Sample(ShadowSampler, uv);
    if (index == 2) return ShadowMap2.Sample(ShadowSampler, uv);
    return ShadowMap3.Sample(ShadowSampler, uv);
}

float ComputeShadow(int shadowIndex, float3 worldPos, float3 N, float3 L) {
    if (shadowIndex < 0) return 1.0;

    // Normal-offset bias: shift the lookup along the surface normal so
    // self-shadowing on grazing surfaces doesn't acne up the lit side.
    float NdotL = saturate(dot(N, L));
    float normalOffset = 0.03 * (1.0 - NdotL);
    float3 biasedPos = worldPos + N * normalOffset;

    float4 lightClip = mul(ShadowVP[shadowIndex], float4(biasedPos, 1.0));
    float3 ndc = lightClip.xyz / lightClip.w;

    float2 shadowUV = ndc.xy * 0.5 + 0.5;
    shadowUV.y = 1.0 - shadowUV.y;
    float currentDepth = ndc.z;

    // Outside the light frustum -> not shadowed (treat as fully lit).
    if (any(shadowUV < 0.0) || any(shadowUV > 1.0)) return 1.0;
    if (currentDepth < 0.0 || currentDepth > 1.0) return 1.0;

    // 5x5 PCF: 25 taps. More expensive than 3x3 but produces much
    // smoother edges at low shadow-map densities.
    float texelSize = 1.0 / 1024.0;
    float shadow = 0.0;
    for (int y = -2; y <= 2; y++) {
        for (int x = -2; x <= 2; x++) {
            float depth = SampleShadowMap(shadowIndex, shadowUV + float2(x, y) * texelSize);
            shadow += (currentDepth > depth) ? 0.0 : 1.0;
        }
    }
    return shadow / 25.0;
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
    float3 N = normalize(input.Normal);
    float3 lighting = AmbientColor;

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

        float diffuse = saturate(dot(N, L));
        float shadow = ComputeShadow(Lights[i].ShadowIndex, input.WorldPos, N, L);
        lighting += Lights[i].Color * diffuse * attenuation * shadow;
    }

    float3 hdr = input.ColorTint * lighting;
    float3 mapped = 1.0 - exp(-hdr * 0.6);
    mapped += float3(input.TexCoord, 0.0) * 1e-6;

    PSOutput o;
    o.Color = float4(mapped, 1.0);
    return o;
}
