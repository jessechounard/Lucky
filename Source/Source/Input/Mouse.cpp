#include <cassert>

#include <Lucky/Input/Mouse.hpp>

const Lucky::MouseState &GetMouseState_impl();

namespace Lucky
{
    const MouseState &GetMouseState()
    {
        return GetMouseState_impl();
    }
} // namespace Lucky