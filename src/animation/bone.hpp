#ifndef TOGR_BONE_HPP
#define TOGR_BONE_HPP
#pragma once

#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
struct aiNodeAnim;

namespace togr {

struct KeyPosition {
    glm::vec3 position;
    float timeStamp; // Clip ticks; rotation and scale keys use the same unit.
};

struct KeyRotation {
    glm::quat orientation;
    float timeStamp;
};

struct KeyScale {
    glm::vec3 scale;
    float timeStamp;
};

// One node's immutable copied key tracks; sampling has no per-clip cached state.
class Bone {
public:
    Bone() = default;
    // Copies a non-null Assimp channel; the caller may then release the channel.
    explicit Bone(const aiNodeAnim* channel);
    // ticks is clip time; returns the sampled local TRS relative to the parent.
    glm::mat4 sample(float ticks) const;
private:
    int getPositionIndex(float ticks) const;
    int getRotationIndex(float ticks) const;
    int getScaleIndex(float ticks) const;

    std::vector<KeyPosition> positions;
    std::vector<KeyRotation> rotations;
    std::vector<KeyScale> scales;
    int numPositions = 0;
    int numRotations = 0;
    int numScalings = 0;

    float getScaleFactor(float lastTimeStamp, float nextTimeStamp, float ticks) const;
    glm::mat4 interpolatePosition(float ticks) const;
    glm::mat4 interpolateRotation(float ticks) const;
    glm::mat4 interpolateScaling(float ticks) const;
};

} // namespace togr

#endif
