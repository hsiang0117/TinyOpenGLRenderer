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


class ModelLoader{
public:
	ModelLoader() = default;

	std::future<ModelPtr> loadAsync(const std::string& path){
		return Model::LoadAsync(path.c_str());
	}

	void loadFromPath(const std::string& path) {
		if (!isLoaded(path)) {
			asyncQueue.push(path);
		}
	}

	bool isLoaded(const std::string& key) const{
		return cache.find(key) != cache.end();
	}

	ModelPtr get(const std::string& key) const{
		auto it = cache.find(key);
		return (it != cache.end()) ? it->second : nullptr;
	}

	std::vector<ModelPtr> getAllLoadedModels() const {
		std::vector<ModelPtr> models;
		for (const auto& pair : cache) {
			models.push_back(pair.second);
		}
		return models;
	}

	void addToCache(const std::string& key, ModelPtr model) {
		if (model && model->initGLResources()) {
			cache[key] = model;
		}
	}

	void update() {
		// 处理异步加载任务
		if (asyncFuture.valid() && asyncFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
			auto model = asyncFuture.get();
			if (!asyncQueue.empty()) {
				auto path = asyncQueue.front();
				asyncQueue.pop();
				addToCache(path, model);
			}
		}
		// 启动新的异步任务
		if ((!asyncFuture.valid() || asyncFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready) 
			&& !asyncQueue.empty()) {
			auto path = asyncQueue.front();
			if (!isLoaded(path)) {
				asyncFuture = loadAsync(path);
			} else {
				asyncQueue.pop();
			}
		}
	}

private:
	std::unordered_map<std::string, ModelPtr> cache;
	std::future<ModelPtr> asyncFuture;
	std::queue<std::string> asyncQueue;
};

class ShaderLoader{
public:
	ShaderLoader() = default;

	bool isLoaded(const std::string& key) const{
		return cache.find(key) != cache.end();
	}

	ShaderPtr get(const std::string& key) const{
		auto it = cache.find(key);
		return (it != cache.end()) ? it->second : nullptr;
	}

	void registerShader(const std::string& key, const char* vert, const char* frag) {
		if (!isLoaded(key)) {
			cache[key] = std::make_shared<Shader>(vert, frag);
		}
	}

	void registerShader(const std::string& key, const char* vert, const char* geom, const char* frag) {
		if (!isLoaded(key)) {
			cache[key] = std::make_shared<Shader>(vert, geom, frag);
		}
	}

private:
	std::unordered_map<std::string, ShaderPtr> cache;
};

class SceneManager {
public:
	using ObjectFilter = std::function<bool(const GameObjectPtr&)>;

	void addObject(GameObjectPtr obj) {
		if (!obj) return;
		objects.push_back(obj);
		updateLightCount(obj, 1);
	}

	void removeObject(GameObjectPtr obj) {
		auto it = std::find(objects.begin(), objects.end(), obj);
		if (it != objects.end()) {
			updateLightCount(*it, -1);
			objects.erase(it);
		}
	}

	void removeObjectByIndex(size_t idx) {
		if (idx < objects.size()) {
			updateLightCount(objects[idx], -1);
			objects.erase(objects.begin() + idx);
		}
	}

	void queueRemoval(size_t idx) {
		removalQueue.push(idx);
	}

	void processRemovals() {
		std::vector<size_t> indices;
		while (!removalQueue.empty()) {
			indices.push_back(removalQueue.front());
			removalQueue.pop();
		}
		std::sort(indices.rbegin(), indices.rend());

		for (size_t idx : indices) {
			if (idx < objects.size()) {
				removeObjectByIndex(idx);
			}
		}
	}

	std::vector<GameObjectPtr> getObjects(ObjectFilter filter = nullptr) const {
		if (!filter) return objects;

		std::vector<GameObjectPtr> filtered;
		std::copy_if(objects.begin(), objects.end(), std::back_inserter(filtered), filter);
		return filtered;
	}

