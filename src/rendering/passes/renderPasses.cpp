#include "renderPasses.hpp"
#include "rendering/renderUtils.hpp"
#include "rendering/renderSystem.hpp"
#include "rendering/clouds/cloudVolume.hpp"
#include "scene/component.hpp"
#include "rendering/opengl/glBuffer.hpp"
#include "rendering/geometry/mesh.hpp"
#include "rendering/opengl/texture.hpp"
#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <GLFW/glfw3.h>
#include <string>
#include <vector>

// Camera culling only applies to the forward pass. Offscreen objects can cast visible shadows.
static void drawAllRenderables(RenderContext& ctx, ShaderPtr shader, bool cameraCull = true) {
	for (auto& obj : ctx.modelObjects) {
		if (!cameraCull || RenderHelpers::isRenderableInFrustum(obj, ctx.frustum)) RenderHelpers::drawRenderable(obj, shader);
	}
	for (auto& obj : ctx.materialMeshObjects) {
		if (obj->has<ModelRenderComponent>() || obj->has<CloudVolumeComponent>()) continue;
		if (!cameraCull || RenderHelpers::isRenderableInFrustum(obj, ctx.frustum)) RenderHelpers::drawRenderable(obj, shader);
	}
}

void FrameSetupPass::execute(RenderContext& ctx) {
	glDisable(GL_FRAMEBUFFER_SRGB);
	// Unbind program before clearing so the driver does not validate the leftover
	// active program's sampler completeness against unbound units.
	glUseProgram(0);
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// Global fallback: unit 8 is the bone-matrix texture, declared by the
	// directionalShadow/pointShadow/forward shaders. Static-mesh vertices have boneIds all -1 so
	// the shader skips the texelFetch and never truly samples it; we only need a
	// complete texture on the unit to pass the driver's completeness check. Animated
	// models overwrite unit 8 with their real bone texture when drawn.
	bindDummyTexture2D(GL_TEXTURE8);
}

void DirectionalShadowPass::execute(RenderContext& ctx) {
	glViewport(0, 0, 1024, 1024);
	for (auto& obj : ctx.directionalLights) {
		ctx.directionalShadowShader->use();
		auto shadowCaster = obj->getComponent<ShadowCaster2D>();
		if (shadowCaster && shadowCaster->enabled) {
			ctx.directionalShadowShader->setMat4("lightMatrices", RenderHelpers::directionalShadowMatrix(obj));
			ctx.directionalShadowFbo->bind();
			ctx.directionalShadowFbo->attachTexture2D(*ctx.directionalShadowTexture, GL_DEPTH_ATTACHMENT);
			glClear(GL_DEPTH_BUFFER_BIT);
			glEnable(GL_CULL_FACE);
			glCullFace(GL_FRONT);
			drawAllRenderables(ctx, ctx.directionalShadowShader, false);
			glDisable(GL_CULL_FACE);
			ctx.directionalShadowFbo->unbind();
		}
		else {
			ctx.directionalShadowFbo->bind();
			ctx.directionalShadowFbo->attachTexture2D(*ctx.directionalShadowTexture, GL_DEPTH_ATTACHMENT);
			glClear(GL_DEPTH_BUFFER_BIT);
			ctx.directionalShadowFbo->unbind();
		}
	}
}

