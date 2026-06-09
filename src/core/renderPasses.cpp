#include "renderPasses.hpp"
#include "renderUtils.hpp"
#include "renderSystem.hpp"
#include "../component.hpp"
#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <GLFW/glfw3.h>
#include <string>
#include <vector>

// Shared by the shadow passes and the forward HDR pass: draw all renderables /
// dynamic-material static meshes that pass the frustum test.
static void drawAllRenderablesInFrustum(RenderContext& ctx, ShaderPtr shader) {
	for (auto& obj : ctx.renderables) {
		if (RenderHelpers::isRenderableInFrustum(obj, ctx.frustum)) RenderHelpers::drawRenderable(obj, shader);
	}
	for (auto& obj : ctx.staticMeshes) {
		if (obj->has<RenderComponent>() || obj->has<VolumeMarkerComponent>()) continue;
		if (RenderHelpers::isRenderableInFrustum(obj, ctx.frustum)) RenderHelpers::drawRenderable(obj, shader);
	}
}

void FrameSetupPass::execute(RenderContext& ctx) {
	// Unbind program before clearing so the driver does not validate the leftover
	// active program's sampler completeness against unbound units.
	glUseProgram(0);
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// Global fallback: unit 8 is the bone-matrix texture, declared by the
	// depth/depthCube/default shaders. Static-mesh vertices have boneIds all -1 so
	// the shader skips the texelFetch and never truly samples it; we only need a
	// complete texture on the unit to pass the driver's completeness check. Animated
	// models overwrite unit 8 with their real bone texture when drawn.
	bindDummyTexture2D(GL_TEXTURE8);
}

void DirectionalShadowPass::execute(RenderContext& ctx) {
	glViewport(0, 0, 1024, 1024);
	for (auto& obj : ctx.directionLights) {
		ctx.depthShader->use();
		auto shadowCaster = obj->getComponent<ShadowCaster2D>();
		if (shadowCaster && shadowCaster->enabled) {
			ctx.depthShader->setMat4("lightMatrices", RenderHelpers::directionLightMatrices(obj));
			ctx.directionLightDepthFBO->bind();
			ctx.directionLightDepthFBO->attachTexture2D(*ctx.directionLightDepthTexture, GL_DEPTH_ATTACHMENT);
			glClear(GL_DEPTH_BUFFER_BIT);
			glEnable(GL_CULL_FACE);
			glCullFace(GL_FRONT);
			drawAllRenderablesInFrustum(ctx, ctx.depthShader);
			glDisable(GL_CULL_FACE);
			ctx.directionLightDepthFBO->unbind();
		}
		else {
			ctx.directionLightDepthFBO->bind();
			ctx.directionLightDepthFBO->attachTexture2D(*ctx.directionLightDepthTexture, GL_DEPTH_ATTACHMENT);
			glClear(GL_DEPTH_BUFFER_BIT);
			ctx.directionLightDepthFBO->unbind();
		}
	}
}

void PointShadowPass::execute(RenderContext& ctx) {
	glViewport(0, 0, 1024, 1024);
	int pointLightIndex = 0;
	for (auto& obj : ctx.pointLights) {
		ctx.depthCubeShader->use();
		auto shadowCaster = obj->getComponent<ShadowCasterCube>();
		if (shadowCaster && shadowCaster->enabled) {
			auto transform = obj->getComponent<Transform>();
			ctx.depthCubeShader->setVec3("lightPos", transform->translate);
			ctx.depthCubeShader->setFloat("farPlane", shadowCaster->farPlane);
			ctx.depthCubeShader->setInt("lightIndex", pointLightIndex);
			std::vector<glm::mat4> lightMatrices = RenderHelpers::pointLightMatricesCube(obj);
			for (int j = 0; j < 6; j++) {
				ctx.depthCubeShader->setMat4(("lightMatrices[" + std::to_string(j) + "]").c_str(), lightMatrices[j]);
			}
			ctx.pointLightDepthFBO->bind();
			for (int j = 0; j < 6; j++) {
				ctx.pointLightDepthFBO->attachTextureLayer(*ctx.pointLightDepthTexture, GL_DEPTH_ATTACHMENT, pointLightIndex * 6 + j);
				glClear(GL_DEPTH_BUFFER_BIT);
			}
			ctx.pointLightDepthFBO->attachTexture(*ctx.pointLightDepthTexture, GL_DEPTH_ATTACHMENT);
			glEnable(GL_CULL_FACE);
			glCullFace(GL_FRONT);
			drawAllRenderablesInFrustum(ctx, ctx.depthCubeShader);
			glDisable(GL_CULL_FACE);
			ctx.pointLightDepthFBO->unbind();
		}
		else {
			ctx.pointLightDepthFBO->bind();
			for (int j = 0; j < 6; j++) {
				ctx.pointLightDepthFBO->attachTextureLayer(*ctx.pointLightDepthTexture, GL_DEPTH_ATTACHMENT, pointLightIndex * 6 + j);
				glClear(GL_DEPTH_BUFFER_BIT);
			}
			ctx.pointLightDepthFBO->unbind();
		}
		pointLightIndex++;
	}
}

