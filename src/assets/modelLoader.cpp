#include "modelLoader.hpp"
#include "model.hpp"
#include <chrono>

std::future<std::shared_ptr<Model>> ModelLoader::loadAsync(const std::string& path)
{
	return Model::LoadAsync(path.c_str());
}

void ModelLoader::loadFromPath(const std::string& path)
{
	if (!isLoaded(path)) {
		asyncQueue.push(path);
	}
}

bool ModelLoader::isLoaded(const std::string& key) const
{
	return cache.find(key) != cache.end();
}

std::vector<std::shared_ptr<Model>> ModelLoader::getAllLoadedModels() const
{
	std::vector<std::shared_ptr<Model>> models;
	for (const auto& pair : cache) {
		models.push_back(pair.second);
	}
	return models;
}

void ModelLoader::addToCache(const std::string& key, std::shared_ptr<Model> model)
{
	if (model && model->initGLResources()) {
		cache[key] = model;
	}
}

void ModelLoader::shutdown()
{
	if (asyncFuture.valid()) {
		asyncFuture.wait();
		// Discard the CPU-only result before the GL context is destroyed.
		asyncFuture = {};
	}
	cache.clear();
	while (!asyncQueue.empty()) asyncQueue.pop();
}

void ModelLoader::update()
{
	// Collect the CPU result and initialize its GL resources on this thread.
	if (asyncFuture.valid() && asyncFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
		auto model = asyncFuture.get();
		if (!asyncQueue.empty()) {
			auto path = asyncQueue.front();
			asyncQueue.pop();
			addToCache(path, model);
		}
	}
	// Start the next queued CPU import.
	if ((!asyncFuture.valid() || asyncFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
		&& !asyncQueue.empty()) {
		auto path = asyncQueue.front();
		if (!isLoaded(path)) {
			asyncFuture = loadAsync(path);
		} else {
			asyncQueue.pop();
		}
	}
}
