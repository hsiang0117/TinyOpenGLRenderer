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
	FrameBuffer directionLightDepthFBO;
	Texture2D directionLightDepthTexture;

	void drawScreenQuad();
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

	directionLightDepthFBO.init();
	GLenum attachments[1] = { GL_NONE };
	directionLightDepthFBO.drawBuffers(attachments);
	directionLightDepthFBO.readBuffer(GL_NONE);
	directionLightDepthTexture = Texture2D(1024, 1024, GL_CLAMP_TO_BORDER, GL_NEAREST, GL_DEPTH_COMPONENT, GL_DEPTH_COMPONENT);
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
	ShaderPtr depthShader = ResourceManager::getInstance().shaderCache["depth"];
	ShaderPtr screenQuadShader = ResourceManager::getInstance().shaderCache["screenQuad"];

	//shadowmapPass
	//depthShader->use();
	//for (int i = 0; i < ResourceManager::getInstance().gameObjects.size(); i++) {
	//	GameObjectPtr object = ResourceManager::getInstance().gameObjects[i];
	//	if (object->getType() == GameObject::Type::DIRECTIONLIGHTOBJECT) {
	//		glm::mat4 lightMatrices = object->getLightMatrices();
	//		depthShader->setMat4("lightMatrices", lightMatrices);
	//		glViewport(0, 0, 1024, 1024);
	//		directionLightDepthFBO.bind();
	//		directionLightDepthFBO.attachTexture2D(directionLightDepthTexture, GL_DEPTH_ATTACHMENT);
	//		glClear(GL_DEPTH_BUFFER_BIT);
	//		for (int j = 0; j < ResourceManager::getInstance().gameObjects.size(); j++) {
	//			GameObjectPtr object = ResourceManager::getInstance().gameObjects[j];
	//			if (object->getType() == GameObject::Type::RENDEROBJECT) {
	//				object->draw(depthShader);
	//			}
	//		}
	//		directionLightDepthFBO.unbind();
	//	}
	//}

	//screenQuadShader->use();
	//screenQuadShader->setInt("directionLightDepth", 0);
	//directionLightDepthTexture.use(GL_TEXTURE0);
	//glViewport(300, 250, width, height);
	//glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	//drawScreenQuad();

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
	defaultShader->setVec3("cameraPos", camera.getPos());
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
void RenderSystem::drawScreenQuad()
{
	static GLuint quadVAO, quadVBO;

	if (!quadVAO) {
		float quadVertices[] = {
			// positions   // texCoords
			-1.0f,  1.0f, 0.0f, 1.0f,
			-1.0f, -1.0f, 0.0f, 0.0f,
			 1.0f,  1.0f, 1.0f, 1.0f,
			 1.0f, -1.0f, 1.0f, 0.0f,
		};
		glGenVertexArrays(1, &quadVAO);
		glGenBuffers(1, &quadVBO);
		glBindVertexArray(quadVAO);
		glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
		glEnableVertexAttribArray(1);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);
	}
	glBindVertexArray(quadVAO);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	glBindVertexArray(0);
}
#endif // !RENDERSYSTEM_HPP