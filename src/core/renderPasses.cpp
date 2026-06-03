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
	glViewport(0, 0, ctx.width, ctx.height);

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
	glViewport(0, 0, ctx.width, ctx.height);
	ctx.afterEffectFBO->bind();
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	// Same as above: unbind program before clear to avoid the volume shader's
	// depthMap unit being flagged as incomplete.
	glUseProgram(0);
	glClear(GL_COLOR_BUFFER_BIT);

	// Lazy volume-cloud noise: only kick off background generation when a volume
	// exists; skip volume drawing until the noise is ready.
	ctx.owner->ensureVolumeNoise(!ctx.volumes.empty());
	if (*ctx.noiseReady && !ctx.volumes.empty()) {
		ctx.volumeShader->use();
		ctx.volumeShader->setVec3("cameraPos", ctx.camera->getPos());
		ctx.volumeShader->setInt("depthMap", 0);
		ctx.volumeShader->setInt("worleyNoiseTexture", 1);
		ctx.volumeShader->setInt("weatherMap", 2);
		ctx.volumeShader->setInt("perlinNoiseTexture", 3);
		ctx.volumeShader->setVec2("resolution", glm::vec2(ctx.width, ctx.height));
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
				RenderHelpers::drawScreenQuad();
			}
		}
	}

	ctx.boneShader->use();
	for (auto& obj : ctx.skeletons) {
		if (RenderHelpers::isRenderableInFrustum(obj, ctx.frustum)) RenderHelpers::drawSkeleton(obj, ctx.boneShader);
	}

	ctx.afterEffectFBO->unbind();
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
	glViewport(ctx.x, ctx.y, ctx.width, ctx.height);
	ctx.screenQuadShader->use();
	ctx.hdrTexture->use(GL_TEXTURE0);
	ctx.pingpongTexture[1]->use(GL_TEXTURE1);
	ctx.afterEffectTexture->use(GL_TEXTURE2);
	RenderHelpers::drawScreenQuad();
}
