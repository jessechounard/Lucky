Texture2D u_texture : register(t0, space2);
SamplerState u_sampler : register(s0, space2);

cbuffer MsdfParams : register(b0, space3) {
    float pxRange;
    float atlasWidth;
    float atlasHeight;
    float outlineThickness;   // screen pixels, 0 = no outline
    float4 outlineColor;      // RGBA
    float glowThickness;      // how far glow extends in screen pixels
    float glowSoftness;       // width of glow falloff in screen pixels, 0 = no glow
    float2 _pad0;
    float4 glowColor;         // RGBA
};

struct PSInput {
    float2 TexCoord : TEXCOORD0;
    float4 Color    : TEXCOORD1;
};

struct PSOutput {
    float4 Color : SV_Target;
};

float median(float r, float g, float b) {
    return max(min(r, g), min(max(r, g), b));
}

void paint(inout float4 dst, float4 src, float layerOpacity) {
    float opacity = clamp(src.a * layerOpacity - dst.a, 0.0, 1.0);
    dst += float4(src.rgb, 1.0) * opacity;
}

PSOutput main(PSInput input) {
    PSOutput output;

    float4 texSample = u_texture.Sample(u_sampler, input.TexCoord);
    float3 msd = texSample.rgb;
    float sd = median(msd.r, msd.g, msd.b);
    float sdfChannel = texSample.a; // true single-channel SDF from mtsdf atlas

    // Convert atlas-space distance to screen-pixel distance
    float2 unitRange = float2(pxRange, pxRange) / float2(atlasWidth, atlasHeight);
    float2 screenTexSize = 1.0 / fwidth(input.TexCoord);
    float screenPxRange = max(0.5, length(unitRange * screenTexSize));

    float screenPxDistance = screenPxRange * (sd - 0.5);

    bool hasOutline = outlineThickness > 0.0;
    bool hasGlow = glowSoftness > 0.0;

    if (hasOutline || hasGlow) {
        float4 dst = float4(0.0, 0.0, 0.0, 0.0);

        // Front-to-back compositing: fill first, then outline, then glow

        // Layer 1: fill
        float fillOpacity = clamp(screenPxDistance + 0.5, 0.0, 1.0);
        paint(dst, input.Color, fillOpacity);

        // Layer 2: outline (uses MSDF median for sharp edges)
        if (hasOutline) {
            float outlineOpacity = clamp(screenPxDistance + outlineThickness + 0.5, 0.0, 1.0);
            paint(dst, outlineColor, outlineOpacity);
        }

        // Layer 3: glow (uses true single-channel SDF for smooth falloff)
        if (hasGlow) {
            float glowSd = sdfChannel;
            float glowScreenPxDistance = screenPxRange * (glowSd - 0.5);
            float glowEdge = glowThickness + outlineThickness;
            float glowOpacity = clamp((glowScreenPxDistance + glowEdge) / glowSoftness + 0.5, 0.0, 1.0);
            paint(dst, glowColor, glowOpacity);
        }

        // paint() produces premultiplied alpha; convert to straight alpha
        // for standard alpha blending
        output.Color = dst.a > 0.0 ? float4(dst.rgb / dst.a, dst.a) : float4(0, 0, 0, 0);
    } else {
        // Original path — fill only
        float opacity = saturate(screenPxDistance + 0.5);
        output.Color = float4(input.Color.rgb, input.Color.a * opacity);
    }

    return output;
}
