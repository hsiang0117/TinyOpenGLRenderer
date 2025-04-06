#ifndef RESOURCEMANAGER_HPP
#define RESOURCEMANAGER_HPP
#pragma once

#include "../model.hpp"
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
	std::vector<ModelPtr> modelCache; //cache for models
	std::unordered_map<std::string, ShaderPtr> shaderCache; //cache for shaders
private:
	std::queue<std::string> modelQueue; //queue for loading models
};

ResourceManager& ResourceManager::getInstance() {
	static ResourceManager instance;
	return instance;
}

void ResourceManager::init() {
	modelQueue.push("data/model/backpack/backpack.obj");
	modelQueue.push("data/model/nanosuit_reflection/nanosuit.obj");
	shaderCache["default"] = std::make_shared<Shader>("data/shader/test.vert", "data/shader/test.frag");
}

void ResourceManager::update() {
	if (modelFuture.valid() && modelFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
		Model model = modelFuture.get();
		if (model.initGLResources()) {
			modelCache.push_back(std::make_shared<Model>(model));
		}
	}

	if ((!modelFuture.valid() || modelFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
		&& !modelQueue.empty()) {
		modelFuture = Model::LoadAsync(modelQueue.front().c_str());
		modelQueue.pop();
	}
}

#endif // !RESOURCEMANAGER_HPP