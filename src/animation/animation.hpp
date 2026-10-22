#ifndef TOGR_ANIMATION_HPP
#define TOGR_ANIMATION_HPP
#pragma once

#include "bone.hpp"
#include "assimpNode.hpp"
#include "skeletonPose.hpp"
#include "rendering/opengl/texture.hpp"
#include <cstddef>
#include <unordered_map>
#include <string>
#include <vector>
#include <glm/glm.hpp>

struct aiAnimation;

namespace togr {

// Owns immutable imported key tracks. Assimp inputs are borrowed only during
// construction; multiple animators may sample one clip without changing it.
class Animation {
public:
    // A null animation yields an invalid clip; nodes match imported channel names.
    Animation(const aiAnimation* animation, const std::vector<Node>& nodes);
    ~Animation() = default;
    // Borrowed immutable track, or nullptr when the clip has no channel for this node.
    const Bone* findBone(const std::string& name) const;
    float getTicksPerSecond() const { return ticksPerSecond; }
    float getDuration() const { return duration; } // Animation ticks, not seconds.
    bool isValid() const { return valid; }
    std::string getName() const { return name; }
private:
    std::string name;
    bool valid = false;
    float duration = 0.0f;
    float ticksPerSecond = 25.0f;
    std::unordered_map<std::string, Bone> boneInfoMap;
    void importChannels(const aiAnimation* animation, const std::vector<Node>& nodes);
};

// Owns its sampled pose and GPU palette; borrows immutable nodes and current clip.
// Both imported inputs must outlive the animator.
// Keep node topology/indices fixed. Do not erase, reorder or reallocate clips in
// their owning container while its selected clip is borrowed here.
// Construction, updates and texture destruction require the current GL context.
class Animator {
public:
    // Initial pose uses bind positions and an uploaded identity palette.
    Animator(const std::vector<Node>& nodes);
    Animator(const Animation* animation, const std::vector<Node>& nodes);
    // dt is seconds; accumulates clip ticks and updates this instance's pose.
    // Uploads G(t) * modelToBoneBind for baked model-space mesh vertices.
    // Null/invalid clips or empty nodes leave the current pose unchanged.
    void updateAnimation(float dt);
    // A nullable borrowed clip; resets time but waits for updateAnimation() to sample.
    void playAnimation(const Animation* animation);
    const Animation* getCurrentAnimation() const { return currentAnimation; }
    const SkeletonPose& getPose() const { return pose; }
    Texture2D& getBoneMatrixTexture() { return boneMatrixTexture; }
private:
    void calculateBoneTransform(std::size_t nodeIndex, const glm::mat4& parentTransform);

    const std::vector<Node>& nodes;
    const Animation* currentAnimation = nullptr;
    float currentTime = 0.0f;
    SkeletonPose pose;
    Texture2D boneMatrixTexture;
};

} // namespace togr

#endif
