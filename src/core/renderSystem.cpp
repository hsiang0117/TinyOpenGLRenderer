#include "renderSystem.hpp"
#include "guiSystem.hpp"
#include "resourceManager.hpp"
#include "../utils.hpp"
#include "../Input.hpp"
#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <GLFW/glfw3.h>
#include <iostream>
#include <algorithm>

// 视口尺寸 = 窗口尺寸 - 侧边栏；窗口最小化时会得到负值，钳到至少 1 避免
// 用负的 width/height 去 glViewport / resetSize 触发 GL_INVALID_VALUE。
static float viewportWidth() {
	float w = Input::getInstance().getWindowWidth() - GuiSystem::leftSideBarWidth - GuiSystem::rightSideBarWidth;
	return std::max(w, 1.0f);
}

static float viewportHeight() {
	float h = Input::getInstance().getWindowHeight() - GuiSystem::bottomSideBarHeight;
	return std::max(h, 1.0f);
}

static void APIENTRY glDebugOutput(GLenum source, GLenum type, unsigned int id, GLenum severity,
	GLsizei length, const char* message, const void* userParam) {
	// 过滤无关紧要的通知（如缓冲区详细信息）
	if (severity == GL_DEBUG_SEVERITY_NOTIFICATION) return;

	const char* sourceStr = "Other";
	switch (source) {
		case GL_DEBUG_SOURCE_API:             sourceStr = "API"; break;
		case GL_DEBUG_SOURCE_WINDOW_SYSTEM:   sourceStr = "Window System"; break;
		case GL_DEBUG_SOURCE_SHADER_COMPILER: sourceStr = "Shader Compiler"; break;
		case GL_DEBUG_SOURCE_THIRD_PARTY:     sourceStr = "Third Party"; break;
		case GL_DEBUG_SOURCE_APPLICATION:     sourceStr = "Application"; break;
	}

	const char* typeStr = "Other";
	switch (type) {
		case GL_DEBUG_TYPE_ERROR:               typeStr = "Error"; break;
		case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: typeStr = "Deprecated"; break;
		case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:  typeStr = "Undefined Behavior"; break;
		case GL_DEBUG_TYPE_PORTABILITY:         typeStr = "Portability"; break;
		case GL_DEBUG_TYPE_PERFORMANCE:         typeStr = "Performance"; break;
		case GL_DEBUG_TYPE_MARKER:              typeStr = "Marker"; break;
	}

	const char* severityStr = "Low";
	switch (severity) {
		case GL_DEBUG_SEVERITY_HIGH:   severityStr = "High"; break;
		case GL_DEBUG_SEVERITY_MEDIUM: severityStr = "Medium"; break;
	}

	std::cerr << "[GL][" << severityStr << "][" << sourceStr << "][" << typeStr
		<< "] (" << id << ") " << message << std::endl;
}

glm::mat4 RenderSystem::buildModelMatrix(const std::shared_ptr<Transform>& transform) {
	glm::mat4 model = glm::mat4(1.0f);
	if (!transform) return model;
	model = glm::translate(model, transform->translate);
	model = glm::rotate(model, glm::radians(transform->rotate.z), glm::vec3(0, 0, 1));
	model = glm::rotate(model, glm::radians(transform->rotate.y), glm::vec3(0, 1, 0));
	model = glm::rotate(model, glm::radians(transform->rotate.x), glm::vec3(1, 0, 0));
	model = glm::scale(model, transform->scale);
	return model;
}

glm::vec3 RenderSystem::directionFromTransform(const std::shared_ptr<Transform>& transform) {
	glm::mat4 rotation(1.0);
	rotation = glm::rotate(rotation, glm::radians(transform->rotate.z), glm::vec3(0, 0, 1));
	rotation = glm::rotate(rotation, glm::radians(transform->rotate.y), glm::vec3(0, 1, 0));
	rotation = glm::rotate(rotation, glm::radians(transform->rotate.x), glm::vec3(1, 0, 0));
	glm::vec3 direction(1.0, 0.0, 0.0);
	return glm::mat3(rotation) * direction;
}

