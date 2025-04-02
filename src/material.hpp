#ifndef MATERIAL_HPP
#define MATERIAL_HPP
#pragma once

#include "texture.hpp"
#include <glad/glad.h>
#include <iostream>
#include <vector>

class Material
{
public:
	Material() {};
	void init(std::string name, std::string albedoPath, std::string ambientPath, std::string specularPath, std::string normalPath, std::string shininessPath);
	void bind();
private:
	std::string name;
	std::vector<Texture2D> textures;
	void deleteTextures();
};

void Material::init(std::string name, std::string albedoPath, std::string ambientPath, std::string specularPath, std::string normalPath, std::string shininessPath)
{
	deleteTextures();
	this->name = name;
	textures.push_back(Texture2D(albedoPath.c_str(), "textureAlbedo", GL_REPEAT, GL_LINEAR));
	textures.push_back(Texture2D(ambientPath.c_str(), "textureAmbient", GL_REPEAT, GL_LINEAR));
	textures.push_back(Texture2D(specularPath.c_str(), "textureSpecular", GL_REPEAT, GL_LINEAR));
	textures.push_back(Texture2D(normalPath.c_str(), "textureNormal", GL_REPEAT, GL_LINEAR));
	textures.push_back(Texture2D(shininessPath.c_str(), "textureShininess", GL_REPEAT, GL_LINEAR));
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