#ifndef SCENE_HPP
#define SCENE_HPP
#pragma once

#include "gameObject.hpp"
#include <cstddef>
#include <memory>
#include <queue>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>
#include <utility>
#include <vector>

class Scene {
public:
	bool addObject(GameObjectPtr obj);
	void removeObjectByIndex(std::size_t idx);
	void queueRemoval(std::size_t idx);
	void processRemovals();
	std::vector<GameObjectPtr> getObjects() const;

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

	GameObjectPtr getObjectAt(std::size_t idx) const {
		return (idx < objects.size()) ? objects[idx] : nullptr;
	}

	void shutdown();

private:
	std::vector<GameObjectPtr> objects;
	std::queue<std::size_t> removalQueue;
	mutable std::unordered_map<std::type_index, std::vector<GameObjectPtr>> viewCache_;
};

using ScenePtr = std::shared_ptr<Scene>;

#endif // !SCENE_HPP
