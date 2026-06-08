#include "renderSystem.hpp"
#include "guiSystem.hpp"
#include "resourceManager.hpp"
#include "renderUtils.hpp"
#include "renderPass.hpp"
#include "renderPasses.hpp"
#include "logger.hpp"
#include "../utils.hpp"
#include "../Input.hpp"
#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <GLFW/glfw3.h>
#include <iostream>
#include <algorithm>

static void APIENTRY glDebugOutput(GLenum source, GLenum type, unsigned int id, GLenum severity,
	GLsizei length, const char* message, const void* userParam) {
	// Skip low-value notifications (e.g. buffer detail messages)
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
	LogLevel level = LogLevel::Info;
	switch (severity) {
		case GL_DEBUG_SEVERITY_HIGH:   severityStr = "High";   level = LogLevel::Error;   break;
		case GL_DEBUG_SEVERITY_MEDIUM: severityStr = "Medium"; level = LogLevel::Warning; break;
	}

	LOG_AT(level, "[GL][" << severityStr << "][" << sourceStr << "][" << typeStr
		<< "] (" << id << ") " << message);
}

void RenderSystem::init() {
	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
		LOG_ERROR("Failed to initialize GLAD");
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
	x = 0;
	y = 0;
	// Size-dependent targets follow the Viewport panel, not the window. Seed from
	// the initial viewport guess; update() resizes once the panel reports its size.
	width = (float)viewportW_;
	height = (float)viewportH_;
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
	hdrTexture = Texture2D(viewportW_, viewportH_, GL_CLAMP_TO_BORDER, GL_LINEAR, GL_RGBA16F, GL_RGBA, GL_FLOAT);
	brightTexture = Texture2D(viewportW_, viewportH_, GL_CLAMP_TO_BORDER, GL_LINEAR, GL_RGBA16F, GL_RGBA, GL_FLOAT);
	hdrDepthTexture = Texture2D(viewportW_, viewportH_, GL_CLAMP_TO_BORDER, GL_NEAREST, GL_DEPTH_COMPONENT, GL_DEPTH_COMPONENT, GL_FLOAT);
	hdrFBO.bind();
	hdrFBO.attachTexture2D(hdrTexture, GL_COLOR_ATTACHMENT0);
	hdrFBO.attachTexture2D(brightTexture, GL_COLOR_ATTACHMENT1);
	hdrFBO.attachTexture2D(hdrDepthTexture, GL_DEPTH_ATTACHMENT);
	hdrFBO.drawBuffers({ GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 });
	hdrFBO.unbind();

	pingpongFBO[0].init();
	pingpongFBO[1].init();
	pingpongTexture[0] = Texture2D(viewportW_, viewportH_, GL_CLAMP_TO_BORDER, GL_LINEAR, GL_RGBA16F, GL_RGBA, GL_FLOAT);
	pingpongTexture[1] = Texture2D(viewportW_, viewportH_, GL_CLAMP_TO_BORDER, GL_LINEAR, GL_RGBA16F, GL_RGBA, GL_FLOAT);
	pingpongFBO[0].bind();
	pingpongFBO[0].attachTexture2D(pingpongTexture[0], GL_COLOR_ATTACHMENT0);
	pingpongFBO[0].unbind();
	pingpongFBO[1].bind();
	pingpongFBO[1].attachTexture2D(pingpongTexture[1], GL_COLOR_ATTACHMENT0);
	pingpongFBO[1].unbind();

	afterEffectFBO.init();
	afterEffectTexture = Texture2D(viewportW_, viewportH_, GL_CLAMP_TO_BORDER, GL_LINEAR, GL_RGBA16F, GL_RGBA, GL_FLOAT);
	afterEffectFBO.bind();
	afterEffectFBO.attachTexture2D(afterEffectTexture, GL_COLOR_ATTACHMENT0);
	afterEffectFBO.unbind();

	// Offscreen scene target for the Viewport panel. Color-only (no depth needed for
	// the composite fullscreen quad), CLAMP_TO_EDGE + LINEAR so ImGui samples it clean.
	sceneFBO.init();
	sceneColorTexture = Texture2D(viewportW_, viewportH_, GL_CLAMP_TO_EDGE, GL_LINEAR, GL_RGBA16F, GL_RGBA, GL_FLOAT);
	sceneFBO.bind();
	sceneFBO.attachTexture2D(sceneColorTexture, GL_COLOR_ATTACHMENT0);
	sceneFBO.drawBuffers({ GL_COLOR_ATTACHMENT0 });
	sceneFBO.unbind();

	// Volume-cloud noise is not generated here; it is built in the background the
	// first time the scene contains a volume (see ensureVolumeNoise).

	// Build the ordered pass list. Order encodes producer->consumer dependencies.
	// LightUpload MUST stay immediately before ForwardHdr: the SSBO bindings,
	// TEX6/7 shadow maps and light-count uniforms it sets persist into the forward pass.
	passes.clear();
	passes.push_back(std::make_unique<FrameSetupPass>());
	passes.push_back(std::make_unique<DirectionalShadowPass>());
	passes.push_back(std::make_unique<PointShadowPass>());
	passes.push_back(std::make_unique<LightUploadPass>());
	passes.push_back(std::make_unique<ForwardHdrPass>());
	passes.push_back(std::make_unique<VolumeSkeletonPass>());
	passes.push_back(std::make_unique<BloomPass>());
	passes.push_back(std::make_unique<CompositePass>());
}

