#ifndef RENDERSYSTEM_HPP
#define RENDERSYSTEM_HPP
#pragma once

#include "resourceManager.hpp"
#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>
#include <GLFW/glfw3.h>

class RenderSystem {
public:
	RenderSystem() {};
	void init();
	void update();
	void render(Camera& camera);
private:
	int width, height; //viewport width and height
	GLuint uboMatrices; //uniform buffer object for view and projection matrices
};

void RenderSystem::init() {
	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
		std::cout << "Failed to initialize GLAD" << std::endl;
		exit(EXIT_FAILURE);
	}
	int viewport[4];
	glGetIntegerv(GL_VIEWPORT, viewport);
	width = viewport[2] - 250 - 300;
	height = viewport[3] - 250;
	glViewport(300, 250, width, height);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);

	glGenBuffers(1, &uboMatrices);
	glBindBuffer(GL_UNIFORM_BUFFER, uboMatrices);
	glBufferData(GL_UNIFORM_BUFFER, 2 * sizeof(glm::mat4), NULL, GL_STATIC_DRAW);
	glBindBufferRange(GL_UNIFORM_BUFFER, 0, uboMatrices, 0, 2 * sizeof(glm::mat4));
	glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

void RenderSystem::update() {
	if (Input::getInstance().isWindowResized()) {
		width = Input::getInstance().getWindowWidth() - 250 - 300;
		height = Input::getInstance().getWindowHeight() - 250;
		glViewport(300, 250, width, height);
	}
}

void RenderSystem::render(Camera& camera) {
	glBindBuffer(GL_UNIFORM_BUFFER, uboMatrices);
	glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(glm::mat4), glm::value_ptr(camera.getViewMat()));
	glBufferSubData(GL_UNIFORM_BUFFER, sizeof(glm::mat4), sizeof(glm::mat4), glm::value_ptr(camera.getProjectionMat((float)width, (float)height)));
	glBindBuffer(GL_UNIFORM_BUFFER, 0);

	glClearColor(0.1, 0.1f, 0.1f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	ResourceManager::getInstance().shaderCache["default"].get()->use();
	for (int i = 0; i < ResourceManager::getInstance().gameObjects.size(); i++) {
		ResourceManager::getInstance().gameObjects[i]->draw(ResourceManager::getInstance().shaderCache["default"]);
	}
}
#endif // !RENDERSYSTEM_HPP