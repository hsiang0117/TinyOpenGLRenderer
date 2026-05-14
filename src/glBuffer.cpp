#include "glBuffer.hpp"

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

void FrameBuffer::drawBuffers(std::initializer_list<GLenum> types) {
	glDrawBuffers(static_cast<GLsizei>(types.size()), types.begin());
}

void FrameBuffer::readBuffer(const GLenum type) {
	glReadBuffer(type);
}
