#ifndef ASSIMPFILESYSTEM_HPP
#define ASSIMPFILESYSTEM_HPP
#pragma once

#include <assimp/IOSystem.hpp>

// Assimp exchanges UTF-8 filenames; file access uses native filesystem paths.
// Ownership is transferred to Assimp::Importer::SetIOHandler().
class AssimpFileSystem final : public Assimp::IOSystem {
public:
	bool Exists(const char* file) const override;
	char getOsSeparator() const override;
	Assimp::IOStream* Open(const char* file, const char* mode = "rb") override;
	void Close(Assimp::IOStream* stream) override;
	bool ComparePaths(const char* first, const char* second) const override;
};

#endif // !ASSIMPFILESYSTEM_HPP
