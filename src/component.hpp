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

	static constexpr int glslSize = 48;
};

class DirectionLightComponent : public Component {
public:
	DirectionLightComponent() : Component("DirectionLightComponent") {
		color = glm::vec3(1.0f);
		brightness = 1.0f;
	}

	glm::vec3 color;
	float brightness;

	static constexpr int glslSize = 32;
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

	static constexpr int glslSize = 64;
};

class VolumeMarkerComponent : public Component {
public:
	VolumeMarkerComponent() : Component("VolumeMarkerComponent") {
		// Defaults reproduce the previous hardcoded volume.frag look exactly.
		cloudType = 0.1f;
		coverageLow = 0.3f;
		coverageHigh = 0.85f;
		baseNoiseScale = 0.01f;
		warpScale = 0.003f;
		warpStrength = 25.0f;
		detailScale = 0.01f;
		detailErosionStrength = 0.35f;
		lightAttenuation = 2.0f;
		densityGain = 2.5f;
		viewAttenuation = 2.0f;
		powderStrength = 2.0f;
		windVelocity = glm::vec3(1.0f, 0.0f, 1.0f);   // == old vec3(time,0,time)
		edgeFadeDistance = 10.0f;
		viewStepWorldSize = 2.0f;
	}

	// Shape
	float cloudType;              // 0=stratus, 0.5=stratocumulus, 1=cumulus
	// Coverage (weather-map remap range)
	float coverageLow;
	float coverageHigh;
	// Noise sampling scales (UV multipliers into the pre-generated noise textures)
	float baseNoiseScale;
	float warpScale;
	float warpStrength;
	float detailScale;
	float detailErosionStrength;
	// Lighting
	float lightAttenuation;       // shadow march extinction
	float densityGain;            // global radiance gain
	float viewAttenuation;        // view march extinction
	float powderStrength;         // powder dark-edge falloff
	// Animation
	glm::vec3 windVelocity;       // world units/sec advection (base + detail)
	// Quality / boundary
	float edgeFadeDistance;       // soft box-edge fade width
	float viewStepWorldSize;      // target world spacing between view march steps
};

class SkyBoxComponent : public Component {
public:
	SkyBoxComponent() : Component("SkyBoxComponent") {}

	SkyBoxPtr skybox;
	char folderPath[255] = {};   // edit buffer; seeded by setSkyBox so the field reflects the current path
	void setSkyBox(std::string folderPath);
};

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

class SkeletonViewerComponent : public Component {
public:
	SkeletonViewerComponent(const std::vector<Node>& nodes);
	~SkeletonViewerComponent();
	SkeletonViewerComponent(const SkeletonViewerComponent&) = delete;
	SkeletonViewerComponent& operator=(const SkeletonViewerComponent&) = delete;
	SkeletonViewerComponent(SkeletonViewerComponent&& other) noexcept;
	SkeletonViewerComponent& operator=(SkeletonViewerComponent&& other) noexcept;
	std::vector<Node>& getNodes() { return nodes; }
	void drawSkeleton();

	bool show;
private:
	std::vector<Node> nodes;
	GLuint VAO = 0, VBO = 0, lineVAO = 0, lineVBO = 0;
};

#endif
