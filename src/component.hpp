#ifndef COMPONENT_HPP
#define COMPONENT_HPP
#pragma once

#include "texture.hpp"
#include "model.hpp"
#include "skybox.hpp"
#include <iostream>
#include <glm/glm.hpp>

class Component {
public:
	virtual ~Component() {}
	virtual std::string getName() const = 0;
};

using ComponentPtr = std::shared_ptr<Component>;

class Transform : public Component {
public:
	Transform() : translate(0.0f), scale(1.0f), rotate(0.0f) {
		name = "Transform";
	}
	virtual std::string getName() const override { return name; }

	glm::vec3 translate;
	glm::vec3 scale;
	glm::vec3 rotate;
private:
	std::string name;
};

class RenderComponent : public Component {
public:
	RenderComponent() {
		name = "RenderComponent";
	}
	virtual std::string getName() const override { return name; }

	void setModel(ModelPtr model) { this->model = model; }
	ModelPtr model;
private:
	std::string name;
};

class PointLightComponent : public Component {
public:
	PointLightComponent() : color(1.0f), constant(1.0f), linear(0.09f), quadratic(0.032f) {
		name = "PointLightComponent";
	}
	virtual std::string getName() const override { return name; }

	glm::vec3 color;
	float constant;
	float linear;
	float quadratic;
private:
	std::string name;
};

class DirectionLightComponent : public Component {
public:
	DirectionLightComponent() : color(1.0f) {
		name = "DirectionLightComponent";
	}
	virtual std::string getName() const override { return name; }

	glm::vec3 color;
private:
	std::string name;
};

class SpotLightComponent : public Component {
public:
	SpotLightComponent() : color(1.0f), cutOff(12.5f), outerCutOff(17.5f){
		name = "SpotLightComponent";
	}
	virtual std::string getName() const override { return name; }

	glm::vec3 color;
	float cutOff;
	float outerCutOff;
private:
	std::string name;
};

class SkyBoxComponent : public Component {
public:
	SkyBoxComponent() {
		name = "SkyBoxComponent";
	}
	virtual std::string getName() const override { return name; }
	
	SkyBoxPtr skybox;
	void setSkyBox(std::string folderPath);
private:
	std::string name;
};

void SkyBoxComponent::setSkyBox(std::string folderPath) {
	skybox = std::make_shared<SkyBox>(folderPath);
	skybox->initGLResources();
}

class ShadowCaster2D : public Component {
public:
	ShadowCaster2D() {
		name = "ShadowCaster";
	}
	virtual std::string getName() const override { return name; }

	bool enabled;
private:
	std::string name;
};

class ShadowCasterCube : public Component {
public:
	ShadowCasterCube() {
		name = "ShadowCasterCube";
	}
	virtual std::string getName() const override { return name; }
	bool enabled;
private:
	std::string name;
};

class MeshComponent : public Component {
public:
	MeshComponent() {
		name = "MeshComponent";
	}
	virtual std::string getName() const override { return name; }

	void setMesh(MeshPtr mesh) { this->mesh = mesh; }
	MeshPtr mesh;
private:
	std::string name;
};
#endif