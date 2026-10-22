#include "glBuffer.hpp"
#include "texture.hpp"
#include <sstream>
#include <stdexcept>
#include <string>

namespace togr {

GLBuffer::~GLBuffer() {
	destroy();
}

GLBuffer::GLBuffer(GLBuffer&& other) noexcept : ID(other.ID) {
	other.ID = 0;
}

GLBuffer& GLBuffer::operator=(GLBuffer&& other) noexcept {
	if (this != &other) {
		destroy();
		ID = other.ID;
		other.ID = 0;
	}
	return *this;
}

void GLBuffer::init() {
	if (ID) destroy();
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
	// 每帧用 bufferSubdata 更新（view/proj 矩阵），用 DYNAMIC_DRAW 匹配实际访问模式
	glBufferData(GL_UNIFORM_BUFFER, size, data, GL_DYNAMIC_DRAW);
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
	// 每帧重新上传灯光数据，用 DYNAMIC_DRAW 匹配实际访问模式
	glBufferData(GL_SHADER_STORAGE_BUFFER, size, data, GL_DYNAMIC_DRAW);
}

void ShaderStorageBuffer::bufferBase(GLuint index) {
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, index, ID);
}

void ShaderStorageBuffer::bufferSubdata(GLintptr offset, GLsizeiptr size, const GLvoid* data) {
	glBufferSubData(GL_SHADER_STORAGE_BUFFER, offset, size, data);
}

void FrameBuffer::init() {
	if (ID) destroy();
	glGenFramebuffers(1, &ID);
}

void FrameBuffer::destroy() {
	if (ID) {
		glDeleteFramebuffers(1, &ID);
	}
	ID = 0;
}

FrameBuffer::~FrameBuffer() {
	destroy();
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

void FrameBuffer::drawBuffers(std::initializer_list<GLenum> types) {
	glDrawBuffers(static_cast<GLsizei>(types.size()), types.begin());
}

void FrameBuffer::readBuffer(const GLenum type) {
	glReadBuffer(type);
}

void FrameBuffer::requireComplete(const char* label) const {
	const char* name = label ? label : "Unnamed framebuffer";
	if (!ID) {
		throw std::runtime_error(std::string(name) + ": framebuffer object was not created (ID=0)");
	}
	const GLenum status = glCheckNamedFramebufferStatus(ID, GL_FRAMEBUFFER);
	if (status == GL_FRAMEBUFFER_COMPLETE) return;
	const char* reason = "unknown framebuffer status";
	switch (status) {
	case GL_FRAMEBUFFER_UNDEFINED: reason = "undefined framebuffer"; break;
	case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT: reason = "incomplete attachment"; break;
	case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT: reason = "missing attachment"; break;
	case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER: reason = "incomplete draw buffer"; break;
	case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER: reason = "incomplete read buffer"; break;
	case GL_FRAMEBUFFER_UNSUPPORTED: reason = "unsupported attachment formats"; break;
	case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE: reason = "inconsistent multisample attachments"; break;
	case GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS: reason = "inconsistent layered attachments"; break;
	}
	std::ostringstream message;
	message << name << ": " << reason << " (status=0x" << std::hex << std::uppercase << status << ")";
	throw std::runtime_error(message.str());
}

} // namespace togr
