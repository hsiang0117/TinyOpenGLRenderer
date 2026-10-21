#ifndef MODELLOADER_HPP
#define MODELLOADER_HPP
#pragma once

#include <future>
#include <memory>
#include <queue>
#include <string>
#include <unordered_map>
#include <vector>

class Model;

// Imports models on a worker thread and publishes them through update().
// Call update(), addToCache(), and shutdown() while the GL context is current.
class ModelLoader {
public:
	ModelLoader() = default;

	std::future<std::shared_ptr<Model>> loadAsync(const std::string& path);
	void loadFromPath(const std::string& path);
	bool isLoaded(const std::string& key) const;
	std::shared_ptr<Model> get(const std::string& key) const;
	std::vector<std::shared_ptr<Model>> getAllLoadedModels() const;
	void addToCache(const std::string& key, std::shared_ptr<Model> model);
	void shutdown();
	void update();

private:
	std::unordered_map<std::string, std::shared_ptr<Model>> cache;
	std::future<std::shared_ptr<Model>> asyncFuture;
	std::queue<std::string> asyncQueue;
};

#endif // !MODELLOADER_HPP
