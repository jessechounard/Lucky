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

RWStructuredBuffer<Particle> particles : register(u0, space1);
RWByteAddressBuffer aliveCounter : register(u1, space1);

cbuffer UpdateParams : register(b0, space2) {
    float deltaTime;
    float _pad0;
    float2 gravity;
    float screenWidth;
    float screenHeight;
    float2 _pad1;
};

[numthreads(64, 1, 1)]
void main(uint3 tid : SV_DispatchThreadID) {
    Particle p = particles[tid.x];
    if (p.lifetime <= 0.0)
        return;

    p.lifetime -= deltaTime;
    p.velocity += gravity * deltaTime;
    p.position += p.velocity * deltaTime;
    p.rotation += p.rotationSpeed * deltaTime;

    if (p.lifetime > 0.0) {
        uint dummy;
        aliveCounter.InterlockedAdd(0, 1, dummy);
    }

    particles[tid.x] = p;
}
