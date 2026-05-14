#ifndef ANIMATION_HPP
#define ANIMATION_HPP
#pragma once

#include "bone.hpp"
#include "assimpNode.hpp"
#include "texture.hpp"
#include "mesh.hpp"
#include <assimp/scene.h>
#include <unordered_map>
#include <memory>

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
	std::unordered_map<std::string, Bone> boneInfoMap;
	void readMissingBones(const aiAnimation* animation, std::vector<Node>& nodes);
};

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
	std::vector<glm::mat4> finalBoneMatrices;
	Texture2D boneMatrixTexture;
};

#endif
