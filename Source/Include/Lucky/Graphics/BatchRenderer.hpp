#pragma once

#include <memory>
#include <stdint.h>
#include <vector>

#include <glm/glm.hpp>

#include <Lucky/Graphics/GraphicsDevice.hpp>
#include <Lucky/Graphics/ShaderProgram.hpp>
#include <Lucky/Graphics/Types.hpp>
#include <Lucky/Graphics/VertexBuffer.hpp>
#include <Lucky/Math/Rectangle.hpp>
#include <Lucky/Math/Vertex.hpp>

namespace Lucky
{
    struct BatchRenderer;
    struct Color;

    inline constexpr UVMode operator&(UVMode lhs, UVMode rhs)
    {
        return static_cast<UVMode>(static_cast<uint32_t>(lhs) & static_cast<uint32_t>(rhs));
    }

    inline UVMode &operator&=(UVMode &lhs, UVMode rhs)
    {
        lhs = lhs & rhs;
        return lhs;
    }

    inline constexpr UVMode operator|(UVMode lhs, UVMode rhs)
    {
        return static_cast<UVMode>(static_cast<uint32_t>(lhs) | static_cast<uint32_t>(rhs));
    }

    inline UVMode &operator|=(UVMode &lhs, UVMode rhs)
    {
        lhs = lhs | rhs;
        return lhs;
    }

    inline bool HasFlag(UVMode t, UVMode flag)
    {
        return (static_cast<uint32_t>(t) & static_cast<uint32_t>(flag)) != 0;
    }

    struct BatchRenderer
    {
      public:
        BatchRenderer(std::shared_ptr<GraphicsDevice> graphicsDevice, uint32_t maximumTriangles);
        BatchRenderer(const BatchRenderer &) = delete;
        ~BatchRenderer();

        BatchRenderer &operator=(const BatchRenderer &) = delete;

        bool BatchStarted() const
        {
            return batchStarted;
        }

        void Begin(BlendMode blendMode, std::shared_ptr<Texture> texture,
            std::shared_ptr<ShaderProgram> shaderProgram = nullptr, const glm::mat4 &transformMatrix = glm::mat4(1.0f));
        void End();

        void BatchQuadUV(
            const glm::vec2 &uv0, const glm::vec2 &uv1, const glm::vec2 &xy0, const glm::vec2 &xy1, const Color &color);

        void BatchQuad(const Rectangle *sourceRectangle, const glm::vec2 &position, const float rotation,
            const glm::vec2 &scale, const glm::vec2 &origin, const UVMode uvMode, const Color &color);

        void BatchTriangles(Vertex *triangleVertices, const int triangleCount);

      private:
        void Flush();

        std::shared_ptr<GraphicsDevice> graphicsDevice;
        std::shared_ptr<Texture> texture;
        std::shared_ptr<ShaderProgram> currentShaderProgram;
        std::unique_ptr<VertexShader> defaultVertexShader;
        std::unique_ptr<FragmentShader> defaultFragmentShader;
        std::shared_ptr<ShaderProgram> defaultShaderProgram;
        std::unique_ptr<VertexBuffer> vertexBuffer;
        glm::mat4 transformMatrix;
        BlendMode blendMode;

        uint32_t activeVertices;
        uint32_t maximumVertices;

        std::vector<Vertex> vertices;

        bool batchStarted;
    };
} // namespace Lucky