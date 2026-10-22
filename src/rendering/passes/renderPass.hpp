#ifndef TOGR_RENDERPASS_HPP
#define TOGR_RENDERPASS_HPP
#pragma once

#include "rendering/camera.hpp"
#include "scene/gameObject.hpp"
#include "rendering/opengl/shader.hpp"
#include <vector>

namespace togr {

class RenderSystem;
class UniformBuffer;
class ShaderStorageBuffer;
class FrameBuffer;
class Texture2D;
class Texture3D;
class CubeMapArray;
class CloudTemporal;

// Frame-local inputs and resource references, assembled by RenderSystem.
// Scene objects and shaders are shared owners held for this frame; camera,
// buffers, targets and cloud services are borrowed from longer-lived owners.
// This is not a GL-state snapshot: a resource pointer does not bind it, and
// shared object views do not make the components themselves immutable.
struct RenderContext {
    // Applied viewport size and camera input. Requested sizes take effect in update().
    Camera* camera = nullptr;
    Frustum frustum;
    float width = 0, height = 0;

    // Shared scene-view snapshots. Light lists are clamped before pass execution.
    // modelObjects contain ModelRenderComponent; materialMeshObjects contain
    // DynamicMaterialComponent and may overlap models/clouds (draw helpers filter).
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

    // Shared programs fetched once per frame. Uniform values belong to each
    // program object; changing the active program does not erase those values.
    ShaderPtr forwardShader, skyboxShader, directionalShadowShader, compositeShader,
              pointShadowShader, lightMarkerShader, gaussianBlurShader,
              skeletonShader, cloudRaymarchShader, cloudUpsampleShader, gizmoShader;
    ShaderPtr cloudTemporalShader;

    // Borrowed buffers. Indexed binding slots: camera UBO 0; point/directional/
    // spot SSBOs 1/2/3; cloud SSBO 4. Consumers establish the required bindings.
    UniformBuffer* cameraMatricesUbo = nullptr;
    ShaderStorageBuffer* pointLightsSsbo = nullptr;
    ShaderStorageBuffer* directionalLightSsbo = nullptr;
    ShaderStorageBuffer* spotLightsSsbo = nullptr;
    ShaderStorageBuffer* cloudVolumesSsbo = nullptr;

    // Borrowed targets. Passes bind their destination explicitly; Gizmo appends
    // to overlay instead of clearing it. These pointers never transfer ownership.
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

    // Borrowed cloud history and scene identity used to validate that history.
    const void* sceneIdentity = nullptr;
    CloudTemporal* cloudTemporal = nullptr;

    // Borrowed noise resources. The overlay pass invokes only ensureCloudNoise()
    // on owner, at its existing execution/measurement point. CPU workers never
    // touch GL; the renderer publishes completed textures on this thread.
    // noiseReady is a live read-only view, not the initial readiness snapshot.
    // It is required by the overlay pass; owner is required while it is false.
    RenderSystem* owner = nullptr;
    Texture3D* worleyNoiseTexture = nullptr;
    Texture3D* perlinNoiseTexture = nullptr;
    Texture2D* weatherMapTexture = nullptr;
    const bool* noiseReady = nullptr;
};

class RenderPass {
public:
    // Executes on the render thread with the GL context current. FrameSetup
    // establishes the baseline required by later passes. Populate each pass's inputs.
    // Passes may leave their program/texture bindings active and set raster state
    // back to the pipeline baseline; they do not restore arbitrary caller GL state.
    // Dependencies and target contents still require the registered pass order.
    virtual ~RenderPass() = default;
    virtual void execute(RenderContext& ctx) = 0;
    virtual const char* name() const = 0;
};

} // namespace togr

#endif // !TOGR_RENDERPASS_HPP
