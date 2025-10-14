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
	virtual void drawSkeleton(ShaderPtr shader) {}
	virtual void sendToSSBO(int index, ShaderStorageBuffer ssbo) {}
	virtual glm::mat4 getLightMatrices() { return glm::mat4(1.0f); }
	virtual std::vector<glm::mat4> getLightMatricesCube() { return std::vector<glm::mat4>(); }
	virtual void useCubeMap(ShaderPtr shader) {}
	virtual bool isOnFrustum(Frustum& frustum) { return false; }

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
	void draw(ShaderPtr shader) override;
	void drawSkeleton(ShaderPtr shader) override;
	bool isOnFrustum(Frustum& frustum) override;
};

void RenderObject::draw(ShaderPtr shader) {
	if (auto renderComponent = getComponent<RenderComponent>()) {
		glm::mat4 model = glm::mat4(1.0f);
		if (auto transform = getComponent<Transform>()) {
			model = glm::translate(model, transform->translate);
			model = glm::rotate(model, glm::radians(transform->rotate.z), glm::vec3(0, 0, 1));
			model = glm::rotate(model, glm::radians(transform->rotate.y), glm::vec3(0, 1, 0));
			model = glm::rotate(model, glm::radians(transform->rotate.x), glm::vec3(1, 0, 0));
			model = glm::scale(model, transform->scale);
			shader.get()->setMat4("model", model);
		}
		if (auto animator = getComponent<AnimatorComponent>()) {
			if (renderComponent->model && renderComponent->model->hasAnimation()) {
				auto boneMatrices = animator->getFinalBoneMatrices();
				for (int i = 0; i < boneMatrices.size(); i++) {
					shader.get()->setMat4(("finalBoneMatrices[" + std::to_string(i) + "]").c_str(), boneMatrices[i]);
				}
			}
		}
		if (renderComponent->model) {
			renderComponent->model->draw(shader);
		}
	}
}

void RenderObject::drawSkeleton(ShaderPtr shader) {
	if (auto renderComponent = getComponent<RenderComponent>()) {
		glm::mat4 model = glm::mat4(1.0f);
		if (auto transform = getComponent<Transform>()) {
			model = glm::translate(model, transform->translate);
			model = glm::rotate(model, glm::radians(transform->rotate.z), glm::vec3(0, 0, 1));
			model = glm::rotate(model, glm::radians(transform->rotate.y), glm::vec3(0, 1, 0));
			model = glm::rotate(model, glm::radians(transform->rotate.x), glm::vec3(1, 0, 0));
			model = glm::scale(model, transform->scale);
			shader.get()->setMat4("model", model);
			if (renderComponent->model && renderComponent->skeletonVisible) {
				renderComponent->model->drawBones();
			}
		}
	}
}

bool RenderObject::isOnFrustum(Frustum& frustum) {
	if (auto renderComponent = getComponent<RenderComponent>()) {
		glm::mat4 model = glm::mat4(1.0f);
		if (auto transform = getComponent<Transform>()) {
			model = glm::translate(model, transform->translate);
			model = glm::rotate(model, glm::radians(transform->rotate.z), glm::vec3(0, 0, 1));
			model = glm::rotate(model, glm::radians(transform->rotate.y), glm::vec3(0, 1, 0));
			model = glm::rotate(model, glm::radians(transform->rotate.x), glm::vec3(1, 0, 0));
			model = glm::scale(model, transform->scale);

			glm::vec3 minAABB = renderComponent->aabb.min;
			glm::vec3 maxAABB = renderComponent->aabb.max;

			// 8个顶点
			glm::vec3 vertices[8] = {
				glm::vec3(minAABB.x, minAABB.y, minAABB.z),
				glm::vec3(maxAABB.x, minAABB.y, minAABB.z),
				glm::vec3(minAABB.x, maxAABB.y, minAABB.z),
				glm::vec3(maxAABB.x, maxAABB.y, minAABB.z),
				glm::vec3(minAABB.x, minAABB.y, maxAABB.z),
				glm::vec3(maxAABB.x, minAABB.y, maxAABB.z),
				glm::vec3(minAABB.x, maxAABB.y, maxAABB.z),
				glm::vec3(maxAABB.x, maxAABB.y, maxAABB.z)
			};

			// 变换到世界空间
			for (int i = 0; i < 8; ++i) {
				glm::vec4 v = model * glm::vec4(vertices[i], 1.0f);
				vertices[i] = glm::vec3(v);
			}

			// 检查AABB是否在视锥体内
			glm::vec4 planes[6] = {
				frustum.leftPlane, frustum.rightPlane,
				frustum.bottomPlane, frustum.topPlane,
				frustum.nearPlane, frustum.farPlane
			};

			for (int p = 0; p < 6; ++p) {
				int out = 0;
				for (int i = 0; i < 8; ++i) {
					const glm::vec4& plane = planes[p];
					if (plane.x * vertices[i].x + plane.y * vertices[i].y + plane.z * vertices[i].z + plane.w < 0)
						out++;
				}
				// 如果所有点都在某个平面外，则不在视锥体内
				if (out == 8)
					return false;
			}
			return true;
		}
	}
	return false;
}

