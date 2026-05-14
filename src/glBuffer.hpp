#ifndef GLBUFFER_HPP
#define GLBUFFER_HPP
#pragma once

#include "texture.hpp"
#include <initializer_list>

class GLBuffer {
public:
	GLBuffer() : ID(0) {};
	GLuint ID;
	virtual void init();
	virtual void reset();
	virtual void destroy();
	virtual void bind() = 0;
	virtual void unbind() = 0;
};

class UniformBuffer : public GLBuffer {
public:
	UniformBuffer() {};
	virtual void bind() override;
	virtual void unbind() override;

	void bufferData(GLsizeiptr size, const GLvoid* data);
	void bufferBase(GLuint index);
	void bufferSubdata(GLintptr offset, GLsizeiptr size, const GLvoid* data);
};

class ShaderStorageBuffer : public GLBuffer {
public:
	ShaderStorageBuffer() {};
	virtual void bind() override;
	virtual void unbind() override;

	void bufferData(GLsizeiptr size, const GLvoid* data);
	void bufferBase(GLuint index);
	void bufferSubdata(GLintptr offset, GLsizeiptr size, const GLvoid* data);
};

class FrameBuffer : public GLBuffer {
public:
	GLuint ID;
	FrameBuffer() {};
	virtual void init() override;
	virtual void reset() override;
	virtual void destroy() override;
	virtual void bind() override;
	virtual void unbind() override;

	void attachTexture(Texture& texture, GLenum type);
	void attachTexture2D(Texture2D& texture, GLenum type);
	void attachRenderBuffer(RenderBuffer& renderBuffer, GLenum type);
	template<typename T>
	void attachTextureLayer(T& textureArray, GLenum type, int index);
	void drawBuffers(std::initializer_list<GLenum> types);
	void readBuffer(const GLenum type);
};

template<typename T>
void FrameBuffer::attachTextureLayer(T& textureArray, GLenum type, int index) {
	glFramebufferTextureLayer(GL_FRAMEBUFFER, type, textureArray.ID, 0, index);
}

#endif // !GLBUFFER_HPP
