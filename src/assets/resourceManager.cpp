#include "resourceManager.hpp"
#include "shaderCatalog.hpp"
#include "scene/component.hpp"

namespace togr {

ResourceManager::ResourceManager() : currentScene_(std::make_shared<Scene>()) {}

void ResourceManager::init() {
    for (const auto& program : ShaderCatalog::programs) {
        if (program.geometryPath) {
            shaderLoader_.registerShader(program.key, program.vertexPath, program.geometryPath, program.fragmentPath);
        } else {
            shaderLoader_.registerShader(program.key, program.vertexPath, program.fragmentPath);
        }
    }
}

void ResourceManager::update() {
    modelLoader_.update();
    currentScene_->processRemovals();
}

void ResourceManager::shutdown() {
    currentScene_->shutdown();
    modelLoader_.shutdown();
    shaderLoader_.shutdown();
}

void ResourceManager::queueModelLoad(const std::string& path) {
    modelLoader_.requestLoad(path);
}

std::vector<std::shared_ptr<Model>> ResourceManager::getAllModels() const {
    return modelLoader_.getAllLoadedModels();
}

std::shared_ptr<Shader> ResourceManager::getShader(const std::string& key) const {
    return shaderLoader_.get(key);
}

ScenePtr ResourceManager::getCurrentScene() const {
    return currentScene_;
}

void ResourceManager::setCurrentScene(ScenePtr scene) {
    currentScene_ = scene ? scene : std::make_shared<Scene>();
}

bool ResourceManager::addGameObject(GameObjectPtr object) {
    return currentScene_->addObject(object);
}

void ResourceManager::removeGameObject(std::size_t index) {
    currentScene_->queueRemoval(index);
}

std::vector<GameObjectPtr> ResourceManager::getGameObjects() const {
    return currentScene_->getObjects();
}

int ResourceManager::getPointLightCount() const {
    return static_cast<int>(currentScene_->getView<PointLightComponent>().size());
}

int ResourceManager::getDirectionalLightCount() const {
    return static_cast<int>(currentScene_->getView<DirectionalLightComponent>().size());
}

int ResourceManager::getSpotLightCount() const {
    return static_cast<int>(currentScene_->getView<SpotLightComponent>().size());
}

} // namespace togr
