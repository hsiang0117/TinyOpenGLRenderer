#ifndef GLM_ENABLE_EXPERIMENTAL
#define GLM_ENABLE_EXPERIMENTAL
#endif
#include "bone.hpp"
#include <assimp/anim.h>
#include <algorithm>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace togr {

Bone::Bone(const aiNodeAnim* channel) {
    numPositions = channel->mNumPositionKeys;
    for (int positionIndex = 0; positionIndex < numPositions; ++positionIndex) {
        const aiVector3D aiPosition = channel->mPositionKeys[positionIndex].mValue;
        const float timeStamp = channel->mPositionKeys[positionIndex].mTime;
        KeyPosition data;
        data.position = glm::vec3(aiPosition.x, aiPosition.y, aiPosition.z);
        data.timeStamp = timeStamp;
        positions.push_back(data);
    }
    numRotations = channel->mNumRotationKeys;
    for (int rotationIndex = 0; rotationIndex < numRotations; ++rotationIndex) {
        const aiQuaternion aiOrientation = channel->mRotationKeys[rotationIndex].mValue;
        const float timeStamp = channel->mRotationKeys[rotationIndex].mTime;
        KeyRotation data;
        data.orientation = glm::quat(aiOrientation.w, aiOrientation.x, aiOrientation.y, aiOrientation.z);
        data.timeStamp = timeStamp;
        rotations.push_back(data);
    }
    numScalings = channel->mNumScalingKeys;
    for (int keyIndex = 0; keyIndex < numScalings; ++keyIndex) {
        const aiVector3D scale = channel->mScalingKeys[keyIndex].mValue;
        const float timeStamp = channel->mScalingKeys[keyIndex].mTime;
        KeyScale data;
        data.scale = glm::vec3(scale.x, scale.y, scale.z);
        data.timeStamp = timeStamp;
        scales.push_back(data);
    }
}

glm::mat4 Bone::sample(float ticks) const {
    const glm::mat4 translation = interpolatePosition(ticks);
    const glm::mat4 rotation = interpolateRotation(ticks);
    const glm::mat4 scale = interpolateScaling(ticks);
    return translation * rotation * scale;
}

int Bone::getPositionIndex(float ticks) const {
    for (int index = 0; index < numPositions - 1; ++index) {
        if (ticks < positions[index + 1].timeStamp) return index;
    }
    return std::max(0, numPositions - 2);
}

int Bone::getRotationIndex(float ticks) const {
    for (int index = 0; index < numRotations - 1; ++index) {
        if (ticks < rotations[index + 1].timeStamp) return index;
    }
    return std::max(0, numRotations - 2);
}

int Bone::getScaleIndex(float ticks) const {
    for (int index = 0; index < numScalings - 1; ++index) {
        if (ticks < scales[index + 1].timeStamp) return index;
    }
    return std::max(0, numScalings - 2);
}

float Bone::getScaleFactor(float lastTimeStamp, float nextTimeStamp, float ticks) const {
    const float framesDiff = nextTimeStamp - lastTimeStamp;
    if (framesDiff <= 0.0f) return 0.0f;
    return glm::clamp((ticks - lastTimeStamp) / framesDiff, 0.0f, 1.0f);
}

glm::mat4 Bone::interpolatePosition(float ticks) const {
    if (numPositions == 0) return glm::mat4(1.0f);
    if (numPositions == 1) {
        return glm::translate(glm::mat4(1.0f), positions[0].position);
    }
    const int p0Index = getPositionIndex(ticks);
    const int p1Index = p0Index + 1;
    const float scaleFactor = getScaleFactor(positions[p0Index].timeStamp,
        positions[p1Index].timeStamp, ticks);
    const glm::vec3 finalPosition = glm::mix(positions[p0Index].position, positions[p1Index].position, scaleFactor);
    return glm::translate(glm::mat4(1.0f), finalPosition);
}

glm::mat4 Bone::interpolateRotation(float ticks) const {
    if (numRotations == 0) return glm::mat4(1.0f);
    if (numRotations == 1) {
        const auto rotation = glm::normalize(rotations[0].orientation);
        return glm::toMat4(rotation);
    }
    const int r0Index = getRotationIndex(ticks);
    const int r1Index = r0Index + 1;
    const float scaleFactor = getScaleFactor(rotations[r0Index].timeStamp,
        rotations[r1Index].timeStamp, ticks);
    glm::quat finalRotation = glm::slerp(rotations[r0Index].orientation, rotations[r1Index].orientation, scaleFactor);
    finalRotation = glm::normalize(finalRotation);
    return glm::toMat4(finalRotation);
}

glm::mat4 Bone::interpolateScaling(float ticks) const {
    if (numScalings == 0) return glm::mat4(1.0f);
    if (numScalings == 1) {
        return glm::scale(glm::mat4(1.0f), scales[0].scale);
    }
    const int s0Index = getScaleIndex(ticks);
    const int s1Index = s0Index + 1;
    const float scaleFactor = getScaleFactor(scales[s0Index].timeStamp,
        scales[s1Index].timeStamp, ticks);
    const glm::vec3 finalScale = glm::mix(scales[s0Index].scale, scales[s1Index].scale, scaleFactor);
    return glm::scale(glm::mat4(1.0f), finalScale);
}

} // namespace togr
