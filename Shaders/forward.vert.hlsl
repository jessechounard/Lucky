cbuffer Frame : register(b0, space1) {
    float4x4 ViewProjection;
};

cbuffer Object : register(b1, space1) {
    float4x4 Model;
    float4   ColorTint;
};

struct VSInput {
    float3 Position : TEXCOORD0;
    float2 TexCoord : TEXCOORD1;
    float3 Normal   : TEXCOORD2;
    float4 Tangent  : TEXCOORD3; // xyz = tangent, w = bitangent handedness
};

struct VSOutput {
    float3 WorldPos  : TEXCOORD0;
    float2 TexCoord  : TEXCOORD1;
    float3 Normal    : TEXCOORD2;
    float3 ColorTint : TEXCOORD3;
    float4 Tangent   : TEXCOORD4; // xyz transformed; w handedness preserved
    float4 Position  : SV_Position;
};

VSOutput main(VSInput input) {
    VSOutput o;

    float4 worldPos = mul(Model, float4(input.Position, 1.0));
    o.WorldPos = worldPos.xyz;
    o.Position = mul(ViewProjection, worldPos);

    // Normal is transformed by the upper 3x3 of the model matrix.
    // For non-uniform scales the inverse-transpose would be more correct,
    // but uniform scales (and the demo's transforms) are fine with this.
    o.Normal = normalize(mul((float3x3)Model, input.Normal));

    // Tangent: transform xyz the same way as the normal; preserve w
    // (bitangent handedness) untouched. Don't normalize here -- the
    // fragment shader does it after Gram-Schmidt against the normal.
    o.Tangent = float4(mul((float3x3)Model, input.Tangent.xyz), input.Tangent.w);

    o.TexCoord = input.TexCoord;
    o.ColorTint = ColorTint.rgb;
    return o;
}
