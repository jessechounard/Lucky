#include <algorithm>
#include <cmath>

#include <SDL3/SDL_assert.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

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
    playbacks.assign(model.GetAnimationCount(), AnimationPlayback{});

    // Size the joint matrix storage to match the model's skins. The
    // outer vector never resizes again, so `&skinJointMatrices[i]`
    // remains stable for the instance's lifetime -- which lets
    // `SkinnedSceneObject::jointMatrices` borrow it safely.
    const int skinCount = model.GetSkinCount();
    skinJointMatrices.resize(skinCount);
    for (int s = 0; s < skinCount; s++) {
        skinJointMatrices[s].assign(model.GetSkin(s).jointNodes.size(), glm::mat4(1.0f));
    }
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

void ModelInstance::PlayAnimation(int index, bool loop) {
    if (index < 0 || index >= static_cast<int>(playbacks.size())) {
        return;
    }
    AnimationPlayback &pb = playbacks[index];
    pb.time = 0.0f;
    pb.playing = true;
    pb.loop = loop;
}

void ModelInstance::StopAnimation(int index) {
    if (index < 0 || index >= static_cast<int>(playbacks.size())) {
        return;
    }
    playbacks[index].playing = false;
}

void ModelInstance::StopAllAnimations() {
    for (AnimationPlayback &pb : playbacks) {
        pb.playing = false;
    }
}

namespace {

// Binary search for the keyframe pair bracketing `t`. Returns the index
// `i` such that keyframes[i].time <= t < keyframes[i+1].time. If `t`
// falls outside the keyframe range, returns the boundary index (caller
// handles clamp).
int FindKeyframe(const std::vector<AnimationKeyframe> &keyframes, float t) {
    if (keyframes.size() < 2) {
        return 0;
    }
    if (t <= keyframes.front().time) {
        return 0;
    }
    if (t >= keyframes.back().time) {
        return static_cast<int>(keyframes.size()) - 1;
    }
    int lo = 0;
    int hi = static_cast<int>(keyframes.size()) - 1;
    while (lo + 1 < hi) {
        const int mid = (lo + hi) / 2;
        if (keyframes[mid].time <= t) {
            lo = mid;
        } else {
            hi = mid;
        }
    }
    return lo;
}

// Sample one channel at time `t`, writing the result into the
// referenced node's TRS slot. Returns true if a write occurred.
bool SampleChannel(const AnimationChannel &channel, float t, std::vector<glm::vec3> &translations,
    std::vector<glm::quat> &rotations, std::vector<glm::vec3> &scales) {
    const auto &kfs = channel.keyframes;
    if (kfs.empty()) {
        return false;
    }
    if (channel.nodeIndex < 0 || channel.nodeIndex >= static_cast<int>(translations.size())) {
        return false;
    }

    const int idx = FindKeyframe(kfs, t);
    const int next = std::min(idx + 1, static_cast<int>(kfs.size()) - 1);
    float factor = 0.0f;
    if (idx != next) {
        const float t0 = kfs[idx].time;
        const float t1 = kfs[next].time;
        factor = (t1 > t0) ? (t - t0) / (t1 - t0) : 0.0f;
    }
    if (channel.interpolation == AnimationInterpolation::Step) {
        factor = 0.0f;
    }
    // CubicSpline accepted but treated as Linear for now -- producing
    // a smoother curve would require sampling the in/out tangent
    // values that cgltf interleaves alongside the keyframe data.

    const glm::vec4 &v0 = kfs[idx].value;
    const glm::vec4 &v1 = kfs[next].value;

    switch (channel.path) {
    case AnimationPath::Translation: {
        const glm::vec3 a(v0.x, v0.y, v0.z);
        const glm::vec3 b(v1.x, v1.y, v1.z);
        translations[channel.nodeIndex] = glm::mix(a, b, factor);
        break;
    }
    case AnimationPath::Scale: {
        const glm::vec3 a(v0.x, v0.y, v0.z);
        const glm::vec3 b(v1.x, v1.y, v1.z);
        scales[channel.nodeIndex] = glm::mix(a, b, factor);
        break;
    }
    case AnimationPath::Rotation: {
        // glTF stores quaternion components as (x, y, z, w);
        // glm::quat ctor takes (w, x, y, z).
        const glm::quat q0(v0.w, v0.x, v0.y, v0.z);
        const glm::quat q1(v1.w, v1.x, v1.y, v1.z);
        rotations[channel.nodeIndex] = glm::slerp(q0, q1, factor);
        break;
    }
    }
    return true;
}

} // namespace

