#ifndef GAMEOBJECT_HPP
#define GAMEOBJECT_HPP
#pragma once

#include "glBuffer.hpp"
#include "component.hpp"
#include "model.hpp"
#include "shader.hpp"

class GameObject {
public:
	enum class Type {
		RENDEROBJECT,
		POINTLIGHTOBJECT,
		DIRECTIONLIGHTOBJECT,
		SPOTLIGHTOBJECT,
		SKYBOXOBJECT
	};

	GameObject(std::string name);
	virtual ~GameObject() = default;
	std::string getName() { return name; }
	Type getType() { return type; }
	virtual void draw(ShaderPtr shader) {}
	virtual void sendToSSBO(int index, ShaderStorageBuffer ssbo) {}
	virtual glm::mat4 getLightMatrices() { return glm::mat4(1.0f); }
	virtual void useCubeMap(ShaderPtr shader) {}

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
	virtual void sendToSSBO(int index, ShaderStorageBuffer ssbo) override;
	static const int glslSize = 48;
};

void PointLightObject::sendToSSBO(int index, ShaderStorageBuffer ssbo) {
	auto transform = getComponent<Transform>();
	auto pointLight = getComponent<PointLightComponent>();
	ssbo.bind();
	ssbo.bufferSubdata(index * glslSize, 12, glm::value_ptr(transform->translate));
	ssbo.bufferSubdata(index * glslSize + 12, 4, nullptr);
	ssbo.bufferSubdata(index * glslSize + 16, 12, glm::value_ptr(pointLight->color));
	ssbo.bufferSubdata(index * glslSize + 28, 4, nullptr);
	ssbo.bufferSubdata(index * glslSize + 32, 4, &pointLight->constant);
	ssbo.bufferSubdata(index * glslSize + 36, 4, &pointLight->linear);
	ssbo.bufferSubdata(index * glslSize + 40, 4, &pointLight->quadratic);
	ssbo.bufferSubdata(index * glslSize + 44, 4, nullptr);
	ssbo.unbind();
}

class DirectionLightObject : public GameObject {
public:
	DirectionLightObject(std::string name) : GameObject(name) {
		type = GameObject::Type::DIRECTIONLIGHTOBJECT;
	}
	virtual void sendToSSBO(int index, ShaderStorageBuffer ssbo) override;
	virtual glm::mat4 getLightMatrices() override;

	static const int glslSize = 32;
private:
};

void DirectionLightObject::sendToSSBO(int index, ShaderStorageBuffer ssbo) {
	auto transform = getComponent<Transform>();
	auto directionLight = getComponent<DirectionLightComponent>();
	glm::mat4 rotation(1.0);
	rotation = glm::rotate(rotation, glm::radians(transform->rotate.z), glm::vec3(0, 0, 1));
	rotation = glm::rotate(rotation, glm::radians(transform->rotate.y), glm::vec3(0, 1, 0));
	rotation = glm::rotate(rotation, glm::radians(transform->rotate.x), glm::vec3(1, 0, 0));
	glm::vec3 direction(1.0,0.0,0.0);
	direction = glm::mat3(rotation) * direction;
	ssbo.bind();
	ssbo.bufferSubdata(0, 12, glm::value_ptr(direction));
	ssbo.bufferSubdata(12, 4, nullptr);
	ssbo.bufferSubdata(16, 12, glm::value_ptr(directionLight->color));
	ssbo.bufferSubdata(28, 4, nullptr);
	ssbo.unbind();
}

glm::mat4 DirectionLightObject::getLightMatrices() {
	auto transform = getComponent<Transform>();
	glm::mat4 rotation(1.0);
	rotation = glm::rotate(rotation, glm::radians(transform->rotate.z), glm::vec3(0, 0, 1));
	rotation = glm::rotate(rotation, glm::radians(transform->rotate.y), glm::vec3(0, 1, 0));
	rotation = glm::rotate(rotation, glm::radians(transform->rotate.x), glm::vec3(1, 0, 0));
	glm::vec3 direction(1.0, 0.0, 0.0);
	direction = glm::mat3(rotation) * direction;
	glm::mat4 lightProjection = glm::ortho(-10.0f, 10.0f, -10.0f, 10.0f, -10.0f, 10.0f);
	glm::mat4 lightView = glm::lookAt(-direction, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
	return lightProjection * lightView;
}

class SpotLightObject : public GameObject {
public:
	SpotLightObject(std::string name) : GameObject(name) {
		type = GameObject::Type::SPOTLIGHTOBJECT;
	}
	virtual void sendToSSBO(int index, ShaderStorageBuffer ssbo) override;

	static const int glslSize = 64;
};

void SpotLightObject::sendToSSBO(int index, ShaderStorageBuffer ssbo) {
	auto transform = getComponent<Transform>();
	auto spotLight = getComponent<SpotLightComponent>();
	glm::mat4 rotation(1.0);
	rotation = glm::rotate(rotation, glm::radians(transform->rotate.z), glm::vec3(0, 0, 1));
	rotation = glm::rotate(rotation, glm::radians(transform->rotate.y), glm::vec3(0, 1, 0));
	rotation = glm::rotate(rotation, glm::radians(transform->rotate.x), glm::vec3(1, 0, 0));
	glm::vec3 direction(1.0, 0.0, 0.0);
	direction = glm::mat3(rotation) * direction;
	float cutOff = glm::cos(glm::radians(spotLight->cutOff));
	float outerCutOff = glm::cos(glm::radians(spotLight->outerCutOff));
	ssbo.bind();
	ssbo.bufferSubdata(index * glslSize, 12, glm::value_ptr(transform->translate));
	ssbo.bufferSubdata(index * glslSize + 12, 4, nullptr);
	ssbo.bufferSubdata(index * glslSize + 16, 12, glm::value_ptr(direction));
	ssbo.bufferSubdata(index * glslSize + 28, 4, nullptr);
	ssbo.bufferSubdata(index * glslSize + 32, 12, glm::value_ptr(spotLight->color));
	ssbo.bufferSubdata(index * glslSize + 44, 4, nullptr);
	ssbo.bufferSubdata(index * glslSize + 48, 4, &cutOff);
	ssbo.bufferSubdata(index * glslSize + 52, 4, &outerCutOff);
	ssbo.bufferSubdata(index * glslSize + 56, 8, nullptr);
	ssbo.unbind();
}

class SkyBoxObject : public GameObject {
public:
	SkyBoxObject(std::string name) : GameObject(name) {
		type = GameObject::Type::SKYBOXOBJECT;
	}
	virtual void draw(ShaderPtr shader) override;
	virtual void useCubeMap(ShaderPtr shader) override;
};

void SkyBoxObject::draw(ShaderPtr shader) {
	shader->setInt("skybox", 5);
	if (auto skyboxComponent = getComponent<SkyBoxComponent>()) {
		if (skyboxComponent->skybox) {
			skyboxComponent->skybox->draw();
		}
	}
}

void SkyBoxObject::useCubeMap(ShaderPtr shader) {
	shader->setInt("skybox", 5);
	if (auto skyboxComponent = getComponent<SkyBoxComponent>()) {
		if (skyboxComponent->skybox) {
			skyboxComponent->skybox->useCubeMap();
		}
	}
}
#endif // !GAMEOBJECT_HPP