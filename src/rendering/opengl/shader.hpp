#ifndef SHADER_HPP
#define SHADER_HPP
#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <string>
#include <memory>
#include <map>
#include <functional>

class Shader
{
public:
    GLuint ID = 0;
    Shader(const char* vertexShaderPath, const char* fragmentShaderPath);
    Shader(const char* vertexShaderPath, const char* geometryShaderPath, const char* fragmentShaderPath);
    ~Shader();
    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;
    Shader(Shader&& other) noexcept;
    Shader& operator=(Shader&& other) noexcept;
    bool reload();
    bool isValid() const { return valid_; }
    const std::string& lastError() const { return lastError_; }
    bool settingEnabled(const char* name) const;
    // Compile and save before replacing the active program. Failure changes neither.
    bool applySetting(const char* name, bool value);
    void use();
    void setVec2(const char* name, glm::vec2 vec);
    void setVec3(const char* name, glm::vec3 vec);
    void setMat4(const char* name, glm::mat4 mat);
    void setFloat(const char* name, float value);
    void setInt(const char* name, int value);
    void setBool(const char* name, bool value);
    bool hasUniform(const char* name);
    static bool changeSettings(const char* name, bool value);
private:
    Shader() = default;
    void compile(const std::string* settingsOverride = nullptr);
    GLint uniformLocation(const char* name);
    // Transparent lookup avoids a temporary string on every uniform update.
    std::map<std::string, GLint, std::less<>> uniformLocations;
    std::string preprocessShader(std::string shaderContent, const std::string* settingsOverride);
    bool valid_ = false;
    std::string lastError_;
    std::map<std::string, bool, std::less<>> compiledSettings_;
    std::string vertexShaderPath;
    std::string fragmentShaderPath;
    std::string geometryShaderPath;
};

using ShaderPtr = std::shared_ptr<Shader>;
#endif
