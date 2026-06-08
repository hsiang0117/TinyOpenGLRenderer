#ifndef RENDERSYSTEM_HPP
#define RENDERSYSTEM_HPP
#pragma once

#include "../glBuffer.hpp"
#include "../texture.hpp"
#include "../camera.hpp"
#include "../component.hpp"
#include "../gameObject.hpp"
#include "../shader.hpp"
#include "../utils.hpp"
#include "renderPass.hpp"
#include <future>
#include <memory>
#include <vector>

class ResourceManager;

class RenderSystem {
public:
	RenderSystem() = default;
	~RenderSystem() = default;
	void init();
	void update(double deltaTime);
	void render(Camera& camera);

	void setResourceManager(ResourceManager* rm) { rm_ = rm; }

	// Public so render passes (which receive RenderSystem* via RenderContext::owner)
	// can trigger lazy volume-noise generation.
	void ensureVolumeNoise(bool sceneHasVolume);

	// Viewport panel hand-off: GuiSystem reports its content size each frame; the
	// scene is rendered into sceneColorTexture at that size and displayed via Image.
	void setViewportSize(int w, int h);
	unsigned int getSceneColorTextureID() const;
private:
	float x, y, width, height;
	UniformBuffer uboMatrices;
	ShaderStorageBuffer ssboPointLights, ssboDirectionLight, ssboSpotLights;
	FrameBuffer directionLightDepthFBO, pointLightDepthFBO, hdrFBO, pingpongFBO[2], afterEffectFBO, cloudFBO;
	Texture2D directionLightDepthTexture, hdrTexture, brightTexture, pingpongTexture[2], afterEffectTexture, hdrDepthTexture, cloudTexture;
	Texture2D weatherMapTexture;
	Texture3D worleyNoiseTexture3D;
	Texture3D perlinNoiseTexture3D;
	CubeMapArray pointLightDepthTexture;

	// Offscreen scene target: the final composite lands here; the Viewport panel
	// samples it via ImGui::Image. Sized to the Viewport panel content region.
	FrameBuffer sceneFBO;
	Texture2D sceneColorTexture;
	int viewportW_ = 1280, viewportH_ = 720;   // latest size requested by the Viewport panel

	// 体积云噪声纹理：延迟 + 后台生成。仅当场景出现体积云时才在 worker 线程
	// 并行计算（带磁盘缓存），算完在主线程上传 GL。避免启动时白屏。
	bool noiseRequested = false;   // 是否已发起生成
	bool noiseReady = false;       // GL 纹理是否已就绪
	std::future<NoiseVolumeData> worleyFuture, perlinFuture, weatherFuture;

	// Non-owning; injected by Engine. Source of scene views + shaders.
	ResourceManager* rm_ = nullptr;

	// Ordered render passes, populated once in init() and iterated each frame.
	std::vector<std::unique_ptr<RenderPass>> passes;
	RenderContext buildContext(Camera& camera);
};

#endif // !RENDERSYSTEM_HPP
