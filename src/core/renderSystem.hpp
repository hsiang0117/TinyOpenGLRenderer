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
#include <future>

class RenderSystem {
public:
	RenderSystem() = default;
	~RenderSystem() = default;
	void init();
	void update(double deltaTime);
	void render(Camera& camera);
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
	void ensureVolumeNoise(bool sceneHasVolume);

	void drawScreenQuad();

	static glm::mat4 buildModelMatrix(const std::shared_ptr<Transform>& transform);
	static glm::vec3 directionFromTransform(const std::shared_ptr<Transform>& transform);
	static bool aabbInFrustum(const glm::vec3& minAABB, const glm::vec3& maxAABB, const glm::mat4& model, Frustum& frustum);
	static bool isRenderableInFrustum(const GameObjectPtr& obj, Frustum& frustum);
	static bool isPointLightInFrustum(const GameObjectPtr& obj, Frustum& frustum);

	static bool isRenderable(const GameObjectPtr& obj);
	static void drawRenderable(const GameObjectPtr& obj, ShaderPtr shader);

	static void drawLightCube(const GameObjectPtr& obj, ShaderPtr shader);

	static void drawSkybox(const GameObjectPtr& obj, ShaderPtr shader);
	static void bindSkyboxCubeMap(const GameObjectPtr& obj, ShaderPtr shader);

	static void drawSkeleton(const GameObjectPtr& obj, ShaderPtr shader);

	static void uploadPointLight(const GameObjectPtr& obj, int index, ShaderStorageBuffer& ssbo);
	static void uploadDirectionLight(const GameObjectPtr& obj, ShaderStorageBuffer& ssbo);
	static void uploadSpotLight(const GameObjectPtr& obj, int index, ShaderStorageBuffer& ssbo);

	static glm::mat4 directionLightMatrices(const GameObjectPtr& obj);
	static std::vector<glm::mat4> pointLightMatricesCube(const GameObjectPtr& obj);
};

#endif // !RENDERSYSTEM_HPP
