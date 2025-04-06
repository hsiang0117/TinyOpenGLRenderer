#ifndef SHADER_HPP
#define SHADER_HPP
#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <fstream>
#include <sstream>
#include <iostream>

class Shader
{
public:
    GLuint ID;
    Shader(const char* vertexShaderPath, const char* fragmentShaderPath);
    Shader(const char* vertexShaderPath, const char* geometryShaderPath, const char* fragmentShaderPath);
    void use();
    void setVec3(const char* name, glm::vec3 vec);
    void setMat4(const char* name, glm::mat4 mat);
    void setFloat(const char* name, float value);
    void setInt(const char* name, int value);
    void setBool(const char* name, bool value);
};

std::string readShaderFile(const char* shaderFilePath) {
    std::ifstream shaderFile(shaderFilePath);
    std::stringstream fileStream;
    fileStream << shaderFile.rdbuf();
    shaderFile.close();
    return fileStream.str();
}

Shader::Shader(const char* vertexShaderPath, const char* fragmentShaderPath) {
    GLuint vShader = glCreateShader(GL_VERTEX_SHADER);
    GLuint fShader = glCreateShader(GL_FRAGMENT_SHADER);

    std::string vShaderContent = readShaderFile(vertexShaderPath);
    std::string fShaderContent = readShaderFile(fragmentShaderPath);

    const char* vshaderCode = vShaderContent.c_str();
    const char* fshaderCode = fShaderContent.c_str();

    glShaderSource(vShader, 1, &vshaderCode, NULL);
    glShaderSource(fShader, 1, &fshaderCode, NULL);

    int success;
    char infoLog[512];

    glCompileShader(vShader);
    glGetShaderiv(vShader, GL_COMPILE_STATUS, &success);
    if (!success) {
		glGetShaderInfoLog(vShader, 512, NULL, infoLog);
		std::cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << infoLog << std::endl;
    }
    glCompileShader(fShader);
	glGetShaderiv(fShader, GL_COMPILE_STATUS, &success);
	if (!success) {
		glGetShaderInfoLog(fShader, 512, NULL, infoLog);
		std::cout << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n" << infoLog << std::endl;
	}

    ID = glCreateProgram();
    glAttachShader(ID, vShader);
    glAttachShader(ID, fShader);
    glLinkProgram(ID);
    glGetProgramiv(ID, GL_LINK_STATUS, &success);
    if (!success)
    {
        glGetProgramInfoLog(ID, 512, NULL, infoLog);
        std::cout << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
    }
    glDeleteShader(vShader);
    glDeleteShader(fShader);
}

Shader::Shader(const char* vertexShaderPath, const char* geometryShaderPath, const char* fragmentShaderPath) {
    GLuint vShader = glCreateShader(GL_VERTEX_SHADER);
    GLuint gShader = glCreateShader(GL_GEOMETRY_SHADER);
    GLuint fShader = glCreateShader(GL_FRAGMENT_SHADER);

    std::string vShaderContent = readShaderFile(vertexShaderPath);
    std::string gShaderContent = readShaderFile(geometryShaderPath);
    std::string fShaderContent = readShaderFile(fragmentShaderPath);

    const char* vshaderCode = vShaderContent.c_str();
    const char* gshaderCode = gShaderContent.c_str();
    const char* fshaderCode = fShaderContent.c_str();

    glShaderSource(vShader, 1, &vshaderCode, NULL);
    glShaderSource(gShader, 1, &gshaderCode, NULL);
    glShaderSource(fShader, 1, &fshaderCode, NULL);

    int success;
    char infoLog[512];

    glCompileShader(vShader);
    glGetShaderiv(vShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(vShader, 512, NULL, infoLog);
        std::cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << infoLog << std::endl;
    }
    glCompileShader(gShader);
	glGetShaderiv(gShader, GL_COMPILE_STATUS, &success);
	if (!success) {
		glGetShaderInfoLog(gShader, 512, NULL, infoLog);
		std::cout << "ERROR::SHADER::GEOMETRY::COMPILATION_FAILED\n" << infoLog << std::endl;
	}
    glCompileShader(fShader);
	glGetShaderiv(fShader, GL_COMPILE_STATUS, &success);
	if (!success) {
		glGetShaderInfoLog(fShader, 512, NULL, infoLog);
		std::cout << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n" << infoLog << std::endl;
	}

    ID = glCreateProgram();
    glAttachShader(ID, vShader);
    glAttachShader(ID, gShader);
    glAttachShader(ID, fShader);
    glLinkProgram(ID);
    glGetProgramiv(ID, GL_LINK_STATUS, &success);
    if (!success)
    {
        glGetProgramInfoLog(ID, 512, NULL, infoLog);
        std::cout << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
    }
    glDeleteShader(vShader);
    glDeleteShader(gShader);
    glDeleteShader(fShader);
}

void Shader::use() {
    glUseProgram(ID);
}

void Shader::setVec3(const char* name, glm::vec3 vec) {
    int location = glGetUniformLocation(ID, name);
    glUniform3fv(location, 1, glm::value_ptr(vec));
}

void Shader::setMat4(const char* name, glm::mat4 mat) {
    int location = glGetUniformLocation(ID, name);
    glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(mat));
}

void Shader::setFloat(const char* name, float value) {
    int location = glGetUniformLocation(ID, name);
    glUniform1f(location, value);
}

void Shader::setInt(const char* name, int value) {
    int location = glGetUniformLocation(ID, name);
    glUniform1i(location, value);
}

void Shader::setBool(const char* name, bool value) {
    int location = glGetUniformLocation(ID, name);
    glUniform1i(location, value);
}

using ShaderPtr = std::shared_ptr<Shader>;
#endif