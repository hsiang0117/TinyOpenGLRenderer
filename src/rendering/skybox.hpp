#ifndef TOGR_SKYBOX_HPP
#define TOGR_SKYBOX_HPP
#pragma once

#include "rendering/opengl/texture.hpp"
#include <glad/glad.h>
#include <filesystem>
#include <string>

namespace togr {

class Skybox {
public:
    // UTF-8 folder configuration only; construction does not create GL resources.
    Skybox(std::string folderPath);
    // Destruction and replacement of owned GL resources require a current context.
    ~Skybox();
    Skybox(const Skybox&) = delete;
    Skybox& operator=(const Skybox&) = delete;
    Skybox(Skybox&& other) noexcept;
    Skybox& operator=(Skybox&& other) noexcept;
    // Requires the current GL context. Ready objects return true without reloading.
    // Failure leaves this object unready and records a reason; a later call retries.
    // Replace a working skybox by initializing a separate candidate first.
    bool initGLResources();
    bool isReady() const { return glInitialized; }
    const std::string& lastError() const { return lastError_; }
    // Require the current GL context; both are no-ops until initialization succeeds.
    void draw();
    void useCubeMap();
private:
    std::filesystem::path folderPath;
    bool glInitialized = false;
    std::string lastError_;
    CubeMap cubemap;
    GLuint VAO = 0, VBO = 0;
    float vertices[108] = {
        -1.0f,  1.0f, -1.0f,
        -1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,

         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,

        -1.0f,  1.0f, -1.0f,
         1.0f,  1.0f, -1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f,  1.0f
    };
};

} // namespace togr

#endif
