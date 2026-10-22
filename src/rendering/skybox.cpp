#include "skybox.hpp"
#include "core/logger.hpp"
#include <utility>

namespace togr {

Skybox::Skybox(std::string folderPath) : folderPath(std::filesystem::u8path(folderPath)) {}

Skybox::~Skybox() {
	if (VAO) glDeleteVertexArrays(1, &VAO);
	if (VBO) glDeleteBuffers(1, &VBO);
}

Skybox::Skybox(Skybox&& other) noexcept
	: folderPath(std::move(other.folderPath)),
	  glInitialized(other.glInitialized),
	  lastError_(std::move(other.lastError_)),
	  cubemap(std::move(other.cubemap)),
	  VAO(other.VAO), VBO(other.VBO) {
	other.VAO = 0;
	other.VBO = 0;
	other.glInitialized = false;
}

Skybox& Skybox::operator=(Skybox&& other) noexcept {
	if (this != &other) {
		if (VAO) glDeleteVertexArrays(1, &VAO);
		if (VBO) glDeleteBuffers(1, &VBO);
		folderPath = std::move(other.folderPath);
		glInitialized = other.glInitialized;
		lastError_ = std::move(other.lastError_);
		cubemap = std::move(other.cubemap);
		VAO = other.VAO;
		VBO = other.VBO;
		other.VAO = 0;
		other.VBO = 0;
		other.glInitialized = false;
	}
	return *this;
}

bool Skybox::initGLResources() {
	if (glInitialized) return true;
	lastError_.clear();
	CubeMap candidate(folderPath, GL_CLAMP_TO_EDGE, GL_LINEAR);
	if (!candidate.isValid()) {
		lastError_ = candidate.lastError();
		return false;
	}
	glGenVertexArrays(1, &VAO);
	glGenBuffers(1, &VBO);
	if (!VAO || !VBO) {
		if (VAO) glDeleteVertexArrays(1, &VAO);
		if (VBO) glDeleteBuffers(1, &VBO);
		VAO = VBO = 0;
		lastError_ = "Cannot allocate skybox geometry: " + folderPath.u8string();
		TOGR_LOG_ERROR(lastError_);
		return false;
	}
	glBindVertexArray(VAO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), &vertices, GL_STATIC_DRAW);
	GLint bufferSize = 0;
	glGetBufferParameteriv(GL_ARRAY_BUFFER, GL_BUFFER_SIZE, &bufferSize);
	if (bufferSize != sizeof(vertices)) {
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);
		glDeleteVertexArrays(1, &VAO);
		glDeleteBuffers(1, &VBO);
		VAO = VBO = 0;
		lastError_ = "Cannot upload skybox geometry: " + folderPath.u8string();
		TOGR_LOG_ERROR(lastError_);
		return false;
	}
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * (sizeof(float)), (void*)0);
	glEnableVertexAttribArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
	cubemap = std::move(candidate);
	glInitialized = true;
	return true;
}

void Skybox::draw()
{
	if (!isReady()) return;
	cubemap.use(GL_TEXTURE5);
	glBindVertexArray(VAO);
	glDrawArrays(GL_TRIANGLES, 0, 36);
	glBindVertexArray(0);
}

void Skybox::useCubeMap()
{
	if (!isReady()) return;
	cubemap.use(GL_TEXTURE5);
}

} // namespace togr
