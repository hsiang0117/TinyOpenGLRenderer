#ifndef LIGHT_HPP
#define LIGHT_HPP
#pragma once

#include <glm/glm.hpp>

class Light {
public:
	Light() {};
	glm::vec3 color;
};

class PointLight : public Light {

};

class DirectionalLight : public Light {

};

class SpotLight : public Light {

};
#endif