bool RenderSystem::aabbInFrustum(const glm::vec3& minAABB, const glm::vec3& maxAABB, const glm::mat4& model, Frustum& frustum) {
	glm::vec3 vertices[8] = {
		glm::vec3(minAABB.x, minAABB.y, minAABB.z),
		glm::vec3(maxAABB.x, minAABB.y, minAABB.z),
		glm::vec3(minAABB.x, maxAABB.y, minAABB.z),
		glm::vec3(maxAABB.x, maxAABB.y, minAABB.z),
		glm::vec3(minAABB.x, minAABB.y, maxAABB.z),
		glm::vec3(maxAABB.x, minAABB.y, maxAABB.z),
		glm::vec3(minAABB.x, maxAABB.y, maxAABB.z),
		glm::vec3(maxAABB.x, maxAABB.y, maxAABB.z)
	};
	for (int i = 0; i < 8; ++i) {
		glm::vec4 v = model * glm::vec4(vertices[i], 1.0f);
		vertices[i] = glm::vec3(v);
	}
	glm::vec4 planes[6] = {
		frustum.leftPlane, frustum.rightPlane,
		frustum.bottomPlane, frustum.topPlane,
		frustum.nearPlane, frustum.farPlane
	};
	for (int p = 0; p < 6; ++p) {
		int out = 0;
		for (int i = 0; i < 8; ++i) {
			const glm::vec4& plane = planes[p];
			if (plane.x * vertices[i].x + plane.y * vertices[i].y + plane.z * vertices[i].z + plane.w < 0)
				out++;
		}
		if (out == 8) return false;
	}
	return true;
}

bool RenderSystem::isRenderable(const GameObjectPtr& obj) {
	if (!obj) return false;
	if (obj->has<RenderComponent>()) return true;
	if (obj->has<StaticMeshComponent>() && obj->has<DynamicMaterialComponent>() && !obj->has<VolumeMarkerComponent>())
		return true;
	return false;
}

bool RenderSystem::isRenderableInFrustum(const GameObjectPtr& obj, Frustum& frustum) {
	if (!obj) return false;
	auto transform = obj->getComponent<Transform>();
	if (!transform) return false;
	glm::mat4 model = buildModelMatrix(transform);
	if (auto rc = obj->getComponent<RenderComponent>()) {
		return aabbInFrustum(rc->aabb.min, rc->aabb.max, model, frustum);
	}
	if (auto sm = obj->getComponent<StaticMeshComponent>()) {
		return aabbInFrustum(sm->aabb.min, sm->aabb.max, model, frustum);
	}
	return false;
}

bool RenderSystem::isPointLightInFrustum(const GameObjectPtr& obj, Frustum& frustum) {
	if (!obj) return false;
	auto transform = obj->getComponent<Transform>();
	auto sm = obj->getComponent<StaticMeshComponent>();
	if (!transform || !sm) return false;
	glm::mat4 model = buildModelMatrix(transform);
	return aabbInFrustum(sm->aabb.min, sm->aabb.max, model, frustum);
}

void RenderSystem::drawRenderable(const GameObjectPtr& obj, ShaderPtr shader) {
	auto transform = obj->getComponent<Transform>();
	glm::mat4 model = buildModelMatrix(transform);
	shader->setMat4("model", model);

	if (auto renderComponent = obj->getComponent<RenderComponent>()) {
		if (auto animator = obj->getComponent<AnimatorComponent>()) {
			auto& boneMatrixTexture = animator->getBoneMatrixTexture();
			boneMatrixTexture.use(GL_TEXTURE8);
		}
		if (renderComponent->model) {
			renderComponent->model->draw(shader);
		}
		return;
	}

	if (auto staticMeshComponent = obj->getComponent<StaticMeshComponent>()) {
		if (auto dynamicMaterialComponent = obj->getComponent<DynamicMaterialComponent>()) {
			dynamicMaterialComponent->material.bind(shader);
		}
		// 静态网格无骨骼动画，unit 8 没有骨骼矩阵纹理，绑占位纹理消除警告
		bindDummyTexture2D(GL_TEXTURE8);
		for (int i = 0; i < 100; i++) {
			shader->setMat4(("finalBoneMatrices[" + std::to_string(i) + "]").c_str(), glm::mat4(1.0f));
		}
		if (staticMeshComponent->mesh) {
			staticMeshComponent->mesh->draw();
		}
	}
}

