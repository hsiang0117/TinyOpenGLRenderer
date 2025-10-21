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
	AnimatorComponent(std::vector<Node>& nodes) : Component("AnimatorComponent"), playing(false), animator(nodes) {}
	std::vector<std::string> getAnimationsNames();
	void update(float dt) { animator.updateAnimation(dt); }
	void setAnimation(std::vector<Animation>* animations) { this->animations = animations; }
	void playAnimation(std::string name);
	Texture2D& getBoneMatrixTexture() { return animator.getBoneMatrixTexture(); }
	std::string getCurrentAnimation() const { return currentAnimation; }
	bool playing;
private:
	std::string currentAnimation;
	std::vector<Animation>* animations;
	Animator animator;
};

std::vector<std::string> AnimatorComponent::getAnimationsNames() {
	std::vector<std::string> names;
	if (animations) {
		for (auto& anim : *animations) {
			names.push_back(anim.getName());
		}
	}
	return names;
}

void AnimatorComponent::playAnimation(std::string name) { 
	if (animations) {
		currentAnimation = name;
		for (auto i = 0; i < animations->size(); i++) {
			if ((*animations)[i].getName() == name) {
				animator.playAnimation(&(*animations)[i]);
				break;
			}
		}
	}
}

class SkeletonViewerComponent : public Component {
public:
	SkeletonViewerComponent(const std::vector<Node>& nodes) : Component("SkeletonViewerComponent"), nodes(nodes), show(false) {
		glGenVertexArrays(1, &VAO);
		glGenBuffers(1, &VBO);
		glGenVertexArrays(1, &lineVAO);
		glGenBuffers(1, &lineVBO);
	}
	std::vector<Node>& getNodes() { return nodes; }
	void drawSkeleton();

	bool show;
private:
	std::vector<Node> nodes;
	GLuint VAO, VBO, lineVAO, lineVBO;
};

void SkeletonViewerComponent::drawSkeleton() {
	if (!show) return;
	std::vector<glm::vec3> bonePositions;
	for (const auto& node : nodes) {
		if (node.isBoneNode) {
			bonePositions.push_back(node.position);
		}
	}
	if (!bonePositions.empty()) {
		glBindVertexArray(VAO);
		glBindBuffer(GL_ARRAY_BUFFER, VBO);
		glBufferData(GL_ARRAY_BUFFER, bonePositions.size() * sizeof(glm::vec3), &bonePositions[0], GL_STATIC_DRAW);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
		glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(bonePositions.size()));
		glBindVertexArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}

	std::vector<glm::vec3> lineVertices;
	for (int i = 0; i < nodes.size(); i++) {
		if (nodes[i].isBoneNode && nodes[nodes[i].parentIndex].isBoneNode && nodes[i].parentIndex != -1) {
			lineVertices.push_back(nodes[i].position);
			lineVertices.push_back(nodes[nodes[i].parentIndex].position);
		}
	}

	if (!lineVertices.empty()) {
		glBindVertexArray(lineVAO);
		glBindBuffer(GL_ARRAY_BUFFER, lineVBO);
		glBufferData(GL_ARRAY_BUFFER, lineVertices.size() * sizeof(glm::vec3), lineVertices.data(), GL_STATIC_DRAW);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
		glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(lineVertices.size()));
		glBindVertexArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}
}
#endif