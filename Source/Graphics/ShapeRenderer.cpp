#include <algorithm>
#include <cmath>

#include <SDL3/SDL_assert.h>

#include <glm/gtc/constants.hpp>

#include <Lucky/ShapeRenderer.hpp>

// ----------------------------------------------------------------------------
// Design note: batching and the Begin/End pattern
//
// The Begin/End pattern on ShapeRenderer looks like it should collect shapes
// into a batch and flush them as a single draw call, matching what its host
// BatchRenderer does for sprites. In the current implementation it does not:
// every Draw*() method calls EmitQuad(), which in turn calls
// BatchRenderer.Begin/End internally, flushing one draw call per shape.
//
// The reason is that each shape needs unique per-instance values in the
// fragment-shader uniform buffer (shapeType, halfThickness, count, starRatio,
// shapeSize, tri[0..2]). A uniform buffer holds one set of values at a time,
// so two shapes with different parameters cannot live in the same draw call
// as long as the parameters flow through uniforms.
//
// This is acceptable for the intended use case (debug draw, small UI, scenes
// with tens to low hundreds of shapes). It is not acceptable for thousands
// of shapes per frame. If ShapeRenderer ever needs to scale further, the
// plan is roughly:
//
//   1. Replace the per-draw uniform buffer with a GPU storage buffer holding
//      an array of SdfParams structs, one entry per shape instance.
//   2. Use instanced drawing: issue a single draw call for N shapes, letting
//      the vertex shader fetch its instance's SdfParams via SV_InstanceID.
//   3. Update BatchRenderer (or add a new renderer class) to support
//      instanced draws with per-instance SSBO data.
//   4. In EmitQuad, append the shape's SdfParams and its quad vertices to
//      per-frame buffers instead of calling BatchRenderer.Begin/End. Flush
//      the whole lot in ShapeRenderer::End().
//   5. Once all of the above is in place, Begin/End actually becomes a batch
//      scope and the API delivers on its name.
//
// Until then, Begin/End is just a state scope that caches the active shader
// and blend mode. It does not delimit a batch. Callers should assume each
// Draw*() call costs one draw call, one pipeline bind, and one uniform
// upload.
// ----------------------------------------------------------------------------