class PointLightObject : public GameObject {
public:
	PointLightObject(std::string name) : GameObject(name) {
		type = GameObject::Type::POINTLIGHTOBJECT;
	}
	void sendToSSBO(int index, ShaderStorageBuffer ssbo) override;
	std::vector<glm::mat4> getLightMatricesCube() override;
	void draw(ShaderPtr shader) override;
	bool isOnFrustum(Frustum& frustum) override;

	static const int glslSize = 48;
};

void PointLightObject::sendToSSBO(int index, ShaderStorageBuffer ssbo) {
	auto transform = getComponent<Transform>();
	auto pointLight = getComponent<PointLightComponent>();
	auto shadowCaster = getComponent<ShadowCasterCube>();
	ssbo.bind();
	ssbo.bufferSubdata(index * glslSize, 12, glm::value_ptr(transform->translate));
	ssbo.bufferSubdata(index * glslSize + 12, 4, nullptr);
	ssbo.bufferSubdata(index * glslSize + 16, 12, glm::value_ptr(pointLight->color));
	ssbo.bufferSubdata(index * glslSize + 28, 4, &pointLight->brightness);
	ssbo.bufferSubdata(index * glslSize + 32, 4, &pointLight->constant);
	ssbo.bufferSubdata(index * glslSize + 36, 4, &pointLight->linear);
	ssbo.bufferSubdata(index * glslSize + 40, 4, &pointLight->quadratic);
	ssbo.bufferSubdata(index * glslSize + 44, 4, &shadowCaster->farPlane);
	ssbo.unbind();
}

std::vector<glm::mat4> PointLightObject::getLightMatricesCube() {
	auto transform = getComponent<Transform>();
	glm::vec3 position = transform->translate;
	auto shadowCaster = getComponent<ShadowCasterCube>();
	glm::mat4 lightProjection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, shadowCaster->farPlane);
	std::vector<glm::mat4> lightViews;
	lightViews.push_back(lightProjection * glm::lookAt(position, position + glm::vec3(1.0, 0.0, 0.0), glm::vec3(0.0, -1.0, 0.0)));
	lightViews.push_back(lightProjection * glm::lookAt(position, position + glm::vec3(-1.0, 0.0, 0.0), glm::vec3(0.0, -1.0, 0.0)));
	lightViews.push_back(lightProjection * glm::lookAt(position, position + glm::vec3(0.0, 1.0, 0.0), glm::vec3(0.0, 0.0, 1.0)));
	lightViews.push_back(lightProjection * glm::lookAt(position, position + glm::vec3(0.0, -1.0, 0.0), glm::vec3(0.0, 0.0, -1.0)));
	lightViews.push_back(lightProjection * glm::lookAt(position, position + glm::vec3(0.0, 0.0, 1.0), glm::vec3(0.0, -1.0, 0.0)));
	lightViews.push_back(lightProjection * glm::lookAt(position, position + glm::vec3(0.0, 0.0, -1.0), glm::vec3(0.0, -1.0, 0.0)));
	return lightViews;
}

