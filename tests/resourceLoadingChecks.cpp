#include "assets/modelLoader.hpp"
#include "assets/model.hpp"
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace togr::tests {

namespace {
namespace fs = std::filesystem;
using Models = std::vector<std::shared_ptr<Model>>;

void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void writeTriangle(const fs::path& path, int x) {
    std::ofstream output(path);
    output << "ply\nformat ascii 1.0\nelement vertex 3\n"
        "property float x\nproperty float y\nproperty float z\n"
        "element face 1\nproperty list uchar int vertex_indices\nend_header\n"
        << x << " 2 3\n" << x + 3 << " 2 3\n" << x << " 5 3\n3 0 1 2\n";
    check(bool(output), "Model-loader fixture write failed");
}

std::shared_ptr<Model> findModel(const Models& models, const std::string& path) {
    for (const auto& model : models) {
        if (fs::u8path(model->getPath()) == fs::u8path(path)) return model;
    }
    return nullptr;
}

void checkTriangle(const std::shared_ptr<Model>& model, int x) {
    check(model && model->isReady(), "Model loader exposed an uninitialized model");
    glm::vec3 minimum, maximum;
    model->buildAABB(minimum, maximum);
    const glm::vec3 expectedMinimum(x, 2, 3), expectedMaximum(x + 3, 5, 3);
    for (int axis = 0; axis < 3; ++axis) {
        check(std::isfinite(minimum[axis]) && std::isfinite(maximum[axis]) &&
            std::abs(minimum[axis] - expectedMinimum[axis]) < 0.0001f &&
            std::abs(maximum[axis] - expectedMaximum[axis]) < 0.0001f,
            "Model loader associated a request with the wrong imported geometry");
    }
}

template<class Predicate>
Models waitForModels(ModelLoader& loader, Predicate ready, const char* timeoutMessage) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
    for (;;) {
        const auto previousCount = loader.getAllLoadedModels().size();
        loader.update();
        auto models = loader.getAllLoadedModels();
        check(models.size() <= previousCount + 1, "One model-loader update published multiple requests");
        for (const auto& model : models) {
            check(model && model->isReady(), "Model loader published before GL initialization");
        }
        if (ready(models)) return models;
        check(std::chrono::steady_clock::now() < deadline, timeoutMessage);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}
}

// Called with renderCorrectness's current GL context; workers only import CPU data.
void runResourceLoadingChecks() {
    const auto base = fs::absolute("x64/RenderChecks/resource-loading");
    fs::create_directories(base);
    const auto directory = base / std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    check(fs::create_directory(directory), "Cannot create isolated model-loader fixtures");
    const auto aPath = (directory / "a.ply").generic_u8string();
    const auto bPath = (directory / "b.ply").generic_u8string();
    const auto retryPath = (directory / "retry.ply").generic_u8string();
    const auto sentinelPath = (directory / "sentinel.ply").generic_u8string();
    // Keep the lexical dot: the physical file is shared, but cache keys are exact.
    const auto aliasPath = (directory / "." / "a.ply").generic_u8string();
    writeTriangle(fs::u8path(aPath), 1);
    writeTriangle(fs::u8path(bPath), 20);
    writeTriangle(fs::u8path(sentinelPath), 70);
    {
        const auto cpuModel = Model::importAsync(aPath.c_str()).get();
        check(cpuModel && !cpuModel->isReady(), "CPU import unexpectedly published GL resources");
    }

    std::shared_ptr<Model> survivor;
    std::weak_ptr<Model> observer;
    {
        ModelLoader loader;
        {
            loader.requestLoad(aPath);
            loader.requestLoad(aPath);
            loader.requestLoad(bPath);
            check(loader.getAllLoadedModels().empty(), "Requesting a model published it without update");
            loader.update();
            check(loader.getAllLoadedModels().empty(), "Starting an import also published its result");
            const auto loaded = waitForModels(loader, [&](const Models& models) {
                return findModel(models, bPath) != nullptr;
            }, "A/A/B model-loader requests timed out");
            check(loaded.size() == 2, "Duplicate model requests created extra cache entries");
            const auto a = findModel(loaded, aPath), b = findModel(loaded, bPath);
            checkTriangle(a, 1);
            checkTriangle(b, 20);

            // Alias completion also marks progress past both cached requests.
            loader.requestLoad(aPath);
            loader.requestLoad(bPath);
            loader.requestLoad(aliasPath);
            const auto aliased = waitForModels(loader, [&](const Models& models) {
                return findModel(models, aliasPath) != nullptr;
            }, "Exact-key model alias timed out");
            check(aliased.size() == 3, "Model cache normalized an exact path key or repeated a cached import");
            check(findModel(aliased, aPath) == a && findModel(aliased, bPath) == b,
                "Repeated model requests replaced cached shared instances");
            const auto alias = findModel(aliased, aliasPath);
            check(alias != a, "Lexically different model keys shared a cache entry");
            checkTriangle(alias, 1);

            // A valid later request proves the missing request has been consumed.
            loader.requestLoad(retryPath);
            loader.requestLoad(sentinelPath);
            const auto afterFailure = waitForModels(loader, [&](const Models& models) {
                return findModel(models, sentinelPath) != nullptr;
            }, "Model loader did not advance after a missing file");
            check(afterFailure.size() == 4 && !findModel(afterFailure, retryPath),
                "Failed model import was cached or displaced another request");
            checkTriangle(findModel(afterFailure, sentinelPath), 70);
            writeTriangle(fs::u8path(retryPath), 40);
            loader.requestLoad(retryPath);
            const auto retried = waitForModels(loader, [&](const Models& models) {
                return findModel(models, retryPath) != nullptr;
            }, "Explicit retry after a failed model import timed out");
            check(retried.size() == 5, "Retry lost or duplicated a model cache entry");
            checkTriangle(findModel(retried, retryPath), 40);
            survivor = a;
            observer = a;
        }
        // All cache snapshots are gone; this owner alone must survive shutdown.
        loader.shutdown();
        loader.shutdown();
        loader.update();
        check(loader.getAllLoadedModels().empty(), "Model-loader shutdown retained published or pending models");
        check(!observer.expired(), "Model-loader shutdown invalidated an external shared owner");
        checkTriangle(survivor, 1);
        survivor.reset();
        check(observer.expired(), "Model loader retained a model after its last external owner released");
    }

    {
        ModelLoader loader;
        loader.requestLoad(aPath);
        loader.update();
        loader.requestLoad(bPath);
        // Discard an active CPU result and a separate request that has not started.
        loader.shutdown();
        loader.shutdown();
        loader.update();
        check(loader.getAllLoadedModels().empty(), "Shutdown published an active or queued import");
        loader.requestLoad(sentinelPath);
        const auto reused = waitForModels(loader, [&](const Models& models) {
            return findModel(models, sentinelPath) != nullptr;
        }, "Model loader could not be reused after discarding requests");
        check(reused.size() == 1 && !findModel(reused, aPath) && !findModel(reused, bPath),
            "Discarded requests survived model-loader reuse");
        checkTriangle(findModel(reused, sentinelPath), 70);
        loader.shutdown();
    }
    std::cout << "PASS: model-loader request ownership, exact cache keys, retry, publication, and shutdown reuse\n";
}

} // namespace togr::tests