void PointShadowPass::execute(RenderContext& ctx) {
	glViewport(0, 0, 1024, 1024);
	int pointLightIndex = 0;
	for (auto& obj : ctx.pointLights) {
		ctx.pointShadowShader->use();
		auto shadowCaster = obj->getComponent<ShadowCasterCube>();
		if (shadowCaster && shadowCaster->enabled && shadowCaster->farPlane > 0.1f) {
			auto transform = obj->getComponent<Transform>();
			ctx.pointShadowShader->setVec3("lightPos", transform->translate);
			ctx.pointShadowShader->setFloat("farPlane", shadowCaster->farPlane);
			ctx.pointShadowShader->setInt("lightIndex", pointLightIndex);
			std::vector<glm::mat4> lightMatrices = RenderHelpers::pointShadowMatrices(obj);
			for (int j = 0; j < 6; j++) {
				ctx.pointShadowShader->setMat4(("lightMatrices[" + std::to_string(j) + "]").c_str(), lightMatrices[j]);
			}
			ctx.pointShadowFbo->bind();
			for (int j = 0; j < 6; j++) {
				ctx.pointShadowFbo->attachTextureLayer(*ctx.pointShadowTexture, GL_DEPTH_ATTACHMENT, pointLightIndex * 6 + j);
				glClear(GL_DEPTH_BUFFER_BIT);
			}
			ctx.pointShadowFbo->attachTexture(*ctx.pointShadowTexture, GL_DEPTH_ATTACHMENT);
			glEnable(GL_CULL_FACE);
			glCullFace(GL_FRONT);
			drawAllRenderables(ctx, ctx.pointShadowShader, false);
			glDisable(GL_CULL_FACE);
			ctx.pointShadowFbo->unbind();
		}
		else {
			ctx.pointShadowFbo->bind();
			for (int j = 0; j < 6; j++) {
				ctx.pointShadowFbo->attachTextureLayer(*ctx.pointShadowTexture, GL_DEPTH_ATTACHMENT, pointLightIndex * 6 + j);
				glClear(GL_DEPTH_BUFFER_BIT);
			}
			ctx.pointShadowFbo->unbind();
		}
		pointLightIndex++;
	}
}

void LightUploadPass::execute(RenderContext& ctx) {
	ctx.forwardShader->use();
	ctx.forwardShader->setBool("directionalShadowEnabled", false);
	{
		int idx = 0;
		for (auto& obj : ctx.pointLights) {
			RenderHelpers::uploadPointLight(obj, idx++, *ctx.pointLightsSsbo);
		}
	}
	{
		int idx = 0;
		for (auto& obj : ctx.spotLights) {
			RenderHelpers::uploadSpotLight(obj, idx++, *ctx.spotLightsSsbo);
		}
	}
	for (auto& obj : ctx.directionalLights) {
		RenderHelpers::uploadDirectionalLight(obj, *ctx.directionalLightSsbo);
		auto shadowCaster = obj->getComponent<ShadowCaster2D>();
		if (shadowCaster && shadowCaster->enabled) {
			ctx.forwardShader->setBool("directionalShadowEnabled", true);
			ctx.forwardShader->setMat4("lightSpaceMatrix", RenderHelpers::directionalShadowMatrix(obj));
		}
	}
	ctx.forwardShader->setInt("pointLightCount", (int)ctx.pointLights.size());
	ctx.forwardShader->setInt("directionalLightCount", (int)ctx.directionalLights.size());
	ctx.forwardShader->setInt("spotLightCount", (int)ctx.spotLights.size());
	ctx.directionalShadowTexture->use(GL_TEXTURE6);
	ctx.pointShadowTexture->use(GL_TEXTURE7);
}

