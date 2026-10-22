#ifndef TOGR_GAMEOBJECTFACTORY_HPP
#define TOGR_GAMEOBJECTFACTORY_HPP
#pragma once

#include "gameObject.hpp"
#include <memory>

namespace togr {

class Model;

class GameObjectFactory {
public:
	static GameObjectPtr createPointLight();
	static GameObjectPtr createDirectionalLight();
	static GameObjectPtr createSpotLight();
	static GameObjectPtr createCube();
	static GameObjectPtr createPlane();
	static GameObjectPtr createSphere();
	static GameObjectPtr createSkybox();
	static GameObjectPtr createCloudVolume();
	static GameObjectPtr createFromModel(std::shared_ptr<Model> model);
};

} // namespace togr

#endif // !TOGR_GAMEOBJECTFACTORY_HPP
