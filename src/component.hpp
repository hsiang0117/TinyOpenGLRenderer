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
	Component(std::string name): name(name) {}
	virtual ~Component() {}
	std::string getName() const;
private:
	std::string name;
};

std::string Component::getName() const {
	return name;
}

using ComponentPtr = std::shared_ptr<Component>;

class Transform : public Component {
public:
	Transform() : Component("Transform") {
		translate = glm::vec3(0.0f);
		scale = glm::vec3(1.0f);
		rotate = glm::vec3(0.0f);
	}

	glm::vec3 translate;
	glm::vec3 scale;
	glm::vec3 rotate;
};

class RenderComponent : public Component {
public:

	RenderComponent() : Component("RenderComponent") {
		skeletonVisible = false;
	}

	void setModel(ModelPtr model) { 
		this->model = model; 
		this->model->buildAABB(aabb.min, aabb.max);
	}
	ModelPtr model;

	struct AABB {
		glm::vec3 min;
		glm::vec3 max;
	}aabb;

	bool skeletonVisible;
};

class PointLightComponent : public Component {
public:
	PointLightComponent() : Component("PointLightComponent") {
		color = glm::vec3(1.0f);
		brightness = 1.0f;
		constant = 1.0f;
		linear = 0.009f;
		quadratic = 0.032f;
	}

	glm::vec3 color;
	float brightness;
	float constant;
	float linear;
	float quadratic;
};

class DirectionLightComponent : public Component {
public:
	DirectionLightComponent() : Component("DirectionLightComponent") {
		color = glm::vec3(1.0f);
		brightness = 1.0f;
	}

	glm::vec3 color;
	float brightness;
};

class SpotLightComponent : public Component {
public:
	SpotLightComponent() : Component("SpotLightComponent") {
		color = glm::vec3(1.0f);
		cutOff = 12.5f;
		outerCutOff = 17.5f;
		brightness = 1.0f;
	}

	glm::vec3 color;
	float brightness;
	float cutOff;
	float outerCutOff;
};

class SkyBoxComponent : public Component {
public:
	SkyBoxComponent() : Component("SkyBoxComponent") {}
	
	SkyBoxPtr skybox;
	void setSkyBox(std::string folderPath);
};

void SkyBoxComponent::setSkyBox(std::string folderPath) {
	skybox = std::make_shared<SkyBox>(folderPath);
	skybox->initGLResources();
}

class ShadowCaster2D : public Component {
public:
	ShadowCaster2D() : Component("ShadowCaster2D") {
		enabled = true;
	}

	bool enabled;
};

class ShadowCasterCube : public Component {
public:
	ShadowCasterCube() : Component("ShadowCasterCube") {
		enabled = true;
		farPlane = 25.0f;
	}

	bool enabled;
	float farPlane;
};

class StaticMeshComponent : public Component {
public:
	StaticMeshComponent() : Component("StaticMeshComponent") {}

	void setMesh(MeshPtr mesh) {
		this->mesh = mesh;
		this->mesh->buildAABB(aabb.min, aabb.max);
	}

	MeshPtr mesh;
	struct AABB {
		glm::vec3 min;
		glm::vec3 max;
	}aabb;
};

class DynamicMaterialComponent : public Component {
public:
	DynamicMaterialComponent() : Component("DynamicMaterialComponent") {}

	char albedoPath[255] = {};
	char specularPath[255] = {};
	char normalPath[255] = {};
	Material material;
	void setMaterial();
};

void DynamicMaterialComponent::setMaterial() {
	material.albedoPath = albedoPath;
	material.specularPath = specularPath;
	material.normalPath = normalPath;
	material.initGLResources();
}

class AnimatorComponent : public Component {
public:
	AnimatorComponent() : Component("AnimatorComponent"), playing(false) {}
	void update(float dt) { animator.updateAnimation(dt); }
	void setAnimation(Animation* animation) { animator.playAnimation(animation); }
	std::vector<glm::mat4>& getFinalBoneMatrices() { return animator.getFinalBoneMatrices(); }
	bool playing;
private:
	Animator animator;
};
#endif