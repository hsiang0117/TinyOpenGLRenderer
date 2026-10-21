#ifndef COMPONENT_HPP
#define COMPONENT_HPP
#pragma once

#include "animation/animation.hpp"
#include "rendering/material.hpp"
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <vector>

class Mesh;
class Model;
class Skybox;
class Texture2D;

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

class ModelRenderComponent : public Component {
public:

	ModelRenderComponent() : Component("ModelRenderComponent") {
		skeletonVisible = false;
	}

	void setModel(std::shared_ptr<Model> model);
	std::shared_ptr<Model> model;

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

class DirectionalLightComponent : public Component {
public:
	DirectionalLightComponent() : Component("DirectionalLightComponent") {
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

class CloudVolumeComponent : public Component {
public:
	enum class Mode { SkyLayer, LocalVolume };
	enum class Preset { ScatteredCumulus, BrokenStratocumulus, Overcast };
	Mode mode = Mode::SkyLayer;
	float layerBottom = 100.0f;         // world-space altitude, independent of Transform
	float layerThickness = 200.0f;
	float maxTraceDistance = 10000.0f;  // view/light ray budget in world units
	float distanceFadeStart = 8000.0f;  // view-only fade to the sky at the tracing limit
	float weatherScale;                // individual cloud coverage frequency
	// Broad sky weather; independent of the cloud body and local weather texture.
	float regionScale;                 // inverse world units; smaller values make broader regions
	float regionCoverage;              // large-scale cloud/clear-sky distribution, in [0,1]
	float densityVariation;            // regional density variation, in [0,1]
	float heightVariation;             // maximum fractional reduction of local layer thickness, in [0,0.8]

	CloudVolumeComponent();
	// Changes appearance only; preserves altitude, bounds, animation and quality.
	void applyPreset(Preset preset);

	// Shape
	float cloudType;              // 0=stratus, 0.5=stratocumulus, 1=cumulus
	// Coverage (world weather for sky layers; weather texture for local volumes)
	float coverageLow;
	float coverageHigh;
	// Noise sampling scales (UV multipliers into the pre-generated noise textures)
	float baseNoiseScale;
	float warpScale;
	float warpStrength;           // maximum X/Z displacement in world units; no Y warp
	float detailScale;
	float detailErosionStrength;
	// Medium and lighting: one extinction field for both view and shadow rays.
	float densityMultiplier;      // scales the medium density, not its brightness
	float extinctionCoefficient;  // inverse world units at density 1
	float scatteringAlbedo;       // sigma_s / sigma_t, in [0,1]
	float ambientStrength;        // diffuse sky illumination
	float sunStrength;            // cloud sun illumination relative to the directional light
	float phaseForwardG;          // HG forward lobe, in [0,0.85]
	float multipleScattering;     // strength of the approximate higher scattering orders
	// Animation
	glm::vec3 windVelocity = glm::vec3(1.0f, 0.0f, 1.0f); // world units/sec; all fields move together
	// Quality / boundary
	float edgeFadeDistance = 10.0f; // local-volume boundary only; sky layers have no XZ edge
	float viewStepWorldSize = 2.0f; // target world spacing between view march steps
	int viewMaxSteps = 96;
	int lightSteps = 8;
	int emptyStepMultiplier = 4; // skip only blocks proven to contain no density
	bool temporalAccumulation = true;
};

class SkyboxComponent : public Component {
public:
	SkyboxComponent() : Component("SkyboxComponent") {}

	std::shared_ptr<Skybox> skybox;
	char folderPath[255] = {};   // edit buffer; seeded by setSkybox so the field reflects the current path
	void setSkybox(std::string folderPath);
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

	void setMesh(std::shared_ptr<Mesh> mesh);

	std::shared_ptr<Mesh> mesh;
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
	std::vector<Animation>* animations = nullptr;
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
