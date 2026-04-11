#include <SDL3/SDL_assert.h>

#include <glm/glm.hpp>

#include <Lucky/DebugDraw.hpp>

using namespace glm;

namespace Lucky {

DebugDraw::DebugDraw(GraphicsDevice &graphicsDevice, std::shared_ptr<BatchRenderer> batchRenderer)
    : transformMatrix(1.0f) {
    if (batchRenderer) {
        this->batchRenderer = batchRenderer;
    } else {
        this->batchRenderer = std::make_shared<BatchRenderer>(graphicsDevice, 1000);
    }
    uint8_t pixels[] = {0xff, 0xff, 0xff, 0xff};
    texture = std::make_unique<Texture>(
        graphicsDevice, TextureType::Default, 1, 1, pixels, 4, TextureFilter::Point);
    beginCalled = false;
}

DebugDraw::~DebugDraw() {
}

void DebugDraw::SetTransform(const glm::mat4 &transform) {
    transformMatrix = transform;
}

void DebugDraw::BeginFrame() {
    // todo: check beginCalled

    batchRenderer->Begin(BlendMode::Alpha, *texture, transformMatrix);
    beginCalled = true;
}

void DebugDraw::EndFrame() {
    // todo: check beginCalled

    batchRenderer->End();
    beginCalled = false;
}

void DebugDraw::DrawLine(
    const glm::vec2 &start, const glm::vec2 &end, const Color &color, const float thickness) {
    // todo: check beginCalled

    glm::vec2 difference = end - start;
    float length = glm::length(difference);
    difference = glm::normalize(difference);
    float angle = atan2(difference.y, difference.x);

    batchRenderer->BatchQuad(
        nullptr, start, angle, {length, thickness}, {0, 0.5f}, UVMode::Normal, color);
}

void DebugDraw::DrawLineList(glm::vec2 *points, int pointCount, const glm::vec2 &offset,
    bool closeShape, const Color &color, float thickness) {
    SDL_assert(points != nullptr);
    SDL_assert(pointCount > 0);

    // todo: check beginCalled

    for (int i = pointCount - 1; i > 0; i--) {
        DrawLine(points[i] + offset, points[i - 1] + offset, color, thickness);
    }
    if (closeShape) {
        DrawLine(points[0] + offset, points[pointCount - 1] + offset, Color::White, thickness);
    }
}

void DebugDraw::DrawArrow(const glm::vec2 &start, const glm::vec2 &end, const Color &color,
    float thickness, float arrowHeadLength, float arrowHeadAngle) {
    // todo: check beginCalled

    glm::vec2 difference = end - start;
    float length = glm::length(difference);
    difference = glm::normalize(difference);
    float angle = atan2(difference.y, difference.x);

    batchRenderer->BatchQuad(
        nullptr, start, angle, {length, thickness}, {0, 0.5f}, UVMode::Normal, color);
    batchRenderer->BatchQuad(nullptr,
        end,
        angle + (MathConstants::Pi - arrowHeadAngle),
        {arrowHeadLength, thickness},
        {0, 0.5f},
        UVMode::Normal,
        color);
    batchRenderer->BatchQuad(nullptr,
        end,
        angle + (MathConstants::Pi + arrowHeadAngle),
        {arrowHeadLength, thickness},
        {0, 0.5f},
        UVMode::Normal,
        color);
}

void DebugDraw::DrawFilledRectangle(
    const glm::vec2 &position, const glm::vec2 &dimensions, float rotation, const Color &color) {
    // todo: check beginCalled

    batchRenderer->BatchQuad(
        nullptr, position, rotation, dimensions, {0.5f, 0.5f}, UVMode::Normal, color);
}

void DebugDraw::DrawRectangle(
    const glm::vec2 &position, const glm::vec2 &dimensions, const Color &color, float thickness) {
    // todo: check beginCalled

    glm::vec2 topLeft = position - dimensions / 2.0f;
    glm::vec2 topRight = position + glm::vec2(dimensions.x, -dimensions.y) / 2.0f;
    glm::vec2 bottomRight = position + dimensions / 2.0f;
    glm::vec2 bottomLeft = position + glm::vec2(-dimensions.x, dimensions.y) / 2.0f;

    DrawLine(topLeft, topRight, color);
    DrawLine(topRight, bottomRight, color);
    DrawLine(bottomRight, bottomLeft, color);
    DrawLine(bottomLeft, topLeft, color);
}

void DebugDraw::DrawPoint(
    const glm::vec2 &position, const Color &color, float size, float thickness) {
    // todo: check beginCalled

    float length = 0.7071f * size / 2;
    batchRenderer->BatchQuad(nullptr,
        position + glm::vec2(-size / 2.0f, 0),
        0,
        {size, thickness},
        {0, 0.5f},
        UVMode::Normal,
        color);
    batchRenderer->BatchQuad(nullptr,
        position + glm::vec2(0, -size / 2.0f),
        MathConstants::Pi / 2.0f,
        {size, thickness},
        {0, 0.5f},
        UVMode::Normal,
        color);
    batchRenderer->BatchQuad(nullptr,
        position + glm::vec2(-length, -length),
        MathConstants::Pi / 4.0f,
        {size, thickness},
        {0, 0.5f},
        UVMode::Normal,
        color);
    batchRenderer->BatchQuad(nullptr,
        position + glm::vec2(-length, length),
        -MathConstants::Pi / 4.0f,
        {size, thickness},
        {0, 0.5f},
        UVMode::Normal,
        color);
}

void DebugDraw::NewDrawFilledRectangle(
    const glm::vec2 &topLeft, const glm::vec2 &dimensions, const Color &color) {

    batchRenderer->BatchQuad(nullptr, topLeft, 0, dimensions, {0, 0}, UVMode::Normal, color);
}

void DebugDraw::NewDrawRectangle(
    const glm::vec2 &topLeft, const glm::vec2 &dimensions, float thickness, const Color &color) {
    batchRenderer->BatchQuad(
        nullptr, topLeft, 0, {dimensions.x, thickness}, {0, 0}, UVMode::Normal, color);
    batchRenderer->BatchQuad(nullptr,
        topLeft + glm::vec2{0, dimensions.y},
        0,
        {dimensions.x, thickness},
        {0, 1},
        UVMode::Normal,
        color);
    batchRenderer->BatchQuad(
        nullptr, topLeft, 0, {thickness, dimensions.y}, {0, 0}, UVMode::Normal, color);
    batchRenderer->BatchQuad(nullptr,
        topLeft + glm::vec2{dimensions.x, 0},
        0,
        {thickness, dimensions.y},
        {1, 0},
        UVMode::Normal,
        color);
}

} // namespace Lucky
