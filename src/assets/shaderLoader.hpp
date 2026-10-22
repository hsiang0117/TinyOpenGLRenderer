#ifndef TOGR_SHADERLOADER_HPP
#define TOGR_SHADERLOADER_HPP
#pragma once

#include <memory>
#include <string>
#include <unordered_map>

namespace togr {

class Shader;

// Single-owner-thread cache. Registration and release require a current GL context.
class ShaderLoader {
public:
    ShaderLoader() = default;

    // UTF-8 source paths. Existing keys are left unchanged without reading or
    // compiling the supplied paths. A new program is published only after it is
    // valid; read/compile/link failures throw std::runtime_error and leave no entry.
    void registerShader(const std::string& key, const char* vertexPath, const char* fragmentPath);
    void registerShader(const std::string& key, const char* vertexPath, const char* geometryPath, const char* fragmentPath);

    // Shared ownership; a missing key returns nullptr. The returned program may
    // outlive this cache, but its last owner must release it under a current context.
    std::shared_ptr<Shader> get(const std::string& key) const;

    // Drop cache references. Repeated calls are harmless; external owners remain.
    void shutdown();

private:
    bool hasCachedShader(const std::string& key) const;

    std::unordered_map<std::string, std::shared_ptr<Shader>> shaderCache_;
};

} // namespace togr

#endif // !TOGR_SHADERLOADER_HPP
