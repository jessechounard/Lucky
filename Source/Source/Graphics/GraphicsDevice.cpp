#include <assert.h>
#include <stdexcept>

#include <SDL3/SDL.h>
#include <spdlog/spdlog.h>

#include <Lucky/Graphics/Color.hpp>
#include <Lucky/Graphics/GraphicsDevice.hpp>
#include <Lucky/Graphics/Texture.hpp>
#include <Lucky/Graphics/VertexBuffer.hpp>
#include <Lucky/Math/Rectangle.hpp>

#include "IncludeOpenGL.h"

namespace Lucky
{
    uint32_t GraphicsDevice::PrepareWindowAttributes(GraphicsAPI api)
    {
        switch (api)
        {
        case GraphicsAPI::OpenGL:
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);

            SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
            SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
            SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
            SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
            SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);

            return SDL_WINDOW_OPENGL; // todo: investigate SDL_WINDOW_ALLOW_HIGHDPI here

        default:
            spdlog::error("Unsupported GraphicsAPI type.");
            throw;
        }
    }

    GraphicsDevice::GraphicsDevice(GraphicsAPI api, void *windowHandle, VerticalSyncType verticalSyncType)
    {
        assert(windowHandle != nullptr);
        assert(api == GraphicsAPI::OpenGL);

        SDL_Window *sdlWindow = (SDL_Window *)windowHandle;
        glContext = SDL_GL_CreateContext(sdlWindow);
        SDL_GL_MakeCurrent(sdlWindow, glContext);

        switch (verticalSyncType)
        {
        case VerticalSyncType::AdaptiveEnabled:
            if (!SDL_GL_SetSwapInterval(-1))
            {
                SDL_GL_SetSwapInterval(1);
            }
            break;

        case VerticalSyncType::Enabled:
            SDL_GL_SetSwapInterval(1);
            break;

        case VerticalSyncType::Disabled:
            SDL_GL_SetSwapInterval(0);
            break;
        }

        // todo: move Windows specific code to somewhere else?
#if defined(PLATFORM_WINDOWS)
        if (!gladLoadGL((GLADloadfunc)SDL_GL_GetProcAddress))
        {
            spdlog::error("Failed to load OpenGL");
            throw;
        }
#endif

        glEnable(GL_BLEND);
        glDisable(GL_CULL_FACE);

        SDL_GetWindowSizeInPixels(sdlWindow, &screenWidth, &screenHeight);
        glViewport(0, 0, screenWidth, screenHeight);
        viewport = {0, 0, screenWidth, screenHeight};

        glClearColor(0, 0, 0, 1);
        clearColor = Color::Black;

        blendMode = (BlendMode)-1;
        SetBlendMode(BlendMode::PremultipliedAlpha);

        scissorsEnabled = false;

        glGetIntegerv(GL_FRAMEBUFFER_BINDING, (int32_t *)&defaultFramebufferObject);
        currentFramebufferObject = defaultFramebufferObject;

        drawCallsThisFrame = 0;

        spdlog::info("GL: OpenGL device information:");
        spdlog::info("  Vendor:   {}", (const char *)glGetString(GL_VENDOR));
        spdlog::info("  Renderer: {}", (const char *)glGetString(GL_RENDERER));
        spdlog::info("  Version:  {}", (const char *)glGetString(GL_VERSION));
        spdlog::info("  GLSL:     {}", (const char *)glGetString(GL_SHADING_LANGUAGE_VERSION));
    }

    GraphicsDevice::~GraphicsDevice()
    {
        SDL_GL_DestroyContext(glContext);
    }

    void GraphicsDevice::SetViewport(const Rectangle &vp)
    {
        viewport = vp;
        glViewport(viewport.x, viewport.y, viewport.width, viewport.height);
    }

    void GraphicsDevice::GetViewport(Rectangle &vp) const
    {
        vp = viewport;
    }

    void GraphicsDevice::ClearScreen(const Color &color)
    {
        if (scissorsEnabled)
        {
            glDisable(GL_SCISSOR_TEST);
        }

        if (color != clearColor)
        {
            glClearColor(color.r, color.g, color.b, color.a);
            clearColor = color;
        }

        glClear(GL_COLOR_BUFFER_BIT);

        if (scissorsEnabled)
        {
            glEnable(GL_SCISSOR_TEST);
        }
    }

    void GraphicsDevice::SetBlendMode(BlendMode mode)
    {
        if (blendMode == mode)
        {
            return;
        }

        switch (mode)
        {
        case BlendMode::None:
            glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
            glBlendFuncSeparate(GL_ONE, GL_ZERO, GL_ONE, GL_ZERO);
            break;
        case BlendMode::Additive:
            glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
            glBlendFuncSeparate(GL_ONE, GL_ONE, GL_ONE, GL_ZERO);
            break;
        case BlendMode::Alpha:
            glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
            glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ZERO);
            break;
        case BlendMode::PremultipliedAlpha:
            glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
            glBlendFuncSeparate(GL_ONE, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ZERO);
            break;
        default:
            spdlog::error("Unsupported BlendMode type.");
            throw;
        }

        blendMode = mode;
    }

    void GraphicsDevice::EnableScissorsRectangle(const Rectangle &scissorsRect)
    {
        scissorsEnabled = true;
        scissorsRectangle = scissorsRect;

        if (!IsUsingRenderTarget())
        {
            scissorsRectangle.y = viewport.height - scissorsRectangle.y - scissorsRectangle.height;
        }

        glEnable(GL_SCISSOR_TEST);
        glScissor(scissorsRectangle.x, scissorsRectangle.y, scissorsRectangle.width, scissorsRectangle.height);
    }

    void GraphicsDevice::DisableScissorsRectangle()
    {
        glDisable(GL_SCISSOR_TEST);
        scissorsEnabled = false;
    }

    void GraphicsDevice::BindTexture(const Texture &texture)
    {
        glBindTexture(GL_TEXTURE_2D, texture.GetTextureId());
    }

    void GraphicsDevice::BindRenderTarget(const Texture &texture, bool setViewport)
    {
        assert(texture.GetTextureType() == TextureType::RenderTarget);

        currentFramebufferObject = texture.GetFramebufferId();
        glBindFramebuffer(GL_FRAMEBUFFER, currentFramebufferObject);

        if (setViewport)
        {
            SetViewport({0, 0, static_cast<int>(texture.GetWidth()), static_cast<int>(texture.GetHeight())});
        }
    }

    void GraphicsDevice::UnbindRenderTarget(bool resetViewport)
    {
        currentFramebufferObject = defaultFramebufferObject;
        glBindFramebuffer(GL_FRAMEBUFFER, defaultFramebufferObject);

        if (resetViewport)
        {
            SetViewport({0, 0, screenWidth, screenHeight});
        }
    }

    bool GraphicsDevice::IsUsingRenderTarget() const
    {
        return defaultFramebufferObject != currentFramebufferObject;
    }

    void GraphicsDevice::ReadPixels(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint8_t *pixels)
    {
        glReadPixels(x, y, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    }

    void GraphicsDevice::ApplyShaderProgram(const ShaderProgram &shaderProgram)
    {
        glUseProgram(shaderProgram.GetShaderId());
    }

    void GraphicsDevice::BeginFrame()
    {
        drawCallsThisFrame = 0;
    }

    void GraphicsDevice::EndFrame()
    {
    }

    void GraphicsDevice::DrawPrimitives(
        const VertexBuffer &vertexBuffer, PrimitiveType primitiveType, uint32_t vertexStart, uint32_t primitiveCount)
    {
        glBindVertexArray(vertexBuffer.GetArrayId());
        glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer.GetBufferId());

        int vertexCount;
        GLenum mode;

        switch (primitiveType)
        {
        case PrimitiveType::Triangles:
            vertexCount = primitiveCount * 3;
            mode = GL_TRIANGLES;
            break;
        case PrimitiveType::TriangleStrip:
            vertexCount = primitiveCount + 2;
            mode = GL_TRIANGLE_STRIP;
            break;
        case PrimitiveType::Lines:
            vertexCount = primitiveCount * 2;
            mode = GL_LINES;
            break;
        case PrimitiveType::LineStrip:
            vertexCount = primitiveCount + 1;
            mode = GL_LINE_STRIP;
            break;
        case PrimitiveType::Points:
            vertexCount = primitiveCount;
            mode = GL_POINTS;
            break;
        default:
            spdlog::error("Unsupported PrimitiveType.");
            throw;
        }

        glDrawArrays(mode, vertexStart, vertexCount);
        drawCallsThisFrame++;
    }
} // namespace Lucky
