#ifndef TOGR_RESOURCEMANAGER_HPP
#define TOGR_RESOURCEMANAGER_HPP
#pragma once

#include "modelLoader.hpp"
#include "shaderLoader.hpp"
#include "scene/scene.hpp"
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace togr {

class Model;
class Shader;

// Main-thread facade for asset loading and the active Scene. Owns both loaders
// and a non-null shared scene; object membership and views remain Scene's job.
class ResourceManager {
public:
    ResourceManager();
    ResourceManager(const ResourceManager&) = delete;
    ResourceManager& operator=(const ResourceManager&) = delete;

    // GL lifecycle: init registers built-in shaders and can throw after partial
    // registration. shutdown also handles that partial state. All three methods
    // require a current context; shutdown releases references, not external owners.
    void init();

    // Advances model import/publication on this thread, then commits removals.
    void update();
    void shutdown();

    // Assets: the original UTF-8 path bytes form the cache key. This call queues
    // work; getAllModels() sees it only after successful CPU import and GL upload.
    void queueModelLoad(const std::string& path);
    std::vector<std::shared_ptr<Model>> getAllModels() const;

    // Shared program, or nullptr for an unknown key. Retaining it extends its
    // lifetime; release external GL-resource owners before destroying the context.
    std::shared_ptr<Shader> getShader(const std::string& key) const;

    // Scene hand-off: nullptr selects a fresh empty scene. Returned shared scenes
    // may outlive a later switch; this does not merge or destroy their contents.
    ScenePtr getCurrentScene() const;
    void setCurrentScene(ScenePtr scene);

    // add is immediate and reports Scene's null/capacity rejection. remove queues
    // a current-list index until update(); it does not remove synchronously.
    bool addGameObject(GameObjectPtr object);
    void removeGameObject(std::size_t index);

    // Object snapshots share ownership; the typed view is borrowed from Scene
    // and follows Scene::getView()'s membership and invalidation contract.
    std::vector<GameObjectPtr> getGameObjects() const;
    template<typename T>
    const std::vector<GameObjectPtr>& getSceneView() const {
        return currentScene_->getView<T>();
    }

    int getPointLightCount() const;
    int getDirectionalLightCount() const;
    int getSpotLightCount() const;

private:
    ModelLoader modelLoader_;
    ShaderLoader shaderLoader_;
    ScenePtr currentScene_;
};

} // namespace togr

#endif // !TOGR_RESOURCEMANAGER_HPP
