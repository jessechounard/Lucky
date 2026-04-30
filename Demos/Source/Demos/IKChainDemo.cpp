#include <cmath>
#include <filesystem>
#include <vector>

#include <SDL3/SDL.h>
#include <SDL3/SDL_filesystem.h>

#include <glm/glm.hpp>

#include <Lucky/BatchRenderer.hpp>
#include <Lucky/Color.hpp>
#include <Lucky/GraphicsDevice.hpp>
#include <Lucky/Shader.hpp>
#include <Lucky/ShapeRenderer.hpp>

#include "../DemoBase.hpp"
#include "../DemoRegistry.hpp"

namespace {

// Returns a unit vector from `from` to `to`, falling back to `fallback` if
// the two points are too close to safely normalize. Adjacent FABRIK joints
// can briefly coincide during the pose blend, which would otherwise produce
// a NaN when normalized.
glm::vec2 SafeDirection(glm::vec2 from, glm::vec2 to, glm::vec2 fallback) {
    const glm::vec2 delta = to - from;
    const float lengthSquared = glm::dot(delta, delta);
    if (lengthSquared < 1e-8f) {
        return fallback;
    }
    return delta * (1.0f / std::sqrt(lengthSquared));
}

// Projects each joint forward from its parent at the segment's fixed length.
// Used both as the second half of FABRIK and as a cleanup pass after the
// pose blend, which lerps positions and so temporarily violates lengths.
void EnforceSegmentLengths(
    std::vector<glm::vec2> &joints, const std::vector<float> &lengths, glm::vec2 anchor) {
    joints[0] = anchor;
    for (size_t i = 1; i < joints.size(); i++) {
        const glm::vec2 dir = SafeDirection(joints[i - 1], joints[i], glm::vec2(0.0f, 1.0f));
        joints[i] = joints[i - 1] + dir * lengths[i - 1];
    }
}

// Standard FABRIK: alternating forward (end -> root) and backward (root -> end)
// passes that drag each joint along the segment toward its neighbor, then
// enforce each segment's fixed length. Converges in a handful of iterations
// for short chains; ten is overkill but cheap.
void SolveFabrik(std::vector<glm::vec2> &joints, const std::vector<float> &lengths,
    glm::vec2 anchor, glm::vec2 target, int iterations) {
    const size_t end = joints.size() - 1;

    for (int iter = 0; iter < iterations; iter++) {
        // Forward pass: pin the end effector to the target and walk back
        // toward the root, snapping each joint to its child's segment.
        joints[end] = target;
        for (size_t i = end; i-- > 0;) {
            const glm::vec2 dir = SafeDirection(joints[i + 1], joints[i], glm::vec2(0.0f, -1.0f));
            joints[i] = joints[i + 1] + dir * lengths[i];
        }

        // Backward pass: re-pin the root and walk forward, restoring the
        // anchor constraint that the forward pass just broke.
        EnforceSegmentLengths(joints, lengths, anchor);
    }
}

// Two-DOF planar IK chain anchored at the bottom-center of the screen.
// Left-click (or click-drag) sets the target; FABRIK solves the goal pose
// each frame, and the displayed pose eases toward it with a frame-rate
// independent lerp so the chain visibly swings rather than snaps. A length
// constraint pass after the lerp keeps segments rigid during the blend.
class IKChainDemo : public LuckyDemos::DemoBase {
  public:
    explicit IKChainDemo(SDL_Window *window)
        : graphicsDevice(window), batchRenderer(graphicsDevice, 256), shapeRenderer(batchRenderer),
          sdfShader(graphicsDevice,
              (std::filesystem::path(SDL_GetBasePath()) / "Content/Shaders/sdf_outline.frag")
                  .generic_string(),
              SDL_GPU_SHADERSTAGE_FRAGMENT) {
        graphicsDevice.SetClearColor({0.06f, 0.07f, 0.10f, 1.0f});

        const int segmentCount = 6;
        const float segmentLength = 55.0f;

        segmentLengths.assign(segmentCount, segmentLength);

        // Initial pose: chain pointing straight up from the anchor. The
        // anchor itself gets re-pinned to the bottom-center each frame in
        // Update() so resizing the window doesn't leave it floating.
        const glm::vec2 initialAnchor(
            graphicsDevice.GetScreenWidth() * 0.5f, anchorHeightAboveBottom);
        displayedJoints.resize(segmentCount + 1);
        solvedJoints.resize(segmentCount + 1);
        for (size_t i = 0; i < displayedJoints.size(); i++) {
            displayedJoints[i] = initialAnchor + glm::vec2(0.0f, segmentLength * i);
        }
        target = displayedJoints.back();
    }

