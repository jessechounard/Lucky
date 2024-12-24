#pragma once

#include <stdint.h>

#include <SDL3/SDL_video.h>

#include <Lucky/Graphics/Color.hpp>
#include <Lucky/Graphics/ShaderProgram.hpp>
#include <Lucky/Graphics/Types.hpp>
#include <Lucky/Math/Rectangle.hpp>

namespace Lucky
{
    enum class GraphicsAPI
    {
        OpenGL,
    };

    enum class VerticalSyncType
    {
        AdaptiveEnabled, // Adaptive with Enabled as fallback
        Enabled,
        Disabled,
    };

    enum class PrimitiveType
    {
        Triangles,
        TriangleStrip,
        Lines,
        LineStrip,
        Points,
    };

    struct Color;
    struct ShaderProgram;
    struct Texture;
    struct VertexBuffer;

    struct GraphicsDevice
    {
      public:
        static uint32_t PrepareWindowAttributes(GraphicsAPI api);

        GraphicsDevice(GraphicsAPI api, void *windowHandle, VerticalSyncType verticalSyncType);
        GraphicsDevice(const GraphicsDevice &) = delete;
        ~GraphicsDevice();

        GraphicsDevice &operator=(const GraphicsDevice &) = delete;

        void SetViewport(const Rectangle &viewport);
        void GetViewport(Rectangle &viewport) const;

        int GetScreenWidth() const
        {
            return screenWidth;
        }

        int GetScreenHeight() const
        {
            return screenHeight;
        }

        void ClearScreen(const Color &clearColor);

        void SetBlendMode(BlendMode blendMode);

        void EnableScissorsRectangle(const Rectangle &scissorsRectangle);
        void DisableScissorsRectangle();

        void BindTexture(const Texture &texture);

        // Note: if you don't set the viewport, scissor rectangles might not work properly
        void BindRenderTarget(const Texture &texture, bool setViewport = true);
        void UnbindRenderTarget(bool resetViewport = true);
        bool IsUsingRenderTarget() const;

        void ReadPixels(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint8_t *pixels);

        void ApplyShaderProgram(const ShaderProgram &shaderProgram);

        void BeginFrame();
        void EndFrame();

        void DrawPrimitives(const VertexBuffer &vertexBuffer, PrimitiveType primitiveType, uint32_t vertexStart,
            uint32_t primitiveCount);

        void *GetGLContext()
        {
            return glContext;
        }

      private:
        Rectangle viewport;
        Color clearColor;

        SDL_GLContext glContext;
        BlendMode blendMode;

        int screenWidth;
        int screenHeight;

        bool scissorsEnabled;
        Rectangle scissorsRectangle;

        int drawCallsThisFrame;

        uint32_t defaultFramebufferObject;
        uint32_t currentFramebufferObject;
    };
} // namespace Lucky