void LightUploadPass::execute(RenderContext& ctx) {
	ctx.defaultShader->use();
	{
		int idx = 0;
		for (auto& obj : ctx.pointLights) {
			RenderHelpers::uploadPointLight(obj, idx++, *ctx.ssboPointLights);
		}
	}
	{
		int idx = 0;
		for (auto& obj : ctx.spotLights) {
			RenderHelpers::uploadSpotLight(obj, idx++, *ctx.ssboSpotLights);
		}
	}
	for (auto& obj : ctx.directionLights) {
		RenderHelpers::uploadDirectionLight(obj, *ctx.ssboDirectionLight);
		auto shadowCaster = obj->getComponent<ShadowCaster2D>();
		if (shadowCaster && shadowCaster->enabled) {
			ctx.defaultShader->setMat4("lightSpaceMatrix", RenderHelpers::directionLightMatrices(obj));
		}
	}
	ctx.defaultShader->setInt("pointLightNum", (int)ctx.pointLights.size());
	ctx.defaultShader->setInt("directionLightNum", (int)ctx.directionLights.size());
	ctx.defaultShader->setInt("spotLightNum", (int)ctx.spotLights.size());
	ctx.directionLightDepthTexture->use(GL_TEXTURE6);
	ctx.pointLightDepthTexture->use(GL_TEXTURE7);
}

