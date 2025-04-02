#ifndef FRAMEBUFFER_HPP
#define FRAMEBUFFER_HPP
#pragma once

#include "texture.hpp"

class FrameBuffer {
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
	void reset();
	void destroy();

	void attachTexture(std::shared_ptr<Texture> texture, AttachmentType type);
	void attachRenderBuffer(std::shared_ptr<RenderBuffer> renderBuffer, AttachmentType type);
	void bind();
	void unbind();
	void drawBuffers(const AttachmentType attachments[]);
};

FrameBuffer::FrameBuffer() {
	glGenFramebuffers(1, &ID);
}

void FrameBuffer::reset() {
	destroy();
	glBindFramebuffer(GL_FRAMEBUFFER, ID);
}

void FrameBuffer::destroy() {
	if (ID) {
		glDeleteFramebuffers(1, &ID);
	}
	ID = 0;
}

void FrameBuffer::attachTexture(std::shared_ptr<Texture> texture, AttachmentType type) {
	glBindFramebuffer(GL_FRAMEBUFFER, ID);
	glFramebufferTexture2D(GL_FRAMEBUFFER, static_cast<GLenum>(type), GL_TEXTURE_2D, texture->ID, 0);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void FrameBuffer::attachRenderBuffer(std::shared_ptr<RenderBuffer> renderBuffer, AttachmentType type) {
	glBindFramebuffer(GL_FRAMEBUFFER, ID);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, static_cast<GLenum>(type), GL_RENDERBUFFER, renderBuffer->ID);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void FrameBuffer::bind() {
	glBindFramebuffer(GL_FRAMEBUFFER, ID);
}

void FrameBuffer::unbind() {
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void FrameBuffer::drawBuffers(const AttachmentType attachments[]) {
	glBindFramebuffer(GL_FRAMEBUFFER, ID);
	glDrawBuffers(sizeof(attachments) / sizeof(attachments[0]), reinterpret_cast<const GLenum*>(attachments));
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

#endif // !FRAMEBUFFER_HPP