void PointLightObject::draw(ShaderPtr shader) {
	if (auto staticMeshComponent = getComponent<StaticMeshComponent>()) {
		glm::mat4 model = glm::mat4(1.0f);
		if (auto transform = getComponent<Transform>()) {
			model = glm::translate(model, transform->translate);
			model = glm::rotate(model, glm::radians(transform->rotate.z), glm::vec3(0, 0, 1));
			model = glm::rotate(model, glm::radians(transform->rotate.y), glm::vec3(0, 1, 0));
			model = glm::rotate(model, glm::radians(transform->rotate.x), glm::vec3(1, 0, 0));
			model = glm::scale(model, transform->scale);
		}
		shader->setMat4("model", model);
		glm::vec3 color = glm::vec3(1.0f, 1.0f, 1.0f);
		float brightness = 1.0f;
		if (auto pointLight = getComponent<PointLightComponent>()) {
			color = pointLight->color;
			brightness = pointLight->brightness;
		}
		shader->setVec3("color", color);
		shader->setFloat("brightness", brightness);
		if (staticMeshComponent->mesh) {
			staticMeshComponent->mesh->draw();
		}
	}
}

bool PointLightObject::isOnFrustum(Frustum& frustum) {
	if (auto staticMeshComponent = getComponent<StaticMeshComponent>()) {
		glm::mat4 model = glm::mat4(1.0f);
		if (auto transform = getComponent<Transform>()) {
			model = glm::translate(model, transform->translate);
			model = glm::rotate(model, glm::radians(transform->rotate.z), glm::vec3(0, 0, 1));
			model = glm::rotate(model, glm::radians(transform->rotate.y), glm::vec3(0, 1, 0));
			model = glm::rotate(model, glm::radians(transform->rotate.x), glm::vec3(1, 0, 0));
			model = glm::scale(model, transform->scale);

			glm::vec3 minAABB = staticMeshComponent->aabb.min;
			glm::vec3 maxAABB = staticMeshComponent->aabb.max;

			// 8个顶点
			glm::vec3 vertices[8] = {
				glm::vec3(minAABB.x, minAABB.y, minAABB.z),
				glm::vec3(maxAABB.x, minAABB.y, minAABB.z),
				glm::vec3(minAABB.x, maxAABB.y, minAABB.z),
				glm::vec3(maxAABB.x, maxAABB.y, minAABB.z),
				glm::vec3(minAABB.x, minAABB.y, maxAABB.z),
				glm::vec3(maxAABB.x, minAABB.y, maxAABB.z),
				glm::vec3(minAABB.x, maxAABB.y, maxAABB.z),
				glm::vec3(maxAABB.x, maxAABB.y, maxAABB.z)
			};

			// 变换到世界空间
			for (int i = 0; i < 8; ++i) {
				glm::vec4 v = model * glm::vec4(vertices[i], 1.0f);
				vertices[i] = glm::vec3(v);
			}

			// 检查AABB是否在视锥体内
			glm::vec4 planes[6] = {
				frustum.leftPlane, frustum.rightPlane,
				frustum.bottomPlane, frustum.topPlane,
				frustum.nearPlane, frustum.farPlane
			};

			for (int p = 0; p < 6; ++p) {
				int out = 0;
				for (int i = 0; i < 8; ++i) {
					const glm::vec4& plane = planes[p];
					if (plane.x * vertices[i].x + plane.y * vertices[i].y + plane.z * vertices[i].z + plane.w < 0)
						out++;
				}
				// 如果所有点都在某个平面外，则不在视锥体内
				if (out == 8)
					return false;
			}
			return true;
		}
	}
	return false;
}