    void HandleEvent(const SDL_Event &event) override {
        switch (event.type) {
        case SDL_EVENT_KEY_DOWN:
            if (!event.key.repeat) {
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
            break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            if (event.button.button == SDL_BUTTON_LEFT) {
                target = MouseToWorld(event.button.x, event.button.y);
            }
            break;
        case SDL_EVENT_MOUSE_MOTION:
            // Allow click-drag for continuous targeting -- much easier to
            // explore the chain's reach than re-clicking.
            if ((event.motion.state & SDL_BUTTON_LMASK) != 0) {
                target = MouseToWorld(event.motion.x, event.motion.y);
            }
            break;
        }
    }

    void Update(float deltaSeconds) override {
        const glm::vec2 anchor(graphicsDevice.GetScreenWidth() * 0.5f, anchorHeightAboveBottom);

        // Seed the solver with the current displayed pose so it converges
        // from where the chain visibly is, not from a fresh straight pose.
        solvedJoints = displayedJoints;
        SolveFabrik(solvedJoints, segmentLengths, anchor, target, 10);

        // Frame-rate-independent ease toward the solved pose. Equivalent to
        // an exponential decay; doubles the blend speed for halved timestep.
        const float blendSpeed = 8.0f;
        const float blendAlpha = 1.0f - std::exp(-blendSpeed * deltaSeconds);
        for (size_t i = 0; i < displayedJoints.size(); i++) {
            displayedJoints[i] = glm::mix(displayedJoints[i], solvedJoints[i], blendAlpha);
        }

        // Positional lerp shortens segments mid-blend; this restores them
        // without disturbing the visible direction the lerp produced.
        EnforceSegmentLengths(displayedJoints, segmentLengths, anchor);
    }

    void Draw() override {
        graphicsDevice.BeginFrame();
        if (!graphicsDevice.GetCommandBuffer()) {
            return;
        }

        shapeRenderer.Begin(Lucky::BlendMode::Alpha, sdfShader);

        // Bones first, so joint circles sit on top of the segment ends.
        for (size_t i = 1; i < displayedJoints.size(); i++) {
            shapeRenderer.DrawLine(
                displayedJoints[i - 1], displayedJoints[i], 0.0f, Lucky::Color::White, 4.0f);
        }

        // Joints: anchor in cornflower blue, intermediate joints white,
        // end effector green so the "hand" of the arm is visually obvious.
        for (size_t i = 0; i < displayedJoints.size(); i++) {
            Lucky::Color color = Lucky::Color::White;
            float radius = 6.0f;
            if (i == 0) {
                color = Lucky::Color::CornflowerBlue;
                radius = 9.0f;
            } else if (i == displayedJoints.size() - 1) {
                color = Lucky::Color::Green;
                radius = 8.0f;
            }
            shapeRenderer.DrawCircle(displayedJoints[i], radius, color, 2.0f);
        }

        // Target reticle: a red ring with a smaller inner ring so it reads
        // as a goal even when the end effector lands on top of it.
        shapeRenderer.DrawCircle(target, 12.0f, Lucky::Color::Red, 2.0f);
        shapeRenderer.DrawCircle(target, 4.0f, Lucky::Color::Red, 2.0f);

        shapeRenderer.End();

        graphicsDevice.EndFrame();
    }

  private:
    glm::vec2 MouseToWorld(float mouseX, float mouseY) const {
        // BatchRenderer uses a Y-up ortho projection; SDL mouse coords are
        // Y-down. Flip Y to convert.
        return glm::vec2(mouseX, graphicsDevice.GetScreenHeight() - mouseY);
    }

    Lucky::GraphicsDevice graphicsDevice;
    Lucky::BatchRenderer batchRenderer;
    Lucky::ShapeRenderer shapeRenderer;
    Lucky::Shader sdfShader;

    std::vector<glm::vec2> displayedJoints;
    std::vector<glm::vec2> solvedJoints;
    std::vector<float> segmentLengths;
    glm::vec2 target{0.0f, 0.0f};

    static constexpr float anchorHeightAboveBottom = 40.0f;
};

} // namespace

LUCKY_DEMO("IKChain",
    "FABRIK-solved chain anchored at bottom; left-click or drag to set the IK target.",
    IKChainDemo);
