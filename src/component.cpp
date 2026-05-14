#include "component.hpp"

std::string Component::getName() const {
	return name;
}

void SkyBoxComponent::setSkyBox(std::string folderPath) {
	skybox = std::make_shared<SkyBox>(folderPath);
	skybox->initGLResources();
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
