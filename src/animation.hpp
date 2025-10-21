#ifndef ANIMATION_HPP
#define ANIMATION_HPP
#pragma once

#include "bone.hpp"
#include <assimp/scene.h>
#include <map>

class Animation 
{
public:
	Animation(const aiAnimation* animation, std::vector<Node>& nodes);
	~Animation() = default;
	Bone* findBone(const std::string& name);
	float getTicksPerSecond() { return ticksPerSecond; }
	float getDuration() { return duration; }
	bool isValid() const { return valid; }
	std::string getName() const { return name; }
private:
	std::string name;
	bool valid;
	float duration;
	float ticksPerSecond;
	std::vector<Bone> bones;
	void readMissingBones(const aiAnimation* animation, std::vector<Node>& nodes);
};

Animation::Animation(const aiAnimation* animation, std::vector<Node>& nodes)
{
	if (!animation) {
		valid = false;
		return;
	}
	name = animation->mName.C_Str();
	valid = true;
	duration = animation->mDuration;
	ticksPerSecond = animation->mTicksPerSecond ? animation->mTicksPerSecond : 25.0f;
	readMissingBones(animation, nodes);
}

Bone* Animation::findBone(const std::string& name)
{
	for (auto& bone : bones) {
		if (bone.getBoneName() == name) {
			return &bone;
		}
	}
	return nullptr;
}

void Animation::readMissingBones(const aiAnimation* animation, std::vector<Node>& nodes)
{
	int size = animation->mNumChannels;

	for (int i = 0; i < size; i++) {
		auto channel = animation->mChannels[i];
		std::string boneName = channel->mNodeName.C_Str();

		Node* node = nullptr;
		for (auto& n : nodes) {
			if (n.name == boneName) {
				node = &n;
				break;
			}
		}
		if (node != nullptr) {
			int id = node->id;
			if (id == -1) {
				std::cout << "Error: Bone node not found in model nodes: " << boneName << std::endl;
				continue;
			}
			bones.push_back(Bone(channel->mNodeName.C_Str(), id, channel));
		}
	}
}

using AnimationPtr = std::shared_ptr<Animation>;

class Animator
{
public:
	Animator(std::vector<Node>& nodes);
	Animator(Animation* animation, std::vector<Node>& nodes);
	void updateAnimation(float dt);
	void playAnimation(Animation* pAnimation);
	void calculateBoneTransform(Node& node, const glm::mat4 parentTransform);
	Texture2D& getBoneMatrixTexture() { return boneMatrixTexture; }
private:
	std::vector<Node>& nodes;
	Animation* currentAnimation;
	float currentTime;
	float deltaTime;
	Texture2D boneMatrixTexture;
};

Animator::Animator(std::vector<Node>& nodes)
	: nodes(nodes)
{
	currentAnimation = nullptr;
	currentTime = 0.0f;
	deltaTime = 0.0f;
	boneMatrixTexture = Texture2D(4, MAX_BONES, GL_CLAMP_TO_BORDER, GL_LINEAR, GL_RGBA32F, GL_RGBA, GL_FLOAT);
}

Animator::Animator(Animation* animation, std::vector<Node>& nodes)
	: nodes(nodes)
{
	currentAnimation = animation;
	currentTime = 0.0f;
	deltaTime = 0.0f;
	boneMatrixTexture = Texture2D(4, MAX_BONES, GL_CLAMP_TO_BORDER, GL_LINEAR, GL_RGBA32F, GL_RGBA, GL_FLOAT);
}

void Animator::updateAnimation(float dt)
{
	deltaTime = dt;
	if (currentAnimation) {
		currentTime += currentAnimation->getTicksPerSecond() * deltaTime;
		currentTime = fmod(currentTime, currentAnimation->getDuration());
		calculateBoneTransform(nodes[0], glm::mat4(1.0f));
	}
}

void Animator::playAnimation(Animation* pAnimation)
{
	currentAnimation = pAnimation;
	currentTime = 0.0f;
}

void Animator::calculateBoneTransform(Node& node, const glm::mat4 parentTransform)
{
	std::string nodeName = node.name;
	glm::mat4 nodeTransform = node.transform;
	glm::mat4 globalTransformation = parentTransform;
	Bone* bone = currentAnimation->findBone(nodeName);
	if (bone) {
		bone->update(currentTime);
		nodeTransform = bone->getLocalTransform();
		globalTransformation = parentTransform * nodeTransform;
		if (bone->getBoneID() >= 0 && bone->getBoneID() < MAX_BONES) {
			auto matrix = globalTransformation * node.offsetMatrix;
			boneMatrixTexture.subImage2D(0, bone->getBoneID(), 4, 1, glm::value_ptr(matrix));
		}
	}
	node.position = glm::vec3(globalTransformation[3]);
	for(auto childIndex : node.childrenIndices) {
		calculateBoneTransform(nodes[childIndex], globalTransformation);
	}
}

#endif