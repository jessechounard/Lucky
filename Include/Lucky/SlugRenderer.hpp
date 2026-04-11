#pragma once

#include <memory>
#include <stdint.h>
#include <vector>

#include <glm/glm.hpp>

#include <Lucky/Shader.hpp>
#include <Lucky/VertexBuffer.hpp>

struct SDL_GPUGraphicsPipeline;
struct SDL_GPUBuffer;

namespace Lucky {

struct GraphicsDevice;
class SlugFont;
enum class BlendMode;

struct SlugVertex {
    float posX, posY;         // object-space position
    float normX, normY;       // outward normal for dilation     (float4 @ location 0)
    float emU, emV;           // em-space texture coordinates
    float invJac00, invJac01; // inverse Jacobian row 0          (float4 @ location 1)
    float invJac10, invJac11; // inverse Jacobian row 1
    float glyphLocF;          // atlas offset (bitcast to uint in shader)
    float _pad0;              //                                 (float4 @ location 2)
    float r, g, b, a;         // color                           (float4 @ location 3)
}; // 64 bytes

struct SlugRenderer {
    SlugRenderer(GraphicsDevice &graphicsDevice, uint32_t maxTriangles);
    ~SlugRenderer();

    SlugRenderer(const SlugRenderer &) = delete;
    SlugRenderer &operator=(const SlugRenderer &) = delete;

    void Begin(
        BlendMode blendMode, const SlugFont &font, const glm::mat4 &mvpMatrix = glm::mat4(1.0f));
    void End();

    void UpdateAtlas(const SlugFont &font);
    void BatchGlyphQuad(const SlugVertex verts[6]);

  private:
    void Flush();
    void CreatePipeline(BlendMode blendMode);

    GraphicsDevice *graphicsDevice;

    std::unique_ptr<Shader> vertexShader;
    std::unique_ptr<Shader> fragmentShader;
    SDL_GPUGraphicsPipeline *pipeline = nullptr;
    BlendMode currentBlendMode{};

    std::unique_ptr<VertexBuffer<SlugVertex>> vertexBuffer;

    std::vector<SlugVertex> vertices;
    uint32_t activeVertices = 0;
    uint32_t maxVertices;

    // State for current batch
    SDL_GPUBuffer *atlasBuffer = nullptr;
    glm::mat4 mvpMatrix;
    bool batchStarted = false;
};

} // namespace Lucky
