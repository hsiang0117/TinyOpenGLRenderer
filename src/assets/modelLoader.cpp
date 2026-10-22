#include "modelLoader.hpp"
#include "model.hpp"
#include <chrono>
#include <utility>

namespace togr {

void ModelLoader::requestLoad(const std::string& path) {
    if (!hasCachedModel(path)) {
        pendingPaths_.push(path);
    }
}

void ModelLoader::update() {
    collectCompletedImport();
    startNextImport();
}

void ModelLoader::shutdown() {
    discardActiveImport();
    modelCache_.clear();
    while (!pendingPaths_.empty()) {
        pendingPaths_.pop();
    }
}

std::vector<std::shared_ptr<Model>> ModelLoader::getAllLoadedModels() const {
    std::vector<std::shared_ptr<Model>> models;
    for (const auto& entry : modelCache_) {
        models.push_back(entry.second);
    }
    return models;
}

void ModelLoader::collectCompletedImport() {
    if (!activeImport_ ||
        activeImport_->future.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
        return;
    }

    // Consume this request before get()/GL publication, including when either
    // throws. The local result still releases partial GL resources on this thread.
    auto completed = std::move(*activeImport_);
    activeImport_.reset();
    auto model = completed.future.get();
    publishImportedModel(completed.cacheKey, std::move(model));
}

void ModelLoader::startNextImport() {
    if (activeImport_ || pendingPaths_.empty()) {
        return;
    }

    if (hasCachedModel(pendingPaths_.front())) {
        // Preserve one pending-head action per update, even for duplicates.
        pendingPaths_.pop();
        return;
    }

    // Copy the cache key and launch successfully before consuming the request.
    // A path conversion or launch exception leaves the pending head unchanged.
    ActiveImport next{pendingPaths_.front(), {}};
    next.future = Model::importAsync(next.cacheKey.c_str());
    activeImport_.emplace(std::move(next));
    pendingPaths_.pop();
}

void ModelLoader::discardActiveImport() {
    if (!activeImport_) {
        return;
    }
    activeImport_->future.wait();
    // No get(): shutdown discards the CPU-only result and any worker exception.
    activeImport_.reset();
}

bool ModelLoader::hasCachedModel(const std::string& key) const {
    return modelCache_.find(key) != modelCache_.end();
}

void ModelLoader::publishImportedModel(const std::string& key, std::shared_ptr<Model> model) {
    if (model && model->initGLResources()) {
        modelCache_[key] = std::move(model);
    }
}

} // namespace togr
