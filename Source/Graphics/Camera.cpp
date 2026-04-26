#include <SDL3/SDL_assert.h>
#include <algorithm>
#include <cmath>

#include <glm/gtc/matrix_transform.hpp>

#include <Lucky/Camera.hpp>

namespace Lucky {

namespace {

// Just shy of pi/2 so the lookAt up vector never collinearizes with forward.
constexpr float PitchLimit = 1.5533430343f; // pi/2 - 0.0174 (~89 deg)

glm::vec3 ForwardFromAngles(float yaw, float pitch) {
    const float cosP = std::cos(pitch);
    const float sinP = std::sin(pitch);
    const float cosY = std::cos(yaw);
    const float sinY = std::sin(yaw);
    return {cosP * -sinY, sinP, cosP * -cosY};
}

} // namespace

glm::vec3 Camera::GetForward() const {
    return ForwardFromAngles(yaw, pitch);
}

glm::vec3 Camera::GetRight() const {
    const glm::vec3 worldUp(0.0f, 1.0f, 0.0f);
    return glm::normalize(glm::cross(GetForward(), worldUp));
}

glm::vec3 Camera::GetUp() const {
    return glm::normalize(glm::cross(GetRight(), GetForward()));
}

glm::mat4 Camera::GetViewMatrix() const {
    const glm::vec3 worldUp(0.0f, 1.0f, 0.0f);
    return glm::lookAt(position, position + GetForward(), worldUp);
}

glm::mat4 Camera::GetProjectionMatrix(float aspectRatio) const {
    SDL_assert(aspectRatio > 0.0f);
    SDL_assert(zFar > zNear);
    return glm::perspective(fovY, aspectRatio, zNear, zFar);
}

void UpdateFlyCamera(Camera &camera, const FlyCameraInput &input, float deltaTime) {
    camera.yaw += input.yawDelta;
    camera.pitch = std::clamp(camera.pitch + input.pitchDelta, -PitchLimit, PitchLimit);

    const float lengthSq = glm::dot(input.moveLocal, input.moveLocal);
    if (lengthSq <= 0.0f) {
        return;
    }

    const glm::vec3 forward = ForwardFromAngles(camera.yaw, 0.0f);
    const glm::vec3 right(std::cos(camera.yaw), 0.0f, -std::sin(camera.yaw));
    const glm::vec3 worldUp(0.0f, 1.0f, 0.0f);

    const glm::vec3 dir = glm::normalize(
        input.moveLocal.x * right + input.moveLocal.y * worldUp + input.moveLocal.z * forward);
    camera.position += dir * (input.moveSpeed * deltaTime);
}

void UpdateOrbitCamera(Camera &camera, OrbitCameraState &state, const OrbitCameraInput &input) {
    SDL_assert(input.minDistance > 0.0f);
    SDL_assert(input.maxDistance >= input.minDistance);

    camera.yaw += input.yawDelta;
    camera.pitch = std::clamp(camera.pitch + input.pitchDelta, -PitchLimit, PitchLimit);

    state.distance =
        std::clamp(state.distance * input.distanceScale, input.minDistance, input.maxDistance);

    // Position the camera by walking from the target backwards along the
    // forward vector by `distance` -- equivalent to looking from
    // `target - forward * distance` toward `target`.
    camera.position = state.target - ForwardFromAngles(camera.yaw, camera.pitch) * state.distance;
}

} // namespace Lucky