class DirectionLightObject : public GameObject {
public:
	DirectionLightObject(std::string name) : GameObject(name) {
		type = GameObject::Type::DIRECTIONLIGHTOBJECT;
	}
	void sendToSSBO(int index, ShaderStorageBuffer ssbo) override;
	glm::mat4 getLightMatrices() override;

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
	ssbo.bufferSubdata(28, 4, &directionLight->brightness);
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
	glm::mat4 lightProjection = glm::ortho(-100.0f, 100.0f, -100.0f, 100.0f, -100.0f, 100.0f);
	glm::mat4 lightView = glm::lookAt(-direction, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
	return lightProjection * lightView;
}

class SpotLightObject : public GameObject {
public:
	SpotLightObject(std::string name) : GameObject(name) {
		type = GameObject::Type::SPOTLIGHTOBJECT;
	}
	void sendToSSBO(int index, ShaderStorageBuffer ssbo) override;

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
	ssbo.bufferSubdata(index * glslSize + 44, 4, &spotLight->brightness);
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
	void draw(ShaderPtr shader) override;
	void useCubeMap(ShaderPtr shader) override;
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

class StaticMeshObject : public GameObject {
public:
	StaticMeshObject(std::string name) : GameObject(name) {
		type = GameObject::Type::RENDEROBJECT;
	}
	void draw(ShaderPtr shader) override;
	bool isOnFrustum(Frustum& frustum) override;
};

void StaticMeshObject::draw(ShaderPtr shader) {
	if (auto staticMeshComponent = getComponent<StaticMeshComponent>()) {
		if (auto dynamicMaterialComponent = getComponent<DynamicMaterialComponent>()) {
			dynamicMaterialComponent->material.bind(shader);
		}
		glm::mat4 model = glm::mat4(1.0f);
		if (auto transform = getComponent<Transform>()) {
			model = glm::translate(model, transform->translate);
			model = glm::rotate(model, glm::radians(transform->rotate.z), glm::vec3(0, 0, 1));
			model = glm::rotate(model, glm::radians(transform->rotate.y), glm::vec3(0, 1, 0));
			model = glm::rotate(model, glm::radians(transform->rotate.x), glm::vec3(1, 0, 0));
			model = glm::scale(model, transform->scale);
			shader.get()->setMat4("model", model);
			for (int i = 0; i < 100; i++) {
				shader.get()->setMat4(("finalBoneMatrices[" + std::to_string(i) + "]").c_str(), glm::mat4(1.0f));
			}
			if (staticMeshComponent->mesh) {
				staticMeshComponent->mesh->draw();
			}
		}
	}
}

bool StaticMeshObject::isOnFrustum(Frustum& frustum) {
	if (auto staticMeshComponent = getComponent<StaticMeshComponent>()) {
		glm::mat4 model = glm::mat4(1.0f);
		if (auto transform = getComponent<Transform>()) {
			model = glm::translate(model, transform->translate);
			model = glm::rotate(model, glm::radians(transform->rotate.z), glm::vec3(0, 0, 1));
			model = glm::rotate(model, glm::radians(transform->rotate.y), glm::vec3(0, 1, 0));
			model = glm::rotate(model, glm::radians(transform->rotate.x), glm::vec3(1, 0, 0));
			model = glm::scale(model, transform->scale);

			glm::vec3 minAABB = staticMeshComponent->aabb.min;
			glm::vec3 maxAABB = staticMeshComponent->aabb.max;

			// 8个顶点
			glm::vec3 vertices[8] = {
				glm::vec3(minAABB.x, minAABB.y, minAABB.z),
				glm::vec3(maxAABB.x, minAABB.y, minAABB.z),
				glm::vec3(minAABB.x, maxAABB.y, minAABB.z),
				glm::vec3(maxAABB.x, maxAABB.y, minAABB.z),
				glm::vec3(minAABB.x, minAABB.y, maxAABB.z),
				glm::vec3(maxAABB.x, minAABB.y, maxAABB.z),
				glm::vec3(minAABB.x, maxAABB.y, maxAABB.z),
				glm::vec3(maxAABB.x, maxAABB.y, maxAABB.z)
			};

			// 变换到世界空间
			for (int i = 0; i < 8; ++i) {
				glm::vec4 v = model * glm::vec4(vertices[i], 1.0f);
				vertices[i] = glm::vec3(v);
			}

			// 检查AABB是否在视锥体内
			glm::vec4 planes[6] = {
				frustum.leftPlane, frustum.rightPlane,
				frustum.bottomPlane, frustum.topPlane,
				frustum.nearPlane, frustum.farPlane
			};

			for (int p = 0; p < 6; ++p) {
				int out = 0;
				for (int i = 0; i < 8; ++i) {
					const glm::vec4& plane = planes[p];
					if (plane.x * vertices[i].x + plane.y * vertices[i].y + plane.z * vertices[i].z + plane.w < 0)
						out++;
				}
				// 如果所有点都在某个平面外，则不在视锥体内
				if (out == 8)
					return false;
			}
			return true;
		}
	}
	return false;
}
#endif // !GAMEOBJECT_HPP