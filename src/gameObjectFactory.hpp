#ifndef GAMEOBJECTFACTORY_HPP
#define GAMEOBJECTFACTORY_HPP
#pragma once

#include "gameObject.hpp"
#include "model.hpp"

class GameObjectFactory {
public:
	static GameObjectPtr createPointLight();
	static GameObjectPtr createDirectionLight();
	static GameObjectPtr createSpotLight();
	static GameObjectPtr createCube();
	static GameObjectPtr createPlane();
	static GameObjectPtr createSphere();
	static GameObjectPtr createSkyBox();
	static GameObjectPtr createVolumeCloud();
	static GameObjectPtr createFromModel(ModelPtr model);
};

#endif // !GAMEOBJECTFACTORY_HPP
