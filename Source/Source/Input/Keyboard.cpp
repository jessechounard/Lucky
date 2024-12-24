#include <Lucky/Input/Keyboard.hpp>

const Lucky::KeyboardState &GetKeyboardState_impl();

namespace Lucky
{
    const KeyboardState &GetKeyboardState()
    {
        return GetKeyboardState_impl();
    }
} // namespace Lucky