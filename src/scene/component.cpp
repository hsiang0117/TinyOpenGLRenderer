#include "component.hpp"
#include "rendering/geometry/mesh.hpp"
#include "assets/model.hpp"
#include "rendering/skybox.hpp"
#include "core/logger.hpp"
#include <system_error>
#include <utility>

namespace togr {

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
    model_ = std::move(model);
    model_->buildAABB(bounds_.min, bounds_.max);
}

void StaticMeshComponent::setMesh(std::shared_ptr<Mesh> mesh) {
    mesh_ = std::move(mesh);
    mesh_->buildAABB(bounds_.min, bounds_.max);
}

std::string Component::getName() const {
	return name;
}

bool SkyboxComponent::setSkybox(std::string folderPath) {
    try {
        auto candidate = std::make_shared<Skybox>(folderPath);
        if (!candidate->initGLResources()) {
            lastError_ = candidate->lastError();
            return false;
        }
        skybox_ = std::move(candidate);
        // The by-value argument also permits setSkybox(draftFolderPath).
        draftFolderPath = std::move(folderPath);
        lastError_.clear();
        return true;
    } catch (const std::system_error& error) {
        lastError_ = "Invalid skybox path: " + folderPath + ": " + error.what();
        TOGR_LOG_ERROR(lastError_);
        return false;
    }
}

bool DynamicMaterialComponent::applyMaterial() {
    Material candidate;
    candidate.albedoPath = draft.albedoPath;
    candidate.specularPath = draft.specularPath;
    candidate.normalPath = draft.normalPath;
    candidate.ambientPath = material_.ambientPath;
    candidate.shininessPath = material_.shininessPath;
    if (!candidate.initGLResources()) {
        lastError_ = candidate.lastError();
        return false;
    }
    material_ = std::move(candidate);
    lastError_.clear();
    return true;
}

void DynamicMaterialComponent::bindMaterial(std::shared_ptr<Shader> shader) {
    material_.bind(std::move(shader));
}

AnimatorComponent::AnimatorComponent(std::shared_ptr<const Model> model)
    : Component("AnimatorComponent"), playing(false), model_(std::move(model)), animator(model_->getNodes()) {}

std::vector<std::string> AnimatorComponent::getAnimationsNames() const {
    std::vector<std::string> names;
    for (const auto& clip : model_->getAnimations()) {
        names.push_back(clip.getName());
    }
    return names;
}

void AnimatorComponent::playAnimation(std::string name) {
    for (const auto& clip : model_->getAnimations()) {
        if (clip.getName() == name) {
            animator.playAnimation(&clip);
            return;
        }
    }
}

std::string AnimatorComponent::getCurrentAnimation() const {
    const Animation* clip = animator.getCurrentAnimation();
    return clip ? clip->getName() : std::string();
}

SkeletonViewerComponent::SkeletonViewerComponent()
    : Component("SkeletonViewerComponent"), show(false) {
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
    : Component(std::move(other)), show(other.show),
      VAO(other.VAO), VBO(other.VBO), lineVAO(other.lineVAO), lineVBO(other.lineVBO) {
    other.VAO = 0;
    other.VBO = 0;
    other.lineVAO = 0;
    other.lineVBO = 0;
}

SkeletonViewerComponent& SkeletonViewerComponent::operator=(SkeletonViewerComponent&& other) noexcept {
    if (this != &other) {
        if (VAO) glDeleteVertexArrays(1, &VAO);
        if (VBO) glDeleteBuffers(1, &VBO);
        if (lineVAO) glDeleteVertexArrays(1, &lineVAO);
        if (lineVBO) glDeleteBuffers(1, &lineVBO);
        Component::operator=(std::move(other));
        VAO = other.VAO;
        VBO = other.VBO;
        lineVAO = other.lineVAO;
        lineVBO = other.lineVBO;
        show = other.show;
        other.VAO = 0;
        other.VBO = 0;
        other.lineVAO = 0;
        other.lineVBO = 0;
    }
    return *this;
}

void SkeletonViewerComponent::drawSkeleton(const std::vector<Node>& nodes, const SkeletonPose* pose) {
    if (!show) return;
    const bool hasPose = pose && pose->nodePositions.size() == nodes.size();
    const auto positionAt = [&](size_t index) -> const glm::vec3& {
        return hasPose ? pose->nodePositions[index] : nodes[index].bindPosition;
    };

    std::vector<glm::vec3> bonePositions;
    std::vector<glm::vec3> lineVertices;
    for (size_t index = 0; index < nodes.size(); ++index) {
        const Node& node = nodes[index];
        if (!node.isBoneNode) continue;
        bonePositions.push_back(positionAt(index));
        if (node.parentIndex < 0 || static_cast<size_t>(node.parentIndex) >= nodes.size()) continue;
        if (nodes[node.parentIndex].isBoneNode) {
            lineVertices.push_back(positionAt(index));
            lineVertices.push_back(positionAt(node.parentIndex));
        }
    }

    if (!bonePositions.empty()) {
        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, bonePositions.size() * sizeof(glm::vec3), bonePositions.data(), GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
        glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(bonePositions.size()));
        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
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

} // namespace togr
