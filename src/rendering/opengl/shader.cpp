#include "shader.hpp"
#include "assets/shaderCatalog.hpp"
#include "core/logger.hpp"
#include <glm/gtc/type_ptr.hpp>
#include <array>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <regex>
#include <stdexcept>
#include <utility>
#include <vector>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifdef APIENTRY
#undef APIENTRY
#endif
#include <windows.h>
#endif

namespace {
using ShaderCatalog::settingsPath;
using ShaderCatalog::defaultsPath;

std::string readShaderFile(const char* path) {
    std::ifstream input(path);
    if (!input) throw std::runtime_error(std::string("Cannot read shader file: ") + path);
    std::ostringstream contents; contents << input.rdbuf();
    if (input.bad()) throw std::runtime_error(std::string("Failed reading shader file: ") + path);
    return contents.str();
}

std::string withoutComments(std::string source) {
    static const std::regex comments(R"(//[^\n]*|/\*[\s\S]*?\*/)");
    for (std::sregex_iterator it(source.begin(), source.end(), comments), end; it != end; ++it) {
        const auto start = it->position(), finish = start + it->length();
        for (auto i = start; i < finish; ++i) if (source[i] != '\n' && source[i] != '\r') source[i] = ' ';
    }
    return source;
}

std::string settingsSource() {
    return readShaderFile(std::filesystem::exists(settingsPath) ? settingsPath : defaultsPath);
}

bool knownSetting(const char* name) {
    for (auto setting : {"PCF_SHADOW", "USE_HDR", "USE_BLOOM", "USE_ENVIRONMENT_MAPPING"})
        if (std::string(name) == setting) return true;
    return false;
}

std::string updateSetting(const std::string& source, const char* name, bool enabled) {
    if (!knownSetting(name)) throw std::runtime_error(std::string("Unknown render setting: ") + name);
    std::regex linePattern("^\\s*(//\\s*)?#\\s*(define|undef)\\s+" + std::string(name) + "\\b.*$");
    std::istringstream input(source); std::string line, result;
    bool found = false;
    while (std::getline(input, line)) {
        if (std::regex_match(line, linePattern)) {
            line = std::string(enabled ? "#define " : "//#define ") + name;
            found = true;
        }
        result += line + '\n';
    }
    if (!found) result += std::string(enabled ? "#define " : "//#define ") + name + '\n';
    return result;
}

// Write beside the destination, then replace atomically; a failed write never
// truncates the user's previous configuration. Both files are on the same volume.
bool saveSettings(const std::string& source, std::string& error) {
    const std::string temporary = std::string(settingsPath) + ".tmp";
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        bool created = output.is_open();
        if (created) { output << source; output.close(); }
        if (!output) {
            error = "Cannot save render settings: " + temporary;
            if (created) { std::error_code ec; std::filesystem::remove(temporary, ec); }
            return false;
        }
    }
    std::error_code ec;
#ifdef _WIN32
    // std::filesystem::rename does not replace an existing file on Windows.
    // MoveFileExW performs the replacement without a delete/truncate window.
    if (!MoveFileExW(std::filesystem::path(temporary).c_str(), std::filesystem::path(settingsPath).c_str(),
        MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        error = "Cannot replace render settings (Windows error " + std::to_string(GetLastError()) + "): " + settingsPath;
        std::filesystem::remove(temporary, ec); return false;
    }
#else
    std::filesystem::rename(temporary, settingsPath, ec);
    if (ec) { error = "Cannot replace render settings: " + ec.message(); std::filesystem::remove(temporary, ec); return false; }
#endif
    return true;
}

std::map<std::string, bool, std::less<>> definedSettings(const std::string& source) {
    // Configuration flags are flat #define/#undef directives. Ignore comments,
    // including multi-line blocks, when recording the successfully compiled state.
    auto clean = withoutComments(source);
    std::istringstream input(clean); std::string line;
    std::regex directive(R"(^\s*#\s*(define|undef)\s+(\w+)\b.*$)");
    std::map<std::string, bool, std::less<>> result;
    while (std::getline(input, line)) {
        std::smatch match;
        if (std::regex_match(line, match, directive)) result[match[2].str()] = match[1].str() == "define";
    }
    return result;
}
}

