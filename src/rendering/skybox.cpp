#include "skybox.hpp"

Skybox::Skybox(std::string folderPath) { this->folderPath = folderPath; }

Skybox::~Skybox() {
	if (VAO) glDeleteVertexArrays(1, &VAO);
	if (VBO) glDeleteBuffers(1, &VBO);
}

Skybox::Skybox(Skybox&& other) noexcept
	: folderPath(std::move(other.folderPath)),
	  glInitialized(other.glInitialized),
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
	if (glInitialized) return false;
	glGenVertexArrays(1, &VAO);
	glGenBuffers(1, &VBO);
	glBindVertexArray(VAO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), &vertices, GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * (sizeof(float)), (void*)0);
	glEnableVertexAttribArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
	cubemap = CubeMap(folderPath.c_str(), GL_CLAMP_TO_EDGE, GL_LINEAR);
	if (cubemap.ID == 0) return false;
	glInitialized = true;
	return true;
}

void Skybox::draw()
{
	cubemap.use(GL_TEXTURE5);
	glBindVertexArray(VAO);
	glDrawArrays(GL_TRIANGLES, 0, 36);
	glBindVertexArray(0);
}

void Skybox::useCubeMap()
{
	cubemap.use(GL_TEXTURE5);
}
