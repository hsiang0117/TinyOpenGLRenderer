#ifndef TOGR_SCENEFACTORY_HPP
#define TOGR_SCENEFACTORY_HPP
#pragma once

#include <memory>

namespace togr {

class Scene;

// Builds preconfigured Scenes (mirrors GameObjectFactory for GameObjects).
class SceneFactory {
public:
	static std::shared_ptr<Scene> createDefaultScene();
};

} // namespace togr

#endif // !TOGR_SCENEFACTORY_HPP
