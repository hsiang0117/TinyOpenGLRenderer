#include "component.hpp"
#include "rendering/geometry/mesh.hpp"
#include "assets/model.hpp"
#include "rendering/skybox.hpp"
#include <algorithm>
#include <utility>

CloudVolumeComponent::CloudVolumeComponent() : Component("CloudVolumeComponent") {
	applyPreset(Preset::ScatteredCumulus);
}

void CloudVolumeComponent::applyPreset(Preset preset) {
	densityMultiplier = 2.0f;
	scatteringAlbedo = 0.95f;
	sunStrength = 3.0f;
	switch (preset) {
	case Preset::ScatteredCumulus:
		cloudType = 1.0f; coverageLow = 0.0f; coverageHigh = 0.95f;
		regionScale = 0.0004f; regionCoverage = 0.5f; densityVariation = 0.65f; heightVariation = 0.4f;
		weatherScale = 0.004f; baseNoiseScale = 0.0028f;
		warpScale = 0.003f; warpStrength = 50.0f;
		detailScale = 0.02f; detailErosionStrength = 0.16f;
		extinctionCoefficient = 0.08f; ambientStrength = 0.45f;
		phaseForwardG = 0.65f; multipleScattering = 0.65f;
		break;
	case Preset::BrokenStratocumulus:
		cloudType = 0.55f; coverageLow = 0.3f; coverageHigh = 1.0f;
		regionScale = 0.0005f; regionCoverage = 0.8f; densityVariation = 0.5f; heightVariation = 0.25f;
		weatherScale = 0.0032f; baseNoiseScale = 0.0045f;
		warpScale = 0.003f; warpStrength = 40.0f;
		detailScale = 0.016f; detailErosionStrength = 0.17f;
		extinctionCoefficient = 0.08f; ambientStrength = 0.5f;
		phaseForwardG = 0.6f; multipleScattering = 0.65f;
		break;
	case Preset::Overcast:
		cloudType = 0.0f; coverageLow = 0.8f; coverageHigh = 1.0f;
		regionScale = 0.0003f; regionCoverage = 1.0f; densityVariation = 0.25f; heightVariation = 0.1f;
		weatherScale = 0.0012f; baseNoiseScale = 0.0012f;
		warpScale = 0.0015f; warpStrength = 25.0f;
		detailScale = 0.01f; detailErosionStrength = 0.07f;
		extinctionCoefficient = 0.12f; ambientStrength = 0.5f;
		phaseForwardG = 0.55f; multipleScattering = 0.65f;
		break;
	}
}

void ModelRenderComponent::setModel(std::shared_ptr<Model> model) {
	this->model = model;
	this->model->buildAABB(aabb.min, aabb.max);
}

void StaticMeshComponent::setMesh(std::shared_ptr<Mesh> mesh) {
	this->mesh = mesh;
	this->mesh->buildAABB(aabb.min, aabb.max);
}

std::string Component::getName() const {
	return name;
}

void SkyboxComponent::setSkybox(std::string folderPath) {
	skybox = std::make_shared<Skybox>(folderPath);
	skybox->initGLResources();
	// keep the edit buffer in sync so the inspector shows the applied path
	size_t n = std::min(folderPath.size(), sizeof(this->folderPath) - 1);
	folderPath.copy(this->folderPath, n);
	this->folderPath[n] = '\0';
}

void DynamicMaterialComponent::setMaterial() {
	material.albedoPath = albedoPath;
	material.specularPath = specularPath;
	material.normalPath = normalPath;
	material.initGLResources();
}

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

SkeletonViewerComponent::SkeletonViewerComponent(const std::vector<Node>& nodes)
	: Component("SkeletonViewerComponent"), nodes(nodes), show(false) {
	glGenVertexArrays(1, &VAO);
	glGenBuffers(1, &VBO);
	glGenVertexArrays(1, &lineVAO);
	glGenBuffers(1, &lineVBO);
}

SkeletonViewerComponent::~SkeletonViewerComponent() {
	if (VAO) glDeleteVertexArrays(1, &VAO);
	if (VBO) glDeleteBuffers(1, &VBO);
	if (lineVAO) glDeleteVertexArrays(1, &lineVAO);
	if (lineVBO) glDeleteBuffers(1, &lineVBO);
}

SkeletonViewerComponent::SkeletonViewerComponent(SkeletonViewerComponent&& other) noexcept
	: Component(std::move(other)),
	  nodes(std::move(other.nodes)),
	  VAO(other.VAO), VBO(other.VBO), lineVAO(other.lineVAO), lineVBO(other.lineVBO) {
	show = other.show;
	other.VAO = other.VBO = other.lineVAO = other.lineVBO = 0;
}

SkeletonViewerComponent& SkeletonViewerComponent::operator=(SkeletonViewerComponent&& other) noexcept {
	if (this != &other) {
		if (VAO) glDeleteVertexArrays(1, &VAO);
		if (VBO) glDeleteBuffers(1, &VBO);
		if (lineVAO) glDeleteVertexArrays(1, &lineVAO);
		if (lineVBO) glDeleteBuffers(1, &lineVBO);
		Component::operator=(std::move(other));
		nodes = std::move(other.nodes);
		VAO = other.VAO; VBO = other.VBO; lineVAO = other.lineVAO; lineVBO = other.lineVBO;
		show = other.show;
		other.VAO = other.VBO = other.lineVAO = other.lineVBO = 0;
	}
	return *this;
}

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
	for (const auto& node : nodes) {
		if (!node.isBoneNode || node.parentIndex < 0 ||
			static_cast<size_t>(node.parentIndex) >= nodes.size()) continue;
		const auto& parent = nodes[node.parentIndex];
		if (parent.isBoneNode) {
			lineVertices.push_back(node.position);
			lineVertices.push_back(parent.position);
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
