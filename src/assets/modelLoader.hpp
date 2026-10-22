#ifndef TOGR_MODELLOADER_HPP
#define TOGR_MODELLOADER_HPP
#pragma once

#include <future>
#include <memory>
#include <optional>
#include <queue>
#include <string>
#include <unordered_map>
#include <vector>

namespace togr {

class Model;

// Single-owner-thread coordinator; its worker only imports CPU data.
// update() publishes GL resources, so it and shutdown() need the current GL context.
class ModelLoader {
public:
    ModelLoader() = default;
    // Keep the coordinator and its active-task invariant at a stable identity.
    ModelLoader(const ModelLoader&) = delete;
    ModelLoader& operator=(const ModelLoader&) = delete;
    ModelLoader(ModelLoader&&) = delete;
    ModelLoader& operator=(ModelLoader&&) = delete;

    // Queue an owned UTF-8 path. Cache keys compare the original bytes without
    // normalization. Cached paths are ignored; pending/active duplicates remain.
    // This call does not start an import or perform GL work.
    void requestLoad(const std::string& path);

    // Collect at most one completed import, then handle one pending head: start it
    // or discard a cached duplicate. CPU import precedes this thread's GL upload.
    // Ordinary import/upload failures are not cached, so later requests can retry.
    // Unexpected collection/publication exceptions consume the active request and
    // propagate; launch failures leave the pending request available for retry.
    void update();

    // Snapshot of shared owners, not borrowed pointers. External owners must release
    // initialized models while the GL context is current, even after shutdown().
    std::vector<std::shared_ptr<Model>> getAllLoadedModels() const;

    // Wait without publishing or rethrowing worker errors, then discard the CPU
    // result and clear requests/cache. Repeated calls are harmless;
    // externally shared models keep their resources until their last owner releases.
    void shutdown();

private:
    struct ActiveImport {
        std::string cacheKey;
        std::future<std::shared_ptr<Model>> future;
    };

    void collectCompletedImport();
    void startNextImport();
    void discardActiveImport();
    bool hasCachedModel(const std::string& key) const;
    void publishImportedModel(const std::string& key, std::shared_ptr<Model> model);

    std::unordered_map<std::string, std::shared_ptr<Model>> modelCache_;
    // Active state always pairs its original key with a valid future. Pending
    // paths have not started, and neither container holds published models.
    std::optional<ActiveImport> activeImport_;
    std::queue<std::string> pendingPaths_;
};

} // namespace togr

#endif // !TOGR_MODELLOADER_HPP
