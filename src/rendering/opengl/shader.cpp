#include "shader.hpp"
#include "assets/shaderCatalog.hpp"
#include "core/logger.hpp"
#include <glm/gtc/type_ptr.hpp>
#include <array>
#include <algorithm>
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
namespace fs = std::filesystem;
using ShaderCatalog::settingsPath;
using ShaderCatalog::defaultsPath;

fs::path normalizedPath(const fs::path& path) {
    return fs::absolute(path).lexically_normal();
}

bool samePath(const fs::path& left, const fs::path& right) {
#ifdef _WIN32
    return CompareStringOrdinal(left.c_str(), -1, right.c_str(), -1, TRUE) == CSTR_EQUAL;
#else
    return left == right;
#endif
}

std::string readShaderFile(const fs::path& path) {
    std::ifstream input(path);
    if (!input) throw std::runtime_error("Cannot read shader file: " + path.generic_u8string());
    std::ostringstream contents; contents << input.rdbuf();
    if (input.bad()) throw std::runtime_error("Failed reading shader file: " + path.generic_u8string());
    return contents.str();
}

std::string withoutComments(std::string source) {
    enum class State { code, quoted, lineComment, blockComment };
    State state = State::code;
    bool escaped = false;
    for (std::size_t i = 0; i < source.size(); ++i) {
        const char current = source[i];
        const char next = i + 1 < source.size() ? source[i + 1] : '\0';
        if (state == State::quoted) {
            if (current == '"' && !escaped) state = State::code;
            escaped = current == '\\' && !escaped;
        } else if (state == State::lineComment) {
            if (current == '\n' || current == '\r') state = State::code;
            else source[i] = ' ';
        } else if (state == State::blockComment) {
            if (current == '*' && next == '/') {
                source[i] = source[i + 1] = ' '; ++i; state = State::code;
            } else if (current != '\n' && current != '\r') source[i] = ' ';
        } else if (current == '"') {
            state = State::quoted; escaped = false;
        } else if (current == '/' && (next == '/' || next == '*')) {
            state = next == '/' ? State::lineComment : State::blockComment;
            source[i] = source[i + 1] = ' '; ++i;
        }
    }
    return source;
}

std::string settingsSource() {
    const auto settings = normalizedPath(fs::u8path(settingsPath));
    return readShaderFile(fs::exists(settings) ? settings : normalizedPath(fs::u8path(defaultsPath)));
}

fs::path resolveInclude(const fs::path& included, const fs::path& source) {
    if (included.has_root_path()) return normalizedPath(included);

    // Existing data/... and tests/... includes are explicitly relative to the
    // working directory. A sibling data/ directory must not shadow those paths.
    const auto relative = included.lexically_normal();
    const auto first = relative.begin();
    if (first != relative.end() && (samePath(*first, "data") || samePath(*first, "tests")))
        return normalizedPath(relative);

    // Other includes belong to the containing file; only a missing sibling can
    // fall back to the original working-directory lookup convention.
    const auto sibling = normalizedPath(source.parent_path() / included);
    if (samePath(sibling, normalizedPath(fs::u8path(settingsPath))) || fs::exists(sibling)) return sibling;
    const auto rootRelative = normalizedPath(included);
    if (samePath(rootRelative, normalizedPath(fs::u8path(settingsPath))) || fs::exists(rootRelative)) return rootRelative;
    return sibling;
}

