#ifndef SHADERLOADER_HPP
#define SHADERLOADER_HPP
#pragma once

#include <memory>
#include <string>
#include <unordered_map>

class Shader;

// Register and release programs while the GL context is current.
class ShaderLoader {
public:
	ShaderLoader() = default;

	bool isLoaded(const std::string& key) const;
	std::shared_ptr<Shader> get(const std::string& key) const;
	void registerShader(const std::string& key, const char* vert, const char* frag);
	void registerShader(const std::string& key, const char* vert, const char* geom, const char* frag);
	void shutdown();

private:
	std::unordered_map<std::string, std::shared_ptr<Shader>> cache;
};

#endif // !SHADERLOADER_HPP
