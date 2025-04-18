#ifndef RESOURCEMANAGER_HPP
#define RESOURCEMANAGER_HPP
#pragma once

#include "../glBuffer.hpp"
#include "../shader.hpp"
#include "../model.hpp"
#include "../gameObject.hpp"
#include <iostream>
#include <queue>
#include <unordered_map>

class ResourceManager {
public:
	ResourceManager() = default;
	~ResourceManager() = default;
	static ResourceManager& getInstance();
	void init();
	void update();
	std::future<ModelPtr> modelFuture; //future for loading models
	std::queue<std::string> modelQueue; //queue for loading models
	std::unordered_map<std::string, ModelPtr> modelCache; //cache for models
	std::unordered_map<std::string, ShaderPtr> shaderCache; //cache for shaders
	std::vector<GameObjectPtr> gameObjects;
	std::queue<int> removeQueue;
	int pointLightNum = 0, directionLightNum = 0, spotLightNum = 0;
private:
};
 
ResourceManager& ResourceManager::getInstance() {
	static ResourceManager instance;
	return instance;
}

void ResourceManager::init() {
	shaderCache["default"] = std::make_shared<Shader>("data/shader/test.vert", "data/shader/test.frag");
	shaderCache["skybox"] = std::make_shared<Shader>("data/shader/skybox.vert", "data/shader/skybox.frag");
	shaderCache["depth"] = std::make_shared<Shader>("data/shader/depth.vert", "data/shader/depth.frag");
	shaderCache["screenQuad"] = std::make_shared<Shader>("data/shader/screenQuad.vert", "data/shader/screenQuad.frag");
}

void ResourceManager::update() {
	if (modelFuture.valid() && modelFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
		ModelPtr model = modelFuture.get();
		if (model->initGLResources()) {
			modelCache.insert({ model->getPath(), model});
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

	while (!removeQueue.empty()) {
		int i = removeQueue.front();
		if (gameObjects[i]->getType() == GameObject::Type::POINTLIGHTOBJECT) {
			pointLightNum--;
		}
		else if (gameObjects[i]->getType() == GameObject::Type::DIRECTIONLIGHTOBJECT) {
			directionLightNum--;
		}
		else if (gameObjects[i]->getType() == GameObject::Type::SPOTLIGHTOBJECT) {
			spotLightNum--;
		}
		gameObjects[i] = nullptr;
		removeQueue.pop();
	}

	for (auto it = gameObjects.begin(); it < gameObjects.end();) {
		if (!it->get()) {
			it = gameObjects.erase(it);
		}
		else ++it;
	}
}

#endif // !RESOURCEMANAGER_HPP