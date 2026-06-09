#ifndef GAMEOBJECT_HPP
#define GAMEOBJECT_HPP
#pragma once

#include "component.hpp"
#include <typeindex>
#include <unordered_map>
#include <memory>
#include <string>
#include <vector>

class GameObject {
public:
	GameObject(std::string name);
	~GameObject() = default;
	std::string getName() { return name; }

	template<typename T, typename... Args>
	std::shared_ptr<T> addComponent(Args&&... args);

	template <typename T>
	std::shared_ptr<T> getComponent();

	template <typename T>
	bool has();

	std::vector<ComponentPtr> getAllComponents();
private:
	std::string name;
	std::unordered_map<std::type_index, ComponentPtr> components;
};

inline GameObject::GameObject(std::string name) : name(std::move(name)) {}

inline std::vector<ComponentPtr> GameObject::getAllComponents()
{
	std::vector<ComponentPtr> result;
	result.reserve(components.size());
	for (auto& kv : components) {
		result.push_back(kv.second);
	}
	return result;
}

template<typename T, typename ...Args>
std::shared_ptr<T> GameObject::addComponent(Args && ...args) {
	auto component = std::make_shared<T>(std::forward<Args>(args)...);
	components[std::type_index(typeid(T))] = component;
	return component;
}

template<typename T>
std::shared_ptr<T> GameObject::getComponent()
{
	auto it = components.find(std::type_index(typeid(T)));
	if (it == components.end()) return nullptr;
	return std::static_pointer_cast<T>(it->second);
}

template<typename T>
bool GameObject::has()
{
	return components.find(std::type_index(typeid(T))) != components.end();
}

using GameObjectPtr = std::shared_ptr<GameObject>;

// Single source of truth for the currently-selected object, shared between the GUI
// (scene tree writes + inspector reads) and the renderer (selection gizmo). Holds a
// weak_ptr so it auto-expires if the object is destroyed elsewhere. Owned by Engine
// (composition root) and injected into both systems by non-owning pointer, keeping
// GuiSystem and RenderSystem free of a mutual dependency.
class SelectionModel {
public:
	void set(const GameObjectPtr& obj) { selected_ = obj; }
	void clear() { selected_.reset(); }
	GameObjectPtr get() const { return selected_.lock(); }
private:
	std::weak_ptr<GameObject> selected_;
};

#endif // !GAMEOBJECT_HPP
