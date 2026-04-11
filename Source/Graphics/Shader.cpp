#include <stdexcept>

#include <nlohmann/json.hpp>

#include <Lucky/GraphicsDevice.hpp>
#include <Lucky/Shader.hpp>
#include <SDL3/SDL.h>
#include <spdlog/spdlog.h>

namespace Lucky {

Shader::ShaderBinary Shader::LoadBinary(SDL_GPUDevice *device, const std::string &path) {
    ShaderBinary binary = {};

    // Load metadata from JSON
    std::string jsonPath = path + ".json";
    size_t jsonSize;
    void *jsonData = SDL_LoadFile(jsonPath.c_str(), &jsonSize);
    if (!jsonData) {
        spdlog::error("Failed to load shader JSON: {} - {}", jsonPath, SDL_GetError());
        throw std::runtime_error("Failed to load shader JSON: " + jsonPath);
    }

    nlohmann::json metadata =
        nlohmann::json::parse(std::string(static_cast<char *>(jsonData), jsonSize));
    SDL_free(jsonData);

    binary.numSamplers = metadata.value("samplers", 0);
    binary.numStorageTextures = metadata.value("storage_textures", 0);
    binary.numStorageBuffers = metadata.value("storage_buffers", 0);
    binary.numUniformBuffers = metadata.value("uniform_buffers", 0);
    binary.numReadonlyStorageTextures = metadata.value("readonly_storage_textures", 0);
    binary.numReadonlyStorageBuffers = metadata.value("readonly_storage_buffers", 0);
    binary.numReadwriteStorageTextures = metadata.value("readwrite_storage_textures", 0);
    binary.numReadwriteStorageBuffers = metadata.value("readwrite_storage_buffers", 0);

    // Load shader binary with platform-specific format detection
    void *code = nullptr;
    size_t codeSize = 0;

#if defined(__PROSPERO__)
    {
        std::string binPath = path + ".ps5.bin";
        size_t fileSize;
        void *fileData = SDL_LoadFile(binPath.c_str(), &fileSize);
        if (fileData && fileSize > 12) {
            const uint8_t *p = (const uint8_t *)fileData;
            uint32_t shaderSize;
            SDL_memcpy(&shaderSize, p + 8, sizeof(uint32_t));
            binary.code.resize(shaderSize);
            SDL_memcpy(binary.code.data(), p + 12, shaderSize);
            SDL_free(fileData);
            binary.format = SDL_GPU_SHADERFORMAT_PRIVATE;
        } else {
            if (fileData)
                SDL_free(fileData);
            spdlog::error("Failed to load PS5 shader: {}", binPath);
            throw std::runtime_error("Failed to load PS5 shader: " + binPath);
        }
    }
#elif defined(__ORBIS__)
    {
        std::string binPath = path + ".ps4.bin";
        size_t fileSize;
        void *fileData = SDL_LoadFile(binPath.c_str(), &fileSize);
        if (fileData && fileSize > 12) {
            const uint8_t *p = (const uint8_t *)fileData;
            uint32_t shaderSize;
            SDL_memcpy(&shaderSize, p + 8, sizeof(uint32_t));
            binary.code.resize(shaderSize);
            SDL_memcpy(binary.code.data(), p + 12, shaderSize);
            SDL_free(fileData);
            binary.format = SDL_GPU_SHADERFORMAT_PRIVATE;
        } else {
            if (fileData)
                SDL_free(fileData);
            spdlog::error("Failed to load PS4 shader: {}", binPath);
            throw std::runtime_error("Failed to load PS4 shader: " + binPath);
        }
    }
#else
    {
        SDL_GPUShaderFormat formats = SDL_GetGPUShaderFormats(device);
        std::string fullPath;

        if (formats & SDL_GPU_SHADERFORMAT_SPIRV) {
            binary.format = SDL_GPU_SHADERFORMAT_SPIRV;
            fullPath = path + ".spv";
        } else if (formats & SDL_GPU_SHADERFORMAT_DXIL) {
            binary.format = SDL_GPU_SHADERFORMAT_DXIL;
            fullPath = path + ".dxil";
        } else if (formats & SDL_GPU_SHADERFORMAT_MSL) {
            binary.format = SDL_GPU_SHADERFORMAT_MSL;
            fullPath = path + ".msl";
        } else {
            spdlog::error("No supported shader format found");
            throw std::runtime_error("No supported shader format found");
        }

        size_t fileSize;
        void *fileData = SDL_LoadFile(fullPath.c_str(), &fileSize);
        if (!fileData) {
            spdlog::error("Failed to load shader file: {} - {}", fullPath, SDL_GetError());
            throw std::runtime_error("Failed to load shader file: " + fullPath);
        }

        binary.code.resize(fileSize);
        SDL_memcpy(binary.code.data(), fileData, fileSize);
        SDL_free(fileData);
    }
#endif

    return binary;
}

Shader::Shader(GraphicsDevice &graphicsDevice, const std::string &path, SDL_GPUShaderStage stage)
    : graphicsDevice(&graphicsDevice), shader(nullptr), stage(stage) {

    SDL_GPUDevice *device = graphicsDevice.GetDevice();
    ShaderBinary binary = LoadBinary(device, path);

    SDL_GPUShaderCreateInfo shaderCI;
    SDL_zero(shaderCI);
    shaderCI.code = binary.code.data();
    shaderCI.code_size = binary.code.size();
    shaderCI.entrypoint = "main";
    shaderCI.format = binary.format;
    shaderCI.stage = stage;
    shaderCI.num_samplers = binary.numSamplers;
    shaderCI.num_storage_textures = binary.numStorageTextures;
    shaderCI.num_storage_buffers = binary.numStorageBuffers;
    shaderCI.num_uniform_buffers = binary.numUniformBuffers;

    shader = SDL_CreateGPUShader(device, &shaderCI);
    if (!shader) {
        spdlog::error("Failed to create GPU shader: {} - {}", path, SDL_GetError());
        throw std::runtime_error("Failed to create GPU shader: " + path);
    }
}

Shader::~Shader() {
    if (shader) {
        SDL_ReleaseGPUShader(graphicsDevice->GetDevice(), shader);
    }
}

} // namespace Lucky
