#ifndef TOGR_RENDERPASSES_HPP
#define TOGR_RENDERPASSES_HPP
#pragma once

#include "renderPass.hpp"

namespace togr {

// The renderer executes these nine timed stages in this order. Resources travel
// through RenderContext, while framebuffer, program, texture and buffer bindings
// also persist in GL. FrameSetup establishes the listed pipeline baseline; later
// passes rely on it rather than saving/restoring arbitrary incoming GL state.

// Establish the default FBO/full viewport, depth test LEQUAL with writes, full
// color writes and program point size; disable blend/cull/scissor/framebuffer sRGB.
// Unbind program before clearing, then bind complete bone/environment fallbacks
// on texture units 8/5. Camera UBO upload and frustum preparation precede this pass.
class FrameSetupPass : public RenderPass {
public:
    void execute(RenderContext& ctx) override;
    const char* name() const override { return "FrameSetup"; }
};

// Scene geometry -> directionalShadowTexture; no main-camera culling of casters.
// Uses a 1024-square viewport and temporarily enables front-face culling to draw.
// Processed lights leave FBO 0 and culling disabled; an empty list only sets the
// viewport. Depth/blend follow the frame baseline.
class DirectionalShadowPass : public RenderPass {
public:
    void execute(RenderContext& ctx) override;
    const char* name() const override { return "DirectionalShadow"; }
};

// Scene geometry -> pointShadowTexture cube array; clears six faces per light
// before layered drawing, without main-camera caster culling. Same viewport and
// temporary cull state as DirectionalShadow. Processed lights leave FBO 0 and
// culling disabled; an empty list only sets the viewport.
class PointShadowPass : public RenderPass {
public:
    void execute(RenderContext& ctx) override;
    const char* name() const override { return "PointShadow"; }
};

// Light components -> three SSBO contents and forward-program light/shadow
// uniforms. Does not draw or bind shadow textures. Must precede ForwardHdr, whose
// resource bindings are explicit; uniforms remain stored on the program object.
class LightUploadPass : public RenderPass {
public:
    void execute(RenderContext& ctx) override;
    const char* name() const override { return "LightUpload"; }
};

// Geometry, lights, shadows and camera -> hdrFbo (HDR/bright color plus depth).
// Explicitly binds light SSBOs 1/2/3 and shadow textures 6/7 before drawing models,
// material meshes, light markers and skyboxes. Camera UBO 0 comes from prepare;
// materials/bone palettes/environment use their existing per-draw/fallback slots.
// Leaves FBO 0, full viewport and skybox program; depth/blend/cull stay at baseline.
class ForwardHdrPass : public RenderPass {
public:
    void execute(RenderContext& ctx) override;
    const char* name() const override { return "ForwardHdr"; }
};

// Opaque depth + cloud inputs/history -> half-resolution marching/optional
// temporal resolve -> full-resolution overlay; then append visible skeletons.
// Cloud noise is requested/polled here; no ready clouds invalidate history.
// Always clears overlay to transparent, even when there is no cloud draw. March
// and resolve disable depth/blend, then enable depth again. Leaves FBO 0, full
// viewport and skeleton program; overlay is retained for Gizmo and Composite.
class CloudSkeletonOverlayPass : public RenderPass {
public:
    void execute(RenderContext& ctx) override;
    const char* name() const override { return "CloudSkeletonOverlay"; }
};

// Selection -> constant-screen-size arrows appended to overlayFbo without clear.
// A skipped gizmo changes no state. Drawing disables depth/blend/cull, then leaves
// depth enabled, FBO 0 and full viewport; it does not restore arbitrary prior state.
class GizmoPass : public RenderPass {
public:
    void execute(RenderContext& ctx) override;
    const char* name() const override { return "Gizmo"; }
};

// Bright color -> ten Gaussian ping-pong draws, final result in blur texture 1.
// When disabled, changes no GL state. When enabled, sets the full viewport and
// leaves FBO 0, blur program and texture unit 0; depth/blend/cull follow baseline.
class BloomPass : public RenderPass {
public:
    void execute(RenderContext& ctx) override;
    const char* name() const override { return "Bloom"; }
};

// HDR + optional bloom + premultiplied overlay -> sceneColorTexture for Viewport.
// Uses full viewport and disables depth while drawing; leaves FBO 0, depth enabled,
// composite program and texture unit 2. Display encoding happens in the shader.
class CompositePass : public RenderPass {
public:
    void execute(RenderContext& ctx) override;
    const char* name() const override { return "Composite"; }
};

} // namespace togr

#endif // !TOGR_RENDERPASSES_HPP
