#include "gameObjectFactory.hpp"
#include "component.hpp"
#include "assets/model.hpp"
#include "rendering/geometry/meshGenerator.hpp"

GameObjectPtr GameObjectFactory::createPointLight() {
	auto obj = std::make_shared<GameObject>("PointLight");
	obj->addComponent<Transform>()->scale = glm::vec3(0.2f);
	obj->addComponent<PointLightComponent>();
	obj->addComponent<ShadowCasterCube>();
	obj->addComponent<StaticMeshComponent>()->setMesh(MeshGenerator::generateCube());
	return obj;
}

GameObjectPtr GameObjectFactory::createDirectionalLight() {
	auto obj = std::make_shared<GameObject>("DirectionalLight");
	obj->addComponent<Transform>();
	obj->addComponent<DirectionalLightComponent>();
	obj->addComponent<ShadowCaster2D>();
	return obj;
}

GameObjectPtr GameObjectFactory::createSpotLight() {
	auto obj = std::make_shared<GameObject>("SpotLight");
	obj->addComponent<Transform>();
	obj->addComponent<SpotLightComponent>();
	return obj;
}

GameObjectPtr GameObjectFactory::createCube() {
	auto obj = std::make_shared<GameObject>("Cube");
	obj->addComponent<Transform>();
	obj->addComponent<StaticMeshComponent>()->setMesh(MeshGenerator::generateCube());
	obj->addComponent<DynamicMaterialComponent>();
	return obj;
}

GameObjectPtr GameObjectFactory::createPlane() {
	auto obj = std::make_shared<GameObject>("Plane");
	obj->addComponent<Transform>();
	obj->addComponent<StaticMeshComponent>()->setMesh(MeshGenerator::generatePlane());
	obj->addComponent<DynamicMaterialComponent>();
	return obj;
}

GameObjectPtr GameObjectFactory::createSphere() {
	auto obj = std::make_shared<GameObject>("Sphere");
	obj->addComponent<Transform>();
	obj->addComponent<StaticMeshComponent>()->setMesh(MeshGenerator::generateSphere());
	obj->addComponent<DynamicMaterialComponent>();
	return obj;
}

GameObjectPtr GameObjectFactory::createSkybox() {
	auto obj = std::make_shared<GameObject>("Skybox");
	obj->addComponent<SkyboxComponent>();
	return obj;
}

GameObjectPtr GameObjectFactory::createCloudVolume() {
	auto obj = std::make_shared<GameObject>("Sky Cloud Layer");
	// Sky-layer altitude/thickness live in CloudVolumeComponent. Retain a local box
	// so switching the component to LocalVolume still has usable bounds.
	auto transform = obj->addComponent<Transform>();
	transform->translate = glm::vec3(0.0f, 200.0f, 0.0f);
	transform->scale = glm::vec3(5000.0f, 200.0f, 5000.0f);
	obj->addComponent<StaticMeshComponent>()->setMesh(MeshGenerator::generateCube());
	obj->addComponent<CloudVolumeComponent>();
	return obj;
}

GameObjectPtr GameObjectFactory::createFromModel(ModelPtr model) {
	auto obj = std::make_shared<GameObject>(model->getName());
	obj->addComponent<Transform>();
	obj->addComponent<ModelRenderComponent>()->setModel(model);
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
