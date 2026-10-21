#include "assets/resourceManager.hpp"
#include "assets/shaderCatalog.hpp"
#include "assets/shaderLoader.hpp"
#include "rendering/opengl/shader.hpp"
#include "app/engine.hpp"
#include <GLFW/glfw3.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
namespace fs = std::filesystem;
void check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
std::string read(const fs::path& path) {
    std::ifstream input(path, std::ios::binary); check(input.is_open(), "Cannot read startup test fixture");
    return std::string(std::istreambuf_iterator<char>(input), {});
}
void write(const fs::path& path, const std::string& text) {
    std::ofstream output(path, std::ios::binary); check(output.is_open(), "Cannot write startup test fixture"); output << text;
}
int errors = 0;
void APIENTRY debugCallback(GLenum, GLenum type, GLuint, GLenum severity, GLsizei, const GLchar* text, const void*) {
    if (type == GL_DEBUG_TYPE_ERROR || type == GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR || severity == GL_DEBUG_SEVERITY_HIGH) {
        ++errors; std::cerr << "GL: " << text << '\n';
    }
}
struct FreshCheckout {
    fs::path original = fs::current_path();
    FreshCheckout() {
        fs::path root = original / "x64/StartupChecks" / std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        const auto settingsName = fs::path(ShaderCatalog::settingsPath).filename();
        const auto temporaryName = fs::path(std::string(ShaderCatalog::settingsPath) + ".tmp").filename();
        fs::create_directories(root / ShaderCatalog::directory);
        for (auto& entry : fs::directory_iterator(original / ShaderCatalog::directory)) {
            auto name = entry.path().filename();
            if (entry.is_regular_file() && name != settingsName && name != temporaryName)
                fs::copy_file(entry.path(), root / ShaderCatalog::directory / name);
        }
        fs::current_path(root);
    }
    ~FreshCheckout() { std::error_code ec; fs::current_path(original, ec); }
};

