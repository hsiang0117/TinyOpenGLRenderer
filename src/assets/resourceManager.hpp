#ifndef RESOURCEMANAGER_HPP
#define RESOURCEMANAGER_HPP
#pragma once

#include "modelLoader.hpp"
#include "shaderLoader.hpp"
#include "scene/scene.hpp"
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

class Model;
class Shader;

// Coordinates the asset loaders and the active scene.
class ResourceManager {
public:
	ResourceManager();
	ResourceManager(const ResourceManager&) = delete;
	ResourceManager& operator=(const ResourceManager&) = delete;

	// Call lifecycle methods while the GL context is current.
	void init();
	void update();
	void shutdown();

	void queueModelLoad(const std::string& path);
	std::vector<std::shared_ptr<Model>> getAllModels() const;
	std::shared_ptr<Shader> getShader(const std::string& key) const;

	bool addGameObject(GameObjectPtr obj);
	void removeGameObject(std::size_t idx);
	std::vector<GameObjectPtr> getGameObjects() const;
	int getPointLightCount() const;
	int getDirectionalLightCount() const;
	int getSpotLightCount() const;

	template<typename T>
	const std::vector<GameObjectPtr>& getSceneView() const {
		return currentScene->getView<T>();
	}

	ScenePtr getCurrentScene() const;
	void setCurrentScene(ScenePtr s);

private:
	ModelLoader modelLoader;
	ShaderLoader shaderLoader;
	ScenePtr currentScene;
};

#endif // !RESOURCEMANAGER_HPP
