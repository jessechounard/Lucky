#include <SDL3/SDL_assert.h>
#include <limits>
#include <stdexcept>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <Lucky/GraphicsDevice.hpp>
#include <Lucky/Texture.hpp>
#include <spdlog/spdlog.h>
#include <stb_image.h>

namespace Lucky {

namespace {

struct StbiPixelData {
    uint8_t *data = nullptr;
    ~StbiPixelData() {
        if (data) {
            stbi_image_free(data);
        }
    }
};

uint32_t ExpectedByteSize(uint32_t width, uint32_t height, TextureFormat format) {
    const size_t bytes = static_cast<size_t>(width) * static_cast<size_t>(height) *
                         static_cast<size_t>(BytesPerPixel(format));
    if (bytes > std::numeric_limits<uint32_t>::max()) {
        throw std::runtime_error("Texture dimensions exceed 4 GiB upload limit");
    }
    return static_cast<uint32_t>(bytes);
}

} // namespace

Texture::Texture(GraphicsDevice &graphicsDevice, const std::string &filename,
    TextureFilter textureFilter, TextureFormat textureFormat)
    : graphicsDevice(graphicsDevice) {
    if (textureFormat == TextureFormat::HDR) {
        spdlog::error("HDR file loading is not yet supported: {}", filename);
        throw std::runtime_error("HDR file loading is not yet supported");
    }

    int imageWidth, imageHeight, imageChannels;
    StbiPixelData pixels;
    pixels.data = stbi_load(filename.c_str(), &imageWidth, &imageHeight, &imageChannels, 4);
    if (!pixels.data) {
        spdlog::error("Failed to load image file: {}", filename);
        throw std::runtime_error("Failed to load image file: " + filename);
    }

    const uint32_t dataLength = ExpectedByteSize(
        static_cast<uint32_t>(imageWidth), static_cast<uint32_t>(imageHeight), textureFormat);

    Initialize(TextureType::Default,
        static_cast<uint32_t>(imageWidth),
        static_cast<uint32_t>(imageHeight),
        pixels.data,
        dataLength,
        textureFilter,
        textureFormat);
}

Texture::Texture(GraphicsDevice &graphicsDevice, uint8_t *memory, uint32_t memoryLength,
    TextureFilter textureFilter, TextureFormat textureFormat)
    : graphicsDevice(graphicsDevice) {
    SDL_assert(memory != nullptr);

    if (textureFormat == TextureFormat::HDR) {
        spdlog::error("HDR in-memory image loading is not yet supported");
        throw std::runtime_error("HDR in-memory image loading is not yet supported");
    }

    int imageWidth, imageHeight, imageChannels;
    StbiPixelData pixels;
    pixels.data =
        stbi_load_from_memory(memory, memoryLength, &imageWidth, &imageHeight, &imageChannels, 4);
    if (!pixels.data) {
        spdlog::error("Failed to load image from memory");
        throw std::runtime_error("Failed to load image from memory");
    }

    const uint32_t dataLength = ExpectedByteSize(
        static_cast<uint32_t>(imageWidth), static_cast<uint32_t>(imageHeight), textureFormat);

    Initialize(TextureType::Default,
        static_cast<uint32_t>(imageWidth),
        static_cast<uint32_t>(imageHeight),
        pixels.data,
        dataLength,
        textureFilter,
        textureFormat);
}

Texture::Texture(GraphicsDevice &graphicsDevice, TextureType textureType, uint32_t width,
    uint32_t height, uint8_t *pixelData, uint32_t dataLength, TextureFilter textureFilter,
    TextureFormat textureFormat)
    : graphicsDevice(graphicsDevice) {
    SDL_assert(width > 0);
    SDL_assert(height > 0);
    if (pixelData != nullptr) {
        const uint32_t expected = ExpectedByteSize(width, height, textureFormat);
        SDL_assert(dataLength >= expected);
    }

    Initialize(textureType, width, height, pixelData, dataLength, textureFilter, textureFormat);
}

void Texture::Initialize(TextureType textureType, uint32_t width, uint32_t height,
    uint8_t *pixelData, uint32_t dataLength, TextureFilter textureFilter,
    TextureFormat textureFormat) {
    this->width = width;
    this->height = height;
    this->textureType = textureType;
    this->textureFormat = textureFormat;

    SDL_GPUTextureCreateInfo texCI;
    SDL_zero(texCI);
    texCI.type = SDL_GPU_TEXTURETYPE_2D;
    texCI.format = (textureFormat == TextureFormat::HDR) ? SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT
                                                         : SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    texCI.width = width;
    texCI.height = height;
    texCI.layer_count_or_depth = 1;
    texCI.num_levels = 1;
    texCI.sample_count = SDL_GPU_SAMPLECOUNT_1;
    texCI.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;

    if (textureType == TextureType::RenderTarget) {
        texCI.usage |= SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
    }

    gpuTexture = SDL_CreateGPUTexture(graphicsDevice.GetDevice(), &texCI);
    if (!gpuTexture) {
        spdlog::error("Failed to create GPU texture: {}", SDL_GetError());
        throw std::runtime_error("Failed to create GPU texture");
    }

    CreateSampler(textureFilter);

    if (pixelData != nullptr) {
        UploadRegion(0, 0, width, height, pixelData, dataLength);
    }
}

Texture::~Texture() {
    if (sampler) {
        SDL_ReleaseGPUSampler(graphicsDevice.GetDevice(), sampler);
    }
    if (gpuTexture) {
        SDL_ReleaseGPUTexture(graphicsDevice.GetDevice(), gpuTexture);
    }
}

void Texture::CreateSampler(TextureFilter filter) {
    this->textureFilter = filter;

    if (sampler) {
        SDL_ReleaseGPUSampler(graphicsDevice.GetDevice(), sampler);
        sampler = nullptr;
    }

    SDL_GPUSamplerCreateInfo sampCI;
    SDL_zero(sampCI);

    switch (filter) {
    case TextureFilter::Linear:
        sampCI.min_filter = SDL_GPU_FILTER_LINEAR;
        sampCI.mag_filter = SDL_GPU_FILTER_LINEAR;
        break;
    case TextureFilter::Point:
        sampCI.min_filter = SDL_GPU_FILTER_NEAREST;
        sampCI.mag_filter = SDL_GPU_FILTER_NEAREST;
        break;
    }

    sampCI.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
    sampCI.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    sampCI.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    sampCI.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;

    sampler = SDL_CreateGPUSampler(graphicsDevice.GetDevice(), &sampCI);
    if (!sampler) {
        spdlog::error("Failed to create GPU sampler: {}", SDL_GetError());
        throw std::runtime_error("Failed to create GPU sampler");
    }
}

void Texture::UploadRegion(
    uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint8_t *pixelData, uint32_t dataLength) {
    SDL_GPUDevice *device = graphicsDevice.GetDevice();

    SDL_GPUTransferBufferCreateInfo tbCI;
    SDL_zero(tbCI);
    tbCI.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    tbCI.size = dataLength;

    SDL_GPUTransferBuffer *transferBuffer = SDL_CreateGPUTransferBuffer(device, &tbCI);
    if (!transferBuffer) {
        spdlog::error("Failed to create transfer buffer: {}", SDL_GetError());
        throw std::runtime_error("Failed to create transfer buffer for texture upload");
    }

    void *mapped = SDL_MapGPUTransferBuffer(device, transferBuffer, false);
    if (!mapped) {
        SDL_ReleaseGPUTransferBuffer(device, transferBuffer);
        spdlog::error("Failed to map transfer buffer: {}", SDL_GetError());
        throw std::runtime_error("Failed to map transfer buffer for texture upload");
    }
    memcpy(mapped, pixelData, dataLength);
    SDL_UnmapGPUTransferBuffer(device, transferBuffer);

    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(device);
    if (!cmd) {
        SDL_ReleaseGPUTransferBuffer(device, transferBuffer);
        spdlog::error("Failed to acquire command buffer: {}", SDL_GetError());
        throw std::runtime_error("Failed to acquire command buffer for texture upload");
    }

    SDL_GPUCopyPass *copyPass = SDL_BeginGPUCopyPass(cmd);
    if (!copyPass) {
        SDL_SubmitGPUCommandBuffer(cmd);
        SDL_ReleaseGPUTransferBuffer(device, transferBuffer);
        spdlog::error("Failed to begin copy pass: {}", SDL_GetError());
        throw std::runtime_error("Failed to begin copy pass for texture upload");
    }

    SDL_GPUTextureTransferInfo src;
    SDL_zero(src);
    src.transfer_buffer = transferBuffer;

    SDL_GPUTextureRegion dst;
    SDL_zero(dst);
    dst.texture = gpuTexture;
    dst.x = x;
    dst.y = y;
    dst.w = w;
    dst.h = h;
    dst.d = 1;

    SDL_UploadToGPUTexture(copyPass, &src, &dst, false);
    SDL_EndGPUCopyPass(copyPass);
    SDL_SubmitGPUCommandBuffer(cmd);

    SDL_ReleaseGPUTransferBuffer(device, transferBuffer);
}

void Texture::SetTextureData(
    uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint8_t *pixelData, uint32_t dataLength) {
    SDL_assert(pixelData != nullptr);
    SDL_assert(x + w <= width);
    SDL_assert(y + h <= height);
    const uint32_t expected = ExpectedByteSize(w, h, textureFormat);
    SDL_assert(dataLength == expected);

    UploadRegion(x, y, w, h, pixelData, dataLength);
}

void Texture::SetTextureFilter(TextureFilter filter) {
    CreateSampler(filter);
}

} // namespace Lucky
