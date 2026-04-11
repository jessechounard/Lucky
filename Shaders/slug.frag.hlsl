// Slug fragment shader using HarfBuzz GPU

StructuredBuffer<int4> hb_gpu_atlas : register(t0, space2);

#include "hb-gpu-fragment.hlsl"

struct PSInput {
    float4 position   : SV_Position;
    float2 emCoord    : TEXCOORD0;
    float4 color      : TEXCOORD1;
    nointerpolation float glyphLocF : TEXCOORD2;
};

struct PSOutput {
    float4 color : SV_Target;
};

PSOutput main(PSInput input) {
    PSOutput output;

    float coverage = hb_gpu_render(input.emCoord, asuint(input.glyphLocF));

    float alpha = input.color.a * coverage;
    output.color = float4(input.color.rgb * alpha, alpha);

    return output;
}
