#pragma once

#include <memory>

#include <glm/glm.hpp>

#include <Lucky/Graphics/BatchRenderer.hpp>
#include <Lucky/Graphics/Color.hpp>
#include <Lucky/Graphics/GraphicsDevice.hpp>
#include <Lucky/Graphics/Texture.hpp>
#include <Lucky/Math/MathConstants.hpp>

namespace Lucky
{
    struct DebugDraw
    {
      public:
        DebugDraw(std::shared_ptr<GraphicsDevice> graphicsDevice);
        DebugDraw(const DebugDraw &) = delete;
        ~DebugDraw();

        DebugDraw &operator=(const DebugDraw &) = delete;

        void BeginFrame();
        void EndFrame();

        void SetTransform(const glm::mat4 &transform);

        void DrawLine(const glm::vec2 &start, const glm::vec2 &end, const Color &color = Color::White,
            const float thickness = 1.0f);
        void DrawLineList(glm::vec2 *points, int pointCount, const glm::vec2 &offset, bool closeShape,
            const Color &color, float thickness);
        void DrawArrow(const glm::vec2 &start, const glm::vec2 &end, const Color &color, float thickness,
            float arrowHeadLength = 15.0f, float arrowHeadAngle = 25 * MathConstants::Pi / 180.0f);
        void DrawFilledRectangle(
            const glm::vec2 &position, const glm::vec2 &dimensions, float rotation, const Color &color);
        void DrawRectangle(const glm::vec2 &position, const glm::vec2 &dimensions, const Color &color, float thickness);
        void DrawPoint(const glm::vec2 &position, const Color &color, float size, float thickness);

      private:
        std::unique_ptr<BatchRenderer> batchRenderer;
        std::shared_ptr<Texture> texture;
        glm::mat4 transformMatrix;
        bool beginCalled;
    };
} // namespace Lucky
