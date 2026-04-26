#include <doctest/doctest.h>

#include <Lucky/BlendState.hpp>
#include <Lucky/Types.hpp>

using namespace Lucky;

TEST_CASE("GetBlendState disables blend for BlendMode::None") {
    SDL_GPUColorTargetBlendState state = GetBlendState(BlendMode::None);
    CHECK(state.enable_blend == false);
}

TEST_CASE("GetBlendState configures additive blend") {
    SDL_GPUColorTargetBlendState state = GetBlendState(BlendMode::Additive);
    CHECK(state.enable_blend == true);
    CHECK(state.color_blend_op == SDL_GPU_BLENDOP_ADD);
    CHECK(state.src_color_blendfactor == SDL_GPU_BLENDFACTOR_ONE);
    CHECK(state.dst_color_blendfactor == SDL_GPU_BLENDFACTOR_ONE);
}

TEST_CASE("GetBlendState configures alpha blend") {
    SDL_GPUColorTargetBlendState state = GetBlendState(BlendMode::Alpha);
    CHECK(state.enable_blend == true);
    CHECK(state.color_blend_op == SDL_GPU_BLENDOP_ADD);
    CHECK(state.src_color_blendfactor == SDL_GPU_BLENDFACTOR_SRC_ALPHA);
    CHECK(state.dst_color_blendfactor == SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA);
}

TEST_CASE("GetBlendState configures premultiplied alpha blend") {
    SDL_GPUColorTargetBlendState state = GetBlendState(BlendMode::PremultipliedAlpha);
    CHECK(state.enable_blend == true);
    CHECK(state.color_blend_op == SDL_GPU_BLENDOP_ADD);
    CHECK(state.src_color_blendfactor == SDL_GPU_BLENDFACTOR_ONE);
    CHECK(state.dst_color_blendfactor == SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA);
}

TEST_CASE("GetBlendState configures min blend") {
    SDL_GPUColorTargetBlendState state = GetBlendState(BlendMode::Min);
    CHECK(state.enable_blend == true);
    CHECK(state.color_blend_op == SDL_GPU_BLENDOP_MIN);
}

TEST_CASE("GetBlendState writes all RGBA channels for every valid mode") {
    const SDL_GPUColorComponentFlags expected = SDL_GPU_COLORCOMPONENT_R |
                                                SDL_GPU_COLORCOMPONENT_G |
                                                SDL_GPU_COLORCOMPONENT_B | SDL_GPU_COLORCOMPONENT_A;
    const BlendMode modes[] = {
        BlendMode::None,
        BlendMode::Additive,
        BlendMode::Alpha,
        BlendMode::PremultipliedAlpha,
        BlendMode::Min,
    };
    for (BlendMode mode : modes) {
        SDL_GPUColorTargetBlendState state = GetBlendState(mode);
        CHECK(state.color_write_mask == expected);
    }
}
