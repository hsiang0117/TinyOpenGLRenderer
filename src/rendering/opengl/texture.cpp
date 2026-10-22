#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "texture.hpp"
#include "core/logger.hpp"
#include <cstdio>
#include <array>
#include <memory>
#include <utility>

namespace togr {

namespace {
unsigned char* loadImage(const std::filesystem::path& path, int& width, int& height, int& channels, std::string& error) {
	if (path.empty()) { error = "Image path is empty"; return nullptr; }
	FILE* handle = nullptr;
#ifdef _WIN32
	_wfopen_s(&handle, path.c_str(), L"rb");
#else
	handle = std::fopen(path.c_str(), "rb");
#endif
	const std::unique_ptr<FILE, decltype(&std::fclose)> file(handle, &std::fclose);
	if (!file) { error = "Cannot open image: " + path.u8string(); return nullptr; }
	auto data = stbi_load_from_file(file.get(), &width, &height, &channels, 0);
	if (!data) {
		const char* reason = stbi_failure_reason();
		error = "Cannot decode image: " + path.u8string() + (reason ? ": " + std::string(reason) : "");
	}
	return data;
}

struct ImageDeleter {
	void operator()(unsigned char* pixels) const { stbi_image_free(pixels); }
};
using Image = std::unique_ptr<unsigned char, ImageDeleter>;

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

void Texture::fail(std::string reason) {
	destroy();
	lastError_ = std::move(reason);
	TOGR_LOG_ERROR(lastError_);
}

Texture::Texture(Texture&& other) noexcept : ID(other.ID), lastError_(std::move(other.lastError_)) {
	other.ID = 0;
}

Texture& Texture::operator=(Texture&& other) noexcept {
	if (this != &other) {
		if (ID) glDeleteTextures(1, &ID);
		ID = other.ID;
		lastError_ = std::move(other.lastError_);
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

Texture2D::Texture2D(const char* path, Type type, GLenum wrap, GLenum filter)
	: Texture2D(std::filesystem::u8path(path ? path : ""), type, wrap, filter) {}

Texture2D::Texture2D(const std::filesystem::path& path, Type type, GLenum wrap, GLenum filter) {
	this->type = type;
	stbi_set_flip_vertically_on_load(true);
	int width, height, nrChannels;
	std::string error;
	Image data(loadImage(path, width, height, nrChannels, error));
	if (!data) {
		fail(std::move(error));
		return;
	}
	if (!imageFormats(nrChannels, internalFormat, format)) {
		fail("Unsupported texture channel count in " + path.u8string());
		return;
	}
	GLint maximumSize = 0;
	glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maximumSize);
	if (width <= 0 || height <= 0 || width > maximumSize || height > maximumSize) {
		fail("Texture dimensions exceed GPU limits: " + path.u8string());
		return;
	}
	dataType = GL_UNSIGNED_BYTE;
	{
		PackedPixelUpload upload;
		glGenTextures(1, &ID);
		if (!ID) { fail("Cannot create texture: " + path.u8string()); return; }
		glBindTexture(GL_TEXTURE_2D, ID);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
		glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, format, GL_UNSIGNED_BYTE, data.get());
		GLint uploadedWidth = 0, uploadedHeight = 0;
		glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &uploadedWidth);
		glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &uploadedHeight);
		if (!ID || uploadedWidth != width || uploadedHeight != height) {
			glBindTexture(GL_TEXTURE_2D, 0);
			fail("GPU texture allocation failed: " + path.u8string());
			return;
		}
		glGenerateMipmap(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, 0);
	}
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

CubeMap::CubeMap(const char* folderPath, GLenum wrap, GLenum filter)
	: CubeMap(std::filesystem::u8path(folderPath ? folderPath : ""), wrap, filter) {}

CubeMap::CubeMap(const std::filesystem::path& folderPath, GLenum wrap, GLenum filter) {
	if (folderPath.empty()) {
		fail("Cubemap directory is empty");
		return;
	}
	const char* faces[] = { "right.jpg", "left.jpg", "top.jpg", "bottom.jpg", "front.jpg", "back.jpg" };
	stbi_set_flip_vertically_on_load(false);
	std::array<Image, 6> images;
	int faceSize = 0, faceChannels = 0;
	GLenum internalFormat = GL_RGBA8, format = GL_RGBA;
	GLint maximumSize = 0;
	glGetIntegerv(GL_MAX_CUBE_MAP_TEXTURE_SIZE, &maximumSize);
	for (int i = 0; i < 6; ++i) {
		const auto facePath = folderPath / faces[i];
		int width, height, channels;
		std::string error;
		images[i].reset(loadImage(facePath, width, height, channels, error));
		if (!images[i]) { fail(std::move(error)); return; }
		if (width <= 0 || width != height || width > maximumSize) {
			fail("Cubemap face must be square and within GPU size limits: " + facePath.u8string());
			return;
		}
		if (i == 0) {
			faceSize = width; faceChannels = channels;
			if (!imageFormats(channels, internalFormat, format)) {
				fail("Unsupported cubemap face channels: " + facePath.u8string());
				return;
			}
		} else if (width != faceSize || channels != faceChannels) {
			fail("Cubemap face dimensions or format differ from right.jpg: " + facePath.u8string());
			return;
		}
	}
	PackedPixelUpload upload;
	glGenTextures(1, &ID);
	if (!ID) { fail("Cannot create cubemap: " + folderPath.u8string()); return; }
	glBindTexture(GL_TEXTURE_CUBE_MAP, ID);
	for (int i = 0; i < 6; i++) {
		glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, internalFormat, faceSize, faceSize, 0, format, GL_UNSIGNED_BYTE, images[i].get());
		GLint uploadedWidth = 0;
		glGetTexLevelParameteriv(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_TEXTURE_WIDTH, &uploadedWidth);
		if (!ID || uploadedWidth != faceSize) {
			glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
			fail("GPU cubemap allocation failed: " + (folderPath / faces[i]).u8string());
			return;
		}
	}
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, wrap);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, wrap);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, wrap);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, filter);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, filter);
	glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
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
	GLuint dummyCubeMap = 0;
}

void destroyDummyCubeMap() {
	if (dummyCubeMap) glDeleteTextures(1, &dummyCubeMap);
	dummyCubeMap = 0;
}

void bindDummyCubeMap(GLenum textureUnit) {
	glActiveTexture(textureUnit);
	if (!dummyCubeMap) {
		const unsigned char black[4] = {0, 0, 0, 255};
		glGenTextures(1, &dummyCubeMap);
		glBindTexture(GL_TEXTURE_CUBE_MAP, dummyCubeMap);
		for (int face = 0; face < 6; ++face)
			glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, black);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	}
	glBindTexture(GL_TEXTURE_CUBE_MAP, dummyCubeMap);
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

} // namespace togr
