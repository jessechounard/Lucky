#include <doctest/doctest.h>

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

#include <Lucky/Camera.hpp>

using namespace Lucky;

TEST_CASE("Camera at default yaw=0 pitch=0 looks down -Z") {
    Camera camera;
    glm::vec3 forward = camera.GetForward();
    CHECK(forward.x == doctest::Approx(0.0f));
    CHECK(forward.y == doctest::Approx(0.0f));
    CHECK(forward.z == doctest::Approx(-1.0f));
}

TEST_CASE("Camera right vector is perpendicular to forward and world up") {
    Camera camera;
    camera.yaw = 0.5f;
    camera.pitch = 0.3f;
    glm::vec3 forward = camera.GetForward();
    glm::vec3 right = camera.GetRight();
    CHECK(glm::dot(forward, right) == doctest::Approx(0.0f).epsilon(1e-5f));
    CHECK(right.y == doctest::Approx(0.0f).epsilon(1e-5f));
}

TEST_CASE("UpdateFlyCamera advances yaw and pitch by their deltas") {
    Camera camera;
    FlyCameraInput input;
    input.yawDelta = 0.1f;
    input.pitchDelta = 0.2f;
    UpdateFlyCamera(camera, input, 0.016f);
    CHECK(camera.yaw == doctest::Approx(0.1f));
    CHECK(camera.pitch == doctest::Approx(0.2f));
}

TEST_CASE("UpdateFlyCamera clamps pitch below pi/2") {
    Camera camera;
    FlyCameraInput input;
    input.pitchDelta = 100.0f;
    UpdateFlyCamera(camera, input, 0.016f);
    CHECK(camera.pitch < glm::half_pi<float>());
    CHECK(camera.pitch > 1.5f);
}

TEST_CASE("UpdateFlyCamera moveLocal.z translates along forward") {
    Camera camera; // yaw=0, pitch=0 -> forward = -Z
    FlyCameraInput input;
    input.moveLocal = {0.0f, 0.0f, 1.0f};
    input.moveSpeed = 2.0f;
    UpdateFlyCamera(camera, input, 0.5f); // 2 * 0.5 = 1 unit along forward
    CHECK(camera.position.x == doctest::Approx(0.0f));
    CHECK(camera.position.y == doctest::Approx(0.0f));
    CHECK(camera.position.z == doctest::Approx(4.0f)); // 5 + (-1)
}

TEST_CASE("UpdateFlyCamera moveLocal.y always moves along world up regardless of pitch") {
    Camera camera;
    camera.pitch = 0.5f; // looking up
    FlyCameraInput input;
    input.moveLocal = {0.0f, 1.0f, 0.0f};
    input.moveSpeed = 1.0f;
    UpdateFlyCamera(camera, input, 1.0f);
    CHECK(camera.position.x == doctest::Approx(0.0f));
    CHECK(camera.position.y == doctest::Approx(1.0f));
    CHECK(camera.position.z == doctest::Approx(5.0f));
}

TEST_CASE("UpdateOrbitCamera positions the camera at target - forward * distance") {
    Camera camera; // yaw=0, pitch=0 -> forward = -Z
    OrbitCameraState state;
    state.target = {0.0f, 0.0f, 0.0f};
    state.distance = 4.0f;
    OrbitCameraInput input; // no rotation, no zoom
    UpdateOrbitCamera(camera, state, input);
    // target - (-Z) * 4 = (0, 0, 4)
    CHECK(camera.position.x == doctest::Approx(0.0f));
    CHECK(camera.position.y == doctest::Approx(0.0f));
    CHECK(camera.position.z == doctest::Approx(4.0f));
}

TEST_CASE("UpdateOrbitCamera distanceScale clamps within min/max") {
    Camera camera;
    OrbitCameraState state;
    state.distance = 5.0f;
    OrbitCameraInput input;
    input.distanceScale = 0.01f;
    input.minDistance = 1.0f;
    input.maxDistance = 100.0f;
    UpdateOrbitCamera(camera, state, input);
    CHECK(state.distance == doctest::Approx(1.0f));

    state.distance = 5.0f;
    input.distanceScale = 1000.0f;
    UpdateOrbitCamera(camera, state, input);
    CHECK(state.distance == doctest::Approx(100.0f));
}