void ForwardHdrPass::execute(RenderContext& ctx) {
	glViewport(0, 0, (GLsizei)ctx.width, (GLsizei)ctx.height);

	ctx.hdrFBO->bind();
	// Unbind program before clear (see FrameSetupPass): avoids the driver
	// validating the previous active program's samplers against unbound units.
	glUseProgram(0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	ctx.defaultShader->use();
	ctx.defaultShader->setVec3("cameraPos", ctx.camera->getPos());
	for (auto& obj : ctx.skyboxes) {
		RenderHelpers::bindSkyboxCubeMap(obj, ctx.defaultShader);
	}
	drawAllRenderablesInFrustum(ctx, ctx.defaultShader);

	ctx.lightCubeShader->use();
	for (auto& obj : ctx.pointLights) {
		if (obj->has<StaticMeshComponent>() && RenderHelpers::isPointLightInFrustum(obj, ctx.frustum)) {
			RenderHelpers::drawLightCube(obj, ctx.lightCubeShader);
		}
	}

	ctx.skyboxShader->use();
	for (auto& obj : ctx.skyboxes) {
		RenderHelpers::drawSkybox(obj, ctx.skyboxShader);
	}
	ctx.hdrFBO->unbind();
}

void VolumeSkeletonPass::execute(RenderContext& ctx) {
	// Lazy volume-cloud noise: only kick off background generation when a volume
	// exists; skip volume drawing until the noise is ready.
	ctx.owner->ensureVolumeNoise(!ctx.volumes.empty());
	bool drawClouds = *ctx.noiseReady && !ctx.volumes.empty();

	// --- Pass 1: raymarch clouds into the half-res cloud target. -------------
	// Clouds are low frequency, so the expensive march runs at half resolution
	// (~4x fewer fragments). volume.frag derives its uv from gl_FragCoord/resolution,
	// so it just needs the half-res viewport + resolution uniform; depthMap is the
	// full-res scene depth, sampled in normalized uv, so depth-aware termination
	// still works.
	if (drawClouds) {
		glViewport(0, 0, (GLsizei)ctx.cloudWidth, (GLsizei)ctx.cloudHeight);
		ctx.cloudFBO->bind();
		glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
		glUseProgram(0);
		glClear(GL_COLOR_BUFFER_BIT);

		ctx.volumeShader->use();
		ctx.volumeShader->setVec3("cameraPos", ctx.camera->getPos());
		ctx.volumeShader->setInt("depthMap", 0);
		ctx.volumeShader->setInt("worleyNoiseTexture", 1);
		ctx.volumeShader->setInt("weatherMap", 2);
		ctx.volumeShader->setInt("perlinNoiseTexture", 3);
		ctx.volumeShader->setVec2("resolution", glm::vec2(ctx.cloudWidth, ctx.cloudHeight));
		ctx.volumeShader->setFloat("time", (float)glfwGetTime());
		glm::mat4 invVP = glm::inverse(ctx.camera->getProjectionMat(ctx.width, ctx.height) * ctx.camera->getViewMat());
		ctx.volumeShader->setMat4("invVP", invVP);

		bool foundDirLight = false;
		for (auto& obj : ctx.directionLights) {
			auto transform = obj->getComponent<Transform>();
			auto dirLight = obj->getComponent<DirectionLightComponent>();
			glm::vec3 direction = RenderHelpers::directionFromTransform(transform);
			ctx.volumeShader->setVec3("lightDir", -direction);
			ctx.volumeShader->setVec3("lightColor", dirLight->color * dirLight->brightness);
			foundDirLight = true;
			break;
		}
		if (!foundDirLight) {
			ctx.volumeShader->setVec3("lightDir", glm::vec3(0.0, 1.0, 0.0));
			ctx.volumeShader->setVec3("lightColor", glm::vec3(1.0));
		}
		ctx.hdrDepthTexture->use(GL_TEXTURE0);
		ctx.worleyNoiseTexture3D->use(GL_TEXTURE1);
		ctx.weatherMapTexture->use(GL_TEXTURE2);
		ctx.perlinNoiseTexture3D->use(GL_TEXTURE3);
		for (auto& obj : ctx.volumes) {
			auto staticMesh = obj->getComponent<StaticMeshComponent>();
			auto transform = obj->getComponent<Transform>();
			if (staticMesh && transform) {
				glm::mat4 model = glm::mat4(1.0f);
				model = glm::translate(model, transform->translate);
				model = glm::scale(model, transform->scale);
				glm::vec3 minAABB = glm::vec3(model * glm::vec4(staticMesh->aabb.min, 1.0f));
				glm::vec3 maxAABB = glm::vec3(model * glm::vec4(staticMesh->aabb.max, 1.0f));
				ctx.volumeShader->setVec3("aabbMin", minAABB);
				ctx.volumeShader->setVec3("aabbMax", maxAABB);

				// Per-instance cloud parameters. Fall back to defaults (= the previous
				// hardcoded look) if the marker is somehow absent.
				auto marker = obj->getComponent<VolumeMarkerComponent>();
				VolumeMarkerComponent fallback;
				const VolumeMarkerComponent& v = marker ? *marker : fallback;
				ctx.volumeShader->setFloat("cloudType", v.cloudType);
				ctx.volumeShader->setFloat("coverageLow", v.coverageLow);
				ctx.volumeShader->setFloat("coverageHigh", v.coverageHigh);
				ctx.volumeShader->setFloat("baseNoiseScale", v.baseNoiseScale);
				ctx.volumeShader->setFloat("warpScale", v.warpScale);
				ctx.volumeShader->setFloat("warpStrength", v.warpStrength);
				ctx.volumeShader->setFloat("detailScale", v.detailScale);
				ctx.volumeShader->setFloat("detailErosionStrength", v.detailErosionStrength);
				ctx.volumeShader->setFloat("lightAttenuation", v.lightAttenuation);
				ctx.volumeShader->setFloat("densityGain", v.densityGain);
				ctx.volumeShader->setFloat("viewAttenuation", v.viewAttenuation);
				ctx.volumeShader->setFloat("powderStrength", v.powderStrength);
				ctx.volumeShader->setVec3("windVelocity", v.windVelocity);
				ctx.volumeShader->setFloat("edgeFadeDistance", v.edgeFadeDistance);
				ctx.volumeShader->setFloat("viewStepWorldSize", v.viewStepWorldSize);

				RenderHelpers::drawScreenQuad();
			}
		}
		ctx.cloudFBO->unbind();
	}

	// --- Pass 2: upsample clouds (+ skeleton overlay) into afterEffectFBO. ----
	glViewport(0, 0, (GLsizei)ctx.width, (GLsizei)ctx.height);
	ctx.afterEffectFBO->bind();
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	// Unbind program before clear (see FrameSetupPass): avoids the driver validating
	// a leftover active program's samplers against unbound units.
	glUseProgram(0);
	glClear(GL_COLOR_BUFFER_BIT);

	// Depth-aware bilateral upsample from the half-res cloud target. Skipped when no
	// clouds were drawn, leaving afterEffect cleared to (0,0,0,0) (a no-op in the
	// composite, so the scene shows through).
	if (drawClouds) {
		ctx.cloudUpsampleShader->use();
		ctx.cloudUpsampleShader->setInt("cloudTexture", 0);
		ctx.cloudUpsampleShader->setInt("depthTexture", 1);
		ctx.cloudUpsampleShader->setVec2("cloudTexSize", glm::vec2(ctx.cloudWidth, ctx.cloudHeight));
		// Must match Camera::getProjectionMat (near 0.1, far 100.0).
		ctx.cloudUpsampleShader->setFloat("nearPlane", 0.1f);
		ctx.cloudUpsampleShader->setFloat("farPlane", 100.0f);
		ctx.cloudTexture->use(GL_TEXTURE0);
		ctx.hdrDepthTexture->use(GL_TEXTURE1);
		RenderHelpers::drawScreenQuad();
	}

	ctx.boneShader->use();
	for (auto& obj : ctx.skeletons) {
		if (RenderHelpers::isRenderableInFrustum(obj, ctx.frustum)) RenderHelpers::drawSkeleton(obj, ctx.boneShader);
	}

	ctx.afterEffectFBO->unbind();
}

void GizmoPass::execute(RenderContext& ctx) {
	if (!ctx.selectedObject) return;
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

	// Draw into the shared after-effect target (no depth attachment) so the gizmo is
	// always on top. Do NOT clear: it overlays whatever VolumeSkeletonPass left there.
	glViewport(0, 0, (GLsizei)ctx.width, (GLsizei)ctx.height);
	ctx.afterEffectFBO->bind();
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

	ctx.afterEffectFBO->unbind();
	// Restore the global baseline the rest of the pipeline expects (cull stays disabled).
	glEnable(GL_DEPTH_TEST);
}

void BloomPass::execute(RenderContext& ctx) {
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	ctx.gaussianBlurShader->use();
	for (int i = 0; i < 10; i++) {
		ctx.pingpongFBO[i % 2]->bind();
		ctx.gaussianBlurShader->setBool("horizontal", i % 2 == 0);
		// Bind the sampled texture before clearing so the current program's sampler
		// unit is already complete at clear time.
		if (i == 0) ctx.brightTexture->use(GL_TEXTURE0);
		else ctx.pingpongTexture[(i + 1) % 2]->use(GL_TEXTURE0);
		glClear(GL_COLOR_BUFFER_BIT);
		RenderHelpers::drawScreenQuad();
		ctx.pingpongFBO[i % 2]->unbind();
	}
}

void CompositePass::execute(RenderContext& ctx) {
	// Composite into the offscreen scene target (the Viewport panel samples it),
	// not the backbuffer. Color-only FBO; disable depth test so the fullscreen
	// quad (z=0) is never rejected by the global GL_LEQUAL.
	ctx.sceneFBO->bind();
	glViewport(0, 0, (GLsizei)ctx.width, (GLsizei)ctx.height);
	glDisable(GL_DEPTH_TEST);
	ctx.screenQuadShader->use();
	ctx.hdrTexture->use(GL_TEXTURE0);
	ctx.pingpongTexture[1]->use(GL_TEXTURE1);
	ctx.afterEffectTexture->use(GL_TEXTURE2);
	RenderHelpers::drawScreenQuad();
	glEnable(GL_DEPTH_TEST);
	ctx.sceneFBO->unbind();
}
