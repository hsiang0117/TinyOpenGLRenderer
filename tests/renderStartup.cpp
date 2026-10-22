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
#include <vector>

namespace togr::tests {

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
        fs::path root = original / "x64/StartupChecks" / fs::u8path(u8"\u8def\u5f84\u68c0\u67e5") /
            std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        const auto settingsName = fs::u8path(ShaderCatalog::settingsPath).filename();
        auto temporaryName = settingsName; temporaryName += ".tmp";
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
    auto temporarySettings = fs::u8path(ShaderCatalog::settingsPath); temporarySettings += ".tmp";
    check(fs::create_directory(temporarySettings), "Save failure fixture was not created");
    std::cout << "EXPECTED: settings save failure must keep the current program and file\n" << std::flush;
    check(!composite->applySetting("USE_HDR", true), "Settings save failure was ignored");
    check(composite->ID == previousID && !composite->settingEnabled("USE_HDR"), "Failed save replaced the active program/state");
    check(read(ShaderCatalog::settingsPath) == previous, "Failed save truncated the previous settings");
    check(!composite->lastError().empty(), "Save failure did not report a reason");
    fs::remove(temporarySettings); // Empty directory created by this test.
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
    Shader missingOption(ShaderCatalog::fullscreenVertexPath, ShaderCatalog::composite.fragmentPath);
    check(missingOption.isValid() && !missingOption.settingEnabled("USE_BLOOM"), "Missing option fixture unexpectedly enables Bloom");
    check(missingOption.applySetting("USE_BLOOM", true), "Absent configuration option was not inserted");
    check(missingOption.settingEnabled("USE_BLOOM") && missingOption.hasUniform("bloomBlurTexture"),
        "Inserted macro did not update the active shader");
    const auto inserted = read(ShaderCatalog::settingsPath);
    check(inserted.find("#define USE_BLOOM") != std::string::npos, "Inserted configuration option was not saved");
    check(inserted.find("Keep this custom note") != std::string::npos, "Updating settings discarded unrelated text");
    Shader persistedOption(ShaderCatalog::fullscreenVertexPath, ShaderCatalog::composite.fragmentPath);
    check(persistedOption.isValid() && persistedOption.settingEnabled("USE_BLOOM"), "Inserted macro did not survive a new shader instance");
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

void checkShaderPaths() {
    FreshCheckout checkout;
    const auto directory = fs::u8path(u8"data/shader/\u6d4b\u8bd5\u7740\u8272\u5668");
    const auto leaf = directory / "nested" / fs::u8path(u8"\u989c\u8272.glsl");
    fs::create_directories(leaf.parent_path());
    write(directory / "main.vert", read(ShaderCatalog::fullscreenVertexPath));
    write(leaf, "uniform vec4 nestedProbe;\nvec4 fixtureColor(){return nestedProbe;}\n");
    write(directory / "nested/common.glsl", u8"#include \"./\u989c\u8272.glsl\"\n");
    fs::create_directories("nested");
    write("nested/common.glsl", "This working-directory file must not shadow a sibling include.\n");

    const auto vertex = fs::absolute(directory / "main.vert").u8string();
    const auto nestedFragment = (directory / "nested.frag").u8string();
    const std::string fragmentEnd = "out vec4 color;void main(){color=fixtureColor();}\n";
    write(fs::u8path(nestedFragment), "#version 450 core\n#include \"nested/common.glsl\"\n" + fragmentEnd);
    Shader nested(vertex.c_str(), nestedFragment.c_str());
    check(nested.isValid() && nested.hasUniform("nestedProbe"), "Nested UTF-8 sibling includes were not resolved from their containing files");
    check(nested.reload(), "UTF-8 shader paths could not be reloaded");

    auto absoluteInclude = fs::absolute(leaf).generic_u8string();
    const auto separator = absoluteInclude.find("/nested/");
    check(separator != std::string::npos, "Absolute include fixture has no separator");
    absoluteInclude.insert(separator, "/"); // A quoted // must not become a comment.
    const auto absoluteFragment = fs::absolute(directory / "absolute.frag").u8string();
    write(fs::u8path(absoluteFragment), "#version 450 core\n#include \"" + absoluteInclude + "\"\n" + fragmentEnd);
    Shader absolute(vertex.c_str(), absoluteFragment.c_str());
    check(absolute.isValid() && absolute.hasUniform("nestedProbe"), "Absolute includes or quoted repeated separators were misread");

    write("legacy-helper.glsl", read(leaf));
    const auto fallbackFragment = (directory / "fallback.frag").u8string();
    write(fs::u8path(fallbackFragment), "#version 450 core\n#include \"legacy-helper.glsl\"\n" + fragmentEnd);
    Shader fallback(vertex.c_str(), fallbackFragment.c_str());
    check(fallback.isValid() && fallback.hasUniform("nestedProbe"), "Working-directory include fallback no longer works");
    std::cout << "PASS: UTF-8 shader paths, nested sibling precedence, absolute includes, quoted separators, and root fallback\n";

    // Explicit data/... includes keep their root meaning even when a containing
    // directory has another data/ tree with the same filename.
    fs::create_directories(directory / "data/shader");
    write(directory / "data/shader/settings.glsl", "This sibling must not shadow the project settings.\n");
    const auto settings = fs::u8path(ShaderCatalog::settingsPath);
    std::vector<std::string> aliases = {
        "data/shader/settings.glsl", "./data/shader/settings.glsl", "../settings.glsl",
        fs::absolute(settings).generic_u8string()
    };
#ifdef _WIN32
    aliases.push_back("data\\shader\\settings.glsl");
    aliases.push_back("DATA/SHADER/SETTINGS.GLSL");
#endif
    const auto aliasFragment = (directory / "settings-alias.frag").u8string();
    for (const auto& alias : aliases) {
        fs::remove(settings);
        write(fs::u8path(aliasFragment), "#version 450 core\n#include \"" + alias + "\"\n"
            "out vec4 color;\n#ifdef USE_HDR\nuniform float pathSettingProbe;\n#endif\n"
            "void main(){\n#ifdef USE_HDR\ncolor=vec4(pathSettingProbe);\n#else\ncolor=vec4(0.5);\n#endif\n}\n");
        Shader configured(vertex.c_str(), aliasFragment.c_str());
        check(configured.isValid() && configured.settingEnabled("USE_HDR") && configured.hasUniform("pathSettingProbe"),
            "Equivalent settings include did not fall back to tracked defaults");
        check(!fs::exists(settings), "Reading an aliased settings include created local preferences");
        check(configured.applySetting("USE_HDR", false), "Aliased settings include could not be changed");
        check(!configured.settingEnabled("USE_HDR") && !configured.hasUniform("pathSettingProbe"),
            "Aliased settings include compiled old settings instead of the replacement");
        check(read(settings).find("//#define USE_HDR") != std::string::npos, "Aliased settings update was not saved");
        check(configured.applySetting("USE_HDR", true) && configured.hasUniform("pathSettingProbe"),
            "Aliased settings include could not restore a setting");
        check(configured.reload() && configured.settingEnabled("USE_HDR"), "Aliased settings reload lost the saved configuration");
    }
    std::cout << "PASS: normalized root, relative, absolute, and native-separator settings aliases compile and save effective settings\n";

    write(directory / "path-cycle.glsl", "#include \"nested/../path-cycle.glsl\"\n");
    const auto cycleFragment = (directory / "path-cycle.frag").u8string();
    write(fs::u8path(cycleFragment), "#version 450 core\n#include \"path-cycle.glsl\"\n");
    Shader cycle(vertex.c_str(), cycleFragment.c_str());
    check(!cycle.isValid() && cycle.lastError().find("cycle") != std::string::npos, "Normalized include alias bypassed cycle detection");
    write(directory / "empty.glsl", "// An empty include for the expansion bound.\n");
    const auto boundedFragment = (directory / "bounded.frag").u8string();
    std::string boundedSource = "#version 450 core\n";
    for (int i = 0; i < 65; ++i) boundedSource += "#include \"empty.glsl\"\n";
    write(fs::u8path(boundedFragment), boundedSource + "out vec4 color;void main(){color=vec4(1);}\n");
    Shader bounded(vertex.c_str(), boundedFragment.c_str());
    check(!bounded.isValid() && bounded.lastError().find("expansion limit") != std::string::npos, "Acyclic include expansion was not bounded");
    std::cout << "PASS: normalized include cycles and excessive acyclic expansion fail safely\n";
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

void checkMissingStartupSkybox() {
    FreshCheckout checkout;
    check(glfwInit() != 0, "GLFW init failed"); glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    bool rejected = false;
    try {
        Engine engine;
        engine.init();
    } catch (const std::runtime_error& error) {
        const std::string message = error.what();
        rejected = message.find("data/textures/skybox") != std::string::npos && message.find("right.jpg") != std::string::npos;
    }
    check(rejected, "Engine did not report the missing required skybox face and path");
    check(glfwGetCurrentContext() == nullptr, "Missing startup skybox retained the Engine's GL context");
    // Restore only this fixture's required images, then prove a subsequent full
    // initialization in the same process still succeeds after partial startup.
    const fs::path skybox = "data/textures/skybox"; fs::create_directories(skybox);
    for (const char* face : {"right.jpg", "left.jpg", "top.jpg", "bottom.jpg", "front.jpg", "back.jpg"})
        fs::copy_file(checkout.original / skybox / face, skybox / face);
    check(glfwInit() != 0, "GLFW could not restart after a failed Engine initialization");
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    {
        Engine recovered; recovered.init();
        check(glfwGetCurrentContext() != nullptr, "Recovered Engine did not initialize a GL context");
        check(glGetError() == GL_NO_ERROR, "Recovered Engine initialization produced a GL error");
    }
    check(glfwGetCurrentContext() == nullptr, "Recovered Engine did not release its GL context");
    std::cout << "PASS: required skybox startup failure reports its face/path, closes the context, and permits a successful restart\n";
}

PFNGLCHECKNAMEDFRAMEBUFFERSTATUSPROC actualFramebufferStatus = nullptr;
int injectedFramebufferFailures = 0;
GLenum APIENTRY failOneFramebufferStatus(GLuint framebuffer, GLenum target) {
    if (injectedFramebufferFailures == 0) {
        ++injectedFramebufferFailures;
        return GL_FRAMEBUFFER_UNSUPPORTED;
    }
    return actualFramebufferStatus(framebuffer, target);
}

void checkFailedFramebufferResize() {
    ResourceManager resources;
    struct ScopedRenderer {
        RenderSystem renderer;
        ~ScopedRenderer() { renderer.shutdown(); }
    } scoped;
    auto& renderer = scoped.renderer;
    renderer.init(); renderer.setResourceManager(&resources);
    // init reloads GLAD; install the observer afterwards and restore it before
    // cleanup. No invalid GL operation is needed to exercise failure propagation.
    glDebugMessageCallback(debugCallback, nullptr);
    const GLuint sceneTexture = renderer.getSceneColorTextureId();
    check(sceneTexture != 0 && glIsTexture(sceneTexture), "Renderer resize fixture has no scene texture");
    actualFramebufferStatus = glad_glCheckNamedFramebufferStatus;
    injectedFramebufferFailures = 0; glad_glCheckNamedFramebufferStatus = failOneFramebufferStatus;
    renderer.setViewportSize(73, 59);
    bool rejected = false;
    try { renderer.update(0.0); }
    catch (const std::runtime_error& error) {
        const std::string message = error.what();
        rejected = message.find("framebuffer") != std::string::npos && message.find("0x8CDD") != std::string::npos;
    }
    glad_glCheckNamedFramebufferStatus = actualFramebufferStatus;
    check(rejected && injectedFramebufferFailures == 1, "Incomplete framebuffer resize was silently accepted");
    renderer.shutdown();
    check(renderer.getSceneColorTextureId() == 0 && !glIsTexture(sceneTexture),
        "Renderer framebuffer failure could not release its GL targets");
    check(glGetError() == GL_NO_ERROR, "Framebuffer failure cleanup produced a GL error");
    std::cout << "PASS: framebuffer resize failure propagates its status and targets remain safely releasable\n";
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
    checkShaderPaths();
    checkFailedFramebufferResize();
    check(glGetError() == GL_NO_ERROR && errors == 0, "GL errors during startup configuration checks");
    glfwDestroyWindow(window); glfwTerminate();
    checkFailedEngineStartup();
    checkMissingStartupSkybox();
}

} // namespace togr::tests
