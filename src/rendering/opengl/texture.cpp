#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "texture.hpp"
#include "core/logger.hpp"
#include <vector>
#include <string>

namespace {
// Image and sub-image data is tightly packed; do not leak pixel-store state.
class PackedPixelUpload {
public:
	PackedPixelUpload() {
		glGetIntegerv(GL_UNPACK_ALIGNMENT, &alignment);
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	}
	~PackedPixelUpload() { glPixelStorei(GL_UNPACK_ALIGNMENT, alignment); }
private:
	GLint alignment;
};

bool imageFormats(int channels, GLenum& internalFormat, GLenum& format) {
	switch (channels) {
	case 1: internalFormat = GL_R8; format = GL_RED; return true;
	case 2: internalFormat = GL_RG8; format = GL_RG; return true;
	case 3: internalFormat = GL_RGB8; format = GL_RGB; return true;
	case 4: internalFormat = GL_RGBA8; format = GL_RGBA; return true;
	default: return false;
	}
}
}

Texture::~Texture() {
	destroy();
}

void Texture::destroy() {
	if (ID) {
		glDeleteTextures(1, &ID);
		ID = 0;
	}
}

Texture::Texture(Texture&& other) noexcept : ID(other.ID) {
	other.ID = 0;
}

Texture& Texture::operator=(Texture&& other) noexcept {
	if (this != &other) {
		if (ID) glDeleteTextures(1, &ID);
		ID = other.ID;
		other.ID = 0;
	}
	return *this;
}

Texture2D::Texture2D(int width, int height, GLenum wrap, GLenum filter, GLenum internalFormat, GLenum format, GLenum dataType) {
	this->type = Type::BUFFER;
	glGenTextures(1, &ID);
	glBindTexture(GL_TEXTURE_2D, ID);
	glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, format, dataType, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
	glBindTexture(GL_TEXTURE_2D, 0);
	this->internalFormat = internalFormat;
	this->format = format;
	this->dataType = dataType;
}

Texture2D::Texture2D(const char* path, Type type, GLenum wrap, GLenum filter) {
	this->type = type;
	stbi_set_flip_vertically_on_load(true);
	int width, height, nrChannels;
	unsigned char* data = stbi_load(path, &width, &height, &nrChannels, 0);
	if (!data) {
		LOG_ERROR("Failed to load texture: " << path);
		return;
	}
	if (!imageFormats(nrChannels, internalFormat, format)) {
		LOG_ERROR("Unsupported texture channel count: " << nrChannels << " in " << path);
		stbi_image_free(data);
		return;
	}
	dataType = GL_UNSIGNED_BYTE;
	{
		PackedPixelUpload upload;
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

void Texture2D::use(GLenum textureUnit) {
	glActiveTexture(textureUnit);
	glBindTexture(GL_TEXTURE_2D, ID);
}

void Texture2D::setBorderColor(float r, float g, float b, float a) {
	if (!ID) return;
	const float color[] = { r, g, b, a };
	glBindTexture(GL_TEXTURE_2D, ID);
	glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, color);
	glBindTexture(GL_TEXTURE_2D, 0);
}

void Texture2D::setDefaultColor(float r, float g, float b, float a)
{
	if (!ID) return;
	const float color[] = { r, g, b, a };
	// The source is float even when the texture's storage is normalized bytes.
	glClearTexImage(ID, 0, format, GL_FLOAT, color);
}

void Texture2D::resetSize(int width, int height) {
	if (!ID) return;
	glBindTexture(GL_TEXTURE_2D, ID);
	glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, format, dataType, nullptr);
	glBindTexture(GL_TEXTURE_2D, 0);
}

void Texture2D::subImage2D(int xOffset, int yOffset, int width, int height, const void* data)
{
	if (!ID) return;
	PackedPixelUpload upload;
	glBindTexture(GL_TEXTURE_2D, ID);
	glTexSubImage2D(GL_TEXTURE_2D, 0, xOffset, yOffset, width, height, format, dataType, data);
	glBindTexture(GL_TEXTURE_2D, 0);
}

Texture2DArray::Texture2DArray(int width, int height, int length, GLenum wrap, GLenum filter, GLenum internalFormat, GLenum format) {
	glGenTextures(1, &ID);
	glBindTexture(GL_TEXTURE_2D_ARRAY, ID);
	glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, internalFormat, width, height, length, 0, format, GL_FLOAT, nullptr);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, wrap);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, wrap);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, filter);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, filter);
	glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
}

void Texture2DArray::use(GLenum textureUnit) {
	glActiveTexture(textureUnit);
	glBindTexture(GL_TEXTURE_2D_ARRAY, ID);
}

