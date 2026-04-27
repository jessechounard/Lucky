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

// Textured PBR demo: Khronos BoomBox.glb on a ground plane. Unlike
// MaterialDemo (which uses per-sphere factors only), this asset has
// embedded base-color and metallic-roughness textures, so this is the
// demo that actually exercises the fragment shader's texture-sampling
// path. BoomBox also ships normal, emissive, and AO maps -- those are
// Tier-3-and-beyond features the renderer ignores for now, so the
// model looks softer than reference but materially correct.
class TexturedModelDemo : public LuckyDemos::DemoBase {
  public:
    explicit TexturedModelDemo(SDL_Window *window)
        : graphicsDevice(window), forwardRenderer(graphicsDevice) {
        graphicsDevice.SetClearColor({0.08f, 0.10f, 0.13f, 1.0f});
        graphicsDevice.SetDepthEnabled(true);

        const std::filesystem::path basePath = SDL_GetBasePath();
        const std::string modelPath = (basePath / "Content/Models/BoomBox.glb").generic_string();
        boomboxModel = std::make_unique<Lucky::Model>(graphicsDevice, modelPath);
        boomboxInstance = std::make_unique<Lucky::ModelInstance>(*boomboxModel);

        planeMesh = std::make_unique<Lucky::Mesh>(graphicsDevice, Lucky::MakePlaneMeshData(8.0f));

        camera.position = {0.0f, 1.5f, 4.0f};
        camera.fovY = glm::radians(45.0f);
        camera.zNear = 0.1f;
        camera.zFar = 50.0f;
        const glm::vec3 toOrigin = glm::normalize(-camera.position);
        camera.yaw = std::atan2(-toOrigin.x, -toOrigin.z);
        camera.pitch = std::asin(glm::clamp(toOrigin.y, -1.0f, 1.0f));

        scene.ambientColor = {0.12f, 0.13f, 0.15f};

        Lucky::Light sun;
        sun.type = Lucky::LightType::Directional;
        sun.direction = glm::normalize(glm::vec3(-0.4f, -0.8f, -0.3f));
        sun.color = {1.0f, 0.95f, 0.85f};
        sun.intensity = 0.8f;
        sun.castsShadows = true;
        scene.lights.push_back(sun);

        Lucky::Light fill;
        fill.type = Lucky::LightType::Point;
        fill.position = {1.5f, 1.5f, 1.0f};
        fill.color = {0.6f, 0.7f, 1.0f};
        fill.intensity = 1.0f;
        fill.range = 5.0f;
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
        rotation += deltaSeconds * 0.4f;
        // BoomBox is authored in real-world meters (~10cm), so scale up
        // significantly to be visible at this camera distance.
        const float scale = 60.0f;
        const glm::mat4 t = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.6f, 0.0f));
        const glm::mat4 r = glm::rotate(glm::mat4(1.0f), rotation, glm::vec3(0.0f, 1.0f, 0.0f));
        const glm::mat4 s = glm::scale(glm::mat4(1.0f), glm::vec3(scale));
        boomboxInstance->SetRootTransform(t * r * s);

        scene.objects.clear();
        Lucky::SceneObject ground;
        ground.mesh = planeMesh.get();
        ground.transform = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -0.5f, 0.0f));
        ground.color = glm::vec3(0.55f, 0.55f, 0.6f);
        scene.objects.push_back(ground);

        boomboxInstance->AppendToScene(scene, glm::vec3(1.0f));
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

    std::unique_ptr<Lucky::Model> boomboxModel;
    std::unique_ptr<Lucky::ModelInstance> boomboxInstance;
    std::unique_ptr<Lucky::Mesh> planeMesh;

    Lucky::Camera camera;
    Lucky::Scene3D scene;
    float rotation = 0.0f;
};

} // namespace

LUCKY_DEMO("Textured Model",
    "BoomBox.glb with base-color and metallic-roughness textures (PBR end-to-end test)",
    TexturedModelDemo);
