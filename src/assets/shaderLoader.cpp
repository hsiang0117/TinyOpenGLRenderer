#include "shaderLoader.hpp"
#include "rendering/opengl/shader.hpp"
#include <stdexcept>
#include <utility>

namespace togr {

void ShaderLoader::registerShader(const std::string& key, const char* vertexPath, const char* fragmentPath) {
    if (hasCachedShader(key)) {
        return;
    }

    auto shader = std::make_shared<Shader>(vertexPath, fragmentPath);
    if (!shader->isValid()) {
        throw std::runtime_error(shader->lastError());
    }
    shaderCache_[key] = std::move(shader);
}

void ShaderLoader::registerShader(const std::string& key, const char* vertexPath,
    const char* geometryPath, const char* fragmentPath) {
    if (hasCachedShader(key)) {
        return;
    }

    auto shader = std::make_shared<Shader>(vertexPath, geometryPath, fragmentPath);
    if (!shader->isValid()) {
        throw std::runtime_error(shader->lastError());
    }
    shaderCache_[key] = std::move(shader);
}

std::shared_ptr<Shader> ShaderLoader::get(const std::string& key) const {
    auto found = shaderCache_.find(key);
    return found != shaderCache_.end() ? found->second : nullptr;
}

void ShaderLoader::shutdown() {
    shaderCache_.clear();
}

bool ShaderLoader::hasCachedShader(const std::string& key) const {
    return shaderCache_.find(key) != shaderCache_.end();
}

} // namespace togr