void ForwardHdrPass::execute(RenderContext& ctx) {
	glViewport(0, 0, (GLsizei)ctx.width, (GLsizei)ctx.height);

	ctx.hdrFbo->bind();
	// Unbind program before clear (see FrameSetupPass): avoids the driver
	// validating the previous active program's samplers against unbound units.
	glUseProgram(0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	ctx.forwardShader->use();
	ctx.forwardShader->setVec3("cameraPos", ctx.camera->getPos());
	for (auto& obj : ctx.skyboxes) {
		RenderHelpers::bindSkyboxCubeMap(obj, ctx.forwardShader);
	}
	drawAllRenderables(ctx, ctx.forwardShader);

	ctx.lightMarkerShader->use();
	for (auto& obj : ctx.pointLights) {
		if (obj->has<StaticMeshComponent>() && RenderHelpers::isPointLightInFrustum(obj, ctx.frustum)) {
			RenderHelpers::drawLightMarker(obj, ctx.lightMarkerShader);
		}
	}

	ctx.skyboxShader->use();
	for (auto& obj : ctx.skyboxes) {
		RenderHelpers::drawSkybox(obj, ctx.skyboxShader);
	}
	ctx.hdrFbo->unbind();
}

void CloudSkeletonOverlayPass::execute(RenderContext& ctx) {
	// Lazy volume-cloud noise: only kick off background generation when a volume
	// exists; skip volume drawing until the noise is ready.
	if (!*ctx.noiseReady) ctx.owner->ensureCloudNoise(!ctx.cloudVolumes.empty());
	bool drawClouds = *ctx.noiseReady && !ctx.cloudVolumes.empty();
	Texture2D* resolvedCloud = ctx.cloudRaymarchTexture;
	if (!drawClouds && ctx.cloudTemporal) ctx.cloudTemporal->invalidate();

	// --- Pass 1: raymarch clouds into the half-res cloud target. -------------
	// Clouds are low frequency, so the expensive march runs at half resolution
	// (~4x fewer fragments). cloudRaymarch.frag derives its uv from gl_FragCoord/resolution,
	// so it just needs the half-res viewport + resolution uniform; depthMap is the
	// full-res scene depth, sampled in normalized uv, so depth-aware termination
	// still works.
	if (drawClouds) {
		glViewport(0, 0, (GLsizei)ctx.cloudRaymarchWidth, (GLsizei)ctx.cloudRaymarchHeight);
		ctx.cloudRaymarchFbo->bind();
		glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
		glUseProgram(0);
		glClear(GL_COLOR_BUFFER_BIT);

		ctx.cloudRaymarchShader->use();
		ctx.cloudRaymarchShader->setVec3("cameraPos", ctx.camera->getPos());
		ctx.cloudRaymarchShader->setInt("depthMap", 0);
		ctx.cloudRaymarchShader->setInt("worleyNoiseTexture", 1);
		ctx.cloudRaymarchShader->setInt("weatherMap", 2);
		ctx.cloudRaymarchShader->setInt("perlinNoiseTexture", 3);
		ctx.cloudRaymarchShader->setVec2("resolution", glm::vec2(ctx.cloudRaymarchWidth, ctx.cloudRaymarchHeight));
		const float seconds = (float)glfwGetTime();
		ctx.cloudRaymarchShader->setFloat("time", seconds);
		glm::mat4 projection = ctx.camera->getProjectionMat(ctx.width, ctx.height);
		glm::mat4 vp = projection * ctx.camera->getViewMat(), invVP = glm::inverse(vp);
		ctx.cloudRaymarchShader->setMat4("invVP", invVP);

		glm::vec3 lightDir(0, 1, 0), lightColor(1);
		for (auto& obj : ctx.directionalLights) {
			auto transform = obj->getComponent<Transform>();
			auto directionalLight = obj->getComponent<DirectionalLightComponent>();
			glm::vec3 direction = RenderHelpers::directionFromTransform(transform);
			lightDir = -direction;
			lightColor = directionalLight->color * directionalLight->brightness;
			break;
		}
		ctx.cloudRaymarchShader->setVec3("lightDir", lightDir);
		ctx.cloudRaymarchShader->setVec3("lightColor", lightColor);
		ctx.hdrDepthTexture->use(GL_TEXTURE0);
		ctx.worleyNoiseTexture->use(GL_TEXTURE1);
		ctx.weatherMapTexture->use(GL_TEXTURE2);
		ctx.perlinNoiseTexture->use(GL_TEXTURE3);
		std::vector<CloudVolumeGpu> cloudVolumes;
		bool temporalEnabled = true;
		for (auto& obj : ctx.cloudVolumes) {
			auto marker = obj->getComponent<CloudVolumeComponent>();
			if (!marker) continue;
			const bool skyLayer = marker->mode == CloudVolumeComponent::Mode::SkyLayer;
			glm::vec3 lo(0.0f), hi(0.0f);
			if (skyLayer) {
				if (marker->layerThickness <= 0.0001f || marker->maxTraceDistance <= 0.0f) continue;
			} else {
				auto mesh = obj->getComponent<StaticMeshComponent>();
				auto transform = obj->getComponent<Transform>();
				if (!mesh || !transform) continue;
				glm::vec3 a = transform->translate + transform->scale * mesh->aabb.min;
				glm::vec3 b = transform->translate + transform->scale * mesh->aabb.max;
				lo = glm::min(a, b); hi = glm::max(a, b);
				if (glm::any(glm::lessThanEqual(hi - lo, glm::vec3(0.0001f)))) continue;
			}
			const auto& v = *marker;
			temporalEnabled = temporalEnabled && v.temporalAccumulation;
			cloudVolumes.push_back({lo, v.cloudType, hi, v.coverageLow,
				v.coverageHigh, v.baseNoiseScale, v.warpScale, v.warpStrength,
				v.detailScale, v.detailErosionStrength, v.extinctionCoefficient, v.densityMultiplier,
				v.scatteringAlbedo, v.multipleScattering, v.edgeFadeDistance, v.viewStepWorldSize,
				v.windVelocity, skyLayer ? 1 : 0,
				v.layerBottom, v.layerThickness, v.maxTraceDistance, v.distanceFadeStart,
				v.weatherScale, v.ambientStrength, v.phaseForwardG, v.sunStrength,
				v.viewMaxSteps, v.lightSteps, v.emptyStepMultiplier, 0,
				v.regionScale, v.regionCoverage, v.densityVariation, v.heightVariation});
		}
		if (ctx.cloudTemporal) {
			CloudTemporal::Frame frame;
			frame.vp = vp; frame.invVP = invVP; frame.projection = projection;
			frame.cameraPos = ctx.camera->getPos(); frame.cameraFront = ctx.camera->getFront();
			frame.lightDir = lightDir; frame.lightColor = lightColor; frame.seconds = seconds;
			frame.scene = ctx.sceneIdentity; frame.volumes = cloudVolumes;
			frame.raymarchProgram = ctx.cloudRaymarchShader->ID; frame.resolveProgram = ctx.cloudTemporalShader->ID;
			frame.noiseTextures = {ctx.perlinNoiseTexture->ID, ctx.worleyNoiseTexture->ID, ctx.weatherMapTexture->ID};
			ctx.cloudTemporal->beginFrame(frame, temporalEnabled);
		}
		ctx.cloudRaymarchShader->setFloat("jitterOffset", ctx.cloudTemporal ? ctx.cloudTemporal->jitterOffset() : 0.0f);
		ctx.cloudVolumesSsbo->bind();
		ctx.cloudVolumesSsbo->bufferBase(4);
		ctx.cloudVolumesSsbo->bufferData(cloudVolumes.size() * sizeof(CloudVolumeGpu), cloudVolumes.data());
		ctx.cloudVolumesSsbo->unbind();
		ctx.cloudRaymarchShader->setInt("cloudVolumeCount", static_cast<int>(cloudVolumes.size()));
		glDisable(GL_DEPTH_TEST);
		glDisable(GL_BLEND);
		RenderHelpers::drawFullscreenQuad();
		glEnable(GL_DEPTH_TEST);
		ctx.cloudRaymarchFbo->unbind();
		if (ctx.cloudTemporal) resolvedCloud = &ctx.cloudTemporal->resolve(*ctx.cloudRaymarchTexture, *ctx.cloudTemporalShader);
	}

	// --- Pass 2: upsample clouds (+ skeleton overlay) into overlayFbo. ----
	glViewport(0, 0, (GLsizei)ctx.width, (GLsizei)ctx.height);
	ctx.overlayFbo->bind();
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	// Unbind program before clear (see FrameSetupPass): avoids the driver validating
	// a leftover active program's samplers against unbound units.
	glUseProgram(0);
	glClear(GL_COLOR_BUFFER_BIT);

	// Depth-aware bilateral upsample from the half-res cloud target. Skipped when no
	// clouds were drawn, leaving overlayColor cleared to (0,0,0,0) (a no-op in the
	// composite, so the scene shows through).
	if (drawClouds) {
		ctx.cloudUpsampleShader->use();
		ctx.cloudUpsampleShader->setInt("cloudRaymarchTexture", 0);
		ctx.cloudUpsampleShader->setInt("depthTexture", 1);
		ctx.cloudUpsampleShader->setVec2("cloudTexSize", glm::vec2(ctx.cloudRaymarchWidth, ctx.cloudRaymarchHeight));
		// Must match Camera::getProjectionMat (near 0.1, far 100.0).
		ctx.cloudUpsampleShader->setFloat("nearPlane", 0.1f);
		ctx.cloudUpsampleShader->setFloat("farPlane", 100.0f);
		resolvedCloud->use(GL_TEXTURE0);
		ctx.hdrDepthTexture->use(GL_TEXTURE1);
		RenderHelpers::drawFullscreenQuad();
	}

	ctx.skeletonShader->use();
	for (auto& obj : ctx.skeletons) {
		if (RenderHelpers::isRenderableInFrustum(obj, ctx.frustum)) RenderHelpers::drawSkeleton(obj, ctx.skeletonShader);
	}

	ctx.overlayFbo->unbind();
}

void GizmoPass::execute(RenderContext& ctx) {
	if (!ctx.selectedObject) return;
	auto cloud = ctx.selectedObject->getComponent<CloudVolumeComponent>();
	if (cloud && cloud->mode == CloudVolumeComponent::Mode::SkyLayer) return;
	auto transform = ctx.selectedObject->getComponent<Transform>();
	if (!transform) return;

	// Constant screen size: scale by distance to the camera so the gizmo subtends a
	// roughly fixed angle. v1 ignores fov (no getter on Camera) -> mild size drift on
	// scroll-zoom; acceptable. k tuned by eye.
	const float kGizmoScreenScale = 0.15f;
	glm::vec3 origin = transform->translate;
	float dist = glm::length(ctx.camera->getPos() - origin);
	float gizmoScale = dist * kGizmoScreenScale;

	// model = translate(origin) * object-rotation (Z,Y,X like buildModelMatrix) *
	// uniform scale. Deliberately excludes the object's own scale (a 10x object must
	// not give a 10x gizmo) and uses only its rotation.
	glm::mat4 model = glm::translate(glm::mat4(1.0f), origin);
	model = glm::rotate(model, glm::radians(transform->rotate.z), glm::vec3(0, 0, 1));
	model = glm::rotate(model, glm::radians(transform->rotate.y), glm::vec3(0, 1, 0));
	model = glm::rotate(model, glm::radians(transform->rotate.x), glm::vec3(1, 0, 0));
	model = glm::scale(model, glm::vec3(gizmoScale));

	// Draw into the shared overlay target (no depth attachment) so the gizmo is
	// always on top. Do NOT clear: it overlays whatever CloudSkeletonOverlayPass left there.
	glViewport(0, 0, (GLsizei)ctx.width, (GLsizei)ctx.height);
	ctx.overlayFbo->bind();
	glDisable(GL_DEPTH_TEST);
	// No backface culling: the arrows are single-color and this target has no depth
	// buffer, so front and back faces rasterize to the identical color -> the solid
	// arrow silhouette is correct regardless of triangle winding. Disabling cull (the
	// global baseline anyway) keeps correctness independent of the geometry's winding.
	glDisable(GL_CULL_FACE);
	glDisable(GL_BLEND);

	ctx.gizmoShader->use();
	ctx.gizmoShader->setMat4("model", model);
	RenderHelpers::drawSelectionGizmo();

	ctx.overlayFbo->unbind();
	// Restore the global baseline the rest of the pipeline expects (cull stays disabled).
	glEnable(GL_DEPTH_TEST);
}

void BloomPass::execute(RenderContext& ctx) {
	// The linked composite program eliminates bloomBlurTexture when USE_BLOOM is off.
	// Query through the shader cache so this follows successful hot reloads.
	if (!ctx.compositeShader->hasUniform("bloomBlurTexture")) return;
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	ctx.gaussianBlurShader->use();
	for (int i = 0; i < 10; i++) {
		ctx.bloomBlurFbos[i % 2]->bind();
		ctx.gaussianBlurShader->setBool("horizontal", i % 2 == 0);
		// Bind the sampled texture before clearing so the current program's sampler
		// unit is already complete at clear time.
		if (i == 0) ctx.bloomBrightTexture->use(GL_TEXTURE0);
		else ctx.bloomBlurTextures[(i + 1) % 2]->use(GL_TEXTURE0);
		glClear(GL_COLOR_BUFFER_BIT);
		RenderHelpers::drawFullscreenQuad();
		ctx.bloomBlurFbos[i % 2]->unbind();
	}
}

void CompositePass::execute(RenderContext& ctx) {
	// Composite into the offscreen scene target (the Viewport panel samples it),
	// not the backbuffer. Color-only FBO; disable depth test so the fullscreen
	// quad (z=0) is never rejected by the global GL_LEQUAL.
	ctx.sceneFbo->bind();
	glViewport(0, 0, (GLsizei)ctx.width, (GLsizei)ctx.height);
	glDisable(GL_DEPTH_TEST);
	ctx.compositeShader->use();
	ctx.hdrColorTexture->use(GL_TEXTURE0);
	ctx.bloomBlurTextures[1]->use(GL_TEXTURE1);
	ctx.overlayColorTexture->use(GL_TEXTURE2);
	RenderHelpers::drawFullscreenQuad();
	glEnable(GL_DEPTH_TEST);
	ctx.sceneFbo->unbind();
}
