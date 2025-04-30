#ifndef RENDERSYSTEM_HPP
#define RENDERSYSTEM_HPP
#pragma once

#include "guiSystem.hpp"
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
	float x, y, width, height; //viewport width and height
	UniformBuffer uboMatrices;
	ShaderStorageBuffer ssboPointLights, ssboDirectionLight, ssboSpotLights;
	FrameBuffer directionLightDepthFBO, pointLightDepthFBO;
	Texture2D directionLightDepthTexture;
	CubeMapArray pointLightDepthTexture;
	void drawScreenQuad();
};

void RenderSystem::init() {
	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
		std::cout << "Failed to initialize GLAD" << std::endl;
		exit(EXIT_FAILURE);
	}
	x = GuiSystem::leftSideBarWidth;
	y = GuiSystem::bottomSideBarHeight;
	width = Input::getInstance().getWindowWidth() - GuiSystem::leftSideBarWidth - GuiSystem::rightSideBarWidth;
	height = Input::getInstance().getWindowHeight() - GuiSystem::bottomSideBarHeight;
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
	GLenum attachments1[1] = { GL_NONE };
	directionLightDepthFBO.drawBuffers(attachments1);
	directionLightDepthFBO.readBuffer(GL_NONE);
	directionLightDepthTexture = Texture2D(1024, 1024, GL_CLAMP_TO_BORDER, GL_NEAREST, GL_DEPTH_COMPONENT, GL_DEPTH_COMPONENT);
	directionLightDepthTexture.setBorderColor(1.0, 1.0, 1.0, 1.0);

	pointLightDepthFBO.init();
	GLenum attachments2[1] = { GL_NONE };
	pointLightDepthFBO.drawBuffers(attachments2);
	pointLightDepthFBO.readBuffer(GL_NONE);
	pointLightDepthTexture = CubeMapArray(1024, 1024, 10, GL_CLAMP_TO_BORDER, GL_NEAREST, GL_DEPTH_COMPONENT, GL_DEPTH_COMPONENT);
}