	size_t getObjectCount() const { return objects.size(); }
	GameObjectPtr getObjectAt(size_t idx) const {
		return (idx < objects.size()) ? objects[idx] : nullptr;
	}

	int getPointLightCount() const { return pointLightCount; }
	int getDirectionLightCount() const { return directionLightCount; }
	int getSpotLightCount() const { return spotLightCount; }

private:
	std::vector<GameObjectPtr> objects;
	std::queue<size_t> removalQueue;
	int pointLightCount = 0;
	int directionLightCount = 0;
	int spotLightCount = 0;

	void updateLightCount(const GameObjectPtr& obj, int delta) {
		if (!obj) return;
		switch (obj->getType()) {
			case GameObject::Type::POINTLIGHTOBJECT:
				pointLightCount += delta;
				break;
			case GameObject::Type::DIRECTIONLIGHTOBJECT:
				directionLightCount += delta;
				break;
			case GameObject::Type::SPOTLIGHTOBJECT:
				spotLightCount += delta;
				break;
			default:
				break;
		}
	}
};

class ResourceManager {
public:
	static ResourceManager& getInstance() {
		static ResourceManager instance;
		return instance;
	}

	void init() {
		shaderLoader.registerShader("default", "data/shader/test.vert", "data/shader/test.frag");
		shaderLoader.registerShader("skybox", "data/shader/skybox.vert", "data/shader/skybox.frag");
		shaderLoader.registerShader("depth", "data/shader/depth.vert", "data/shader/depth.frag");
		shaderLoader.registerShader("depthCube", "data/shader/depthCube.vert", "data/shader/depthCube.geom", "data/shader/depthCube.frag");
		shaderLoader.registerShader("screenQuad", "data/shader/screenQuad.vert", "data/shader/screenQuad.frag");
		shaderLoader.registerShader("lightCube", "data/shader/lightCube.vert", "data/shader/lightCube.frag");
		shaderLoader.registerShader("gaussianBlur", "data/shader/gaussianBlur.vert", "data/shader/gaussianBlur.frag");
		shaderLoader.registerShader("bone", "data/shader/bone.vert", "data/shader/bone.frag");
	}

	void update() {
		modelLoader.update();
		sceneManager.processRemovals();
	}

	void queueModelLoad(const std::string& path) {
		modelLoader.loadFromPath(path);
	}

	ModelPtr getModel(const std::string& key) const {
		return modelLoader.get(key);
	}

	std::vector<ModelPtr> getAllModels() const {
		return modelLoader.getAllLoadedModels();
	}

	ShaderPtr getShader(const std::string& key) const {
		return shaderLoader.get(key);
	}

	void addGameObject(GameObjectPtr obj) {
		sceneManager.addObject(obj);
	}

	void removeGameObject(size_t idx) {
		sceneManager.queueRemoval(idx);
	}

	std::vector<GameObjectPtr> getGameObjects() const {
		return sceneManager.getObjects();
	}

	GameObjectPtr getGameObjectAt(size_t idx) const {
		return sceneManager.getObjectAt(idx);
	}

	size_t getGameObjectCount() const {
		return sceneManager.getObjectCount();
	}

	int getPointLightCount() const { return sceneManager.getPointLightCount(); }
	int getDirectionLightCount() const { return sceneManager.getDirectionLightCount(); }
	int getSpotLightCount() const { return sceneManager.getSpotLightCount(); }

	std::vector<GameObjectPtr> getObjectsByType(GameObject::Type type) const {
		return sceneManager.getObjects([type](const GameObjectPtr& obj) {
			return obj && obj->getType() == type;
			});
	}
private:
	ResourceManager() = default;
	~ResourceManager() = default;
	ResourceManager(const ResourceManager&) = delete;
	ResourceManager& operator=(const ResourceManager&) = delete;
	ModelLoader modelLoader;
	ShaderLoader shaderLoader;
	SceneManager sceneManager;
};
#endif // !RESOURCEMANAGER_HPP