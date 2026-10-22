#ifndef TOGR_MATERIAL_HPP
#define TOGR_MATERIAL_HPP
#pragma once

#include "rendering/opengl/texture.hpp"
#include <memory>
#include <vector>
#include <string>

namespace togr {

class Shader;

// Path configuration and the textures loaded by the most recent initialization.
// Texture creation, binding and destruction require the current GL context.
class Material {
public:
    Material() {}
    Material(std::string albedoPath, std::string ambientPath, std::string specularPath,
        std::string normalPath, std::string shininessPath);
    // Clears previous textures before rebuilding from the current UTF-8 paths.
    // Empty paths use defaults; false reports requested images that failed to load.
    // Successful images remain usable, and bind() supplies defaults for missing maps.
    // Preserve an active material by initializing a separate candidate before replacement.
    bool initGLResources();
    const std::string& lastError() const { return lastError_; }
    void bind(std::shared_ptr<Shader> shader);

    // Editing these paths does not reload textures; call initGLResources() to apply.
    std::string albedoPath;
    std::string ambientPath;
    std::string specularPath;
    std::string normalPath;
    std::string shininessPath;
private:
    std::vector<Texture2D> textures; // Successful loads, not a live view of path edits.
    std::string lastError_;
};

} // namespace togr

#endif // !TOGR_MATERIAL_HPP