Shader::~Shader() { if (ID) glDeleteProgram(ID); }

Shader::Shader(Shader&& other) noexcept
    : ID(std::exchange(other.ID, 0)), uniformLocations(std::move(other.uniformLocations)),
      valid_(std::exchange(other.valid_, false)), lastError_(std::move(other.lastError_)),
      compiledSettings_(std::move(other.compiledSettings_)), vertexShaderPath(std::move(other.vertexShaderPath)),
      fragmentShaderPath(std::move(other.fragmentShaderPath)), geometryShaderPath(std::move(other.geometryShaderPath)) {}

Shader& Shader::operator=(Shader&& other) noexcept {
    if (this != &other) {
        if (ID) glDeleteProgram(ID);
        ID = std::exchange(other.ID, 0); valid_ = std::exchange(other.valid_, false);
        uniformLocations = std::move(other.uniformLocations); compiledSettings_ = std::move(other.compiledSettings_);
        lastError_ = std::move(other.lastError_);
        vertexShaderPath = std::move(other.vertexShaderPath); fragmentShaderPath = std::move(other.fragmentShaderPath);
        geometryShaderPath = std::move(other.geometryShaderPath);
    }
    return *this;
}

std::string Shader::preprocessShader(std::string content, const std::string* settingsOverride) {
    content = withoutComments(std::move(content));
    const std::regex includePattern(R"###(^[ \t]*#[ \t]*include[ \t]*"([^"]+)")###", std::regex::ECMAScript | std::regex::multiline);
    std::smatch match; int expansions = 0;
    while (std::regex_search(content, match, includePattern)) {
        if (++expansions > 64) throw std::runtime_error("Shader include cycle or expansion limit exceeded: " + match[1].str());
        std::string path = match[1].str();
        std::string included = path == settingsPath
            ? (settingsOverride ? *settingsOverride : settingsSource()) : readShaderFile(path.c_str());
        content.replace(match.position(), match.length(), withoutComments(std::move(included)));
    }
    return content;
}

Shader::Shader(const char* vertex, const char* fragment)
    : vertexShaderPath(vertex), fragmentShaderPath(fragment) { compile(); }

Shader::Shader(const char* vertex, const char* geometry, const char* fragment)
    : vertexShaderPath(vertex), fragmentShaderPath(fragment), geometryShaderPath(geometry) { compile(); }

void Shader::compile(const std::string* settingsOverride) {
    std::array<GLuint, 3> handles{};
    try {
        struct Stage { GLenum type; std::string path, source; };
        std::vector<Stage> stages = {{GL_VERTEX_SHADER, vertexShaderPath, {}}};
        if (!geometryShaderPath.empty()) stages.push_back({GL_GEOMETRY_SHADER, geometryShaderPath, {}});
        stages.push_back({GL_FRAGMENT_SHADER, fragmentShaderPath, {}});
        std::string combined;
        for (auto& stage : stages) {
            stage.source = preprocessShader(readShaderFile(stage.path.c_str()), settingsOverride);
            combined += stage.source + '\n';
        }
        bool compiled = true;
        for (std::size_t i = 0; i < stages.size(); ++i) {
            handles[i] = glCreateShader(stages[i].type); const char* source = stages[i].source.c_str();
            glShaderSource(handles[i], 1, &source, nullptr); glCompileShader(handles[i]);
            GLint status = GL_FALSE; glGetShaderiv(handles[i], GL_COMPILE_STATUS, &status);
            if (!status) {
                char message[4096]{}; glGetShaderInfoLog(handles[i], sizeof(message), nullptr, message);
                lastError_ += "Shader compilation failed: " + stages[i].path + "\n" + message + '\n'; compiled = false;
            }
        }
        if (compiled) {
            ID = glCreateProgram(); for (auto handle : handles) if (handle) glAttachShader(ID, handle);
            glLinkProgram(ID); GLint status = GL_FALSE; glGetProgramiv(ID, GL_LINK_STATUS, &status);
            valid_ = status == GL_TRUE;
            if (!valid_) {
                char message[4096]{}; glGetProgramInfoLog(ID, sizeof(message), nullptr, message);
                lastError_ = "Shader link failed: " + vertexShaderPath + " / " + fragmentShaderPath + "\n" + message;
            } else compiledSettings_ = definedSettings(combined);
        }
    } catch (const std::exception& error) { lastError_ = error.what(); valid_ = false; }
    for (auto handle : handles) if (handle) glDeleteShader(handle);
    if (!valid_) LOG_ERROR(lastError_);
}

bool Shader::reload() {
    Shader replacement;
    replacement.vertexShaderPath = vertexShaderPath; replacement.geometryShaderPath = geometryShaderPath;
    replacement.fragmentShaderPath = fragmentShaderPath; replacement.compile();
    if (!replacement.isValid()) { lastError_ = replacement.lastError(); return false; }
    *this = std::move(replacement); return true;
}

bool Shader::settingEnabled(const char* name) const {
    auto found = compiledSettings_.find(name);
    return found != compiledSettings_.end() && found->second;
}

bool Shader::applySetting(const char* name, bool value) {
    try {
        std::string source = updateSetting(settingsSource(), name, value);
        Shader replacement;
        replacement.vertexShaderPath = vertexShaderPath; replacement.geometryShaderPath = geometryShaderPath;
        replacement.fragmentShaderPath = fragmentShaderPath; replacement.compile(&source);
        if (!replacement.isValid()) { lastError_ = replacement.lastError(); return false; }
        if (!saveSettings(source, lastError_)) { LOG_ERROR(lastError_); return false; }
        *this = std::move(replacement); return true;
    } catch (const std::exception& error) { lastError_ = error.what(); LOG_ERROR(lastError_); return false; }
}

bool Shader::changeSettings(const char* name, bool value) {
    try {
        std::string source = updateSetting(settingsSource(), name, value), error;
        if (saveSettings(source, error)) return true;
        LOG_ERROR(error);
    } catch (const std::exception& error) { LOG_ERROR(error.what()); }
    return false;
}

void Shader::use() { glUseProgram(ID); }
void Shader::setVec3(const char* name, glm::vec3 value) {
    GLint location = uniformLocation(name); if (location >= 0) glProgramUniform3fv(ID, location, 1, glm::value_ptr(value));
}
void Shader::setVec2(const char* name, glm::vec2 value) {
    GLint location = uniformLocation(name); if (location >= 0) glProgramUniform2fv(ID, location, 1, glm::value_ptr(value));
}
void Shader::setMat4(const char* name, glm::mat4 value) {
    GLint location = uniformLocation(name); if (location >= 0) glProgramUniformMatrix4fv(ID, location, 1, GL_FALSE, glm::value_ptr(value));
}
void Shader::setFloat(const char* name, float value) {
    GLint location = uniformLocation(name); if (location >= 0) glProgramUniform1f(ID, location, value);
}
void Shader::setInt(const char* name, int value) {
    GLint location = uniformLocation(name); if (location >= 0) glProgramUniform1i(ID, location, value);
}
void Shader::setBool(const char* name, bool value) { setInt(name, value); }
GLint Shader::uniformLocation(const char* name) {
    auto found = uniformLocations.find(name); if (found != uniformLocations.end()) return found->second;
    GLint location = glGetUniformLocation(ID, name); uniformLocations.emplace(name, location); return location;
}
bool Shader::hasUniform(const char* name) { return uniformLocation(name) >= 0; }
