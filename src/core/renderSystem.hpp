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
	FrameBuffer directionLightDepthFBO, pointLightDepthFBO, hdrFBO, pingpongFBO[2];
	Texture2D directionLightDepthTexture, hdrTexture, brightTexture, pingpongTexture[2];
	RenderBuffer hdrDepthBuffer;
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
	directionLightDepthTexture = Texture2D(1024, 1024, GL_CLAMP_TO_BORDER, GL_NEAREST, GL_DEPTH_COMPONENT, GL_DEPTH_COMPONENT, GL_FLOAT);
	directionLightDepthTexture.setBorderColor(1.0, 1.0, 1.0, 1.0);

	pointLightDepthFBO.init();
	GLenum attachments2[1] = { GL_NONE };
	pointLightDepthFBO.drawBuffers(attachments2);
	pointLightDepthFBO.readBuffer(GL_NONE);
	pointLightDepthTexture = CubeMapArray(1024, 1024, 10, GL_CLAMP_TO_BORDER, GL_NEAREST, GL_DEPTH_COMPONENT, GL_DEPTH_COMPONENT);

	hdrFBO.init();
	hdrTexture = Texture2D(width, height, GL_CLAMP_TO_BORDER, GL_LINEAR, GL_RGBA16F, GL_RGBA, GL_FLOAT);
	brightTexture = Texture2D(width, height, GL_CLAMP_TO_BORDER, GL_LINEAR, GL_RGBA16F, GL_RGBA, GL_FLOAT);
	hdrDepthBuffer = RenderBuffer(width, height, GL_DEPTH_COMPONENT);
	hdrFBO.bind();
	hdrFBO.attachTexture2D(hdrTexture, GL_COLOR_ATTACHMENT0);
	hdrFBO.attachTexture2D(brightTexture, GL_COLOR_ATTACHMENT1);
	hdrFBO.attachRenderBuffer(hdrDepthBuffer, GL_DEPTH_ATTACHMENT);
	GLenum attachments3[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
	hdrFBO.drawBuffers(attachments3);
	hdrFBO.unbind();

	pingpongFBO[0].init();
	pingpongFBO[1].init();
	pingpongTexture[0] = Texture2D(width, height, GL_CLAMP_TO_BORDER, GL_LINEAR, GL_RGBA16F, GL_RGBA, GL_FLOAT);
	pingpongTexture[1] = Texture2D(width, height, GL_CLAMP_TO_BORDER, GL_LINEAR, GL_RGBA16F, GL_RGBA, GL_FLOAT);
	pingpongFBO[0].bind();
	pingpongFBO[0].attachTexture2D(pingpongTexture[0], GL_COLOR_ATTACHMENT0);
	pingpongFBO[0].unbind();
	pingpongFBO[1].bind();
	pingpongFBO[1].attachTexture2D(pingpongTexture[1], GL_COLOR_ATTACHMENT0);
	pingpongFBO[1].unbind();
}

void RenderSystem::update() {
	if (Input::getInstance().isWindowResized()) {
		width = Input::getInstance().getWindowWidth() - GuiSystem::leftSideBarWidth - GuiSystem::rightSideBarWidth;
		height = Input::getInstance().getWindowHeight() - GuiSystem::bottomSideBarHeight;
		hdrTexture.resetSize(width, height);
		brightTexture.resetSize(width, height);
		hdrDepthBuffer.resetSize(width, height);
		pingpongTexture[0].resetSize(width, height);
		pingpongTexture[1].resetSize(width, height);
	}
	if (Input::getInstance().isUiResized()) {
		x = GuiSystem::leftSideBarWidth;
		y = GuiSystem::bottomSideBarHeight;
		width = Input::getInstance().getWindowWidth() - GuiSystem::leftSideBarWidth - GuiSystem::rightSideBarWidth;
		height = Input::getInstance().getWindowHeight() - GuiSystem::bottomSideBarHeight;
		hdrTexture.resetSize(width, height);
		brightTexture.resetSize(width, height);
		hdrDepthBuffer.resetSize(width, height);
		pingpongTexture[0].resetSize(width, height);
		pingpongTexture[1].resetSize(width, height);
	}
}

