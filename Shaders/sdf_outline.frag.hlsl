#include "sdf_primitives.hlsl"

// General triangle SDF (from iquilezles.org)
float sdTriangleGeneral(float2 p, float2 p0, float2 p1, float2 p2) {
    float2 e0 = p1 - p0, e1 = p2 - p1, e2 = p0 - p2;
    float2 v0 = p - p0, v1 = p - p1, v2 = p - p2;
    float2 pq0 = v0 - e0 * clamp(dot(v0, e0) / dot(e0, e0), 0.0, 1.0);
    float2 pq1 = v1 - e1 * clamp(dot(v1, e1) / dot(e1, e1), 0.0, 1.0);
    float2 pq2 = v2 - e2 * clamp(dot(v2, e2) / dot(e2, e2), 0.0, 1.0);
    float s = sign(e0.x * e2.y - e0.y * e2.x);
    float2 d = min(min(
        float2(dot(pq0, pq0), s * (v0.x * e0.y - v0.y * e0.x)),
        float2(dot(pq1, pq1), s * (v1.x * e1.y - v1.y * e1.x))),
        float2(dot(pq2, pq2), s * (v2.x * e2.y - v2.y * e2.x)));
    return -sqrt(d.x) * sign(d.y);
}

// General regular polygon (approximate — slightly rounded corners)
float sdNgon(float2 p, float r, int n) {
    float an = PI / float(n);
    float bn = glsl_mod(atan2(p.x, p.y), 2.0 * an) - an;
    return length(p) * cos(bn) - r * cos(an);
}

struct PSInput {
    float2 TexCoord : TEXCOORD0;
    float4 Color    : TEXCOORD1;
};

struct PSOutput {
    float4 Color : SV_Target;
};

cbuffer SDFParams : register(b0, space3) {
    uint shapeType;
    float halfThickness;
    uint count;
    float starRatio;
    float2 shapeSize;
    float2 tri0;
    float2 tri1;
    float2 tri2;
};

PSOutput main(PSInput input) {
    PSOutput output;
    float2 p = input.TexCoord * 2.0 - 1.0;

    float d = 0;
    switch (shapeType) {
        case 0: d = sdCircle(p, shapeSize.x); break;
        case 1: d = sdBox(p, shapeSize); break;
        case 2: d = sdTriangleGeneral(p, tri0, tri1, tri2); break;
        case 3: d = sdRhombus(p, shapeSize); break;
        case 4: d = sdNgon(p, shapeSize.x, count); break;
        case 5: d = sdStar(p, shapeSize.x, count, starRatio); break;
        case 6: d = sdSegment(p, tri0, tri1); break;
        default: d = sdCircle(p, shapeSize.x); break;
    }

    d = abs(d) - halfThickness;
    float aa = fwidth(d);
    float alpha = 1.0 - smoothstep(-aa, aa, d);

    output.Color = input.Color * alpha;
    return output;
}
