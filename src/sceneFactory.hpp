#ifndef SCENEFACTORY_HPP
#define SCENEFACTORY_HPP
#pragma once

#include "core/resourceManager.hpp"

// Builds preconfigured Scenes (mirrors GameObjectFactory for GameObjects).
class SceneFactory {
public:
	static ScenePtr createDefaultScene();
};

#endif // !SCENEFACTORY_HPP
