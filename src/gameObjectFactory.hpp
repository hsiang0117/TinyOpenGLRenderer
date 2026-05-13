#ifndef GAMEOBJECTFACTORY_HPP
#define GAMEOBJECTFACTORY_HPP
#pragma once

#include "gameObject.hpp"
#include "component.hpp"
#include "meshGenerator.hpp"
#include "model.hpp"

class GameObjectFactory {
public:
	static GameObjectPtr createPointLight();
	static GameObjectPtr createDirectionLight();
	static GameObjectPtr createSpotLight();
	static GameObjectPtr createCube();
	static GameObjectPtr createPlane();
	static GameObjectPtr createSphere();
	static GameObjectPtr createSkyBox();
	static GameObjectPtr createVolumeCloud();
	static GameObjectPtr createFromModel(ModelPtr model);
};

inline GameObjectPtr GameObjectFactory::createPointLight() {
	auto obj = std::make_shared<GameObject>("PointLight");
	obj->addComponent<Transform>()->scale = glm::vec3(0.2f);
	obj->addComponent<PointLightComponent>();
	obj->addComponent<ShadowCasterCube>();
	obj->addComponent<StaticMeshComponent>()->setMesh(MeshGenerator::generateCube());
	return obj;
}

inline GameObjectPtr GameObjectFactory::createDirectionLight() {
	auto obj = std::make_shared<GameObject>("DirectionLight");
	obj->addComponent<Transform>();
	obj->addComponent<DirectionLightComponent>();
	obj->addComponent<ShadowCaster2D>();
	return obj;
}

inline GameObjectPtr GameObjectFactory::createSpotLight() {
	auto obj = std::make_shared<GameObject>("SpotLight");
	obj->addComponent<Transform>();
	obj->addComponent<SpotLightComponent>();
	return obj;
}

inline GameObjectPtr GameObjectFactory::createCube() {
	auto obj = std::make_shared<GameObject>("Cube");
	obj->addComponent<Transform>();
	obj->addComponent<StaticMeshComponent>()->setMesh(MeshGenerator::generateCube());
	obj->addComponent<DynamicMaterialComponent>();
	return obj;
}

inline GameObjectPtr GameObjectFactory::createPlane() {
	auto obj = std::make_shared<GameObject>("Plane");
	obj->addComponent<Transform>();
	obj->addComponent<StaticMeshComponent>()->setMesh(MeshGenerator::generatePlane());
	obj->addComponent<DynamicMaterialComponent>();
	return obj;
}

inline GameObjectPtr GameObjectFactory::createSphere() {
	auto obj = std::make_shared<GameObject>("Sphere");
	obj->addComponent<Transform>();
	obj->addComponent<StaticMeshComponent>()->setMesh(MeshGenerator::generateSphere());
	obj->addComponent<DynamicMaterialComponent>();
	return obj;
}

inline GameObjectPtr GameObjectFactory::createSkyBox() {
	auto obj = std::make_shared<GameObject>("SkyBox");
	obj->addComponent<SkyBoxComponent>();
	return obj;
}

inline GameObjectPtr GameObjectFactory::createVolumeCloud() {
	auto obj = std::make_shared<GameObject>("Volume");
	obj->addComponent<Transform>();
	obj->addComponent<StaticMeshComponent>()->setMesh(MeshGenerator::generateCube());
	obj->addComponent<VolumeMarkerComponent>();
	return obj;
}

inline GameObjectPtr GameObjectFactory::createFromModel(ModelPtr model) {
	auto obj = std::make_shared<GameObject>(model->getName());
	obj->addComponent<Transform>();
	obj->addComponent<RenderComponent>()->setModel(model);
	obj->addComponent<SkeletonViewerComponent>(model->getNodes());
	if (!model->getAnimations().empty()) {
		auto skeleton = obj->getComponent<SkeletonViewerComponent>();
		auto animator = obj->addComponent<AnimatorComponent>(skeleton->getNodes());
		animator->setAnimation(&model->getAnimations());
		animator->playAnimation(model->getAnimations()[0].getName());
		animator->update(0.0f);
	}
	return obj;
}

#endif // !GAMEOBJECTFACTORY_HPP
