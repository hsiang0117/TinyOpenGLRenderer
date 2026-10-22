#ifndef ASSIMPNODE_HPP
#define ASSIMPNODE_HPP
#pragma once

#include <string>
#include <vector>
#include <glm/glm.hpp>

struct Node {
	std::string name;
	int id;
	int parentIndex;
	glm::vec3 position;
	glm::mat4 transform;
	// Converts baked model-space vertices into this bone's bind space.
	glm::mat4 offsetMatrix;
	bool isBoneNode = false;
	std::vector<int> childrenIndices;
};

#endif
