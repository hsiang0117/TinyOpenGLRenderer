#ifndef TOGR_TEXTURE_HPP
#define TOGR_TEXTURE_HPP
#pragma once

#include <glad/glad.h>
#include <filesystem>
#include <string>

namespace togr {

class Texture {
public:
	Texture() : ID(0) {}
	virtual ~Texture();

	Texture(const Texture&) = delete;
	Texture& operator=(const Texture&) = delete;
	Texture(Texture&& other) noexcept;
	Texture& operator=(Texture&& other) noexcept;

	GLuint ID;
	bool isValid() const { return ID != 0; }
	const std::string& lastError() const { return lastError_; }
	void destroy();
	virtual void use(GLenum textureUnit) = 0;
protected:
	void fail(std::string reason);
private:
	std::string lastError_;
};

class Texture2D : public Texture {
public:
	enum class Type {
		ALBEDO,
		AMBIENT,
		SPECULAR,
		NORMAL,
		SHININESS,
		BUFFER
	};
	Texture2D() {};
	Texture2D(int width, int height, GLenum wrap, GLenum filter, GLenum internalFormat, GLenum format, GLenum dataType);
	// Public character paths are UTF-8; path overloads preserve native filenames.
	Texture2D(const char* path, Type type, GLenum wrap, GLenum filter);
	Texture2D(const std::filesystem::path& path, Type type, GLenum wrap, GLenum filter);
	virtual void use(GLenum textureUnit) override;
	void setBorderColor(float r, float g, float b, float a);
	void setDefaultColor(float r, float g, float b, float a);
	void resetSize(int width, int height);
	void subImage2D(int xOffset, int yOffset, int width, int height, const void* data);
	Type getType() { return type; }
private:
	Type type = Type::BUFFER;
	GLenum internalFormat = GL_RGBA8, format = GL_RGBA, dataType = GL_UNSIGNED_BYTE;
};

class CubeMap : public Texture {
public:
	CubeMap() {};
	CubeMap(const char* folderPath, GLenum wrap, GLenum filter);
	CubeMap(const std::filesystem::path& folderPath, GLenum wrap, GLenum filter);
	virtual void use(GLenum textureUnit) override;
};

class CubeMapArray : public Texture {
public:
	CubeMapArray() {};
	CubeMapArray(int width, int height, int length, GLenum wrap, GLenum filter, GLenum internalFormat, GLenum format);
	virtual void use(GLenum textureUnit) override;
};

class Texture3D : public Texture {
public:
	Texture3D() {};
	Texture3D(int width, int height, int depth, GLenum wrap, GLenum filter, GLenum internalFormat, GLenum format, GLenum dataType);
	void subImage3D(int xOffset, int yOffset, int zOffset, int width, int height, int depth, const void* data);
	virtual void use(GLenum textureUnit) override;
private:
	GLenum format = GL_RED, dataType = GL_UNSIGNED_BYTE;
};

// 1x1 白色占位纹理：绑到 shader 声明了 sampler2D 但本次绘制无实际纹理的单元，
// 避免驱动报 “texture object (0) ... does not have a defined base level” 警告。
// The renderer explicitly releases this shared texture before context shutdown.
void bindDummyTexture2D(GLenum textureUnit);
void destroyDummyTexture2D();
// Complete black environment for scenes without a ready skybox.
void bindDummyCubeMap(GLenum textureUnit);
void destroyDummyCubeMap();

} // namespace togr

#endif // !TOGR_TEXTURE_HPP
