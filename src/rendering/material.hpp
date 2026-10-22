#ifndef MATERIAL_HPP
#define MATERIAL_HPP
#pragma once

#include "rendering/opengl/texture.hpp"
#include <memory>
#include <vector>
#include <string>

class Shader;

class Material
{
public:
	Material() {};
	Material(std::string albedoPath, std::string ambientPath, std::string specularPath, std::string normalPath, std::string shininessPath);
	// Empty paths use defaults; failed requested textures are reported and excluded.
	bool initGLResources();
	const std::string& lastError() const { return lastError_; }
	void bind(std::shared_ptr<Shader> shader);

	std::string albedoPath;
	std::string ambientPath;
	std::string specularPath;
	std::string normalPath;
	std::string shininessPath;
private:
	std::vector<Texture2D> textures;
	std::string lastError_;
};

#endif // !MATERIAL_HPP
