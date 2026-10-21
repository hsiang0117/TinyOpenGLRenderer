#include "rendering/passes/renderPasses.hpp"
#include "rendering/renderUtils.hpp"
#include "assets/resourceManager.hpp"
#include "assets/shaderCatalog.hpp"
#include "scene/component.hpp"
#include "rendering/opengl/shader.hpp"
#include "rendering/renderSystem.hpp"
#include "rendering/geometry/meshGenerator.hpp"
#include "scene/gameObjectFactory.hpp"
#include <GLFW/glfw3.h>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <set>

namespace {
void check(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
int glErrors = 0;
void APIENTRY debugCallback(GLenum, GLenum type, GLuint, GLenum severity, GLsizei, const GLchar* message, const void*) {
    if (type == GL_DEBUG_TYPE_ERROR || type == GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR || severity == GL_DEBUG_SEVERITY_HIGH) {
        ++glErrors; std::cerr << "GL: " << message << '\n';
    }
}
PFNGLGETUNIFORMLOCATIONPROC originalLookup;
PFNGLDRAWARRAYSPROC originalDraw;
int queries = 0, boneQueries = 0, quadDraws = 0;
GLint APIENTRY countLookup(GLuint program, const GLchar* name) {
    ++queries;
    if (std::strstr(name, "finalBoneMatrices")) ++boneQueries;
    return originalLookup(program, name);
}
void APIENTRY countDraw(GLenum mode, GLint first, GLsizei count) {
    if (mode == GL_TRIANGLE_STRIP && count == 4) ++quadDraws;
    originalDraw(mode, first, count);
}
struct Calls {
    Calls() {
        queries = boneQueries = quadDraws = 0;
        originalLookup = glad_glGetUniformLocation; originalDraw = glad_glDrawArrays;
        glad_glGetUniformLocation = countLookup; glad_glDrawArrays = countDraw;
    }
    ~Calls() { glad_glGetUniformLocation = originalLookup; glad_glDrawArrays = originalDraw; }
};
void write(const char* path, const std::string& source) {
    std::ofstream output(path); check(static_cast<bool>(output), "Cannot write shader fixture"); output << source;
}
void linked(const Shader& shader) {
    GLint status = 0; glGetProgramiv(shader.ID, GL_LINK_STATUS, &status);
    check(status == GL_TRUE, "Performance fixture failed to link");
}
const char* vertexPath = "x64/RenderChecks/performance.vert";
const char* fragmentPath = "x64/RenderChecks/performance.frag";
void writeFragment(int gainLocation) {
    write(fragmentPath,
        "#version 450 core\nlayout(location=0) out vec4 color;\n"
        "layout(location=0) uniform vec3 tint;\n"
        "layout(location=" + std::to_string(gainLocation) + ") uniform float gain;\n"
        "layout(location=5) uniform vec2 offset;\n"
        "layout(location=6) uniform int selector;\n"
        "layout(location=7) uniform bool flag;\n"
        "void main(){color=vec4(tint*gain+vec3(offset,0)+vec3(float(selector)*0.1)+(flag?vec3(0.05):vec3(0)),1);}\n");
}
void checkUniformCache() {
    write(vertexPath, "#version 450 core\nlayout(location=0) in vec3 position;\n"
        "layout(location=10) uniform mat4 model;\nvoid main(){gl_Position=model*vec4(position,1);}\n");
    writeFragment(4);
    Shader target(vertexPath, fragmentPath), current(vertexPath, fragmentPath); linked(target); linked(current);
    current.use(); current.setFloat("gain", 9);
    {
        Calls calls;
        for (int i = 0; i < 100; ++i) {
            target.setVec3("tint", glm::vec3(0.25f)); target.setVec2("offset", glm::vec2(0));
            target.setMat4("model", glm::mat4(1)); target.setFloat("gain", 2);
            target.setInt("selector", 0); target.setBool("flag", false);
            target.setFloat("missingUniform", 1); check(!target.hasUniform("missingUniform"), "Missing uniform appears active");
        }
        check(queries == 7, "Uniform locations were repeatedly queried instead of cached");
        float targetGain = 0, currentGain = 0;
        glGetUniformfv(target.ID, originalLookup(target.ID, "gain"), &targetGain);
        glGetUniformfv(current.ID, originalLookup(current.ID, "gain"), &currentGain);
        check(targetGain == 2 && currentGain == 9, "Uniform setter wrote to the currently bound program instead of its target");
        GLint bound = 0; glGetIntegerv(GL_CURRENT_PROGRAM, &bound);
        check(bound == static_cast<GLint>(current.ID), "Uniform setter unnecessarily changed the bound program");
        Shader moved(std::move(target)); moved.setFloat("gain", 3);
        check(queries == 7 && target.ID == 0, "Moving shader lost its uniform cache or ownership");
        target = std::move(moved); target.setFloat("gain", 4);
        check(queries == 7 && moved.ID == 0, "Move assignment retained the wrong cache");
        writeFragment(8); target.reload(); linked(target); target.setFloat("gain", 5);
        check(queries == 8, "Recompile must invalidate old uniform locations");
        glGetUniformfv(target.ID, originalLookup(target.ID, "gain"), &targetGain);
        check(targetGain == 5, "Cached uniform location survived a relink incorrectly");
        GLuint workingProgram = target.ID;
        std::cout << "EXPECTED: deliberately invalid shader; reload must preserve the working program\n" << std::flush;
        write(fragmentPath, "#version 450 core\ninvalid shader fixture\n");
        // Compilation failure is intentional; suppress expected compiler diagnostics from GL's error counter.
        glDebugMessageCallback(nullptr, nullptr);
        target.reload();
        glDebugMessageCallback(debugCallback, nullptr);
        target.setFloat("gain", 6);
        check(target.ID == workingProgram && queries == 8, "Failed reload discarded the working program or cache");
        glGetUniformfv(target.ID, originalLookup(target.ID, "gain"), &targetGain);
        check(targetGain == 6, "Failed reload left shader unusable");
        writeFragment(4);
    }
    glUseProgram(0);
    std::cout << "PASS: 700 setter calls use 7 lookups; target updates, moves, and reload cache invalidation\n";
}
void checkStaticSubmission() {
    auto shader = std::make_shared<Shader>(vertexPath, fragmentPath); linked(*shader); shader->use();
    shader->setVec3("tint", glm::vec3(0.25f)); shader->setFloat("gain", 1);
    auto object = std::make_shared<GameObject>("Static mesh"); object->addComponent<Transform>();
    object->addComponent<StaticMeshComponent>()->setMesh(MeshGenerator::generateCube());
    object->addComponent<DynamicMaterialComponent>();
    Texture2D color(8, 8, GL_CLAMP_TO_EDGE, GL_NEAREST, GL_RGBA32F, GL_RGBA, GL_FLOAT);
    FrameBuffer fbo; fbo.init(); fbo.bind(); fbo.attachTexture2D(color, GL_COLOR_ATTACHMENT0); glViewport(0, 0, 8, 8);
    Calls calls;
    for (int i = 0; i < 100; ++i) RenderHelpers::drawRenderable(object, shader);
    check(boneQueries == 0, "Static mesh queried nonexistent bone uniforms");
    check(queries == 3, "Static mesh resubmission repeated model/material uniform lookups");
    glm::vec4 pixel; glReadPixels(4, 4, 1, 1, GL_RGBA, GL_FLOAT, &pixel);
    check(std::abs(pixel.r - 0.25f) < 0.001f, "Static mesh rendering changed after removing bone uploads");
    fbo.unbind(); glUseProgram(0);
    std::cout << "PASS: 100 static mesh submissions use 3 lookups and 0 bone uniform queries\n";
}
void checkBloom(ResourceManager& resources) {
    const char* compositePath = "x64/RenderChecks/performanceComposite.frag";
    std::ifstream source(ShaderCatalog::composite.fragmentPath); std::string original((std::istreambuf_iterator<char>(source)), {});
    auto settings = original.find("#include"); check(settings != std::string::npos, "Composite settings include missing");
    auto writeComposite = [&](bool enabled) {
        auto content = original; content.replace(settings, content.find('\n', settings) - settings, enabled ? "#define USE_BLOOM\n#define USE_HDR" : "#define USE_HDR");
        write(compositePath, content);
    };
    writeComposite(false);
    auto composite = std::make_shared<Shader>(ShaderCatalog::fullscreenVertexPath, compositePath); linked(*composite);
    Texture2D bright(16, 16, GL_CLAMP_TO_EDGE, GL_NEAREST, GL_RGBA32F, GL_RGBA, GL_FLOAT);
    Texture2D ping0(16, 16, GL_CLAMP_TO_EDGE, GL_NEAREST, GL_RGBA32F, GL_RGBA, GL_FLOAT);
    Texture2D ping1(16, 16, GL_CLAMP_TO_EDGE, GL_NEAREST, GL_RGBA32F, GL_RGBA, GL_FLOAT);
    std::vector<glm::vec4> values(16 * 16, glm::vec4(2, 1, 0.5f, 1)); bright.subImage2D(0, 0, 16, 16, values.data());
    FrameBuffer fbo0, fbo1; fbo0.init(); fbo1.init(); fbo0.bind(); fbo0.attachTexture2D(ping0, GL_COLOR_ATTACHMENT0);
    fbo1.bind(); fbo1.attachTexture2D(ping1, GL_COLOR_ATTACHMENT0); fbo1.unbind();
    RenderContext ctx; ctx.compositeShader = composite; ctx.gaussianBlurShader = resources.getShader(ShaderCatalog::gaussianBlur.key);
    ctx.bloomBlurFbos[0] = &fbo0; ctx.bloomBlurFbos[1] = &fbo1; ctx.bloomBlurTextures[0] = &ping0; ctx.bloomBlurTextures[1] = &ping1;
    ctx.bloomBrightTexture = &bright; glViewport(0, 0, 16, 16);
    BloomPass bloom;
    Calls calls;
    bloom.execute(ctx); bloom.execute(ctx);
    check(quadDraws == 0 && queries == 1, "Disabled Bloom must skip blur draws and reuse its feature lookup");
    writeComposite(true); composite->reload(); linked(*composite); composite->setFloat("exposure", 1.7f);
    bloom.execute(ctx);
    check(quadDraws == 10, "Enabling Bloom after reload did not execute its 10 blur passes");
    fbo1.bind(); glm::vec4 pixel; glReadPixels(8, 8, 1, 1, GL_RGBA, GL_FLOAT, &pixel); fbo1.unbind();
    check(std::abs(pixel.r - 2) < 0.01f && std::abs(pixel.g - 1) < 0.01f, "Bloom blur output changed");
    int lookups = queries;
    bloom.execute(ctx); check(quadDraws == 20 && queries == lookups, "Steady-state Bloom queried uniforms again");
    writeComposite(false); composite->reload(); linked(*composite); bloom.execute(ctx);
    check(quadDraws == 20, "Disabling Bloom after reload still executed blur draws");
    glUseProgram(0);
    std::cout << "PASS: Bloom off=0 blur draws, on=10; hot toggle and steady-state uniform cache\n";
}
void checkGeometryReload(ResourceManager& resources) {
    auto shader = resources.getShader(ShaderCatalog::pointShadow.key); shader->setInt("lightIndex", 1);
    shader->reload(); linked(*shader);
    check(shader->hasUniform("lightMatrices[5]"), "Reload dropped the geometry shader stage");
    shader->setInt("lightIndex", 2);
    GLint index = 0; glGetUniformiv(shader->ID, glGetUniformLocation(shader->ID, "lightIndex"), &index);
    check(index == 2, "Geometry program cannot update uniforms after reload");
    std::cout << "PASS: geometry shader stage survives reload\n";
}

bool forceTimerUnavailable = false;
int timerWrites = 0, timerReads = 0, timerPolls = 0, timerFinishes = 0;
std::vector<GLuint> timerObjects;
std::set<GLuint> pendingTimers;
PFNGLGENQUERIESPROC originalGenTimers;
PFNGLQUERYCOUNTERPROC originalTimestamp;
PFNGLGETQUERYOBJECTIVPROC originalTimerAvailability;
PFNGLGETQUERYOBJECTUI64VPROC originalTimerResult;
PFNGLFINISHPROC originalFinish;
void APIENTRY traceGenTimers(GLsizei count, GLuint* ids) {
    originalGenTimers(count, ids); timerObjects.insert(timerObjects.end(), ids, ids + count);
}
void APIENTRY traceTimestamp(GLuint id, GLenum target) {
    check(!forceTimerUnavailable || pendingTimers.count(id) == 0, "Profiler overwrote an unfinished GPU query");
    pendingTimers.insert(id); ++timerWrites; originalTimestamp(id, target);
}
void APIENTRY traceTimerAvailability(GLuint id, GLenum pname, GLint* value) {
    check(pname == GL_QUERY_RESULT_AVAILABLE, "Profiler used a potentially blocking result read");
    ++timerPolls;
    if (forceTimerUnavailable) *value = GL_FALSE;
    else originalTimerAvailability(id, pname, value);
}
void APIENTRY traceTimerResult(GLuint id, GLenum pname, GLuint64* value) {
    GLint ready = GL_FALSE; originalTimerAvailability(id, GL_QUERY_RESULT_AVAILABLE, &ready);
    check(!forceTimerUnavailable && ready, "Profiler read a GPU result before it was available");
    ++timerReads; pendingTimers.erase(id); originalTimerResult(id, pname, value);
}
void APIENTRY traceFinish() { ++timerFinishes; originalFinish(); }
struct TimerProbe {
    TimerProbe() {
        timerWrites = timerReads = timerPolls = timerFinishes = 0;
        timerObjects.clear(); pendingTimers.clear(); forceTimerUnavailable = false;
        originalGenTimers = glad_glGenQueries; originalTimestamp = glad_glQueryCounter;
        originalTimerAvailability = glad_glGetQueryObjectiv; originalTimerResult = glad_glGetQueryObjectui64v;
        originalFinish = glad_glFinish;
        glad_glGenQueries = traceGenTimers; glad_glQueryCounter = traceTimestamp;
        glad_glGetQueryObjectiv = traceTimerAvailability; glad_glGetQueryObjectui64v = traceTimerResult;
        glad_glFinish = traceFinish;
    }
    ~TimerProbe() {
        glad_glGenQueries = originalGenTimers; glad_glQueryCounter = originalTimestamp;
        glad_glGetQueryObjectiv = originalTimerAvailability; glad_glGetQueryObjectui64v = originalTimerResult;
        glad_glFinish = originalFinish; forceTimerUnavailable = false;
    }
};

void checkRenderProfiling(ResourceManager& resources) {
    RenderSystem renderer; renderer.init(); renderer.setResourceManager(&resources);
    glDebugMessageCallback(debugCallback, nullptr);
    auto cube = GameObjectFactory::createCube(); auto light = GameObjectFactory::createPointLight();
    check(resources.addGameObject(cube) && resources.addGameObject(light), "Profiler scene fixture insertion failed");
    renderer.setViewportSize(64, 64); renderer.update(0); Camera camera;
    TimerProbe probe;
    renderer.render(camera);
    check(timerObjects.empty() && timerWrites == 0 && timerPolls == 0 && timerReads == 0,
        "Disabled profiler allocated queries or submitted timer work");
    auto pixels = [&] {
        std::vector<float> result(64 * 64 * 4);
        glBindTexture(GL_TEXTURE_2D, renderer.getSceneColorTextureId());
        glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, result.data()); glBindTexture(GL_TEXTURE_2D, 0);
        return result;
    };
    const auto baseline = pixels();
    renderer.setProfilingEnabled(true); forceTimerUnavailable = true;
    for (int frame = 0; frame < 4; ++frame) renderer.render(camera);
    const auto allocated = timerObjects.size(); const auto submitted = timerWrites;
    check(allocated == 44 && submitted == 44 && timerReads == 0 && !renderer.getRenderProfile().valid,
        "Profiler did not defer four complete samples while GPU results were unavailable");
    renderer.render(camera);
    check(timerWrites == submitted && timerObjects.size() == allocated && renderer.skippedProfileFrames() == 1,
        "Full profiler ring did not skip sampling without new allocations");
    check(pixels() == baseline, "Profiling changed rendered pixels");
    renderer.setViewportSize(96, 32); renderer.update(0);
    forceTimerUnavailable = false;
    originalFinish(); // Test-only synchronization; production must never call glFinish.
    renderer.render(camera);
    const auto previous = renderer.getRenderProfile();
    check(previous.valid && previous.frame == 5 && renderer.profileAgeFrames() == 2,
        "Profiler published a current or mismatched frame instead of the completed sample");
    check(previous.width == 64 && previous.height == 64 && previous.opaqueObjectCount == 1 && previous.pointLightCount == 1 &&
        previous.directionalLightCount == 0 && previous.spotLightCount == 0 && previous.cloudVolumeCount == 0,
        "Profiler scene metadata came from a different frame");
    check(previous.passes.size() == 9, "Profiler omitted render passes");
    const char* names[] = {"FrameSetup", "DirectionalShadow", "PointShadow", "LightUpload", "ForwardHdr",
        "CloudSkeletonOverlay", "Gizmo", "Bloom", "Composite"};
    double cpuSum = previous.prepareCpuMs, gpuSum = previous.prepareGpuMs;
    for (std::size_t i = 0; i < previous.passes.size(); ++i) {
        const auto& pass = previous.passes[i];
        check(pass.name == names[i] && std::isfinite(pass.cpuMs) && pass.cpuMs >= 0 &&
            std::isfinite(pass.gpuMs) && pass.gpuMs >= 0, "Profiler pass timing or name invalid");
        cpuSum += pass.cpuMs; gpuSum += pass.gpuMs;
    }
    check(previous.cpuMs > 0 && previous.gpuMs > 0 && cpuSum <= previous.cpuMs + 0.000001 &&
        std::abs(gpuSum - previous.gpuMs) < 0.000001, "Profiler frame totals do not match their components");
    originalFinish(); renderer.render(camera);
    check(renderer.getRenderProfile().width == 96 && renderer.getRenderProfile().height == 32,
        "Profiler did not publish the resized sample");
    renderer.setProfilingEnabled(false);
    const auto writesBeforeDisable = timerWrites, readsBeforeDisable = timerReads, pollsBeforeDisable = timerPolls;
    renderer.render(camera);
    check(!renderer.getRenderProfile().valid && timerWrites == writesBeforeDisable && timerReads == readsBeforeDisable &&
        timerPolls == pollsBeforeDisable, "Disabling profiler kept stale timings or timer work");
    pendingTimers.clear(); renderer.setProfilingEnabled(true); forceTimerUnavailable = true;
    renderer.render(camera);
    check(timerObjects.size() == allocated && !renderer.getRenderProfile().valid, "Re-enabling profiler leaked queries or stale data");
    forceTimerUnavailable = false; originalFinish(); renderer.render(camera);
    check(renderer.getRenderProfile().valid && renderer.getRenderProfile().frame > previous.frame,
        "Re-enabled profiler did not publish a fresh sample");
    check(timerFinishes == 0, "Profiler forced CPU/GPU synchronization");
    renderer.shutdown();
    for (const auto id : timerObjects) check(!glIsQuery(id), "Renderer shutdown leaked timer queries");
    renderer.shutdown();
    std::cout << "PASS: asynchronous per-pass CPU/GPU timing, busy-ring skipping, matching metadata, toggles, pixels, and query cleanup\n";
}
}

void runRenderPerformanceChecks() {
    check(glfwInit() != 0, "GLFW init failed"); glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4); glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
    auto window = glfwCreateWindow(64, 64, "Render performance checks", nullptr, nullptr);
    check(window != nullptr, "Hidden GL context failed"); glfwMakeContextCurrent(window);
    check(gladLoadGLLoader((GLADloadproc)glfwGetProcAddress) != 0, "GL loader failed");
    glEnable(GL_DEBUG_OUTPUT); glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS); glDebugMessageCallback(debugCallback, nullptr);
    glDisable(GL_DEPTH_TEST); glDisable(GL_BLEND); glDisable(GL_FRAMEBUFFER_SRGB);
    std::filesystem::create_directories("x64/RenderChecks");
    {
        ResourceManager resources; resources.init();
        checkUniformCache(); checkStaticSubmission(); checkBloom(resources); checkGeometryReload(resources); checkRenderProfiling(resources);
        resources.shutdown(); RenderHelpers::shutdownSharedGeometry(); destroyDummyTexture2D();
    }
    check(glGetError() == GL_NO_ERROR && glErrors == 0, "GL errors during performance regression checks");
    glfwDestroyWindow(window); glfwTerminate();
}
