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
	Material(unsigned int index, std::string albedoPath, std::string ambientPath, std::string specularPath, std::string normalPath, std::string shininessPath);
	void initGLResources();
	void bind(std::shared_ptr<Shader> shader);

	std::string albedoPath;
	std::string ambientPath;
	std::string specularPath;
	std::string normalPath;
	std::string shininessPath;
private:
	unsigned int index;
	std::vector<Texture2D> textures;
};

#endif // !MATERIAL_HPP
