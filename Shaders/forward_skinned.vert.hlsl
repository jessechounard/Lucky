// Skinned forward vertex shader. Produces the same outputs as
// forward.vert.hlsl so the existing forward.frag.hlsl works unchanged;
// the difference is that world-space position and normal are derived
// by blending up to four joint matrices per vertex instead of
// multiplying by a single per-object Model matrix.

cbuffer Frame : register(b0, space1) {
    float4x4 ViewProjection;
};

cbuffer Object : register(b1, space1) {
    float4 ColorTint;
};

// Joint matrices are world-space (already include the model placement),
// so the vertex shader applies them directly without a separate model
// transform. 128 entries is enough for typical game characters
// (CesiumMan = 19, fully-rigged humans usually 50-80) and fits well
// inside the 16 KB push-uniform cap (8 KB used).
cbuffer Joints : register(b2, space1) {
    float4x4 JointMatrices[128];
};

struct VSInput {
    float3 Position : TEXCOORD0;
    float2 TexCoord : TEXCOORD1;
    float3 Normal   : TEXCOORD2;
    uint4  Joints   : TEXCOORD3;
    float4 Weights  : TEXCOORD4;
};

struct VSOutput {
    float3 WorldPos  : TEXCOORD0;
    float2 TexCoord  : TEXCOORD1;
    float3 Normal    : TEXCOORD2;
    float3 ColorTint : TEXCOORD3;
    float4 Position  : SV_Position;
};

VSOutput main(VSInput input) {
    VSOutput o;

    // Linear blend skinning: weighted sum of per-joint world matrices.
    // Weights are expected to sum to 1.0; if they don't, the implicit
    // scaling is treated as authored intent (used for partial influence).
    float4x4 skinMatrix =
        input.Weights.x * JointMatrices[input.Joints.x] +
        input.Weights.y * JointMatrices[input.Joints.y] +
        input.Weights.z * JointMatrices[input.Joints.z] +
        input.Weights.w * JointMatrices[input.Joints.w];

    float4 worldPos = mul(skinMatrix, float4(input.Position, 1.0));
    o.WorldPos = worldPos.xyz;
    o.Position = mul(ViewProjection, worldPos);

    // Normal transform: upper-3x3 of the skin matrix. Same uniform-scale
    // assumption as the rigid path; non-uniform scales would need an
    // inverse-transpose per joint.
    o.Normal = normalize(mul((float3x3)skinMatrix, input.Normal));

    o.TexCoord = input.TexCoord;
    o.ColorTint = ColorTint.rgb;
    return o;
}
