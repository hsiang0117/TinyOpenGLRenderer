#ifndef SCENEFACTORY_HPP
#define SCENEFACTORY_HPP
#pragma once

#include <memory>

class Scene;

// Builds preconfigured Scenes (mirrors GameObjectFactory for GameObjects).
class SceneFactory {
public:
	static std::shared_ptr<Scene> createDefaultScene();
};

#endif // !SCENEFACTORY_HPP
