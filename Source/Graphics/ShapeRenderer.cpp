#include <algorithm>
#include <cmath>

#include <glm/gtc/constants.hpp>

#include <Lucky/ShapeRenderer.hpp>

namespace Lucky {

ShapeRenderer::ShapeRenderer(BatchRenderer &batchRenderer) : batchRenderer(batchRenderer) {
}

void ShapeRenderer::Begin(BlendMode blendMode, Shader &shader) {
    this->blendMode = blendMode;
    this->shader = &shader;
}

void ShapeRenderer::End() {
    shader = nullptr;
}

void ShapeRenderer::EmitQuad(
    glm::vec2 center, float quadHalf, float rotation, Color color, const SdfParams &params) {
    glm::vec2 xy0 = center - glm::vec2(quadHalf);
    glm::vec2 xy1 = center + glm::vec2(quadHalf);

    batchRenderer.Begin(blendMode, *shader);
    batchRenderer.SetFragmentUniformData(&params, sizeof(params));
    batchRenderer.BatchQuadUV({0, 0}, {1, 1}, xy0, xy1, color, rotation);
    batchRenderer.End();
}

void ShapeRenderer::DrawLine(glm::vec2 start, glm::vec2 end, Color color, float thickness) {
    // Build a square quad centered at the line's midpoint that contains the
    // entire line plus the stroke padding. The endpoints get stored in
    // normalized [-1, 1] coordinates relative to that quad.
    glm::vec2 center = (start + end) * 0.5f;
    float halfLength = glm::length(end - start) * 0.5f;
    float quadHalf = halfLength + thickness;

    SdfParams params = {};
    params.shapeType = 6;
    params.halfThickness = (thickness * 0.5f) / quadHalf;
    params.tri[0] = (start - center) / quadHalf;
    params.tri[1] = (end - center) / quadHalf;

    EmitQuad(center, quadHalf, 0, color, params);
}

void ShapeRenderer::DrawCircle(glm::vec2 center, float radius, Color color, float thickness) {
    float quadHalf = radius + thickness;

    SdfParams params = {};
    params.shapeType = 0;
    params.halfThickness = (thickness * 0.5f) / quadHalf;
    params.shapeSize.x = radius / quadHalf;

    EmitQuad(center, quadHalf, 0, color, params);
}

void ShapeRenderer::DrawRectangle(
    glm::vec2 center, glm::vec2 dimensions, float rotation, Color color, float thickness) {
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
    glm::vec2 triCenter = (p0 + p1 + p2) / 3.0f;

    float maxDist = glm::length(p0 - triCenter);
    maxDist = std::max(maxDist, glm::length(p1 - triCenter));
    maxDist = std::max(maxDist, glm::length(p2 - triCenter));
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