// Lazy generation + upload of the volume-cloud noise textures. Called every frame:
//  - scene has a volume and generation not yet started -> kick off background workers
//  - background compute done -> upload to GL textures on the main thread, mark ready
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
	// FBO sizing follows the Viewport panel content size (set via setViewportSize),
	// not the OS window. Resize all size-dependent targets when it changes.
	if ((int)width != viewportW_ || (int)height != viewportH_) {
		width = (float)viewportW_;
		height = (float)viewportH_;
		hdrTexture.resetSize(viewportW_, viewportH_);
		brightTexture.resetSize(viewportW_, viewportH_);
		hdrDepthTexture.resetSize(viewportW_, viewportH_);
		pingpongTexture[0].resetSize(viewportW_, viewportH_);
		pingpongTexture[1].resetSize(viewportW_, viewportH_);
		afterEffectTexture.resetSize(viewportW_, viewportH_);
		sceneColorTexture.resetSize(viewportW_, viewportH_);
	}
	for (auto& obj : rm_->getSceneView<AnimatorComponent>()) {
		auto animator = obj->getComponent<AnimatorComponent>();
		if (animator->playing) animator->update((float)deltaTime);
	}
}

void RenderSystem::setViewportSize(int w, int h) {
	viewportW_ = std::max(w, 1);
	viewportH_ = std::max(h, 1);
}

unsigned int RenderSystem::getSceneColorTextureID() const {
	return sceneColorTexture.ID;
}

RenderContext RenderSystem::buildContext(Camera& camera) {
	auto& rm = *rm_;

	// Per-frame context: snapshots + non-owning pointers to GL targets.
	RenderContext ctx;
	ctx.camera = &camera;
	ctx.x = x; ctx.y = y; ctx.width = width; ctx.height = height;

	ctx.defaultShader = rm.getShader("default");
	ctx.skyboxShader = rm.getShader("skybox");
	ctx.depthShader = rm.getShader("depth");
	ctx.screenQuadShader = rm.getShader("screenQuad");
	ctx.depthCubeShader = rm.getShader("depthCube");
	ctx.lightCubeShader = rm.getShader("lightCube");
	ctx.gaussianBlurShader = rm.getShader("gaussianBlur");
	ctx.boneShader = rm.getShader("bone");
	ctx.volumeShader = rm.getShader("volume");

	ctx.renderables = rm.getSceneView<RenderComponent>();
	ctx.staticMeshes = rm.getSceneView<DynamicMaterialComponent>();
	ctx.pointLights = rm.getSceneView<PointLightComponent>();
	ctx.directionLights = rm.getSceneView<DirectionLightComponent>();
	ctx.spotLights = rm.getSceneView<SpotLightComponent>();
	ctx.skyboxes = rm.getSceneView<SkyBoxComponent>();
	ctx.volumes = rm.getSceneView<VolumeMarkerComponent>();
	ctx.skeletons = rm.getSceneView<SkeletonViewerComponent>();

	ctx.uboMatrices = &uboMatrices;
	ctx.ssboPointLights = &ssboPointLights;
	ctx.ssboDirectionLight = &ssboDirectionLight;
	ctx.ssboSpotLights = &ssboSpotLights;
	ctx.directionLightDepthFBO = &directionLightDepthFBO;
	ctx.pointLightDepthFBO = &pointLightDepthFBO;
	ctx.hdrFBO = &hdrFBO;
	ctx.pingpongFBO[0] = &pingpongFBO[0];
	ctx.pingpongFBO[1] = &pingpongFBO[1];
	ctx.afterEffectFBO = &afterEffectFBO;
	ctx.sceneFBO = &sceneFBO;
	ctx.directionLightDepthTexture = &directionLightDepthTexture;
	ctx.pointLightDepthTexture = &pointLightDepthTexture;
	ctx.hdrTexture = &hdrTexture;
	ctx.brightTexture = &brightTexture;
	ctx.hdrDepthTexture = &hdrDepthTexture;
	ctx.pingpongTexture[0] = &pingpongTexture[0];
	ctx.pingpongTexture[1] = &pingpongTexture[1];
	ctx.afterEffectTexture = &afterEffectTexture;
	ctx.sceneColorTexture = &sceneColorTexture;
	ctx.owner = this;
	ctx.worleyNoiseTexture3D = &worleyNoiseTexture3D;
	ctx.perlinNoiseTexture3D = &perlinNoiseTexture3D;
	ctx.weatherMapTexture = &weatherMapTexture;
	ctx.noiseReady = &noiseReady;
	return ctx;
}

void RenderSystem::render(Camera& camera) {
	RenderContext ctx = buildContext(camera);

	// Upload camera matrices + compute frustum before the pass loop (they feed ctx).
	ctx.uboMatrices->bind();
	ctx.uboMatrices->bufferSubdata(0, sizeof(glm::mat4), glm::value_ptr(camera.getViewMat()));
	ctx.uboMatrices->bufferSubdata(sizeof(glm::mat4), sizeof(glm::mat4), glm::value_ptr(camera.getProjectionMat(ctx.width, ctx.height)));
	ctx.uboMatrices->unbind();
	ctx.frustum = camera.getFrustum(ctx.width, ctx.height);

	for (auto& pass : passes) {
#ifdef _DEBUG
		glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, pass->name());
#endif
		pass->execute(ctx);
#ifdef _DEBUG
		glPopDebugGroup();
#endif
	}
}
