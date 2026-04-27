#include <SDL3/SDL.h>
#include <filesystem>
#include <memory>

#include <glm/gtc/matrix_transform.hpp>

#include <Lucky/Camera.hpp>
#include <Lucky/ForwardRenderer.hpp>
#include <Lucky/GraphicsDevice.hpp>
#include <Lucky/Mesh.hpp>
#include <Lucky/Model.hpp>
#include <Lucky/ModelInstance.hpp>
#include <Lucky/Scene3D.hpp>

#include "../DemoBase.hpp"
#include "../DemoRegistry.hpp"

namespace {

// Animation playback demo: BoxAnimated.glb (Khronos sample) plays its
// embedded translate+rotate animation on a ground plane. Validates the
// rigid-node animation path: parse keyframes, advance time, sample
// LERP/SLERP into per-node TRS, render through the existing
// ModelInstance world-transform pipeline.
class AnimationDemo : public LuckyDemos::DemoBase {
  public:
    explicit AnimationDemo(SDL_Window *window)
        : graphicsDevice(window), forwardRenderer(graphicsDevice) {
        graphicsDevice.SetClearColor({0.08f, 0.10f, 0.13f, 1.0f});
        graphicsDevice.SetDepthEnabled(true);

        const std::filesystem::path basePath = SDL_GetBasePath();
        const std::string modelPath =
            (basePath / "Content/Models/BoxAnimated.glb").generic_string();
        boxModel = std::make_unique<Lucky::Model>(graphicsDevice, modelPath);
        boxInstance = std::make_unique<Lucky::ModelInstance>(*boxModel);

        // Start the first embedded animation looping. Most Khronos
        // sample assets ship a single animation at index 0; demos that
        // ship multiple can pick by name with `Model::FindAnimation`.
        if (boxModel->GetAnimationCount() > 0) {
            boxInstance->PlayAnimation(0, true);
        }

        planeMesh = std::make_unique<Lucky::Mesh>(graphicsDevice, Lucky::MakePlaneMeshData(8.0f));

        camera.position = {4.0f, 3.0f, 5.0f};
        camera.fovY = glm::radians(60.0f);
        camera.zNear = 0.1f;
        camera.zFar = 50.0f;
        const glm::vec3 toOrigin = glm::normalize(-camera.position);
        camera.yaw = std::atan2(-toOrigin.x, -toOrigin.z);
        camera.pitch = std::asin(glm::clamp(toOrigin.y, -1.0f, 1.0f));

        scene.ambientColor = {0.10f, 0.11f, 0.13f};

        Lucky::Light sun;
        sun.type = Lucky::LightType::Directional;
        sun.direction = glm::normalize(glm::vec3(-0.4f, -1.0f, -0.3f));
        sun.color = {1.0f, 0.95f, 0.85f};
        sun.intensity = 1.0f;
        sun.castsShadows = true;
        scene.lights.push_back(sun);
    }

    void HandleEvent(const SDL_Event &event) override {
        if (event.type != SDL_EVENT_KEY_DOWN || event.key.repeat) {
            return;
        }
        switch (event.key.key) {
        case SDLK_ESCAPE:
            RequestExit();
            break;
        case SDLK_PAGEDOWN:
            RequestNextDemo();
            break;
        case SDLK_PAGEUP:
            RequestPrevDemo();
            break;
        }
    }

    void Update(float deltaSeconds) override {
        boxInstance->Update(deltaSeconds);

        // Place the model with a static root transform; the animation
        // moves the box's interior nodes within it.
        boxInstance->SetRootTransform(glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.5f, 0.0f)));

        scene.objects.clear();
        Lucky::SceneObject ground;
        ground.mesh = planeMesh.get();
        ground.transform = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -0.5f, 0.0f));
        ground.color = glm::vec3(0.6f, 0.6f, 0.65f);
        scene.objects.push_back(ground);

        boxInstance->AppendToScene(scene, glm::vec3(1.0f));
    }

    void Draw() override {
        graphicsDevice.BeginFrame();
        if (!graphicsDevice.GetCommandBuffer()) {
            return;
        }
        forwardRenderer.Render(scene, camera);
        graphicsDevice.EndFrame();
    }

  private:
    Lucky::GraphicsDevice graphicsDevice;
    Lucky::ForwardRenderer forwardRenderer;

    std::unique_ptr<Lucky::Model> boxModel;
    std::unique_ptr<Lucky::ModelInstance> boxInstance;
    std::unique_ptr<Lucky::Mesh> planeMesh;

    Lucky::Camera camera;
    Lucky::Scene3D scene;
};

} // namespace

LUCKY_DEMO("Animation", "BoxAnimated.glb playing back its embedded translate+rotate animation",
    AnimationDemo);
