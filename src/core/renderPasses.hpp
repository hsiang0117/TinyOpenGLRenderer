#ifndef RENDERPASSES_HPP
#define RENDERPASSES_HPP
#pragma once

#include "renderPass.hpp"

// Concrete render passes. Each maps to one segment of the former monolithic
// RenderSystem::render(). They share state only through RenderContext; the
// ordering in the pass list encodes the producer->consumer dependencies.

// Frame setup: unbind program, clear the backbuffer, bind the unit-8 bone
// fallback texture. (UBO upload + frustum are built before the pass loop.)
class FrameSetupPass : public RenderPass {
public:
	void execute(RenderContext& ctx) override;
	const char* name() const override { return "FrameSetup"; }
};

// Directional-light shadow map -> directionLightDepthTexture.
class DirectionalShadowPass : public RenderPass {
public:
	void execute(RenderContext& ctx) override;
	const char* name() const override { return "DirectionalShadow"; }
};

// Point-light cube shadow maps -> pointLightDepthTexture (cubemap array).
class PointShadowPass : public RenderPass {
public:
	void execute(RenderContext& ctx) override;
	const char* name() const override { return "PointShadow"; }
};

// Upload light data into the 3 SSBOs, set light counts + lightSpaceMatrix,
// bind shadow maps to TEX6/7. Non-draw. MUST run immediately before
// ForwardHdrPass: the SSBO bindings / TEX6-7 / uniforms it sets on defaultShader
// persist into the forward pass because nothing rebinds between them.
class LightUploadPass : public RenderPass {
public:
	void execute(RenderContext& ctx) override;
	const char* name() const override { return "LightUpload"; }
};

// Forward HDR render -> hdrFBO (MRT: hdr + bright + depth). Renderables, light
// cubes, skybox. Depends on LightUploadPass running immediately before it.
class ForwardHdrPass : public RenderPass {
public:
	void execute(RenderContext& ctx) override;
	const char* name() const override { return "ForwardHdr"; }
};

// Volume clouds + skeleton overlay -> afterEffectFBO (shared target). Volume
// drawing is gated on noise readiness; lazy noise generation is triggered here.
class VolumeSkeletonPass : public RenderPass {
public:
	void execute(RenderContext& ctx) override;
	const char* name() const override { return "VolumeSkeleton"; }
};

// Selection gizmo: XYZ arrows at the selected object's origin, drawn into
// afterEffectFBO (no depth) so it is always on top, at constant screen size.
class GizmoPass : public RenderPass {
public:
	void execute(RenderContext& ctx) override;
	const char* name() const override { return "Gizmo"; }
};

// Gaussian bloom blur, ping-pong x10 -> pingpongTexture.
class BloomPass : public RenderPass {
public:
	void execute(RenderContext& ctx) override;
	const char* name() const override { return "Bloom"; }
};

// Final composite (hdr + bloom + after-effect) -> sceneColorTexture (Viewport panel).
class CompositePass : public RenderPass {
public:
	void execute(RenderContext& ctx) override;
	const char* name() const override { return "Composite"; }
};

#endif // !RENDERPASSES_HPP