void RenderSystem::drawLightCube(const GameObjectPtr& obj, ShaderPtr shader) {
	auto staticMeshComponent = obj->getComponent<StaticMeshComponent>();
	if (!staticMeshComponent) return;
	auto transform = obj->getComponent<Transform>();
	glm::mat4 model = buildModelMatrix(transform);
	shader->setMat4("model", model);
	glm::vec3 color = glm::vec3(1.0f);
	float brightness = 1.0f;
	if (auto pointLight = obj->getComponent<PointLightComponent>()) {
		color = pointLight->color;
		brightness = pointLight->brightness;
	}
	shader->setVec3("color", color);
	shader->setFloat("brightness", brightness);
	if (staticMeshComponent->mesh) {
		staticMeshComponent->mesh->draw();
	}
}

void RenderSystem::drawSkybox(const GameObjectPtr& obj, ShaderPtr shader) {
	shader->setInt("skybox", 5);
	if (auto skyboxComponent = obj->getComponent<SkyBoxComponent>()) {
		if (skyboxComponent->skybox) skyboxComponent->skybox->draw();
	}
}

void RenderSystem::bindSkyboxCubeMap(const GameObjectPtr& obj, ShaderPtr shader) {
	shader->setInt("skybox", 5);
	if (auto skyboxComponent = obj->getComponent<SkyBoxComponent>()) {
		if (skyboxComponent->skybox) skyboxComponent->skybox->useCubeMap();
	}
}

void RenderSystem::drawSkeleton(const GameObjectPtr& obj, ShaderPtr shader) {
	auto transform = obj->getComponent<Transform>();
	glm::mat4 model = buildModelMatrix(transform);
	shader->setMat4("model", model);
	if (auto skeletonViewer = obj->getComponent<SkeletonViewerComponent>()) {
		if (skeletonViewer->show) skeletonViewer->drawSkeleton();
	}
}

void RenderSystem::uploadPointLight(const GameObjectPtr& obj, int index, ShaderStorageBuffer& ssbo) {
	auto transform = obj->getComponent<Transform>();
	auto pointLight = obj->getComponent<PointLightComponent>();
	auto shadowCaster = obj->getComponent<ShadowCasterCube>();
	const int sz = PointLightComponent::glslSize;
	ssbo.bind();
	ssbo.bufferSubdata(index * sz, 12, glm::value_ptr(transform->translate));
	ssbo.bufferSubdata(index * sz + 12, 4, nullptr);
	ssbo.bufferSubdata(index * sz + 16, 12, glm::value_ptr(pointLight->color));
	ssbo.bufferSubdata(index * sz + 28, 4, &pointLight->brightness);
	ssbo.bufferSubdata(index * sz + 32, 4, &pointLight->constant);
	ssbo.bufferSubdata(index * sz + 36, 4, &pointLight->linear);
	ssbo.bufferSubdata(index * sz + 40, 4, &pointLight->quadratic);
	ssbo.bufferSubdata(index * sz + 44, 4, &shadowCaster->farPlane);
	ssbo.unbind();
}

void RenderSystem::uploadDirectionLight(const GameObjectPtr& obj, ShaderStorageBuffer& ssbo) {
	auto transform = obj->getComponent<Transform>();
	auto directionLight = obj->getComponent<DirectionLightComponent>();
	glm::vec3 direction = directionFromTransform(transform);
	ssbo.bind();
	ssbo.bufferSubdata(0, 12, glm::value_ptr(direction));
	ssbo.bufferSubdata(12, 4, nullptr);
	ssbo.bufferSubdata(16, 12, glm::value_ptr(directionLight->color));
	ssbo.bufferSubdata(28, 4, &directionLight->brightness);
	ssbo.unbind();
}

