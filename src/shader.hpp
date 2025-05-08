#ifndef SHADER_HPP
#define SHADER_HPP
#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <fstream>
#include <sstream>
#include <iostream>
#include <regex>

class Shader
{
public:
    GLuint ID;
    Shader(const char* vertexShaderPath, const char* fragmentShaderPath);
    Shader(const char* vertexShaderPath, const char* geometryShaderPath, const char* fragmentShaderPath);
    void reCompile();
    void use();
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

std::string readShaderFile(const char* shaderFilePath) {
    std::ifstream shaderFile(shaderFilePath);
    std::stringstream fileStream;
    fileStream << shaderFile.rdbuf();
    shaderFile.close();
    return fileStream.str();
}

std::string Shader::preprocessShader(const std::string shaderContent) {
    std::string processedContent = shaderContent;
    std::regex includeRegex(R"###(#include\s*"([^"]+)")###");
    std::smatch match;

    while (std::regex_search(processedContent, match, includeRegex)) {
        std::string includeFile = match[1].str();
        std::string includeContent = readShaderFile(includeFile.c_str());
        processedContent.replace(match.position(0), match.length(0), includeContent);
    }
    return processedContent;
}

Shader::Shader(const char* vertexShaderPath, const char* fragmentShaderPath) {
	this->vertexShaderPath = vertexShaderPath;
	this->fragmentShaderPath = fragmentShaderPath;

    GLuint vShader = glCreateShader(GL_VERTEX_SHADER);
    GLuint fShader = glCreateShader(GL_FRAGMENT_SHADER);

    std::string vShaderContent = readShaderFile(vertexShaderPath);
    std::string fShaderContent = readShaderFile(fragmentShaderPath);
    fShaderContent = preprocessShader(fShaderContent);

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
	this->vertexShaderPath = vertexShaderPath;
	this->geometryShaderPath = geometryShaderPath;
	this->fragmentShaderPath = fragmentShaderPath;

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

void Shader::reCompile() {
    glDeleteProgram(ID);
    GLuint vShader = glCreateShader(GL_VERTEX_SHADER);
    GLuint fShader = glCreateShader(GL_FRAGMENT_SHADER);
    std::string vShaderContent = readShaderFile(vertexShaderPath.c_str());
    std::string fShaderContent = readShaderFile(fragmentShaderPath.c_str());
    fShaderContent = preprocessShader(fShaderContent);
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

void Shader::changeSettings(const char* name, bool value)
{
    const std::string filename = "data/shader/settings.glsl";
    std::ifstream in(filename);
    if (!in.is_open()) return;
    std::stringstream buffer;
    buffer << in.rdbuf();
    in.close();
    std::string content = buffer.str();

    std::regex defineRe(R"(^(\s*//\s*)?#define\s+)" + std::string(name) + R"(\b.*$)",
        std::regex_constants::ECMAScript);

    auto replacer = [&](const std::smatch& m) -> std::string {
        std::string prefix = m[1].matched ? m[1].str() : "";
        if (value) {
            if (prefix.find("//") == std::string::npos)
                return m.str();
            return m.str().substr(prefix.size());
        }
        else {
            if (prefix.find("//") != std::string::npos)
                return m.str();
            return "//" + m.str();
        }
        };

    std::string result;
    std::sregex_iterator it(content.begin(), content.end(), defineRe), end;
    size_t lastPos = 0;
    for (; it != end; ++it) {
        std::smatch m = *it;
        result += content.substr(lastPos, m.position() - lastPos);
        result += replacer(m);
        lastPos = m.position() + m.length();
    }
    result += content.substr(lastPos);
    std::ofstream out(filename);
    out << result;
}

using ShaderPtr = std::shared_ptr<Shader>;
#endif