#include "material.hpp"
#include <filesystem>

Material::Material(unsigned int index, std::string albedoPath, std::string ambientPath, std::string specularPath, std::string normalPath, std::string shininessPath)
{
	this->index = index;
	this->albedoPath = albedoPath;
	this->ambientPath = ambientPath;
	this->specularPath = specularPath;
	this->normalPath = normalPath;
	this->shininessPath = shininessPath;
}

void Material::initGLResources()
{
	textures.clear();
	if (std::filesystem::is_regular_file(albedoPath))
	{
		textures.push_back(Texture2D(albedoPath.c_str(), Texture2D::Type::ALBEDO, GL_REPEAT, GL_LINEAR));
	}
	if (std::filesystem::is_regular_file(ambientPath))
	{
		textures.push_back(Texture2D(ambientPath.c_str(), Texture2D::Type::AMBIENT, GL_REPEAT, GL_LINEAR));
	}
	if (std::filesystem::is_regular_file(specularPath))
	{
		textures.push_back(Texture2D(specularPath.c_str(), Texture2D::Type::SPECULAR, GL_REPEAT, GL_LINEAR));
	}
	if (std::filesystem::is_regular_file(normalPath))
	{
		textures.push_back(Texture2D(normalPath.c_str(), Texture2D::Type::NORMAL, GL_REPEAT, GL_LINEAR));
	}
	if (std::filesystem::is_regular_file(shininessPath))
	{
		textures.push_back(Texture2D(shininessPath.c_str(), Texture2D::Type::SHININESS, GL_REPEAT, GL_LINEAR));
	}
}

void Material::bind(ShaderPtr shader)
{
	for (auto i = GL_TEXTURE0; i <= GL_TEXTURE4; i++) {
		glActiveTexture(i);
		glBindTexture(GL_TEXTURE_2D, 0);
	}
	shader->setInt("useDefaultMaterial", 0);
	if (textures.size() == 0) {
		shader->setInt("useDefaultMaterial", 1);
		return;
	}
	shader->setInt("hasNormalMap", 0);
	for (int i = 0; i < textures.size(); i++)
	{
		switch (textures[i].getType()) {
			case Texture2D::Type::ALBEDO:
				textures[i].use(GL_TEXTURE0);
				break;
			case Texture2D::Type::AMBIENT:
				textures[i].use(GL_TEXTURE1);
				break;
			case Texture2D::Type::SPECULAR:
				textures[i].use(GL_TEXTURE2);
				break;
			case Texture2D::Type::NORMAL:
				shader->setInt("hasNormalMap", 1);
				textures[i].use(GL_TEXTURE3);
				break;
			case Texture2D::Type::SHININESS:
				textures[i].use(GL_TEXTURE4);
				break;
		}
	}
}