void RenderSystem::uploadSpotLight(const GameObjectPtr& obj, int index, ShaderStorageBuffer& ssbo) {
	auto transform = obj->getComponent<Transform>();
	auto spotLight = obj->getComponent<SpotLightComponent>();
	glm::vec3 direction = directionFromTransform(transform);
	float cutOff = glm::cos(glm::radians(spotLight->cutOff));
	float outerCutOff = glm::cos(glm::radians(spotLight->outerCutOff));
	const int sz = SpotLightComponent::glslSize;
	ssbo.bind();
	ssbo.bufferSubdata(index * sz, 12, glm::value_ptr(transform->translate));
	ssbo.bufferSubdata(index * sz + 12, 4, nullptr);
	ssbo.bufferSubdata(index * sz + 16, 12, glm::value_ptr(direction));
	ssbo.bufferSubdata(index * sz + 28, 4, nullptr);
	ssbo.bufferSubdata(index * sz + 32, 12, glm::value_ptr(spotLight->color));
	ssbo.bufferSubdata(index * sz + 44, 4, &spotLight->brightness);
	ssbo.bufferSubdata(index * sz + 48, 4, &cutOff);
	ssbo.bufferSubdata(index * sz + 52, 4, &outerCutOff);
	ssbo.bufferSubdata(index * sz + 56, 8, nullptr);
	ssbo.unbind();
}

glm::mat4 RenderSystem::directionLightMatrices(const GameObjectPtr& obj) {
	auto transform = obj->getComponent<Transform>();
	glm::vec3 direction = directionFromTransform(transform);
	glm::mat4 lightProjection = glm::ortho(-100.0f, 100.0f, -100.0f, 100.0f, -100.0f, 100.0f);
	glm::mat4 lightView = glm::lookAt(-direction, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
	return lightProjection * lightView;
}

std::vector<glm::mat4> RenderSystem::pointLightMatricesCube(const GameObjectPtr& obj) {
	auto transform = obj->getComponent<Transform>();
	glm::vec3 position = transform->translate;
	auto shadowCaster = obj->getComponent<ShadowCasterCube>();
	glm::mat4 lightProjection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, shadowCaster->farPlane);
	std::vector<glm::mat4> lightViews;
	lightViews.push_back(lightProjection * glm::lookAt(position, position + glm::vec3(1.0, 0.0, 0.0), glm::vec3(0.0, -1.0, 0.0)));
	lightViews.push_back(lightProjection * glm::lookAt(position, position + glm::vec3(-1.0, 0.0, 0.0), glm::vec3(0.0, -1.0, 0.0)));
	lightViews.push_back(lightProjection * glm::lookAt(position, position + glm::vec3(0.0, 1.0, 0.0), glm::vec3(0.0, 0.0, 1.0)));
	lightViews.push_back(lightProjection * glm::lookAt(position, position + glm::vec3(0.0, -1.0, 0.0), glm::vec3(0.0, 0.0, -1.0)));
	lightViews.push_back(lightProjection * glm::lookAt(position, position + glm::vec3(0.0, 0.0, 1.0), glm::vec3(0.0, -1.0, 0.0)));
	lightViews.push_back(lightProjection * glm::lookAt(position, position + glm::vec3(0.0, 0.0, -1.0), glm::vec3(0.0, -1.0, 0.0)));
	return lightViews;
}

void RenderSystem::init() {
	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
		std::cout << "Failed to initialize GLAD" << std::endl;
		exit(EXIT_FAILURE);
	}
#ifdef _DEBUG
	{
		GLint flags = 0;
		glGetIntegerv(GL_CONTEXT_FLAGS, &flags);
		if (flags & GL_CONTEXT_FLAG_DEBUG_BIT) {
			glEnable(GL_DEBUG_OUTPUT);
			glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
			glDebugMessageCallback(glDebugOutput, nullptr);
			glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);
		}
	}
