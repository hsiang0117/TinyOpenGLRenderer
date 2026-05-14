#ifndef SHADER_HPP
#define SHADER_HPP
#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <string>
#include <memory>

class Shader
{
public:
    GLuint ID;
    Shader(const char* vertexShaderPath, const char* fragmentShaderPath);
    Shader(const char* vertexShaderPath, const char* geometryShaderPath, const char* fragmentShaderPath);
    void reCompile();
    void use();
    void setVec2(const char* name, glm::vec2 vec);
    void setVec3(const char* name, glm::vec3 vec);
    void setMat4(const char* name, glm::mat4 mat);
    void setFloat(const char* name, float value);
    void setInt(const char* name, int value);
    void setBool(const char* name, bool value);
    static void changeSettings(const char* name, bool value);
private:
    std::string preprocessShader(const std::string shaderContent);
    std::string vertexShaderPath;
    std::string fragmentShaderPath;
    std::string geometryShaderPath;
};

using ShaderPtr = std::shared_ptr<Shader>;
#endif