CubeMap::CubeMap(const char* folderPath, GLenum wrap, GLenum filter) {
	std::vector<std::string> faces;
	faces.push_back(folderPath + std::string("/right.jpg"));
	faces.push_back(folderPath + std::string("/left.jpg"));
	faces.push_back(folderPath + std::string("/top.jpg"));
	faces.push_back(folderPath + std::string("/bottom.jpg"));
	faces.push_back(folderPath + std::string("/front.jpg"));
	faces.push_back(folderPath + std::string("/back.jpg"));
	stbi_set_flip_vertically_on_load(false);
	int width, height, nrChannels;
	unsigned char* data;
	bool validFlag = true;
	PackedPixelUpload upload;
	glGenTextures(1, &ID);
	glBindTexture(GL_TEXTURE_CUBE_MAP, ID);
	for (int i = 0; i < faces.size(); i++) {
		data = stbi_load(faces[i].c_str(), &width, &height, &nrChannels, 0);
		if (data) {
			GLenum informat, outformat;
			if (!imageFormats(nrChannels, informat, outformat)) {
				stbi_image_free(data);
				validFlag = false;
				break;
			}
			glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, informat, width, height, 0, outformat, GL_UNSIGNED_BYTE, data);
			stbi_image_free(data);
		}
		else {
			LOG_ERROR("Failed to load cubemap face: " << faces[i]);
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

void CubeMap::use(GLenum textureUnit) {
	glActiveTexture(textureUnit);
	glBindTexture(GL_TEXTURE_CUBE_MAP, ID);
}

CubeMapArray::CubeMapArray(int width, int height, int length, GLenum wrap, GLenum filter, GLenum internalFormat, GLenum format) {
	glGenTextures(1, &ID);
	glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, ID);
	glTexImage3D(GL_TEXTURE_CUBE_MAP_ARRAY, 0, internalFormat, width, height, length * 6, 0, format, GL_FLOAT, nullptr);
	glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_S, wrap);
	glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_T, wrap);
	glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_R, wrap);
	glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_MIN_FILTER, filter);
	glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_MAG_FILTER, filter);
	glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, 0);
}

void CubeMapArray::use(GLenum textureUnit) {
	glActiveTexture(textureUnit);
	glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, ID);
}

RenderBuffer::RenderBuffer(int width, int height, GLenum internalFormat) {
	glGenRenderbuffers(1, &ID);
	glBindRenderbuffer(GL_RENDERBUFFER, ID);
	glRenderbufferStorage(GL_RENDERBUFFER, internalFormat, width, height);
	glBindRenderbuffer(GL_RENDERBUFFER, 0);
	this->internalFormat = internalFormat;
}

RenderBuffer::~RenderBuffer() {
	if (ID) {
		glDeleteRenderbuffers(1, &ID);
		ID = 0;
	}
}

RenderBuffer::RenderBuffer(RenderBuffer&& other) noexcept : ID(other.ID), internalFormat(other.internalFormat) {
	other.ID = 0;
}

RenderBuffer& RenderBuffer::operator=(RenderBuffer&& other) noexcept {
	if (this != &other) {
		if (ID) glDeleteRenderbuffers(1, &ID);
		ID = other.ID;
		internalFormat = other.internalFormat;
		other.ID = 0;
	}
	return *this;
}

void RenderBuffer::resetSize(int width, int height) {
	glBindRenderbuffer(GL_RENDERBUFFER, ID);
	glRenderbufferStorage(GL_RENDERBUFFER, internalFormat, width, height);
	glBindRenderbuffer(GL_RENDERBUFFER, 0);
}

Texture3D::Texture3D(int width, int height, int depth, GLenum wrap, GLenum filter, GLenum internalFormat, GLenum format, GLenum dataType) {
	this->format = format;
	this->dataType = dataType;
	glGenTextures(1, &ID);
	glBindTexture(GL_TEXTURE_3D, ID);
	glTexImage3D(GL_TEXTURE_3D, 0, internalFormat, width, height, depth, 0, format, dataType, nullptr);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, wrap);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, wrap);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, wrap);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, filter);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, filter);
	glBindTexture(GL_TEXTURE_3D, 0);
}

void Texture3D::subImage3D(int xOffset, int yOffset, int zOffset, int width, int height, int depth, const void* data) {
	if (!ID) return;
	PackedPixelUpload upload;
	glBindTexture(GL_TEXTURE_3D, ID);
	glTexSubImage3D(GL_TEXTURE_3D, 0, xOffset, yOffset, zOffset, width, height, depth, format, dataType, data);
	glBindTexture(GL_TEXTURE_3D, 0);
}

void Texture3D::use(GLenum textureUnit) {
	glActiveTexture(textureUnit);
	glBindTexture(GL_TEXTURE_3D, ID);
}

namespace {
	GLuint dummy = 0;
}

void destroyDummyTexture2D() {
	if (dummy) glDeleteTextures(1, &dummy);
	dummy = 0;
}

void bindDummyTexture2D(GLenum textureUnit) {
	if (!dummy) {
		const unsigned char white[4] = { 255, 255, 255, 255 };
		glGenTextures(1, &dummy);
		glBindTexture(GL_TEXTURE_2D, dummy);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, white);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glBindTexture(GL_TEXTURE_2D, 0);
	}
	glActiveTexture(textureUnit);
	glBindTexture(GL_TEXTURE_2D, dummy);
}
