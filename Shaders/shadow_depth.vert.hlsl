cbuffer Frame : register(b0, space1) {
    float4x4 LightViewProj;
};

cbuffer Object : register(b1, space1) {
    float4x4 Model;
    float4   ColorTint; // unused in shadow pass; kept to share the per-draw UBO
};

struct VSInput {
    float3 Position : TEXCOORD0;
    float2 TexCoord : TEXCOORD1;
    float3 Normal   : TEXCOORD2;
};

struct VSOutput {
    float4 Position : SV_Position;
};

VSOutput main(VSInput input) {
    VSOutput o;
    float4 worldPos = mul(Model, float4(input.Position, 1.0));
    o.Position = mul(LightViewProj, worldPos);
    return o;
}
