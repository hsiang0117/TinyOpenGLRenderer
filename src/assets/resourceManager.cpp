#include "resourceManager.hpp"
#include "shaderCatalog.hpp"
#include "scene/component.hpp"

ResourceManager::ResourceManager()
{
	currentScene = std::make_shared<Scene>();
}

void ResourceManager::init()
{
	for (const auto& program : ShaderCatalog::programs) {
		if (program.geometryPath) {
			shaderLoader.registerShader(program.key, program.vertexPath, program.geometryPath, program.fragmentPath);
		} else {
			shaderLoader.registerShader(program.key, program.vertexPath, program.fragmentPath);
		}
	}
}

void ResourceManager::update()
{
	modelLoader.update();
	currentScene->processRemovals();
}

void ResourceManager::queueModelLoad(const std::string& path)
{
	modelLoader.loadFromPath(path);
}

std::vector<std::shared_ptr<Model>> ResourceManager::getAllModels() const
{
	return modelLoader.getAllLoadedModels();
}

std::shared_ptr<Shader> ResourceManager::getShader(const std::string& key) const
{
	return shaderLoader.get(key);
}

bool ResourceManager::addGameObject(GameObjectPtr obj)
{
	return currentScene->addObject(obj);
}

void ResourceManager::removeGameObject(std::size_t idx)
{
	currentScene->queueRemoval(idx);
}

std::vector<GameObjectPtr> ResourceManager::getGameObjects() const
{
	return currentScene->getObjects();
}

int ResourceManager::getPointLightCount() const
{
	return (int)currentScene->getView<PointLightComponent>().size();
}

int ResourceManager::getDirectionalLightCount() const
{
	return (int)currentScene->getView<DirectionalLightComponent>().size();
}

int ResourceManager::getSpotLightCount() const
{
	return (int)currentScene->getView<SpotLightComponent>().size();
}

ScenePtr ResourceManager::getCurrentScene() const
{
	return currentScene;
}

void ResourceManager::setCurrentScene(ScenePtr s)
{
	currentScene = s ? s : std::make_shared<Scene>();
}

void ResourceManager::shutdown()
{
	currentScene->shutdown();
	modelLoader.shutdown();
	shaderLoader.shutdown();
}
