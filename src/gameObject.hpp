#ifndef GAMEOBJECT_HPP
#define GAMEOBJECT_HPP
#pragma once

#include "component.hpp"
#include "model.hpp"
#include "shader.hpp"

class GameObject {
public:
	enum class Type {
		RENDEROBJECT,
		POINTLIGHTOBJECT,
		DIRECTIONLIGHTOBJECT,
		SPOTLIGHTOBJECT
	};

	GameObject(std::string name);
	virtual ~GameObject() = default;
	std::string getName() { return name; }
	Type getType() { return type; }
	virtual void draw(ShaderPtr shader) {}; // RenderObject在渲染时调用的接口
	virtual void sendToSSBO(int index, GLuint ssbo) {}; // LightObject在渲染时调用的接口

	template<typename T, typename... Args>
	std::shared_ptr<T> addComponent(Args&&... args);

	template <typename T>
	std::shared_ptr<T> getComponent();

	std::vector <ComponentPtr> getAllComponents();
protected:
	std::string name;
	Type type;
	std::vector <ComponentPtr> components;
};

GameObject::GameObject(std::string name) {
	this->name = name;
}

std::vector<ComponentPtr> GameObject::getAllComponents()
{
	return components;
}

template<typename T, typename ...Args>
std::shared_ptr<T> GameObject::addComponent(Args && ...args) {
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

class RenderObject : public GameObject {
public:
	RenderObject(std::string name) : GameObject(name) {
		type = GameObject::Type::RENDEROBJECT;
	}
	virtual void draw(ShaderPtr shader) override;
};

void RenderObject::draw(ShaderPtr shader) {
	shader->setInt("albedoMap", 0);
	shader->setInt("specularMap", 2);
	if (auto renderComponent = getComponent<RenderComponent>()) {
		glm::mat4 model = glm::mat4(1.0f);
		if (auto transform = getComponent<Transform>()) {
			model = glm::translate(model, transform->translate);
			model = glm::rotate(model, glm::radians(transform->rotate.z), glm::vec3(0, 0, 1));
			model = glm::rotate(model, glm::radians(transform->rotate.y), glm::vec3(0, 1, 0));
			model = glm::rotate(model, glm::radians(transform->rotate.x), glm::vec3(1, 0, 0));
			model = glm::scale(model, transform->scale);
			shader.get()->setMat4("model", model);
			if (renderComponent->model) {
				renderComponent->model->draw();
			}
		}
	}
}

class PointLightObject : public GameObject {
public:
	PointLightObject(std::string name) : GameObject(name) {
		type = GameObject::Type::POINTLIGHTOBJECT;
	}
	virtual void sendToSSBO(int index, GLuint ssbo) override;
	static const int glslSize = 48;
};

void PointLightObject::sendToSSBO(int index, GLuint ssbo) {
	std::shared_ptr<Transform> transform = getComponent<Transform>();
	std::shared_ptr<PointLightComponent> pointLight = getComponent<PointLightComponent>();
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
	glBufferSubData(GL_SHADER_STORAGE_BUFFER, index * glslSize, 12, glm::value_ptr(transform->translate));
	glBufferSubData(GL_SHADER_STORAGE_BUFFER, index * glslSize + 12, 4, 0);
	glBufferSubData(GL_SHADER_STORAGE_BUFFER, index * glslSize + 16, 12, glm::value_ptr(pointLight->color));
	glBufferSubData(GL_SHADER_STORAGE_BUFFER, index * glslSize + 28, 4, 0);
	glBufferSubData(GL_SHADER_STORAGE_BUFFER, index * glslSize + 32, 4, &pointLight->constant);
	glBufferSubData(GL_SHADER_STORAGE_BUFFER, index * glslSize + 36, 4, &pointLight->linear);
	glBufferSubData(GL_SHADER_STORAGE_BUFFER, index * glslSize + 40, 4, &pointLight->quadratic);
	glBufferSubData(GL_SHADER_STORAGE_BUFFER, index * glslSize + 44, 4, 0);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

class DirectionLightObject : public GameObject {
public:
	DirectionLightObject(std::string name) : GameObject(name) {
		type = GameObject::Type::DIRECTIONLIGHTOBJECT;
	}
	virtual void sendToSSBO(int index, GLuint ssbo) override;
	static const int glslSize = 32;
};

void DirectionLightObject::sendToSSBO(int index, GLuint ssbo) {
	std::shared_ptr<Transform> transform = getComponent<Transform>();
	std::shared_ptr<DirectionLightComponent> directionLight = getComponent<DirectionLightComponent>();
	glm::mat4 rotation(1.0);
	rotation = glm::rotate(rotation, glm::radians(transform->rotate.z), glm::vec3(0, 0, 1));
	rotation = glm::rotate(rotation, glm::radians(transform->rotate.y), glm::vec3(0, 1, 0));
	rotation = glm::rotate(rotation, glm::radians(transform->rotate.x), glm::vec3(1, 0, 0));
	glm::vec3 direction(1.0,0.0,0.0);
	direction = glm::mat3(rotation) * direction;
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
	glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, 12, glm::value_ptr(direction));
	glBufferSubData(GL_SHADER_STORAGE_BUFFER, 12, 4, 0);
	glBufferSubData(GL_SHADER_STORAGE_BUFFER, 16, 12, glm::value_ptr(directionLight->color));
	glBufferSubData(GL_SHADER_STORAGE_BUFFER, 28, 4, 0);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

class SpotLightObject : public GameObject {
public:
	SpotLightObject(std::string name) : GameObject(name) {
		type = GameObject::Type::SPOTLIGHTOBJECT;
	}
	virtual void sendToSSBO(int index, GLuint ssbo) override;
	static const int glslSize = 64;
};

void SpotLightObject::sendToSSBO(int index, GLuint ssbo) {
	std::shared_ptr<Transform> transform = getComponent<Transform>();
	std::shared_ptr<SpotLightComponent> spotLight = getComponent<SpotLightComponent>();
	glm::mat4 rotation(1.0);
	rotation = glm::rotate(rotation, glm::radians(transform->rotate.z), glm::vec3(0, 0, 1));
	rotation = glm::rotate(rotation, glm::radians(transform->rotate.y), glm::vec3(0, 1, 0));
	rotation = glm::rotate(rotation, glm::radians(transform->rotate.x), glm::vec3(1, 0, 0));
	glm::vec3 direction(1.0, 0.0, 0.0);
	direction = glm::mat3(rotation) * direction;
	float cutOff = glm::cos(glm::radians(spotLight->cutOff));
	float outerCutOff = glm::cos(glm::radians(spotLight->outerCutOff));
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
	glBufferSubData(GL_SHADER_STORAGE_BUFFER, index * glslSize, 12, glm::value_ptr(transform->translate));
	glBufferSubData(GL_SHADER_STORAGE_BUFFER, index * glslSize + 12, 4, 0);
	glBufferSubData(GL_SHADER_STORAGE_BUFFER, index * glslSize + 16, 12, glm::value_ptr(direction));
	glBufferSubData(GL_SHADER_STORAGE_BUFFER, index * glslSize + 28, 4, 0);
	glBufferSubData(GL_SHADER_STORAGE_BUFFER, index * glslSize + 32, 12, glm::value_ptr(spotLight->color));
	glBufferSubData(GL_SHADER_STORAGE_BUFFER, index * glslSize + 44, 4, 0);
	glBufferSubData(GL_SHADER_STORAGE_BUFFER, index * glslSize + 48, 4, &cutOff);
	glBufferSubData(GL_SHADER_STORAGE_BUFFER, index * glslSize + 52, 4, &outerCutOff);
	glBufferSubData(GL_SHADER_STORAGE_BUFFER, index * glslSize + 56, 8, 0);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}
#endif // !GAMEOBJEC_HPP