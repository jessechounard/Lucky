#include <SDL3/SDL.h>
#include <cmath>
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

// Pose-blend demo. Samples two snapshots from CesiumMan's walk cycle
// (start of cycle vs. mid-cycle, where the legs are roughly mirrored)
// and oscillates the blend weight with a slow cosine. No PlayAnimation
// call -- motion is synthesized entirely from two static poses plus a
// gameplay-driven weight, which is the building block for pose-based
// procedural animation.
class PoseBlendDemo : public LuckyDemos::DemoBase {
  public:
    explicit PoseBlendDemo(SDL_Window *window)
        : graphicsDevice(window), forwardRenderer(graphicsDevice) {
        graphicsDevice.SetClearColor({0.08f, 0.10f, 0.13f, 1.0f});
        graphicsDevice.SetDepthEnabled(true);

        const std::filesystem::path basePath = SDL_GetBasePath();
        const std::string modelPath = (basePath / "Content/Models/CesiumMan.glb").generic_string();
        manModel = std::make_unique<Lucky::Model>(graphicsDevice, modelPath);
        manInstance = std::make_unique<Lucky::ModelInstance>(*manModel);

        // Capture two key poses from the walk animation. Sampling at
        // t=0 and t=duration/2 gives roughly opposite leg positions
        // for most loop-friendly walk cycles. If the asset shipped no
        // animations, fall back to the rest pose for both -- the demo
        // still runs but won't visibly move.
        if (manModel->GetAnimationCount() > 0) {
            const float duration = manModel->GetAnimation(0).duration;
            Lucky::SampleAnimationPose(*manModel, 0, 0.0f, poseA);
            Lucky::SampleAnimationPose(*manModel, 0, duration * 0.5f, poseB);
        } else {
            Lucky::MakeRestPose(*manModel, poseA);
            Lucky::MakeRestPose(*manModel, poseB);
        }

        planeMesh = std::make_unique<Lucky::Mesh>(graphicsDevice, Lucky::MakePlaneMeshData(8.0f));

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
        // Drive the blend weight with a slow cosine. 0.5 Hz = one
        // full A->B->A cycle every two seconds; slow enough that the
        // intermediate poses are clearly visible without strobing.
        time += deltaSeconds;
        const float frequency = 0.5f;
        const float weight = 0.5f - 0.5f * std::cos(time * frequency * 2.0f * glm::pi<float>());

        Lucky::BlendPoses(poseA, poseB, weight, blended);
        manInstance->SetPose(blended);

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

    Lucky::AnimationPose poseA;
    Lucky::AnimationPose poseB;
    Lucky::AnimationPose blended;

    Lucky::Camera camera;
    Lucky::Scene3D scene;

    float time = 0.0f;
};

} // namespace

LUCKY_DEMO("PoseBlend",
    "Procedural motion synthesized by oscillating between two pose snapshots from CesiumMan",
    PoseBlendDemo);
