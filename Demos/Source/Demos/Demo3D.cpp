#include <SDL3/SDL.h>
#include <memory>

#include <glm/gtc/matrix_transform.hpp>

#include <Lucky/Camera.hpp>
#include <Lucky/ForwardRenderer.hpp>
#include <Lucky/GraphicsDevice.hpp>
#include <Lucky/Mesh.hpp>
#include <Lucky/Scene3D.hpp>

#include "../DemoBase.hpp"
#include "../DemoRegistry.hpp"

namespace {

// Minimal 3D demo: a spinning octahedron ("diamond") on a generated
// ground plane, lit by a directional sun with shadow mapping. Pure
// procedural geometry -- no glTF loading, no texture sampling. Use this
// to validate that the basic 3D pipeline path is healthy without any
// asset dependency.
class Demo3D : public LuckyDemos::DemoBase {
  public:
    explicit Demo3D(SDL_Window *window) : graphicsDevice(window), forwardRenderer(graphicsDevice) {
        graphicsDevice.SetClearColor({0.08f, 0.10f, 0.13f, 1.0f});
        graphicsDevice.SetDepthEnabled(true);

        diamondMesh =
            std::make_unique<Lucky::Mesh>(graphicsDevice, Lucky::MakeDiamondMeshData(1.0f, 1.5f));
        planeMesh = std::make_unique<Lucky::Mesh>(graphicsDevice, Lucky::MakePlaneMeshData(8.0f));
        // Thin vertical slab as a peter-panning probe: its bottom edge
        // sits exactly on the ground plane (Y=-0.5), so the contact
        // line should have zero gap between the slab and its shadow.
        // If the shadow bias is too aggressive we'll see a sliver of
        // light right at the base.
        slabMesh = std::make_unique<Lucky::Mesh>(
            graphicsDevice, Lucky::MakeBoxMeshData(0.08f, 1.5f, 1.5f));

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
        // Low-angled sun so the slabs cast long shadows across the
        // ground -- makes the slab/shadow contact line easy to see for
        // peter-panning and acne inspection.
        sun.direction = glm::normalize(glm::vec3(-0.7f, -0.45f, -0.3f));
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
        diamondRotation += deltaSeconds * 0.6f;

        scene.objects.clear();

        Lucky::SceneObject ground;
        ground.mesh = planeMesh.get();
        ground.transform = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -0.5f, 0.0f));
        ground.color = glm::vec3(0.6f, 0.6f, 0.65f);
        scene.objects.push_back(ground);

        Lucky::SceneObject diamond;
        diamond.mesh = diamondMesh.get();
        const glm::mat4 t = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 1.2f, 0.0f));
        const glm::mat4 r =
            glm::rotate(glm::mat4(1.0f), diamondRotation, glm::vec3(0.0f, 1.0f, 0.0f));
        diamond.transform = t * r;
        diamond.color = glm::vec3(0.85f, 0.45f, 0.30f);
        scene.objects.push_back(diamond);

        // Two slabs on either side of the diamond, with their bottom
        // edges touching the ground (slab half-height = 0.75, ground at
        // Y=-0.5, so slab center sits at Y=0.25). Watch for a light
        // gap at the base of either slab.
        Lucky::SceneObject slabLeft;
        slabLeft.mesh = slabMesh.get();
        slabLeft.transform = glm::translate(glm::mat4(1.0f), glm::vec3(-2.5f, 0.25f, 0.0f));
        slabLeft.color = glm::vec3(0.7f, 0.7f, 0.75f);
        scene.objects.push_back(slabLeft);

        Lucky::SceneObject slabRight;
        slabRight.mesh = slabMesh.get();
        slabRight.transform =
            glm::rotate(glm::translate(glm::mat4(1.0f), glm::vec3(2.5f, 0.25f, 0.0f)),
                glm::radians(45.0f),
                glm::vec3(0.0f, 1.0f, 0.0f));
        slabRight.color = glm::vec3(0.7f, 0.7f, 0.75f);
        scene.objects.push_back(slabRight);
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

    std::unique_ptr<Lucky::Mesh> diamondMesh;
    std::unique_ptr<Lucky::Mesh> planeMesh;
    std::unique_ptr<Lucky::Mesh> slabMesh;

    Lucky::Camera camera;
    Lucky::Scene3D scene;
    float diamondRotation = 0.0f;
};

} // namespace

LUCKY_DEMO(
    "3D", "Spinning diamond on a ground plane with a directional sun and shadow mapping", Demo3D);
