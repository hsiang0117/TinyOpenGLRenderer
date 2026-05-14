#include "skybox.hpp"

SkyBox::SkyBox(std::string folderPath) { this->folderPath = folderPath; }

bool SkyBox::initGLResources() {
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

void SkyBox::draw()
{
	cubemap.use(GL_TEXTURE5);
	glBindVertexArray(VAO);
	glDrawArrays(GL_TRIANGLES, 0, 36);
	glBindVertexArray(0);
}

void SkyBox::useCubeMap()
{
	cubemap.use(GL_TEXTURE5);
}
