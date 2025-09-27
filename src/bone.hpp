#ifndef BONE_HPP
#define BONE_HPP
#pragma once

#include <string>
#include <vector>
#include <iostream>
#include <glm/glm.hpp>

class Bone {
public:
	Bone();
	Bone(const std::string& name, int id);
	std::string getName() const { return name; }
	glm::mat4 getLocalTransform();
	glm::vec3 getPosition();
	void setTransform(const glm::mat4& mat) { transform = mat; }
	void setOffsetMatrix(const glm::mat4& mat) { offsetMatrix = mat; }
	void setParent(Bone* p) { parent = p; }
private:
	int id;
	std::string name;
	glm::mat4 transform;
	glm::mat4 offsetMatrix;
	Bone* parent;
};

glm::mat4 Bone::getLocalTransform()
{
	if (parent) {
		return parent->getLocalTransform() * transform;
	}
	return transform;
}

glm::vec3 Bone::getPosition()
{
	return glm::vec3(getLocalTransform() * glm::vec4(0, 0, 0, 1));
}

Bone::Bone(const std::string& name, int id)
	: name(name), id(id), parent(nullptr)
{
	transform = glm::mat4(1.0f);
	offsetMatrix = glm::mat4(1.0f);
}

using BonePtr = std::shared_ptr<Bone>;

#endif