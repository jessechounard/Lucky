#pragma once

#include <map>
#include <memory>
#include <stdint.h>
#include <vector>

#include <glm/glm.hpp>
#include <Lucky/GraphicsDevice.hpp>
#include <Lucky/Shader.hpp>
#include <Lucky/Types.hpp>
#include <Lucky/VertexBuffer.hpp>
#include <Lucky/Rectangle.hpp>
#include <Lucky/Vertex.hpp>

namespace Lucky {

struct BatchRenderer;
struct Color;

inline constexpr UVMode operator&(UVMode lhs, UVMode rhs) {
    return static_cast<UVMode>(static_cast<uint32_t>(lhs) & static_cast<uint32_t>(rhs));
}

inline UVMode &operator&=(UVMode &lhs, UVMode rhs) {
    lhs = lhs & rhs;
    return lhs;
}

inline constexpr UVMode operator|(UVMode lhs, UVMode rhs) {
    return static_cast<UVMode>(static_cast<uint32_t>(lhs) | static_cast<uint32_t>(rhs));
}

inline UVMode &operator|=(UVMode &lhs, UVMode rhs) {
    lhs = lhs | rhs;
    return lhs;
}

inline bool HasFlag(UVMode t, UVMode flag) {
    return (static_cast<uint32_t>(t) & static_cast<uint32_t>(flag)) != 0;
}

struct BatchRenderer {
  public:
    BatchRenderer(GraphicsDevice &graphicsDevice, uint32_t maximumTriangles);
    BatchRenderer(const BatchRenderer &) = delete;
    ~BatchRenderer();

    BatchRenderer &operator=(const BatchRenderer &) = delete;
    BatchRenderer &operator=(const BatchRenderer &&) = delete;

    bool BatchStarted() const {
        return batchStarted;
    }

    void Begin(BlendMode blendMode, const glm::mat4 &transformMatrix = glm::mat4(1.0f));
    void Begin(
        BlendMode blendMode, Texture &texture, const glm::mat4 &transformMatrix = glm::mat4(1.0f));
    void Begin(BlendMode blendMode, Texture &texture, Shader &fragmentShader,
        const glm::mat4 &transformMatrix = glm::mat4(1.0f));
    void Begin(BlendMode blendMode, Shader &fragmentShader,
        const glm::mat4 &transformMatrix = glm::mat4(1.0f));
    void End();

    void BatchQuadUV(const glm::vec2 &uv0, const glm::vec2 &uv1, const glm::vec2 &xy0,
        const glm::vec2 &xy1, const Color &color, float rotation = 0.0f);

    void BatchQuad(const Rectangle *sourceRectangle, const glm::vec2 &position,
        const float rotation, const glm::vec2 &scale, const glm::vec2 &origin, const UVMode uvMode,
        const Color &color);

    void BatchQuad(const glm::vec2 corners[4], const glm::vec2 uvs[4], const Color &color);

    void BatchTriangles(Vertex *triangleVertices, const int triangleCount);

    void SetFragmentUniformData(const void *data, uint32_t size, uint32_t slot = 0);

  private:
    void Flush();
    SDL_GPUGraphicsPipeline *GetOrCreatePipeline(
        BlendMode blendMode, SDL_GPUTextureFormat targetFormat, SDL_GPUShader *fragShader);

    GraphicsDevice *graphicsDevice;
    std::unique_ptr<Texture> whitePixelTexture;
    Texture *texture;
    std::unique_ptr<Shader> vertexShader;
    std::unique_ptr<Shader> fragmentShader;
    Shader *activeFragmentShader;
    std::unique_ptr<VertexBuffer<Vertex>> vertexBuffer;
    glm::mat4 transformMatrix;
    BlendMode blendMode;

    uint32_t activeVertices;
    uint32_t maximumVertices;

    std::vector<Vertex> vertices;

    bool batchStarted;

    const void *fragmentUniformData = nullptr;
    uint32_t fragmentUniformSize = 0;
    uint32_t fragmentUniformSlot = 0;

    struct PipelineKey {
        BlendMode blendMode;
        SDL_GPUTextureFormat targetFormat;
        SDL_GPUShader *fragShader;

        bool operator<(const PipelineKey &other) const {
            if (blendMode != other.blendMode)
                return blendMode < other.blendMode;
            if (targetFormat != other.targetFormat)
                return targetFormat < other.targetFormat;
            return fragShader < other.fragShader;
        }
    };
    std::map<PipelineKey, SDL_GPUGraphicsPipeline *> pipelines;
};

} // namespace Lucky
