Texture2D u_texture : register(t0, space2);
SamplerState u_sampler : register(s0, space2);

struct PSInput {
    float2 TexCoord : TEXCOORD0;
    float4 Color    : TEXCOORD1;
};

struct PSOutput {
    float4 Color : SV_Target;
};

PSOutput main(PSInput input) {
    PSOutput output;
    output.Color = u_texture.Sample(u_sampler, input.TexCoord) * input.Color;
    return output;
}
