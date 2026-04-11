#pragma once

#include <SDL3/SDL_gpu.h>
#include <Lucky/Types.hpp>

namespace Lucky {

inline SDL_GPUColorTargetBlendState GetBlendState(BlendMode blendMode) {
    SDL_GPUColorTargetBlendState state;
    SDL_zero(state);
    state.enable_blend = true;
    state.color_write_mask = SDL_GPU_COLORCOMPONENT_R | SDL_GPU_COLORCOMPONENT_G |
                             SDL_GPU_COLORCOMPONENT_B | SDL_GPU_COLORCOMPONENT_A;

    switch (blendMode) {
    case BlendMode::None:
        state.color_blend_op = SDL_GPU_BLENDOP_ADD;
        state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
        state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
        state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ZERO;
        state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
        state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ZERO;
        break;
    case BlendMode::Additive:
        state.color_blend_op = SDL_GPU_BLENDOP_ADD;
        state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
        state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
        state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
        state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
        state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ZERO;
        break;
    case BlendMode::Alpha:
        state.color_blend_op = SDL_GPU_BLENDOP_ADD;
        state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
        state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
        state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
        state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ZERO;
        break;
    case BlendMode::PremultipliedAlpha:
        state.color_blend_op = SDL_GPU_BLENDOP_ADD;
        state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
        state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
        state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
        state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ZERO;
        break;
    case BlendMode::Min:
        state.color_blend_op = SDL_GPU_BLENDOP_MIN;
        state.alpha_blend_op = SDL_GPU_BLENDOP_MIN;
        state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
        state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
        state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
        state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
        break;
    default:
        break;
    }

    return state;
}

} // namespace Lucky
