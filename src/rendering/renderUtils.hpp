#ifndef TOGR_RENDERUTILS_HPP
#define TOGR_RENDERUTILS_HPP
#pragma once

#include "scene/gameObject.hpp"
#include "rendering/opengl/shader.hpp"
#include <glm/glm.hpp>
#include <memory>
#include <vector>

namespace togr {

class Transform;
class ShaderStorageBuffer;
struct Frustum;

// Stateless render helpers shared across passes. Extracted verbatim from
// RenderSystem's former static members; every helper takes all state via
// parameters, so they carry no RenderSystem instance dependency.
namespace RenderHelpers {
	glm::mat4 buildModelMatrix(const std::shared_ptr<Transform>& transform);
	glm::vec3 directionFromTransform(const std::shared_ptr<Transform>& transform);
	bool aabbInFrustum(const glm::vec3& minAABB, const glm::vec3& maxAABB, const glm::mat4& model, Frustum& frustum);
	bool isRenderableInFrustum(const GameObjectPtr& obj, Frustum& frustum);
	bool isPointLightInFrustum(const GameObjectPtr& obj, Frustum& frustum);

	void drawRenderable(const GameObjectPtr& obj, ShaderPtr shader);
	void drawLightMarker(const GameObjectPtr& obj, ShaderPtr shader);
	void drawSkybox(const GameObjectPtr& obj, ShaderPtr shader);
	void bindSkyboxCubeMap(const GameObjectPtr& obj, ShaderPtr shader);
	void drawSkeleton(const GameObjectPtr& obj, ShaderPtr shader);

	void uploadPointLight(const GameObjectPtr& obj, int index, ShaderStorageBuffer& ssbo);
	void uploadDirectionalLight(const GameObjectPtr& obj, ShaderStorageBuffer& ssbo);
	void uploadSpotLight(const GameObjectPtr& obj, int index, ShaderStorageBuffer& ssbo);

	glm::mat4 directionalShadowMatrix(const GameObjectPtr& obj);
	std::vector<glm::mat4> pointShadowMatrices(const GameObjectPtr& obj);

	// Full-screen quad for post-processing; lazily creates a process-wide VAO.
	void drawFullscreenQuad();

	// Selection gizmo: three cone-tipped XYZ arrows (X red, Y green, Z blue) baked
	// into a lazily-created process-wide VAO. Drawn with GL_TRIANGLES. The caller
	// supplies the model matrix via the shader; this just binds + draws.
	void drawSelectionGizmo();
	void shutdownSharedGeometry();
}

} // namespace togr

#endif // !TOGR_RENDERUTILS_HPP
