#ifndef GAMEOBJECT_HPP
#define GAMEOBJECT_HPP
#pragma once

#include "model.hpp"
#include "shader.hpp"

class GameObject {
public:
	GameObject(std::string name, ModelPtr model);
	void draw(ShaderPtr shader);
	std::string getName() { return name; }
	glm::vec3 scale = glm::vec3(1.0f),
		translate = glm::vec3(0.0f),
		rotate = glm::vec3(0.0f);
private:
	std::string name;
	ModelPtr model;
};

GameObject::GameObject(std::string name, ModelPtr model) {
	this->name = name;
	this->model = model;
}

void GameObject::draw(ShaderPtr shader) {
	glm::mat4 model = glm::mat4(1.0f);
	model = glm::translate(model, translate);
	model = glm::rotate(model, glm::radians(rotate.z), glm::vec3(0, 0, 1));
	model = glm::rotate(model, glm::radians(rotate.y), glm::vec3(0, 1, 0));
	model = glm::rotate(model, glm::radians(rotate.x), glm::vec3(1, 0, 0));
	model = glm::scale(model, scale);
	shader.get()->setMat4("model", model);
	if (this->model) {
		this->model->draw();
	}
}

using GameObjectPtr = std::shared_ptr<GameObject>;
#endif // !ITEM_HPP