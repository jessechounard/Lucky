#pragma once

#include <memory>
#include <stdint.h>

#include <glm/glm.hpp>

#include <Lucky/Graphics/Color.hpp>
#include <Lucky/Graphics/GraphicsDevice.hpp>
#include <Lucky/Graphics/Texture.hpp>

namespace Lucky
{
    // todo: This stuff (mostly) works, but it should probably be refactored
    // to be built from components and work like other "game objects"

    struct Camera
    {
        static constexpr uint64_t EverythingMask = UINT64_C(0xffffffffffffffff);
        static constexpr uint64_t NothingMask = UINT64_C(0);

        Camera(std::shared_ptr<GraphicsDevice> graphicsDevice, std::shared_ptr<Texture> renderTarget);

        virtual ~Camera()
        {
        }

        virtual void Update(float deltaSeconds)
        {
        }

        virtual void BeginFrame();
        virtual void EndFrame();

        void UpdateTransform();

        void GetViewport(glm::vec2 &topLeft, glm::vec2 &bottomRight);

        std::shared_ptr<GraphicsDevice> graphicsDevice;
        std::shared_ptr<Texture> renderTarget;

        glm::vec2 position = {0.0f, 0.0f};
        float rotation = 0.0f;
        float zoom = 1.0f;
        bool transformDirty = true;

        glm::mat4 transformMatrix{1.0f};

        Color clearColor = Color::CornflowerBlue;

        int32_t renderPriority = 0; // higher priority is rendered first, negative won't render
        uint64_t renderMask = EverythingMask;
    };
} // namespace Lucky
