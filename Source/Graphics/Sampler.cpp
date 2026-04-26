#include <stdexcept>

#include <SDL3/SDL.h>
#include <spdlog/spdlog.h>

#include <Lucky/GraphicsDevice.hpp>
#include <Lucky/Sampler.hpp>

namespace Lucky {

namespace {

SDL_GPUFilter ToSDL(SamplerFilter filter) {
    switch (filter) {
    case SamplerFilter::Linear:
        return SDL_GPU_FILTER_LINEAR;
    case SamplerFilter::Point:
        return SDL_GPU_FILTER_NEAREST;
    }
    return SDL_GPU_FILTER_LINEAR;
}

SDL_GPUSamplerMipmapMode ToSDLMipmap(SamplerFilter filter) {
    switch (filter) {
    case SamplerFilter::Linear:
        return SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
    case SamplerFilter::Point:
        return SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
    }
    return SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
}

SDL_GPUSamplerAddressMode ToSDL(SamplerAddressMode mode) {
    switch (mode) {
    case SamplerAddressMode::Repeat:
        return SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
    case SamplerAddressMode::ClampToEdge:
        return SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    case SamplerAddressMode::MirroredRepeat:
        return SDL_GPU_SAMPLERADDRESSMODE_MIRRORED_REPEAT;
    }
    return SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
}

} // namespace

Sampler::Sampler(GraphicsDevice &graphicsDevice, const SamplerDescription &description)
    : graphicsDevice(graphicsDevice) {
    SDL_GPUSamplerCreateInfo ci;
    SDL_zero(ci);
    ci.min_filter = ToSDL(description.filter);
    ci.mag_filter = ToSDL(description.filter);
    ci.mipmap_mode = ToSDLMipmap(description.mipmapFilter);
    ci.address_mode_u = ToSDL(description.addressU);
    ci.address_mode_v = ToSDL(description.addressV);
    ci.address_mode_w = ToSDL(description.addressW);

    sampler = SDL_CreateGPUSampler(graphicsDevice.GetDevice(), &ci);
    if (!sampler) {
        spdlog::error("Failed to create GPU sampler: {}", SDL_GetError());
        throw std::runtime_error("Failed to create GPU sampler");
    }
}

Sampler::~Sampler() {
    if (sampler) {
        SDL_ReleaseGPUSampler(graphicsDevice.GetDevice(), sampler);
    }
}

} // namespace Lucky
