#ifndef GLBUFFER_HPP
#define GLBUFFER_HPP
#pragma once

#include "texture.hpp"

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

void GLBuffer::init() {
	glGenBuffers(1, &ID);
}

void GLBuffer::reset() {
	destroy();
	glGenBuffers(1, &ID);
}

void GLBuffer::destroy() {
	if (ID) {
		glDeleteBuffers(1, &ID);
	}
	ID = 0;
}

class UniformBuffer : public GLBuffer{
public:
	UniformBuffer() {};
	virtual void bind() override;
	virtual void unbind() override;

	void bufferData(GLsizeiptr size, const GLvoid* data);
	void bufferBase(GLuint index);
	void bufferSubdata(GLintptr offset, GLsizeiptr size, const GLvoid* data);
};

void UniformBuffer::bind() {
	glBindBuffer(GL_UNIFORM_BUFFER, ID);
}

void UniformBuffer::unbind() {
	glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

void UniformBuffer::bufferData(GLsizeiptr size, const GLvoid* data) {
	glBufferData(GL_UNIFORM_BUFFER, size, data, GL_STATIC_DRAW);
}

void UniformBuffer::bufferBase(GLuint index) {
	glBindBufferBase(GL_UNIFORM_BUFFER, index, ID);
}

void UniformBuffer::bufferSubdata(GLintptr offset, GLsizeiptr size, const GLvoid* data) {
	glBufferSubData(GL_UNIFORM_BUFFER, offset, size, data);
}

class ShaderStorageBuffer :public GLBuffer {
public:
	ShaderStorageBuffer() {};
	virtual void bind() override;
	virtual void unbind() override;

	void bufferData(GLsizeiptr size, const GLvoid* data);
	void bufferBase(GLuint index);
	void bufferSubdata(GLintptr offset, GLsizeiptr size, const GLvoid* data);
};

void ShaderStorageBuffer::bind() {
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, ID);
}

void ShaderStorageBuffer::unbind() {
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void ShaderStorageBuffer::bufferData(GLsizeiptr size, const GLvoid* data) {
	glBufferData(GL_SHADER_STORAGE_BUFFER, size, data, GL_STATIC_DRAW);
}

void ShaderStorageBuffer::bufferBase(GLuint index) {
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, index, ID);
}

void ShaderStorageBuffer::bufferSubdata(GLintptr offset, GLsizeiptr size, const GLvoid* data) {
	glBufferSubData(GL_SHADER_STORAGE_BUFFER, offset, size, data);
}

class FrameBuffer : public GLBuffer{
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
	void drawBuffers(const GLenum type[]);
	void readBuffer(const GLenum type);
};

void FrameBuffer::init() {
	glGenFramebuffers(1, &ID);
}

void FrameBuffer::reset() {
	destroy();
	glGenFramebuffers(1, &ID);
}

void FrameBuffer::destroy() {
	if (ID) {
		glDeleteFramebuffers(1, &ID);
	}
	ID = 0;
}

void FrameBuffer::bind() {
	glBindFramebuffer(GL_FRAMEBUFFER, ID);
}

void FrameBuffer::unbind() {
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void FrameBuffer::attachTexture(Texture& texture, GLenum type) {
	glFramebufferTexture(GL_FRAMEBUFFER, type, texture.ID, 0);
}

void FrameBuffer::attachTexture2D(Texture2D& texture, GLenum type) {
	glFramebufferTexture2D(GL_FRAMEBUFFER, type, GL_TEXTURE_2D, texture.ID, 0);
}

void FrameBuffer::attachRenderBuffer(RenderBuffer& renderBuffer, GLenum type) {
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, type, GL_RENDERBUFFER, renderBuffer.ID);
}

template<typename T>
void FrameBuffer::attachTextureLayer(T& textureArray, GLenum type, int index) {
	glFramebufferTextureLayer(GL_FRAMEBUFFER, type, textureArray.ID, 0, index);
}

void FrameBuffer::drawBuffers(const GLenum type[]) {
	glDrawBuffers(sizeof(type) / sizeof(type[0]), type);
}

void FrameBuffer::readBuffer(const GLenum type) {
	glReadBuffer(type);
}
#endif // !GLBUFFER_HPP