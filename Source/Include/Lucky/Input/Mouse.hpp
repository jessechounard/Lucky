#pragma once

namespace Lucky
{
    struct MouseButton
    {
        static constexpr int Left = 1;
        static constexpr int Middle = 2;
        static constexpr int Right = 3;

        static constexpr int MaxValue = 16;
    };

    constexpr int MaxMouseButtons = MouseButton::MaxValue;

    struct MouseState
    {
        float x;
        float y;
        float wheelDelta;
        bool buttons[MaxMouseButtons];
    };

    const MouseState &GetMouseState();
} // namespace Lucky
