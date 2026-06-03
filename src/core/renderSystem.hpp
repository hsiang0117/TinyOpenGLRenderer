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

class RenderSystem {
public:
	RenderSystem() = default;
	~RenderSystem() = default;
	void init();
	void update(double deltaTime);
	void render(Camera& camera);

	// Public so render passes (which receive RenderSystem* via RenderContext::owner)
	// can trigger lazy volume-noise generation.
	void ensureVolumeNoise(bool sceneHasVolume);
private:
	float x, y, width, height;
	UniformBuffer uboMatrices;
	ShaderStorageBuffer ssboPointLights, ssboDirectionLight, ssboSpotLights;
	FrameBuffer directionLightDepthFBO, pointLightDepthFBO, hdrFBO, pingpongFBO[2], afterEffectFBO;
	Texture2D directionLightDepthTexture, hdrTexture, brightTexture, pingpongTexture[2], afterEffectTexture, hdrDepthTexture;
	Texture2D weatherMapTexture;
	Texture3D worleyNoiseTexture3D;
	Texture3D perlinNoiseTexture3D;
	CubeMapArray pointLightDepthTexture;

	// 体积云噪声纹理：延迟 + 后台生成。仅当场景出现体积云时才在 worker 线程
	// 并行计算（带磁盘缓存），算完在主线程上传 GL。避免启动时白屏。
	bool noiseRequested = false;   // 是否已发起生成
	bool noiseReady = false;       // GL 纹理是否已就绪
	std::future<NoiseVolumeData> worleyFuture, perlinFuture, weatherFuture;

	// Ordered render passes, populated once in init() and iterated each frame.
	std::vector<std::unique_ptr<RenderPass>> passes;
	RenderContext buildContext(Camera& camera);
};

#endif // !RENDERSYSTEM_HPP
