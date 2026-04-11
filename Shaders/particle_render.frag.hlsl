Texture2D particleTexture : register(t0, space2);
SamplerState particleSampler : register(s0, space2);

struct PSInput {
    float2 TexCoord : TEXCOORD0;
    float4 Color : TEXCOORD1;
};

struct PSOutput {
    float4 Color : SV_Target;
};

PSOutput main(PSInput input) {
    float4 texel = particleTexture.Sample(particleSampler, input.TexCoord);
    float shape = texel.a;

    float opacity = shape * input.Color.a;

    PSOutput output;
    output.Color = float4(input.Color.rgb * texel.rgb * shape, opacity);
    return output;
}
