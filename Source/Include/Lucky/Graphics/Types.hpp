#pragma once

namespace Lucky
{
    enum class BlendMode
    {
        Invalid,
        None,
        Additive,
        Alpha,
        PremultipliedAlpha,
    };

    enum class UVMode : uint32_t
    {
        Normal = 0,
        RotatedCW90 = 1 << 1,    // Matches TexturePacker's rotation
        FlipHorizontal = 1 << 2, // Flipped left to right
        FlipVertical = 1 << 3,   // Flipped top to bottom
    };
} // namespace Lucky