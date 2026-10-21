#include "animation.hpp"
#include "core/logger.hpp"
#include "rendering/geometry/mesh.hpp"
#include <assimp/anim.h>
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
				LOG_WARN("Bone node not found in model nodes: " << boneName);
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
	if (currentAnimation && currentAnimation->isValid() && !nodes.empty()) {
		currentTime += currentAnimation->getTicksPerSecond() * deltaTime;
		const float duration = currentAnimation->getDuration();
		currentTime = duration > 0.0f ? fmod(currentTime, duration) : 0.0f;
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
	Bone* bone = currentAnimation ? currentAnimation->findBone(nodeName) : nullptr;
	if (bone) {
		bone->update(currentTime);
		nodeTransform = bone->getLocalTransform();
	}
	glm::mat4 globalTransformation = parentTransform * nodeTransform;
	if (node.isBoneNode && node.id >= 0 && node.id < MAX_BONES) {
		finalBoneMatrices[node.id] = globalTransformation * node.offsetMatrix;
	}
	node.position = glm::vec3(globalTransformation[3]);
	for(auto childIndex : node.childrenIndices) {
		if (childIndex < 0 || static_cast<size_t>(childIndex) >= nodes.size()) continue;
		calculateBoneTransform(nodes[childIndex], globalTransformation);
	}
}
