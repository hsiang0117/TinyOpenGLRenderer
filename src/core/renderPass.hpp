#ifndef RENDERPASS_HPP
#define RENDERPASS_HPP
#pragma once

#include "../camera.hpp"      // Camera, Frustum
#include "../gameObject.hpp"  // GameObjectPtr
#include "../glBuffer.hpp"
#include "../texture.hpp"
#include "../shader.hpp"
#include <vector>

class RenderSystem;

// Per-frame render context. Owns NOTHING GL: all targets are raw non-owning
// pointers back into RenderSystem members (which outlive the per-frame context).
// GL wrapper types are move-only, so they cannot be held by value here.
struct RenderContext {
	// frame inputs (by value / small)
	Camera* camera = nullptr;
	Frustum frustum;
	float x = 0, y = 0, width = 0, height = 0;

	// scene snapshots (getSceneView<T>() returns vectors by value)
	std::vector<GameObjectPtr> renderables;
	std::vector<GameObjectPtr> staticMeshes;
	std::vector<GameObjectPtr> pointLights;
	std::vector<GameObjectPtr> directionLights;
	std::vector<GameObjectPtr> spotLights;
	std::vector<GameObjectPtr> skyboxes;
	std::vector<GameObjectPtr> volumes;
	std::vector<GameObjectPtr> skeletons;

	// Currently-selected object (selection gizmo), or null if nothing is selected.
	GameObjectPtr selectedObject;

	// shaders (shared_ptr by value, fetched once per frame)
	ShaderPtr defaultShader, skyboxShader, depthShader, screenQuadShader,
	          depthCubeShader, lightCubeShader, gaussianBlurShader,
	          boneShader, volumeShader, cloudUpsampleShader, gizmoShader;

	// shared GL targets: references into RenderSystem (NOT owned)
	UniformBuffer* uboMatrices = nullptr;
	ShaderStorageBuffer* ssboPointLights = nullptr;
	ShaderStorageBuffer* ssboDirectionLight = nullptr;
	ShaderStorageBuffer* ssboSpotLights = nullptr;

	FrameBuffer* directionLightDepthFBO = nullptr;
	FrameBuffer* pointLightDepthFBO = nullptr;
	FrameBuffer* hdrFBO = nullptr;
	FrameBuffer* pingpongFBO[2] = { nullptr, nullptr };
	FrameBuffer* afterEffectFBO = nullptr;
	FrameBuffer* cloudFBO = nullptr;            // half-res cloud raymarch target
	FrameBuffer* sceneFBO = nullptr;            // offscreen target for the Viewport panel

	Texture2D* directionLightDepthTexture = nullptr;
	CubeMapArray* pointLightDepthTexture = nullptr;
	Texture2D* hdrTexture = nullptr;
	Texture2D* brightTexture = nullptr;
	Texture2D* hdrDepthTexture = nullptr;
	Texture2D* pingpongTexture[2] = { nullptr, nullptr };
	Texture2D* afterEffectTexture = nullptr;
	Texture2D* cloudTexture = nullptr;          // half-res cloud raymarch result
	Texture2D* sceneColorTexture = nullptr;     // composite lands here (Viewport panel)

	// Half-res cloud target dimensions (afterEffect/scene are full-res width/height).
	int cloudWidth = 0, cloudHeight = 0;

	// volume-cloud noise: owned by RenderSystem; pass reads + triggers lazy gen
	RenderSystem* owner = nullptr;
	Texture3D* worleyNoiseTexture3D = nullptr;
	Texture3D* perlinNoiseTexture3D = nullptr;
	Texture2D* weatherMapTexture = nullptr;
	bool* noiseReady = nullptr;
};

class RenderPass {
public:
	virtual ~RenderPass() = default;
	virtual void execute(RenderContext& ctx) = 0;
	virtual const char* name() const = 0;
};

#endif // !RENDERPASS_HPP
