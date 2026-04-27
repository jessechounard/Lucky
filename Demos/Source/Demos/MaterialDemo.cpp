#include <SDL3/SDL.h>
#include <filesystem>
#include <memory>

#include <glm/gtc/matrix_transform.hpp>

#include <Lucky/Camera.hpp>
#include <Lucky/ForwardRenderer.hpp>
#include <Lucky/GraphicsDevice.hpp>
#include <Lucky/Model.hpp>
#include <Lucky/ModelInstance.hpp>
#include <Lucky/Scene3D.hpp>

#include "../DemoBase.hpp"
#include "../DemoRegistry.hpp"

namespace {

// PBR validation demo: MetalRoughSpheres.glb (Khronos sample) lit by a
// directional sun and a blue point fill, no ground or shadows -- the
// grid sweeps metallic on one axis and roughness on the other so the
// Cook-Torrance BRDF math is verifiable against reference renderers.
class MaterialDemo : public LuckyDemos::DemoBase {
  public:
    explicit MaterialDemo(SDL_Window *window)
        : graphicsDevice(window), forwardRenderer(graphicsDevice) {
        graphicsDevice.SetClearColor({0.08f, 0.10f, 0.13f, 1.0f});
        graphicsDevice.SetDepthEnabled(true);

        const std::filesystem::path basePath = SDL_GetBasePath();
        const std::string modelPath =
            (basePath / "Content/Models/MetalRoughSpheres.glb").generic_string();
        loadedModel = std::make_unique<Lucky::Model>(graphicsDevice, modelPath);
        modelInstance = std::make_unique<Lucky::ModelInstance>(*loadedModel);

        camera.position = {0.0f, 0.0f, 18.0f};
        camera.fovY = glm::radians(45.0f);
        camera.zNear = 0.1f;
        camera.zFar = 100.0f;
        const glm::vec3 toOrigin = glm::normalize(-camera.position);
        camera.yaw = std::atan2(-toOrigin.x, -toOrigin.z);
        camera.pitch = std::asin(glm::clamp(toOrigin.y, -1.0f, 1.0f));

        scene.ambientColor = {0.10f, 0.11f, 0.13f};

        Lucky::Light sun;
        sun.type = Lucky::LightType::Directional;
        sun.direction = glm::normalize(glm::vec3(-0.4f, -1.0f, -0.3f));
        sun.color = {1.0f, 0.95f, 0.85f};
        sun.intensity = 1.0f;
        // No ground in this demo, so shadows have nothing useful to
        // fall on. Disabled to keep the sphere grid the only subject.
        sun.castsShadows = false;
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
        modelRotation += deltaSeconds * 0.2f;
        const glm::mat4 r =
            glm::rotate(glm::mat4(1.0f), modelRotation, glm::vec3(0.0f, 1.0f, 0.0f));
        modelInstance->SetRootTransform(r);

        scene.objects.clear();
        modelInstance->AppendToScene(scene, glm::vec3(1.0f));
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

    std::unique_ptr<Lucky::Model> loadedModel;
    std::unique_ptr<Lucky::ModelInstance> modelInstance;

    Lucky::Camera camera;
    Lucky::Scene3D scene;
    float modelRotation = 0.0f;
};

} // namespace

LUCKY_DEMO("Materials",
    "PBR validation grid: Khronos MetalRoughSpheres.glb with metallic x roughness sweep",
    MaterialDemo);