#endif
	x = GuiSystem::leftSideBarWidth;
	y = GuiSystem::bottomSideBarHeight;
	width = viewportWidth();
	height = viewportHeight();
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_PROGRAM_POINT_SIZE);
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
	ssboPointLights.bufferData(100 * PointLightComponent::glslSize, NULL);
	ssboPointLights.unbind();

	ssboDirectionLight.bind();
	ssboDirectionLight.bufferBase(2);
	ssboDirectionLight.bufferData(DirectionLightComponent::glslSize, NULL);
	ssboDirectionLight.unbind();

	ssboSpotLights.bind();
	ssboSpotLights.bufferBase(3);
	ssboSpotLights.bufferData(50 * SpotLightComponent::glslSize, NULL);
	ssboSpotLights.unbind();

	directionLightDepthFBO.init();
	directionLightDepthFBO.bind();
	directionLightDepthFBO.drawBuffers({ GL_NONE });
	directionLightDepthFBO.readBuffer(GL_NONE);
	directionLightDepthFBO.unbind();
	directionLightDepthTexture = Texture2D(1024, 1024, GL_CLAMP_TO_BORDER, GL_NEAREST, GL_DEPTH_COMPONENT, GL_DEPTH_COMPONENT, GL_FLOAT);
	directionLightDepthTexture.setBorderColor(1.0, 1.0, 1.0, 1.0);

	pointLightDepthFBO.init();
	pointLightDepthFBO.bind();
	pointLightDepthFBO.drawBuffers({ GL_NONE });
	pointLightDepthFBO.readBuffer(GL_NONE);
	pointLightDepthFBO.unbind();
	pointLightDepthTexture = CubeMapArray(1024, 1024, 10, GL_CLAMP_TO_BORDER, GL_NEAREST, GL_DEPTH_COMPONENT, GL_DEPTH_COMPONENT);

	hdrFBO.init();
	hdrTexture = Texture2D(width, height, GL_CLAMP_TO_BORDER, GL_LINEAR, GL_RGBA16F, GL_RGBA, GL_FLOAT);
	brightTexture = Texture2D(width, height, GL_CLAMP_TO_BORDER, GL_LINEAR, GL_RGBA16F, GL_RGBA, GL_FLOAT);
	hdrDepthTexture = Texture2D(width, height, GL_CLAMP_TO_BORDER, GL_NEAREST, GL_DEPTH_COMPONENT, GL_DEPTH_COMPONENT, GL_FLOAT);
	hdrFBO.bind();
	hdrFBO.attachTexture2D(hdrTexture, GL_COLOR_ATTACHMENT0);
	hdrFBO.attachTexture2D(brightTexture, GL_COLOR_ATTACHMENT1);
	hdrFBO.attachTexture2D(hdrDepthTexture, GL_DEPTH_ATTACHMENT);
	hdrFBO.drawBuffers({ GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 });
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

	afterEffectFBO.init();
	afterEffectTexture = Texture2D(width, height, GL_CLAMP_TO_BORDER, GL_LINEAR, GL_RGBA16F, GL_RGBA, GL_FLOAT);
	afterEffectFBO.bind();
	afterEffectFBO.attachTexture2D(afterEffectTexture, GL_COLOR_ATTACHMENT0);
	afterEffectFBO.unbind();

	// 体积云噪声不在此生成，改为场景首次出现体积云时后台并行生成（见 ensureVolumeNoise）
}

// 体积云噪声纹理的延迟生成与上传。每帧调用：
//  - 场景出现体积云且尚未发起 → 启动后台 worker 并行计算（带磁盘缓存）
//  - 后台计算完成 → 在主线程（持有 GL context）上传为纹理，标记就绪
void RenderSystem::ensureVolumeNoise(bool sceneHasVolume) {
	if (noiseReady) return;
	if (!sceneHasVolume) return;

	if (!noiseRequested) {
		noiseRequested = true;
		worleyFuture = std::async(std::launch::async, []() {
			return NoiseTextureGenerator3D::computeWorleyNoise(128, 128, 128);
			});
		perlinFuture = std::async(std::launch::async, []() {
			return NoiseTextureGenerator3D::computePerlinNoise(128, 128, 128);
			});
		weatherFuture = std::async(std::launch::async, []() {
			return NoiseTextureGenerator3D::computeWeatherMap(512, 512);
			});
		return;
	}

	using namespace std::chrono;
	bool allDone =
		worleyFuture.valid() && worleyFuture.wait_for(seconds(0)) == std::future_status::ready &&
		perlinFuture.valid() && perlinFuture.wait_for(seconds(0)) == std::future_status::ready &&
		weatherFuture.valid() && weatherFuture.wait_for(seconds(0)) == std::future_status::ready;
	if (!allDone) return;

	worleyNoiseTexture3D = NoiseTextureGenerator3D::uploadVolume3D(worleyFuture.get());
	perlinNoiseTexture3D = NoiseTextureGenerator3D::uploadVolume3D(perlinFuture.get());
	weatherMapTexture = NoiseTextureGenerator3D::uploadVolume2D(weatherFuture.get());
	noiseReady = true;
}