void ModelInstance::Update(float deltaTime) {
    bool any = false;
    const int animCount = model->GetAnimationCount();
    for (int i = 0; i < animCount; i++) {
        AnimationPlayback &pb = playbacks[i];
        if (!pb.playing) {
            continue;
        }
        const AnimationDef &anim = model->GetAnimation(i);

        pb.time += deltaTime;
        if (pb.time > anim.duration) {
            if (pb.loop && anim.duration > 0.0f) {
                pb.time = std::fmod(pb.time, anim.duration);
            } else {
                pb.time = anim.duration;
                pb.playing = false;
            }
        }

        for (const AnimationChannel &channel : anim.channels) {
            if (SampleChannel(channel, pb.time, translations, rotations, scales)) {
                any = true;
            }
        }
    }
    if (any) {
        dirty = true;
    }
}

const std::vector<glm::mat4> &ModelInstance::GetWorldTransforms() const {
    RecomputeWorldTransformsIfDirty();
    return worldTransforms;
}

const std::vector<glm::mat4> &ModelInstance::GetSkinJointMatrices(int skinIndex) const {
    SDL_assert(skinIndex >= 0 && skinIndex < static_cast<int>(skinJointMatrices.size()));
    RecomputeWorldTransformsIfDirty();
    return skinJointMatrices[skinIndex];
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

    // Refresh per-skin joint matrices alongside the world transforms.
    // Each joint's matrix maps mesh-local space straight into world
    // space, so the skinned vertex shader doesn't need a separate model
    // transform.
    const int skinCount = model->GetSkinCount();
    for (int s = 0; s < skinCount; s++) {
        const Skin &skin = model->GetSkin(s);
        std::vector<glm::mat4> &out = skinJointMatrices[s];
        const size_t jointCount = skin.jointNodes.size();
        for (size_t j = 0; j < jointCount; j++) {
            out[j] = worldTransforms[skin.jointNodes[j]] * skin.inverseBindMatrices[j];
        }
    }

    dirty = false;
}

void ModelInstance::AppendToScene(Scene3D &scene, const glm::vec3 &colorTint) {
    RecomputeWorldTransformsIfDirty();
    const int count = model->GetNodeCount();
    for (int i = 0; i < count; i++) {
        const NodeTemplate &node = model->GetNode(i);
        for (int meshIdx : node.meshIndices) {
            SceneObject obj;
            obj.mesh = &model->GetMesh(meshIdx);
            obj.material = model->GetMaterialForMesh(meshIdx);
            obj.transform = worldTransforms[i];
            obj.color = colorTint;
            scene.objects.push_back(obj);
        }
        // Skinned primitives. Skip silently if the source glTF
        // attached a skinned mesh to a node with no skin reference --
        // that's malformed but seen in the wild.
        if (node.skinIndex < 0) {
            continue;
        }
        for (int meshIdx : node.skinnedMeshIndices) {
            SkinnedSceneObject obj;
            obj.mesh = &model->GetSkinnedMesh(meshIdx);
            obj.material = model->GetMaterialForSkinnedMesh(meshIdx);
            obj.color = colorTint;
            obj.jointMatrices = &skinJointMatrices[node.skinIndex];
            scene.skinnedObjects.push_back(obj);
        }
    }
}

} // namespace Lucky
