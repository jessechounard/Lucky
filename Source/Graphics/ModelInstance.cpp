#include <SDL3/SDL_assert.h>

#include <glm/gtc/matrix_transform.hpp>

#include <Lucky/Model.hpp>
#include <Lucky/ModelInstance.hpp>
#include <Lucky/Scene3D.hpp>

namespace Lucky {

ModelInstance::ModelInstance(Model &model) : model(&model) {
    const int count = model.GetNodeCount();
    translations.reserve(count);
    rotations.reserve(count);
    scales.reserve(count);
    for (int i = 0; i < count; i++) {
        const NodeTemplate &node = model.GetNode(i);
        translations.push_back(node.restTranslation);
        rotations.push_back(node.restRotation);
        scales.push_back(node.restScale);
    }
    worldTransforms.assign(count, glm::mat4(1.0f));
}

void ModelInstance::SetRootTransform(const glm::mat4 &transform) {
    rootTransform = transform;
    dirty = true;
}

void ModelInstance::SetNodeTransform(int nodeIndex, const glm::vec3 &translation,
    const glm::quat &rotation, const glm::vec3 &scale) {
    SDL_assert(nodeIndex >= 0 && nodeIndex < static_cast<int>(translations.size()));
    translations[nodeIndex] = translation;
    rotations[nodeIndex] = rotation;
    scales[nodeIndex] = scale;
    dirty = true;
}

void ModelInstance::ResetToRestPose() {
    const int count = model->GetNodeCount();
    for (int i = 0; i < count; i++) {
        const NodeTemplate &node = model->GetNode(i);
        translations[i] = node.restTranslation;
        rotations[i] = node.restRotation;
        scales[i] = node.restScale;
    }
    dirty = true;
}

const std::vector<glm::mat4> &ModelInstance::GetWorldTransforms() const {
    RecomputeWorldTransformsIfDirty();
    return worldTransforms;
}

void ModelInstance::RecomputeWorldTransformsIfDirty() const {
    if (!dirty) {
        return;
    }

    const int count = model->GetNodeCount();
    std::vector<bool> computed(count, false);

    auto compute = [&](auto &self, int i) -> void {
        if (computed[i]) {
            return;
        }
        const NodeTemplate &n = model->GetNode(i);
        const glm::mat4 local = glm::translate(glm::mat4(1.0f), translations[i]) *
                                glm::mat4_cast(rotations[i]) *
                                glm::scale(glm::mat4(1.0f), scales[i]);
        if (n.parentIndex < 0) {
            worldTransforms[i] = rootTransform * local;
        } else {
            self(self, n.parentIndex);
            worldTransforms[i] = worldTransforms[n.parentIndex] * local;
        }
        computed[i] = true;
    };
    for (int i = 0; i < count; i++) {
        compute(compute, i);
    }

    dirty = false;
}

void ModelInstance::AppendToScene(Scene3D &scene, const glm::vec3 &colorTint) const {
    RecomputeWorldTransformsIfDirty();
    const int count = model->GetNodeCount();
    for (int i = 0; i < count; i++) {
        const NodeTemplate &node = model->GetNode(i);
        for (int meshIdx : node.meshIndices) {
            scene.objects.push_back({&model->GetMesh(meshIdx), worldTransforms[i], colorTint});
        }
    }
}

} // namespace Lucky
