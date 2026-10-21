#ifndef RENDERSYSTEM_HPP
#define RENDERSYSTEM_HPP
#pragma once

#include "rendering/opengl/glBuffer.hpp"
#include "rendering/opengl/texture.hpp"
#include "rendering/clouds/noiseVolumeData.hpp"
#include "rendering/clouds/cloudTemporal.hpp"
#include <glad/glad.h>
#include <future>
#include <memory>
#include <vector>
#include <array>
#include <cstdint>
#include <cstddef>
#include <string>

class Camera;
class ResourceManager;
class SelectionModel;
class RenderPass;
struct RenderContext;

struct RenderPassTiming {
	std::string name;
	double cpuMs = 0, gpuMs = 0;
};

// CPU and GPU values always belong to the same completed sample frame.
struct RenderProfile {
	bool valid = false;
	std::uint64_t frame = 0;
	int width = 0, height = 0;
	std::size_t opaqueObjectCount = 0;
	std::size_t pointLightCount = 0, directionalLightCount = 0, spotLightCount = 0;
	std::size_t cloudVolumeCount = 0;
	double cpuMs = 0, gpuMs = 0, prepareCpuMs = 0, prepareGpuMs = 0;
	std::vector<RenderPassTiming> passes;
};

class RenderSystem {
public:
	RenderSystem();
	~RenderSystem();
	void init();
	// Must be called while the window's GL context is still current. Idempotent.
	void shutdown();
	void update(double deltaTime);
	void render(Camera& camera);

	void setResourceManager(ResourceManager* rm) { rm_ = rm; }
	void setSelectionModel(SelectionModel* s) { selection_ = s; }

	// Public so render passes (which receive RenderSystem* via RenderContext::owner)
	// can trigger lazy volume-noise generation.
	void ensureCloudNoise(bool sceneHasVolume);
	bool cloudNoiseReady() const { return noiseReady; }

	// Viewport panel hand-off: GuiSystem reports its content size each frame; the
	// scene is rendered into sceneColorTexture at that size and displayed via Image.
	void setViewportSize(int w, int h);
	unsigned int getSceneColorTextureId() const;
	void setProfilingEnabled(bool enabled);
	bool profilingEnabled() const { return profilingEnabled_; }
	const RenderProfile& getRenderProfile() const { return profile_; }
	std::uint64_t profileAgeFrames() const { return profile_.valid ? renderFrame_ - profile_.frame : 0; }
	std::uint64_t skippedProfileFrames() const { return skippedProfileFrames_; }
private:
	float x, y, width, height;
	UniformBuffer cameraMatricesUbo;
	ShaderStorageBuffer pointLightsSsbo, directionalLightSsbo, spotLightsSsbo, cloudVolumesSsbo;
	FrameBuffer directionalShadowFbo, pointShadowFbo, hdrFbo, bloomBlurFbos[2], overlayFbo, cloudRaymarchFbo;
	Texture2D directionalShadowTexture, hdrColorTexture, bloomBrightTexture, bloomBlurTextures[2], overlayColorTexture, hdrDepthTexture, cloudRaymarchTexture;
	Texture2D weatherMapTexture;
	CloudTemporal cloudTemporal;
	Texture3D worleyNoiseTexture;
	Texture3D perlinNoiseTexture;
	CubeMapArray pointShadowTexture;

	// Offscreen scene target: the final composite lands here; the Viewport panel
	// samples it via ImGui::Image. Sized to the Viewport panel content region.
	FrameBuffer sceneFbo;
	Texture2D sceneColorTexture;
	int viewportW_ = 1280, viewportH_ = 720;   // latest size requested by the Viewport panel

	// 体积云噪声纹理：延迟 + 后台生成。仅当场景出现体积云时才在 worker 线程
	// 并行计算（带磁盘缓存），算完在主线程上传 GL。避免启动时白屏。
	bool noiseRequested = false;   // 是否已发起生成
	bool noiseReady = false;       // GL 纹理是否已就绪
	std::future<NoiseVolumeData> worleyFuture, perlinFuture, weatherFuture;

	// Non-owning; injected by Engine. Source of scene views + shaders.
	ResourceManager* rm_ = nullptr;

	// Non-owning; injected by Engine. Currently-selected object for the gizmo pass.
	SelectionModel* selection_ = nullptr;

	// Ordered render passes, populated once in init() and iterated each frame.
	std::vector<std::unique_ptr<RenderPass>> passes;
	RenderContext buildContext(Camera& camera);

	// Bounded asynchronous query ring. A full ring skips samples, never waits.
	struct ProfileFrame {
		std::vector<GLuint> queries;
		std::vector<GLuint64> timestamps;
		RenderProfile sample;
	};
	std::array<ProfileFrame, 4> profileFrames_;
	RenderProfile profile_;
	bool profilingEnabled_ = false;
	std::size_t profileRead_ = 0, profileWrite_ = 0, pendingProfiles_ = 0;
	std::uint64_t renderFrame_ = 0, skippedProfileFrames_ = 0;
	GLint timestampBits_ = 0;
	void collectProfileResults();
	bool initProfileQueries();
	void shutdownProfileQueries();
	double gpuDurationMs(GLuint64 begin, GLuint64 end) const;
};

#endif // !RENDERSYSTEM_HPP
