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
#include <typeindex>


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

	void shutdown() {
		if (asyncFuture.valid()) {
			asyncFuture.wait();
		}
		cache.clear();
		while (!asyncQueue.empty()) asyncQueue.pop();
	}

	void update() {
		// �����첽��������
		if (asyncFuture.valid() && asyncFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
			auto model = asyncFuture.get();
			if (!asyncQueue.empty()) {
				auto path = asyncQueue.front();
				asyncQueue.pop();
				addToCache(path, model);
			}
		}
		// �����µ��첽����
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

	void shutdown() {
		cache.clear();
	}

private:
	std::unordered_map<std::string, ShaderPtr> cache;
};

class Scene {
public:
	using ObjectFilter = std::function<bool(const GameObjectPtr&)>;

	const std::string& getName() const { return name; }
	void setName(std::string n) { name = std::move(n); }

	void addObject(GameObjectPtr obj) {
		if (!obj) return;
		objects.push_back(obj);
		viewCache_.clear();
	}

	void removeObject(GameObjectPtr obj) {
		auto it = std::find(objects.begin(), objects.end(), obj);
		if (it != objects.end()) {
			objects.erase(it);
			viewCache_.clear();
		}
	}

	void removeObjectByIndex(size_t idx) {
		if (idx < objects.size()) {
			objects.erase(objects.begin() + idx);
			viewCache_.clear();
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

	// Cached component view. Built lazily on first request per type, returned by
	// const-ref thereafter. Cache is cleared whenever objects are added/removed.
	// CONTRACT: components must not be added to an object already in the scene
	// (today all addComponent happens in GameObjectFactory before addObject). If
	// that ever changes, call viewCache_.clear() after mutating an in-scene object.
	template<typename T>
	const std::vector<GameObjectPtr>& getView() const {
		std::type_index key(typeid(T));
		auto it = viewCache_.find(key);
		if (it != viewCache_.end()) return it->second;
		std::vector<GameObjectPtr> filtered;
		for (auto& obj : objects) {
			if (obj && obj->has<T>()) filtered.push_back(obj);
		}
		return viewCache_.emplace(key, std::move(filtered)).first->second;
	}

	size_t getObjectCount() const { return objects.size(); }
	GameObjectPtr getObjectAt(size_t idx) const {
		return (idx < objects.size()) ? objects[idx] : nullptr;
	}

	void shutdown() {
		objects.clear();
		while (!removalQueue.empty()) removalQueue.pop();
		viewCache_.clear();
	}

private:
	std::string name = "Untitled";
	std::vector<GameObjectPtr> objects;
	std::queue<size_t> removalQueue;
	mutable std::unordered_map<std::type_index, std::vector<GameObjectPtr>> viewCache_;
};

using ScenePtr = std::shared_ptr<Scene>;

class ResourceManager {
public:
	ResourceManager() { currentScene = std::make_shared<Scene>(); }

	void init() {
		shaderLoader.registerShader("default", "data/shader/test.vert", "data/shader/test.frag");
		shaderLoader.registerShader("skybox", "data/shader/skybox.vert", "data/shader/skybox.frag");
		shaderLoader.registerShader("depth", "data/shader/depth.vert", "data/shader/depth.frag");
		shaderLoader.registerShader("depthCube", "data/shader/depthCube.vert", "data/shader/depthCube.geom", "data/shader/depthCube.frag");
		shaderLoader.registerShader("screenQuad", "data/shader/screenQuad.vert", "data/shader/screenQuad.frag");
		shaderLoader.registerShader("lightCube", "data/shader/lightCube.vert", "data/shader/lightCube.frag");
		shaderLoader.registerShader("gaussianBlur", "data/shader/gaussianBlur.vert", "data/shader/gaussianBlur.frag");
		shaderLoader.registerShader("bone", "data/shader/bone.vert", "data/shader/bone.frag");
		shaderLoader.registerShader("volume", "data/shader/volume.vert", "data/shader/volume.frag");
	}

	void update() {
		modelLoader.update();
		currentScene->processRemovals();
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
		currentScene->addObject(obj);
	}

	void removeGameObject(size_t idx) {
		currentScene->queueRemoval(idx);
	}

	std::vector<GameObjectPtr> getGameObjects() const {
		return currentScene->getObjects();
	}

	GameObjectPtr getGameObjectAt(size_t idx) const {
		return currentScene->getObjectAt(idx);
	}

	size_t getGameObjectCount() const {
		return currentScene->getObjectCount();
	}

	int getPointLightCount() const { return (int)currentScene->getView<PointLightComponent>().size(); }
	int getDirectionLightCount() const { return (int)currentScene->getView<DirectionLightComponent>().size(); }
	int getSpotLightCount() const { return (int)currentScene->getView<SpotLightComponent>().size(); }

	template<typename T>
	const std::vector<GameObjectPtr>& getSceneView() const {
		return currentScene->getView<T>();
	}

	ScenePtr getCurrentScene() const { return currentScene; }
	void setCurrentScene(ScenePtr s) {
		currentScene = s ? s : std::make_shared<Scene>();
	}

	void shutdown() {
		currentScene->shutdown();
		modelLoader.shutdown();
		shaderLoader.shutdown();
	}
	ResourceManager(const ResourceManager&) = delete;
	ResourceManager& operator=(const ResourceManager&) = delete;
private:
	ModelLoader modelLoader;
	ShaderLoader shaderLoader;
	ScenePtr currentScene;
};
#endif // !RESOURCEMANAGER_HPP