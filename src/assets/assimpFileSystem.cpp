#include "assimpFileSystem.hpp"
#include <assimp/IOStream.hpp>
#include <cstdio>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>

namespace {
namespace fs = std::filesystem;
using File = std::unique_ptr<std::FILE, decltype(&std::fclose)>;

int seekFile(std::FILE* file, std::int64_t offset, int origin) {
#ifdef _WIN32
	return _fseeki64(file, offset, origin);
#else
	return fseeko(file, offset, origin);
#endif
}

std::int64_t tellFile(std::FILE* file) {
#ifdef _WIN32
	return _ftelli64(file);
#else
	return ftello(file);
#endif
}

class AssimpFileStream final : public Assimp::IOStream {
public:
	explicit AssimpFileStream(File file) : file(std::move(file)) {}
	size_t Read(void* buffer, size_t size, size_t count) override {
		return std::fread(buffer, size, count, file.get());
	}
	size_t Write(const void* buffer, size_t size, size_t count) override {
		return std::fwrite(buffer, size, count, file.get());
	}
	aiReturn Seek(size_t offset, aiOrigin origin) override {
		int nativeOrigin;
		switch (origin) {
		case aiOrigin_SET: nativeOrigin = SEEK_SET; break;
		case aiOrigin_CUR: nativeOrigin = SEEK_CUR; break;
		case aiOrigin_END: nativeOrigin = SEEK_END; break;
		default: return aiReturn_FAILURE;
		}
		// Assimp represents negative offsets from the end through size_t.
		return seekFile(file.get(), static_cast<std::int64_t>(offset), nativeOrigin) == 0
			? aiReturn_SUCCESS : aiReturn_FAILURE;
	}
	size_t Tell() const override {
		const auto position = tellFile(file.get());
		return position < 0 ? 0 : static_cast<size_t>(position);
	}
	size_t FileSize() const override {
		const auto position = tellFile(file.get());
		if (position < 0 || seekFile(file.get(), 0, SEEK_END) != 0) return 0;
		const auto size = tellFile(file.get());
		if (seekFile(file.get(), position, SEEK_SET) != 0 || size < 0) return 0;
		return static_cast<size_t>(size);
	}
	void Flush() override { std::fflush(file.get()); }
private:
	File file;
};
}

bool AssimpFileSystem::Exists(const char* file) const {
	if (!file || !*file) return false;
	std::error_code error;
	return fs::is_regular_file(fs::u8path(file), error);
}

char AssimpFileSystem::getOsSeparator() const {
	return static_cast<char>(fs::path::preferred_separator);
}

Assimp::IOStream* AssimpFileSystem::Open(const char* file, const char* mode) {
	if (!file || !*file || !mode) return nullptr;
	const auto path = fs::u8path(file);
	std::FILE* handle = nullptr;
#ifdef _WIN32
	const std::string modeText(mode);
	const std::wstring nativeMode(modeText.begin(), modeText.end());
	_wfopen_s(&handle, path.c_str(), nativeMode.c_str());
#else
	handle = std::fopen(path.c_str(), mode);
#endif
	if (!handle) return nullptr;
	return new AssimpFileStream(File(handle, &std::fclose));
}

void AssimpFileSystem::Close(Assimp::IOStream* stream) { delete stream; }

bool AssimpFileSystem::ComparePaths(const char* first, const char* second) const {
	if (!first || !second) return false;
	const auto left = fs::u8path(first), right = fs::u8path(second);
	std::error_code error;
	const bool sameFile = fs::equivalent(left, right, error);
	if (!error) return sameFile;
	const auto absoluteLeft = fs::absolute(left, error);
	if (error) return false;
	const auto absoluteRight = fs::absolute(right, error);
	return !error && absoluteLeft.lexically_normal() == absoluteRight.lexically_normal();
}
