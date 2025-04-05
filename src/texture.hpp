#ifndef TEXTURE_HPP
#define TEXTURE_HPP
#pragma once

#include <glad/glad.h>
#include <iostream>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

class Texture {
public:
	GLuint ID;
	std::string type;
	Texture() {};
	void use(GLenum textureUnit);
};

void Texture::use(GLenum textureUnit) {
	glActiveTexture(textureUnit);
	glBindTexture(GL_TEXTURE_2D, ID);
}

class Texture2D : public Texture {
public:
	Texture2D(int width, int height, GLenum wrap, GLenum filter, GLenum internalFormat, GLenum format); // Empty texture, for framebuffer usage etc.
	Texture2D(const char* path, const std::string& type, GLenum wrap, GLenum filter); // Load texture from file.
};

Texture2D::Texture2D(int width, int height, GLenum wrap, GLenum filter, GLenum internalFormat, GLenum format) {
	glGenTextures(1, &ID);
	glBindTexture(GL_TEXTURE_2D, ID);
	glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, format, GL_FLOAT, NULL);
	glGenerateMipmap(GL_TEXTURE_2D);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
	glBindTexture(GL_TEXTURE_2D, 0);
}

Texture2D::Texture2D(const char* path, const std::string& type, GLenum wrap, GLenum filter) {
	stbi_set_flip_vertically_on_load(true);
	int width, height, nrChannels;
	unsigned char* data = stbi_load(path, &width, &height, &nrChannels, 4);
	if (!data) {
		std::cerr << "Failed to load texture" << std::endl;
	}
	else {
		GLenum internalFormat, format;
		if (nrChannels == 1) {
			internalFormat = GL_RED;
			format = GL_RGB;
		}
		else if (nrChannels == 3) {
			internalFormat = GL_RGB;
			format = GL_RGB;
		}
		else if (nrChannels == 4) {
			internalFormat = GL_RGBA;
			format = GL_RGBA;
		}
		glGenTextures(1, &ID);
		glBindTexture(GL_TEXTURE_2D, ID);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
		glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, format, GL_UNSIGNED_BYTE, data);
		glGenerateMipmap(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, 0);
	}
	stbi_image_free(data);
}

class RenderBuffer {
public:
	GLuint ID;
	RenderBuffer(int width, int height, GLenum internalFormat);
};

RenderBuffer::RenderBuffer(int width, int height, GLenum internalFormat) {
	glGenRenderbuffers(1, &ID);
	glBindRenderbuffer(GL_RENDERBUFFER, ID);
	glRenderbufferStorage(GL_RENDERBUFFER, internalFormat, width, height);
	glBindRenderbuffer(GL_RENDERBUFFER, 0);
}

#endif // !TEXTURE_HPP
