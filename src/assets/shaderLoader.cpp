#include "shaderLoader.hpp"
#include "rendering/opengl/shader.hpp"
#include <stdexcept>
#include <utility>

bool ShaderLoader::isLoaded(const std::string& key) const
{
	return cache.find(key) != cache.end();
}

std::shared_ptr<Shader> ShaderLoader::get(const std::string& key) const
{
	auto it = cache.find(key);
	return (it != cache.end()) ? it->second : nullptr;
}

void ShaderLoader::registerShader(const std::string& key, const char* vert, const char* frag)
{
	if (!isLoaded(key)) {
		auto shader = std::make_shared<Shader>(vert, frag);
		if (!shader->isValid()) throw std::runtime_error(shader->lastError());
		cache[key] = std::move(shader);
	}
}

void ShaderLoader::registerShader(const std::string& key, const char* vert, const char* geom, const char* frag)
{
	if (!isLoaded(key)) {
		auto shader = std::make_shared<Shader>(vert, geom, frag);
		if (!shader->isValid()) throw std::runtime_error(shader->lastError());
		cache[key] = std::move(shader);
	}
}

void ShaderLoader::shutdown()
{
	cache.clear();
}
