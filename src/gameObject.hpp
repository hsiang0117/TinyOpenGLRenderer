#ifndef GAMEOBJECT_HPP
#define GAMEOBJECT_HPP
#pragma once

#include "component.hpp"
#include "model.hpp"
#include "shader.hpp"

class GameObject {
public:
	GameObject(std::string name, ModelPtr model);
	void draw(ShaderPtr shader);
	std::string getName() { return name; }

	template<typename T, typename... Args>
	std::shared_ptr<T> addComponent(Args&&... args);

	template <typename T>
	std::shared_ptr<T> getComponent();
private:
	std::string name;
	ModelPtr model;
	std::vector <ComponentPtr> components;
};

GameObject::GameObject(std::string name, ModelPtr model) {
	this->name = name;
	this->model = model;
	addComponent<Transform>();
}

void GameObject::draw(ShaderPtr shader) {
	glm::mat4 model = glm::mat4(1.0f);
	auto transform = getComponent<Transform>();
	model = glm::translate(model, transform->translate);
	model = glm::rotate(model, glm::radians(transform->rotate.z), glm::vec3(0, 0, 1));
	model = glm::rotate(model, glm::radians(transform->rotate.y), glm::vec3(0, 1, 0));
	model = glm::rotate(model, glm::radians(transform->rotate.x), glm::vec3(1, 0, 0));
	model = glm::scale(model, transform->scale);
	shader.get()->setMat4("model", model);
	if (this->model) {
		this->model->draw();
	}
}

template<typename T, typename ...Args>
std::shared_ptr<T> GameObject::addComponent(Args && ...args){
	auto component = std::make_shared<T>(std::forward<Args>(args)...);
	components.push_back(component);
	return component;
}

template<typename T>
std::shared_ptr<T> GameObject::getComponent()
{
	for (auto& comp : components) {
		std::shared_ptr<T> casted = std::dynamic_pointer_cast<T>(comp);
		if (casted) return casted;
	}
	return nullptr;
}

using GameObjectPtr = std::shared_ptr<GameObject>;
#endif // !ITEM_HPP