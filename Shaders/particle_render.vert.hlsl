struct Particle {
    float2 position;
    float2 velocity;
    float4 color;
    float4 uvRect;
    float lifetime;
    float maxLifetime;
    float size;
    float rotation;
    float rotationSpeed;
    float _pad0;
    float _pad1;
    float _pad2;
};

StructuredBuffer<Particle> particles : register(t0, space0);

cbuffer RenderParams : register(b0, space1) {
    float4x4 ProjectionMatrix;
};

struct VSOutput {
    float2 TexCoord : TEXCOORD0;
    float4 Color : TEXCOORD1;
    float4 Position : SV_Position;
};

static const float2 corners[6] = {
    float2(-0.5, -0.5), float2(0.5, -0.5), float2(0.5, 0.5),
    float2(-0.5, -0.5), float2(0.5, 0.5),  float2(-0.5, 0.5),
};

static const float2 uvs[6] = {
    float2(0, 1), float2(1, 1), float2(1, 0),
    float2(0, 1), float2(1, 0), float2(0, 0),
};

VSOutput main(uint vertexID : SV_VertexID) {
    uint particleIndex = vertexID / 6;
    uint cornerIndex = vertexID % 6;

    Particle p = particles[particleIndex];

    float s, c;
    sincos(p.rotation, s, c);
    float2 corner = corners[cornerIndex] * p.size;
    float2 rotated = float2(corner.x * c - corner.y * s, corner.x * s + corner.y * c);
    float2 worldPos = p.position + rotated;

    VSOutput output;
    output.Position = mul(ProjectionMatrix, float4(worldPos, 0.0, 1.0));
    // Map local [0,1] UVs to atlas region
    float2 localUV = uvs[cornerIndex];
    output.TexCoord = p.uvRect.xy + localUV * p.uvRect.zw;
    float fade = saturate(p.lifetime / p.maxLifetime);
    output.Color = p.color * fade;

    if (p.lifetime <= 0.0)
        output.Position = float4(0, 0, 0, 0);

    return output;
}
