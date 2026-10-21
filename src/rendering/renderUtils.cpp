#include "renderUtils.hpp"
#include "camera.hpp"
#include "scene/component.hpp"
#include "rendering/opengl/glBuffer.hpp"
#include "rendering/geometry/mesh.hpp"
#include "assets/model.hpp"
#include "skybox.hpp"
#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <string>
#include <vector>
#include <cmath>

namespace RenderHelpers {

namespace {
	GLuint quadVAO = 0, quadVBO = 0;
	GLuint gizmoVAO = 0, gizmoVBO = 0;
	GLsizei gizmoVertexCount = 0;
}

void shutdownSharedGeometry() {
	if (quadVAO) glDeleteVertexArrays(1, &quadVAO);
	if (quadVBO) glDeleteBuffers(1, &quadVBO);
	if (gizmoVAO) glDeleteVertexArrays(1, &gizmoVAO);
	if (gizmoVBO) glDeleteBuffers(1, &gizmoVBO);
	quadVAO = quadVBO = gizmoVAO = gizmoVBO = 0;
	gizmoVertexCount = 0;
}

glm::mat4 buildModelMatrix(const std::shared_ptr<Transform>& transform) {
	glm::mat4 model = glm::mat4(1.0f);
	if (!transform) return model;
	model = glm::translate(model, transform->translate);
	model = glm::rotate(model, glm::radians(transform->rotate.z), glm::vec3(0, 0, 1));
	model = glm::rotate(model, glm::radians(transform->rotate.y), glm::vec3(0, 1, 0));
	model = glm::rotate(model, glm::radians(transform->rotate.x), glm::vec3(1, 0, 0));
	model = glm::scale(model, transform->scale);
	return model;
}

glm::vec3 directionFromTransform(const std::shared_ptr<Transform>& transform) {
	glm::mat4 rotation(1.0);
	rotation = glm::rotate(rotation, glm::radians(transform->rotate.z), glm::vec3(0, 0, 1));
	rotation = glm::rotate(rotation, glm::radians(transform->rotate.y), glm::vec3(0, 1, 0));
	rotation = glm::rotate(rotation, glm::radians(transform->rotate.x), glm::vec3(1, 0, 0));
	glm::vec3 direction(1.0, 0.0, 0.0);
	return glm::mat3(rotation) * direction;
}

bool aabbInFrustum(const glm::vec3& minAABB, const glm::vec3& maxAABB, const glm::mat4& model, Frustum& frustum) {
	glm::vec3 vertices[8] = {
		glm::vec3(minAABB.x, minAABB.y, minAABB.z),
		glm::vec3(maxAABB.x, minAABB.y, minAABB.z),
		glm::vec3(minAABB.x, maxAABB.y, minAABB.z),
		glm::vec3(maxAABB.x, maxAABB.y, minAABB.z),
		glm::vec3(minAABB.x, minAABB.y, maxAABB.z),
		glm::vec3(maxAABB.x, minAABB.y, maxAABB.z),
		glm::vec3(minAABB.x, maxAABB.y, maxAABB.z),
		glm::vec3(maxAABB.x, maxAABB.y, maxAABB.z)
	};
	for (int i = 0; i < 8; ++i) {
		glm::vec4 v = model * glm::vec4(vertices[i], 1.0f);
		vertices[i] = glm::vec3(v);
	}
	glm::vec4 planes[6] = {
		frustum.leftPlane, frustum.rightPlane,
		frustum.bottomPlane, frustum.topPlane,
		frustum.nearPlane, frustum.farPlane
	};
	for (int p = 0; p < 6; ++p) {
		int out = 0;
		for (int i = 0; i < 8; ++i) {
			const glm::vec4& plane = planes[p];
			if (plane.x * vertices[i].x + plane.y * vertices[i].y + plane.z * vertices[i].z + plane.w < 0)
				out++;
		}
		if (out == 8) return false;
	}
	return true;
}

bool isRenderable(const GameObjectPtr& obj) {
	if (!obj) return false;
	if (obj->has<ModelRenderComponent>()) return true;
	if (obj->has<StaticMeshComponent>() && obj->has<DynamicMaterialComponent>() && !obj->has<CloudVolumeComponent>())
		return true;
	return false;
}

bool isRenderableInFrustum(const GameObjectPtr& obj, Frustum& frustum) {
	if (!obj) return false;
	auto transform = obj->getComponent<Transform>();
	if (!transform) return false;
	glm::mat4 model = buildModelMatrix(transform);
	if (auto rc = obj->getComponent<ModelRenderComponent>()) {
		return aabbInFrustum(rc->aabb.min, rc->aabb.max, model, frustum);
	}
	if (auto sm = obj->getComponent<StaticMeshComponent>()) {
		return aabbInFrustum(sm->aabb.min, sm->aabb.max, model, frustum);
	}
	return false;
}

bool isPointLightInFrustum(const GameObjectPtr& obj, Frustum& frustum) {
	if (!obj) return false;
	auto transform = obj->getComponent<Transform>();
	auto sm = obj->getComponent<StaticMeshComponent>();
	if (!transform || !sm) return false;
	glm::mat4 model = buildModelMatrix(transform);
	return aabbInFrustum(sm->aabb.min, sm->aabb.max, model, frustum);
}

void drawRenderable(const GameObjectPtr& obj, ShaderPtr shader) {
	auto transform = obj->getComponent<Transform>();
	glm::mat4 model = buildModelMatrix(transform);
	shader->setMat4("model", model);

	if (auto modelRenderComponent = obj->getComponent<ModelRenderComponent>()) {
		if (auto animator = obj->getComponent<AnimatorComponent>()) {
			auto& boneMatrixTexture = animator->getBoneMatrixTexture();
			boneMatrixTexture.use(GL_TEXTURE8);
		}
		if (modelRenderComponent->model) {
			modelRenderComponent->model->draw(shader);
		}
		return;
	}

	if (auto staticMeshComponent = obj->getComponent<StaticMeshComponent>()) {
		if (auto dynamicMaterialComponent = obj->getComponent<DynamicMaterialComponent>()) {
			dynamicMaterialComponent->material.bind(shader);
		}
		// unit 8 (bone matrix texture) is globally bound once per frame by the
		// frame-setup pass; static meshes need no per-draw handling here.
		if (staticMeshComponent->mesh) {
			staticMeshComponent->mesh->draw();
		}
	}
}

void drawLightMarker(const GameObjectPtr& obj, ShaderPtr shader) {
	auto staticMeshComponent = obj->getComponent<StaticMeshComponent>();
	if (!staticMeshComponent) return;
	auto transform = obj->getComponent<Transform>();
	glm::mat4 model = buildModelMatrix(transform);
	shader->setMat4("model", model);
	glm::vec3 color = glm::vec3(1.0f);
	float brightness = 1.0f;
	if (auto pointLight = obj->getComponent<PointLightComponent>()) {
		color = pointLight->color;
		brightness = pointLight->brightness;
	}
	shader->setVec3("color", color);
	shader->setFloat("brightness", brightness);
	if (staticMeshComponent->mesh) {
		staticMeshComponent->mesh->draw();
	}
}

void drawSkybox(const GameObjectPtr& obj, ShaderPtr shader) {
	shader->setInt("skybox", 5);
	if (auto skyboxComponent = obj->getComponent<SkyboxComponent>()) {
		if (skyboxComponent->skybox) skyboxComponent->skybox->draw();
	}
}

void bindSkyboxCubeMap(const GameObjectPtr& obj, ShaderPtr shader) {
	shader->setInt("skybox", 5);
	if (auto skyboxComponent = obj->getComponent<SkyboxComponent>()) {
		if (skyboxComponent->skybox) skyboxComponent->skybox->useCubeMap();
	}
}

void drawSkeleton(const GameObjectPtr& obj, ShaderPtr shader) {
	auto transform = obj->getComponent<Transform>();
	glm::mat4 model = buildModelMatrix(transform);
	shader->setMat4("model", model);
	if (auto skeletonViewer = obj->getComponent<SkeletonViewerComponent>()) {
		if (skeletonViewer->show) skeletonViewer->drawSkeleton();
	}
}

void uploadPointLight(const GameObjectPtr& obj, int index, ShaderStorageBuffer& ssbo) {
	auto transform = obj->getComponent<Transform>();
	auto pointLight = obj->getComponent<PointLightComponent>();
	auto shadowCaster = obj->getComponent<ShadowCasterCube>();
	const int sz = PointLightComponent::glslSize;
	const float farPlane = shadowCaster ? shadowCaster->farPlane : 25.0f;
	// position.w carries the shadow switch without changing the 48-byte layout.
	const float shadowEnabled = shadowCaster && shadowCaster->enabled && farPlane > 0.1f ? 1.0f : 0.0f;
	ssbo.bind();
	ssbo.bufferSubdata(index * sz, 12, glm::value_ptr(transform->translate));
	ssbo.bufferSubdata(index * sz + 12, 4, &shadowEnabled);
	ssbo.bufferSubdata(index * sz + 16, 12, glm::value_ptr(pointLight->color));
	ssbo.bufferSubdata(index * sz + 28, 4, &pointLight->brightness);
	ssbo.bufferSubdata(index * sz + 32, 4, &pointLight->constant);
	ssbo.bufferSubdata(index * sz + 36, 4, &pointLight->linear);
	ssbo.bufferSubdata(index * sz + 40, 4, &pointLight->quadratic);
	ssbo.bufferSubdata(index * sz + 44, 4, &farPlane);
	ssbo.unbind();
}

void uploadDirectionalLight(const GameObjectPtr& obj, ShaderStorageBuffer& ssbo) {
	auto transform = obj->getComponent<Transform>();
	auto directionalLight = obj->getComponent<DirectionalLightComponent>();
	glm::vec3 direction = directionFromTransform(transform);
	const float padding = 0.0f;
	ssbo.bind();
	ssbo.bufferSubdata(0, 12, glm::value_ptr(direction));
	ssbo.bufferSubdata(12, 4, &padding);
	ssbo.bufferSubdata(16, 12, glm::value_ptr(directionalLight->color));
	ssbo.bufferSubdata(28, 4, &directionalLight->brightness);
	ssbo.unbind();
}

void uploadSpotLight(const GameObjectPtr& obj, int index, ShaderStorageBuffer& ssbo) {
	auto transform = obj->getComponent<Transform>();
	auto spotLight = obj->getComponent<SpotLightComponent>();
	glm::vec3 direction = directionFromTransform(transform);
	float cutOff = glm::cos(glm::radians(spotLight->cutOff));
	float outerCutOff = glm::cos(glm::radians(spotLight->outerCutOff));
	const int sz = SpotLightComponent::glslSize;
	const float padding[2] = { 0.0f, 0.0f };
	ssbo.bind();
	ssbo.bufferSubdata(index * sz, 12, glm::value_ptr(transform->translate));
	ssbo.bufferSubdata(index * sz + 12, 4, padding);
	ssbo.bufferSubdata(index * sz + 16, 12, glm::value_ptr(direction));
	ssbo.bufferSubdata(index * sz + 28, 4, padding);
	ssbo.bufferSubdata(index * sz + 32, 12, glm::value_ptr(spotLight->color));
	ssbo.bufferSubdata(index * sz + 44, 4, &spotLight->brightness);
	ssbo.bufferSubdata(index * sz + 48, 4, &cutOff);
	ssbo.bufferSubdata(index * sz + 52, 4, &outerCutOff);
	ssbo.bufferSubdata(index * sz + 56, 8, padding);
	ssbo.unbind();
}

glm::mat4 directionalShadowMatrix(const GameObjectPtr& obj) {
	auto transform = obj->getComponent<Transform>();
	glm::vec3 direction = directionFromTransform(transform);
	glm::mat4 lightProjection = glm::ortho(-100.0f, 100.0f, -100.0f, 100.0f, -100.0f, 100.0f);
	glm::mat4 lightView = glm::lookAt(-direction, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
	return lightProjection * lightView;
}

std::vector<glm::mat4> pointShadowMatrices(const GameObjectPtr& obj) {
	auto transform = obj->getComponent<Transform>();
	glm::vec3 position = transform->translate;
	auto shadowCaster = obj->getComponent<ShadowCasterCube>();
	glm::mat4 lightProjection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, shadowCaster->farPlane);
	std::vector<glm::mat4> lightViews;
	lightViews.push_back(lightProjection * glm::lookAt(position, position + glm::vec3(1.0, 0.0, 0.0), glm::vec3(0.0, -1.0, 0.0)));
	lightViews.push_back(lightProjection * glm::lookAt(position, position + glm::vec3(-1.0, 0.0, 0.0), glm::vec3(0.0, -1.0, 0.0)));
	lightViews.push_back(lightProjection * glm::lookAt(position, position + glm::vec3(0.0, 1.0, 0.0), glm::vec3(0.0, 0.0, 1.0)));
	lightViews.push_back(lightProjection * glm::lookAt(position, position + glm::vec3(0.0, -1.0, 0.0), glm::vec3(0.0, 0.0, -1.0)));
	lightViews.push_back(lightProjection * glm::lookAt(position, position + glm::vec3(0.0, 0.0, 1.0), glm::vec3(0.0, -1.0, 0.0)));
	lightViews.push_back(lightProjection * glm::lookAt(position, position + glm::vec3(0.0, 0.0, -1.0), glm::vec3(0.0, -1.0, 0.0)));
	return lightViews;
}

void drawFullscreenQuad() {

	if (!quadVAO) {
		float quadVertices[] = {
			-1.0f,  1.0f, 0.0f, 1.0f,
			-1.0f, -1.0f, 0.0f, 0.0f,
			 1.0f,  1.0f, 1.0f, 1.0f,
			 1.0f, -1.0f, 1.0f, 0.0f,
		};
		glGenVertexArrays(1, &quadVAO);
		glGenBuffers(1, &quadVBO);
		glBindVertexArray(quadVAO);
		glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
		glEnableVertexAttribArray(1);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);
	}
	glBindVertexArray(quadVAO);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	glBindVertexArray(0);
}

