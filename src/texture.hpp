#ifndef TEXTURE_HPP
#define TEXTURE_HPP
#pragma once

#include <glad/glad.h>

class Texture {
public:
	Texture() : ID(0) {}
	virtual ~Texture();

	Texture(const Texture&) = delete;
	Texture& operator=(const Texture&) = delete;
	Texture(Texture&& other) noexcept;
	Texture& operator=(Texture&& other) noexcept;

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
	RenderBuffer() : ID(0) {}
	RenderBuffer(int width, int height, GLenum internalFormat);
	~RenderBuffer();
	RenderBuffer(const RenderBuffer&) = delete;
	RenderBuffer& operator=(const RenderBuffer&) = delete;
	RenderBuffer(RenderBuffer&& other) noexcept;
	RenderBuffer& operator=(RenderBuffer&& other) noexcept;
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

// 1x1 白色占位纹理：绑到 shader 声明了 sampler2D 但本次绘制无实际纹理的单元，
// 避免驱动报 “texture object (0) ... does not have a defined base level” 警告。
// 故意泄漏（裸 GLuint，永不删除），规避 RAII 在 GL context 销毁后析构。
void bindDummyTexture2D(GLenum textureUnit);

#endif // !TEXTURE_HPP
