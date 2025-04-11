#ifndef TEXTURE_HPP
#define TEXTURE_HPP
#pragma once

#include <glad/glad.h>
#include <iostream>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

class Texture {
public:
	Texture() = default;
	~Texture() = default;
	GLuint ID;
	void use(GLenum textureUnit);
};
   
void Texture::use(GLenum textureUnit) {
	glActiveTexture(textureUnit);
	glBindTexture(GL_TEXTURE_2D, ID);
}

class Texture2D : public Texture {
public:	
	enum class Type {
		ALBEDO,
		AMBIENT,
		SPECULAR,
		NORMAL,
		SHININESS,
		COLORBUFFER
	};
	Texture2D(int width, int height, GLenum wrap, GLenum filter, GLenum internalFormat, GLenum format); // Empty texture, for framebuffer usage etc.
	Texture2D(const char* path, Type type, GLenum wrap, GLenum filter); // Load texture from file.
	Type getType() { return type; }
private:
	Type type;
};

Texture2D::Texture2D(int width, int height, GLenum wrap, GLenum filter, GLenum internalFormat, GLenum format) {
	this->type = Type::COLORBUFFER;
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

Texture2D::Texture2D(const char* path, Type type, GLenum wrap, GLenum filter) {
	this->type = type;
	stbi_set_flip_vertically_on_load(true);
	int width, height, nrChannels;
	unsigned char* data = stbi_load(path, &width, &height, &nrChannels, 0);
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

class CubeMap : public Texture {
public:
	CubeMap() {};
	CubeMap(const char* folderPath, GLenum wrap, GLenum filter);
};

CubeMap::CubeMap(const char* folderPath, GLenum wrap, GLenum filter) {
	std::vector<std::string> faces;
	faces.push_back(folderPath + std::string("/right.jpg"));
	faces.push_back(folderPath + std::string("/left.jpg"));
	faces.push_back(folderPath + std::string("top.jpg"));
	faces.push_back(folderPath + std::string("bottom.jpg"));
	faces.push_back(folderPath + std::string("front.jpg"));
	faces.push_back(folderPath + std::string("back.jpg"));
	stbi_set_flip_vertically_on_load(false);
	int width, height, nrChannels;
	unsigned char* data;
	bool validFlag = true;
	glGenTextures(1, &ID);
	glBindTexture(GL_TEXTURE_CUBE_MAP, ID);
	for (int i = 0; i < faces.size(); i++) {
		data = stbi_load(faces[i].c_str(), &width, &height, &nrChannels, 0);
		if (data) {
			GLenum informat, outformat;
			if (nrChannels == 1) {
				informat = GL_RED;
				outformat = GL_RED;
			}
			else if (nrChannels == 3) {
				informat = GL_RGB;
				outformat = GL_RGB;
			}
			else if (nrChannels == 4) {
				informat = GL_RGBA;
				outformat = GL_RGBA;
			}
			glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, informat, width, height, 0, outformat, GL_UNSIGNED_BYTE, data);
			stbi_image_free(data);
		}
		else {
			validFlag = false;
			break;
		}
	}
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, wrap);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, wrap);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, wrap);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, filter);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, filter);
	glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
	if (!validFlag) {
		glDeleteTextures(1, &ID);
		ID = 0;
	}
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
