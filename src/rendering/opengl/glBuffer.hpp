#ifndef GLBUFFER_HPP
#define GLBUFFER_HPP
#pragma once

#include <glad/glad.h>
#include <initializer_list>

class Texture;
class Texture2D;

class GLBuffer {
public:
	GLBuffer() : ID(0) {};
	virtual ~GLBuffer();

	GLBuffer(const GLBuffer&) = delete;
	GLBuffer& operator=(const GLBuffer&) = delete;
	GLBuffer(GLBuffer&& other) noexcept;
	GLBuffer& operator=(GLBuffer&& other) noexcept;

	GLuint ID;
	virtual void init();
	virtual void destroy();
	virtual void bind() = 0;
	virtual void unbind() = 0;
};

class UniformBuffer : public GLBuffer {
public:
	UniformBuffer() = default;
	virtual void bind() override;
	virtual void unbind() override;

	void bufferData(GLsizeiptr size, const GLvoid* data);
	void bufferBase(GLuint index);
	void bufferSubdata(GLintptr offset, GLsizeiptr size, const GLvoid* data);
};

class ShaderStorageBuffer : public GLBuffer {
public:
	ShaderStorageBuffer() = default;
	virtual void bind() override;
	virtual void unbind() override;

	void bufferData(GLsizeiptr size, const GLvoid* data);
	void bufferBase(GLuint index);
	void bufferSubdata(GLintptr offset, GLsizeiptr size, const GLvoid* data);
};

class FrameBuffer : public GLBuffer {
public:
	FrameBuffer() = default;
	~FrameBuffer() override;
	FrameBuffer(FrameBuffer&&) = default;
	FrameBuffer& operator=(FrameBuffer&&) = default;

	virtual void init() override;
	virtual void destroy() override;
	virtual void bind() override;
	virtual void unbind() override;

	void attachTexture(Texture& texture, GLenum type);
	void attachTexture2D(Texture2D& texture, GLenum type);
	template<typename T>
	void attachTextureLayer(T& textureArray, GLenum type, int index);
	void drawBuffers(std::initializer_list<GLenum> types);
	void readBuffer(const GLenum type);
	// Validate this object without changing either framebuffer binding.
	void requireComplete(const char* label) const;
};

template<typename T>
void FrameBuffer::attachTextureLayer(T& textureArray, GLenum type, int index) {
	glFramebufferTextureLayer(GL_FRAMEBUFFER, type, textureArray.ID, 0, index);
}

#endif // !GLBUFFER_HPP
