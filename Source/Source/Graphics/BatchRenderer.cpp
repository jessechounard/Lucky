#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <Lucky/Graphics/BatchRenderer.hpp>
#include <Lucky/Graphics/Color.hpp>
#include <Lucky/Graphics/GraphicsDevice.hpp>
#include <Lucky/Graphics/ShaderProgram.hpp>
#include <Lucky/Graphics/Texture.hpp>
#include <Lucky/Math/Rectangle.hpp>
#include <Lucky/Math/Vertex.hpp>

#include "IncludeOpenGL.h"

using namespace glm;

namespace Lucky
{
    constexpr char defaultVertexShaderSource[] =
        // input from CPU
        "attribute vec4 position;\n"
        "attribute vec4 color;\n"
        "attribute vec2 texcoord;\n"
        // output to fragment shader
        "varying vec4 v_color;\n"
        "varying vec2 v_texcoord;\n"
        // custom input from program
        "uniform mat4 ProjectionMatrix;\n"
        //
        "void main()\n"
        "{\n"
        "	gl_Position = ProjectionMatrix * position;\n"
        "	v_color = color;\n"
        "	v_texcoord = texcoord;\n"
        "}\n";

    constexpr char defaultFragmentShaderSource[] =
        // input from vertex shader
        "varying vec4 v_color;\n"
        "varying vec2 v_texcoord;\n"
        // custom input from program
        "uniform sampler2D TextureSampler;\n"
        //
        "void main()\n"
        "{\n"
        "	gl_FragColor = texture2D(TextureSampler, v_texcoord) * v_color;\n"
        "}\n";

    BatchRenderer::BatchRenderer(std::shared_ptr<GraphicsDevice> graphicsDevice, uint32_t maximumTriangles)
        : graphicsDevice(graphicsDevice)
    {
        assert(maximumTriangles > 0);

        maximumVertices = maximumTriangles * 3;
        batchStarted = false;

        defaultVertexShader = std::make_unique<VertexShader>(
            (uint8_t *)defaultVertexShaderSource, (int)strlen(defaultVertexShaderSource));
        defaultFragmentShader = std::make_unique<FragmentShader>(
            (uint8_t *)defaultFragmentShaderSource, (int)strlen(defaultFragmentShaderSource));
        defaultShaderProgram =
            std::make_shared<ShaderProgram>(graphicsDevice, *defaultVertexShader, *defaultFragmentShader);
        vertexBuffer = std::make_unique<VertexBuffer>(VertexBufferType::Dynamic, maximumVertices);

        vertices.resize(maximumVertices);
    }

    BatchRenderer::~BatchRenderer()
    {
    }

    void BatchRenderer::Begin(BlendMode blendMode, std::shared_ptr<Texture> texture,
        std::shared_ptr<ShaderProgram> shaderProgram, const glm::mat4 &transformMatrix)
    {
        if (batchStarted)
        {
            // todo:
            throw;
        }

        activeVertices = 0;
        batchStarted = true;
        this->blendMode = blendMode;
        this->texture = texture;
        this->currentShaderProgram = (shaderProgram != nullptr) ? shaderProgram : defaultShaderProgram;
        this->transformMatrix = transformMatrix;
    }

    void BatchRenderer::End()
    {
        if (!batchStarted)
        {
            // todo:
            throw;
        }

        Flush();

        currentShaderProgram.reset();
        texture.reset();
        batchStarted = false;
    }

