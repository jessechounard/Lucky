// Slug vertex shader using HarfBuzz GPU

cbuffer ViewConstants : register(b0, space1) {
    float4x4 MVP;
    float2 Viewport;
    float2 _pad;
};

struct VSInput {
    float4 posAndNorm : TEXCOORD0;
    float4 emAndJac0  : TEXCOORD1;
    float4 jac1AndLoc : TEXCOORD2;
    float4 color      : TEXCOORD3;
};

struct VSOutput {
    float4 position   : SV_Position;
    float2 emCoord    : TEXCOORD0;
    float4 color      : TEXCOORD1;
    nointerpolation float glyphLocF : TEXCOORD2;
};

VSOutput main(VSInput input) {
    VSOutput output;

    output.position = mul(MVP, float4(input.posAndNorm.xy, 0.0, 1.0));
    output.emCoord = input.emAndJac0.xy;
    output.color = input.color;
    output.glyphLocF = input.jac1AndLoc.z;

    return output;
}
