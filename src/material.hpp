#ifndef MATERIAL_HPP
#define MATERIAL_HPP
#pragma once

#include "texture.hpp"
#include "shader.hpp"
#include <vector>
#include <string>

class Material
{
public:
	Material() {};
	Material(unsigned int index, std::string albedoPath, std::string ambientPath, std::string specularPath, std::string normalPath, std::string shininessPath);
	void initGLResources();
	void bind(ShaderPtr shader);

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