namespace Lucky {

ShapeRenderer::ShapeRenderer(BatchRenderer &batchRenderer)
    : batchRenderer(batchRenderer), blendMode(BlendMode::Alpha), transformMatrix(1.0f) {
}

void ShapeRenderer::Begin(
    BlendMode blendMode, Shader &shader, const glm::mat4 &transformMatrix) {
    SDL_assert(this->shader == nullptr);
    this->blendMode = blendMode;
    this->shader = &shader;
    this->transformMatrix = transformMatrix;
}

void ShapeRenderer::End() {
    shader = nullptr;
}

void ShapeRenderer::EmitQuad(
    glm::vec2 center, float quadHalf, float rotation, Color color, const SdfParams &params) {
    SDL_assert(shader != nullptr);

    glm::vec2 xy0 = center - glm::vec2(quadHalf);
    glm::vec2 xy1 = center + glm::vec2(quadHalf);

    batchRenderer.Begin(blendMode, *shader, transformMatrix);
    batchRenderer.SetFragmentUniformData(&params, sizeof(params));
    batchRenderer.BatchQuadUV({0, 0}, {1, 1}, xy0, xy1, color, rotation);
    batchRenderer.End();
}

void ShapeRenderer::DrawLine(
    glm::vec2 start, glm::vec2 end, float rotation, Color color, float thickness) {
    SDL_assert(thickness > 0.0f);
    SDL_assert(start != end);

    // Build a square quad centered at the line's midpoint that contains the
    // entire line plus the stroke padding. The endpoints get stored in
    // normalized [-1, 1] coordinates relative to that quad. The optional
    // rotation is passed through to EmitQuad, which rotates the whole quad
    // (and therefore the line inside it) around the midpoint.
    glm::vec2 center = (start + end) * 0.5f;
    float halfLength = glm::length(end - start) * 0.5f;
    float quadHalf = halfLength + thickness;

    SdfParams params = {};
    params.shapeType = 6;
    params.halfThickness = (thickness * 0.5f) / quadHalf;
    params.tri[0] = (start - center) / quadHalf;
    params.tri[1] = (end - center) / quadHalf;

    EmitQuad(center, quadHalf, rotation, color, params);
}

void ShapeRenderer::DrawCircle(glm::vec2 center, float radius, Color color, float thickness) {
    SDL_assert(radius > 0.0f);
    SDL_assert(thickness > 0.0f);

    float quadHalf = radius + thickness;

    SdfParams params = {};
    params.shapeType = 0;
    params.halfThickness = (thickness * 0.5f) / quadHalf;
    params.shapeSize.x = radius / quadHalf;

    EmitQuad(center, quadHalf, 0, color, params);
}

void ShapeRenderer::DrawRectangle(
    glm::vec2 center, glm::vec2 dimensions, float rotation, Color color, float thickness) {
    SDL_assert(dimensions.x > 0.0f && dimensions.y > 0.0f);
    SDL_assert(thickness > 0.0f);

    glm::vec2 halfExtents = dimensions * 0.5f;
    float maxExtent = std::max(halfExtents.x, halfExtents.y);
    float quadHalf = maxExtent + thickness;

    SdfParams params = {};
    params.shapeType = 1;
    params.halfThickness = (thickness * 0.5f) / quadHalf;
    params.shapeSize = halfExtents / quadHalf;

    EmitQuad(center, quadHalf, rotation, color, params);
}

void ShapeRenderer::DrawTriangle(
    glm::vec2 p0, glm::vec2 p1, glm::vec2 p2, float rotation, Color color, float thickness) {
    SDL_assert(thickness > 0.0f);

    glm::vec2 triCenter = (p0 + p1 + p2) / 3.0f;

    float maxDist = glm::length(p0 - triCenter);
    maxDist = std::max(maxDist, glm::length(p1 - triCenter));
    maxDist = std::max(maxDist, glm::length(p2 - triCenter));
    SDL_assert(maxDist > 0.0f); // caller passed three coincident points
    float quadHalf = maxDist + thickness;

    SdfParams params = {};
    params.shapeType = 2;
    params.halfThickness = (thickness * 0.5f) / quadHalf;
    params.tri[0] = (p0 - triCenter) / quadHalf;
    params.tri[1] = (p1 - triCenter) / quadHalf;
    params.tri[2] = (p2 - triCenter) / quadHalf;

    EmitQuad(triCenter, quadHalf, rotation, color, params);
}

void ShapeRenderer::DrawDiamond(glm::vec2 center, float width, float height, float rotation,
    Color color, float thickness) {
    SDL_assert(width > 0.0f);
    SDL_assert(height > 0.0f);
    SDL_assert(thickness > 0.0f);

    float halfWidth = width * 0.5f;
    float halfHeight = height * 0.5f;
    float maxExtent = std::max(halfWidth, halfHeight);
    float quadHalf = maxExtent + thickness;

    SdfParams params = {};
    params.shapeType = 3;
    params.halfThickness = (thickness * 0.5f) / quadHalf;
    params.shapeSize = {halfWidth / quadHalf, halfHeight / quadHalf};

    EmitQuad(center, quadHalf, rotation, color, params);
}

void ShapeRenderer::DrawRegularPolygon(glm::vec2 center, float radius, int sides, float rotation,
    Color color, float thickness) {
    SDL_assert(radius > 0.0f);
    SDL_assert(sides >= 3);
    SDL_assert(thickness > 0.0f);

    float quadHalf = radius + thickness;

    SdfParams params = {};
    params.shapeType = 4;
    params.halfThickness = (thickness * 0.5f) / quadHalf;
    params.count = sides;
    params.shapeSize.x = radius / quadHalf;

    EmitQuad(center, quadHalf, rotation, color, params);
}

void ShapeRenderer::DrawStar(glm::vec2 center, float outerRadius, float innerRadius, int points,
    float rotation, Color color, float thickness) {
    SDL_assert(outerRadius > 0.0f);
    SDL_assert(innerRadius > 0.0f);
    SDL_assert(innerRadius <= outerRadius);
    SDL_assert(points >= 2);
    SDL_assert(thickness > 0.0f);

    float quadHalf = outerRadius + thickness;

    // Convert inner/outer ratio to IQ's m parameter
    float an = glm::pi<float>() / static_cast<float>(points);
    float f = innerRadius / outerRadius;
    float denom = std::cos(an) - f;
    float m;
    if (denom > 0.001f) {
        m = glm::pi<float>() / std::atan(std::sin(an) / denom);
    } else {
        m = static_cast<float>(points);
    }

    SdfParams params = {};
    params.shapeType = 5;
    params.halfThickness = (thickness * 0.5f) / quadHalf;
    params.count = points;
    params.starRatio = m;
    params.shapeSize.x = outerRadius / quadHalf;

    EmitQuad(center, quadHalf, rotation, color, params);
}

std::vector<glm::vec2> ShapeRenderer::GetRectangleVertices(
    glm::vec2 center, glm::vec2 dimensions, float rotation) {
    glm::vec2 halfExtents = dimensions * 0.5f;
    float c = std::cos(rotation);
    float s = std::sin(rotation);
    glm::vec2 ax(c, s);
    glm::vec2 ay(-s, c);
    glm::vec2 dx = ax * halfExtents.x;
    glm::vec2 dy = ay * halfExtents.y;
    return {
        center - dx - dy,
        center + dx - dy,
        center + dx + dy,
        center - dx + dy,
    };
}

std::vector<glm::vec2> ShapeRenderer::GetTriangleVertices(
    glm::vec2 p0, glm::vec2 p1, glm::vec2 p2, float rotation) {
    glm::vec2 center = (p0 + p1 + p2) / 3.0f;
    float c = std::cos(rotation);
    float s = std::sin(rotation);
    auto rotate = [&](glm::vec2 p) {
        glm::vec2 d = p - center;
        return center + glm::vec2(c * d.x - s * d.y, s * d.x + c * d.y);
    };
    return {rotate(p0), rotate(p1), rotate(p2)};
}

std::vector<glm::vec2> ShapeRenderer::GetDiamondVertices(
    glm::vec2 center, float width, float height, float rotation) {
    float halfWidth = width * 0.5f;
    float halfHeight = height * 0.5f;
    float c = std::cos(rotation);
    float s = std::sin(rotation);
    glm::vec2 ax(c, s);
    glm::vec2 ay(-s, c);
    return {
        center + ax * halfWidth,
        center + ay * halfHeight,
        center - ax * halfWidth,
        center - ay * halfHeight,
    };
}

std::vector<glm::vec2> ShapeRenderer::GetRegularPolygonVertices(
    glm::vec2 center, float radius, int sides, float rotation) {
    SDL_assert(sides >= 3);
    std::vector<glm::vec2> verts;
    verts.reserve(sides);
    float step = glm::two_pi<float>() / sides;
    for (int i = 0; i < sides; i++) {
        float a = rotation + glm::half_pi<float>() + step * i;
        verts.push_back(center + glm::vec2(std::cos(a), std::sin(a)) * radius);
    }
    return verts;
}

std::vector<glm::vec2> ShapeRenderer::GetStarVertices(
    glm::vec2 center, float outerRadius, float innerRadius, int points, float rotation) {
    SDL_assert(points >= 2);
    std::vector<glm::vec2> verts;
    verts.reserve(points * 2);
    float step = glm::pi<float>() / points;
    for (int i = 0; i < points * 2; i++) {
        float a = rotation + glm::half_pi<float>() + step * i;
        float r = (i % 2 == 0) ? outerRadius : innerRadius;
        verts.push_back(center + glm::vec2(std::cos(a), std::sin(a)) * r);
    }
    return verts;
}

} // namespace Lucky