void checkConfiguration() {
    FreshCheckout checkout;
    check(!fs::exists(ShaderCatalog::settingsPath), "Fresh fixture unexpectedly contains local settings");
    ResourceManager resources; resources.init();
    auto material = resources.getShader(ShaderCatalog::forward.key), composite = resources.getShader(ShaderCatalog::composite.key);
    check(material->settingEnabled("PCF_SHADOW") && !material->settingEnabled("USE_ENVIRONMENT_MAPPING"), "Material defaults are incorrect");
    check(composite->settingEnabled("USE_HDR") && !composite->settingEnabled("USE_BLOOM"), "Composite defaults are incorrect");
    check(composite->hasUniform("exposure") && !composite->hasUniform("bloomBlurTexture"), "Compiled defaults differ from reported settings");
    check(!fs::exists(ShaderCatalog::settingsPath), "Loading defaults unexpectedly changed local preferences");
    struct Option { const char* name; ShaderPtr shader; bool first; };
    for (auto option : {Option{"PCF_SHADOW", material, false}, Option{"USE_HDR", composite, false},
        Option{"USE_BLOOM", composite, true}, Option{"USE_ENVIRONMENT_MAPPING", material, true}}) {
        for (bool value : {option.first, !option.first, option.first}) {
            check(option.shader->applySetting(option.name, value), "Render option could not be toggled");
            check(option.shader->settingEnabled(option.name) == value, "Checkbox state differs from compiled setting");
            std::string directive = std::string(value ? "\n#define " : "\n//#define ") + option.name;
            auto text = read(ShaderCatalog::settingsPath);
            check(text.find(directive) != std::string::npos, "Saved option lost its macro name");
        }
    }
    check(!composite->hasUniform("exposure") && composite->hasUniform("bloomBlurTexture"), "HDR/Bloom uniforms differ from selected settings");
    check(material->hasUniform("skybox"), "Environment switch was not compiled into material shader");
    {
        ResourceManager restarted; restarted.init();
        auto m = restarted.getShader(ShaderCatalog::forward.key), c = restarted.getShader(ShaderCatalog::composite.key);
        check(!m->settingEnabled("PCF_SHADOW") && m->settingEnabled("USE_ENVIRONMENT_MAPPING"), "Restart lost material settings");
        check(!c->settingEnabled("USE_HDR") && c->settingEnabled("USE_BLOOM"), "Restart lost composite settings");
    }
    std::cout << "PASS: fresh checkout defaults, all four option round trips, and persisted restart state\n";

    const auto previous = read(ShaderCatalog::settingsPath); const GLuint previousID = composite->ID;
    check(fs::create_directory(std::string(ShaderCatalog::settingsPath) + ".tmp"), "Save failure fixture was not created");
    std::cout << "EXPECTED: settings save failure must keep the current program and file\n" << std::flush;
    check(!composite->applySetting("USE_HDR", true), "Settings save failure was ignored");
    check(composite->ID == previousID && !composite->settingEnabled("USE_HDR"), "Failed save replaced the active program/state");
    check(read(ShaderCatalog::settingsPath) == previous, "Failed save truncated the previous settings");
    check(!composite->lastError().empty(), "Save failure did not report a reason");
    fs::remove(std::string(ShaderCatalog::settingsPath) + ".tmp"); // Empty directory created by this test.
    std::string invalid = previous + "\ninvalid shader configuration\n"; write(ShaderCatalog::settingsPath, invalid);
    std::cout << "EXPECTED: invalid shader configuration must not replace the current program\n" << std::flush;
    glDebugMessageCallback(nullptr, nullptr);
    bool applied = composite->applySetting("USE_BLOOM", false);
    glDebugMessageCallback(debugCallback, nullptr);
    check(!applied && composite->ID == previousID && composite->settingEnabled("USE_BLOOM"), "Failed compilation changed effective UI state");
    check(read(ShaderCatalog::settingsPath) == invalid, "Failed compilation wrote a new configuration");
    write(ShaderCatalog::settingsPath, previous);
    check(!composite->applySetting("UNSUPPORTED_SETTING", true), "Unsupported setting was accepted");
    check(read(ShaderCatalog::settingsPath) == previous, "Unsupported setting changed preferences");
    std::cout << "PASS: failed save/compile leaves effective settings and files unchanged\n";

    write(ShaderCatalog::settingsPath, "#define USE_HDR\n// Keep this custom note\n");
    check(Shader::changeSettings("USE_BLOOM", true), "Absent configuration option was not inserted");
    check(read(ShaderCatalog::settingsPath).find("Keep this custom note") != std::string::npos, "Updating settings discarded unrelated text");
    Shader missingOption(ShaderCatalog::fullscreenVertexPath, ShaderCatalog::composite.fragmentPath);
    check(missingOption.isValid() && missingOption.settingEnabled("USE_BLOOM"), "Inserted macro did not compile");
    write(ShaderCatalog::settingsPath, previous);

    ShaderLoader loader; bool rejected = false;
    try { loader.registerShader("missing", "data/shader/not-present.vert", ShaderCatalog::forward.fragmentPath); }
    catch (const std::runtime_error& error) { rejected = std::string(error.what()).find("not-present.vert") != std::string::npos; }
    check(rejected && !loader.get("missing"), "Missing shader file did not fail initialization with its path");
    write("data/shader/cycle.glsl", "#include \"data/shader/cycle.glsl\"\n");
    write("data/shader/cycle.frag", "#version 450 core\n#include \"data/shader/cycle.glsl\"\n");
    Shader cycle(ShaderCatalog::fullscreenVertexPath, "data/shader/cycle.frag");
    check(!cycle.isValid() && cycle.lastError().find("cycle") != std::string::npos, "Recursive include did not stop safely");
    write("data/shader/commented.frag", "#version 450 core\n// #include \"not-present.glsl\"\n/*\n#include \"also-missing.glsl\"\n*/\nout vec4 color;void main(){color=vec4(1);}\n");
    Shader commented(ShaderCatalog::fullscreenVertexPath, "data/shader/commented.frag");
    check(commented.isValid(), "Commented-out include was loaded");
    std::cout << "PASS: missing shader diagnostics, bounded include expansion, and commented includes\n";
    resources.shutdown();
}

void checkFailedEngineStartup() {
    FreshCheckout checkout;
    check(fs::remove(ShaderCatalog::forward.vertexPath), "Missing shader fixture was not prepared");
    check(glfwInit() != 0, "GLFW init failed"); glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    bool rejected = false;
    try {
        Engine engine;
        engine.init();
    } catch (const std::runtime_error& error) {
        rejected = std::string(error.what()).find(ShaderCatalog::forward.vertexPath) != std::string::npos;
    }
    check(rejected, "Engine did not report its missing startup shader");
    check(glfwGetCurrentContext() == nullptr, "Failed Engine initialization retained its GL context");
    std::cout << "PASS: failed Engine startup reports missing shader and closes the context\n";
}
}

void runRenderStartupChecks() {
    check(glfwInit() != 0, "GLFW init failed"); glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4); glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
    auto window = glfwCreateWindow(64, 64, "Startup checks", nullptr, nullptr); check(window != nullptr, "Hidden GL context failed");
    glfwMakeContextCurrent(window); check(gladLoadGLLoader((GLADloadproc)glfwGetProcAddress) != 0, "GL loader failed");
    glEnable(GL_DEBUG_OUTPUT); glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS); glDebugMessageCallback(debugCallback, nullptr);
    checkConfiguration();
    check(glGetError() == GL_NO_ERROR && errors == 0, "GL errors during startup configuration checks");
    glfwDestroyWindow(window); glfwTerminate();
    checkFailedEngineStartup();
}
