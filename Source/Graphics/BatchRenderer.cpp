#include <filesystem>

#include <SDL3/SDL_assert.h>
#include <SDL3/SDL_filesystem.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <Lucky/BatchRenderer.hpp>
#include <Lucky/BlendState.hpp>
#include <Lucky/Color.hpp>
#include <Lucky/GraphicsDevice.hpp>
#include <Lucky/Shader.hpp>
#include <Lucky/Texture.hpp>
#include <Lucky/Rectangle.hpp>
#include <Lucky/Vertex.hpp>
#include <spdlog/spdlog.h>

using namespace glm;

namespace Lucky {

BatchRenderer::BatchRenderer(GraphicsDevice &graphicsDevice, uint32_t maximumTriangles)
    : graphicsDevice(&graphicsDevice) {
    SDL_assert(maximumTriangles > 0);

    maximumVertices = maximumTriangles * 3;
    batchStarted = false;

    std::filesystem::path basePath = SDL_GetBasePath();
    vertexShader = std::make_unique<Shader>(graphicsDevice,
        (basePath / "Content/Shaders/sprite.vert").generic_string(),
        SDL_GPU_SHADERSTAGE_VERTEX);
    fragmentShader = std::make_unique<Shader>(graphicsDevice,
        (basePath / "Content/Shaders/sprite.frag").generic_string(),
        SDL_GPU_SHADERSTAGE_FRAGMENT);

    activeFragmentShader = fragmentShader.get();

    vertexBuffer = std::make_unique<VertexBuffer<Vertex>>(
        graphicsDevice, VertexBufferType::Dynamic, maximumVertices);

    vertices.resize(maximumVertices);

    uint8_t whitePixel[] = {255, 255, 255, 255};
    whitePixelTexture = std::make_unique<Texture>(graphicsDevice,
        TextureType::Default,
        1,
        1,
        whitePixel,
        static_cast<uint32_t>(sizeof(whitePixel)));
}

BatchRenderer::~BatchRenderer() {
    SDL_GPUDevice *device = graphicsDevice->GetDevice();
    for (auto &[key, pipeline] : pipelines) {
        SDL_ReleaseGPUGraphicsPipeline(device, pipeline);
    }
}

void BatchRenderer::Begin(BlendMode blendMode, const glm::mat4 &transformMatrix) {
    Begin(blendMode, *whitePixelTexture, transformMatrix);
}

void BatchRenderer::Begin(BlendMode blendMode, Texture &texture, const glm::mat4 &transformMatrix) {
    SDL_assert(!batchStarted);

    activeVertices = 0;
    batchStarted = true;
    this->blendMode = blendMode;
    this->texture = &texture;
    this->activeFragmentShader = fragmentShader.get();
    this->transformMatrix = transformMatrix;
    fragmentUniformData = nullptr;
    fragmentUniformSize = 0;
    fragmentUniformSlot = 0;
}

void BatchRenderer::Begin(BlendMode blendMode, Texture &texture, Shader &fragmentShader,
    const glm::mat4 &transformMatrix) {
    SDL_assert(!batchStarted);

    activeVertices = 0;
    batchStarted = true;
    this->blendMode = blendMode;
    this->texture = &texture;
    this->activeFragmentShader = &fragmentShader;
    this->transformMatrix = transformMatrix;
    fragmentUniformData = nullptr;
    fragmentUniformSize = 0;
    fragmentUniformSlot = 0;
}

void BatchRenderer::Begin(
    BlendMode blendMode, Shader &fragmentShader, const glm::mat4 &transformMatrix) {
    SDL_assert(!batchStarted);

    activeVertices = 0;
    batchStarted = true;
    this->blendMode = blendMode;
    this->texture = nullptr;
    this->activeFragmentShader = &fragmentShader;
    this->transformMatrix = transformMatrix;
    fragmentUniformData = nullptr;
    fragmentUniformSize = 0;
    fragmentUniformSlot = 0;
}

void BatchRenderer::End() {
    SDL_assert(batchStarted);

    if (activeVertices > 0) {
        Flush();
    }

    texture = nullptr;
    activeFragmentShader = fragmentShader.get();
    fragmentUniformData = nullptr;
    fragmentUniformSize = 0;
    fragmentUniformSlot = 0;
    batchStarted = false;
}

void BatchRenderer::BatchQuadUV(const glm::vec2 &uv0, const glm::vec2 &uv1, const glm::vec2 &xy0,
    const glm::vec2 &xy1, const Color &color, float rotation) {
    SDL_assert(batchStarted);

    if (activeVertices + 6 > maximumVertices) {
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

    float cx0 = xy0.x, cy0 = xy0.y;
    float cx1 = xy1.x, cy1 = xy0.y;
    float cx2 = xy1.x, cy2 = xy1.y;
    float cx3 = xy0.x, cy3 = xy1.y;

    if (rotation != 0.0f) {
        float centerX = (xy0.x + xy1.x) * 0.5f;
        float centerY = (xy0.y + xy1.y) * 0.5f;
        float cosR = cos(rotation);
        float sinR = sin(rotation);

        auto rotate = [&](float &px, float &py) {
            float dx = px - centerX;
            float dy = py - centerY;
            px = dx * cosR - dy * sinR + centerX;
            py = dx * sinR + dy * cosR + centerY;
        };

        rotate(cx0, cy0);
        rotate(cx1, cy1);
        rotate(cx2, cy2);
        rotate(cx3, cy3);
    }

    Vertex *vertices = &this->vertices[0] + activeVertices;
    vertices->x = cx0;
    vertices->y = cy0;
    vertices->u = uvs[0].first;
    vertices->v = uvs[0].second;
    vertices->r = color.r;
    vertices->g = color.g;
    vertices->b = color.b;
    vertices->a = color.a;
    vertices++;

    vertices->x = cx1;
    vertices->y = cy1;
    vertices->u = uvs[1].first;
    vertices->v = uvs[1].second;
    vertices->r = color.r;
    vertices->g = color.g;
    vertices->b = color.b;
    vertices->a = color.a;
    vertices++;

    vertices->x = cx2;
    vertices->y = cy2;
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

    vertices->x = cx3;
    vertices->y = cy3;
    vertices->u = uvs[3].first;
    vertices->v = uvs[3].second;
    vertices->r = color.r;
    vertices->g = color.g;
    vertices->b = color.b;
    vertices->a = color.a;

    activeVertices += 6;
}

void BatchRenderer::BatchSprite(const Rectangle *sourceRectangle, const glm::vec2 &position,
    const float rotation, const glm::vec2 &scale, const glm::vec2 &origin, const UVMode uvMode,
    const Color &color) {
    SDL_assert(batchStarted);
    SDL_assert(texture != nullptr);

    if (activeVertices + 6 > maximumVertices) {
        Flush();
    }

    float destX = position.x;
    float destY = position.y;
    int textureW = texture->GetWidth();
    int textureH = texture->GetHeight();

    Rectangle source =
        (sourceRectangle != nullptr) ? *sourceRectangle : Rectangle{0, 0, textureW, textureH};

    float destW = scale.x * source.width;
    float destH = scale.y * source.height;

    std::pair<float, float> uvs[4];
    if (HasFlag(uvMode, UVMode::RotatedCW90)) {
        uvs[0].first = (source.x + source.height) / (float)textureW;
        uvs[0].second = source.y / (float)textureH;
        uvs[1].first = (source.x + source.height) / (float)textureW;
        uvs[1].second = (source.y + source.width) / (float)textureH;
        uvs[2].first = source.x / (float)textureW;
        uvs[2].second = (source.y + source.width) / (float)textureH;
        uvs[3].first = source.x / (float)textureW;
        uvs[3].second = source.y / (float)textureH;
    } else {
        uvs[0].first = source.x / (float)textureW;
        uvs[0].second = source.y / (float)textureH;
        uvs[1].first = (source.x + source.width) / (float)textureW;
        uvs[1].second = source.y / (float)textureH;
        uvs[2].first = (source.x + source.width) / (float)textureW;
        uvs[2].second = (source.y + source.height) / (float)textureH;
        uvs[3].first = source.x / (float)textureW;
        uvs[3].second = (source.y + source.height) / (float)textureH;
    }

    if (HasFlag(uvMode, UVMode::FlipHorizontal)) {
        std::swap(uvs[0], uvs[1]);
        std::swap(uvs[3], uvs[2]);
    }

    if (HasFlag(uvMode, UVMode::FlipVertical)) {
        std::swap(uvs[0], uvs[3]);
        std::swap(uvs[1], uvs[2]);
    }

    // Y-up pixel coords vs Y-down texture rows — see Orientation in
    // BatchQuad's header Doxygen.
    std::swap(uvs[0], uvs[3]);
    std::swap(uvs[1], uvs[2]);

    Vertex *vertices = &this->vertices[0] + activeVertices;

    if (rotation == 0.0f) {
        float left = -origin.x * destW + destX;
        float top = -origin.y * destH + destY;
        float right = (1.0f - origin.x) * destW + destX;
        float bottom = (1.0f - origin.y) * destH + destY;

        vertices->x = left;
        vertices->y = top;
        vertices->u = uvs[0].first;
        vertices->v = uvs[0].second;
        vertices->r = color.r;
        vertices->g = color.g;
        vertices->b = color.b;
        vertices->a = color.a;
        vertices++;

        vertices->x = right;
        vertices->y = top;
        vertices->u = uvs[1].first;
        vertices->v = uvs[1].second;
        vertices->r = color.r;
        vertices->g = color.g;
        vertices->b = color.b;
        vertices->a = color.a;
        vertices++;

        vertices->x = right;
        vertices->y = bottom;
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

        vertices->x = left;
        vertices->y = bottom;
        vertices->u = uvs[3].first;
        vertices->v = uvs[3].second;
        vertices->r = color.r;
        vertices->g = color.g;
        vertices->b = color.b;
        vertices->a = color.a;
    } else {
        float rotationSin = sin(rotation);
        float rotationCos = cos(rotation);

        float cornerX = -origin.x * destW;
        float cornerY = -origin.y * destH;
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
    }

    activeVertices += 6;
}

void BatchRenderer::BatchQuad(
    const glm::vec2 corners[4], const glm::vec2 uvs[4], const Color &color) {
    SDL_assert(batchStarted);

    if (activeVertices + 6 > maximumVertices) {
        Flush();
    }

    Vertex *vertices = &this->vertices[0] + activeVertices;

    vertices->x = corners[0].x;
    vertices->y = corners[0].y;
    vertices->u = uvs[0].x;
    vertices->v = uvs[0].y;
    vertices->r = color.r;
    vertices->g = color.g;
    vertices->b = color.b;
    vertices->a = color.a;
    vertices++;

    vertices->x = corners[1].x;
    vertices->y = corners[1].y;
    vertices->u = uvs[1].x;
    vertices->v = uvs[1].y;
    vertices->r = color.r;
    vertices->g = color.g;
    vertices->b = color.b;
    vertices->a = color.a;
    vertices++;

    vertices->x = corners[2].x;
    vertices->y = corners[2].y;
    vertices->u = uvs[2].x;
    vertices->v = uvs[2].y;
    vertices->r = color.r;
    vertices->g = color.g;
    vertices->b = color.b;
    vertices->a = color.a;
    vertices++;

    *vertices = *(vertices - 3);
    vertices++;
    *vertices = *(vertices - 2);
    vertices++;

    vertices->x = corners[3].x;
    vertices->y = corners[3].y;
    vertices->u = uvs[3].x;
    vertices->v = uvs[3].y;
    vertices->r = color.r;
    vertices->g = color.g;
    vertices->b = color.b;
    vertices->a = color.a;

    activeVertices += 6;
}

void BatchRenderer::BatchTriangles(const Vertex *triangleVertices, const int triangleCount) {
    SDL_assert(batchStarted);
    SDL_assert(triangleVertices != nullptr);
    SDL_assert(triangleCount > 0);

    const Vertex *currentTriangleVertex = triangleVertices;

    for (int index = 0; index < triangleCount * 3; index += 3) {
        if (activeVertices + 3 > maximumVertices) {
            Flush();
        }

        Vertex *vertex = &vertices[activeVertices];
        *vertex++ = *currentTriangleVertex++;
        *vertex++ = *currentTriangleVertex++;
        *vertex++ = *currentTriangleVertex++;
        activeVertices += 3;
    }
}

SDL_GPUGraphicsPipeline *BatchRenderer::GetOrCreatePipeline(
    BlendMode blendMode, SDL_GPUTextureFormat targetFormat, SDL_GPUShader *fragShader) {

    PipelineKey key{blendMode, targetFormat, fragShader};
    auto it = pipelines.find(key);
    if (it != pipelines.end()) {
        return it->second;
    }

    SDL_GPUDevice *device = graphicsDevice->GetDevice();

    SDL_GPUGraphicsPipelineCreateInfo pipelineCI;
    SDL_zero(pipelineCI);

    SDL_GPUVertexBufferDescription vbufDesc;
    SDL_zero(vbufDesc);
    vbufDesc.slot = 0;
    vbufDesc.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
    vbufDesc.pitch = sizeof(Vertex);

    SDL_GPUVertexAttribute vertexAttrs[3];
    SDL_zero(vertexAttrs);

    vertexAttrs[0].buffer_slot = 0;
    vertexAttrs[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
    vertexAttrs[0].location = 0;
    vertexAttrs[0].offset = offsetof(Vertex, x);

    vertexAttrs[1].buffer_slot = 0;
    vertexAttrs[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
    vertexAttrs[1].location = 1;
    vertexAttrs[1].offset = offsetof(Vertex, u);

    vertexAttrs[2].buffer_slot = 0;
    vertexAttrs[2].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
    vertexAttrs[2].location = 2;
    vertexAttrs[2].offset = offsetof(Vertex, r);

    pipelineCI.vertex_input_state.num_vertex_buffers = 1;
    pipelineCI.vertex_input_state.vertex_buffer_descriptions = &vbufDesc;
    pipelineCI.vertex_input_state.num_vertex_attributes = 3;
    pipelineCI.vertex_input_state.vertex_attributes = vertexAttrs;

    pipelineCI.vertex_shader = vertexShader->GetHandle();
    pipelineCI.fragment_shader = fragShader;

    pipelineCI.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;

    pipelineCI.depth_stencil_state.enable_depth_test = false;
    pipelineCI.depth_stencil_state.enable_depth_write = false;

    pipelineCI.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
    pipelineCI.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
    pipelineCI.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;

    SDL_GPUColorTargetDescription ctd;
    SDL_zero(ctd);
    ctd.format = targetFormat;
    ctd.blend_state = GetBlendState(blendMode);

    pipelineCI.target_info.num_color_targets = 1;
    pipelineCI.target_info.color_target_descriptions = &ctd;
    pipelineCI.target_info.has_depth_stencil_target = false;

    SDL_GPUGraphicsPipeline *pipeline = SDL_CreateGPUGraphicsPipeline(device, &pipelineCI);
    if (!pipeline) {
        spdlog::error("Failed to create graphics pipeline: {}", SDL_GetError());
        return nullptr;
    }

    pipelines[key] = pipeline;
    return pipeline;
}

void BatchRenderer::Flush() {
    SDL_assert(activeVertices > 0);
    SDL_assert(activeVertices % 3 == 0);

    if (!batchStarted) {
        activeVertices = 0;
        return;
    }

    vertexBuffer->SetVertexData(&vertices[0], activeVertices);

    Rectangle viewport;
    graphicsDevice->GetViewport(viewport);

    glm::mat4 projectionMatrix;
    projectionMatrix = glm::ortho<float>((float)viewport.x,
        (float)(viewport.x + viewport.width),
        (float)viewport.y,
        (float)(viewport.y + viewport.height));
    projectionMatrix = projectionMatrix * transformMatrix;

    SDL_GPUTextureFormat targetFormat;
    if (graphicsDevice->IsUsingRenderTarget()) {
        targetFormat = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    } else {
        targetFormat = graphicsDevice->GetSwapchainFormat();
    }

    graphicsDevice->BeginRenderPass();

    SDL_GPURenderPass *renderPass = graphicsDevice->GetCurrentRenderPass();
    if (!renderPass) {
        activeVertices = 0;
        return;
    }

    SDL_GPUGraphicsPipeline *pipeline =
        GetOrCreatePipeline(blendMode, targetFormat, activeFragmentShader->GetHandle());
    SDL_BindGPUGraphicsPipeline(renderPass, pipeline);

    SDL_PushGPUVertexUniformData(
        graphicsDevice->GetCommandBuffer(), 0, &projectionMatrix, sizeof(projectionMatrix));

    if (fragmentUniformData) {
        SDL_PushGPUFragmentUniformData(graphicsDevice->GetCommandBuffer(),
            fragmentUniformSlot,
            fragmentUniformData,
            fragmentUniformSize);
    }

    if (texture) {
        SDL_GPUTextureSamplerBinding samplerBinding;
        samplerBinding.texture = texture->GetGPUTexture();
        samplerBinding.sampler = texture->GetSampler();
        SDL_BindGPUFragmentSamplers(renderPass, 0, &samplerBinding, 1);
    }

    SDL_GPUBufferBinding vbufBinding;
    vbufBinding.buffer = vertexBuffer->GetGPUBuffer();
    vbufBinding.offset = 0;
    SDL_BindGPUVertexBuffers(renderPass, 0, &vbufBinding, 1);

    SDL_DrawGPUPrimitives(renderPass, activeVertices, 1, 0, 0);

    activeVertices = 0;
}

void BatchRenderer::SetFragmentUniformData(const void *data, uint32_t size, uint32_t slot) {
    fragmentUniformData = data;
    fragmentUniformSize = size;
    fragmentUniformSlot = slot;
}

} // namespace Lucky
