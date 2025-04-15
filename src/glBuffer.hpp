#ifndef GLBUFFER_HPP
#define GLBUFFER_HPP
#pragma once

#include "texture.hpp"

class GLBuffer {
public:
	GLuint ID;
	GLBuffer() = default;
	void reset();
	void destroy();
	virtual void bind() = 0;
	virtual void unbind() = 0;
};

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
	UniformBuffer();
	virtual void bind() override;
	virtual void unbind() override;

	void bufferData(GLsizeiptr size, const GLvoid* data);
	void bufferBase(GLuint index);
	void bufferSubdata(GLintptr offset, GLsizeiptr size, const GLvoid* data);
};

UniformBuffer::UniformBuffer() {
	glGenBuffers(1, &ID);
}

void UniformBuffer::bind() {
	glBindBuffer(GL_UNIFORM_BUFFER, ID);
}

void UniformBuffer::unbind() {
	glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

void UniformBuffer::bufferData(GLsizeiptr size, const GLvoid* data) {
	glBufferData(GL_UNIFORM_BUFFER, size, &data, GL_STATIC_DRAW);
}

void UniformBuffer::bufferBase(GLuint index) {
	glBindBufferBase(GL_UNIFORM_BUFFER, index, ID);
}

void UniformBuffer::bufferSubdata(GLintptr offset, GLsizeiptr size, const GLvoid* data) {
	glBufferSubData(GL_UNIFORM_BUFFER, offset, size, &data);
}

class ShaderStorageBuffer :public GLBuffer {
public:
	ShaderStorageBuffer();
};

ShaderStorageBuffer::ShaderStorageBuffer() {
	glGenBuffers(1, &ID);
}

class FrameBuffer : public GLBuffer{
public:
	enum class AttachmentType : decltype(GL_COLOR_ATTACHMENT0) {
		COLOR0 = GL_COLOR_ATTACHMENT0,
		COLOR1 = GL_COLOR_ATTACHMENT1,
		COLOR2 = GL_COLOR_ATTACHMENT2,
		COLOR3 = GL_COLOR_ATTACHMENT3,
		DEPTH = GL_DEPTH_ATTACHMENT,
		STENCIL = GL_STENCIL_ATTACHMENT
	};

	GLuint ID;
	FrameBuffer();
	virtual void bind() override;
	virtual void unbind() override;

	void attachTexture(Texture& texture, AttachmentType type);
	void attachRenderBuffer(RenderBuffer& renderBuffer, AttachmentType type);
	void drawBuffers(const AttachmentType attachments[]);
};

FrameBuffer::FrameBuffer() {
	glGenFramebuffers(1, &ID);
}

void FrameBuffer::bind() {
	glBindFramebuffer(GL_FRAMEBUFFER, ID);
}

void FrameBuffer::unbind() {
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void FrameBuffer::attachTexture(Texture& texture, AttachmentType type) {
	glBindFramebuffer(GL_FRAMEBUFFER, ID);
	glFramebufferTexture2D(GL_FRAMEBUFFER, static_cast<GLenum>(type), GL_TEXTURE_2D, texture.ID, 0);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void FrameBuffer::attachRenderBuffer(RenderBuffer& renderBuffer, AttachmentType type) {
	glBindFramebuffer(GL_FRAMEBUFFER, ID);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, static_cast<GLenum>(type), GL_RENDERBUFFER, renderBuffer.ID);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void FrameBuffer::drawBuffers(const AttachmentType attachments[]) {
	glBindFramebuffer(GL_FRAMEBUFFER, ID);
	glDrawBuffers(sizeof(attachments) / sizeof(attachments[0]), reinterpret_cast<const GLenum*>(attachments));
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

#endif // !GLBUFFER_HPP