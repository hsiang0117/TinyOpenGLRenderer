#ifndef BONE_HPP
#define BONE_HPP
#define GLM_ENABLE_EXPERIMENTAL
#pragma once

#include <string>
#include <vector>
#include <iostream>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <assimp/anim.h>

struct KeyPosition {
	glm::vec3 position;
	float timeStamp;
};

struct KeyRotation {
	glm::quat orientation;
	float timeStamp;
};

struct KeyScale {
	glm::vec3 scale;
	float timeStamp;
};

class Bone {
public:
	Bone(const std::string& name, int id, const aiNodeAnim* channel);
	void update(float animationTime);
	glm::mat4 getLocalTransform() const { return localTransform; };
	std::string getBoneName() const { return name; }
	int getBoneID() const { return id; }
	int getPositionIndex(float animationTime);
	int getRotationIndex(float animationTime);
	int getScaleIndex(float animationTime);
private:
	std::vector<KeyPosition> positions;
	std::vector<KeyRotation> rotations;
	std::vector<KeyScale> scales;
	int numPositions;
	int numRotations;
	int numScalings;
	glm::mat4 localTransform;
	std::string name;
	int id;

	float getScaleFactor(float lastTimeStamp, float nextTimeStamp, float animationTime);
	glm::mat4 interpolatePosition(float animationTime);
	glm::mat4 interpolateRotation(float animationTime);
	glm::mat4 interpolateScaling(float animationTime);
};

Bone::Bone(const std::string& name, int id, const aiNodeAnim* channel)
	: name(name), id(id), localTransform(1.0f)
{
	numPositions = channel->mNumPositionKeys;
	for (int positionIndex = 0; positionIndex < numPositions; ++positionIndex) {
		aiVector3D aiPosition = channel->mPositionKeys[positionIndex].mValue;
		float timeStamp = channel->mPositionKeys[positionIndex].mTime;
		KeyPosition data;
		data.position = glm::vec3(aiPosition.x, aiPosition.y, aiPosition.z);
		data.timeStamp = timeStamp;
		positions.push_back(data);
	}
	numRotations = channel->mNumRotationKeys;
	for (int rotationIndex = 0; rotationIndex < numRotations; ++rotationIndex) {
		aiQuaternion aiOrientation = channel->mRotationKeys[rotationIndex].mValue;
		float timeStamp = channel->mRotationKeys[rotationIndex].mTime;
		KeyRotation data;
		data.orientation = glm::quat(aiOrientation.w, aiOrientation.x, aiOrientation.y, aiOrientation.z);
		data.timeStamp = timeStamp;
		rotations.push_back(data);
	}
	numScalings = channel->mNumScalingKeys;
	for (int keyIndex = 0; keyIndex < numScalings; ++keyIndex) {
		aiVector3D scale = channel->mScalingKeys[keyIndex].mValue;
		float timeStamp = channel->mScalingKeys[keyIndex].mTime;
		KeyScale data;
		data.scale = glm::vec3(scale.x, scale.y, scale.z);
		data.timeStamp = timeStamp;
		scales.push_back(data);
	}
}

void Bone::update(float animationTime) {
	glm::mat4 translation = interpolatePosition(animationTime);
	glm::mat4 rotation = interpolateRotation(animationTime);
	glm::mat4 scale = interpolateScaling(animationTime);
	localTransform = translation * rotation * scale;
}

int Bone::getPositionIndex(float animationTime) {
	for (int index = 0; index < numPositions - 1; ++index) {
		if (animationTime < positions[index + 1].timeStamp)
			return index;
	}
	return numPositions - 1;
}

int Bone::getRotationIndex(float animationTime) {
	for (int index = 0; index < numRotations - 1; ++index) {
		if (animationTime < rotations[index + 1].timeStamp)
			return index;
	}
	return numRotations - 1;
}

int Bone::getScaleIndex(float animationTime) {
	for (int index = 0; index < numScalings - 1; ++index) {
		if (animationTime < scales[index + 1].timeStamp)
			return index;
	}
	return numScalings - 1;
}

float Bone::getScaleFactor(float lastTimeStamp, float nextTimeStamp, float animationTime) {
	float scaleFactor = 0.0f;
	float midWayLength = animationTime - lastTimeStamp;
	float framesDiff = nextTimeStamp - lastTimeStamp;
	scaleFactor = midWayLength / framesDiff;
	return scaleFactor;
}

glm::mat4 Bone::interpolatePosition(float animationTime) {
	if (1 == numPositions)
		return glm::translate(glm::mat4(1.0f), positions[0].position);
	int p0Index = getPositionIndex(animationTime);
	int p1Index = p0Index + 1;
	float scaleFactor = getScaleFactor(positions[p0Index].timeStamp,
		positions[p1Index].timeStamp, animationTime);
	glm::vec3 finalPosition = glm::mix(positions[p0Index].position, positions[p1Index].position, scaleFactor);
	return glm::translate(glm::mat4(1.0f), finalPosition);
}

glm::mat4 Bone::interpolateRotation(float animationTime) {
	if (1 == numRotations) {
		auto rotation = glm::normalize(rotations[0].orientation);
		return glm::toMat4(rotation);
	}
	int r0Index = getRotationIndex(animationTime);
	int r1Index = r0Index + 1;
	float scaleFactor = getScaleFactor(rotations[r0Index].timeStamp,
		rotations[r1Index].timeStamp, animationTime);
	glm::quat finalRotation = glm::slerp(rotations[r0Index].orientation, rotations[r1Index].orientation, scaleFactor);
	finalRotation = glm::normalize(finalRotation);
	return glm::toMat4(finalRotation);
}

glm::mat4 Bone::interpolateScaling(float animationTime) {
	if (1 == numScalings)
		return glm::scale(glm::mat4(1.0f), scales[0].scale);
	int s0Index = getScaleIndex(animationTime);
	int s1Index = s0Index + 1;
	float scaleFactor = getScaleFactor(scales[s0Index].timeStamp,
		scales[s1Index].timeStamp, animationTime);
	glm::vec3 finalScale = glm::mix(scales[s0Index].scale, scales[s1Index].scale, scaleFactor);
	return glm::scale(glm::mat4(1.0f), finalScale);
}
#endif