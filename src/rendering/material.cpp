#include "material.hpp"
#include "rendering/opengl/shader.hpp"
#include "core/logger.hpp"
#include <filesystem>
#include <system_error>
#include <utility>

Material::Material(std::string albedoPath, std::string ambientPath, std::string specularPath, std::string normalPath, std::string shininessPath)
{
	this->albedoPath = albedoPath;
	this->ambientPath = ambientPath;
	this->specularPath = specularPath;
	this->normalPath = normalPath;
	this->shininessPath = shininessPath;
}

bool Material::initGLResources()
{
	textures.clear();
	lastError_.clear();
	auto loadTexture = [this](const std::string& name, Texture2D::Type type) {
		if (name.empty()) return;
		try {
			const auto path = std::filesystem::u8path(name);
			Texture2D texture(path, type, GL_REPEAT, GL_LINEAR);
			if (texture.isValid()) textures.push_back(std::move(texture));
			else {
				if (!lastError_.empty()) lastError_ += '\n';
				lastError_ += texture.lastError();
			}
		} catch (const std::system_error& error) {
			const auto message = "Invalid texture path: " + name + ": " + error.what();
			if (!lastError_.empty()) lastError_ += '\n';
			lastError_ += message;
			LOG_ERROR(message);
		}
	};
	loadTexture(albedoPath, Texture2D::Type::ALBEDO);
	loadTexture(ambientPath, Texture2D::Type::AMBIENT);
	loadTexture(specularPath, Texture2D::Type::SPECULAR);
	loadTexture(normalPath, Texture2D::Type::NORMAL);
	// Imported shininess still participates in the existing default-material
	// decision via the loaded-map set, although the shader does not sample it.
	loadTexture(shininessPath, Texture2D::Type::SHININESS);
	if (!lastError_.empty()) LOG_WARN("Using material defaults for failed textures");
	return lastError_.empty();
}

void Material::bind(ShaderPtr shader)
{
	// 默认绑占位纹理（而非纹理 0），避免无贴图材质时 shader 声明的
	// sampler2D 对应单元不完整触发驱动警告
	for (auto i = GL_TEXTURE0; i <= GL_TEXTURE4; i++) {
		bindDummyTexture2D(i);
	}
	shader->setInt("useDefaultMaterial", 0);
	shader->setInt("hasNormalMap", 0);
	if (textures.size() == 0) {
		shader->setInt("useDefaultMaterial", 1);
		return;
	}
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
