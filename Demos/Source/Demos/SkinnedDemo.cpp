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

// Skinning demo: CesiumMan.glb plays its embedded walk animation on a
// ground plane. Validates the full skinning pipeline: glTF skin parse,
// per-vertex JOINTS_0/WEIGHTS_0 attributes, joint matrix assembly in
// ModelInstance, and the skinned forward + shadow render passes.
class SkinnedDemo : public LuckyDemos::DemoBase {
  public:
    explicit SkinnedDemo(SDL_Window *window)
        : graphicsDevice(window), forwardRenderer(graphicsDevice) {
        graphicsDevice.SetClearColor({0.08f, 0.10f, 0.13f, 1.0f});
        graphicsDevice.SetDepthEnabled(true);

        const std::filesystem::path basePath = SDL_GetBasePath();
        const std::string modelPath = (basePath / "Content/Models/CesiumMan.glb").generic_string();
        manModel = std::make_unique<Lucky::Model>(graphicsDevice, modelPath);
        manInstance = std::make_unique<Lucky::ModelInstance>(*manModel);

        if (manModel->GetAnimationCount() > 0) {
            manInstance->PlayAnimation(0, true);
        }

        planeMesh = std::make_unique<Lucky::Mesh>(graphicsDevice, Lucky::MakePlaneMeshData(8.0f));

        // CesiumMan ships in Y-up, ~1.7 unit tall, facing -Z. Pull the
        // camera back along +Z and slightly up for a three-quarter view.
        camera.position = {2.0f, 1.5f, 3.0f};
        camera.fovY = glm::radians(60.0f);
        camera.zNear = 0.1f;
        camera.zFar = 50.0f;
        const glm::vec3 lookTarget = {0.0f, 0.8f, 0.0f};
        const glm::vec3 toTarget = glm::normalize(lookTarget - camera.position);
        camera.yaw = std::atan2(-toTarget.x, -toTarget.z);
        camera.pitch = std::asin(glm::clamp(toTarget.y, -1.0f, 1.0f));

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
        manInstance->Update(deltaSeconds);

        // Lower the model so his feet land on the ground plane (which
        // sits at Y = -0.5). The walk cycle is driven entirely by the
        // glTF animation -- we don't translate the root over time, so
        // he walks in place.
        manInstance->SetRootTransform(
            glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -0.5f, 0.0f)));

        scene.objects.clear();
        scene.skinnedObjects.clear();

        Lucky::SceneObject ground;
        ground.mesh = planeMesh.get();
        ground.transform = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -0.5f, 0.0f));
        ground.color = glm::vec3(0.6f, 0.6f, 0.65f);
        scene.objects.push_back(ground);

        manInstance->AppendToScene(scene, glm::vec3(1.0f));
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

    std::unique_ptr<Lucky::Model> manModel;
    std::unique_ptr<Lucky::ModelInstance> manInstance;
    std::unique_ptr<Lucky::Mesh> planeMesh;

    Lucky::Camera camera;
    Lucky::Scene3D scene;
};

} // namespace

LUCKY_DEMO("Skinned", "CesiumMan.glb walking with joint-matrix skinning", SkinnedDemo);