void drawSelectionGizmo() {

	if (!gizmoVAO) {
		const int SEG = 12;
		const float shaftLen = 0.75f;
		const float shaftRadius = 0.02f;
		const float headRadius = 0.06f;
		const float tipX = 1.0f;
		const float PI = 3.14159265358979323846f;

		// Interleaved pos(3) + color(3). One canonical arrow points along +X; it is
		// rotated to +X/+Y/+Z and tinted R/G/B, all baked here so the whole gizmo is a
		// single static buffer drawn in one call. Triangles are wound outward (CCW),
		// though GizmoPass draws with culling disabled so winding does not matter.
		std::vector<float> verts;

		glm::mat3 axisRot[3] = {
			glm::mat3(1.0f),                                                                  // +X
			glm::mat3(glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0, 0, 1))), // +X -> +Y
			glm::mat3(glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(0, 1, 0))) // +X -> +Z
		};
		glm::vec3 axisColor[3] = { glm::vec3(1, 0, 0), glm::vec3(0, 1, 0), glm::vec3(0, 0, 1) };

		for (int a = 0; a < 3; a++) {
			glm::mat3 R = axisRot[a];
			glm::vec3 c = axisColor[a];
			auto push = [&](glm::vec3 p) {
				glm::vec3 wp = R * p;
				verts.push_back(wp.x); verts.push_back(wp.y); verts.push_back(wp.z);
				verts.push_back(c.r); verts.push_back(c.g); verts.push_back(c.b);
			};

			for (int i = 0; i < SEG; i++) {
				float t0 = (float)i / SEG * 2.0f * PI;
				float t1 = (float)(i + 1) / SEG * 2.0f * PI;
				glm::vec2 d0 = glm::vec2(std::cos(t0), std::sin(t0));
				glm::vec2 d1 = glm::vec2(std::cos(t1), std::sin(t1));
				glm::vec2 s0 = d0 * shaftRadius, s1 = d1 * shaftRadius;
				glm::vec2 h0 = d0 * headRadius, h1 = d1 * headRadius;

				// Shaft (cylinder) side, outward CCW.
				glm::vec3 b0(0.0f, s0.x, s0.y), b1(0.0f, s1.x, s1.y);
				glm::vec3 tp0(shaftLen, s0.x, s0.y), tp1(shaftLen, s1.x, s1.y);
				push(b0); push(b1); push(tp0);
				push(b1); push(tp1); push(tp0);

				// Cone head side, outward CCW.
				glm::vec3 hb0(shaftLen, h0.x, h0.y), hb1(shaftLen, h1.x, h1.y);
				glm::vec3 tip(tipX, 0.0f, 0.0f);
				push(hb0); push(hb1); push(tip);

				// Cone base cap (faces -X), outward CCW.
				glm::vec3 baseCenter(shaftLen, 0.0f, 0.0f);
				push(baseCenter); push(hb1); push(hb0);
			}
		}

		gizmoVertexCount = (GLsizei)(verts.size() / 6);

		glGenVertexArrays(1, &gizmoVAO);
		glGenBuffers(1, &gizmoVBO);
		glBindVertexArray(gizmoVAO);
		glBindBuffer(GL_ARRAY_BUFFER, gizmoVBO);
		glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_STATIC_DRAW);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
		glEnableVertexAttribArray(1);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);
	}

	glBindVertexArray(gizmoVAO);
	glDrawArrays(GL_TRIANGLES, 0, gizmoVertexCount);
	glBindVertexArray(0);
}

} // namespace RenderHelpers