    void BatchRenderer::BatchQuadUV(
        const glm::vec2 &uv0, const glm::vec2 &uv1, const glm::vec2 &xy0, const glm::vec2 &xy1, const Color &color)
    {
        // todo: check batchStarted

        if (activeVertices + 6 > maximumVertices)
        {
            Flush();
        }

        std::pair<float, float> uvs[4];
        uvs[0].first = uv0.x;
        uvs[0].second = uv0.y;
        uvs[1].first = uv1.x;
        uvs[1].second = uv0.y;
        uvs[2].first = uv1.x;
        uvs[2].second = uv1.y;
        uvs[3].first = uv0.x;
        uvs[3].second = uv1.y;

        Vertex *vertices = &this->vertices[0] + activeVertices;
        vertices->x = xy0.x;
        vertices->y = xy0.y;
        vertices->u = uvs[0].first;
        vertices->v = uvs[0].second;
        vertices->r = color.r;
        vertices->g = color.g;
        vertices->b = color.b;
        vertices->a = color.a;
        vertices++;

        vertices->x = xy1.x;
        vertices->y = xy0.y;
        vertices->u = uvs[1].first;
        vertices->v = uvs[1].second;
        vertices->r = color.r;
        vertices->g = color.g;
        vertices->b = color.b;
        vertices->a = color.a;
        vertices++;

        vertices->x = xy1.x;
        vertices->y = xy1.y;
        vertices->u = uvs[2].first;
        vertices->v = uvs[2].second;
        vertices->r = color.r;
        vertices->g = color.g;
        vertices->b = color.b;
        vertices->a = color.a;
        vertices++;

        *vertices = *(vertices - 3);
        vertices++;
        *vertices = *(vertices - 2);
        vertices++;

        vertices->x = xy0.x;
        vertices->y = xy1.y;
        vertices->u = uvs[3].first;
        vertices->v = uvs[3].second;
        vertices->r = color.r;
        vertices->g = color.g;
        vertices->b = color.b;
        vertices->a = color.a;

        activeVertices += 6;
    }

    void BatchRenderer::BatchQuad(const Rectangle *sourceRectangle, const glm::vec2 &position, const float rotation,
        const glm::vec2 &scale, const glm::vec2 &origin, const UVMode uvMode, const Color &color)
    {
        // todo: check batchStarted
        // todo: check for null texture

        if (activeVertices + 6 > maximumVertices)
        {
            Flush();
        }

        float destX = position.x;
        float destY = position.y;
        float destW = scale.x;
        float destH = scale.y;
        int textureW = texture->GetWidth();
        int textureH = texture->GetHeight();
        Rectangle source;

        if (sourceRectangle != nullptr)
        {
            source = *sourceRectangle;
        }
        else
        {
            source = {0, 0, textureW, textureH};
        }

        if (sourceRectangle != nullptr)
        {
            destW *= sourceRectangle->width;
            destH *= sourceRectangle->height;
        }
        else
        {
            destW *= textureW;
            destH *= textureH;
        }

        std::pair<float, float> uvs[4];
        if (HasFlag(uvMode, UVMode::RotatedCW90))
        {
            uvs[0].first = (source.x + source.height) / (float)textureW;
            uvs[0].second = source.y / (float)textureH;
            uvs[1].first = (source.x + source.height) / (float)textureW;
            uvs[1].second = (source.y + source.width) / (float)textureH;
            uvs[2].first = source.x / (float)textureW;
            uvs[2].second = (source.y + source.width) / (float)textureH;
            uvs[3].first = source.x / (float)textureW;
            uvs[3].second = source.y / (float)textureH;
        }
        else
        {
            uvs[0].first = source.x / (float)textureW;
            uvs[0].second = source.y / (float)textureH;
            uvs[1].first = (source.x + source.width) / (float)textureW;
            uvs[1].second = source.y / (float)textureH;
            uvs[2].first = (source.x + source.width) / (float)textureW;
            uvs[2].second = (source.y + source.height) / (float)textureH;
            uvs[3].first = source.x / (float)textureW;
            uvs[3].second = (source.y + source.height) / (float)textureH;
        }

        if (HasFlag(uvMode, UVMode::FlipHorizontal))
        {
            std::swap(uvs[0], uvs[1]);
            std::swap(uvs[3], uvs[2]);
        }

        if (HasFlag(uvMode, UVMode::FlipVertical))
        {
            std::swap(uvs[0], uvs[3]);
            std::swap(uvs[1], uvs[2]);
        }

        float rotationSin = sin(rotation);
        float rotationCos = cos(rotation);

        float cornerX = -origin.x * destW;
        float cornerY = -origin.y * destH;
        Vertex *vertices = &this->vertices[0] + activeVertices;
        vertices->x = cornerX * rotationCos - cornerY * rotationSin + destX;
        vertices->y = cornerX * rotationSin + cornerY * rotationCos + destY;
        vertices->u = uvs[0].first;
        vertices->v = uvs[0].second;
        vertices->r = color.r;
        vertices->g = color.g;
        vertices->b = color.b;
        vertices->a = color.a;
        vertices++;

        cornerX = (1.0f - origin.x) * destW;
        cornerY = -origin.y * destH;
        vertices->x = cornerX * rotationCos - cornerY * rotationSin + destX;
        vertices->y = cornerX * rotationSin + cornerY * rotationCos + destY;
        vertices->u = uvs[1].first;
        vertices->v = uvs[1].second;
        vertices->r = color.r;
        vertices->g = color.g;
        vertices->b = color.b;
        vertices->a = color.a;
        vertices++;

        cornerX = (1.0f - origin.x) * destW;
        cornerY = (1.0f - origin.y) * destH;
        vertices->x = cornerX * rotationCos - cornerY * rotationSin + destX;
        vertices->y = cornerX * rotationSin + cornerY * rotationCos + destY;
        vertices->u = uvs[2].first;
        vertices->v = uvs[2].second;
        vertices->r = color.r;
        vertices->g = color.g;
        vertices->b = color.b;
        vertices->a = color.a;
        vertices++;

        *vertices = *(vertices - 3);
        vertices++;
        *vertices = *(vertices - 2);
        vertices++;

        cornerX = -origin.x * destW;
        cornerY = (1.0f - origin.y) * destH;
        vertices->x = cornerX * rotationCos - cornerY * rotationSin + destX;
        vertices->y = cornerX * rotationSin + cornerY * rotationCos + destY;
        vertices->u = uvs[3].first;
        vertices->v = uvs[3].second;
        vertices->r = color.r;
        vertices->g = color.g;
        vertices->b = color.b;
        vertices->a = color.a;

        activeVertices += 6;
    }

