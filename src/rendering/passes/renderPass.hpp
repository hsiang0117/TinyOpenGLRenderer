#ifndef RENDERPASS_HPP
#define RENDERPASS_HPP
#pragma once

#include "rendering/camera.hpp"      // Camera, Frustum
#include "scene/gameObject.hpp"  // GameObjectPtr
#include "rendering/opengl/shader.hpp"
#include <vector>

class RenderSystem;
class UniformBuffer;
class ShaderStorageBuffer;
class FrameBuffer;
class Texture2D;
class Texture3D;
class CubeMapArray;
class CloudTemporal;

// Per-frame render context. Owns NOTHING GL: all targets are raw non-owning
// pointers back into RenderSystem members (which outlive the per-frame context).
// GL wrapper types are move-only, so they cannot be held by value here.
struct RenderContext {
	// frame inputs (by value / small)
	Camera* camera = nullptr;
	Frustum frustum;
	float width = 0, height = 0;

	// Per-frame copies of the cached scene views. modelObjects contain
	// ModelRenderComponent; materialMeshObjects contain DynamicMaterialComponent.
	std::vector<GameObjectPtr> modelObjects;
	std::vector<GameObjectPtr> materialMeshObjects;
	std::vector<GameObjectPtr> pointLights;
	std::vector<GameObjectPtr> directionalLights;
	std::vector<GameObjectPtr> spotLights;
	std::vector<GameObjectPtr> skyboxes;
	std::vector<GameObjectPtr> cloudVolumes;
	std::vector<GameObjectPtr> skeletons;

	// Currently-selected object (selection gizmo), or null if nothing is selected.
	GameObjectPtr selectedObject;

	// shaders (shared_ptr by value, fetched once per frame)
	ShaderPtr forwardShader, skyboxShader, directionalShadowShader, compositeShader,
	          pointShadowShader, lightMarkerShader, gaussianBlurShader,
	          skeletonShader, cloudRaymarchShader, cloudUpsampleShader, gizmoShader;
	ShaderPtr cloudTemporalShader;
	const void* sceneIdentity = nullptr;
	CloudTemporal* cloudTemporal = nullptr;

	// shared GL targets: references into RenderSystem (NOT owned)
	UniformBuffer* cameraMatricesUbo = nullptr;
	ShaderStorageBuffer* pointLightsSsbo = nullptr;
	ShaderStorageBuffer* directionalLightSsbo = nullptr;
	ShaderStorageBuffer* spotLightsSsbo = nullptr;
	ShaderStorageBuffer* cloudVolumesSsbo = nullptr;

	FrameBuffer* directionalShadowFbo = nullptr;
	FrameBuffer* pointShadowFbo = nullptr;
	FrameBuffer* hdrFbo = nullptr;
	FrameBuffer* bloomBlurFbos[2] = { nullptr, nullptr };
	FrameBuffer* overlayFbo = nullptr;
	FrameBuffer* cloudRaymarchFbo = nullptr;            // half-res cloud raymarch target
	FrameBuffer* sceneFbo = nullptr;            // offscreen target for the Viewport panel

	Texture2D* directionalShadowTexture = nullptr;
	CubeMapArray* pointShadowTexture = nullptr;
	Texture2D* hdrColorTexture = nullptr;       // linear HDR scene color
	Texture2D* bloomBrightTexture = nullptr;
	Texture2D* hdrDepthTexture = nullptr;
	Texture2D* bloomBlurTextures[2] = { nullptr, nullptr };
	Texture2D* overlayColorTexture = nullptr;   // linear HDR, premultiplied alpha
	Texture2D* cloudRaymarchTexture = nullptr;  // half-res, linear HDR, premultiplied alpha

	// Half-res cloud target dimensions (overlay and scene use full-res width/height).
	int cloudRaymarchWidth = 0, cloudRaymarchHeight = 0;

	// volume-cloud noise: owned by RenderSystem; pass reads + triggers lazy gen
	RenderSystem* owner = nullptr;
	Texture3D* worleyNoiseTexture = nullptr;
	Texture3D* perlinNoiseTexture = nullptr;
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
