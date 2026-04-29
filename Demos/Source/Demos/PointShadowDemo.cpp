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

// Cube-shadow demo: a shadow-casting point light orbits a ring of
// pillar-cubes on a ground plane, casting omnidirectional shadows that
// sweep across the floor and onto the cubes themselves. Exists to
// exercise the new point-light cube shadow path -- the old TexturedModel
// and PoseBlend demos both use directional + point fill lights without
// shadow casting on the points.
class PointShadowDemo : public LuckyDemos::DemoBase {
  public:
    explicit PointShadowDemo(SDL_Window *window)
        : graphicsDevice(window), forwardRenderer(graphicsDevice) {
        graphicsDevice.SetClearColor({0.04f, 0.05f, 0.08f, 1.0f});
        graphicsDevice.SetDepthEnabled(true);

        planeMesh = std::make_unique<Lucky::Mesh>(graphicsDevice, Lucky::MakePlaneMeshData(20.0f));
        cubeMesh = std::make_unique<Lucky::Mesh>(graphicsDevice, Lucky::MakeCubeMeshData(1.0f));

        camera.position = {0.0f, 5.5f, 8.0f};
        camera.fovY = glm::radians(50.0f);
        camera.zNear = 0.1f;
        camera.zFar = 100.0f;
        const glm::vec3 toOrigin = glm::normalize(-camera.position);
        camera.yaw = std::atan2(-toOrigin.x, -toOrigin.z);
        camera.pitch = std::asin(glm::clamp(toOrigin.y, -1.0f, 1.0f));

        scene.ambientColor = {0.04f, 0.04f, 0.05f};

        // Dim directional fill so unshadowed surfaces aren't pitch
        // black, but weak enough that the point light's shadow is the
        // dominant visual signal.
        Lucky::Light fill;
        fill.type = Lucky::LightType::Directional;
        fill.direction = glm::normalize(glm::vec3(-0.3f, -1.0f, -0.4f));
        fill.color = {0.45f, 0.5f, 0.6f};
        fill.intensity = 0.25f;
        fill.castsShadows = false;
        scene.lights.push_back(fill);

        // The point caster: warm-orange light suspended above the
        // ring, casting cube shadows. Range chosen to comfortably
        // cover the floor and pillars without falling off the
        // shadow map's far plane.
        Lucky::Light point;
        point.type = Lucky::LightType::Point;
        point.position = {0.0f, 3.0f, 0.0f};
        point.color = {1.0f, 0.7f, 0.4f};
        point.intensity = 8.0f;
        point.range = 14.0f;
        point.castsShadows = true;
        scene.lights.push_back(point);
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
        time += deltaSeconds;

        // Orbit the point light in a horizontal circle so the cast
        // shadows visibly sweep across the floor. Vertical bob keeps
        // the cube-face transitions exercised across the y axis too.
        const float radius = 2.5f;
        scene.lights[1].position = {
            std::cos(time * 0.6f) * radius,
            3.0f + std::sin(time * 1.3f) * 0.6f,
            std::sin(time * 0.6f) * radius,
        };

        scene.objects.clear();

        // Ground.
        {
            Lucky::SceneObject ground;
            ground.mesh = planeMesh.get();
            ground.transform = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 0.0f));
            ground.color = {0.55f, 0.55f, 0.6f};
            scene.objects.push_back(ground);
        }

        // Six pillar-cubes arranged in a ring around the orbiting light.
        const int pillarCount = 6;
        for (int i = 0; i < pillarCount; i++) {
            const float angle = (static_cast<float>(i) / pillarCount) * 2.0f * 3.14159265f;
            const float r = 4.5f;
            const glm::vec3 pos(std::cos(angle) * r, 1.0f, std::sin(angle) * r);
            const glm::mat4 t = glm::translate(glm::mat4(1.0f), pos);
            const glm::mat4 s = glm::scale(glm::mat4(1.0f), glm::vec3(0.8f, 2.0f, 0.8f));

            Lucky::SceneObject pillar;
            pillar.mesh = cubeMesh.get();
            pillar.transform = t * s;
            // Alternate two pillar tints so individual shadows are
            // easier to attribute to a specific caster while debugging.
            pillar.color =
                (i % 2 == 0) ? glm::vec3(0.85f, 0.7f, 0.6f) : glm::vec3(0.6f, 0.7f, 0.85f);
            scene.objects.push_back(pillar);
        }
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

    std::unique_ptr<Lucky::Mesh> planeMesh;
    std::unique_ptr<Lucky::Mesh> cubeMesh;

    Lucky::Camera camera;
    Lucky::Scene3D scene;
    float time = 0.0f;
};

} // namespace

LUCKY_DEMO("Point Shadow",
    "Cube-shadow point light orbits a ring of pillar-cubes; tests omnidirectional shadows.",
    PointShadowDemo);
