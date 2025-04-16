#ifndef RENDERSYSTEM_HPP
#define RENDERSYSTEM_HPP
#pragma once

#include "../glBuffer.hpp"
#include "resourceManager.hpp"
#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>
#include <GLFW/glfw3.h>

class RenderSystem {
public:
	RenderSystem() = default;
	~RenderSystem() = default;
	void init();
	void update();
	void render(Camera& camera);
private:
	int width, height; //viewport width and height
	UniformBuffer uboMatrices;
	ShaderStorageBuffer ssboPointLights, ssboDirectionLight, ssboSpotLights;
	FrameBuffer shadowMapFrameBuffer;
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



	uboMatrices.init();
	ssboPointLights.init();
	ssboDirectionLight.init();
	ssboSpotLights.init();

	uboMatrices.bind();
	uboMatrices.bufferBase(0);
	uboMatrices.bufferData(2 * sizeof(glm::mat4), NULL);
	uboMatrices.unbind();

	ssboPointLights.bind();
	ssboPointLights.bufferBase(1);
	ssboPointLights.bufferData(100 * PointLightObject::glslSize, NULL);
	ssboPointLights.unbind();

	ssboDirectionLight.bind();
	ssboDirectionLight.bufferBase(2);
	ssboDirectionLight.bufferData(DirectionLightObject::glslSize, NULL);
	ssboDirectionLight.unbind();
	
	ssboSpotLights.bind();
	ssboSpotLights.bufferBase(3);
	ssboSpotLights.bufferData(50 * SpotLightObject::glslSize, NULL);
	ssboSpotLights.unbind();
}

void RenderSystem::update() {
	if (Input::getInstance().isWindowResized()) {
		width = Input::getInstance().getWindowWidth() - 250 - 300;
		height = Input::getInstance().getWindowHeight() - 250;
		glViewport(300, 250, width, height);
	}
}

void RenderSystem::render(Camera& camera) {
	uboMatrices.bind();
	uboMatrices.bufferSubdata(0, sizeof(glm::mat4), glm::value_ptr(camera.getViewMat()));
	uboMatrices.bufferSubdata(sizeof(glm::mat4), sizeof(glm::mat4), glm::value_ptr(camera.getProjectionMat((float)width, (float)height)));
	uboMatrices.unbind();

	glClearColor(0.1, 0.1f, 0.1f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	ShaderPtr defaultShader = ResourceManager::getInstance().shaderCache["default"];
	ShaderPtr skyboxShader = ResourceManager::getInstance().shaderCache["skybox"];

	// lightPass
	defaultShader->use();
	int pointLightIndex = 0, spotLightIndex = 0;
	for (int i = 0; i < ResourceManager::getInstance().gameObjects.size(); i++) {
		GameObjectPtr object = ResourceManager::getInstance().gameObjects[i];
		if (object->getType() == GameObject::Type::POINTLIGHTOBJECT) {
			object->sendToSSBO(pointLightIndex, ssboPointLights);
			pointLightIndex++;
		}
		else if (object->getType() == GameObject::Type::SPOTLIGHTOBJECT) {
			object->sendToSSBO(spotLightIndex, ssboSpotLights);
			spotLightIndex++;
		}
		else if (object->getType() == GameObject::Type::DIRECTIONLIGHTOBJECT) {
			object->sendToSSBO(0, ssboDirectionLight);
		}
	}
	defaultShader->setInt("pointLightNum", ResourceManager::getInstance().pointLightNum);
	defaultShader->setInt("directionLightNum", ResourceManager::getInstance().directionLightNum);
	defaultShader->setInt("spotLightNum", ResourceManager::getInstance().spotLightNum);

	//normalPass
	for (int i = 0; i < ResourceManager::getInstance().gameObjects.size(); i++) {
		GameObjectPtr object = ResourceManager::getInstance().gameObjects[i];
		if (object->getType() == GameObject::Type::RENDEROBJECT) {
			object->draw(defaultShader);
		}		
		if (object->getType() == GameObject::Type::SKYBOXOBJECT) {
			object->useCubeMap(defaultShader);
		}
	}

	skyboxShader->use();
	for (int i = 0; i < ResourceManager::getInstance().gameObjects.size(); i++) {
		GameObjectPtr object = ResourceManager::getInstance().gameObjects[i];
		if (object->getType() == GameObject::Type::SKYBOXOBJECT) {
			object->draw(skyboxShader);
		}
	}
}
#endif // !RENDERSYSTEM_HPP