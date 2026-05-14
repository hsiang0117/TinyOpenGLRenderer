#ifndef TEXTURE_HPP
#define TEXTURE_HPP
#pragma once

#include <glad/glad.h>

class Texture {
public:
	Texture() = default;
	~Texture() = default;
	GLuint ID;
	virtual void use(GLenum textureUnit) = 0;
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
	Texture2D(const char* path, Type type, GLenum wrap, GLenum filter);
	virtual void use(GLenum textureUnit) override;
	void setBorderColor(float r, float g, float b, float a);
	void setDefaultColor(float r, float g, float b, float a);
	void resetSize(int width, int height);
	void subImage2D(int xOffset, int yOffset, int width, int height, const void* data);
	Type getType() { return type; }
private:
	Type type;
	GLenum internalFormat, format, dataType;
};

class Texture2DArray : public Texture {
public:
	Texture2DArray() {}
	Texture2DArray(int width, int height, int length, GLenum wrap, GLenum filter, GLenum internalFormat, GLenum format);
	virtual void use(GLenum textureUnit) override;
};

class CubeMap : public Texture {
public:
	CubeMap() {};
	CubeMap(const char* folderPath, GLenum wrap, GLenum filter);
	virtual void use(GLenum textureUnit) override;
};

class CubeMapArray : public Texture {
public:
	CubeMapArray() {};
	CubeMapArray(int width, int height, int length, GLenum wrap, GLenum filter, GLenum internalFormat, GLenum format);
	virtual void use(GLenum textureUnit) override;
};

class RenderBuffer {
public:
	GLuint ID;
	RenderBuffer() {};
	RenderBuffer(int width, int height, GLenum internalFormat);
	void resetSize(int width, int height);
private:
	GLenum internalFormat;
};

class Texture3D : public Texture {
public:
	Texture3D() {};
	Texture3D(int width, int height, int depth, GLenum wrap, GLenum filter, GLenum internalFormat, GLenum format, GLenum dataType);
	void subImage3D(int xOffset, int yOffset, int zOffset, int width, int height, int depth, const void* data);
	virtual void use(GLenum textureUnit) override;
};

#endif // !TEXTURE_HPP
