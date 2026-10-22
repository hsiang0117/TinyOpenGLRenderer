#ifndef TOGR_GAMEOBJECT_HPP
#define TOGR_GAMEOBJECT_HPP
#pragma once

#include <memory>
#include <string>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>
#include <utility>
#include <vector>

namespace togr {

class Component;

// Owns one component per concrete type; shared queries keep that component alive.
// Assemble the component set before adding the object to a Scene. Scene views do
// not observe later addComponent() calls; editing existing component values is OK.
class GameObject {
public:
    GameObject(std::string name);
    ~GameObject() = default;

    // Assembly: replaces any existing component of the same concrete type.
    template<typename T, typename... Args>
    std::shared_ptr<T> addComponent(Args&&... args);

    // Queries do not change membership. Returned components remain editable;
    // const GameObject does not imply deep constness of shared components.
    std::string getName() const { return name; }

    template<typename T>
    std::shared_ptr<T> getComponent() const;

    template<typename T>
    bool has() const;

    std::vector<std::shared_ptr<Component>> getAllComponents() const;

private:
    std::string name;
    std::unordered_map<std::type_index, std::shared_ptr<Component>> components;
};

inline GameObject::GameObject(std::string name) : name(std::move(name)) {}

inline std::vector<std::shared_ptr<Component>> GameObject::getAllComponents() const {
    std::vector<std::shared_ptr<Component>> result;
    result.reserve(components.size());
    for (const auto& entry : components) {
        result.push_back(entry.second);
    }
    return result;
}

template<typename T, typename ...Args>
std::shared_ptr<T> GameObject::addComponent(Args&&... args) {
    auto component = std::make_shared<T>(std::forward<Args>(args)...);
    components[std::type_index(typeid(T))] = component;
    return component;
}

template<typename T>
std::shared_ptr<T> GameObject::getComponent() const {
    auto it = components.find(std::type_index(typeid(T)));
    if (it == components.end()) return nullptr;
    return std::static_pointer_cast<T>(it->second);
}

template<typename T>
bool GameObject::has() const {
    return components.find(std::type_index(typeid(T))) != components.end();
}

using GameObjectPtr = std::shared_ptr<GameObject>;

} // namespace togr

#endif // !TOGR_GAMEOBJECT_HPP
