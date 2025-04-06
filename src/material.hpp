#ifndef MATERIAL_HPP
#define MATERIAL_HPP
#pragma once

#include "texture.hpp"
#include <glad/glad.h>
#include <iostream>
#include <vector>
#include <filesystem>

class Material
{
public:
	Material(){};
	Material(std::string name, std::string albedoPath, std::string ambientPath, std::string specularPath, std::string normalPath, std::string shininessPath);
	void initGLResources();
	void bind();
private:
	std::string name;
	std::string albedoPath;
	std::string ambientPath;
	std::string specularPath;
	std::string normalPath;
	std::string shininessPath;
	std::vector<Texture2D> textures;
	void deleteTextures();
};

Material::Material(std::string name, std::string albedoPath, std::string ambientPath, std::string specularPath, std::string normalPath, std::string shininessPath)
{
	this->name = name;
	this->albedoPath = albedoPath;
	this->ambientPath = ambientPath;
	this->specularPath = specularPath;
	this->normalPath = normalPath;
	this->shininessPath = shininessPath;
}

void Material::initGLResources()
{
	deleteTextures();
	if (std::filesystem::is_regular_file(albedoPath))
	{
		textures.push_back(Texture2D(albedoPath.c_str(), "textureAlbedo", GL_REPEAT, GL_LINEAR));
	}
	if (std::filesystem::is_regular_file(ambientPath))
	{
		textures.push_back(Texture2D(ambientPath.c_str(), "textureAmbient", GL_REPEAT, GL_LINEAR));
	}
	if (std::filesystem::is_regular_file(specularPath))
	{
		textures.push_back(Texture2D(specularPath.c_str(), "textureSpecular", GL_REPEAT, GL_LINEAR));
	}
	if (std::filesystem::is_regular_file(normalPath))
	{
		textures.push_back(Texture2D(normalPath.c_str(), "textureNormal", GL_REPEAT, GL_LINEAR));
	}
	if (std::filesystem::is_regular_file(shininessPath))
	{
		textures.push_back(Texture2D(shininessPath.c_str(), "textureShininess", GL_REPEAT, GL_LINEAR));
	}
}

void Material::bind()
{
	for (unsigned int i = 0; i < textures.size(); i++)
	{
		textures[i].use(GL_TEXTURE0 + i);
	}
}

void Material::deleteTextures()
{
	for (unsigned int i = 0; i < textures.size(); i++)
	{
		glDeleteTextures(1, &textures[i].ID);
		textures[i].ID = 0;
	}
	textures.clear();
}
#endif // !MATERIAL_HPP