std::string preprocessFile(const fs::path& path, const std::string* settingsOverride,
    std::vector<fs::path>& includeStack, unsigned& expansions) {
    const auto sourcePath = normalizedPath(path);
    if (std::find_if(includeStack.begin(), includeStack.end(),
        [&](const fs::path& included) { return samePath(included, sourcePath); }) != includeStack.end())
        throw std::runtime_error("Shader include cycle: " + sourcePath.generic_u8string());
    includeStack.push_back(sourcePath);
    struct PopInclude { std::vector<fs::path>& stack; ~PopInclude() { stack.pop_back(); } } pop{includeStack};

    const bool isSettings = samePath(sourcePath, normalizedPath(fs::u8path(settingsPath)));
    auto content = withoutComments(isSettings ? (settingsOverride ? *settingsOverride : settingsSource()) : readShaderFile(sourcePath));
    static const std::regex includePattern(R"###(^[ \t]*#[ \t]*include[ \t]*"([^"]+)")###", std::regex::ECMAScript | std::regex::multiline);
    std::smatch match;
    std::string result;
    while (std::regex_search(content, match, includePattern)) {
        if (++expansions > 64)
            throw std::runtime_error("Shader include expansion limit exceeded: " + sourcePath.generic_u8string());
        result.append(content, 0, match.position());
        result += preprocessFile(resolveInclude(fs::u8path(match[1].str()), sourcePath), settingsOverride, includeStack, expansions);
        content.erase(0, match.position() + match.length());
    }
    return result + content;
}

std::string preprocessShader(const fs::path& path, const std::string* settingsOverride) {
    std::vector<fs::path> includeStack;
    unsigned expansions = 0;
    return preprocessFile(path, settingsOverride, includeStack, expansions);
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
    const auto destination = normalizedPath(fs::u8path(settingsPath));
    auto temporary = destination; temporary += ".tmp";
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        bool created = output.is_open();
        if (created) { output << source; output.close(); }
        if (!output) {
            error = "Cannot save render settings: " + temporary.generic_u8string();
            if (created) { std::error_code ec; fs::remove(temporary, ec); }
            return false;
        }
    }
    std::error_code ec;
#ifdef _WIN32
    // std::filesystem::rename does not replace an existing file on Windows.
    // MoveFileExW performs the replacement without a delete/truncate window.
    if (!MoveFileExW(temporary.c_str(), destination.c_str(),
        MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        error = "Cannot replace render settings (Windows error " + std::to_string(GetLastError()) + "): " + destination.generic_u8string();
        fs::remove(temporary, ec); return false;
    }
#else
    fs::rename(temporary, destination, ec);
    if (ec) { error = "Cannot replace render settings: " + ec.message(); fs::remove(temporary, ec); return false; }
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

Shader::Shader(const char* vertex, const char* fragment)
    : vertexShaderPath(fs::u8path(vertex)), fragmentShaderPath(fs::u8path(fragment)) { compile(); }

Shader::Shader(const char* vertex, const char* geometry, const char* fragment)
    : vertexShaderPath(fs::u8path(vertex)), fragmentShaderPath(fs::u8path(fragment)), geometryShaderPath(fs::u8path(geometry)) { compile(); }

void Shader::compile(const std::string* settingsOverride) {
    std::array<GLuint, 3> handles{};
    try {
        struct Stage { GLenum type; fs::path path; std::string source; };
        std::vector<Stage> stages = {{GL_VERTEX_SHADER, vertexShaderPath, {}}};
        if (!geometryShaderPath.empty()) stages.push_back({GL_GEOMETRY_SHADER, geometryShaderPath, {}});
        stages.push_back({GL_FRAGMENT_SHADER, fragmentShaderPath, {}});
        std::string combined;
        for (auto& stage : stages) {
            stage.source = preprocessShader(stage.path, settingsOverride);
            combined += stage.source + '\n';
        }
        bool compiled = true;
        for (std::size_t i = 0; i < stages.size(); ++i) {
            handles[i] = glCreateShader(stages[i].type); const char* source = stages[i].source.c_str();
            glShaderSource(handles[i], 1, &source, nullptr); glCompileShader(handles[i]);
            GLint status = GL_FALSE; glGetShaderiv(handles[i], GL_COMPILE_STATUS, &status);
            if (!status) {
                char message[4096]{}; glGetShaderInfoLog(handles[i], sizeof(message), nullptr, message);
                lastError_ += "Shader compilation failed: " + stages[i].path.generic_u8string() + "\n" + message + '\n'; compiled = false;
            }
        }
        if (compiled) {
            ID = glCreateProgram(); for (auto handle : handles) if (handle) glAttachShader(ID, handle);
            glLinkProgram(ID); GLint status = GL_FALSE; glGetProgramiv(ID, GL_LINK_STATUS, &status);
            valid_ = status == GL_TRUE;
            if (!valid_) {
                char message[4096]{}; glGetProgramInfoLog(ID, sizeof(message), nullptr, message);
                lastError_ = "Shader link failed: " + vertexShaderPath.generic_u8string() + " / " + fragmentShaderPath.generic_u8string() + "\n" + message;
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
