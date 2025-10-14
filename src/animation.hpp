#ifndef ANIMATION_HPP
#define ANIMATION_HPP
#pragma once

#include "bone.hpp"
#include <assimp/scene.h>
#include <map>

class Animation 
{
public:
	Animation() {};
	Animation(const aiScene* scene, std::vector<Node> nodes);
	~Animation() = default;
	Bone* findBone(const std::string& name);
	float getTicksPerSecond() { return ticksPerSecond; }
	float getDuration() { return duration; }
	Node& getRootNode() { return nodes[0]; }
	std::vector<Node>& getAllNodes() { return nodes; }
	bool isValid() const { return valid; }
private:
	bool valid;
	float duration;
	float ticksPerSecond;
	std::vector<Node> nodes;
	std::vector<Bone> bones;
	void readMissingBones(const aiAnimation* animation, std::vector<Node>& nodes);
};

Animation::Animation(const aiScene* scene, std::vector<Node> nodes)
{
	auto animation = scene->mAnimations[1];
	if (!animation) {
		valid = false;
		return;
	}
	valid = true;
	this->nodes = nodes;
	duration = animation->mDuration;
	ticksPerSecond = animation->mTicksPerSecond ? animation->mTicksPerSecond : 25.0f;
	readMissingBones(animation, this->nodes);
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

class Animator
{
public:
	Animator();
	Animator(Animation* animation);
	void updateAnimation(float dt);
	void playAnimation(Animation* pAnimation);
	void calculateBoneTransform(const Node& node, const glm::mat4 parentTransform);
	std::vector<glm::mat4>& getFinalBoneMatrices() { return finalBoneMatrices; }
private:
	Animation* currentAnimation;
	float currentTime;
	float deltaTime;
	std::vector<glm::mat4> finalBoneMatrices;
};

Animator::Animator()
{
	currentTime = 0.0f;
	deltaTime = 0.0f;
	finalBoneMatrices.reserve(MAX_BONES);
	for (int i = 0; i < MAX_BONES; i++) {
		finalBoneMatrices.push_back(glm::mat4(1.0f));
	}
	currentAnimation = nullptr;
}

Animator::Animator(Animation* animation)
{
	currentTime = 0.0f;
	deltaTime = 0.0f;
	finalBoneMatrices.reserve(MAX_BONES);
	for (int i = 0; i < MAX_BONES; i++) {
		finalBoneMatrices.push_back(glm::mat4(1.0f));
	}
	currentAnimation = animation;
}

void Animator::updateAnimation(float dt)
{
	deltaTime = dt;
	if (currentAnimation) {
		currentTime += currentAnimation->getTicksPerSecond() * deltaTime;
		currentTime = fmod(currentTime, currentAnimation->getDuration());
		calculateBoneTransform(currentAnimation->getRootNode(), glm::mat4(1.0f));
	}
}

void Animator::playAnimation(Animation* pAnimation)
{
	currentAnimation = pAnimation;
	currentTime = 0.0f;
}

void Animator::calculateBoneTransform(const Node& node, const glm::mat4 parentTransform)
{
	std::string nodeName = node.name;
	glm::mat4 nodeTransform = node.transform;
	glm::mat4 globalTransformation = parentTransform;
	Bone* bone = currentAnimation->findBone(nodeName);
	if (bone) {
		bone->update(currentTime);
		nodeTransform = bone->getLocalTransform();
		globalTransformation = parentTransform * nodeTransform;
		finalBoneMatrices[bone->getBoneID()] = globalTransformation * node.offsetMatrix;
	}
	for(auto childIndex : node.childrenIndices) {
		calculateBoneTransform(currentAnimation->getAllNodes()[childIndex], globalTransformation);
	}
}

#endif