void RenderSystem::update(double deltaTime) {
	if (Input::getInstance().isWindowResized()) {
		width = viewportWidth();
		height = viewportHeight();
		hdrTexture.resetSize(width, height);
		brightTexture.resetSize(width, height);
		hdrDepthTexture.resetSize(width, height);
		pingpongTexture[0].resetSize(width, height);
		pingpongTexture[1].resetSize(width, height);
		afterEffectTexture.resetSize(width, height);
	}
	if (Input::getInstance().isUiResized()) {
		x = GuiSystem::leftSideBarWidth;
		y = GuiSystem::bottomSideBarHeight;
		width = viewportWidth();
		height = viewportHeight();
		hdrTexture.resetSize(width, height);
		brightTexture.resetSize(width, height);
		hdrDepthTexture.resetSize(width, height);
		pingpongTexture[0].resetSize(width, height);
		pingpongTexture[1].resetSize(width, height);
		afterEffectTexture.resetSize(width, height);
	}
	for (auto& obj : ResourceManager::getInstance().getSceneView<AnimatorComponent>()) {
		auto animator = obj->getComponent<AnimatorComponent>();
		if (animator->playing) animator->update(deltaTime);
	}
}

void RenderSystem::render(Camera& camera) {
	uboMatrices.bind();
	uboMatrices.bufferSubdata(0, sizeof(glm::mat4), glm::value_ptr(camera.getViewMat()));
	uboMatrices.bufferSubdata(sizeof(glm::mat4), sizeof(glm::mat4), glm::value_ptr(camera.getProjectionMat((float)width, (float)height)));
	uboMatrices.unbind();

	Frustum frustum = camera.getFrustum((float)width, (float)height);

	// 清屏前解绑 program，避免驱动校验上一帧残留 active program 的 sampler 完整性
	glUseProgram(0);
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	auto& rm = ResourceManager::getInstance();
	ShaderPtr defaultShader = rm.getShader("default");
	ShaderPtr skyboxShader = rm.getShader("skybox");
	ShaderPtr depthShader = rm.getShader("depth");
	ShaderPtr screenQuadShader = rm.getShader("screenQuad");
	ShaderPtr depthCubeShader = rm.getShader("depthCube");
	ShaderPtr lightCubeShader = rm.getShader("lightCube");
	ShaderPtr gaussianBlurShader = rm.getShader("gaussianBlur");
	ShaderPtr boneShader = rm.getShader("bone");
	ShaderPtr volumeShader = rm.getShader("volume");

	auto renderables = rm.getSceneView<RenderComponent>();
	auto staticMeshes = rm.getSceneView<DynamicMaterialComponent>();
	auto pointLights = rm.getSceneView<PointLightComponent>();
	auto directionLights = rm.getSceneView<DirectionLightComponent>();
	auto spotLights = rm.getSceneView<SpotLightComponent>();
	auto skyboxes = rm.getSceneView<SkyBoxComponent>();
	auto volumes = rm.getSceneView<VolumeMarkerComponent>();
	auto skeletons = rm.getSceneView<SkeletonViewerComponent>();

	auto drawAllRenderablesInFrustum = [&](ShaderPtr shader) {
		for (auto& obj : renderables) {
			if (isRenderableInFrustum(obj, frustum)) drawRenderable(obj, shader);
		}
		for (auto& obj : staticMeshes) {
			if (obj->has<RenderComponent>() || obj->has<VolumeMarkerComponent>()) continue;
			if (isRenderableInFrustum(obj, frustum)) drawRenderable(obj, shader);
		}
	};

	glViewport(0, 0, 1024, 1024);

	for (auto& obj : directionLights) {
		depthShader->use();
		auto shadowCaster = obj->getComponent<ShadowCaster2D>();
		if (shadowCaster && shadowCaster->enabled) {
			depthShader->setMat4("lightMatrices", directionLightMatrices(obj));
			directionLightDepthFBO.bind();
			directionLightDepthFBO.attachTexture2D(directionLightDepthTexture, GL_DEPTH_ATTACHMENT);
			glClear(GL_DEPTH_BUFFER_BIT);
			glEnable(GL_CULL_FACE);
			glCullFace(GL_FRONT);
			drawAllRenderablesInFrustum(depthShader);
			glDisable(GL_CULL_FACE);
			directionLightDepthFBO.unbind();
		}
		else {
			directionLightDepthFBO.bind();
			directionLightDepthFBO.attachTexture2D(directionLightDepthTexture, GL_DEPTH_ATTACHMENT);
			glClear(GL_DEPTH_BUFFER_BIT);
			directionLightDepthFBO.unbind();
		}
	}

	int pointLightIndex = 0;
	for (auto& obj : pointLights) {
		depthCubeShader->use();
		auto shadowCaster = obj->getComponent<ShadowCasterCube>();
		if (shadowCaster && shadowCaster->enabled) {
			auto transform = obj->getComponent<Transform>();
			depthCubeShader->setVec3("lightPos", transform->translate);
			depthCubeShader->setFloat("farPlane", shadowCaster->farPlane);
			depthCubeShader->setInt("lightIndex", pointLightIndex);
			std::vector<glm::mat4> lightMatrices = pointLightMatricesCube(obj);
			for (int j = 0; j < 6; j++) {
				depthCubeShader->setMat4(("lightMatrices[" + std::to_string(j) + "]").c_str(), lightMatrices[j]);
			}
			pointLightDepthFBO.bind();
			for (int j = 0; j < 6; j++) {
				pointLightDepthFBO.attachTextureLayer(pointLightDepthTexture, GL_DEPTH_ATTACHMENT, pointLightIndex * 6 + j);
				glClear(GL_DEPTH_BUFFER_BIT);
			}
			pointLightDepthFBO.attachTexture(pointLightDepthTexture, GL_DEPTH_ATTACHMENT);
			glEnable(GL_CULL_FACE);
			glCullFace(GL_FRONT);
			drawAllRenderablesInFrustum(depthCubeShader);
			glDisable(GL_CULL_FACE);
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

	defaultShader->use();
	{
		int idx = 0;
		for (auto& obj : pointLights) {
			uploadPointLight(obj, idx++, ssboPointLights);
		}
	}
	{
		int idx = 0;
		for (auto& obj : spotLights) {
			uploadSpotLight(obj, idx++, ssboSpotLights);
		}
	}
	for (auto& obj : directionLights) {
		uploadDirectionLight(obj, ssboDirectionLight);
		auto shadowCaster = obj->getComponent<ShadowCaster2D>();
		if (shadowCaster && shadowCaster->enabled) {
			defaultShader->setMat4("lightSpaceMatrix", directionLightMatrices(obj));
		}
	}
	defaultShader->setInt("pointLightNum", (int)pointLights.size());
	defaultShader->setInt("directionLightNum", (int)directionLights.size());
	defaultShader->setInt("spotLightNum", (int)spotLights.size());
	directionLightDepthTexture.use(GL_TEXTURE6);
	pointLightDepthTexture.use(GL_TEXTURE7);

	glViewport(0, 0, width, height);

	hdrFBO.bind();
	// 清屏前解绑 program：避免驱动在 glClear 时校验上一个 active program 的
	// sampler 完整性，对未绑定的 normalMap/boneMatrix 单元报无害的占位警告
	glUseProgram(0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	defaultShader->use();
	defaultShader->setVec3("cameraPos", camera.getPos());
	for (auto& obj : skyboxes) {
		bindSkyboxCubeMap(obj, defaultShader);
	}
	drawAllRenderablesInFrustum(defaultShader);

	lightCubeShader->use();
	for (auto& obj : pointLights) {
		if (obj->has<StaticMeshComponent>() && isPointLightInFrustum(obj, frustum)) {
			drawLightCube(obj, lightCubeShader);
		}
	}

	skyboxShader->use();
	for (auto& obj : skyboxes) {
		drawSkybox(obj, skyboxShader);
	}
	hdrFBO.unbind();

	glViewport(0, 0, width, height);
	afterEffectFBO.bind();
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	// 同上：清屏前解绑 program，避免 volume shader 的 depthMap 单元未绑警告
	glUseProgram(0);
	glClear(GL_COLOR_BUFFER_BIT);

	// 体积云噪声延迟生成：有体积云才在后台并行算，算好前跳过 volume 绘制
	ensureVolumeNoise(!volumes.empty());
	if (noiseReady && !volumes.empty()) {
		volumeShader->use();
		volumeShader->setVec3("cameraPos", camera.getPos());
		volumeShader->setInt("depthMap", 0);
		volumeShader->setInt("worleyNoiseTexture", 1);
		volumeShader->setInt("weatherMap", 2);
		volumeShader->setInt("perlinNoiseTexture", 3);
		volumeShader->setVec2("resolution", glm::vec2(width, height));
		volumeShader->setFloat("time", (float)glfwGetTime());
		glm::mat4 invVP = glm::inverse(camera.getProjectionMat((float)width, (float)height) * camera.getViewMat());
		volumeShader->setMat4("invVP", invVP);

		bool foundDirLight = false;
		for (auto& obj : directionLights) {
			auto transform = obj->getComponent<Transform>();
			auto dirLight = obj->getComponent<DirectionLightComponent>();
			glm::vec3 direction = directionFromTransform(transform);
			volumeShader->setVec3("lightDir", -direction);
			volumeShader->setVec3("lightColor", dirLight->color * dirLight->brightness);
			foundDirLight = true;
			break;
		}
		if (!foundDirLight) {
			volumeShader->setVec3("lightDir", glm::vec3(0.0, 1.0, 0.0));
			volumeShader->setVec3("lightColor", glm::vec3(1.0));
		}
		hdrDepthTexture.use(GL_TEXTURE0);
		worleyNoiseTexture3D.use(GL_TEXTURE1);
		weatherMapTexture.use(GL_TEXTURE2);
		perlinNoiseTexture3D.use(GL_TEXTURE3);
		for (auto& obj : volumes) {
			auto staticMesh = obj->getComponent<StaticMeshComponent>();
			auto transform = obj->getComponent<Transform>();
			if (staticMesh && transform) {
				glm::mat4 model = glm::mat4(1.0f);
				model = glm::translate(model, transform->translate);
				model = glm::scale(model, transform->scale);
				glm::vec3 minAABB = glm::vec3(model * glm::vec4(staticMesh->aabb.min, 1.0f));
				glm::vec3 maxAABB = glm::vec3(model * glm::vec4(staticMesh->aabb.max, 1.0f));
				volumeShader->setVec3("aabbMin", minAABB);
				volumeShader->setVec3("aabbMax", maxAABB);
				drawScreenQuad();
			}
		}
	}

	boneShader->use();
	for (auto& obj : skeletons) {
		if (isRenderableInFrustum(obj, frustum)) drawSkeleton(obj, boneShader);
	}

	afterEffectFBO.unbind();

	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	gaussianBlurShader->use();
	for (int i = 0; i < 10; i++) {
		pingpongFBO[i % 2].bind();
		gaussianBlurShader->setBool("horizontal", i % 2 == 0);
		// 先绑采样纹理再清屏：让 clear 时当前 program 的 sampler 单元已完整
		if (i == 0) brightTexture.use(GL_TEXTURE0);
		else pingpongTexture[(i + 1) % 2].use(GL_TEXTURE0);
		glClear(GL_COLOR_BUFFER_BIT);
		drawScreenQuad();
		pingpongFBO[i % 2].unbind();
	}

	glViewport(x, y, width, height);
	screenQuadShader->use();
	hdrTexture.use(GL_TEXTURE0);
	pingpongTexture[1].use(GL_TEXTURE1);
	afterEffectTexture.use(GL_TEXTURE2);
	drawScreenQuad();
}

void RenderSystem::drawScreenQuad()
{
	static GLuint quadVAO, quadVBO;

	if (!quadVAO) {
		float quadVertices[] = {
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
