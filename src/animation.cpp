#include "animation.hpp"
#include <iostream>
#include <cmath>

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
	return boneInfoMap.count(name) ? &boneInfoMap[name] : nullptr;
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
			boneInfoMap[node->name] = Bone(id, channel);
		}
	}
}

Animator::Animator(std::vector<Node>& nodes)
	: nodes(nodes)
{
	currentAnimation = nullptr;
	currentTime = 0.0f;
	deltaTime = 0.0f;
	finalBoneMatrices.resize(MAX_BONES, glm::mat4(1.0f));
	boneMatrixTexture = Texture2D(4, MAX_BONES, GL_CLAMP_TO_BORDER, GL_LINEAR, GL_RGBA32F, GL_RGBA, GL_FLOAT);
}

Animator::Animator(Animation* animation, std::vector<Node>& nodes)
	: nodes(nodes)
{
	currentAnimation = animation;
	currentTime = 0.0f;
	deltaTime = 0.0f;
	finalBoneMatrices.resize(MAX_BONES, glm::mat4(1.0f));
	boneMatrixTexture = Texture2D(4, MAX_BONES, GL_CLAMP_TO_BORDER, GL_LINEAR, GL_RGBA32F, GL_RGBA, GL_FLOAT);
}

void Animator::updateAnimation(float dt)
{
	deltaTime = dt;
	if (currentAnimation) {
		currentTime += currentAnimation->getTicksPerSecond() * deltaTime;
		currentTime = fmod(currentTime, currentAnimation->getDuration());
		calculateBoneTransform(nodes[0], glm::mat4(1.0f));
		boneMatrixTexture.subImage2D(0, 0, 4, MAX_BONES, finalBoneMatrices.data());
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
			finalBoneMatrices[bone->getBoneID()] = matrix;
		}
	}
	node.position = glm::vec3(globalTransformation[3]);
	for(auto childIndex : node.childrenIndices) {
		calculateBoneTransform(nodes[childIndex], globalTransformation);
	}
}
