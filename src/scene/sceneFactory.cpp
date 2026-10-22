#include "sceneFactory.hpp"
#include "scene.hpp"
#include "gameObjectFactory.hpp"
#include "component.hpp"
#include <glm/glm.hpp>
#include <stdexcept>

namespace togr {

ScenePtr SceneFactory::createDefaultScene() {
	auto scene = std::make_shared<Scene>();

	// Skybox from shipped cubemap (data/textures/skybox/{right,left,top,bottom,front,back}.jpg)
	auto sky = GameObjectFactory::createSkybox();
	auto skybox = sky->getComponent<SkyboxComponent>();
	if (!skybox->setSkybox("data/textures/skybox"))
		throw std::runtime_error("Required startup skybox failed: " + skybox->lastError());
	scene->addObject(sky);

	// Ground plane: dropped below eye level (camera starts at y=0) and scaled wide.
	auto plane = GameObjectFactory::createPlane();
	{
		auto t = plane->getComponent<Transform>();
		t->translate = glm::vec3(0.0f, -1.5f, 0.0f);
		t->scale = glm::vec3(20.0f, 1.0f, 20.0f);
	}
	scene->addObject(plane);

	// One directional light so the plane is lit. directionFromTransform starts from
	// (1,0,0) and applies Z->Y->X euler; rotate.z=-45 tilts the light toward the ground.
	auto directionalLight = GameObjectFactory::createDirectionalLight();
	{
		auto t = directionalLight->getComponent<Transform>();
		t->rotate = glm::vec3(0.0f, 0.0f, -45.0f);
		auto dl = directionalLight->getComponent<DirectionalLightComponent>();
		dl->color = glm::vec3(1.0f);
		dl->brightness = 1.0f;
	}
	scene->addObject(directionalLight);

	return scene;
}

} // namespace togr
