#pragma once

#include <string>
#include <vector>

#include <SDL3/SDL_gpu.h>

namespace Lucky {

struct GraphicsDevice;

/**
 * A compiled GPU shader loaded from disk.
 *
 * Loads a shader's binary code and JSON-described resource bindings, then
 * creates an SDL_GPUShader. Renderers attach the resulting handle to a
 * pipeline via SDL's pipeline create info.
 *
 * # On-disk format
 *
 * Each shader is two files sharing a base path:
 *   - `<path>.json`: metadata (sampler/buffer counts) emitted by
 *     shadercross.
 *   - `<path>.<extension>`: the shader binary in whichever format the GPU
 *     driver accepts. The constructor probes the device and picks the
 *     first supported format from SPIR-V (`.spv`), DXIL (`.dxil`), or MSL
 *     (`.msl`).
 *
 * On PlayStation builds (when `__PROSPERO__` or `__ORBIS__` is defined),
 * the loader picks a platform-private binary instead: `<path>.ps5.bin` or
 * `<path>.ps4.bin`. Those files use a 12-byte header (size at offset 8 as
 * a little-endian uint32) followed by raw shader bytes, and load with
 * format `SDL_GPU_SHADERFORMAT_PRIVATE`. They have to be produced by
 * Sony's compiler -- shadercross does not emit them. See the sibling
 * Lucky.PlayStation project for the build pipeline.
 *
 * # Lifetime
 *
 * Holds a pointer to the GraphicsDevice. The GraphicsDevice must outlive
 * this shader.
 */
struct Shader {
    /**
     * Loads a shader from disk and creates an SDL_GPUShader.
     *
     * Throws std::runtime_error if any of the following fail: the JSON
     * metadata file cannot be read or parsed, no supported binary format
     * is found, the binary file cannot be read, or SDL_CreateGPUShader
     * rejects the binary.
     *
     * \param graphicsDevice the graphics device. Must outlive this shader.
     * \param path the shader's base path (without extension).
     * \param stage SDL_GPU_SHADERSTAGE_VERTEX or SDL_GPU_SHADERSTAGE_FRAGMENT.
     */
    Shader(GraphicsDevice &graphicsDevice, const std::string &path, SDL_GPUShaderStage stage);

    Shader(const Shader &) = delete;
    Shader &operator=(const Shader &) = delete;
    Shader(Shader &&) = delete;
    Shader &operator=(Shader &&) = delete;

    ~Shader();

    /**
     * Returns the underlying SDL_GPUShader handle for use in pipeline
     * creation.
     */
    SDL_GPUShader *GetHandle() const {
        return shader;
    }

    /**
     * Returns the stage this shader was loaded for (vertex or fragment).
     */
    SDL_GPUShaderStage GetStage() const {
        return stage;
    }

    /**
     * Decoded shader binary plus the resource binding counts read from the
     * JSON metadata.
     *
     * Most consumers do not need this directly -- the Shader constructor
     * uses it internally to call SDL_CreateGPUShader. Compute pipelines
     * (e.g. ParticleEmitter) load the binary independently and consume
     * the readonly/readwrite storage counts that aren't exposed on
     * SDL_GPUShaderCreateInfo.
     */
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

    /**
     * Loads a shader's binary and metadata from disk without creating an
     * SDL_GPUShader.
     *
     * Used by compute-pipeline creation paths (see ParticleEmitter) where
     * the caller needs the readonly/readwrite storage counts that the
     * regular Shader constructor doesn't surface. Throws std::runtime_error
     * on any I/O or format error; see the class-level "On-disk format"
     * section for the file conventions.
     *
     * \param device the SDL_GPUDevice used to query supported shader formats.
     * \param path the shader's base path (without extension).
     * \returns the decoded binary and metadata.
     */
    static ShaderBinary LoadBinary(SDL_GPUDevice *device, const std::string &path);

  private:
    GraphicsDevice *graphicsDevice;
    SDL_GPUShader *shader;
    SDL_GPUShaderStage stage;
};

} // namespace Lucky