void RenderSystem::render(Camera& camera) {
	uboMatrices.bind();
	uboMatrices.bufferSubdata(0, sizeof(glm::mat4), glm::value_ptr(camera.getViewMat()));
	uboMatrices.bufferSubdata(sizeof(glm::mat4), sizeof(glm::mat4), glm::value_ptr(camera.getProjectionMat((float)width, (float)height)));
	uboMatrices.unbind();

	Frustum frustum = camera.getFrustum((float)width, (float)height);

	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	ShaderPtr defaultShader = ResourceManager::getInstance().shaderCache["default"];
	ShaderPtr skyboxShader = ResourceManager::getInstance().shaderCache["skybox"];
	ShaderPtr depthShader = ResourceManager::getInstance().shaderCache["depth"];
	ShaderPtr screenQuadShader = ResourceManager::getInstance().shaderCache["screenQuad"];
	ShaderPtr depthCubeShader = ResourceManager::getInstance().shaderCache["depthCube"];
	ShaderPtr lightCubeShader = ResourceManager::getInstance().shaderCache["lightCube"];
	ShaderPtr gaussianBlurShader = ResourceManager::getInstance().shaderCache["gaussianBlur"];

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
				glCullFace(GL_FRONT);
				for (int k = 0; k < ResourceManager::getInstance().gameObjects.size(); k++) {
					GameObjectPtr object = ResourceManager::getInstance().gameObjects[k];
					if (object->getType() == GameObject::Type::RENDEROBJECT) {
						object->draw(depthCubeShader);
					}
				}
				glCullFace(GL_BACK);
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

	// lightProcessingPass
	defaultShader->use();
	pointLightIndex = 0;
	int spotLightIndex = 0;
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
			auto shadowCaster = object->getComponent<ShadowCaster2D>();
			if (shadowCaster->enabled) {
				defaultShader->setMat4("lightSpaceMatrix", object->getLightMatrices());
				directionLightDepthTexture.use(GL_TEXTURE6);
			}
		}
	}
	defaultShader->setInt("pointLightNum", ResourceManager::getInstance().pointLightNum);
	defaultShader->setInt("directionLightNum", ResourceManager::getInstance().directionLightNum);
	defaultShader->setInt("spotLightNum", ResourceManager::getInstance().spotLightNum);
	defaultShader->setInt("albedoMap", 0);
	defaultShader->setInt("ambientMap", 1);
	defaultShader->setInt("specularMap", 2);
	defaultShader->setInt("normalMap", 3);
	defaultShader->setInt("shininessMap", 4);
	defaultShader->setInt("skyBox", 5);
	defaultShader->setInt("shadowMap", 6);
	defaultShader->setInt("shadowMapArray", 7);
	pointLightDepthTexture.use(GL_TEXTURE7);

	glViewport(0, 0, width, height);
	// normalPass
	hdrFBO.bind();
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	defaultShader->use();
	defaultShader->setVec3("cameraPos", camera.getPos());
	for (int i = 0; i < ResourceManager::getInstance().gameObjects.size(); i++) {
		GameObjectPtr object = ResourceManager::getInstance().gameObjects[i];
		if (object->getType() == GameObject::Type::RENDEROBJECT && object->isOnFrustum(frustum)) {
			object->draw(defaultShader);
		}
		else if (object->getType() == GameObject::Type::SKYBOXOBJECT) {
			object->useCubeMap(defaultShader);
		}
	}

	lightCubeShader->use();
	for (int i = 0; i < ResourceManager::getInstance().gameObjects.size(); i++) {
		GameObjectPtr object = ResourceManager::getInstance().gameObjects[i];
		if (object->getType() == GameObject::Type::POINTLIGHTOBJECT && object->isOnFrustum(frustum)) {
			object->draw(lightCubeShader);
		}
	}

	skyboxShader->use();
	for (int i = 0; i < ResourceManager::getInstance().gameObjects.size(); i++) {
		GameObjectPtr object = ResourceManager::getInstance().gameObjects[i];
		if (object->getType() == GameObject::Type::SKYBOXOBJECT) {
			object->draw(skyboxShader);
		}
	}
	hdrFBO.unbind();

	glViewport(0, 0, width, height);
	gaussianBlurShader->use();
	for (int i = 0; i < 10; i++) {
		pingpongFBO[i % 2].bind();
		glClear(GL_COLOR_BUFFER_BIT);
		gaussianBlurShader->setInt("image", 0);
		gaussianBlurShader->setBool("horizontal", i % 2 == 0);
		if (i == 0) {
			brightTexture.use(GL_TEXTURE0);
		}
		else {
			pingpongTexture[(i + 1) % 2].use(GL_TEXTURE0);
		}
		drawScreenQuad();
		pingpongFBO[i % 2].unbind();
	}

	glViewport(x, y, width, height);
	screenQuadShader->use();
	screenQuadShader->setInt("colorBuffer", 0);
	screenQuadShader->setInt("blurBuffer", 1);
	hdrTexture.use(GL_TEXTURE0);
	pingpongTexture[0].use(GL_TEXTURE1);
	drawScreenQuad();
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