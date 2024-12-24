#include <assert.h>

#include <glm/glm.hpp>

#include <Lucky/Graphics/DebugDraw.hpp>

using namespace glm;

namespace Lucky
{
    DebugDraw::DebugDraw(std::shared_ptr<GraphicsDevice> graphicsDevice)
        : transformMatrix(1.0f)
    {
        batchRenderer = std::make_unique<BatchRenderer>(graphicsDevice, 1000);
        uint8_t pixels[] = {0xff, 0xff, 0xff, 0xff};
        texture = std::make_shared<Texture>(TextureType::Default, 1, 1, pixels, 4, TextureFilter::Point);
        beginCalled = false;
    }

    DebugDraw::~DebugDraw()
    {
    }

    void DebugDraw::SetTransform(const glm::mat4 &transform)
    {
        transformMatrix = transform;
    }

    void DebugDraw::BeginFrame()
    {
        // todo: check beginCalled

        batchRenderer->Begin(BlendMode::Alpha, texture, nullptr, transformMatrix);
        beginCalled = true;
    }

    void DebugDraw::EndFrame()
    {
        // todo: check beginCalled

        batchRenderer->End();
        beginCalled = false;
    }

    void DebugDraw::DrawLine(const glm::vec2 &start, const glm::vec2 &end, const Color &color, const float thickness)
    {
        // todo: check beginCalled

        glm::vec2 difference = end - start;
        float length = glm::length(difference);
        difference = glm::normalize(difference);
        float angle = atan2(difference.y, difference.x);

        batchRenderer->BatchQuad(nullptr, start, angle, {length, thickness}, {0, 0.5f}, UVMode::Normal, color);
    }

    void DebugDraw::DrawLineList(glm::vec2 *points, int pointCount, const glm::vec2 &offset, bool closeShape,
        const Color &color, float thickness)
    {
        assert(points != nullptr);
        assert(pointCount > 0);

        // todo: check beginCalled

        glm::vec2 lastPoint;
        int startPoint;

        if (closeShape)
        {
            lastPoint = points[pointCount - 1];
            startPoint = 0;
        }
        else
        {
            lastPoint = points[0];
            startPoint = 1;
        }

        for (int i = startPoint; i < pointCount; i++)
        {
            DrawLine(lastPoint + offset, points[i] + offset, color, thickness);
            lastPoint = points[i];
        }
    }

    void DebugDraw::DrawArrow(const glm::vec2 &start, const glm::vec2 &end, const Color &color, float thickness,
        float arrowHeadLength, float arrowHeadAngle)
    {
        // todo: check beginCalled

        glm::vec2 difference = end - start;
        float length = glm::length(difference);
        difference = glm::normalize(difference);
        float angle = atan2(difference.y, difference.x);

        batchRenderer->BatchQuad(nullptr, start, angle, {length, thickness}, {0, 0.5f}, UVMode::Normal, color);
        batchRenderer->BatchQuad(nullptr, end, angle + (MathConstants::Pi - arrowHeadAngle),
            {arrowHeadLength, thickness}, {0, 0.5f}, UVMode::Normal, color);
        batchRenderer->BatchQuad(nullptr, end, angle + (MathConstants::Pi + arrowHeadAngle),
            {arrowHeadLength, thickness}, {0, 0.5f}, UVMode::Normal, color);
    }

    void DebugDraw::DrawFilledRectangle(
        const glm::vec2 &position, const glm::vec2 &dimensions, float rotation, const Color &color)
    {
        // todo: check beginCalled

        batchRenderer->BatchQuad(nullptr, position, rotation, dimensions, {0.5f, 0.5f}, UVMode::Normal, color);
    }

    void DebugDraw::DrawRectangle(
        const glm::vec2 &position, const glm::vec2 &dimensions, const Color &color, float thickness)
    {
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

    void DebugDraw::DrawPoint(const glm::vec2 &position, const Color &color, float size, float thickness)
    {
        // todo: check beginCalled

        float length = 0.7071f * size / 2;
        batchRenderer->BatchQuad(
            nullptr, position + glm::vec2(-size / 2.0f, 0), 0, {size, thickness}, {0, 0.5f}, UVMode::Normal, color);
        batchRenderer->BatchQuad(nullptr, position + glm::vec2(0, -size / 2.0f), MathConstants::Pi / 2.0f,
            {size, thickness}, {0, 0.5f}, UVMode::Normal, color);
        batchRenderer->BatchQuad(nullptr, position + glm::vec2(-length, -length), MathConstants::Pi / 4.0f,
            {size, thickness}, {0, 0.5f}, UVMode::Normal, color);
        batchRenderer->BatchQuad(nullptr, position + glm::vec2(-length, length), -MathConstants::Pi / 4.0f,
            {size, thickness}, {0, 0.5f}, UVMode::Normal, color);
    }
} // namespace Lucky