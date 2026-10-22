#include "animation.hpp"
#include "core/logger.hpp"
#include "rendering/geometry/mesh.hpp"
#include <assimp/anim.h>
#include <cmath>

namespace togr {

Animation::Animation(const aiAnimation* animation, const std::vector<Node>& nodes) {
    if (!animation) return;
    name = animation->mName.C_Str();
    valid = true;
    duration = animation->mDuration;
    ticksPerSecond = animation->mTicksPerSecond ? animation->mTicksPerSecond : 25.0f;
    importChannels(animation, nodes);
}

const Bone* Animation::findBone(const std::string& name) const {
    const auto found = boneInfoMap.find(name);
    return found != boneInfoMap.end() ? &found->second : nullptr;
}

void Animation::importChannels(const aiAnimation* animation, const std::vector<Node>& nodes) {
    const int channelCount = animation->mNumChannels;
    for (int index = 0; index < channelCount; ++index) {
        const auto* channel = animation->mChannels[index];
        const std::string boneName = channel->mNodeName.C_Str();

        const Node* node = nullptr;
        for (const auto& candidate : nodes) {
            if (candidate.name == boneName) {
                node = &candidate;
                break;
            }
        }
        if (node != nullptr) {
            if (node->id == -1) {
                TOGR_LOG_WARN("Bone node not found in model nodes: " << boneName);
                continue;
            }
            boneInfoMap[node->name] = Bone(channel);
        }
    }
}

Animator::Animator(const std::vector<Node>& nodes) : nodes(nodes) {
    for (const auto& node : nodes) {
        pose.nodePositions.push_back(node.bindPosition);
    }
    pose.boneMatrices.resize(TOGR_MAX_BONES, glm::mat4(1.0f));
    boneMatrixTexture = Texture2D(4, TOGR_MAX_BONES, GL_CLAMP_TO_BORDER, GL_LINEAR, GL_RGBA32F, GL_RGBA, GL_FLOAT);
    boneMatrixTexture.subImage2D(0, 0, 4, TOGR_MAX_BONES, pose.boneMatrices.data());
}

Animator::Animator(const Animation* animation, const std::vector<Node>& nodes) : Animator(nodes) {
    currentAnimation = animation;
}

void Animator::updateAnimation(float dt) {
    if (!currentAnimation || !currentAnimation->isValid() || nodes.empty()) return;
    currentTime += currentAnimation->getTicksPerSecond() * dt;
    const float duration = currentAnimation->getDuration();
    currentTime = duration > 0.0f ? fmod(currentTime, duration) : 0.0f;
    calculateBoneTransform(0, glm::mat4(1.0f));
    boneMatrixTexture.subImage2D(0, 0, 4, TOGR_MAX_BONES, pose.boneMatrices.data());
}

void Animator::playAnimation(const Animation* animation) {
    currentAnimation = animation;
    currentTime = 0.0f;
}

void Animator::calculateBoneTransform(std::size_t nodeIndex, const glm::mat4& parentTransform) {
    const auto& node = nodes[nodeIndex];
    glm::mat4 nodeTransform = node.localBindTransform;
    const Bone* bone = currentAnimation ? currentAnimation->findBone(node.name) : nullptr;
    if (bone) {
        nodeTransform = bone->sample(currentTime);
    }
    const glm::mat4 globalTransformation = parentTransform * nodeTransform;
    if (node.isBoneNode && node.id >= 0 && node.id < TOGR_MAX_BONES) {
        // Both the global transform and the imported offset use model space.
        // The root is already accounted for; adding its inverse would change that space.
        pose.boneMatrices[node.id] = globalTransformation * node.modelToBoneBind;
    }
    pose.nodePositions[nodeIndex] = glm::vec3(globalTransformation[3]);
    for (int childIndex : node.childrenIndices) {
        if (childIndex < 0 || static_cast<std::size_t>(childIndex) >= nodes.size()) continue;
        calculateBoneTransform(static_cast<std::size_t>(childIndex), globalTransformation);
    }
}

} // namespace togr