    void BatchRenderer::BatchTriangles(Vertex *triangleVertices, const int triangleCount)
    {
        assert(triangleVertices != nullptr);
        assert(triangleCount > 0);

        // todo: check batchStarted

        Vertex *currentTriangleVertex = triangleVertices;

        for (int index = 0; index < triangleCount * 3; index += 3)
        {
            if (activeVertices + 3 > maximumVertices)
            {
                Flush();
            }

            Vertex *vertex = &vertices[activeVertices];
            *vertex++ = *currentTriangleVertex++;
            *vertex++ = *currentTriangleVertex++;
            *vertex++ = *currentTriangleVertex++;
            activeVertices += 3;
        }
    }

    void BatchRenderer::Flush()
    {
        assert(activeVertices > 0);
        assert(activeVertices % 3 == 0);

        Rectangle viewport;
        graphicsDevice->GetViewport(viewport);

        // todo: can we optimize this creation and multiplication?
        glm::mat4 projectionMatrix;
        if (graphicsDevice->IsUsingRenderTarget())
        {
            projectionMatrix = glm::ortho<float>((float)viewport.x, (float)(viewport.x + viewport.width),
                (float)viewport.y, (float)(viewport.y + viewport.height));
        }
        else
        {
            projectionMatrix = glm::ortho<float>((float)viewport.x, (float)(viewport.x + viewport.width),
                (float)(viewport.y + viewport.height), (float)viewport.y);
        }
        projectionMatrix = projectionMatrix * transformMatrix;

        graphicsDevice->SetBlendMode(blendMode);
        graphicsDevice->ApplyShaderProgram(*currentShaderProgram);

        auto textureSamplerLocation = currentShaderProgram->GetParameterLocation("TextureSampler");
        if (textureSamplerLocation != -1 && texture)
        {
            currentShaderProgram->SetParameter("TextureSampler", *texture, 0);
        }

        auto projectionMatrixLocation = currentShaderProgram->GetParameterLocation("ProjectionMatrix");
        if (projectionMatrixLocation != -1)
        {
            currentShaderProgram->SetParameter("ProjectionMatrix", projectionMatrix);
        }

        currentShaderProgram->ApplyParameters();

        vertexBuffer->SetVertexData(*currentShaderProgram, &vertices[0], activeVertices);
        graphicsDevice->DrawPrimitives(*vertexBuffer, PrimitiveType::Triangles, 0, activeVertices / 3);

        activeVertices = 0;
    }
} // namespace Lucky
