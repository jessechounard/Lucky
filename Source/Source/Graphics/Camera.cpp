#include <glm/ext/matrix_transform.hpp>

#include <Lucky/Graphics/Camera.hpp>

namespace Lucky
{
    Camera::Camera(std::shared_ptr<GraphicsDevice> graphicsDevice, std::shared_ptr<Texture> renderTarget)
        : graphicsDevice(graphicsDevice),
          renderTarget(renderTarget)
    {
    }

    void Camera::BeginFrame()
    {
        if (renderTarget)
        {
            graphicsDevice->BindRenderTarget(*renderTarget);
        }

        graphicsDevice->ClearScreen(clearColor);
    }

    void Camera::EndFrame()
    {
        if (renderTarget)
        {
            graphicsDevice->UnbindRenderTarget();
        }
    }

    void Camera::UpdateTransform()
    {
        if (transformDirty)
        {
            Rectangle viewport;
            graphicsDevice->GetViewport(viewport);

            // having these broken out into multiple transforms and not
            // chained might be less efficient (?) but it's way more readable
            // todo: test this
            auto transform = glm::mat4(1.0f);
            transform = glm::translate(transform, glm::vec3(viewport.width / 2.0f, viewport.height / 2.0f, 0.0));
            transform = glm::scale(transform, glm::vec3(zoom, zoom, 1.0));
            transform = glm::rotate(transform, rotation, glm::vec3(0.0f, 0.0f, 1.0f));
            transform = glm::translate(transform, glm::vec3(-position.x, -position.y, 0.0f));

            transformMatrix = transform;
            transformDirty = false;
        }
    }

    void Camera::GetViewport(glm::vec2 &topLeft, glm::vec2 &bottomRight)
    {
        // todo: Should we factor in rotation to this?

        int width, height;

        if (renderTarget)
        {
            width = renderTarget->GetWidth();
            height = renderTarget->GetHeight();
        }
        else
        {
            Rectangle viewport;
            graphicsDevice->GetViewport(viewport);
            width = viewport.width;
            height = viewport.height;
        }

        topLeft.x = position.x - width / 2.0f / zoom;
        topLeft.y = position.y - height / 2.0f / zoom;
        bottomRight.x = position.x + width / 2.0f / zoom;
        bottomRight.y = position.y + height / 2.0f / zoom;
    }
} // namespace Lucky