void RenderSystem::update() {
	if (Input::getInstance().isWindowResized()) {
		width = Input::getInstance().getWindowWidth() - GuiSystem::leftSideBarWidth - GuiSystem::rightSideBarWidth;
		height = Input::getInstance().getWindowHeight() - GuiSystem::bottomSideBarHeight;
	}
	if (Input::getInstance().isUiResized()) {
		x = GuiSystem::leftSideBarWidth;
		y = GuiSystem::bottomSideBarHeight;
		width = Input::getInstance().getWindowWidth() - GuiSystem::leftSideBarWidth - GuiSystem::rightSideBarWidth;
		height = Input::getInstance().getWindowHeight() - GuiSystem::bottomSideBarHeight;
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
	ShaderPtr depthCubeShader = ResourceManager::getInstance().shaderCache["depthCube"];

	glViewport(0, 0, 1024, 1024);
	//shadowmapPass
	int pointLightIndex = 0;
	for (int i = 0; i < ResourceManager::getInstance().gameObjects.size(); i++) {
		GameObjectPtr object = ResourceManager::getInstance().gameObjects[i];
		if (object->getType() == GameObject::Type::DIRECTIONLIGHTOBJECT) {
			depthShader->use();
			auto shadowCaster = object->getComponent<ShadowCaster2D>();
			if (shadowCaster->enabled) {
				glm::mat4 lightMatrices = object->getLightMatrices();
				depthShader->setMat4("lightMatrices", lightMatrices);
				directionLightDepthFBO.bind();
				directionLightDepthFBO.attachTexture2D(directionLightDepthTexture, GL_DEPTH_ATTACHMENT);
				glClear(GL_DEPTH_BUFFER_BIT);
				glCullFace(GL_FRONT);
				for (int j = 0; j < ResourceManager::getInstance().gameObjects.size(); j++) {
					GameObjectPtr object = ResourceManager::getInstance().gameObjects[j];
					if (object->getType() == GameObject::Type::RENDEROBJECT) {
						object->draw(depthShader);
					}
				}
				glCullFace(GL_BACK);
				directionLightDepthFBO.unbind();
			}
			else {
				directionLightDepthFBO.bind();
				directionLightDepthFBO.attachTexture2D(directionLightDepthTexture, GL_DEPTH_ATTACHMENT);
				glClear(GL_DEPTH_BUFFER_BIT);
				directionLightDepthFBO.unbind();
			}
		}
		else if (object->getType() == GameObject::Type::POINTLIGHTOBJECT) {
			depthCubeShader->use();
			auto shadowCaster = object->getComponent<ShadowCasterCube>();
			if (shadowCaster->enabled) {
				auto transform = object->getComponent<Transform>();
				depthCubeShader->setVec3("lightPos", transform->translate);
				depthCubeShader->setFloat("farPlane", shadowCaster->farPlane);
				depthCubeShader->setInt("lightIndex", pointLightIndex);
				std::vector<glm::mat4> lightMatrices = object->getLightMatricesCube();
				for (int j = 0; j < 6; j++) {
					depthCubeShader->setMat4(("lightMatrices[" + std::to_string(j) + "]").c_str(), lightMatrices[j]);
				}
				pointLightDepthFBO.bind();
				for (int j = 0; j < 6; j++) {
					pointLightDepthFBO.attachTextureLayer(pointLightDepthTexture, GL_DEPTH_ATTACHMENT, pointLightIndex * 6 + j);
					glClear(GL_DEPTH_BUFFER_BIT);
				}
				pointLightDepthFBO.attachTexture(pointLightDepthTexture, GL_DEPTH_ATTACHMENT);
				for (int k = 0; k < ResourceManager::getInstance().gameObjects.size(); k++) {
					GameObjectPtr object = ResourceManager::getInstance().gameObjects[k];
					if (object->getType() == GameObject::Type::RENDEROBJECT) {
						object->draw(depthCubeShader);
					}
				}
				pointLightDepthFBO.unbind();
			}
			else {
				pointLightDepthFBO.bind();				
				for (int j = 0; j < 6; j++) {
					pointLightDepthFBO.attachTextureLayer(pointLightDepthTexture, GL_DEPTH_ATTACHMENT, pointLightIndex * 6 + j);
					glClear(GL_DEPTH_BUFFER_BIT);
				}
				pointLightDepthFBO.unbind();
			}
			pointLightIndex++;
		}
	}

	glViewport(x, y, width, height);
	screenQuadShader->use();
	screenQuadShader->setInt("depthMap", 6);
	drawScreenQuad();

	// lightPass
	//defaultShader->use();
	//int pointLightIndex = 0, spotLightIndex = 0;
	//for (int i = 0; i < ResourceManager::getInstance().gameObjects.size(); i++) {
	//	GameObjectPtr object = ResourceManager::getInstance().gameObjects[i];
	//	if (object->getType() == GameObject::Type::POINTLIGHTOBJECT) {
	//		object->sendToSSBO(pointLightIndex, ssboPointLights);
	//		pointLightIndex++;
	//	}
	//	else if (object->getType() == GameObject::Type::SPOTLIGHTOBJECT) {
	//		object->sendToSSBO(spotLightIndex, ssboSpotLights);
	//		spotLightIndex++;
	//	}
	//	else if (object->getType() == GameObject::Type::DIRECTIONLIGHTOBJECT) {
	//		object->sendToSSBO(0, ssboDirectionLight);
	//		auto shadowCaster = object->getComponent<ShadowCaster2D>();
	//		if (shadowCaster->enabled) {
	//			defaultShader->setMat4("lightSpaceMatrix", object->getLightMatrices());
	//			directionLightDepthTexture.use(GL_TEXTURE6);
	//		}
	//	}
	//}
	//defaultShader->setInt("pointLightNum", ResourceManager::getInstance().pointLightNum);
	//defaultShader->setInt("directionLightNum", ResourceManager::getInstance().directionLightNum);
	//defaultShader->setInt("spotLightNum", ResourceManager::getInstance().spotLightNum);
	//defaultShader->setInt("shadowMap", 6);

	////normalPass
	//defaultShader->setVec3("cameraPos", camera.getPos());
	//for (int i = 0; i < ResourceManager::getInstance().gameObjects.size(); i++) {
	//	GameObjectPtr object = ResourceManager::getInstance().gameObjects[i];
	//	if (object->getType() == GameObject::Type::RENDEROBJECT) {
	//		object->draw(defaultShader);
	//	}		
	//	if (object->getType() == GameObject::Type::SKYBOXOBJECT) {
	//		object->useCubeMap(defaultShader);
	//	}
	//}

	//skyboxShader->use();
	//for (int i = 0; i < ResourceManager::getInstance().gameObjects.size(); i++) {
	//	GameObjectPtr object = ResourceManager::getInstance().gameObjects[i];
	//	if (object->getType() == GameObject::Type::SKYBOXOBJECT) {
	//		object->draw(skyboxShader);
	//	}
	//}
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