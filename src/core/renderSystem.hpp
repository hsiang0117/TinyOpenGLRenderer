#ifndef RENDERSYSTEM_HPP
#define RENDERSYSTEM_HPP
#pragma once

#include "../camera.hpp"
#include "../shader.hpp"
#include "../model.hpp"
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
	Shader shader;
	std::vector<Model> renderList;
	std::future<Model> modelFuture;
};

void RenderSystem::init() {
	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
		std::cout << "Failed to initialize GLAD" << std::endl;
		exit(EXIT_FAILURE);
	}
	int viewport[4];
	glGetIntegerv(GL_VIEWPORT, viewport);
	width = viewport[2];
	height = viewport[3];
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);

	glGenBuffers(1, &uboMatrices);
	glBindBuffer(GL_UNIFORM_BUFFER, uboMatrices);
	glBufferData(GL_UNIFORM_BUFFER, 2 * sizeof(glm::mat4), NULL, GL_STATIC_DRAW);
	glBindBufferRange(GL_UNIFORM_BUFFER, 0, uboMatrices, 0, 2 * sizeof(glm::mat4));
	glBindBuffer(GL_UNIFORM_BUFFER, 0);

	shader = Shader("data/shader/test.vert", "data/shader/test.frag");
	modelFuture = Model::LoadAsync("data/model/nanosuit_reflection/nanosuit.obj");
}

void RenderSystem::update() {
	if (Input::getInstance().isWindowResized()) {
		width = Input::getInstance().getWindowWidth();
		height = Input::getInstance().getWindowHeight();
		glViewport(0, 0, width, height);
	}
}

void RenderSystem::render(Camera& camera) {
	glBindBuffer(GL_UNIFORM_BUFFER, uboMatrices);
	glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(glm::mat4), glm::value_ptr(camera.getViewMat()));
	glBufferSubData(GL_UNIFORM_BUFFER, sizeof(glm::mat4), sizeof(glm::mat4), glm::value_ptr(camera.getProjectionMat((float)width, (float)height)));
	glBindBuffer(GL_UNIFORM_BUFFER, 0);

	glClearColor(0.1, 0.1f, 0.1f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	if (modelFuture.valid() && modelFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
		Model model = modelFuture.get();
		if (model.initGLResources()) {
			renderList.push_back(model);
		}
	}

	shader.use();
	for (int i = 0; i < renderList.size(); i++) {
		renderList[i].draw(shader);
	}
}
#endif // !RENDERSYSTEM_HPP