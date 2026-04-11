#pragma once

#include <memory>

#include <glm/glm.hpp>

#include <Lucky/BatchRenderer.hpp>
#include <Lucky/Color.hpp>
#include <Lucky/GraphicsDevice.hpp>
#include <Lucky/MathConstants.hpp>
#include <Lucky/Texture.hpp>

namespace Lucky {

struct DebugDraw {
  public:
    DebugDraw(GraphicsDevice &graphicsDevice, std::shared_ptr<BatchRenderer> batchRenderer);
    DebugDraw(const DebugDraw &) = delete;
    DebugDraw(const DebugDraw &&) = delete;
    ~DebugDraw();

    DebugDraw &operator=(const DebugDraw &) = delete;
    DebugDraw &operator=(const DebugDraw &&) = delete;

    void BeginFrame();
    void EndFrame();

    void SetTransform(const glm::mat4 &transform);

    // todo: These functions need an overhaul, they tend to scale from the center
    // while that's convenient, it is imprecise which is less that awesome for a
    // debugging tool
    void DrawLine(const glm::vec2 &start, const glm::vec2 &end, const Color &color = Color::White,
        const float thickness = 1.0f);
    void DrawLineList(glm::vec2 *points, int pointCount, const glm::vec2 &offset, bool closeShape,
        const Color &color, float thickness);
    void DrawArrow(const glm::vec2 &start, const glm::vec2 &end, const Color &color,
        float thickness, float arrowHeadLength = 15.0f,
        float arrowHeadAngle = 25 * MathConstants::Pi / 180.0f);
    void DrawFilledRectangle(
        const glm::vec2 &position, const glm::vec2 &dimensions, float rotation, const Color &color);
    void DrawRectangle(const glm::vec2 &position, const glm::vec2 &dimensions, const Color &color,
        float thickness);
    void DrawPoint(const glm::vec2 &position, const Color &color, float size, float thickness);

    void NewDrawFilledRectangle(
        const glm::vec2 &topLeft, const glm::vec2 &dimensions, const Color &color);

    void NewDrawRectangle(
        const glm::vec2 &topLeft, const glm::vec2 &dimensions, float thickness, const Color &color);

  private:
    std::shared_ptr<BatchRenderer> batchRenderer;
    std::unique_ptr<Texture> texture;
    glm::mat4 transformMatrix;
    bool beginCalled;
};

} // namespace Lucky
