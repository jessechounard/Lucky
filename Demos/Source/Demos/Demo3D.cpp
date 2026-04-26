#include <SDL3/SDL.h>
#include <filesystem>
#include <memory>

#include <glm/gtc/matrix_transform.hpp>

#include <Lucky/Camera.hpp>
#include <Lucky/Color.hpp>
#include <Lucky/ForwardRenderer.hpp>
#include <Lucky/GraphicsDevice.hpp>
#include <Lucky/MathConstants.hpp>
#include <Lucky/Mesh.hpp>
#include <Lucky/Model.hpp>
#include <Lucky/ModelInstance.hpp>
#include <Lucky/Scene3D.hpp>

#include "../DemoBase.hpp"
#include "../DemoRegistry.hpp"

namespace {

// 3D demo: a Box.glb loaded via the cgltf-backed Model on top of a
// generated ground plane, lit by a directional sun (with shadows) and
// a blue point fill light. Phase A/B/C of the forward renderer plus the
// Model/ModelInstance loading path.
class Demo3D : public LuckyDemos::DemoBase {
  public:
    explicit Demo3D(SDL_Window *window) : graphicsDevice(window), forwardRenderer(graphicsDevice) {
        graphicsDevice.SetClearColor({0.08f, 0.10f, 0.13f, 1.0f});
        graphicsDevice.SetDepthEnabled(true);

        const std::filesystem::path basePath = SDL_GetBasePath();
        const std::string boxPath = (basePath / "Content/Models/Box.glb").generic_string();
        boxModel = std::make_unique<Lucky::Model>(graphicsDevice, boxPath);
        boxInstance = std::make_unique<Lucky::ModelInstance>(*boxModel);

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

        Lucky::Light fill;
        fill.type = Lucky::LightType::Point;
        fill.position = {2.5f, 1.0f, -1.5f};
        fill.color = {0.4f, 0.6f, 1.0f};
        fill.intensity = 3.0f;
        fill.range = 6.0f;
        scene.lights.push_back(fill);
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
        boxRotation += deltaSeconds * 0.6f;
        const glm::mat4 t = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.6f, 0.0f));
        const glm::mat4 r = glm::rotate(glm::mat4(1.0f), boxRotation, glm::vec3(0.3f, 1.0f, 0.0f));
        boxInstance->SetRootTransform(t * r);

        // Rebuild the per-frame draw list: ground plane + the box's
        // ModelInstance flattened to one SceneObject per (node, mesh).
        scene.objects.clear();
        scene.objects.push_back({planeMesh.get(),
            glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -0.5f, 0.0f)),
            glm::vec3(0.6f, 0.6f, 0.65f)});
        boxInstance->AppendToScene(scene, glm::vec3(0.85f, 0.45f, 0.30f));
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
    float boxRotation = 0.0f;
};

} // namespace

LUCKY_DEMO("3D",
    "Khronos Box.glb on a ground plane, forward-shaded with directional + point lights", Demo3D);
