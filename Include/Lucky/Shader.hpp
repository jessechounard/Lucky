#pragma once

#include <string>
#include <vector>

#include <SDL3/SDL_gpu.h>

namespace Lucky {

struct GraphicsDevice;

struct Shader {
  public:
    Shader(GraphicsDevice &graphicsDevice, const std::string &path, SDL_GPUShaderStage stage);
    Shader(const Shader &) = delete;
    ~Shader();

    Shader &operator=(const Shader &) = delete;

    SDL_GPUShader *GetHandle() const {
        return shader;
    }
    SDL_GPUShaderStage GetStage() const {
        return stage;
    }

    struct ShaderBinary {
        std::vector<uint8_t> code;
        SDL_GPUShaderFormat format;
        uint32_t numSamplers = 0;
        uint32_t numStorageTextures = 0;
        uint32_t numStorageBuffers = 0;
        uint32_t numUniformBuffers = 0;
        uint32_t numReadonlyStorageTextures = 0;
        uint32_t numReadonlyStorageBuffers = 0;
        uint32_t numReadwriteStorageTextures = 0;
        uint32_t numReadwriteStorageBuffers = 0;
    };

    static ShaderBinary LoadBinary(SDL_GPUDevice *device, const std::string &path);

  private:
    GraphicsDevice *graphicsDevice;
    SDL_GPUShader *shader;
    SDL_GPUShaderStage stage;
};

} // namespace Lucky
