#pragma once

#include <stdint.h>

namespace Lucky {

/**
 * Per-pipeline color blend mode.
 *
 * Used by every renderer that exposes a Begin(BlendMode, ...) entry point
 * to select how new geometry combines with existing framebuffer pixels.
 * Translated to SDL_GPUColorTargetBlendState by GetBlendState in
 * BlendState.hpp.
 *
 * Invalid is a sentinel for uninitialized state and asserts if passed to
 * GetBlendState; pick one of the other values for any real draw.
 */
enum class BlendMode {
    Invalid,            /**< sentinel; not a valid mode. */
    None,               /**< no blending; source replaces destination. */
    Additive,           /**< src + dst; brightens. */
    Alpha,              /**< standard src-alpha lerp; assumes straight alpha. */
    PremultipliedAlpha, /**< src + dst * (1 - src.a); alpha pre-baked into rgb. */
    Min,                /**< per-channel minimum; useful for masks. */
};

/**
 * Bit-flag set describing how a sprite's UVs should be transformed before
 * sampling.
 *
 * Used by BatchRenderer::BatchSprite to handle TexturePacker-style sprite
 * atlases where individual frames can be rotated or flipped to pack
 * tighter. Combine flags with `|`; test with `HasFlag` (declared in
 * BatchRenderer.hpp).
 */
enum class UVMode : uint32_t {
    Normal = 0,              /**< no transform; sample UVs as-is. */
    RotatedCW90 = 1 << 0,    /**< rotated 90 degrees clockwise; matches TexturePacker. */
    FlipHorizontal = 1 << 1, /**< mirrored left-to-right. */
    FlipVertical = 1 << 2,   /**< mirrored top-to-bottom. */
};

} // namespace Lucky
