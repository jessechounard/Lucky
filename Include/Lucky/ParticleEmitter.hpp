#pragma once

#include <functional>
#include <string>
#include <vector>

#include <SDL3/SDL_gpu.h>
#include <glm/glm.hpp>

#include <Lucky/Color.hpp>

namespace Lucky {

struct GraphicsDevice;

struct Particle {
    glm::vec2 position;
    glm::vec2 velocity;
    glm::vec4 color;
    glm::vec4 uvRect; // x, y, w, h in atlas space
    float lifetime;
    float maxLifetime;
    float size;
    float rotation;
    float rotationSpeed;
    float _pad0;
    float _pad1;
    float _pad2;
};

struct ParticleEmitParams {
    glm::vec2 position = {0, 0};
    std::function<glm::vec2()> positionFunc;
    glm::vec2 velocityMin = {-100, -100};
    glm::vec2 velocityMax = {100, 100};
    Color colorStart = Color::White;
    Color colorEnd = Color::White;
    float lifetimeMin = 0.5f;
    float lifetimeMax = 2.0f;
    float sizeMin = 4.0f;
    float sizeMax = 8.0f;
    float rotationMin = 0.0f;
    float rotationMax = 0.0f;
    float rotationSpeedMin = 0.0f;
    float rotationSpeedMax = 0.0f;
    glm::vec4 uvRect = {0, 0, 1, 1};
};

struct ParticleEmitter {
    ParticleEmitter(GraphicsDevice &graphicsDevice, const std::string &shaderPath,
        uint32_t maxParticles, SDL_GPUTexture *texture = nullptr);
    ~ParticleEmitter();

    void SetTexture(SDL_GPUTexture *newTexture);

    ParticleEmitter(const ParticleEmitter &) = delete;
    ParticleEmitter &operator=(const ParticleEmitter &) = delete;

    void Emit(const ParticleEmitParams &params, uint32_t count);
    void Update(float deltaTime, const glm::vec2 &gravity = {0, 0});
    void Draw(const glm::mat4 &projectionMatrix);

    uint32_t GetAliveCount() const {
        return aliveCount;
    }

  private:
    GraphicsDevice *graphicsDevice;

    SDL_GPUBuffer *particleBuffer;
    SDL_GPUBuffer *counterBuffer;
    SDL_GPUTransferBuffer *transferBuffer;
    SDL_GPUTransferBuffer *counterDownloadBuffer;
    SDL_GPUComputePipeline *updatePipeline;
    SDL_GPUGraphicsPipeline *renderPipeline;
    SDL_GPUTexture *texture;
    SDL_GPUSampler *sampler;
    bool ownsTexture;

    uint32_t maxParticles;
    uint32_t nextEmitIndex;
    uint32_t aliveCount;

    std::vector<Particle> emitStaging;

    SDL_GPUTexture *CreateDefaultCircleTexture();
    SDL_GPUComputePipeline *CreateComputePipeline(const std::string &shaderPath);
    SDL_GPUGraphicsPipeline *CreateRenderPipeline(
        const std::string &vertPath, const std::string &fragPath);
};

} // namespace Lucky
