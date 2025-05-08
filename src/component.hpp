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
	PointLightComponent() : color(1.0f), constant(1.0f), linear(0.09f), quadratic(0.032f), brightness(1.0f) {
		name = "PointLightComponent";
	}
	virtual std::string getName() const override { return name; }

	glm::vec3 color;
	float brightness;
	float constant;
	float linear;
	float quadratic;
private:
	std::string name;
};

class DirectionLightComponent : public Component {
public:
	DirectionLightComponent() : color(1.0f), brightness(1.0f) {
		name = "DirectionLightComponent";
	}
	virtual std::string getName() const override { return name; }

	glm::vec3 color;
	float brightness;
private:
	std::string name;
};

class SpotLightComponent : public Component {
public:
	SpotLightComponent() : color(1.0f), cutOff(12.5f), outerCutOff(17.5f), brightness(1.0f){
		name = "SpotLightComponent";
	}
	virtual std::string getName() const override { return name; }

	glm::vec3 color;
	float brightness;
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
		name = "ShadowCaster2D";
		enabled = true;
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
		enabled = true;
		farPlane = 25.0f;
	}
	virtual std::string getName() const override { return name; }

	bool enabled;
	float farPlane;
private:
	std::string name;
};

class StaticMeshComponent : public Component {
public:
	StaticMeshComponent() {
		name = "StaticMeshComponent";
	}
	virtual std::string getName() const override { return name; }

	void setMesh(MeshPtr mesh) { this->mesh = mesh; }
	MeshPtr mesh;
private:
	std::string name;
};

class DynamicMaterialComponent : public Component {
public:
	DynamicMaterialComponent() {
		name = "DynamicMaterialComponent";
	}
	virtual std::string getName() const override { return name; }

	char albedoPath[255] = {};
	char specularPath[255] = {};
	char normalPath[255] = {};
	Material material;
	void setMaterial();
private:
	std::string name;
};

void DynamicMaterialComponent::setMaterial() {
	material.albedoPath = albedoPath;
	material.specularPath = specularPath;
	material.normalPath = normalPath;
	material.initGLResources();
}
#endif