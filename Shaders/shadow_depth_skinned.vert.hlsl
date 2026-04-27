// Skinned shadow-depth vertex shader. Produces depth-only output for
// the shadow pass when the source mesh is skinned; the fragment shader
// is the same shadow_depth.frag.hlsl used by the rigid path.

cbuffer Frame : register(b0, space1) {
    float4x4 LightViewProj;
};

// Slot 1 is unused in the shadow path but kept declared so the
// uniform-buffer slot numbering matches the skinned forward shader.
// SDL_GPU shader reflection counts declared cbuffers, and a missing
// b1 between b0 and b2 confuses the slot-to-API-index mapping on
// some backends. The contents are trivially referenced below to
// stop dxc from stripping the declaration.
cbuffer ObjectUnused : register(b1, space1) {
    float4 ColorTintUnused;
};

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
    float4 Position : SV_Position;
};

VSOutput main(VSInput input) {
    VSOutput o;

    float4x4 skinMatrix =
        input.Weights.x * JointMatrices[input.Joints.x] +
        input.Weights.y * JointMatrices[input.Joints.y] +
        input.Weights.z * JointMatrices[input.Joints.z] +
        input.Weights.w * JointMatrices[input.Joints.w];

    float4 worldPos = mul(skinMatrix, float4(input.Position, 1.0));
    o.Position = mul(LightViewProj, worldPos);
    // Trivial reference keeps dxc from stripping the b1 declaration.
    o.Position += 1e-9 * ColorTintUnused.x;
    return o;
}
