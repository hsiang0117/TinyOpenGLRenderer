#ifndef TOGR_SCENE_HPP
#define TOGR_SCENE_HPP
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

namespace togr {

// Owns the scene's object list, deferred removals and cached component membership.
// Use on the main thread. Assemble object components before addObject(); editing
// component values is supported, changing the component set is not tracked here.
class Scene {
public:
    // Immediate membership change. Returns false for null or light-capacity
    // rejection; a rejected object is not inserted.
    bool addObject(GameObjectPtr obj);
    void removeObjectByIndex(std::size_t index);

    // Deferred removal uses indices in the current list. Commit before making
    // other immediate changes that would shift those indices. Invalid indices
    // are ignored; processRemovals() commits in descending index order.
    void queueRemoval(std::size_t index);
    void processRemovals();

    // Shared snapshots keep objects alive independently of scene membership.
    // They do not freeze component values. Invalid indices return nullptr.
    std::vector<GameObjectPtr> getObjects() const;
    GameObjectPtr getObjectAt(std::size_t index) const {
        return (index < objects.size()) ? objects[index] : nullptr;
    }

    // Borrowed membership view, built lazily per type. A successful membership
    // change or shutdown() invalidates it. Other getView() calls do not.
    // The const vector protects the list, not the objects/components it points to.
    template<typename T>
    const std::vector<GameObjectPtr>& getView() const;

    // Releases scene references and pending removals; release GL-owning objects
    // with a current context. External shared snapshots may retain those objects.
    void shutdown();

private:
    std::vector<GameObjectPtr> objects;
    std::queue<std::size_t> removalQueue;
    mutable std::unordered_map<std::type_index, std::vector<GameObjectPtr>> viewCache_;
};

template<typename T>
const std::vector<GameObjectPtr>& Scene::getView() const {
    const std::type_index key(typeid(T));
    const auto cached = viewCache_.find(key);
    if (cached != viewCache_.end()) return cached->second;

    std::vector<GameObjectPtr> filtered;
    for (const auto& object : objects) {
        if (object && object->has<T>()) filtered.push_back(object);
    }
    return viewCache_.emplace(key, std::move(filtered)).first->second;
}

using ScenePtr = std::shared_ptr<Scene>;

} // namespace togr

#endif // !TOGR_SCENE_HPP
