#ifndef RESOURCEMANAGER_HPP
#define RESOURCEMANAGER_HPP
#pragma once

#include "../shader.hpp"
#include "../model.hpp"
#include "../gameObject.hpp"
#include <iostream>
#include <queue>
#include <unordered_map>

class ResourceManager {
public:
	ResourceManager() {};
	static ResourceManager& getInstance();
	void init();
	void update();
	std::future<Model> modelFuture; //future for loading models
	std::queue<std::string> modelQueue; //queue for loading models
	std::unordered_map<std::string, ModelPtr> modelCache; //cache for models
	std::unordered_map<std::string, ShaderPtr> shaderCache; //cache for shaders
	std::vector<GameObjectPtr> gameObjects;
private:
};

ResourceManager& ResourceManager::getInstance() {
	static ResourceManager instance;
	return instance;
}

void ResourceManager::init() {
	shaderCache["default"] = std::make_shared<Shader>("data/shader/test.vert", "data/shader/test.frag");
}

void ResourceManager::update() {
	if (modelFuture.valid() && modelFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
		Model model = modelFuture.get();
		if (model.initGLResources()) {
			modelCache.insert({ model.getPath(), std::make_shared<Model>(model)});
		}
	}

	if ((!modelFuture.valid() || modelFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
		&& !modelQueue.empty()) {
		if (modelCache.find(modelQueue.front()) != modelCache.end()) {
			modelQueue.pop();
			return;
		}
		modelFuture = Model::LoadAsync(modelQueue.front().c_str());
		modelQueue.pop();
	}
}

#endif // !RESOURCEMANAGER_HPP